#include "axis-base.h"
#include "../core.h"
#include "../layoutelements/area.h"
#include "../Profiling.hpp"
#include "../misuse.h"
#include "../signal-delivery.h"
#include <QMouseEvent>
#include <algorithm>
#include <cmath>
#include <utility>

QCPAxisBase::QCPAxisBase(QCPArea* area, Qt::Orientation orientation)
        : QCPLayerable(area ? area->parentPlot() : nullptr, QString(), area)
        , mArea(area)
        , mRange(0, 0)
        , mRangeReversed(false)
        , mScaleType(stLinear)
        , mOrientation(orientation)
        , mSelectableParts(spAxis | spTickLabels | spAxisLabel)
        , mSelectedParts(spNone)
        , mTicker(new QCPAxisTicker)
{
}

QCPAxisBase::~QCPAxisBase()
{
    detachLinks();
}

/*! \internal
  Drops every link, this axis's own and the ones following it, so neither end outlives the other.
  A subclass calls it while its own parts still exist; the destructor's call then finds nothing.
*/
void QCPAxisBase::detachLinks()
{
    // A linked axis leaves its source's list: a dead link must not keep the source busy.
    if (mLink)
    {
        if (QCPAxisBase* source = mLink->source)
            source->mLinkedAxes.removeAll(this);
        mLink.reset();
    }
    // A link cannot outlive its source: its axes keep their range and become plain.
    if (!mLinkedAxes.isEmpty())
    {
        const QList<QPointer<QCPAxisBase>> users = mLinkedAxes;
        mLinkedAxes.clear();
        for (const QPointer<QCPAxisBase>& user : users)
            if (user)
            {
                user->mLink.reset();
                user->mLinked = false;
                user->notifyMappingChanged(); // what it maps through changed
                user->markAppearanceDirty(); // its pixels come from the axis no more
            }
    }
}

/*!
  Sets the scale type of this axis to \a type. A logarithmic scale has no pixel for a non-positive
  coordinate, so the range is sanitized: only the part on one side of zero survives.

  A linked axis takes its range from its source: a scale type change only picks its ticks, it never
  writes a sanitized range back into the source.

  \see setRange
*/
void QCPAxisBase::setScaleType(QCPAxisBase::ScaleType type)
{
    if (mScaleType == type)
        return;
    const QCPEmissionScope scope(mParentPlot);
    if (!scaleTypeWillChange(type))
        return;
    mScaleType = type;
    if (mScaleType == stLogarithmic)
    {
        if (mLink)
            refreshFromLink();
        else
            setRange(mRange.sanitizedForLogScale());
    }
    scaleTypeDidChange();
    qcpEmitCurrent(this, mParentPlot, &QCPAxisBase::scaleTypeChanged,
                   [this] { return mScaleType; });
}

/*! \internal

  A scale type change moves what the subclass draws and what it caches: the cartesian axis drops its
  margin and its item trees. The base only knows the axis itself.
*/
void QCPAxisBase::scaleTypeDidChange()
{
    notifyMappingChanged();
    markAppearanceDirty();
    // The axes following this one map through it, so their pixels moved even where their range did
    // not.
    const QList<QPointer<QCPAxisBase>> users = mLinkedAxes; // a slot may drop one
    for (const QPointer<QCPAxisBase>& user : users)
        if (user)
        {
            user->refreshFromLink();
            user->markAppearanceDirty();
        }
}

/*! \internal

  Dirties what a look change moves: the axis alone by default, whatever else the subclass draws
  with it needs the subclass' own override.
*/
void QCPAxisBase::markAppearanceDirty()
{
    markDirty();
}

/*! \internal

  The checks a subclass adds before it takes a link. The base accepts every source it is handed.
*/
bool QCPAxisBase::checkLinkSource([[maybe_unused]] const QCPAxisBase* source) const
{
    return true;
}

/*!
  Sets the range of this axis to \a range. Every range setter ends in \ref commitRange: on a linked
  axis the range is written back to the source through the link's inverse function instead.
*/
void QCPAxisBase::setRange(const QCPRange& range)
{
    commitRange(range);
}

/*!
  \overload

  Sets the lower and upper bound of the axis range. The upper bound is overridden by the lower one
  if the range is invalid, and the range is sanitized to what the scale type can show.
*/
void QCPAxisBase::setRange(double lower, double upper)
{
    commitRange(QCPRange(lower, upper));
}

/*!
  \overload

  Sets the range of this axis such that its size is \a size and its lower or upper bound (depending
  on \a alignment) or its center (for Qt::AlignCenter) is at \a position.
*/
void QCPAxisBase::setRange(double position, double size, Qt::AlignmentFlag alignment)
{
    if (alignment == Qt::AlignLeft)
        setRange(position, position + size);
    else if (alignment == Qt::AlignRight)
        setRange(position - size, position);
    else // alignment == Qt::AlignCenter
        setRange(position - size / 2.0, position + size / 2.0);
}

