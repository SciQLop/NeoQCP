#include "signal-delivery.h"
#include "core.h"
#include <QCoreApplication>

void QCPEmissionQueue::push(QObject* sender, std::function<void()> emission)
{
    (mOutermost == Source::Method ? mMethod : mInput).push_back({QPointer<QObject>(sender), std::move(emission)});
}

void QCPEmissionQueue::run(std::vector<Pending> pending)
{
    for (auto& [sender, emission] : pending)
        if (sender)
            emission();
}

QCPEmissionScope::QCPEmissionScope(QCustomPlot* plot, QCPEmissionQueue::Source source)
        : mPlot(openOn(plot, source)), mSource(source)
{
}

QCPEmissionScope::~QCPEmissionScope()
{
    if (!mPlot || !mPlot->mEmissions.close(mSource))
        return;
    postFlush(mPlot); // first: the method flush may delete the plot
    if (mSource == QCPEmissionQueue::Source::Method)
        mPlot->mEmissions.flushMethod();
}

// qobject_cast is null once the plot is down to ~QObject, deleting its last children: a QPointer
// to it must not be made then.
QCustomPlot* QCPEmissionScope::openOn(QCustomPlot* plot, QCPEmissionQueue::Source source)
{
    auto* live = qobject_cast<QCustomPlot*>(plot);
    if (!live || live->isBeingDestroyed())
        return nullptr;
    live->mEmissions.open(source);
    return live;
}

// An outermost close posts while input signals wait and no posted flush is outstanding. The posted
// call clears that mark first, so a flush that found a scope open (run from a nested event loop) is
// retried by the next close; only closes post, so nothing spins. The application, not the plot,
// runs the flush: a slot deleting the plot would otherwise delete the widget inside its own event().
void QCPEmissionScope::postFlush(QCustomPlot* plot)
{
    if (!plot->mEmissions.hasPendingInput() || !plot->mEmissions.claimFlushPost())
        return;
    QMetaObject::invokeMethod(
        QCoreApplication::instance(),
        [plot = QPointer<QCustomPlot>(plot)] {
            if (!plot)
                return;
            plot->mEmissions.postedFlushStarted();
            if (!plot->mEmissions.isOpen())
                plot->mEmissions.flushInput();
        },
        Qt::QueuedConnection);
}
