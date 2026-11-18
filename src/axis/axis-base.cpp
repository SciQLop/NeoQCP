#include "axis-base.h"
#include "axistickerlog.h"
#include "axistickersymlog.h"

#include <optional>
#include "../coordinate-system.h"
#include "../core.h"
#include "../drawable.h"
#include "../layoutelements/area.h"
#include "../plottables/plottable.h"
#include "../Profiling.hpp"
#include "../misuse.h"
#include "../signal-delivery.h"
#include <QMouseEvent>
#include <algorithm>
#include <cmath>
#include <utility>

/*! \fn void QCPAxisBase::rangeReversedChanged(bool rangeReversed)

  This signal is emitted when \ref rangeReversed changes, by \ref setRangeReversed or by the
  source of a link, after the plot delivers its deferred signals.
*/

/*! \fn void QCPAxisBase::tickerChanged(const QSharedPointer<QCPAxisTicker>& ticker)

  This signal is emitted when \ref setTicker installs another ticker, after the plot delivers its
  deferred signals.
*/

/*! \fn void QCPAxisBase::rangeDraggableChanged(bool rangeDraggable)

  This signal is emitted when \ref rangeDraggable changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPAxisBase::rangeZoomableChanged(bool rangeZoomable)

  This signal is emitted when \ref rangeZoomable changes, after the plot delivers its deferred
  signals.
*/

QCPAxisBase::QCPAxisBase(QCPArea* area, Qt::Orientation orientation)
        : QCPLayerable(area ? area->parentPlot() : nullptr, QString(), area)
        , mArea(area)
        , mRange(0, 0)
        , mRangeReversed(false)
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
        // Their systems' changed() is deferred past the loop: a slot may delete one of them.
        std::optional<QCPEmissionScope> scope;
    if (defersEmissions())
        scope.emplace(mParentPlot);
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
  Sets the scale this axis shows its range with to \a scale. A logarithmic scale has no pixel for a
  non-positive coordinate, so the range is sanitized: only the part on one side of zero survives.

  The scale also installs the ticker that suits it (\ref QCPAxisTicker, QCPAxisTickerLog,
  QCPAxisTickerSymlog), on every call, so a ticker set before the scale is replaced. A ticker set
  after it stays. A scale equal to the current one leaves the range alone and emits no
  \ref scaleChanged, but still installs its ticker. A scale the axis refuses changes nothing, its
  ticker included: see \ref scaleWillChange.

  An axis linked with no functions shows its source's values, so it shares the source's scale: a
  different \a scale set on it goes to the source, which hands it back to every such follower. An
  axis linked through functions keeps its own scale: it takes its range from its source, so a scale
  change only picks its ticks and never writes a sanitized range back into the source.

  \see QCPScale, setRange, setTicker
*/
void QCPAxisBase::setScale(const QCPScale& scale)
{
    // An identity follower shows its source's values: the scale belongs to the source (P-3), which
    // hands it back to every identity follower.
    if (isIdentityFollower() && scale != mScale)
    {
        mLink->source->setScale(scale);
        return;
    }
    applyScale(scale);
}

/*! \internal

  setScale without the identity-link redirection: the source hands its new scale to its identity
  followers through it.
*/
void QCPAxisBase::applyScale(const QCPScale& scale)
{
    std::optional<QCPEmissionScope> scope;
    if (defersEmissions())
        scope.emplace(mParentPlot);
    const bool changes = mScale != scale;
    if (changes && !scaleWillChange(scale))
        return;
    setTicker(defaultTicker(scale));
    if (!changes)
        return;
    mScale = scale;
    mScaleKind = scale.kind();
    if (scale.isLog())
    {
        if (mLink)
            refreshFromLink();
        else
            setRange(mRange.sanitizedForLogScale());
    }
    scaleDidChange();
    // Taken before the signal: with no scope open a slot runs now and may delete this axis, so
    // nothing of it is read after the emit. (A source with followers always defers: they count in
    // defersEmissions, so its followers get the scale before any slot runs.)
    const QList<QPointer<QCPAxisBase>> users = mLinkedAxes;
    qcpEmitCurrent(this, mParentPlot, &QCPAxisBase::scaleChanged, [this] { return mScale; });
    for (const QPointer<QCPAxisBase>& user : users)
        if (user && user->isIdentityFollower() && user->mScale != scale)
            user->applyScale(scale);
}

