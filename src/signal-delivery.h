#ifndef QCP_SIGNAL_DELIVERY_H
#define QCP_SIGNAL_DELIVERY_H

#include "global.h"
#include <QList>
#include <QObject>
#include <QPointer>
#include <functional>
#include <cstddef>
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
// with it: nothing queued for a deleted plot ever runs.
class QCP_LIB_DECL QCPEmissionQueue
{
public:
    void open() { ++mDepth; }
    // True when the outermost scope closed.
    bool close() { return --mDepth == 0; }
    [[nodiscard]] bool isOpen() const { return mDepth > 0; }
    [[nodiscard]] std::size_t size() const { return mPending.size(); }
    void enterWidgetEvent() { ++mWidgetEventDepth; }
    void leaveWidgetEvent() { --mWidgetEventDepth; }
    // At any nesting depth: a widget event can open under a method scope (processEvents() from a
    // rangeChanged slot).
    [[nodiscard]] bool insideWidgetEvent() const { return mWidgetEventDepth > 0; }
    void push(QObject* sender, std::function<void()> emission)
    {
        mPending.push_back({QPointer<QObject>(sender), std::move(emission)});
    }
    void discard() { mPending.clear(); }
    // Runs the emissions queued since \a mark whose sender is alive. May delete the plot holding
    // this queue: it touches nothing of the queue after the first emission.
    void flushFrom(std::size_t mark);

private:
    struct Pending
    {
        QPointer<QObject> sender;
        std::function<void()> emission;
    };

    int mDepth = 0;
    int mWidgetEventDepth = 0;
    std::vector<Pending> mPending;
};

// Holds back the plot's deferred signals; the outermost scope delivers them as it closes. Inert on a
// plot being destroyed, and safe when a delivered slot deletes the plot.
class QCP_LIB_DECL QCPEmissionScope
{
public:
    enum class Kind
    {
        Method,
        // A Qt event: delivers what it queued as it closes, even nested, since its slots get the
        // event itself, which dies when the event returns.
        Event,
        // An event after which Qt keeps using the widget (resize, show, paint): deleting the plot
        // before its signals are delivered is reported as misuse.
        WidgetEvent
    };

    explicit QCPEmissionScope(QCustomPlot* plot, Kind kind = Kind::Method);
    ~QCPEmissionScope();
    QCPEmissionScope(const QCPEmissionScope&) = delete;
    QCPEmissionScope& operator=(const QCPEmissionScope&) = delete;

private:
    static QCustomPlot* openOn(QCustomPlot* plot, Kind kind);

    QPointer<QCustomPlot> mPlot;
    Kind mKind;
    std::size_t mMark;
};

#endif // QCP_SIGNAL_DELIVERY_H
