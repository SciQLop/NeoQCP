#include "plottable-intervals.h"
#include "../core.h"
#include "../painting/painter.h"
#include "../painting/plottable-rhi-layer.h"
#include "../painting/rhi-utils.h"
#include "../axis/axis.h"
#include "../layoutelements/layoutelement-axisrect.h"
#include <QFontMetricsF>
#include <algorithm>
#include <stdexcept>

// Draws the owning QCPIntervals' bar labels and lane names. Lives one layer above
// the plottable's own layer so its painter-drawn text composites after (i.e. on top
// of) the GPU-rendered bars; see the comment on QCPIntervals::mLabelLayer.
class QCPIntervals::LabelLayer : public QCPLayerable
{
public:
    LabelLayer(QCustomPlot* plot, QCPIntervals* owner, QCPLayer* layer)
        : QCPLayerable(plot), mOwner(owner)
    {
        setLayer(layer);
        setParentLayerable(owner);
        setParent(owner);
    }

protected:
    void applyDefaultAntialiasingHint(QCPPainter* painter) const override
    {
        painter->setAntialiasing(true);
    }

    void draw(QCPPainter* painter) override
    {
        mOwner->drawLabels(painter);
        if (mOwner->drawsLaneNames())
            mOwner->drawLaneNames(painter);
        mOwner->drawSelection(painter);
    }

private:
    QCPIntervals* mOwner;
};

namespace {
QCPLayer* layerAbove(QCustomPlot* plot, QCPLayer* base)
{
    const QString name = base->name() + QLatin1String(".intervals-labels");
    if (auto* existing = plot->layer(name))
        return existing;
    if (!plot->addLayer(name, base, QCustomPlot::limAbove))
        return base;
    auto* created = plot->layer(name);
    // Its own paint buffer, not merged with the (logically adjacent) base layer's:
    // the compositor draws each buffer's texture once, attributed to whichever layer
    // reaches it first. Sharing with base would make the label text composite at
    // base's position in the draw order, i.e. before base's GPU bar quads, right
    // back under them.
    created->setMode(QCPLayer::lmBuffered);
    return created;
}
} // namespace

QCPIntervals::QCPIntervals(QCPAxis* keyAxis, QCPAxis* valueAxis, QCPLaneLayout* layout)
        : QCPAbstractPlottable(keyAxis, valueAxis), mLayout(layout)
{
    setSelectable(QCP::stMultipleDataRanges);
    connect(layout, &QCPLaneLayout::changed, this, [this] { regroupIfLanesWereAdded(); });
    mLabelLayer = new LabelLayer(mParentPlot, this, layerAbove(mParentPlot, layer()));
    // Keep the label layerable one layer above wherever this plottable itself moves to.
    connect(this, &QCPLayerable::layerChanged, this, [this](QCPLayer* newLayer) {
        if (newLayer)
            mLabelLayer->setLayer(layerAbove(mParentPlot, newLayer));
    });
}

void QCPIntervals::setData(qcp::intervals::Columns columns)
{
    if (auto error = qcp::intervals::invalidColumns(columns))
        throw std::invalid_argument(*error);
    mColumns = std::move(columns);
    mLanes = qcp::intervals::groupByLane(mColumns, mLayout ? mLayout->laneNames().size() : 0);
    ++mDataGeneration;
}

// Rows on a lane the layout did not know yet were dropped by groupByLane.
void QCPIntervals::regroupIfLanesWereAdded()
{
    if (static_cast<int>(mLanes.size()) != mLayout->laneNames().size())
        mLanes = qcp::intervals::groupByLane(mColumns, mLayout->laneNames().size());
}

void QCPIntervals::setCategoryColors(const QVector<QColor>& colors)
{
    mCategoryColors = colors;
    ++mColorGeneration;
}

QColor QCPIntervals::categoryColor(int category) const
{
    if (category >= 0 && category < mCategoryColors.size())
        return mCategoryColors[category];
    // golden-angle hue steps keep neighbouring categories far apart
    return QColor::fromHsv((std::max(0, category) * 137) % 360, 160, 220);
}

QCPLayer* QCPIntervals::labelLayer() const { return mLabelLayer->layer(); }

QRectF QCPIntervals::barRect(int row) const
{
    const auto band = mLayout->laneBand(mColumns.lane[row], mKeyAxis->axisRect()->rect());
    if (!band)
        return {};
    const auto bar = qcp::intervals::toPixelBar(mKeyAxis->coordToPixel(mColumns.start[row]),
                                                mKeyAxis->coordToPixel(mColumns.stop[row]),
                                                band->top + 1, band->bottom - 1, 0, row);
    return QRectF(QPointF(bar.x0, bar.y0), QPointF(bar.x1, bar.y1));
}

