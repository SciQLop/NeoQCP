#include "axisrect-grid.h"

#include "../alignment-group.h"
#include "../core.h"
#include "../items/item.h"
#include "../layout.h"
#include "../misuse.h"
#include "../plottables/plottable.h"

#include "layoutelement-axisrect.h"
#include "layoutelement-legend.h"

#include <algorithm>
#include <optional>
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

//! Held while applyAxisRectGrid walks the layout: a slot that re-enters finds it set.
class RunningGuard
{
public:
    RunningGuard(QCustomPlot* plot, bool& flag)
        : mPlot(plot)
        , mFlag(flag)
    {
        mFlag = true;
    }
    // A slot may delete the plot mid-walk; the flag then lives in freed memory, so it is only
    // written while the plot is still there.
    ~RunningGuard()
    {
        if (mPlot)
            mFlag = false;
    }
    RunningGuard(const RunningGuard&) = delete;
    RunningGuard& operator=(const RunningGuard&) = delete;

private:
    QPointer<QCustomPlot> mPlot;
    bool& mFlag;
};

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

// Undoes the shares a previous call made (P-5): the grid's followers leave their sources, the axes
// the share handed over return to the source they had (a full axes box's top axis, say), and the
// tick labels the grid hid show again (even if the user set them while shared: the grid cannot
// tell). A label the user hid before the share stays hidden, and an axis the user relinked since
// is theirs: the grid leaves it as it is.
void unshare(QList<QCPAxisRectGridState::Share>& shares)
{
    QList<QCPAxisRectGridState::Share> records = shares;
    shares.clear();
    records.removeIf([](const QCPAxisRectGridState::Share& record)
                     { return !record.follower || record.follower->linkSource() != record.sharedSource; });
    for (const auto& record : records)
        if (record.follower)
            record.follower->unlink();
    for (const auto& record : records)
    {
        if (!record.follower)
            continue;
        if (record.originalSource)
            record.follower->linkTo(record.originalSource);
        if (record.hidLabels)
            record.follower->setTickLabels(true);
    }
}

// The (follower, source) pairs the options ask for. A cell this call has not built yet gives a null
// axis: it will be a fresh one, with no link to check. A rect without the axis gives no pair.
QList<std::pair<QCPAxis*, QCPAxis*>> plannedShares(QCPLayoutGrid& layout, int rows, int columns,
                                                   const QCPAxisRectGridOptions& options)
{
    using Pick = QCPAxis* (*)(QCPAxisRect*);
    const auto axis = [&](int row, int column, Pick pick) -> std::optional<QCPAxis*>
    {
        if (row >= layout.rowCount() || column >= layout.columnCount())
            return nullptr;
        auto* rect = qobject_cast<QCPAxisRect*>(layout.element(row, column).get());
        if (!rect)
            return nullptr;
        if (QCPAxis* found = pick(rect))
            return found;
        return std::nullopt;
    };
    const Pick x = [](QCPAxisRect* rect) { return rect->xAxis().get(); };
    const Pick y = [](QCPAxisRect* rect) { return rect->yAxis().get(); };
    QList<std::pair<QCPAxis*, QCPAxis*>> pairs;
    const auto plan = [&](std::optional<QCPAxis*> follower, std::optional<QCPAxis*> source)
    {
        if (follower && source)
            pairs.append(std::pair { *follower, *source });
    };
    if (options.shareX)
        for (int column = 0; column < columns; ++column)
            for (int row = 0; row < rows - 1; ++row)
                plan(axis(row, column, x), axis(rows - 1, column, x));
    if (options.shareY)
        for (int row = 0; row < rows; ++row)
            for (int column = 1; column < columns; ++column)
                plan(axis(row, column, y), axis(row, 0, y));
    return pairs;
}

// Whether every planned share can be an identity link: no axis taking part follows or drives a
// link through functions (a top axis in other units, say), which a share would break, and no source
// already follows the axis that would follow it.
bool sharesAreLinkable(const QList<std::pair<QCPAxis*, QCPAxis*>>& pairs)
{
    const auto mapped = [](const QCPAxisBase* axis)
    { return axis->linkSource() && !axis->isIdentityFollower(); };
    const auto drivesMapped = [](const QCPAxisBase* axis)
    {
        const auto users = axis->linkedFollowers();
        return std::any_of(users.cbegin(), users.cend(),
                           [](const QCPAxisBase* user) { return !user->isIdentityFollower(); });
    };
    return std::none_of(pairs.cbegin(), pairs.cend(),
                        [&](const auto& pair)
                        {
                            const auto [follower, source] = pair;
                            // A source that follows its own follower would turn the link round.
                            const bool reversed = follower && source && source->isIdentityFollower()
                                && source->linkSource() == follower;
                            return (follower && (mapped(follower) || drivesMapped(follower)))
                                || (source && mapped(source)) || reversed;
                        });
}