/*!
  Sets the lower bound of the axis range. The upper bound is not changed.
  \see setRange
*/
void QCPAxisBase::setRangeLower(double lower)
{
    if (mRange.lower == lower)
        return;
    commitRange(QCPRange(lower, mRange.upper));
}

/*!
  Sets the upper bound of the axis range. The lower bound is not changed.
  \see setRange
*/
void QCPAxisBase::setRangeUpper(double upper)
{
    if (mRange.upper == upper)
        return;
    commitRange(QCPRange(mRange.lower, upper));
}

/*!
  Sets whether the axis range direction is displayed reversed.

  A linked axis cannot reverse itself: the direction follows the source's.
*/
void QCPAxisBase::setRangeReversed(bool reversed)
{
    if (reversed == mRangeReversed)
        return; // a no-op asks for the direction the axis already shows, link or not
    QCP_CHECK_OR_RETURN(!mLink, "a linked axis follows its source's direction", );
    mRangeReversed = reversed;
    notifyMappingChanged();
    markAppearanceDirty();
    // No signal carries a reversal, so the linked axes are told here; their slots may delete them.
    if (mLinkedAxes.isEmpty())
    {
        refreshLinkedAxes();
        return;
    }
    const QCPEmissionScope scope(mParentPlot);
    refreshLinkedAxes();
}

/*!
  Sets whether a drag in the area also moves this axis, when the area allows range dragging.
*/
void QCPAxisBase::setRangeDraggable(bool draggable)
{
    mRangeDraggable = draggable;
}

/*!
  Sets whether the mouse wheel zooms this axis, when the area allows range zooming.
*/
void QCPAxisBase::setRangeZoomable(bool zoomable)
{
    mRangeZoomable = zoomable;
}

/*!
  Sets the ticker that generates the tick positions and tick labels of this axis.
*/
void QCPAxisBase::setTicker(QSharedPointer<QCPAxisTicker> ticker)
{
    if (!ticker)
    {
        qDebug() << Q_FUNC_INFO << "can not set nullptr as axis ticker";
        return;
    }
    if (mTicker == ticker)
        return;
    mTicker = ticker;
    // No need to invalidate a margin cache here: the tick labels are compared per replot.
    markAppearanceDirty();
}

/*!
  Sets which parts of this axis are selectable by the user.
*/
void QCPAxisBase::setSelectableParts(const QCPAxisBase::SelectableParts& selectableParts)
{
    if (mSelectableParts == selectableParts)
        return;
    mSelectableParts = selectableParts;
    qcpEmit(this, mParentPlot, &QCPAxisBase::selectableChanged);
}

/*!
  Sets which parts of this axis are currently selected.
*/
void QCPAxisBase::setSelectedParts(const QCPAxisBase::SelectableParts& selectedParts)
{
    if (mSelectedParts == selectedParts)
        return;
    mSelectedParts = selectedParts;
    markAppearanceDirty();
    qcpEmit(this, mParentPlot, &QCPAxisBase::selectionChanged);
}

/*!
  Moves the range by \a diff, on a linear scale in coordinates and on a logarithmic scale as a
  factor.
*/
void QCPAxisBase::moveRange(double diff)
{
    QCPRange moved = mRange;
    if (mScaleType == stLinear)
    {
        moved.lower += diff;
        moved.upper += diff;
    }
    else // mScaleType == stLogarithmic
    {
        moved.lower *= diff;
        moved.upper *= diff;
    }
    commitRange(moved);
}

/*!
  Scales the range by \a factor around the center of the current range.
*/
void QCPAxisBase::scaleRange(double factor)
{
    scaleRange(factor, range().center());
}

/*! \overload

  Scales the range by \a factor around the coordinate \a center. On a logarithmic scale the center
  must lie in the same sign domain as the range.
*/
void QCPAxisBase::scaleRange(double factor, double center)
{
    PROFILE_HERE_N("QCPAxisBase::scaleRange");
    QCPRange scaled = mRange;
    if (mScaleType == stLinear)
    {
        scaled.lower = (mRange.lower - center) * factor + center;
        scaled.upper = (mRange.upper - center) * factor + center;
    }
    else // mScaleType == stLogarithmic
    {
        if ((mRange.upper < 0 && center < 0)
            || (mRange.upper > 0 && center > 0)) // make sure center has same sign as range
        {
            scaled.lower = qPow(mRange.lower / center, factor) * center;
            scaled.upper = qPow(mRange.upper / center, factor) * center;
        }
        else
        {
            qDebug() << Q_FUNC_INFO
                     << "Center of scaling operation doesn't lie in same logarithmic sign domain "
                        "as range:"
                     << center;
            return;
        }
    }
    commitRange(scaled);
}

