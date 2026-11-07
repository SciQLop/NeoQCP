#include "plottable-colormapped.h"
#include <QPointer>
#include <core.h>
#include <layoutelements/layoutelement-colorscale.h>

QCPAbstractColorMapped::QCPAbstractColorMapped(QCPAxis* keyAxis, QCPAxis* valueAxis,
                                               bool transparentNan)
        : QCPAbstractPlottable(keyAxis, valueAxis)
        , mDataScaleType(QCPAxis::stLinear)
        , mGradient(QCPColorGradient::gpCold)
{
    if (transparentNan)
        mGradient.setNanHandling(QCPColorGradient::nhTransparent);
}

QCPAbstractColorMapped::~QCPAbstractColorMapped()
{
    if (mColorScale)
        mColorScale->mUsers.removeOne(this);
}

void QCPAbstractColorMapped::setDataRange(const QCPRange& dataRange)
{
    if (!QCPRange::validRange(dataRange))
        return;
    if (!mColorScale)
    {
        applyDataRange(dataRange);
        return;
    }
    // The plottable's own signal comes first, as it did when the two were wired together; a slot
    // may delete this plottable, and then the scale never hears of the change.
    const QPointer<QCPAbstractColorMapped> alive(this);
    applyDataRange(dataRange);
    if (alive && mColorScale)
        mColorScale->setDataRange(mDataRange);
}

void QCPAbstractColorMapped::setDataScaleType(QCPAxis::ScaleType scaleType)
{
    if (!mColorScale)
    {
        applyDataScaleType(scaleType);
        return;
    }
    const QPointer<QCPAbstractColorMapped> alive(this);
    applyDataScaleType(scaleType);
    if (alive && mColorScale)
        mColorScale->setDataScaleType(mDataScaleType);
}

void QCPAbstractColorMapped::setGradient(const QCPColorGradient& gradient)
{
    if (!mColorScale)
    {
        applyGradient(gradient);
        return;
    }
    const QPointer<QCPAbstractColorMapped> alive(this);
    applyGradient(gradient);
    if (alive && mColorScale)
        mColorScale->setGradient(mGradient);
}

void QCPAbstractColorMapped::setColorScale(QCPColorScale* colorScale)
{
    if (mColorScale == colorScale)
        return;
    markDirty();
    if (mColorScale)
        mColorScale->mUsers.removeOne(this);
    mColorScale = colorScale;
    if (!colorScale)
    {
        qcpEmit(this, mParentPlot, &QCPAbstractColorMapped::colorScaleChanged);
        return;
    }
    colorScale->mUsers.append(this);

    // Each setter signals, and a slot may delete this plottable or the scale.
    const QPointer<QCPAbstractColorMapped> alive(this);
    const QPointer<QCPColorScale> live(colorScale);
    applyGradient(colorScale->gradient());
    if (alive && live)
        applyDataRange(colorScale->dataRange());
    if (alive && live)
        applyDataScaleType(colorScale->dataScaleType());
    if (!alive || !live)
        return;
    qcpEmit(this, mParentPlot, &QCPAbstractColorMapped::colorScaleChanged);
}

void QCPAbstractColorMapped::rescaleDataRange(bool)
{
    bool found = false;
    const QCPRange range = colorDataBounds(found);
    if (found && QCPRange::validRange(range))
        setDataRange(range);
}

void QCPAbstractColorMapped::applyDataRange(const QCPRange& dataRange)
{
    const QCPRange range = (mDataScaleType == QCPAxis::stLogarithmic)
        ? dataRange.sanitizedForLogScale()
        : dataRange.sanitizedForLinScale();
    if (mDataRange.lower == range.lower && mDataRange.upper == range.upper)
        return;
    mDataRange = range;
    colorMappingChanged();
    qcpEmit(this, mParentPlot, &QCPAbstractColorMapped::dataRangeChanged, mDataRange);
}

void QCPAbstractColorMapped::applyDataScaleType(QCPAxis::ScaleType scaleType)
{
    if (mDataScaleType == scaleType)
        return;
    mDataScaleType = scaleType;
    // Handing the type to a renderer may start a pipeline job whose slot deletes this plottable.
    const QPointer<QCPAbstractColorMapped> alive(this);
    colorMappingChanged();
    if (!alive)
        return;
    const QCPEmissionScope scope(mParentPlot);
    qcpEmit(this, mParentPlot, &QCPAbstractColorMapped::dataScaleTypeChanged, mDataScaleType);
    if (mDataScaleType == QCPAxis::stLogarithmic)
        applyDataRange(mDataRange.sanitizedForLogScale());
}

void QCPAbstractColorMapped::applyGradient(const QCPColorGradient& gradient)
{
    if (mGradient == gradient)
        return;
    mGradient = gradient;
    colorMappingChanged();
    qcpEmit(this, mParentPlot, &QCPAbstractColorMapped::gradientChanged, mGradient);
}

void QCPAbstractColorMapped::detachColorScale()
{
    if (!mColorScale)
        return;
    mColorScale = nullptr;
    qcpEmit(this, mParentPlot, &QCPAbstractColorMapped::colorScaleChanged);
}