/*! \internal

  Returns the ticker \a scale brings: the plain one for a linear scale, QCPAxisTickerLog with the
  scale's base, QCPAxisTickerSymlog for a symlog scale. A subclass whose geometry only knows one
  scale overrides it, so that setScale never puts a ticker the geometry cannot show.
*/
QSharedPointer<QCPAxisTicker> QCPAxisBase::defaultTicker(const QCPScale& scale)
{
    if (scale.isSymlog())
        return QSharedPointer<QCPAxisTickerSymlog>::create(scale);
    if (scale.isLog())
    {
        const auto ticker = QSharedPointer<QCPAxisTickerLog>::create();
        ticker->setLogBase(scale.base());
        return ticker;
    }
    return QSharedPointer<QCPAxisTicker>::create();
}

/*! \internal

  A scale change moves what the subclass draws and what it caches: the cartesian axis drops its
  margin and its item trees. The base only knows the axis itself.
*/
void QCPAxisBase::scaleDidChange()
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
    update();
}

/*! \internal

  The checks a subclass adds before it takes a link. The base accepts every source it is handed.
*/
/*! \internal

  Whether anything listens to what a mapping change tells: a rangeChanged slot, a linked axis, or a
  system of the area. With none of them, deferring only costs a queue entry per range set.
*/
bool QCPAxisBase::defersEmissions() const
{
    return mRangeListeners > 0 || !mLinkedAxes.isEmpty() || (mArea && mArea->systemsListen());
}

void QCPAxisBase::connectNotify(const QMetaMethod& signal)
{
    if (signal == QMetaMethod::fromSignal(&QCPAxisBase::rangeChanged))
        ++mRangeListeners;
    QCPLayerable::connectNotify(signal);
}

void QCPAxisBase::disconnectNotify(const QMetaMethod& signal)
{
    if (signal == QMetaMethod::fromSignal(&QCPAxisBase::rangeChanged))
        --mRangeListeners;
    QCPLayerable::disconnectNotify(signal);
}

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
    // The systems' revision bumps now; their changed() waits for this scope, as their slots may
    // delete this axis and the linked axes are still to be told.
    std::optional<QCPEmissionScope> scope;
    if (defersEmissions())
        scope.emplace(mParentPlot);
    notifyMappingChanged();
    markAppearanceDirty();
    // No signal carries a reversal, so the linked axes are told here.
    refreshLinkedAxes();
    qcpEmit(this, mParentPlot, &QCPAxisBase::rangeReversedChanged, rangeReversed());
}

/*!
  Sets whether a drag in the area also moves this axis, when the area allows range dragging.
*/
void QCPAxisBase::setRangeDraggable(bool draggable)
{
    if (draggable == mRangeDraggable)
        return;
    mRangeDraggable = draggable;
    qcpEmit(this, mParentPlot, &QCPAxisBase::rangeDraggableChanged, rangeDraggable());
}

