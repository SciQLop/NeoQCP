#ifndef QCP_HANDLE_H
#define QCP_HANDLE_H

#include "misuse.h"
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <concepts>

class TestHandle;

template <typename T>
    requires std::derived_from<T, QObject>
class QCPHandle
{
public:
    QCPHandle() = default;
    explicit QCPHandle(T* object) : mObject(object) {}

    explicit operator bool() const { return !mObject.isNull(); }
    T* get() const { return mObject.data(); }

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

    void remove() { delete mObject.data(); }

    friend bool operator==(const QCPHandle& a, const QCPHandle& b) { return a.get() == b.get(); }

private:
    friend class ::TestHandle;

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
