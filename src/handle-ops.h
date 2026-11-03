#ifndef QCP_HANDLE_OPS_H
#define QCP_HANDLE_OPS_H

#include "handle.h"
#include <type_traits>

// The type a chained setter returns: the most-derived handle (Self), or the ops class itself
// when used directly (Self = void). C++20 has no deducing this, hence the CRTP parameter.
template <typename Self, typename Own>
using QCPOpsRet = std::conditional_t<std::is_void_v<Self>, Own, Self>;

namespace qcp::detail
{
// An inherited Handle wraps a base type: it is not T's own handle.
template <typename T>
concept DeclaresOwnHandle
    = requires { typename T::Handle; } && std::is_base_of_v<QCPHandle<T>, typename T::Handle>;

template <typename T>
struct HandleFor
{
    using type = typename T::template OpsFor<T>;
};

template <DeclaresOwnHandle T>
struct HandleFor<T>
{
    using type = typename T::Handle;
};
} // namespace qcp::detail

// What QCustomPlot::add<T>() returns: T::Handle when T declares one, else T::OpsFor<T>.
template <typename T>
using QCPHandleFor = typename qcp::detail::HandleFor<T>::type;

// Fills the QCPHandle<T> base, so a user Handle needs no constructor.
template <typename H, typename T>
H qcpMakeHandle(T* object)
{
    H handle;
    static_cast<QCPHandle<T>&>(handle) = QCPHandle<T>(object);
    return handle;
}

#endif // QCP_HANDLE_OPS_H
