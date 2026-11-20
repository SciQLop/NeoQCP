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
                                         bool dataSpace, const QCPAxis* fromValue,
                                         const QCPAxis* toValue)
{
    const int begin = std::max(0, source.findBegin(shared.lower) - 1);
    const int end = std::min(source.size(), source.findEnd(shared.upper) + 1);
    const auto gaps = source.gapsBefore(begin, end, /*cacheGaps=*/!dataSpace, keyScale);
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

// A fill draws in its lines' frame, so it asks for nothing the lines themselves do not: a
// system that takes the lines (a user system with a projection, say) takes the fill between
// them.
QCPFillBetween::QCPFillBetween() : QCPPlottable(QCP::capStraightSegments)
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
    const QCPAxisBase* keyBase = keyAxisBase().get();
    if (!first || !second || !system() || !keyBase || keyBase->range().size() <= 0)
        return;
    // The band is drawn in the first line's frame: a line moved to another system leaves the fill
    // drawing nothing until it names lines of this frame again.
    if (first->system().get() != system().get())
        return;
    if (first == second ? mFirstComponent == mSecondComponent
                        : !qcp::fill::canFillTo(*first, *second))
        return;
    if (!componentDraws(*first, mFirstComponent) || !componentDraws(*second, mSecondComponent))
        return;

    const std::optional<Interval> shared = sharedKeys(*first->dataSource(), *second->dataSource());
    if (!shared || !(shared->upper > shared->lower))
        return;
    const QCPScale keyScale = keyBase->scale();
    const bool dataSpace = system()->kind() != QCPTransform::Kind::Cartesian;
    const bool across = first != second && qcp::fill::fillsAcrossValueAxes(*first, *second);
    std::vector<qcp::fill::SeriesRow> firstRows
        = rowsOf(*first->dataSource(), mFirstComponent, keyScale, *shared, dataSpace, nullptr,
                 nullptr);
    std::vector<qcp::fill::SeriesRow> secondRows
        = rowsOf(*second->dataSource(), mSecondComponent, keyScale, *shared, dataSpace,
                 across ? second->valueAxis().get() : nullptr, across ? valueAxisRaw() : nullptr);

    std::vector<qcp::fill::StripPoint> rows = qcp::fill::unionRows(firstRows, secondRows);
    if (rows.empty())
        return;
    // A value a log axis cannot place closes on the range bound on zero's side, as the graph's own
    // fills do: the polygon then follows a bound that moves with the range. A user system whose value
    // axis is no axis at all places every value it is given.
    QCPAxis* valueAxis = qobject_cast<QCPAxis*>(valueAxisBase().get());
    if (valueAxis)
        for (qcp::fill::StripPoint& row : rows)
        {
            row.a = qcp::fill::fillValue(*valueAxis, row.a);
            row.b = qcp::fill::fillValue(*valueAxis, row.b);
        }
    if (valueAxis && valueAxis->scale().isLog())
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
        // Binned per run: a bin keeps one side, where binning the union would merge the two. A user
        // system has no key pixel to bin against: every row is recorded, as the graph's fills did.
        const std::vector<qcp::fill::StripPoint> binned
            = keyAxisRaw() ? qcp::fill::binnedForAxis(context, *keyAxisRaw(), run.rows) : run.rows;
        const auto [upper, lower] = qcp::fill::fillSeries(binned);
        if (!upper)
            continue;
        context.fillBetween(upper, lower, runBrush);
    }
}

// The legend row is a swatch, not a replay of the band: the fill's brush where the first line is
// above, its below brush under it. The box is the bars' icon box.
void QCPFillBetween::drawLegendIcon(QCPDrawContext& icon) const
{
    const QCP::AxisRatio left { 0.165, 0.165 };
    const QCP::AxisRatio right { 0.835, 0.835 };
    if (!mBelowBrush)
    {
        icon.rect(left, right, QPen(Qt::NoPen), brush());
        return;
    }
    const QCP::AxisRatio lowerLeft { 0.165, 0.5 };
    const QCP::AxisRatio lowerRight { 0.835, 0.5 };
    icon.rect(left, lowerRight, QPen(Qt::NoPen), brush());
    icon.rect(lowerLeft, right, QPen(Qt::NoPen), *mBelowBrush);
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
    // In data space the recording reads the system's mapping, so a shear or a resize makes it stale
    // (the graph's B4 rule). A cartesian band is pixels of its frame: a pan must reuse it.
    if (const QCPCoordinateSystem* frame = system().get();
        frame && frame->kind() != QCPTransform::Kind::Cartesian)
        revision = qHashMulti(revision, frame->revision());
    // Across two value axes the band reads both systems' pixels: a pan of either moves it.
    if (first && second && first != second && qcp::fill::fillsAcrossValueAxes(*first, *second))
        revision = qHashMulti(revision, first->system()->revision(), second->system()->revision());
    return revision;
}