/*!
  Links this axis to \a source: its range follows the source's through \a forward and \a inverse.
  They default to the identity, which makes the two axes show the same range.

  A source must be on the same plot, must not be linked itself, and must not already drive a link;
  on a cartesian axis it must have the same orientation. What a subclass refuses else it reports
  from checkLinkSource.
*/
void QCPAxisBase::linkTo(QCPAxisBase* source, QCPAxisFunction forward, QCPAxisFunction inverse)
{
    QCP_CHECK_OR_RETURN(source, "a link needs a source axis", );
    QCP_CHECK_OR_RETURN(source != this, "an axis cannot be linked to itself", );
    QCP_CHECK_OR_RETURN(mParentPlot && source->mParentPlot == mParentPlot,
                        "the linked axes must belong to the same plot", );
    if (!checkLinkSource(source))
        return;
    QCP_CHECK_OR_RETURN(!source->mLink, "the source axis is linked itself", );
    const bool drivesALink
        = std::any_of(mLinkedAxes.cbegin(), mLinkedAxes.cend(),
                      [](const QPointer<QCPAxisBase>& user) { return !user.isNull(); });
    QCP_CHECK_OR_RETURN(!drivesALink, "the axis already drives a link of its own", );
    QCP_CHECK_OR_RETURN(static_cast<bool>(forward) == static_cast<bool>(inverse),
                        "a link needs both its forward and its inverse function", );
    if (!forward)
    {
        forward = [](double v) { return v; };
        inverse = [](double v) { return v; };
    }
    if (mLink)
        unlink();
    auto link = std::make_shared<QCPAxisLink>();
    link->source = source;
    const double forwardLower = forward(source->mRange.lower);
    const double forwardUpper = forward(source->mRange.upper);
    if (!std::isfinite(forwardLower) || !std::isfinite(forwardUpper))
        qDebug() << Q_FUNC_INFO << "the forward function is not finite at the source's range";
    link->decreasing = forwardUpper < forwardLower;
    link->forward = std::move(forward);
    link->inverse = std::move(inverse);
    mLink = link;
    source->mLinkedAxes.append(this);
    mLinked = true;
    notifyMappingChanged();
    {
        const QCPEmissionScope scope(mParentPlot);
        refreshFromLink();
    }
    markAppearanceDirty();
}

/*! \internal

  Takes the source's range, direction included, through the link's forward function.
*/
void QCPAxisBase::refreshFromLink()
{
    if (!mLink)
        return;
    QCPAxisBase* source = mLink->source;
    if (!source)
    {
        mLink.reset();
        mLinked = false;
        markAppearanceDirty();
        return;
    }
    QCPRange fresh(mLink->forward(source->mRange.lower), mLink->forward(source->mRange.upper));
    fresh.normalize();
    if (!QCPRange::validRange(fresh))
        return;
    const bool reversed = source->mRangeReversed != mLink->decreasing;
    if (fresh.lower == mRange.lower && fresh.upper == mRange.upper && reversed == mRangeReversed)
        return;
    const QCPRange oldRange = mRange;
    mRange
        = mScaleType == stLogarithmic ? fresh.sanitizedForLogScale() : fresh.sanitizedForLinScale();
    const bool reversalChanged = reversed != mRangeReversed;
    mRangeReversed = reversed;
    if (reversalChanged)
        markAppearanceDirty();
    rangeDidChange(oldRange);
}

/*! \internal

  Tells every axis linked to this one that the range moved.
*/
void QCPAxisBase::refreshLinkedAxes()
{
    if (mLinkedAxes.isEmpty())
        return;
    const QList<QPointer<QCPAxisBase>> users = mLinkedAxes;
    for (const QPointer<QCPAxisBase>& user : users)
        if (user)
            user->refreshFromLink();
}

/*! \internal

  Every range setter ends here. A range change invalidates every layer that depends on this axis,
  and QCustomPlot::replot only repaints dirty buffers, so the layers are dirtied here, at the
  source. The subclass' override dirties them, then calls this implementation, which refreshes the
  linked axes and emits rangeChanged.
*/
void QCPAxisBase::rangeDidChange(const QCPRange& oldRange)
{
    notifyMappingChanged();
    const auto apply = [&]
    {
        // Before the signal: a slot that reads a linked axis sees the range it was told about.
        refreshLinkedAxes();
        qcpEmitIf(
            this, mParentPlot, &QCPAxisBase::rangeChanged,
            [this, range = mRange] { return mRange == range; }, mRange, oldRange);
    };
    // A linked user emits from inside the refresh, and its slot may delete an axis of the pair.
    // Deferring to the end of this call keeps the refresh and the signal in one piece.
    if (mLinkedAxes.isEmpty())
    {
        apply();
        return;
    }
    const QCPEmissionScope scope(mParentPlot);
    apply();
}

