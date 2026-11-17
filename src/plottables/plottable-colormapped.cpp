#include "plottable-colormapped.h"
#include <QPointer>
#include <core.h>
#include <layoutelements/layoutelement-colorscale.h>

QCPColorMapped::QCPColorMapped(bool transparentNan, QCP::Capabilities required)
        : QCPPlottable(required)
        , mGradient(QCPColorGradient::gpCold)
{
    if (transparentNan)
        mGradient.setNanHandling(QCPColorGradient::nhTransparent);
}

QCPColorMapped::~QCPColorMapped()
{
    const QPointer<QCPColorScale> scale(mColorScale);
    if (mColorScale)
        mColorScale->mUsers.removeOne(this);
    if (scale)
        scale->usersChanged();
}

void QCPColorMapped::setDataRange(const QCPRange& dataRange)
{
    if (!QCPRange::validRange(dataRange))
        return;
    mDataRangeChosen = true;
    if (!mColorScale)
    {
        applyDataRange(dataRange);
        return;
    }
    // The plottable's own signal comes first, as it did when the two were wired together; a slot
    // may delete this plottable, and then the scale never hears of the change.
    const QPointer<QCPColorMapped> alive(this);
    applyDataRange(dataRange);
    if (alive && mColorScale)
        mColorScale->setDataRange(mDataRange);
}

void QCPColorMapped::setDataScale(const QCPScale& scale)
{
    mDataScaleChosen = true;
    if (!mColorScale)
    {
        applyDataScale(scale);
        return;
    }
    const QPointer<QCPColorMapped> alive(this);
    applyDataScale(scale);
    if (alive && mColorScale)
        mColorScale->setDataScale(mDataScale);
}

void QCPColorMapped::setGradient(const QCPColorGradient& gradient)
{
    mGradientChosen = true;
    if (!mColorScale)
    {
        applyGradient(gradient);
        return;
    }
    const QPointer<QCPColorMapped> alive(this);
    applyGradient(gradient);
    if (alive && mColorScale)
        mColorScale->setGradient(mGradient);
}

void QCPColorMapped::setColorScale(QCPColorScale* colorScale)
{
    mScaleChoice = colorScale ? ScaleChoice::User : ScaleChoice::None;
    attachColorScale(colorScale);
}

void QCPColorMapped::leaveRectScale()
{
    if (mScaleChoice == ScaleChoice::Rect)
        attachColorScale(nullptr);
}

bool QCPColorMapped::hasUserColorScale() const
{
    return mScaleChoice == ScaleChoice::User && mColorScale;
}

void QCPColorMapped::attachColorScale(QCPColorScale* colorScale, bool keepsChosenValues)
{
    if (mColorScale == colorScale)
        return;
    update();
    const QPointer<QCPColorScale> previous(mColorScale);
    if (mColorScale)
        mColorScale->mUsers.removeOne(this);
    mColorScale = colorScale;
    if (previous)
        previous->usersChanged(); // a scale nobody uses takes no room
    if (!colorScale)
    {
        qcpEmit(this, mParentPlot, &QCPColorMapped::colorScaleChanged);
        return;
    }
    colorScale->mUsers.append(this);
    colorScale->usersChanged();

    // Each setter signals, and a slot may delete this plottable or the scale.
    const QPointer<QCPColorMapped> alive(this);
    const QPointer<QCPColorScale> live(colorScale);
    // A gradient or range the caller chose before the join is not the scale's to overwrite.
    if (!keepsChosenValues || !mGradientChosen)
        applyGradient(colorScale->gradient());
    if (alive && live && (!keepsChosenValues || !mDataRangeChosen))
        applyDataRange(colorScale->dataRange());
    if (alive && live && (!keepsChosenValues || !mDataScaleChosen))
        applyDataScale(colorScale->dataScale());
    if (!alive || !live)
        return;
    qcpEmit(this, mParentPlot, &QCPColorMapped::colorScaleChanged);
}

void QCPColorMapped::rescaleDataRange(bool)
{
    bool found = false;
    const QCPRange range = colorDataBounds(found);
    if (found && QCPRange::validRange(range))
        setDataRange(range);
}

void QCPColorMapped::applyDataRange(const QCPRange& dataRange)
{
    const QCPRange range = mDataScale.sanitized(dataRange);
    if (mDataRange.lower == range.lower && mDataRange.upper == range.upper)
        return;
    mDataRange = range;
    colorMappingChanged();
    qcpEmit(this, mParentPlot, &QCPColorMapped::dataRangeChanged, mDataRange);
}

void QCPColorMapped::applyDataScale(const QCPScale& scale)
{
    if (mDataScale == scale)
        return;
    mDataScale = scale;
    // Handing the scale to a renderer may start a pipeline job whose slot deletes this plottable.
    const QPointer<QCPColorMapped> alive(this);
    colorMappingChanged();
    if (!alive)
        return;
    const QCPEmissionScope scope(mParentPlot);
    qcpEmitCurrent(this, mParentPlot, &QCPColorMapped::dataScaleChanged,
                   [this] { return mDataScale; });
    if (mDataScale.isLog())
        applyDataRange(mDataRange);
}

void QCPColorMapped::applyGradient(const QCPColorGradient& gradient)
{
    if (mGradient == gradient)
        return;
    mGradient = gradient;
    colorMappingChanged();
    qcpEmit(this, mParentPlot, &QCPColorMapped::gradientChanged, mGradient);
}

void QCPColorMapped::setSystem(QCPCoordinateSystem* system)
{
    // The system's signals wait for the join below: a slot may delete this plottable.
    const QCPEmissionScope scope(mParentPlot);
    const bool follows = mScaleChoice == ScaleChoice::Rect && mColorScale;
    QCPPlottable::setSystem(system);
    if (!follows || !system || mSystem != system)
        return;
    // The scale belonged to the rect it left: an area that is not an axis rect has none to give.
    if (qobject_cast<QCPAxisRect*>(system->area()))
        followKeyAxisRectScale();
    else
        attachColorScale(nullptr);
}

void QCPColorMapped::detachColorScale()
{
    if (!mColorScale)
        return;
    mColorScale = nullptr;
    qcpEmit(this, mParentPlot, &QCPColorMapped::colorScaleChanged);
}

void QCPColorMapped::followKeyAxisRectScale()
{
    if (mScaleChoice != ScaleChoice::Rect)
        return;
    // A cartesian frame hands out the key axis; a user system on an axis rect has no projection but
    // still has that rect's colour scale to join. An area that is not an axis rect has none, and
    // the caller's values then need an explicit scale.
    QCPAxisRect* rect = keyAxisRaw() ? axisRectOf(keyAxisRaw()) : nullptr;
    if (!rect && mSystem)
        rect = qobject_cast<QCPAxisRect*>(mSystem->area());
    if (!rect || rect->parentPlot() != mParentPlot)
        return;
    const bool madeHere = !rect->mColorScale;
    const auto scale = rect->colorScale();
    if (!scale)
        return;
    // A scale this plottable made has no state of its own: it starts as its first user, so this
    // plottable's own defaults (such as transparent NaN) are not silently dropped. A plottable
    // with no range of its own yet leaves the scale's own default range alone.
    // The scale setters and setColorScale() signal, and a slot may delete this plottable.
    const QPointer<QCPColorMapped> alive(this);
    if (madeHere)
    {
        scale->setGradient(mGradient);
        if (!alive)
            return;
        if (QCPRange::validRange(mDataRange))
            scale->setDataRange(mDataRange);
        if (!alive)
            return;
    }
    attachColorScale(scale.get(), true);
    if (!alive)
        return;
    colorMappingChanged();
}
