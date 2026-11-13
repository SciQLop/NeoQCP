#ifndef QCP_HANDLE_H
#define QCP_HANDLE_H

#include "misuse.h"
#include <QDebug>
#include <QHashFunctions>
#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <concepts>
#include <cstddef>
#include <functional>
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
    // `QCPHandle<T> h = nullptr;` copy-initialises, like a QPointer (Opus M-2).
    QCPHandle(std::nullptr_t) : mObject(nullptr) { requireQObject(); }

    // A handle of a derived class makes a handle of a base one. K11: the other specialization is
    // read through its public get().
    template <typename U>
        requires std::derived_from<U, T>
    QCPHandle(const QCPHandle<U>& other) : mObject(other.get()) { }

    // A handle converts implicitly to T*, like QPointer: 2.x code keeps compiling, and
    // QObject::connect(h, ...) reaches the object. It is null when the object is gone, so a handle
    // never hands out a dangling pointer; a raw pointer kept beside it can still dangle. `delete h`
    // therefore compiles through this conversion and bypasses remove().
    operator T*() const noexcept { return mObject.data(); }

    explicit operator bool() const { return !mObject.isNull(); }

    // QPointer's assignment. A raw pointer, nullptr, 0 and {} all take this operator: without a
    // separate nullptr_t overload the null spellings are not ambiguous (opencode I-2).
    QCPHandle& operator=(T* object)
    {
        mObject = object;
        return *this;
    }
    // A derived handle assigns to a base one (Opus I1). A base handle does not assign to a derived
    // one: the constraint only goes up the hierarchy, like the converting constructor.
    template <typename U>
        requires std::derived_from<U, T>
    QCPHandle& operator=(const QCPHandle<U>& other)
    {
        mObject = other.get();
        return *this;
    }

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
    // A handle still compares against nullptr and 0 without ambiguity: these overloads are exact
    // matches and beat the pointer comparison, which needs the handle's pointer conversion.
    friend bool operator==(const QCPHandle& a, std::nullptr_t) { return a.mObject.isNull(); }
    friend bool operator==(std::nullptr_t, const QCPHandle& b) { return b.mObject.isNull(); }

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

namespace qcp
{
// The public list getters hand out handles; the setters keep taking QList<T*> (K5). This is the
// one conversion between them.
template <typename T>
QList<T*> pointers(const QList<QCPHandle<T>>& handles)
{
    // An empty list stays unallocated: reserve(0) would allocate a header on every call.
    if (handles.isEmpty())
        return {};
    QList<T*> result;
    result.reserve(handles.size());
    for (const QCPHandle<T>& handle : handles)
        result.append(handle.get());
    return result;
}
} // namespace qcp

// A handle hashes as the pointer it holds, so QSet<QCPHandle<T>> and std::unordered_set work.
template <typename T>
size_t qHash(const QCPHandle<T>& handle, size_t seed = 0) noexcept
{
    return qHash(handle.get(), seed);
}

namespace std
{
template <typename T>
struct hash<QCPHandle<T>>
{
    size_t operator()(const QCPHandle<T>& handle) const noexcept
    {
        return hash<T*> {}(handle.get());
    }
};
} // namespace std

template <typename T>
QDebug operator<<(QDebug debug, const QCPHandle<T>& handle)
{
    const QDebugStateSaver saver(debug);
    debug.nospace() << "QCPHandle(" << handle.get() << ')';
    return debug;
}

#endif // QCP_HANDLE_H
