#include "contour-overlay.h"
#include <Profiling.hpp>
#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <algorithm>
#include <axis/axis.h>
#include <cmath>
#include <core.h>
#include <layoutelements/layoutelement-axisrect.h>
#include <memory>
#include <painting/draw-context.h>
#include <plottables/plottable-colormap.h> // QCPColorMapData
#include <vector>

void QCPContourOverlay::setLevels(const QVector<double>& levels)
{
    mLevels = levels;
    mAutoCount = 0;
    invalidate();
}

void QCPContourOverlay::setAutoLevels(int count)
{
    mAutoCount = qMax(0, count);
    mLevels.clear();
    invalidate();
}

void QCPContourOverlay::setPen(const QPen& pen)
{
    mPen = pen;
    invalidate();
}

namespace
{
bool drawable(const QCPColorMapData* grid)
{
    if (!grid || grid->keySize() < 2 || grid->valueSize() < 2)
        return false;
    const QCPRange bounds = grid->dataBounds();
    const double keySpan = grid->keyRange().size();
    const double valueSpan = grid->valueRange().size();
    return bounds.lower < bounds.upper && std::isfinite(bounds.lower) && std::isfinite(bounds.upper)
        && keySpan > 0 && valueSpan > 0 && std::isfinite(keySpan) && std::isfinite(valueSpan);
}

//! The grid's extent in pixels, signed: a reversed axis runs the rect backwards, and the label
//! painter needs that direction. Only the visibility intersection wants it normalized.
QRectF gridRect(const QCPColorMapData* grid, const QCPAxis* keyAxis, const QCPAxis* valueAxis)
{
    const QCPRange keys = grid->keyRange();
    const QCPRange values = grid->valueRange();
    return QRectF(QPointF(keyAxis->coordToPixel(keys.lower), valueAxis->coordToPixel(values.upper)),
                  QPointF(keyAxis->coordToPixel(keys.upper), valueAxis->coordToPixel(values.lower)));
}

//! A point of a contour segment (in the extractor's UV space) in data coordinates.
//!
//! The grid is uniform in pixels, not in data: a log key axis spans its columns geometrically
//! (bins are even in log10 space) and a log value axis its rows geometrically (the resampler's
//! Y range is log10-spaced there). Mapping u and v accordingly is what puts a contour on its
//! pixel, exactly like the image's two data corners do for the texture.
QPointF dataPoint(const QCPColorMapData* grid, const QLineF& segment, bool second,
                  bool keyLog, bool valueLog)
{
    const QCPRange keys = grid->keyRange();
    const QCPRange values = grid->valueRange();
    const double u = second ? segment.x2() : segment.x1();
    const double v = second ? segment.y2() : segment.y1();
    const double key = keyLog && keys.lower > 0 && keys.upper > 0
        ? keys.lower * std::pow(keys.upper / keys.lower, u)
        : keys.lower + u * keys.size();
    const double value = valueLog && values.lower > 0 && values.upper > 0
        ? values.upper * std::pow(values.lower / values.upper, v)
        : values.upper - v * values.size();
    return QPointF(key, value);
}
} // namespace

QVector<double> QCPContourOverlay::resolveLevels(const QCPColorMapData* grid,
                                                 QCPAxis::ScaleType dataScaleType) const
{
    QVector<double> levels;
    if (!mLevels.isEmpty())
    {
        std::copy_if(mLevels.cbegin(), mLevels.cend(), std::back_inserter(levels),
                     [](double level) { return std::isfinite(level); });
        return levels;
    }
    const QCPRange bounds = grid->dataBounds();
    if (dataScaleType == QCPAxis::stLogarithmic && bounds.lower > 0 && bounds.upper > 0)
    {
        // Geometric spacing: equal ratios put one line per decade band
        // instead of bunching every line at the top decade.
        const double ratio = std::pow(bounds.upper / bounds.lower, 1.0 / (mAutoCount + 1));
        for (int i = 1; i <= mAutoCount; ++i)
            levels.append(bounds.lower * std::pow(ratio, i));
    }
    else
    {
        const double step = (bounds.upper - bounds.lower) / (mAutoCount + 1);
        for (int i = 1; i <= mAutoCount; ++i)
            levels.append(bounds.lower + i * step);
    }
    return levels;
}

const QCPContourOverlay::Lines& QCPContourOverlay::linesFor(const QCPColorMapData* grid,
                                                            const Key& key, bool exporting)
{
    for (const Lines& cached : mCache)
        if (cached.key == key)
            return cached;
    PROFILE_HERE_N("QCPContourOverlay::extract");
    Lines& slot = mCache[exporting ? 1 : 0];
    slot.key = key;
    slot.lines = QCPContourExtractor::extract(grid, key.levels, key.maxDim);
    return slot;
}

void QCPContourOverlay::clear()
{
    mDrawn = nullptr;
    mLastLevels.clear();
    mLastLabels.clear();
}

void QCPContourOverlay::update(const QCPColorMapData* grid, QCPDrawContext& context,
                               QCPAxis::ScaleType dataScaleType, const QCPAxis* keyAxis,
                               const QCPAxis* valueAxis)
{
    if (!active() || !drawable(grid) || !keyAxis || !valueAxis)
    {
        clear();
        return;
    }
    const bool exportMode = context.exporting();
    // Keyed on the resolution actually marched, so a grid smaller than the screen limit is
    // marched once for both screen and export.
    const int fullDim = std::max(grid->keySize(), grid->valueSize());
    Key key { mGeneration, resolveLevels(grid, dataScaleType), std::min(kScreenMaxDim, fullDim) };
    mDrawn = &linesFor(grid, key, false);
    if (exportMode)
    {
        key.maxDim = fullDim;
        mDrawn = &linesFor(grid, key, true);
    }
    // The axes' scales are what the bins and the resampler followed: a log axis spans its
    // columns or rows geometrically, a linear one evenly (see dataPoint).
    recordLines(context, grid, *mDrawn,
                keyAxis->scaleType() == QCPAxis::stLogarithmic,
                valueAxis->scaleType() == QCPAxis::stLogarithmic);
    mLastLevels = key.levels;
    mLastLabels.clear();
    if (mLabelsEnabled && keyAxis->axisRect() && !mDrawn->lines.isEmpty())
        recordLabels(context, grid, keyAxis, valueAxis);
}

