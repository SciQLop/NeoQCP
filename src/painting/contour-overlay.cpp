#include "contour-overlay.h"
#include "colormap-renderer.h"
#include <axis/axis.h>
#include <core.h>
#include <layoutelements/layoutelement-axisrect.h>
#include <painting/painter.h>
#include <plottables/plottable-colormap.h> // QCPColorMapData
#include <Profiling.hpp>
#include <QFontMetricsF>
#include <QImage>
#include <QPainterPath>
#include <QtMath>
#include <algorithm>
#include <cmath>

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

bool exporting(const QCPPainter* painter)
{
    return painter && painter->modes().testFlag(QCPPainter::pmNoCaching);
}

void mirror(QVector<QCPContourExtractor::ContourLine>& lines, bool mirrorX, bool mirrorY)
{
    if (!mirrorX && !mirrorY)
        return;
    for (auto& line : lines)
        for (QLineF& s : line.segments)
            s = QLineF(mirrorX ? 1 - s.x1() : s.x1(), mirrorY ? 1 - s.y1() : s.y1(),
                       mirrorX ? 1 - s.x2() : s.x2(), mirrorY ? 1 - s.y2() : s.y2());
}
}

QVector<double> QCPContourOverlay::resolveLevels(const QCPColorMapData* grid) const
{
    QVector<double> levels;
    if (!mLevels.isEmpty())
    {
        std::copy_if(mLevels.cbegin(), mLevels.cend(), std::back_inserter(levels),
                     [](double level) { return std::isfinite(level); });
        return levels;
    }
    const QCPRange bounds = grid->dataBounds();
    if (mRenderer.dataScaleType() == QCPAxis::stLogarithmic && bounds.lower > 0 && bounds.upper > 0)
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
    mirror(slot.lines, key.mirrorX, key.mirrorY);
    slot.segments.clear();
    for (const auto& line : slot.lines)
        slot.segments += line.segments;
    ++mExtractionCount;
    return slot;
}

void QCPContourOverlay::clear()
{
    mRenderer.clearContour();
    mRenderer.clearLabelOverlay();
    mUploaded = {};
    mDrawn = nullptr;
    mLastUv.clear();
    mLastLevels.clear();
}

void QCPContourOverlay::update(const QCPColorMapData* grid, const QCPPainter* painter,
                               bool imageWasInvalidated, const QCPAxis* keyAxis,
                               const QCPAxis* valueAxis)
{
    if (!active() || !drawable(grid) || !keyAxis || !valueAxis)
    {
        clear();
        return;
    }
    const bool exportMode = exporting(painter);
    // Keyed on the resolution actually marched, so a grid smaller than the screen limit is
    // marched once for both screen and export.
    const int fullDim = std::max(grid->keySize(), grid->valueSize());
    Key key { mGeneration, resolveLevels(grid), std::min(kScreenMaxDim, fullDim),
              keyAxis->rangeReversed(), valueAxis->rangeReversed() };

    // The image's GPU quad is mirrored for a reversed axis (see QCPColormapRenderer::draw), so
    // the lines are mirrored the same way.
    const Lines& screen = linesFor(grid, key, false);
    if (imageWasInvalidated || !(mUploaded == key))
    {
        mLastUv = QCPContourExtractor::flatten(screen.lines);
        mLastLevels = key.levels;
        // The layer is made by the renderer's first draw, which comes after this: make it now
        // so the first frame's lines are not dropped.
        mRenderer.ensureRhiLayer();
        mRenderer.setContourLines(mLastUv, mPen.color());
        mUploaded = key;
    }
    mDrawn = &screen;
    if (exportMode)
    {
        key.maxDim = fullDim;
        mDrawn = &linesFor(grid, key, true);
    }
    // The painter needs its own lines whenever it paints the image: on export (which bypasses
    // the GPU layer even if one holds lines) or when nothing is on the GPU.
    if (exportMode || !mRenderer.hasContourOnGpu())
        mRenderer.setContourFallback(mDrawn->segments, mPen);
}

bool QCPContourOverlay::labelFits(const QPointF& at, double level) const
{
    // simplify: O(labels) per candidate; fine for the few dozen labels a plot can hold.
    return std::none_of(mLastLabels.cbegin(), mLastLabels.cend(),
                        [&](const Label& placed)
                        {
                            const double d = QLineF(placed.position, at).length();
                            return d < kLabelClearance || (placed.level == level && d < kLabelSpacing);
                        });
}

void QCPContourOverlay::paintLabels(QPainter* painter, const QRectF& imageRect, const QRectF& area)
{
    const QFontMetricsF metrics(painter->font());
    const QColor color = mPen.color();
    // The line cannot leave a gap for the text: a halo in the opposite lightness keeps the
    // label readable over its own line.
    const QPen halo(color.lightness() > 127 ? QColor(0, 0, 0, 170) : QColor(255, 255, 255, 170), 3.0,
                    Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    const auto toPixel = [&](double u, double v)
    { return QPointF(imageRect.left() + u * imageRect.width(), imageRect.top() + v * imageRect.height()); };
    painter->setRenderHint(QPainter::Antialiasing);
    for (const auto& line : mDrawn->lines)
    {
        const QString text = QString::number(line.level, 'g', 4);
        QPainterPath glyphs;
        glyphs.addText(-metrics.horizontalAdvance(text) / 2, metrics.ascent() / 2 - metrics.descent() / 2,
                       painter->font(), text);
        for (const QLineF& segment : line.segments)
        {
            const QPointF a = toPixel(segment.x1(), segment.y1());
            const QPointF b = toPixel(segment.x2(), segment.y2());
            const QPointF at = (a + b) / 2;
            if (a == b || !area.contains(at) || !labelFits(at, line.level))
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

void QCPContourOverlay::drawLabels(const QCPColorMapData* grid, QCPPainter* painter,
                                   const QCPAxis* keyAxis, const QCPAxis* valueAxis)
{
    mLastLabels.clear();
    const bool exportMode = exporting(painter);
    if (!mLabelsEnabled || !mDrawn || !grid || !keyAxis || !valueAxis || !keyAxis->axisRect())
    {
        if (!exportMode)
            mRenderer.clearLabelOverlay();
        return;
    }
    const QRectF area = keyAxis->axisRect()->rect();
    const QRectF imageRect
        = QCPColormapRenderer::imageRect(keyAxis, valueAxis, grid->keyRange(), grid->valueRange());
    if (exportMode || !mRenderer.imageOnGpu())
    {
        painter->save();
        painter->setClipRect(area);
        paintLabels(painter, imageRect, area);
        painter->restore();
        return;
    }
    // The image is a GPU quad composited after this layer's paint buffer: labels painted there
    // would end up under it. They go in an overlay quad drawn over the image instead.
    const double dpr = keyAxis->parentPlot()->bufferDevicePixelRatio();
    QImage overlay((area.size() * dpr).toSize(), QImage::Format_ARGB32_Premultiplied);
    overlay.setDevicePixelRatio(dpr);
    overlay.fill(Qt::transparent);
    QPainter overlayPainter(&overlay);
    overlayPainter.setFont(painter->font());
    overlayPainter.translate(-area.topLeft());
    paintLabels(&overlayPainter, imageRect, area);
    overlayPainter.end();
    mRenderer.setLabelOverlay(overlay, area);
}
