#include "signal-delivery.h"
#include "core.h"
#include <QCoreApplication>

void QCPEmissionQueue::push(QObject* sender, std::function<void()> emission)
{
    (mInputOutermost ? mInput : mMethod).push_back({QPointer<QObject>(sender), std::move(emission)});
}

void QCPEmissionQueue::run(std::vector<Pending> pending)
{
    for (auto& [sender, emission] : pending)
        if (sender)
            emission();
}

QCPEmissionScope::QCPEmissionScope(QCustomPlot* plot, bool nextTurn)
        : mPlot(openOn(plot, nextTurn)), mNextTurn(nextTurn)
{
}

QCPEmissionScope::~QCPEmissionScope()
{
    if (!mPlot || !mPlot->mEmissions.close())
        return;
    if (mNextTurn)
        postFlush(mPlot);
    else
        mPlot->mEmissions.flushMethod();
}

// qobject_cast is null once the plot is down to ~QObject, deleting its last children: a QPointer
// to it must not be made then.
QCustomPlot* QCPEmissionScope::openOn(QCustomPlot* plot, bool input)
{
    auto* live = qobject_cast<QCustomPlot*>(plot);
    if (!live || live->isBeingDestroyed())
        return nullptr;
    live->mEmissions.open(input);
    return live;
}

// The application, not the plot, runs the flush: a slot deleting the plot would otherwise delete
// the widget inside its own event().
void QCPEmissionScope::postFlush(QCustomPlot* plot)
{
    if (!plot->mEmissions.hasPendingInput())
        return;
    QMetaObject::invokeMethod(
        QCoreApplication::instance(),
        [plot = QPointer<QCustomPlot>(plot)] {
            // simplify: run from a nested event loop inside a scope, it leaves the input signals for
            // the next input event's flush. Re-posting would spin for as long as that loop runs.
            if (plot && !plot->mEmissions.isOpen())
                plot->mEmissions.flushInput();
        },
        Qt::QueuedConnection);
}