/*! \internal

  Tells the area that this axis maps differently without a range of its own moving: a link, a
  direction, a polar angle. The area bumps the revision of the systems that use the axis.
*/
void QCPAxisBase::notifyMappingChanged()
{
    if (mArea)
        mArea->axisMappingChanged(this);
}

/*! \internal

  The one place that writes a range: every mutator (and every setRange overload) calls commitRange,
  which either applies the range here or writes it back to the source of a link.
*/
void QCPAxisBase::applyRange(const QCPRange& range)
{
    if ((range.lower == mRange.lower && range.upper == mRange.upper)
        || !QCPRange::validRange(range))
        return;
    const QCPRange oldRange = mRange;
    mRange
        = mScaleType == stLogarithmic ? range.sanitizedForLogScale() : range.sanitizedForLinScale();
    rangeDidChange(oldRange);
}

/*! \internal

  applyRange, or the inverse function of a link and back through the source.
*/
void QCPAxisBase::commitRange(const QCPRange& range)
{
    if (!mLink)
    {
        applyRange(range);
        return;
    }
    if (!mLink->inverse || !QCPRange::validRange(range))
        return;
    const QCPRange clean
        = mScaleType == stLogarithmic ? range.sanitizedForLogScale() : range.sanitizedForLinScale();
    if (clean.lower == mRange.lower && clean.upper == mRange.upper)
        return;
    const double lower = mLink->inverse(clean.lower);
    const double upper = mLink->inverse(clean.upper);
    QCPRange sourceRange(lower, upper);
    sourceRange.normalize();
    if (!(std::isfinite(lower) && std::isfinite(upper) && QCPRange::validRange(sourceRange)))
    {
        qDebug() << Q_FUNC_INFO << "the linked range has no source range under the inverse";
        return;
    }
    QCPAxisBase* source = mLink->source;
    QCP_CHECK_OR_RETURN(source, "the source of the link is gone", );
    const QCPEmissionScope scope(mParentPlot);
    source->setRange(sourceRange);
    refreshFromLink();
}

/*!
  Removes the link, if this axis has one. The axis keeps the range it showed.
*/
void QCPAxisBase::unlink()
{
    if (!mLink)
        return;
    if (QCPAxisBase* source = mLink->source)
        source->mLinkedAxes.removeAll(this);
    mLink.reset();
    mLinked = false;
    notifyMappingChanged();
    markAppearanceDirty();
}

/*!
  Returns the source this axis follows, or null when it is not linked.
*/
QCPAxisBase* QCPAxisBase::linkSource() const
{
    return mLink ? mLink->source.data() : nullptr;
}

/*!
  Returns the axis a drag on this one moves: the source of its link, or this axis itself.
*/
QCPAxisBase* QCPAxisBase::moveTarget() const
{
    return mLink && mLink->source ? mLink->source.data() : const_cast<QCPAxisBase*>(this);
}

/*!
  Returns the selection tolerance when the part at \a pos is selectable, -1 otherwise. The part
  found is put into \a details, so a following selectEvent knows what was clicked.
*/
double QCPAxisBase::selectTest(const QPointF& pos, bool onlySelectable, QVariant* details) const
{
    if (!mParentPlot)
        return -1;
    SelectablePart part = getPartAt(pos);
    if ((onlySelectable && !mSelectableParts.testFlag(part)) || part == spNone)
        return -1;
    if (details)
        details->setValue(part);
    return mParentPlot->selectionTolerance() * 0.99;
}

QCP::Interaction QCPAxisBase::selectionCategory() const
{
    return QCP::iSelectAxes;
}

void QCPAxisBase::selectEvent([[maybe_unused]] QMouseEvent* event, bool additive,
                              const QVariant& details, bool* selectionStateChanged)
{
    SelectablePart part = details.value<SelectablePart>();
    if (mSelectableParts.testFlag(part))
    {
        SelectableParts selBefore = mSelectedParts;
        const QPointer<QCPAxisBase> alive(this);
        setSelectedParts(additive ? mSelectedParts ^ part : part);
        if (selectionStateChanged)
            *selectionStateChanged = !alive || mSelectedParts != selBefore;
    }
}

void QCPAxisBase::deselectEvent(bool* selectionStateChanged)
{
    SelectableParts selBefore = mSelectedParts;
    const QPointer<QCPAxisBase> alive(this);
    setSelectedParts(mSelectedParts & ~mSelectableParts);
    if (selectionStateChanged)
        *selectionStateChanged = !alive || mSelectedParts != selBefore;
}
