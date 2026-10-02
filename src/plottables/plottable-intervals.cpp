#include "plottable-intervals.h"
#include "../core.h"
#include "../painting/painter.h"
#include "../painting/plottable-rhi-layer.h"
#include "../painting/rhi-utils.h"
#include "../axis/axis.h"
#include "../layoutelements/layoutelement-axisrect.h"
#include <algorithm>
#include <stdexcept>

QCPIntervals::QCPIntervals(QCPAxis* keyAxis, QCPAxis* valueAxis, QCPLaneLayout* layout)
        : QCPAbstractPlottable(keyAxis, valueAxis), mLayout(layout)
{
    connect(layout, &QCPLaneLayout::changed, this, [this] { regroupIfLanesWereAdded(); });
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

double QCPIntervals::selectTest(const QPointF&, bool, QVariant*) const { return -1; }

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
    const QRect rect = mKeyAxis->axisRect()->rect();
    for (int lane = 0; lane < static_cast<int>(mLanes.size()); ++lane)
        if (const auto band = mLayout->laneBand(lane, rect))
            appendLaneBars(lane, *band);
    mVertices.clear();
    for (const auto& bar : mBars)
    {
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
