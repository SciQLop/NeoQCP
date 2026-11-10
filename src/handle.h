#ifndef QCP_HANDLE_H
#define QCP_HANDLE_H

#include "misuse.h"
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <concepts>
#include <utility>

class QCPAbstractItem;
class QCPAbstractPlottable;
class QCPAxis;
class QCPBarsGroup;
class QCPLayer;
class QCPLayoutElement;
class QCPPlottableGroup;

namespace qcp::detail
{
// The owner of each family removes its member: a plottable, an item or a group unlists itself
// when it is deleted, an axis asks its rect, a layer asks its plot (which keeps
// one), a layout element leaves its layout first. Defined in core.cpp.
QCP_LIB_DECL void removeObject(QCPAbstractPlottable& plottable);
QCP_LIB_DECL void removeObject(QCPAbstractItem& item);
QCP_LIB_DECL void removeObject(QCPAxis& axis);
QCP_LIB_DECL void removeObject(QCPLayer& layer);
QCP_LIB_DECL void removeObject(QCPLayoutElement& element);
QCP_LIB_DECL void removeObject(QCPBarsGroup& group);
QCP_LIB_DECL void removeObject(QCPPlottableGroup& group);

} // namespace qcp::detail

namespace qcp
{
// A handle removes only what its owner knows how to remove. A widget (the plot itself) is not one
// of them: close the window or call deleteLater().
template <typename T>
concept Removable = requires(T& object) { detail::removeObject(object); };
} // namespace qcp

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

    /*! Removes the object the way its owner does: a plottable, an item or a group
      unlists itself when deleted, an axis goes through QCPAxisRect::removeAxis, a layer through
      QCustomPlot::removeLayer (the plot's last layer is refused with one misuse and stays), a layout
      element is taken out of its layout before it is deleted. A widget is not removable:
      QCPHandle<QCustomPlot> has no remove(), close the window or call deleteLater(). A dead handle
      makes it a no-op.
    */
    void remove()
        requires qcp::Removable<T>
    {
        if (T* object = mObject.data())
            qcp::detail::removeObject(*object);
    }

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
