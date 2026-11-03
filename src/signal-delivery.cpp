#include "signal-delivery.h"
#include "core.h"
#include <iterator>

void QCPEmissionQueue::flushFrom(std::size_t mark)
{
    const auto first = mPending.begin() + static_cast<std::ptrdiff_t>(mark);
    std::vector<Pending> due(std::make_move_iterator(first), std::make_move_iterator(mPending.end()));
    mPending.erase(first, mPending.end());
    for (auto& [sender, emission] : due)
        if (sender)
            emission();
}

QCPEmissionScope::QCPEmissionScope(QCustomPlot* plot, Kind kind)
        : mPlot(openOn(plot, kind)), mKind(kind), mMark(mPlot ? mPlot->mEmissions.size() : 0)
{
}

QCPEmissionScope::~QCPEmissionScope()
{
    if (!mPlot)
        return;
    if (mPlot->mEmissions.close() || mKind != Kind::Method)
        mPlot->mEmissions.flushFrom(mMark); // may delete the plot
    if (mPlot && mKind == Kind::WidgetEvent)
        mPlot->mEmissions.leaveWidgetEvent();
}

// qobject_cast is null once the plot is down to ~QObject, deleting its last children: a QPointer
// to it must not be made then.
QCustomPlot* QCPEmissionScope::openOn(QCustomPlot* plot, Kind kind)
{
    auto* live = qobject_cast<QCustomPlot*>(plot);
    if (!live || live->isBeingDestroyed())
        return nullptr;
    live->mEmissions.open();
    if (kind == Kind::WidgetEvent)
        live->mEmissions.enterWidgetEvent();
    return live;
}