void QCPContourOverlay::recordLines(QCPDrawContext& context, const QCPColorMapData* grid,
                                    const Lines& lines, bool keyLog, bool valueLog) const
{
    for (const auto& line : lines.lines)
    {
        // One polyline per level, every segment kept apart by a NaN so the replays never join
        // the ends of segments the extractor did not connect.
        auto points = std::make_shared<std::vector<QPointF>>();
        points->reserve(size_t(line.segments.size()) * 3);
        for (const QLineF& segment : line.segments)
        {
            if (!points->empty())
                points->emplace_back(qQNaN(), qQNaN());
            points->push_back(dataPoint(grid, segment, false, keyLog, valueLog));
            points->push_back(dataPoint(grid, segment, true, keyLog, valueLog));
        }
        if (!points->empty())
            context.polyline(points, mPen);
    }
}

void QCPContourOverlay::recordLabels(QCPDrawContext& context, const QCPColorMapData* grid,
                                     const QCPAxis* keyAxis, const QCPAxis* valueAxis)
{
    // The tile holds pixels, not data, so it cannot be teleported into a new frame: reading the
    // transform marks the recording for a rebuild on every zoom and pan while labels are on.
    (void)context.transform();
    const QRectF imageRect = gridRect(grid, keyAxis, valueAxis);
    const QRectF visible = imageRect.normalized().intersected(keyAxis->axisRect()->rect());
    if (visible.isEmpty())
        return;
    // Labels drawn under the image would be hidden: they go in one raster over the part of the
    // grid the axes show, recorded after the lines so it draws above both.
    // simplify: a zoom rebuilds the whole tile, only while labels are on. Upgrade path: one small
    // tile per label in an atlas, drawn as one quad each.
    const double dpr = keyAxis->parentPlot()->bufferDevicePixelRatio();
    QImage overlay(QSize(qCeil(visible.width() * dpr), qCeil(visible.height() * dpr)),
                   QImage::Format_ARGB32_Premultiplied);
    overlay.fill(Qt::transparent);
    QPainter painter(&overlay);
    painter.scale(dpr, dpr);
    painter.translate(-visible.topLeft());
    painter.setFont(QFont());
    paintLabels(&painter, imageRect, visible);
    painter.end();

    const QPointF topLeft(keyAxis->pixelToCoord(visible.left()),
                          valueAxis->pixelToCoord(visible.top()));
    const QPointF bottomRight(keyAxis->pixelToCoord(visible.right()),
                              valueAxis->pixelToCoord(visible.bottom()));
    context.image(QCP::Data { topLeft.x(), topLeft.y() },
                  QCP::Data { bottomRight.x(), bottomRight.y() }, overlay);
}

bool QCPContourOverlay::labelFits(const QPointF& at, double level) const
{
    // simplify: O(labels) per candidate; fine for the few dozen labels a plot can hold.
    return std::none_of(mLastLabels.cbegin(), mLastLabels.cend(),
                        [&](const Label& placed)
                        {
                            const double d = QLineF(placed.position, at).length();
                            return d < kLabelClearance
                                || (placed.level == level && d < kLabelSpacing);
                        });
}

void QCPContourOverlay::paintLabels(QPainter* painter, const QRectF& imageRect,
                                    const QRectF& visible)
{
    const QFontMetricsF metrics(painter->font());
    const QColor color = mPen.color();
    // The line cannot leave a gap for the text: a halo in the opposite lightness keeps the
    // label readable over its own line.
    const QPen halo(color.lightness() > 127 ? QColor(0, 0, 0, 170) : QColor(255, 255, 255, 170),
                    3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    const auto toPixel = [&](double u, double v)
    {
        return QPointF(imageRect.left() + u * imageRect.width(),
                       imageRect.top() + v * imageRect.height());
    };
    painter->setRenderHint(QPainter::Antialiasing);
    for (const auto& line : mDrawn->lines)
    {
        const QString text = QString::number(line.level, 'g', 4);
        QPainterPath glyphs;
        glyphs.addText(-metrics.horizontalAdvance(text) / 2,
                       metrics.ascent() / 2 - metrics.descent() / 2, painter->font(), text);
        for (const QLineF& segment : line.segments)
        {
            const QPointF a = toPixel(segment.x1(), segment.y1());
            const QPointF b = toPixel(segment.x2(), segment.y2());
            const QPointF at = (a + b) / 2;
            if (a == b || !visible.contains(at) || !labelFits(at, line.level))
                continue;
            double angle = qRadiansToDegrees(std::atan2(b.y() - a.y(), b.x() - a.x()));
            if (angle > 90)
                angle -= 180;
            else if (angle <= -90)
                angle += 180;
            painter->save();
            painter->translate(at);
            painter->rotate(angle);
            painter->strokePath(glyphs, halo);
            painter->fillPath(glyphs, color);
            painter->restore();
            mLastLabels.append({ line.level, at });
        }
    }
}
