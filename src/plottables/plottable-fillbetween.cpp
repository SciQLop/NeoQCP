#include "plottable-fillbetween.h"

#include "../datasource/abstract-multi-datasource.h"
#include "../drawable.h"
#include "../misuse.h"
#include "../painting/draw-context.h"
#include "graph-fill.h"
#include "plottable-graph.h"

#include <algorithm>
#include <optional>
#include <vector>

namespace
{
struct Interval
{
    double lower;
    double upper;
};

// The key range both lines share, over the lines that span one: a one-row line is a constant, so it
// crops nothing (qcp::fill::unionRows reads them the same way).
std::optional<Interval> sharedKeys(const QCPMultiDataSource& first,
                                   const QCPMultiDataSource& second)
{
    std::optional<Interval> shared;
    for (const QCPMultiDataSource* source : { &first, &second })
    {
        if (source->size() < 2)
            continue;
        bool found = false;
        const QCPRange range = source->keyRange(found);
        if (!found)
            continue;
        shared = shared ? Interval { std::max(shared->lower, range.lower),
                                     std::min(shared->upper, range.upper) }
                        : Interval { range.lower, range.upper };
    }
    return shared;
}

// One component's rows, cropped to the shared range with one row on each side for the
// interpolation at its ends. Across two value axes the values go through their pixel onto
// \a toValue, exactly as a graph-to-graph fill reads its partner.
std::vector<qcp::fill::SeriesRow> rowsOf(const QCPMultiDataSource& source, int component,
                                         const QCPScale& keyScale, const Interval& shared,
                                         const QCPAxis* fromValue, const QCPAxis* toValue)
{
    const int begin = std::max(0, source.findBegin(shared.lower) - 1);
    const int end = std::min(source.size(), source.findEnd(shared.upper) + 1);
    const auto gaps = source.gapsBefore(begin, end, /*cacheGaps=*/true, keyScale);
    std::vector<qcp::fill::SeriesRow> rows;
    rows.reserve(std::size_t(std::max(0, end - begin)));
    for (int i = begin; i < end; ++i)
    {
        double value = source.valueAt(component, i);
        if (fromValue && toValue)
            value = toValue->pixelToCoord(fromValue->coordToPixel(value));
        rows.push_back({ source.keyAt(i), value, gaps[std::size_t(i - begin)] != 0, i });
    }
    return rows;
}
} // namespace

QCPFillBetween::QCPFillBetween() : QCPPlottable()
{
    // A click inside the band reaches the lines under it and pans the view (P-5).
    setSelectable(QCP::stNone);
}

QCPFillBetween::~QCPFillBetween()
{
    detachLines();
}

QCPGraph* QCPFillBetween::firstLine() const
{
    return mFirstLine.data();
}

QCPGraph* QCPFillBetween::secondLine() const
{
    return mSecondLine.data();
}

void QCPFillBetween::setLines(QCPGraph* first, int firstComponent, QCPGraph* second,
                              int secondComponent)
{
    if (mFirstLine == first && mFirstComponent == firstComponent && mSecondLine == second
        && mSecondComponent == secondComponent)
        return;
    detachLines();
    mFirstLine = first;
    mFirstComponent = firstComponent;
    mSecondLine = second;
    mSecondComponent = secondComponent;
    attachLines();
    update();
    qcpEmit(this, mParentPlot, &QCPFillBetween::linesChanged);
}

void QCPFillBetween::setBelowBrush(std::optional<QBrush> brush)
{
    if (mBelowBrush == brush)
        return;
    update();
    mBelowBrush = std::move(brush);
    qcpEmit(this, mParentPlot, &QCPFillBetween::belowBrushChanged);
}

void QCPFillBetween::attachLines()
{
    // The same graph twice (two of its components) registers it once: addDependent() ignores a
    // second registration, and one destroyed connection is enough.
    if (QCPGraph* first = mFirstLine.data())
    {
        first->addDependent(this);
        mFirstDestroyed = connect(first, &QObject::destroyed, this, [this] { update(); });
    }
    if (QCPGraph* second = mSecondLine.data(); second && second != mFirstLine.data())
    {
        second->addDependent(this);
        mSecondDestroyed = connect(second, &QObject::destroyed, this, [this] { update(); });
    }
}

void QCPFillBetween::detachLines()
{
    if (mFirstLine)
        mFirstLine->removeDependent(this);
    if (mSecondLine && mSecondLine != mFirstLine)
        mSecondLine->removeDependent(this);
    QObject::disconnect(mFirstDestroyed);
    QObject::disconnect(mSecondDestroyed);
    mFirstDestroyed = QMetaObject::Connection();
    mSecondDestroyed = QMetaObject::Connection();
}

