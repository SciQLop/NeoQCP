#ifndef QCP_HANDLE_OPS_H
#define QCP_HANDLE_OPS_H

#include "handle.h"
#include <concepts>
#include <type_traits>

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
    static_assert(HasOps<T>, "add<T> needs a plottable (T::OpsFor)");
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

#endif // QCP_HANDLE_OPS_H
