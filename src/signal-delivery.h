#ifndef QCP_SIGNAL_DELIVERY_H
#define QCP_SIGNAL_DELIVERY_H

#include "global.h"
#include <QList>
#include <QObject>
#include <QPointer>
#include <functional>
#include <utility>
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
// with it: nothing queued for a deleted plot ever runs. Two lists, because an input-caused
// emission must wait for its posted turn: a method scope closing first must not run it.
class QCP_LIB_DECL QCPEmissionQueue
{
public:
    void open(bool input)
    {
        if (mDepth++ == 0)
            mInputOutermost = input;
    }
    // True when the outermost scope closed.
    bool close() { return --mDepth == 0; }
    [[nodiscard]] bool isOpen() const { return mDepth > 0; }
    [[nodiscard]] bool hasPendingInput() const { return !mInput.empty(); }
    // Goes with the outermost scope's kind: method-caused or input-caused.
    void push(QObject* sender, std::function<void()> emission);
    void discard()
    {
        mMethod.clear();
        mInput.clear();
    }
    // Run every pending emission of one kind whose sender is alive. May delete the plot holding this
    // queue: they touch nothing of the queue after the first emission.
    void flushMethod() { run(std::exchange(mMethod, {})); }
    void flushInput() { run(std::exchange(mInput, {})); }

private:
    struct Pending
    {
        QPointer<QObject> sender;
        std::function<void()> emission;
    };
    static void run(std::vector<Pending> pending);

    int mDepth = 0;
    bool mInputOutermost = false;
    std::vector<Pending> mMethod;
    std::vector<Pending> mInput;
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
    static QCustomPlot* openOn(QCustomPlot* plot, bool input);
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
