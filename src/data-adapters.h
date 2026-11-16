#ifndef QCP_DATA_ADAPTERS_H
#define QCP_DATA_ADAPTERS_H

#include <concepts>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace qcp::detail
{
// The fallback of the qcp_numbers call. A C variadic function converts worse than anything a user
// writes, so any hook wins over it; a deleted hook is still the best match, and the call then fails
// to compile, which is how a type opts out. Nothing ever calls it: it is only named, unevaluated.
struct NoHook
{
};

NoHook qcp_numbers(...);

template <typename T>
concept numbersCall = requires { qcp_numbers(std::declval<T>()); };

template <typename T>
concept numbersHook
    = numbersCall<T> && !std::same_as<decltype(qcp_numbers(std::declval<T>())), NoHook>;

template <typename T>
concept numberSource
    = std::ranges::input_range<T> && std::convertible_to<std::ranges::range_value_t<T>, double>;

// A contiguous range of doubles is read where it is: no copy, the caller keeps its storage.
template <typename T>
concept contiguousDoubles = std::ranges::contiguous_range<T>
    && std::same_as<std::remove_cv_t<std::ranges::range_value_t<T>>, double>;

template <typename T>
constexpr auto adaptNumbers(T&& numbers)
{
    if constexpr (contiguousDoubles<T>
                  && (std::is_lvalue_reference_v<T> || std::ranges::borrowed_range<T>))
        return std::span<const double>(numbers);
    else
        return std::views::all(std::forward<T>(numbers));
}

template <typename T>
using hookResult = decltype(qcp_numbers(std::declval<T>()));

template <typename T>
concept numbersHookAdaptable = numbersHook<T> && numberSource<std::remove_cvref_t<hookResult<T>>>;

template <typename T>
concept numbersPlainAdaptable = numbersCall<T> && !numbersHook<T> && numberSource<T>;

struct Numbers
{
    //! Never a span of a container the call would destroy: an rvalue non-borrowed range is owned.
    template <typename T>
        requires numbersHookAdaptable<T> || numbersPlainAdaptable<T>
    constexpr auto operator()(T&& numbers) const
    {
        if constexpr (numbersHook<T>)
        {
            // A reference is the caller's storage and is read where it is. Anything else is the
            // hook's own object: it is moved into the range returned here (never a span), and a
            // const value is decayed first, since the adaptor owns what it reads.
            if constexpr (std::is_reference_v<hookResult<T>>)
                return adaptNumbers(qcp_numbers(std::forward<T>(numbers)));
            else
            {
                std::remove_cvref_t<hookResult<T>> hooked
                    = qcp_numbers(std::forward<T>(numbers));
                return adaptNumbers(std::move(hooked));
            }
        }
        else
            return adaptNumbers(std::forward<T>(numbers));
    }
};
} // namespace qcp::detail

namespace qcp
{
/*!
  The numbers of \a numbers, the one place the data-taking calls read their arguments through.

  ADL finds a user's `qcp_numbers(const T&)` first: it returns anything a range can be made of, and
  the returned range is what the calls read (a span of a contiguous buffer, say). Without a hook,
  an input range whose elements convert to double is adapted itself: a `std::span<const double>`
  when the storage is already doubles and can be shared, `std::views::all` otherwise, and an
  owning view when the range is an rvalue the call would otherwise leave dangling.

  A `void qcp_numbers(const T&) = delete` refuses a type: QCPNumberRange is then false for it.
*/
inline constexpr detail::Numbers numbers;
} // namespace qcp

/*!
  Any range of numbers goes in: the keys, values and column arguments of the data-taking calls are
  constrained by this concept, not written once per container type. A std::vector, a QVector, a
  std::array and a view such as std::views::iota all pass; a range whose elements do not convert to
  double (QStringList, say) does not. A user type passes through its \ref qcp::numbers hook.

  The data-taking calls read a const range once, from begin to end: no random access, no second
  pass, so a non-common view passes and a range with only non-const iteration does not.
*/
template <typename R>
concept QCPNumberRange = requires(const R& r) { qcp::numbers(r); };

namespace qcp
{
namespace detail
{
//! toDoubles() for the calls that hand the numbers to a Qt container: one pass, one allocation.
template <typename Container, QCPNumberRange R>
auto toContainer(const R& r) -> Container
{
    // Not const: a view such as std::views::filter iterates a non-const object only, and the
    // adaptor's copy is this local's.
    auto adapted = qcp::numbers(r);
    Container out;
    if constexpr (std::ranges::sized_range<decltype(adapted)>)
        out.reserve(static_cast<typename Container::size_type>(std::ranges::size(adapted)));
    for (auto&& value : adapted)
        out.push_back(static_cast<double>(value));
    return out;
}
} // namespace detail

/*!
  The numbers of a QCPNumberRange as a std::vector<double>, read once: the output reserves its whole
  size up front when the range is sized.
*/
template <QCPNumberRange R>
auto toDoubles(const R& r) -> std::vector<double>
{
    return detail::toContainer<std::vector<double>>(r);
}
} // namespace qcp

#endif // QCP_DATA_ADAPTERS_H
