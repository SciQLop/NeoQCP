#ifndef QCP_HANDLE_OPS_H
#define QCP_HANDLE_OPS_H

#include "handle.h"
#include <QString>
#include <concepts>
#include <type_traits>

class QCPLayerable;

namespace qcp::ops
{
// Refuses (one misuse) a layer the plot doesn't have; the handle setters call it.
QCP_LIB_DECL void setLayer(QCPLayerable& layerable, const QString& layerName);
} // namespace qcp::ops

namespace qcp
{
// The type a chained setter returns: the most-derived handle (Self), or the ops class itself
// when used directly (Self = void). C++20 has no deducing this, hence the CRTP parameter.
template <typename Self, typename Own>
using OpsRet = std::conditional_t<std::is_void_v<Self>, Own, Self>;

namespace detail
{
template <typename T>
concept HasOps = requires { typename T::template OpsFor<T>; };

// An inherited Handle wraps a base type: it is not T's own handle.
template <typename T>
concept DeclaresOwnHandle
    = requires { typename T::Handle; } && std::is_base_of_v<QCPHandle<T>, typename T::Handle>;

template <typename T>
struct HandleFor
{
    static_assert(HasOps<T>, "add<T> needs T::OpsFor<T> (every layerable and group declares one) or T::Handle");
    using type = typename T::template OpsFor<T>;
};

template <DeclaresOwnHandle T>
struct HandleFor<T>
{
    using type = typename T::Handle;
};
} // namespace detail

// What QCustomPlot::add<T>() returns: T::Handle when T declares one, else T::OpsFor<T>.
template <typename T>
using HandleFor = typename detail::HandleFor<T>::type;

// Fills the QCPHandle<T> base, so a user Handle needs no constructor.
template <typename H, typename T>
H makeHandle(T* object)
{
    static_assert(std::default_initializable<H>,
                  "a plottable's Handle must be default-constructible: add<T> builds it so");
    H handle;
    static_cast<QCPHandle<T>&>(handle) = QCPHandle<T>(object);
    return handle;
}
} // namespace qcp

/*!
  The fluent handle of any QObject the plot owns: what QCustomPlot::add<T>() returns for a group.
  Every setter is a no-op on a dead handle and returns the handle by value, so a chain never holds
  a reference to a temporary. Subclass contract (one shape for every family): write the setters in
  a class template taking (T, Self) that derives from the parent family's ops with
  \c qcp::OpsRet<Self, ThisOps<T>> as its Self, and declare
  \code template <typename T> using OpsFor = ThisOps<T>; \endcode in T. Self is the complete,
  most-derived handle type; \c Self = void means "this class itself".
*/
template <typename T, typename Self = void>
class QCPObjectOps : public QCPHandle<T>
{
public:
    using Ret = qcp::OpsRet<Self, QCPObjectOps>;
    using QCPHandle<T>::QCPHandle;

    // Any setter without a handle form: change(T&) runs only while the object lives.
    template <typename Change>
    Ret with(Change&& change)
    {
        return apply(std::forward<Change>(change));
    }

protected:
    // Runs the change, then marks a layerable dirty (one repaint per event-loop turn, however long
    // the chain). The object is re-checked after the change: a slot of it may have deleted it.
    template <typename Change>
    Ret apply(Change&& change)
    {
        static_assert(std::is_base_of_v<QCPObjectOps, Ret>,
                      "Self must be the complete handle type deriving from these ops (CRTP)");
        if (T* object = this->get())
        {
            change(*object);
            if constexpr (std::derived_from<T, QCPLayerable>)
                if (T* alive = this->get())
                    alive->markDirty();
        }
        return static_cast<const Ret&>(*this);
    }
};

// The handle of a layerable (axis, axis rect, text element, colour scale, legend, ...).
template <typename T, typename Self = void>
class QCPLayerableOps : public QCPObjectOps<T, qcp::OpsRet<Self, QCPLayerableOps<T>>>
{
    using Base = QCPObjectOps<T, qcp::OpsRet<Self, QCPLayerableOps<T>>>;

public:
    using typename Base::Ret;
    using Base::Base;

    Ret setVisible(bool visible)
    {
        return this->apply([&](T& l) { l.setVisible(visible); });
    }
    Ret setLayer(const QString& layerName)
    {
        return this->apply([&](T& l) { qcp::ops::setLayer(l, layerName); });
    }
};

#endif // QCP_HANDLE_OPS_H