// A line that goes away leaves the fill in the plot drawing nothing (P-4): its QPointer is already
// null when the line's destroyed() arrives, so update() is all it takes to draw again.
bool QCPFillBetween::componentDraws(const QCPGraph& line, int component)
{
    const QCPMultiDataSource* source = line.dataSource();
    if (!source || source->empty())
        return false;
    if (component < 0 || component >= source->columnCount() || component >= line.componentCount())
    {
        qcp::reportMisuse(Q_FUNC_INFO, "the fill's component does not name a column of its line");
        return false;
    }
    return line.component(component).visible;
}

void QCPFillBetween::draw(QCPDrawContext& context) const
{
    QCPGraph* first = mFirstLine.data();
    QCPGraph* second = mSecondLine.data();
    if (!first || !second || !keyAxisRaw() || !valueAxisRaw() || keyAxisRaw()->range().size() <= 0)
        return;
    // The band is drawn in the first line's frame: a line moved to another rect leaves the fill
    // drawing nothing until it names lines of this one again.
    if (first->keyAxis().get() != keyAxisRaw() || first->valueAxis().get() != valueAxisRaw())
        return;
    if (first == second ? mFirstComponent == mSecondComponent : !first->canFillTo(*second))
        return;
    if (!componentDraws(*first, mFirstComponent) || !componentDraws(*second, mSecondComponent))
        return;

    const std::optional<Interval> shared = sharedKeys(*first->dataSource(), *second->dataSource());
    if (!shared || !(shared->upper > shared->lower))
        return;
    const QCPScale keyScale = keyAxisRaw()->scale();
    const bool across = first != second && first->fillsAcrossValueAxes(*second);
    std::vector<qcp::fill::SeriesRow> firstRows
        = rowsOf(*first->dataSource(), mFirstComponent, keyScale, *shared, nullptr, nullptr);
    std::vector<qcp::fill::SeriesRow> secondRows
        = rowsOf(*second->dataSource(), mSecondComponent, keyScale, *shared,
                 across ? second->valueAxis().get() : nullptr, across ? valueAxisRaw() : nullptr);

    std::vector<qcp::fill::StripPoint> rows = qcp::fill::unionRows(firstRows, secondRows);
    if (rows.empty())
        return;
    // A value a log axis cannot place closes on the range bound on zero's side, as the graph's own
    // fills do: the polygon then follows a bound that moves with the range.
    for (qcp::fill::StripPoint& row : rows)
    {
        row.a = qcp::fill::fillValue(*valueAxisRaw(), row.a);
        row.b = qcp::fill::fillValue(*valueAxisRaw(), row.b);
    }
    if (valueAxisRaw()->scale().isLog())
        (void)context.transform();

    const QBrush above = brush();
    const QBrush below = mBelowBrush ? *mBelowBrush : above;
    const qcp::fill::Step step = QCPGraph::ownerStep(first->component(mFirstComponent));
    // The lines are shaped first, so the colours swap on the riser of a stepped line, where the
    // drawn edges swap, not at the crossing of the straight segments the data would draw.
    const std::vector<qcp::fill::StripPoint> shaped
        = qcp::fill::steppedOnScreen(std::move(rows), step, keyScale);
    for (const qcp::fill::SideRun& run : qcp::fill::splitAtCrossings(shaped))
    {
        const QBrush& runBrush = run.firstAbove ? above : below;
        if (runBrush.style() == Qt::NoBrush || runBrush.color().alpha() == 0)
            continue;
        // Binned per run: a bin keeps one side, where binning the union would merge the two.
        const std::vector<qcp::fill::StripPoint> binned
            = qcp::fill::binnedForAxis(context, *keyAxisRaw(), run.rows);
        const auto [upper, lower] = qcp::fill::fillSeries(binned);
        if (!upper)
            continue;
        context.fillBetween(upper, lower, runBrush);
    }
}

quint64 QCPFillBetween::dependencyRevision() const
{
    const QCPGraph* first = mFirstLine.data();
    const QCPGraph* second = mSecondLine.data();
    quint64 revision = qHashMulti(
        0, qcpDependencyKey(first), qcpDependencyKey(second),
        first ? first->keyAxis().get() : nullptr, first ? first->valueAxis().get() : nullptr,
        second ? second->keyAxis().get() : nullptr, second ? second->valueAxis().get() : nullptr,
        mFirstComponent, mSecondComponent);
    // Across two value axes the band reads both systems' pixels: a pan of either moves it.
    if (first && second && first != second && first->fillsAcrossValueAxes(*second))
        revision = qHashMulti(revision, first->system()->revision(), second->system()->revision());
    return revision;
}
