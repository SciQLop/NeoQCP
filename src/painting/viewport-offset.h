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
// a zoom. Use the span in the axis' own scale space, where a pure pan yields
// ratio == 1 on every non-linear scale.
inline double axisRangeSizeRatio(const QCPAxis* axis, const QCPRange& renderedRange)
{
    if (!axis)
        return 1.0;
    const QCPRange cur = axis->range();
    const QCPScale& scale = axis->scale();
    if (!scale.isLinear() && renderedRange.upper > renderedRange.lower)
    {
        const double oldSpan
            = scale.toScale(renderedRange.upper) - scale.toScale(renderedRange.lower);
        const double curSpan = scale.toScale(cur.upper) - scale.toScale(cur.lower);
        return oldSpan != 0.0 ? curSpan / oldSpan : 1.0;
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

} // namespace qcp