// Shares the planned pairs (P-2): records what unsharing restores first, for every axis taking part,
// before any link moves (C4: the identity followers a follower drives are handed to its source),
// then links and hides the followers' tick labels.
void shareAxes(QCPLayoutGrid& layout, int rows, int columns, const QCPAxisRectGridOptions& options,
               QList<QCPAxisRectGridState::Share>& shares)
{
    QList<std::pair<QCPAxis*, QCPAxis*>> pairs = plannedShares(layout, rows, columns, options);
    pairs.removeIf([](const auto& pair) { return !pair.first || !pair.second; }); // every cell is built now
    const auto recorded = [&](const QCPAxis* axis)
    {
        return std::any_of(shares.cbegin(), shares.cend(),
                           [axis](const QCPAxisRectGridState::Share& s) { return s.follower == axis; });
    };
    for (const auto& [follower, source] : pairs)
    {
        if (!recorded(follower))
            shares.append(QCPAxisRectGridState::Share {
                follower,
                follower->isIdentityFollower() ? qobject_cast<QCPAxis*>(follower->linkSource())
                                               : nullptr,
                false });
        for (QCPAxisBase* user : follower->identityFollowers())
            if (auto* axis = qobject_cast<QCPAxis*>(user); axis && !recorded(axis))
                shares.append(QCPAxisRectGridState::Share { axis, follower, false });
    }
    for (const auto& [follower, source] : pairs)
    {
        if (follower == source)
            continue;
        follower->linkTo(source);
        if (!follower->isIdentityFollower()) // refused (checked up front: a guard only)
            continue;
        auto& record = *std::find_if(shares.begin(), shares.end(),
                                     [follower](const auto& s) { return s.follower == follower; });
        if (follower->tickLabels() && !record.hidLabels)
        {
            record.hidLabels = true;
            follower->setTickLabels(false);
        }
    }
    for (auto& record : shares)
        if (record.follower && record.follower->isIdentityFollower())
            record.sharedSource = record.follower->linkSource();
}

//! Takes the plot's legend out of the dropped rect it sits in, so the plot keeps its legend.
//! Returns whether it did: a legend the user had taken out of every layout stays out.
bool rescueLegend(QCPLayoutGrid& layout, const QPointer<QCPLayoutElement>& legend, int rows,
                  int columns)
{
    if (!legend)
        return false;
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
                    return true;
                }
        }
    return false;
}
} // namespace

bool qcp::applyAxisRectGrid(QCPLayoutGrid& layout, int rows, int columns,
                            const QCPAxisRectGridOptions& options, QCPAxisRectGridState& state,
                            const std::function<QCPAxisRect*()>& makeRect)
{
    const QPointer<QCustomPlot> plot(layout.parentPlot().get());
    QCP_CHECK_OR_RETURN(!state.running,
                        "setAxisRectGrid is already running (re-entered from a slot?): nothing "
                        "changed",
                        false);
    const RunningGuard running(plot, state.running);
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
    QCP_CHECK_OR_RETURN(plot, "the layout belongs to no plot", false);
    QCP_CHECK_OR_RETURN(gridCellsAreRects(layout),
                        "the plot layout holds something that is not an axis rect (a title row, a "
                        "nested layout, a polar area): setAxisRectGrid builds from axis rects only",
                        false);
    QCP_CHECK_OR_RETURN(sharesAreLinkable(plannedShares(layout, rows, columns, options)),
                        "an axis the grid would share follows or drives a link through functions "
                        "(an axis in other units), or a source already follows the axis that would "
                        "follow it: unlink it first",
                        false);

    // Undo the previous call's groups: their members are about to be regrouped by row and column.
    const QList<QPointer<QCPAlignmentGroup>> oldColumnGroups = state.columnGroups;
    const QList<QPointer<QCPAlignmentGroup>> oldRowGroups = state.rowGroups;
    state.columnGroups.clear();
    state.rowGroups.clear();
    unshare(state.shares); // the shares are rebuilt below from the new options
    if (!plot)
        return false;
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
    const bool legendWasRescued = rescueLegend(layout, legend, rows, columns);

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

    if (legendWasRescued && legend && !legend->layout())
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

    shareAxes(layout, rows, columns, options, state.shares);
    if (!plot)
        return false;

    state.options = options;
    state.rows = rows;
    state.columns = columns;
    return true;
}
