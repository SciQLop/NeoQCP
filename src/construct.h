#ifndef QCP_CONSTRUCT_H
#define QCP_CONSTRUCT_H

#include "axis/axis.h"
#include "core.h"
#include "items/item.h"
#include "layoutelements/layoutelement-legend.h"
#include "layout.h"
#include "plottables/lane-layout.h"
#include "plottables/plottable-bars.h"
#include "layoutelements/layoutelement-axisrect.h"
#include "plottables/plottable-group.h"
#include "plottables/plottable.h"
#include "polar/angularaxis.h"
#include "polar/polar-area.h"
#include "polar/radialaxis.h"
#include <concepts>
#include <memory>

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
concept Plottable = std::derived_from<T, QCPPlottable>;
template <typename T>
concept Item = std::derived_from<T, QCPItem>;
template <typename T>
concept LayoutPart = std::derived_from<T, QCPLayoutElement>;
// addAxis returns a QCPAxis*, and QCPAxis::OpsFor is the only axis handle: the family is one type.
template <typename T>
concept Axis = std::same_as<T, QCPAxis>;

inline bool plotIsUsable(const QCustomPlot& plot)
{
    QCP_CHECK_OR_RETURN(!plot.isBeingDestroyed(), "the plot is being destroyed", false);
    return true;
}

inline bool plotHasAxisRect(const QCustomPlot& plot)
{
    if (!plotIsUsable(plot))
        return false;
    QCP_CHECK_OR_RETURN(plot.axisRect(), "the plot has no axis rect", false);
    return true;
}

// A cell at most one row and one column past the grid: the grid grows by that much, no more.
inline bool cellIsNearTheGrid(const QCPLayoutGrid& layout, int row, int column)
{
    QCP_CHECK_OR_RETURN(row >= 0 && row <= layout.rowCount() && column >= 0
                            && column <= layout.columnCount(),
                        "that cell is not within one row or column of the plot layout", false);
    return true;
}
} // namespace detail

// add<T>(ctorArgs...): T is built from its own arguments, then joins the default axes' system and
// the current layer. A frame that refuses it (no axis rect, capabilities it lacks) reports once
// and the object is deleted: the caller gets an empty handle, never an inert plottable.
template <detail::Plottable T>
struct Construct<T>
{
    template <typename... Args>
    static HandleFor<T> make(QCustomPlot& plot, Args&&... args)
    {
        static_assert(std::constructible_from<T, Args...>,
                      "add<T>(args...): T has no constructor taking these arguments");
        return plot.add(std::make_unique<T>(std::forward<Args>(args)...));
    }
};

// add<T>(ctorArgs...): an item is built from its own arguments, then placed on the plot's axis rect
// (adopt() binds it, clips it to the rect and puts it on the annotations layer).
template <detail::Item T>
struct Construct<T>
{
    template <typename... Args>
    static HandleFor<T> make(QCustomPlot& plot, Args&&... args)
    {
        static_assert(std::constructible_from<T, Args...>,
                      "add<T>(args...): T has no constructor taking these arguments");
        return plot.add(std::make_unique<T>(std::forward<Args>(args)...));
    }
};

// add<QCPAxis>(type): a new axis on the default axis rect, on its side.
template <detail::Axis T>
struct Construct<T>
{
    static HandleFor<T> make(QCustomPlot& plot, QCPAxis::AxisType type)
    {
        using Handle = HandleFor<T>;
        if (!detail::plotHasAxisRect(plot))
            return Handle{};
        QCPAxis* axis = plot.axisRect()->addAxis(type);
        if (!axis)
            return Handle{};
        Handle handle = makeHandle<Handle>(axis);
        handle.get()->update();
        return handle;
    }
};

// add<T>(row, column, ctorArgs...): inserted in the plot layout, at most one row or column past it; ctorArgs follow the plot in T's
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
        if (!detail::plotIsUsable(plot) || !detail::cellIsNearTheGrid(*plot.plotLayout(), row, column))
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
        using Handle = HandleFor<QCPPlottableGroup>;
        if (!detail::plotIsUsable(plot))
            return Handle{};
        return makeHandle<Handle>(plot.addPlottableGroup(name).get());
    }
};
} // namespace qcp

// The plottable forms of QCPAxisRect::add: T is built from its own arguments, then joins the
// system of this rect's default pair (its first bottom and left axes), or of the pair given
// explicitly. The frame is the one gate: a system that refuses the plottable reports once, and the
// object is deleted.
template <typename T, typename... Args>
    requires std::derived_from<T, QCPPlottable>
          && qcp::FirstArgumentIsNotAnAxis<Args...>
