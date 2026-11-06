#include "lane-layout.h"
#include <algorithm>
#include <utility>

int QCPLaneLayout::laneIndex(const QString& name)
{
    if (const int i = mNames.indexOf(name); i >= 0)
        return i;
    mNames.append(name);
    mOrder.push_back(mNames.size() - 1);
    notify();
    return mNames.size() - 1;
}

QVector<int> QCPLaneLayout::laneIndices(const QStringList& names)
{
    const Batch batch(this);
    QVector<int> indices;
    for (const auto& name : names)
        indices.append(laneIndex(name));
    return indices;
}

void QCPLaneLayout::setDisplayOrder(const QStringList& names)
{
    const Batch batch(this);
    std::vector<int> order;
    for (const auto& name : names)
        order.push_back(laneIndex(name));
    if (order == mOrder)
        return;
    mOrder = std::move(order);
    notify();
}

QStringList QCPLaneLayout::displayOrder() const
{
    QStringList names;
    for (int lane : mOrder)
        names.append(mNames[lane]);
    return names;
}

bool QCPLaneLayout::renameLane(const QString& from, const QString& to)
{
    const int i = mNames.indexOf(from);
    if (i < 0 || mNames.contains(to))
        return false;
    mNames[i] = to;
    notify();
    return true;
}

// The setters notify only on a change: a changed slot re-applying the current value would
// otherwise notify again, and run itself without end.
void QCPLaneLayout::setLaneHeight(int px)
{
    const int height = std::max(4, px);
    if (height == mLaneHeight)
        return;
    mLaneHeight = height;
    notify();
}

void QCPLaneLayout::setPlacement(Placement placement)
{
    if (placement == mPlacement)
        return;
    mPlacement = placement;
    notify();
}

double QCPLaneLayout::lanePixelHeight(const QRect& axisRect) const
{
    if (mPlacement == plLanes && !mOrder.empty() && axisRect.height() > 0)
        return double(axisRect.height()) / double(totalRows());
    return mLaneHeight;
}

std::optional<QCPLaneBand> QCPLaneLayout::laneBand(int lane, const QRect& axisRect) const
{
    const int position = displayPosition(lane);
    if (position < 0)
        return std::nullopt;
    const double row = lanePixelHeight(axisRect);
    const double top = axisRect.top() + rowsBefore(position) * row;
    return QCPLaneBand { top, top + laneRows(lane) * row };
}

int QCPLaneLayout::laneAt(double y, const QRect& axisRect) const
{
    const double offset = (y - axisRect.top()) / lanePixelHeight(axisRect);
    if (offset < 0)
        return -1;
    int rows = 0;
    for (int lane : mOrder)
    {
        rows += laneRows(lane);
        if (offset < rows)
            return lane;
    }
    return -1;
}

void QCPLaneLayout::setLaneRows(const QObject* owner, std::vector<int> rowsPerLane)
{
    auto it = std::ranges::find(mLaneRows, owner, &decltype(mLaneRows)::value_type::first);
    if (it != mLaneRows.end() && it->second == rowsPerLane)
        return;
    if (it == mLaneRows.end())
        mLaneRows.emplace_back(owner, std::move(rowsPerLane));
    else
        it->second = std::move(rowsPerLane);
    notify();
}

void QCPLaneLayout::removeLaneRows(const QObject* owner)
{
    if (std::erase_if(mLaneRows, [owner](const auto& entry) { return entry.first == owner; }))
        notify();
}

int QCPLaneLayout::laneRows(int lane) const
{
    int rows = 1;
    for (const auto& [owner, perLane] : mLaneRows)
        if (lane >= 0 && lane < static_cast<int>(perLane.size()))
            rows = std::max(rows, perLane[lane]);
    return rows;
}

// simplify: walks the display order on each call, O(lanes); fine for the tens of lanes a
// timeline holds. Cache cumulative rows on notify() if a plot ever has thousands of lanes.
int QCPLaneLayout::rowsBefore(int position) const
{
    int rows = 0;
    for (int i = 0; i < position && i < static_cast<int>(mOrder.size()); ++i)
        rows += laneRows(mOrder[i]);
    return rows;
}

int QCPLaneLayout::totalRows() const
{
    return rowsBefore(static_cast<int>(mOrder.size()));
}

double QCPLaneLayout::laneCentreRow(int lane) const
{
    const int position = displayPosition(lane);
    return position < 0 ? -1 : rowsBefore(position) + laneRows(lane) / 2.0;
}

int QCPLaneLayout::displayPosition(int lane) const
{
    const auto it = std::find(mOrder.begin(), mOrder.end(), lane);
    return it == mOrder.end() ? -1 : static_cast<int>(it - mOrder.begin());
}

QCPLaneLayout::Batch::Batch(QCPLaneLayout* layout) : mLayout(layout)
{
    if (mLayout)
        ++mLayout->mBatchDepth;
}

QCPLaneLayout::Batch::~Batch()
{
    if (mLayout && --mLayout->mBatchDepth == 0)
        mLayout->flush();
}

void QCPLaneLayout::notify()
{
    ++mGeneration;
    mPending = true;
    if (mBatchDepth == 0)
        flush();
}

// The emit runs inside a batch: a receiver re-packing its lanes from the slot (or a slot
// re-packing another timeline) does not emit in the middle of this one, but in the next round,
// which the batch's end starts.
void QCPLaneLayout::flush()
{
    if (!std::exchange(mPending, false))
        return;
    // No plot belongs to a lane layout: the batch guard, not a scope, orders a re-entrant pack.
    const Batch emitting(this);
    emit changed();
}
