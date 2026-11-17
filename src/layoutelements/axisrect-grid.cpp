#include "axisrect-grid.h"

#include "../alignment-group.h"
#include "../core.h"
#include "../items/item.h"
#include "../layout.h"
#include "../misuse.h"
#include "../plottables/plottable.h"

#include "layoutelement-axisrect.h"
#include "layoutelement-legend.h"

#include <cmath>

namespace
{
bool dropElement(const QPointer<QCustomPlot>& plot, QCPLayoutElement* element);

//! True when every factor can reach the layout: a non-positive or NaN one must not.
bool usableStretch(const QList<double>& factors)
{
    for (const double factor : factors)
        if (!std::isfinite(factor) || factor <= 0.0)
            return false;
    return true;
}

//! Drops everything an area holds in its inset layout: nested rects take their own insets along.
bool dropInsetChildren(const QPointer<QCustomPlot>& plot, QCPArea& area)
{
    const QCPHandle<QCPLayoutInset> inset = area.insetLayout();
    if (!inset)
        return static_cast<bool>(plot);
    const QList<QCPHandle<QCPLayoutElement>> children = inset->elements(false);
    for (const QCPHandle<QCPLayoutElement>& child : children)
    {
        if (!plot)
            return false;
        if (!child)
            continue;
        if (!dropElement(plot, child.get()))
            return false;
    }
    return true;
}

//! Drops a rect the way its owners do: plottables and items first (the legend loses their rows),
//! then the rect, whose destructor drops its colour scale and inset layout.
bool dropRect(const QPointer<QCustomPlot>& plot, QCPAxisRect& rect)
{
    QCPHandle<QCPAxisRect> alive(&rect);
    if (!dropInsetChildren(plot, rect))
        return false;
    if (!plot || !alive) // a destroyed slot took the rect with its last inset child
        return static_cast<bool>(plot);
    const QList<QCPHandle<QCPPlottable>> plottables = rect.plottables();
    for (const QCPHandle<QCPPlottable>& plottable : plottables)
    {
        if (!plot)
            return false;
        if (!plottable) // a destroyed slot took it between the listing and here
            continue;
        static_cast<void>(plot->removePlottable(plottable.get()));
    }
    if (!plot || !alive)
        return static_cast<bool>(plot);
    const QList<QCPHandle<QCPItem>> items = rect.items();
    for (const QCPHandle<QCPItem>& item : items)
    {
        if (!plot)
            return false;
        if (!item)
            continue;
        static_cast<void>(plot->removeItem(item.get()));
    }
    if (!plot || !alive)
        return static_cast<bool>(plot);
    alive.remove();
    return static_cast<bool>(plot);
}

//! Drops a layout element and, for a layout, its children first.
bool dropElement(const QPointer<QCustomPlot>& plot, QCPLayoutElement* element)
{
    if (!plot)
        return false;
    if (!element)
        return true;
    QCPHandle<QCPLayoutElement> alive(element);
    if (auto* rect = qobject_cast<QCPAxisRect*>(element))
        return dropRect(plot, *rect);
    if (auto* layout = qobject_cast<QCPLayout*>(element))
    {
        const QList<QCPHandle<QCPLayoutElement>> children = layout->elements(false);
        for (const QCPHandle<QCPLayoutElement>& child : children)
        {
            if (!plot)
                return false;
            if (!child)
                continue;
            if (!dropElement(plot, child.get()))
                return false;
        }
        if (!alive)
            return true;
    }
    alive.remove();
    return static_cast<bool>(plot);
}

//! A grid is built from axis rects only: a title row or a nested layout is refused (P-6, M8).
bool gridCellsAreRects(QCPLayoutGrid& layout)
{
    for (int row = 0; row < layout.rowCount(); ++row)
        for (int column = 0; column < layout.columnCount(); ++column)
        {
            if (!layout.hasElement(row, column))
                continue;
            if (!qobject_cast<QCPAxisRect*>(layout.element(row, column).get()))
                return false;
        }
    return true;
}

void dissolveGroup(const QPointer<QCPAlignmentGroup>& group)
{
    if (!group)
        return;
    const QList<QCPHandle<QCPLayoutElement>> members = group->elements();
    for (const QCPHandle<QCPLayoutElement>& member : members)
        if (member)
            group->remove(member.get());
}

QList<QCPLayoutElement*> gridColumn(QCPLayoutGrid& layout, int column, int rows)
{
    QList<QCPLayoutElement*> members;
    members.reserve(rows);
    for (int row = 0; row < rows; ++row)
        members.append(layout.element(row, column).get());
    return members;
}

QList<QCPLayoutElement*> gridRow(QCPLayoutGrid& layout, int row, int columns)
{
    QList<QCPLayoutElement*> members;
    members.reserve(columns);
    for (int column = 0; column < columns; ++column)
        members.append(layout.element(row, column).get());
    return members;
}

//! Takes the plot's legend out of the dropped rect it sits in, so the plot keeps its legend.
void rescueLegend(QCPLayoutGrid& layout, const QPointer<QCPLayoutElement>& legend, int rows,
                  int columns)
{
    if (!legend)
        return;
    const int oldRows = layout.rowCount();
    const int oldColumns = layout.columnCount();
    for (int row = 0; row < oldRows; ++row)
        for (int column = 0; column < oldColumns; ++column)
        {
            if (row < rows && column < columns)
                continue;
            if (!layout.hasElement(row, column))
                continue;
            auto* rect = qobject_cast<QCPAxisRect*>(layout.element(row, column).get());
            if (!rect)
                continue;
            const QList<QCPHandle<QCPLayoutElement>> children
                = rect->insetLayout()->elements(false);
            for (const QCPHandle<QCPLayoutElement>& child : children)
                if (child.get() == legend.get())
                {
                    static_cast<void>(rect->insetLayout()->take(legend.get()));
                    return;
                }
        }
}
} // namespace

