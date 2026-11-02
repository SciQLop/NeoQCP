#ifndef QCP_SIGNAL_DELIVERY_H
#define QCP_SIGNAL_DELIVERY_H

#include "global.h"
#include <QList>
#include <QObject>
#include <QPointer>
#include <functional>
#include <vector>

class QCustomPlot;

namespace qcp
{
// A snapshot to walk while slots may delete objects or edit the list it was taken from.
template <typename T>
QList<QPointer<T>> guarded(const QList<T*>& objects)
{
    return QList<QPointer<T>>(objects.cbegin(), objects.cend());
}
} // namespace qcp

// The emissions a plot holds back while one of its scopes is open. It lives in the plot, so it dies
// with it: nothing queued for a deleted plot ever runs.
class QCP_LIB_DECL QCPEmissionQueue
{
public:
    void open() { ++mDepth; }
    // True when the outermost scope closed.
    bool close() { return --mDepth == 0; }
    [[nodiscard]] bool isOpen() const { return mDepth > 0; }
    [[nodiscard]] bool hasPending() const { return !mPending.empty(); }
    void push(QObject* sender, std::function<void()> emission);
    void discard() { mPending.clear(); }
    // Runs every pending emission whose sender is alive. May delete the plot holding this queue:
    // it touches nothing of the queue after the first emission.
    void flush();

private:
    struct Pending
    {
        QPointer<QObject> sender;
        std::function<void()> emission;
    };
    static void run(std::vector<Pending> pending);

    int mDepth = 0;
    std::vector<Pending> mPending;
};

// Holds back the plot's deferred signals; the outermost scope delivers them as it closes. Inert on a
// plot being destroyed, and safe when a delivered slot deletes the plot.
class QCP_LIB_DECL QCPEmissionScope
{
public:
    explicit QCPEmissionScope(QCustomPlot* plot) : QCPEmissionScope(plot, false) { }
    ~QCPEmissionScope();
    QCPEmissionScope(const QCPEmissionScope&) = delete;
    QCPEmissionScope& operator=(const QCPEmissionScope&) = delete;

protected:
    QCPEmissionScope(QCustomPlot* plot, bool nextTurn);

private:
    static QCustomPlot* openOn(QCustomPlot* plot);
    static void postFlush(QCustomPlot* plot);

    QPointer<QCustomPlot> mPlot;
    bool mNextTurn;
};

// For Qt event handlers, where deleting the widget is forbidden: the outermost scope delivers on the
// next event-loop turn.
class QCP_LIB_DECL QCPInputScope : public QCPEmissionScope
{
public:
    explicit QCPInputScope(QCustomPlot* plot) : QCPEmissionScope(plot, true) { }
};

#endif // QCP_SIGNAL_DELIVERY_H
