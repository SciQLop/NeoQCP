#ifndef QCP_HANDLE_H
#define QCP_HANDLE_H

#include "misuse.h"
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <concepts>
#include <utility>

class TestHandle;

template <typename T>
class QCPHandle
{
public:
    QCPHandle() = default;
    explicit QCPHandle(T* object) : mObject(object) { requireQObject(); }

    // No implicit conversion to T*: it would bring dangling raw pointers back (T* p = h; delete h;).
    explicit operator bool() const { return !mObject.isNull(); }
    T* get() const
    {
        requireQObject();
        return mObject.data();
    }

    T* operator->() const
    {
        if (!checkAlive())
            qFatal("NeoQCP: call through a handle whose object was deleted");
        return mObject.data();
    }

    template <typename U>
    QCPHandle<U> as() const
    {
        return QCPHandle<U>(qobject_cast<U*>(mObject.data()));
    }

    // simplify: plain delete is safe for plottables and items, which unlist themselves. Parts (axes,
    // layers, axis rects) and deleting the sender from its own signal need owner routing (milestone 2).
    void remove() { delete mObject.data(); }

    friend bool operator==(const QCPHandle& a, const QCPHandle& b) { return a.get() == b.get(); }

private:
    friend class ::TestHandle;

    static void requireQObject()
    {
        static_assert(std::derived_from<T, QObject>, "QCPHandle<T> needs T derived from QObject");
    }

    bool checkAlive() const
    {
        QCP_CHECK_OR_RETURN(!mObject.isNull(), "call through a handle whose object was deleted",
                            false);
        return true;
    }

    QPointer<T> mObject;
};

template <typename T, typename Signal, typename... Rest>
QMetaObject::Connection qcpConnect(const QCPHandle<T>& h, Signal signal, Rest&&... rest)
{
    if (!h)
        return {};
    return QObject::connect(h.get(), signal, std::forward<Rest>(rest)...);
}

#endif // QCP_HANDLE_H