/*!
  Sets whether the mouse wheel zooms this axis, when the area allows range zooming.
*/
void QCPAxisBase::setRangeZoomable(bool zoomable)
{
    if (zoomable == mRangeZoomable)
        return;
    mRangeZoomable = zoomable;
    qcpEmit(this, mParentPlot, &QCPAxisBase::rangeZoomableChanged, rangeZoomable());
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
    qcpEmit(this, mParentPlot, &QCPAxisBase::tickerChanged, mTicker);
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
  Moves the range by \a diff: an offset in data coordinates on a linear scale, an offset in the
  scale's own space (\ref QCPScale::toScale units) on a symlog scale, and a factor on a logarithmic
  scale.
*/
void QCPAxisBase::moveRange(double diff)
{
    QCPRange moved = mRange;
    if (mScaleKind == QCPScale::Kind::Linear)
    {
        moved.lower += diff;
        moved.upper += diff;
    }
    else if (mScaleKind == QCPScale::Kind::Log)
    {
        moved.lower *= diff;
        moved.upper *= diff;
    }
    else // symlog: the offset is in scale space
        moved = mScale.shifted(mRange, diff);
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
  must lie in the same sign domain as the range; a symlog scale takes any center.
*/
void QCPAxisBase::scaleRange(double factor, double center)
{
    PROFILE_HERE_N("QCPAxisBase::scaleRange");
    QCPRange scaled = mRange;
    if (mScaleKind == QCPScale::Kind::Linear)
    {
        scaled.lower = (mRange.lower - center) * factor + center;
        scaled.upper = (mRange.upper - center) * factor + center;
    }
    else if (mScaleKind == QCPScale::Kind::Log)
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
    else // symlog: a scale-space scaling about the center, on either side of the band
        scaled = mScale.scaled(mRange, factor, center);
    commitRange(scaled);
}

/*!
  Returns the plottables of every system that maps through this axis, in the plot's own order.
*/
QList<QCPHandle<QCPPlottable>> QCPAxisBase::plottables() const
{
    QList<QCPHandle<QCPPlottable>> result;
    for (QCPPlottable* plottable : plottablesRaw())
        result << QCPHandle<QCPPlottable>(plottable);
    return result;
}

/*! \internal

  The plottables raw, in the plot's order: rescale() walks them per axis, and QCPAxisRect's passes
  read this one (K8). One name, one order (M1): a cartesian axis used to hide this with a list of
  its own.
*/
QList<QCPPlottable*> QCPAxisBase::plottablesRaw() const
{
    QList<QCPPlottable*> result;
    if (!mArea)
        return result;
    QCustomPlot* plot = mArea->parentPlot().get();
    if (!plot)
        return result;
    for (QCPPlottable* plottable : plot->plottablesRaw())
        if (QCPCoordinateSystem* system = plottable->systemRaw(); system && system->usesAxis(this))
            result.append(plottable);
    return result;
}

/*!
  Sets the fraction of a fitted span left blank on each side by \ref rescale and by the plottable
  rescale functions, as matplotlib's \c axes.xmargin and \c axes.ymargin: five percent on each side
  by default, so a marker or a lollipop disc sitting on an extreme value is not cut in half.

  The fraction is of the span in the axis' own scale (see QCPScale::widened): a fraction of the data
  span on a linear axis, of the decades on a log axis, of the transformed units on a symlog axis. A
  fit of constant data has a zero span: it recentres the current range and takes no margin.

  A negative or non-finite \a fraction is misuse: the margin is unchanged.

  \see rescale, QCustomPlot::rescaleAxes
*/
void QCPAxisBase::setRescaleMargin(double fraction)
{
    QCP_CHECK_OR_RETURN(std::isfinite(fraction) && fraction >= 0.0,
                        "a rescale margin is a fraction: finite and not negative", );
    if (fraction == mRescaleMargin)
        return;
    mRescaleMargin = fraction;
    qcpEmit(this, mParentPlot, &QCPAxisBase::rescaleMarginChanged, rescaleMargin());
}

/*!
  Changes the range so what the plottables of the systems using this axis draw is visible, in this
  axis' dimension (each axis kind collects its own range: see ownDataRange). The fitted range is
  widened by rescaleMargin() on each side, in this axis' scale. Axes linked with no functions show
  one range, so they fit the union of all their data, set once on the source, as matplotlib's shared
  axes do; the margin is then the source's. \ref QCPAxisRect::setupFullAxesBox
  "setupFullAxesBox(true)" makes such links. A constant plottable leaves a zero-size range, which
  recentres the current range instead (see rescaleTo).

  \see QCPPlottable::rescaleAxes, QCustomPlot::rescaleAxes
*/
void QCPAxisBase::rescale(bool onlyVisiblePlottables)
{
    // The margin of the axis that receives the range, the source of an identity link.
    const QCPAxisBase* receiver = isIdentityFollower() ? mLink->source.data() : this;
    rescale(onlyVisiblePlottables, receiver->mRescaleMargin);
}

/*! \overload

  Same as above, but with \a margin instead of the receiving axis' rescaleMargin(), which is left
  unchanged: one fit that does not depend on the axes' properties.
*/
void QCPAxisBase::rescale(bool onlyVisiblePlottables, double margin)
{
    QCP_CHECK_OR_RETURN(std::isfinite(margin) && margin >= 0.0,
                        "a rescale margin is a fraction: finite and not negative", );
    QCPAxisBase* target = isIdentityFollower() ? mLink->source.data() : this;
    const QCP::SignDomain signDomain = target->mScale.isLog()
        ? (target->mRange.upper < 0 ? QCP::sdNegative : QCP::sdPositive)
        : QCP::sdBoth;
    QCPRange unionRange;
    bool haveRange = false;
    const auto include = [&](const QCPAxisBase* axis)
    {
        QCPRange range;
        if (!axis->ownDataRange(onlyVisiblePlottables, signDomain, range))
            return;
        if (haveRange)
            unionRange.expand(range);
        else
            unionRange = range;
        haveRange = true;
    };
    include(target);
    for (const QPointer<QCPAxisBase>& user : target->mLinkedAxes)
        if (user && user->isIdentityFollower())
            include(user.data());
    target->rescaleTo(unionRange, haveRange, margin);
}

QList<QCPAxisBase*> QCPAxisBase::identityFollowers() const
{
    QList<QCPAxisBase*> result;
    for (const QPointer<QCPAxisBase>& user : mLinkedAxes)
        if (user && user->isIdentityFollower())
            result.append(user.data());
    return result;
}

QList<QCPAxisBase*> QCPAxisBase::linkedFollowers() const
{
    QList<QCPAxisBase*> result;
    for (const QPointer<QCPAxisBase>& user : mLinkedAxes)
        if (user)
            result.append(user.data());
    return result;
}

bool QCPAxisBase::isIdentityFollower() const
{
    return mLink && mLink->identity && mLink->source;
}

/*! \internal

  The drawn data ranges of the plottables of every system that maps through this axis, in this axis'
  own dimension.
*/
bool QCPAxisBase::ownDataRange(bool onlyVisiblePlottables, QCP::SignDomain signDomain,
                               QCPRange& range) const
{
    bool haveRange = false;
    // Through the virtual list: a user axis that drops a plottable must be honoured (M1).
    for (const QCPHandle<QCPPlottable>& plottable : plottables())
    {
        if (!plottable)
            continue;
        if (onlyVisiblePlottables && !plottable->realVisibility())
            continue;
        const QCPCoordinateSystem* system = plottable->systemRaw();
        if (!system)
            continue;
        const QCPDrawnSide side =
            system->keyAxis().get() == this ? QCPDrawnSide::Key : QCPDrawnSide::Value;
        bool foundRange = false;
        const QCPRange found = qcpDrawnRange(plottable->drawnDataRange(), side, foundRange, signDomain);
        if (!foundRange)
            continue;
        if (haveRange)
            range.expand(found);
        else
            range = found;
        haveRange = true;
    }
    return haveRange;
}

/*! \internal

  The tail of rescale(): stores the fitted \a range, widened by \a margin on each side (see
  qcp::fittedRange). A range that is not usable (the plottables are constant in this dimension)
  recentres the current range on it instead, in the axis' own scale.
*/
void QCPAxisBase::rescaleTo(const QCPRange& range, bool haveRange, double margin)
{
    if (!haveRange)
        return;
    setRange(qcp::fittedRange(mScale, range, mRange, margin));
}

/*!
  Links this axis to \a source: its range follows the source's through \a forward and \a inverse.
  They default to the identity, which makes the two axes show the same range.

  A source must be on the same plot; on a cartesian axis it must have the same orientation. What a
  subclass refuses else it reports from checkLinkSource.

  Without functions the link also shares the source's scale (see setScale), and links stay a star:
  linking to an axis that itself follows one without functions links to that one's source, and an
  axis that drives followers without functions hands them to its new source. Through functions,
  the source must not be linked itself and this axis must not already drive a link.
*/
void QCPAxisBase::linkTo(QCPAxisBase* source, QCPAxisFunction forward, QCPAxisFunction inverse)
{
    QCP_CHECK_OR_RETURN(source, "a link needs a source axis", );
    QCP_CHECK_OR_RETURN(source != this, "an axis cannot be linked to itself", );
    QCP_CHECK_OR_RETURN(mParentPlot && source->mParentPlot == mParentPlot,
                        "the linked axes must belong to the same plot", );
    QCP_CHECK_OR_RETURN(static_cast<bool>(forward) == static_cast<bool>(inverse),
                        "a link needs both its forward and its inverse function", );
    const bool identity = !forward;
    // Identity links stay a star (pre-flight C4): one to an identity follower goes to its source.
    if (identity && source->isIdentityFollower())
        source = source->mLink->source.data();
    QCP_CHECK_OR_RETURN(source != this, "the source already follows this axis", );
    if (!checkLinkSource(source))
        return;
    QCP_CHECK_OR_RETURN(!source->mLink, "the source axis is linked itself", );
    const QList<QPointer<QCPAxisBase>> followers = mLinkedAxes;
    const bool drivesAMappedLink = std::any_of(followers.cbegin(), followers.cend(),
        [](const QPointer<QCPAxisBase>& user) { return user && !user->isIdentityFollower(); });
    const bool drivesALink = std::any_of(followers.cbegin(), followers.cend(),
        [](const QPointer<QCPAxisBase>& user) { return !user.isNull(); });
    QCP_CHECK_OR_RETURN(!drivesALink || (identity && !drivesAMappedLink),
                        "the axis already drives a link of its own", );
    if (!forward)
    {
        forward = [](double v) { return v; };
        inverse = [](double v) { return v; };
    }
    // Every signal of the link (the re-pointed followers' and this axis' scale and range) waits for
    // the end of the call: a slot may delete this axis or the source.
    const QCPEmissionScope scope(mParentPlot);
    if (mLink)
        unlink();
    auto link = std::make_shared<QCPAxisLink>();
    link->source = source;
    const double forwardLower = forward(source->mRange.lower);
    const double forwardUpper = forward(source->mRange.upper);
    if (!std::isfinite(forwardLower) || !std::isfinite(forwardUpper))
        qDebug() << Q_FUNC_INFO << "the forward function is not finite at the source's range";
    link->decreasing = forwardUpper < forwardLower;
    link->identity = identity;
    link->forward = std::move(forward);
    link->inverse = std::move(inverse);
    mLink = link;
    source->mLinkedAxes.append(this);
    mLinked = true;
    // The identity followers this axis drove follow its new source now: still one star.
    mLinkedAxes.clear();
    for (const QPointer<QCPAxisBase>& user : followers)
        if (user)
        {
            user->mLink.reset();
            user->mLinked = false;
            user->linkTo(source);
        }
    if (identity && mScale != source->mScale)
        applyScale(source->mScale); // shares the source's scale (P-3); the same scale keeps a ticker
    notifyMappingChanged();
    refreshFromLink();
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
    // Compared once sanitized: a scale change that only sanitizes must still take effect.
    fresh = mScale.sanitized(fresh);
    const bool reversed = source->mRangeReversed != mLink->decreasing;
    if (fresh.lower == mRange.lower && fresh.upper == mRange.upper && reversed == mRangeReversed)
        return;
    const QCPRange oldRange = mRange;
    mRange = fresh;
    const bool reversalChanged = reversed != mRangeReversed;
    // Both signals wait for the scope: a rangeChanged slot may delete this axis.
    std::optional<QCPEmissionScope> scope;
    if (reversalChanged)
        scope.emplace(mParentPlot);
    mRangeReversed = reversed;
    if (reversalChanged)
        markAppearanceDirty();
    rangeDidChange(oldRange);
    if (reversalChanged)
        qcpEmit(this, mParentPlot, &QCPAxisBase::rangeReversedChanged, mRangeReversed);
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
    // The systems' revision bumps now, so no frame is reused; their changed() waits for the end of
    // this call, as its slots may delete this axis, and the layers are still to be dirtied.
    std::optional<QCPEmissionScope> scope;
    if (defersEmissions())
        scope.emplace(mParentPlot);
    notifyMappingChanged();
    // Before the signal: a slot that reads a linked axis sees the range it was told about.
    refreshLinkedAxes();
    qcpEmitIf(
        this, mParentPlot, &QCPAxisBase::rangeChanged,
        [this, range = mRange] { return mRange == range; }, mRange, oldRange);
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
    mRange = mScale.sanitized(range);
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
    const QCPRange clean = mScale.sanitized(range);
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
    std::optional<QCPEmissionScope> scope;
    if (defersEmissions())
        scope.emplace(mParentPlot);
    // Without a scope the source's rangeChanged runs inside this call, and its slot may delete us.
    const QPointer<QCPAxisBase> alive(this);
    source->setRange(sourceRange);
    if (alive)
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
    std::optional<QCPEmissionScope> scope;
    if (defersEmissions())
        scope.emplace(mParentPlot);
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
