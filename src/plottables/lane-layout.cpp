#include "lane-layout.h"
#include <QSignalBlocker>
#include <algorithm>

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
    const auto before = mNames.size();
    QSignalBlocker blocker(this);
    QVector<int> indices;
    for (const auto& name : names)
        indices.append(laneIndex(name));
    blocker.unblock();
    if (mNames.size() != before)
        notify();
    return indices;
}

void QCPLaneLayout::setDisplayOrder(const QStringList& names)
{
    QSignalBlocker blocker(this);
    std::vector<int> order;
    for (const auto& name : names)
        order.push_back(laneIndex(name));
    mOrder = std::move(order);
    blocker.unblock();
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

void QCPLaneLayout::setLaneHeight(int px)
{
    mLaneHeight = std::max(4, px);
    notify();
}

void QCPLaneLayout::setPlacement(Placement placement)
{
    mPlacement = placement;
    notify();
}

std::optional<QCPLaneBand> QCPLaneLayout::laneBand(int lane, const QRect& axisRect) const
{
    const int position = displayPosition(lane);
    if (position < 0)
        return std::nullopt;
    const double top = axisRect.top() + position * mLaneHeight;
    return QCPLaneBand { top, top + mLaneHeight };
}

int QCPLaneLayout::laneAt(double y, const QRect& axisRect) const
{
    const double offset = y - axisRect.top();
    if (offset < 0)
        return -1;
    const auto position = static_cast<std::size_t>(offset / mLaneHeight);
    return position < mOrder.size() ? mOrder[position] : -1;
}

int QCPLaneLayout::displayPosition(int lane) const
{
    const auto it = std::find(mOrder.begin(), mOrder.end(), lane);
    return it == mOrder.end() ? -1 : static_cast<int>(it - mOrder.begin());
}

void QCPLaneLayout::notify()
{
    ++mGeneration;
    emit changed();
}