QPointF QCPIntervals::pixelOf(double key, int lane) const
{
    const auto band = mLayout->laneBand(lane, mKeyAxis->axisRect()->rect());
    return { mKeyAxis->coordToPixel(key), band ? (band->top + band->bottom) / 2 : -1.0 };
}

namespace {
QCPIntervals::HitPart partOf(const QRectF& bar, double x)
{
    constexpr double edgeGrab = 4, minEdgeWidth = 10;
    if (bar.width() >= minEdgeWidth && std::abs(x - bar.left()) <= edgeGrab)
        return QCPIntervals::hpLeftEdge;
    if (bar.width() >= minEdgeWidth && std::abs(x - bar.right()) <= edgeGrab)
        return QCPIntervals::hpRightEdge;
    return QCPIntervals::hpBody;
}

double distanceToBar(const QRectF& bar, double x)
{
    return x < bar.left() ? bar.left() - x : (x > bar.right() ? x - bar.right() : 0);
}
} // namespace

QCPIntervals::Hit QCPIntervals::hitTest(const QPointF& pos) const
{
    if (!mLayout || !mKeyAxis)
        return {};
    const int lane = mLayout->laneAt(pos.y(), mKeyAxis->axisRect()->rect());
    if (lane < 0 || lane >= static_cast<int>(mLanes.size()))
        return lane < 0 ? Hit {} : Hit { -1, lane, hpEmpty };
    const double grabKeys = std::abs(mKeyAxis->pixelToCoord(pos.x() + 4) - mKeyAxis->pixelToCoord(pos.x()));
    const double key = mKeyAxis->pixelToCoord(pos.x());
    const auto& rows = mLanes[lane];
    const auto [begin, end] = qcp::intervals::candidateRange(rows, key - grabKeys, key + grabKeys);
    Hit best { -1, lane, hpEmpty };
    double bestDistance = 4.0 + 1e-9;
    for (int i = begin; i < end; ++i)
    {
        const int row = rows.rows[i];
        const QRectF bar = barRect(row);
        // later rows are drawn on top, so they win ties
        if (const double d = distanceToBar(bar, pos.x()); d <= bestDistance)
        {
            bestDistance = d;
            best = { row, lane, partOf(bar, pos.x()) };
        }
    }
    return best;
}

double QCPIntervals::selectTest(const QPointF& pos, bool onlySelectable, QVariant* details) const
{
    if (onlySelectable && !selectable())
        return -1;
    const Hit hit = hitTest(pos);
    if (hit.part == hpNone)
        return -1;
    if (details)
        *details = QVariant::fromValue(hit.row >= 0 ? QCPDataSelection(QCPDataRange(hit.row, hit.row + 1))
                                                    : QCPDataSelection());
    return 0;
}

void QCPIntervals::selectEvent(QMouseEvent*, bool additive, const QVariant& details,
                               bool* selectionStateChanged)
{
    const QCPDataSelection hit = details.value<QCPDataSelection>();
    const QCPDataSelection before = mSelection;
    if (!additive)
        setSelection(hit);
    else if (!hit.isEmpty() && mSelection.contains(hit))
        setSelection(mSelection - hit);
    else
        setSelection(mSelection + hit);
    if (selectionStateChanged)
        *selectionStateChanged = mSelection != before;
}

void QCPIntervals::deselectEvent(bool* selectionStateChanged)
{
    const bool had = selected();
    setSelection(QCPDataSelection());
    if (selectionStateChanged)
        *selectionStateChanged = had;
}

QVector<int> QCPIntervals::selectedRows() const
{
    QVector<int> rows;
    for (const QCPDataRange& range : mSelection.dataRanges())
        for (int row = range.begin(); row < range.end(); ++row)
            rows.append(row);
    return rows;
}

QVector<qint64> QCPIntervals::selectedIds() const
{
    QVector<qint64> ids;
    for (int row : selectedRows())
        ids.append(mColumns.ids[row]);
    return ids;
}

void QCPIntervals::setSelectedRows(const QVector<int>& rows)
{
    QCPDataSelection sel;
    for (int row : rows)
        if (row >= 0 && row < rowCount())
            sel.addDataRange(QCPDataRange(row, row + 1), false);
    sel.simplify();
    setSelection(sel);
}

QVector<int> QCPIntervals::rowsInRect(const QRectF& rect) const
{
    QVector<int> rows;
    const double a = mKeyAxis->pixelToCoord(rect.left()), b = mKeyAxis->pixelToCoord(rect.right());
    for (int lane = 0; lane < static_cast<int>(mLanes.size()); ++lane)
    {
        const auto band = mLayout->laneBand(lane, mKeyAxis->axisRect()->rect());
        if (!band || band->bottom < rect.top() || band->top > rect.bottom())
            continue;
        const auto [begin, end] = qcp::intervals::candidateRange(mLanes[lane], std::min(a, b), std::max(a, b));
        for (int i = begin; i < end; ++i)
            if (const int row = mLanes[lane].rows[i]; barRect(row).intersects(rect))
                rows.append(row);
    }
    std::ranges::sort(rows);
    return rows;
}

