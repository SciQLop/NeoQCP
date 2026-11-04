#ifndef QCP_CONSTRUCT_H
#define QCP_CONSTRUCT_H

#include "axis/axis.h"
#include "core.h"
#include "items/item.h"
#include "layoutelements/layoutelement-legend.h"
#include "layout.h"
#include "plottables/lane-layout.h"
#include "plottables/plottable-bars.h"
#include "plottables/plottable-group.h"
#include "plottables/plottable.h"
#include <concepts>

/*
  What QCustomPlot::add<T>(args...) does for each family of T: one specialisation of qcp::Construct
  per family, chosen by a concept. Each make() refuses with one misuse and an empty handle, and
  returns the handle of an object the plot owns. A new family is one more specialisation here and
  an OpsFor in T (see QCPObjectOps).

  Why a class template and not a function template: partial specialisation by concept, and
  QCustomPlot::add must name it before the specialisations are defined.
*/
namespace qcp
{
namespace detail
{
template <typename T>
concept Plottable = std::derived_from<T, QCPAbstractPlottable>;
template <typename T>
concept Item = std::derived_from<T, QCPAbstractItem>;
template <typename T>
concept LayoutPart = std::derived_from<T, QCPLayoutElement>;

// Intervals share lane layouts; add<T>() gives each its own, owned by the plot.
// simplify: one layout per intervals plottable; sharing one across several needs a setter on it.
template <typename T>
T* newPlottable(QCustomPlot& plot, QCPAxis* keyAxis, QCPAxis* valueAxis)
{
    if constexpr (std::constructible_from<T, QCPAxis*, QCPAxis*>)
        return new T(keyAxis, valueAxis);
    else
    {
        static_assert(std::constructible_from<T, QCPAxis*, QCPAxis*, QCPLaneLayout*>,
                      "add<T> needs T(keyAxis, valueAxis) or T(keyAxis, valueAxis, laneLayout)");
        return new T(keyAxis, valueAxis, new QCPLaneLayout(&plot));
    }
}

inline bool plotIsUsable(const QCustomPlot& plot)
{
    QCP_CHECK_OR_RETURN(!plot.isBeingDestroyed(), "the plot is being destroyed", false);
    return true;
}
} // namespace detail

// add<T>(setDataArgs...): on the default axes and the current layer, args go to the handle's setData.
template <detail::Plottable T>
struct Construct<T>
{
    template <typename... Args>
    static HandleFor<T> make(QCustomPlot& plot, Args&&... args)
    {
        using Handle = HandleFor<T>;
        if constexpr (sizeof...(Args) > 0)
            static_assert(requires(Handle h) { h.setData(std::declval<Args>()...); },
                          "add<T>(args...): T's handle has no setData taking these arguments");
        const auto [keyAxis, valueAxis] = plot.axesForAdd();
        if (!keyAxis)
            return Handle{};
        Handle handle = makeHandle<Handle>(detail::newPlottable<T>(plot, keyAxis, valueAxis));
        handle.get()->markDirty();
        if constexpr (sizeof...(Args) > 0)
            handle.setData(std::forward<Args>(args)...);
        return handle;
    }
};

// add<T>(): an item takes the plot only.
template <detail::Item T>
struct Construct<T>
{
    static HandleFor<T> make(QCustomPlot& plot)
    {
        using Handle = HandleFor<T>;
        if (!detail::plotIsUsable(plot))
            return Handle{};
        Handle handle = makeHandle<Handle>(new T(&plot));
        handle.get()->markDirty();
        return handle;
    }
};

// add<T>(row, column, ctorArgs...): inserted in the plot layout; ctorArgs follow the plot in T's
// constructor. A taken cell refuses, and the element is deleted.
template <detail::LayoutPart T>
struct Construct<T>
{
    template <typename... Args>
    static HandleFor<T> make(QCustomPlot& plot, int row, int column, Args&&... ctorArgs)
    {
        using Handle = HandleFor<T>;
        static_assert(std::constructible_from<T, QCustomPlot*, Args...>,
                      "add<T>(row, column, args...) needs a T(QCustomPlot*, args...) constructor "
                      "(a legend is created by the plot)");
        if (!detail::plotIsUsable(plot))
            return Handle{};
        auto element = std::make_unique<T>(&plot, std::forward<Args>(ctorArgs)...);
        QCP_CHECK_OR_RETURN(plot.plotLayout()->addElement(row, column, element.get()),
                            "that cell of the plot layout is taken", Handle{});
        return makeHandle<Handle>(element.release());
    }
};

// add<QCPBarsGroup>(): a QObject child of the plot, so deleting the plot frees it.
template <>
struct Construct<QCPBarsGroup>
{
    static HandleFor<QCPBarsGroup> make(QCustomPlot& plot)
    {
        using Handle = HandleFor<QCPBarsGroup>;
        if (!detail::plotIsUsable(plot))
            return Handle{};
        return makeHandle<Handle>(new QCPBarsGroup(&plot));
    }
};

// add<QCPPlottableGroup>(name): created by the plot, deleted with it.
template <>
struct Construct<QCPPlottableGroup>
{
    static HandleFor<QCPPlottableGroup> make(QCustomPlot& plot, const QString& name)
    {
        return makeHandle<HandleFor<QCPPlottableGroup>>(plot.addPlottableGroup(name));
    }
};
} // namespace qcp

#endif // QCP_CONSTRUCT_H
