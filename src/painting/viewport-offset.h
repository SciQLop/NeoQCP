#pragma once

#include <QPointF>
#include <QSize>
#include <cmath>
#include <axis/axis.h>
#include <layoutelements/layoutelement-axisrect.h>

namespace qcp {

// Scale-aware size ratio of an axis' current range vs a previously rendered
// range. For a logarithmic axis a pure pan changes the *linear* range size
// (e.g. [1,10]->[2,20] is 2x), so comparing linear sizes would misread a pan as
// a zoom. Use log-space size on log axes so a pure pan yields ratio == 1.
inline double axisRangeSizeRatio(const QCPAxis* axis, const QCPRange& renderedRange)
{
    if (!axis)
        return 1.0;
    const QCPRange cur = axis->range();
    if (axis->scaleType() == QCPAxis::stLogarithmic
        && cur.lower > 0 && cur.upper > 0
        && renderedRange.lower > 0 && renderedRange.upper > 0)
    {
        const double oldLog = std::log(renderedRange.upper) - std::log(renderedRange.lower);
        const double curLog = std::log(cur.upper) - std::log(cur.lower);
        return oldLog != 0.0 ? curLog / oldLog : 1.0;
    }
    return renderedRange.size() != 0.0 ? cur.size() / renderedRange.size() : 1.0;
}

inline QPointF computeViewportOffset(
    const QCPAxis* keyAxis, const QCPAxis* valueAxis,
    const QCPRange& oldKeyRange, const QCPRange& oldValueRange)
{
    if (!keyAxis || !valueAxis)
        return {};

    double dKey = keyAxis->coordToPixel(oldKeyRange.lower)
                - keyAxis->coordToPixel(keyAxis->range().lower);
    double dVal = valueAxis->coordToPixel(oldValueRange.lower)
                - valueAxis->coordToPixel(valueAxis->range().lower);

    if (keyAxis->orientation() == Qt::Horizontal)
        return {dKey, dVal};
    else
        return {dVal, dKey};
}

struct LineCacheResult {
    bool needFreshLines;
    QPointF gpuOffset;
};

inline LineCacheResult evaluateLineCache(
    bool cacheDirty, bool cacheEmpty,
    const QSize& currentPlotSize, const QSize& cachedPlotSize,
    bool hasRenderedRange,
    const QCPRange& renderedKeyRange, const QCPRange& renderedValueRange,
    const QCPAxis* keyAxis, const QCPAxis* valueAxis,
    bool isExportMode)
{
    bool needFresh = cacheDirty || cacheEmpty;

    if (currentPlotSize != cachedPlotSize)
        needFresh = true;

    if (!needFresh && hasRenderedRange)
    {
        double keyRatio = keyAxis->range().size() / renderedKeyRange.size();
        double valRatio = valueAxis->range().size() / renderedValueRange.size();
        if (qAbs(keyRatio - 1.0) > 1e-4 || qAbs(valRatio - 1.0) > 1e-4)
            needFresh = true;
    }

    QPointF gpuOffset;
    if (!needFresh && hasRenderedRange)
    {
        gpuOffset = computeViewportOffset(keyAxis, valueAxis,
                                          renderedKeyRange, renderedValueRange);
        const bool keyIsVertical = keyAxis->orientation() == Qt::Vertical;
        const double keyDim = keyIsVertical
            ? keyAxis->axisRect()->height()
            : keyAxis->axisRect()->width();
        const double valDim = keyIsVertical
            ? keyAxis->axisRect()->width()
            : keyAxis->axisRect()->height();
        const double keyOff = qAbs(keyIsVertical ? gpuOffset.y() : gpuOffset.x());
        const double valOff = qAbs(keyIsVertical ? gpuOffset.x() : gpuOffset.y());
        if (keyOff > keyDim * 1.0 || valOff > valDim * 1.0)
            needFresh = true;
    }

    if (isExportMode)
        needFresh = true;

    return {needFresh, gpuOffset};
}

} // namespace qcp