QCPDataSelection QCPIntervals::selectTestRect(const QRectF& rect, bool onlySelectable) const
{
    QCPDataSelection sel;
    if (onlySelectable && !selectable())
        return sel;
    for (int row : rowsInRect(rect.normalized()))
        sel.addDataRange(QCPDataRange(row, row + 1), false);
    sel.simplify();
    return sel;
}

int QCPIntervals::findBegin(double, bool) const { return 0; }

int QCPIntervals::findEnd(double, bool) const { return rowCount(); }

void QCPIntervals::mousePressEvent(QMouseEvent* event, const QVariant&)
{
    const Hit hit = hitTest(event->pos());
    if (event->button() == Qt::LeftButton && (event->modifiers() & Qt::ShiftModifier) && hit.lane >= 0)
    {
        mRubberBand = QRectF(event->pos(), event->pos());
        event->accept();
        return;
    }
    event->ignore();
}

void QCPIntervals::mouseMoveEvent(QMouseEvent* event, const QPointF& startPos)
{
    if (!mRubberBand)
        return;
    mRubberBand = QRectF(startPos, event->pos()).normalized();
    mParentPlot->replot(QCustomPlot::rpQueuedReplot);
}

void QCPIntervals::mouseReleaseEvent(QMouseEvent* event, const QPointF& startPos)
{
    if (!mRubberBand)
        return;
    setSelectedRows(rowsInRect(QRectF(startPos, event->pos()).normalized()));
    mRubberBand.reset();
    mParentPlot->replot(QCustomPlot::rpQueuedReplot);
}

void QCPIntervals::drawSelection(QCPPainter* painter) const
{
    const QRectF visible = mKeyAxis->axisRect()->rect();
    painter->setBrush(Qt::NoBrush);
    painter->setPen(mSelectionDecorator ? mSelectionDecorator->pen() : QPen(Qt::black, 2));
    for (int row : selectedRows())
        if (const QRectF bar = barRect(row); !bar.isEmpty() && bar.intersects(visible))
            painter->drawRect(bar);
    if (mRubberBand)
    {
        painter->setPen(QPen(mKeyAxis->tickLabelColor(), 1, Qt::DashLine));
        painter->drawRect(*mRubberBand);
    }
}

QCPRange QCPIntervals::getKeyRange(bool& foundRange, QCP::SignDomain) const
{
    foundRange = !mColumns.start.empty();
    if (!foundRange)
        return {};
    return { *std::ranges::min_element(mColumns.start), *std::ranges::max_element(mColumns.stop) };
}

QCPRange QCPIntervals::getValueRange(bool& foundRange, QCP::SignDomain, const QCPRange&) const
{
    foundRange = false;
    return {};
}

void QCPIntervals::draw(QCPPainter* painter)
{
    if (!mLayout)
        return;
    rebuildBarsIfNeeded();
    if (!drawBarsOnGpu(painter))
        drawBarsWithPainter(painter);
}

void QCPIntervals::drawLegendIcon(QCPPainter* painter, const QRectF& rect) const
{
    painter->fillRect(rect.adjusted(0, rect.height() / 4, 0, -rect.height() / 4), categoryColor(0));
}

QCPIntervals::BuildKey QCPIntervals::currentBuildKey() const
{
    const QCPRange range = mKeyAxis->range();
    return { range.lower, range.upper, mKeyAxis->axisRect()->rect(), mDataGeneration,
             mLayout->generation(), mColorGeneration };
}

void QCPIntervals::rebuildBarsIfNeeded()
{
    const BuildKey key = currentBuildKey();
    if (mBuiltFor == key)
        return;
    rebuildBars();
    mBuiltFor = key;
}

// simplify: rebuilds on every view change. The merge bounds the output to about
// lanes x plot width, so a pan costs one scan of the visible rows. Upgrade path: build a
// wider key range once and translate on the GPU while panning, as QCPMultiGraph does.
void QCPIntervals::rebuildBars()
{
    ++mBuildCount;
    mBars.clear();
    mLabelRects.clear();
    const QRect rect = mKeyAxis->axisRect()->rect();
    for (int lane = 0; lane < static_cast<int>(mLanes.size()); ++lane)
        if (const auto band = mLayout->laneBand(lane, rect))
            appendLaneBars(lane, *band);
    mVertices.clear();
    for (const auto& bar : mBars)
    {
        // Merged bars (row == -1, see appendMerged) never get a label: it's ambiguous
        // which source row's text a bar spanning several rows would show.
        if (bar.row != -1)
            appendLabelRect(bar);
        if (bar.instant)
            qcp::intervals::appendDiamond(mVertices, { (bar.x0 + bar.x1) / 2, (bar.y0 + bar.y1) / 2 },
                                          4, rgba(bar.category));
        else
            qcp::intervals::appendQuad(mVertices, QRectF(QPointF(bar.x0, bar.y0), QPointF(bar.x1, bar.y1)),
                                       rgba(bar.category));
    }
}

