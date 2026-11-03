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
    // What opened a scope: a plot method, a Qt input event, or a widget event (resize, show, paint)
    // that Qt keeps using after the handler returns.
    enum class Source
    {
        Method,
        Input,
        WidgetEvent
    };

    void open(Source source)
    {
        if (mDepth++ == 0)
            mOutermost = source;
        if (source == Source::WidgetEvent)
            ++mWidgetEventDepth;
    }
    // True when the outermost scope closed.
    bool close(Source source)
    {
        if (source == Source::WidgetEvent)
            --mWidgetEventDepth;
        return --mDepth == 0;
    }
    [[nodiscard]] bool isOpen() const { return mDepth > 0; }
    [[nodiscard]] bool hasPendingInput() const { return !mInput.empty(); }
    // At any nesting depth: a widget event can open under a method scope (processEvents() from a
    // rangeChanged slot).
    [[nodiscard]] bool insideWidgetEvent() const { return mWidgetEventDepth > 0; }
    // True when no posted input flush is outstanding; it then counts as posted.
    bool claimFlushPost() { return !std::exchange(mFlushPosted, true); }
    void postedFlushStarted() { mFlushPosted = false; }
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
    int mWidgetEventDepth = 0;
    Source mOutermost = Source::Method;
    bool mFlushPosted = false;
    std::vector<Pending> mMethod;
    std::vector<Pending> mInput;
};

// Holds back the plot's deferred signals; the outermost scope delivers them as it closes. Inert on a
// plot being destroyed, and safe when a delivered slot deletes the plot.
class QCP_LIB_DECL QCPEmissionScope
{
public:
    explicit QCPEmissionScope(QCustomPlot* plot)
            : QCPEmissionScope(plot, QCPEmissionQueue::Source::Method)
    {
    }
    ~QCPEmissionScope();
    QCPEmissionScope(const QCPEmissionScope&) = delete;
    QCPEmissionScope& operator=(const QCPEmissionScope&) = delete;

protected:
    QCPEmissionScope(QCustomPlot* plot, QCPEmissionQueue::Source source);

private:
    static QCustomPlot* openOn(QCustomPlot* plot, QCPEmissionQueue::Source source);
    static void postFlush(QCustomPlot* plot);

    QPointer<QCustomPlot> mPlot;
    QCPEmissionQueue::Source mSource;
};

// For Qt event handlers, where deleting the widget is forbidden: the outermost scope delivers on the
// next event-loop turn.
class QCP_LIB_DECL QCPInputScope : public QCPEmissionScope
{
public:
    explicit QCPInputScope(QCustomPlot* plot)
            : QCPEmissionScope(plot, QCPEmissionQueue::Source::Input)
    {
    }
};

// An input scope for resize, show and paint handlers. Qt touches the widget after these return, so
// deleting the plot inside one is reported as misuse.
class QCP_LIB_DECL QCPWidgetEventScope : public QCPEmissionScope
{
public:
    explicit QCPWidgetEventScope(QCustomPlot* plot)
            : QCPEmissionScope(plot, QCPEmissionQueue::Source::WidgetEvent)
    {
    }
};

#endif // QCP_SIGNAL_DELIVERY_H