qcp::HandleFor<T> QCPAxisRect::add(Args&&... args)
{
    QCPAxis* key = xAxis();
    QCPAxis* value = yAxis();
    QCP_CHECK_OR_RETURN(key && value, "the axis rect has no bottom or left axis",
                        qcp::HandleFor<T> { });
    return add<T>(key, value, std::forward<Args>(args)...);
}

template <typename T, typename... Args>
    requires std::derived_from<T, QCPPlottable>
qcp::HandleFor<T> QCPAxisRect::add(QCPAxis* key, QCPAxis* value, Args&&... args)
{
    using Handle = qcp::HandleFor<T>;
    static_assert(std::constructible_from<T, Args...>,
                  "add<T>(args...): T has no constructor taking these arguments");
    // The pair must be this rect's: another rect's axes would put the plottable in another area,
    // and half of this rect's arguments would be ignored.
    const QList<QCPAxis*> own = axesRaw();
    QCP_CHECK_OR_RETURN(key && value && own.contains(key) && own.contains(value),
                        "both axes of a plottable on this rect must belong to it", Handle { });
    QCP_CHECK_OR_RETURN(!mParentPlot->isBeingDestroyed(), "the plot is being destroyed", Handle { });
    QCPCoordinateSystem* system = this->system(key, value).get();
    if (!system)
        return Handle { };
    // The caller's slots run once the handle exists, not before it: the plottable is whole here.
    const QCPEmissionScope scope(mParentPlot);
    auto created = std::make_unique<T>(std::forward<Args>(args)...);
    if (!mParentPlot->adoptPlottable(created.get(), system, this))
        return Handle { };
    Handle handle = qcp::makeHandle<Handle>(created.release());
    handle.get()->update();
    return handle;
}

// The item form of QCPAxisRect::add: built from the item's own arguments, then adopted into this
// rect, clipped to it and bound to its first bottom and left axes.
template <typename T, typename... Args>
    requires std::derived_from<T, QCPItem>
qcp::HandleFor<T> QCPAxisRect::add(Args&&... args)
{
    using Handle = qcp::HandleFor<T>;
    static_assert(std::constructible_from<T, Args...>,
                  "add<T>(args...): T has no constructor taking these arguments");
    QCP_CHECK_OR_RETURN(!mParentPlot->isBeingDestroyed(), "the plot is being destroyed", Handle { });
    auto created = std::make_unique<T>(std::forward<Args>(args)...);
    // adopt() reports the refusal itself: one report per refused item, not two.
    if (!mParentPlot->adopt(created.get(), this))
        return Handle { };
    Handle handle = qcp::makeHandle<Handle>(created.release());
    handle.get()->update();
    return handle;
}

// The plottable forms of QCPPolarArea::add: on the area's system (its angular axis and its first
// radial axis), or the object of another area readded here. Built from its own arguments, like
// every other add<T>.
template <typename T, typename... Args>
    requires std::derived_from<T, QCPPlottable>
qcp::HandleFor<T> QCPPolarArea::add(Args&&... args)
{
    static_assert(std::constructible_from<T, Args...>,
                  "add<T>(args...): T has no constructor taking these arguments");
    return add(std::make_unique<T>(std::forward<Args>(args)...));
}

template <typename T>
    requires std::derived_from<T, QCPPlottable>
qcp::HandleFor<T> QCPPolarArea::add(std::unique_ptr<T> plottable)
{
    using Handle = qcp::HandleFor<T>;
    QCP_CHECK_OR_RETURN(plottable, "there is no plottable to add", Handle { });
    QCP_CHECK_OR_RETURN(!plottable->parentPlot(),
                        "the object already belongs to a plot: take it first", Handle { });
    QCP_CHECK_OR_RETURN(plotTakesNewParts(), "the plot is being destroyed", Handle { });
    QCP_CHECK_OR_RETURN(angularAxis() && radialAxis(0), "the polar area has no axes", Handle { });
    QCPCoordinateSystem* system = this->system(angularAxis(), radialAxis(0));
    QCP_CHECK_OR_RETURN(system, "the polar axes are not a system of this area", Handle { });
    // The frame is the gate: a system that refuses this plottable's capabilities reports once.
    const QCPEmissionScope scope(mParentPlot);
    T* object = plottable.get();
    if (!mParentPlot->adoptPlottable(object, system, this))
        return Handle { };
    plottable.release();
    Handle handle = qcp::makeHandle<Handle>(object);
    object->update();
    return handle;
}

#endif // QCP_CONSTRUCT_H