void QCPIntervals::appendLaneBars(int lane, const QCPLaneBand& band)
{
    const QCPRange range = mKeyAxis->range();
    const auto& rows = mLanes[lane];
    const auto [begin, end] = qcp::intervals::candidateRange(rows, range.lower, range.upper);
    for (int i = begin; i < end; ++i)
    {
        const int row = rows.rows[i];
        if (mColumns.stop[row] < range.lower)
            continue;
        qcp::intervals::appendMerged(
            mBars, qcp::intervals::toPixelBar(mKeyAxis->coordToPixel(mColumns.start[row]),
                                              mKeyAxis->coordToPixel(mColumns.stop[row]),
                                              band.top + 1, band.bottom - 1,
                                              mColumns.category[row], row));
    }
}

void QCPIntervals::appendLabelRect(const qcp::intervals::PixelBar& bar)
{
    if (mColumns.labels.isEmpty() || bar.instant)
        return;
    const QString& text = mColumns.labels[bar.row];
    const QRectF rect(QPointF(bar.x0, bar.y0), QPointF(bar.x1, bar.y1));
    if (!text.isEmpty() && QFontMetricsF(mParentPlot->font()).horizontalAdvance(text) + 4 <= rect.width())
        mLabelRects.emplace_back(rect, bar.row);
}

double QCPIntervals::fillOpacity() const
{
    return mLayout->placement() == QCPLaneLayout::plStrip ? 0.7 : 1.0;
}

std::array<float, 4> QCPIntervals::rgba(int category) const
{
    QColor c = categoryColor(category);
    c.setAlphaF(c.alphaF() * fillOpacity());
    return qcp::rhi::premultipliedColor(c);
}

bool QCPIntervals::drawBarsOnGpu(QCPPainter* painter)
{
    auto* rhi = mParentPlot->rhi();
    if (!rhi || painter->modes().testFlag(QCPPainter::pmVectorized)
        || painter->modes().testFlag(QCPPainter::pmNoCaching))
        return false;
    auto* layer = mParentPlot->plottableRhiLayer(mLayer);
    if (!layer)
        return false;
    if (!mVertices.empty())
        layer->addPlottable(mVertices, {}, clipRect(), mParentPlot->bufferDevicePixelRatio(),
                            mParentPlot->rhiOutputSize().height());
    return true;
}

void QCPIntervals::drawLabels(QCPPainter* painter) const
{
    painter->setPen(mKeyAxis->tickLabelColor());
    for (const auto& [rect, row] : mLabelRects)
        painter->drawText(rect, Qt::AlignCenter, mColumns.labels[row]);
}

bool QCPIntervals::drawsLaneNames() const
{
    if (!mLayout || mLayout->placement() != QCPLaneLayout::plStrip)
        return false;
    for (int i = 0; i < mParentPlot->plottableCount(); ++i)
        if (auto* other = qobject_cast<QCPIntervals*>(mParentPlot->plottable(i));
            other && other->laneLayout() == mLayout)
            return other == this;
    return false;
}

void QCPIntervals::drawLaneNames(QCPPainter* painter) const
{
    const QRect rect = mKeyAxis->axisRect()->rect();
    painter->setPen(mKeyAxis->tickLabelColor());
    const QStringList names = mLayout->laneNames();
    for (int lane = 0; lane < names.size(); ++lane)
        if (const auto band = mLayout->laneBand(lane, rect))
            painter->drawText(QRectF(rect.left() + 3, band->top, rect.width() / 3.0, band->bottom - band->top),
                              Qt::AlignVCenter | Qt::AlignLeft, names[lane]);
}

void QCPIntervals::drawBarsWithPainter(QCPPainter* painter) const
{
    painter->setPen(Qt::NoPen);
    for (const auto& bar : mBars)
    {
        QColor c = categoryColor(bar.category);
        c.setAlphaF(c.alphaF() * fillOpacity());
        painter->setBrush(c);
        if (bar.instant)
        {
            const QPointF m((bar.x0 + bar.x1) / 2, (bar.y0 + bar.y1) / 2);
            painter->drawPolygon(QPolygonF({ m + QPointF(0, -4), m + QPointF(4, 0),
                                             m + QPointF(0, 4), m + QPointF(-4, 0) }));
        }
        else
            painter->drawRect(QRectF(QPointF(bar.x0, bar.y0), QPointF(bar.x1, bar.y1)));
    }
}