bool qcp::applyAxisRectGrid(QCPLayoutGrid& layout, int rows, int columns,
                            const QCPAxisRectGridOptions& options, QCPAxisRectGridState& state,
                            const std::function<QCPAxisRect*()>& makeRect)
{
    QCP_CHECK_OR_RETURN(rows > 0, "a grid needs at least one row", false);
    QCP_CHECK_OR_RETURN(columns > 0, "a grid needs at least one column", false);
    QCP_CHECK_OR_RETURN(options.rowStretch.size() <= rows,
                        "more row stretch factors than the grid has rows", false);
    QCP_CHECK_OR_RETURN(options.columnStretch.size() <= columns,
                        "more column stretch factors than the grid has columns", false);
    QCP_CHECK_OR_RETURN(usableStretch(options.rowStretch),
                        "row stretch factors must be finite and greater than zero", false);
    QCP_CHECK_OR_RETURN(usableStretch(options.columnStretch),
                        "column stretch factors must be finite and greater than zero", false);
    const QPointer<QCustomPlot> plot(layout.parentPlot().get());
    QCP_CHECK_OR_RETURN(plot, "the layout belongs to no plot", false);
    QCP_CHECK_OR_RETURN(gridCellsAreRects(layout),
                        "the plot layout holds something that is not an axis rect (a title row, a "
                        "nested layout, a polar area): setAxisRectGrid builds from axis rects only",
                        false);

    // Undo the previous call's groups: their members are about to be regrouped by row and column.
    const QList<QPointer<QCPAlignmentGroup>> oldColumnGroups = state.columnGroups;
    const QList<QPointer<QCPAlignmentGroup>> oldRowGroups = state.rowGroups;
    state.columnGroups.clear();
    state.rowGroups.clear();
    state.shares.clear(); // the shared-axis step rebuilds them from the new options
    for (const QPointer<QCPAlignmentGroup>& group : oldColumnGroups)
    {
        if (!plot)
            return false;
        dissolveGroup(group);
    }
    for (const QPointer<QCPAlignmentGroup>& group : oldRowGroups)
    {
        if (!plot)
            return false;
        dissolveGroup(group);
    }

    const QPointer<QCPLayoutElement> legend(plot->legend.get());
    rescueLegend(layout, legend, rows, columns);

    // Drop the rects outside the new grid, with everything they own (C1).
    const int oldRows = layout.rowCount();
    const int oldColumns = layout.columnCount();
    for (int row = 0; row < oldRows; ++row)
        for (int column = 0; column < oldColumns; ++column)
        {
            if (row < rows && column < columns)
                continue;
            if (!layout.hasElement(row, column))
                continue;
            if (!dropElement(plot, layout.element(row, column).get()))
                return false;
        }

    if (!plot)
        return false;
    layout.setFillOrder(QCPLayoutGrid::foColumnsFirst, false);
    if (!plot)
        return false;

    // Fill the missing cells, keeping the rects already in the kept cells.
    for (int row = 0; row < rows; ++row)
        for (int column = 0; column < columns; ++column)
        {
            if (layout.hasElement(row, column))
                continue;
            static_cast<void>(layout.addElement(row, column, makeRect()));
        }

    // Only the rows and columns past the new grid are empty now: the kept ones were just filled.
    if (!plot)
        return false;
    layout.simplify();
    if (!plot)
        return false;

    if (legend && !legend->layout())
        if (auto* home = qobject_cast<QCPAxisRect*>(layout.element(0, 0).get()))
            home->insetLayout()->addElement(legend.get(), Qt::AlignRight | Qt::AlignTop);

    QList<double> rowStretch;
    rowStretch.reserve(rows);
    for (int row = 0; row < rows; ++row)
        rowStretch.append(options.rowStretch.value(row, 1.0));
    layout.setRowStretchFactors(rowStretch);
    if (!plot)
        return false;

    QList<double> columnStretch;
    columnStretch.reserve(columns);
    for (int column = 0; column < columns; ++column)
        columnStretch.append(options.columnStretch.value(column, 1.0));
    layout.setColumnStretchFactors(columnStretch);
    if (!plot)
        return false;

    // One group per column and per row: every column's data edges share a left and a right edge,
    // every row's a top and a bottom one, shared axes or not (P-1). A single rect needs no group.
    for (int column = 0; column < columns; ++column)
    {
        const QList<QCPLayoutElement*> members = gridColumn(layout, column, rows);
        if (members.size() > 1)
            state.columnGroups.append(
                QCPAlignmentGroup::create(members, QCP::msLeft | QCP::msRight));
    }
    for (int row = 0; row < rows; ++row)
    {
        const QList<QCPLayoutElement*> members = gridRow(layout, row, columns);
        if (members.size() > 1)
            state.rowGroups.append(QCPAlignmentGroup::create(members, QCP::msTop | QCP::msBottom));
    }

    state.options = options;
    state.rows = rows;
    state.columns = columns;
    return true;
}
