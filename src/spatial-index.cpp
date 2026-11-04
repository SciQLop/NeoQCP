#include "spatial-index.h"

#include <cmath>
#include <numeric>

bool QCPSpatialIndex::hasNan(const QCPDataBox& box)
{
    return std::isnan(box.keyLower) || std::isnan(box.keyUpper) || std::isnan(box.valueLower)
        || std::isnan(box.valueUpper);
}

double QCPSpatialIndex::centre(double lower, double upper)
{
    if (std::isfinite(lower) && std::isfinite(upper))
        return lower + (upper - lower) / 2.0;
    if (std::isfinite(lower))
        return lower;
    if (std::isfinite(upper))
        return upper;
    return 0.0;
}

QCPDataBox QCPSpatialIndex::united(const QCPDataBox& a, const QCPDataBox& b)
{
    return {std::min(a.keyLower, b.keyLower), std::max(a.keyUpper, b.keyUpper),
            std::min(a.valueLower, b.valueLower), std::max(a.valueUpper, b.valueUpper)};
}

QCPDataBox QCPSpatialIndex::boxOf(const std::vector<Entry>& entries, std::uint32_t first,
                                  std::uint32_t count)
{
    QCPDataBox box = entries[first].box;
    for (std::uint32_t i = first + 1; i < first + count; ++i)
        box = united(box, entries[i].box);
    return box;
}

std::uint32_t QCPSpatialIndex::addNode(QCPDataBox box, std::uint32_t first, std::uint32_t count,
                                       bool leaf)
{
    mNodes.push_back({box, first, count, leaf});
    return static_cast<std::uint32_t>(mNodes.size() - 1);
}

void QCPSpatialIndex::packEntries()
{
    const std::size_t n = mEntries.size();
    const std::size_t leafCount = (n + kFanout - 1) / kFanout;
    const std::size_t slices =
        static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<double>(leafCount))));

    std::vector<std::size_t> order(n);
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(order.begin(), order.end(), [this](std::size_t a, std::size_t b) {
        return keyCentre(mEntries[a].box) < keyCentre(mEntries[b].box);
    });
    const std::size_t sliceSize = (n + slices - 1) / slices;
    for (std::size_t start = 0; start < n; start += sliceSize)
    {
        const std::size_t stop = std::min(n, start + sliceSize);
        std::stable_sort(order.begin() + static_cast<std::ptrdiff_t>(start),
                         order.begin() + static_cast<std::ptrdiff_t>(stop),
                         [this](std::size_t a, std::size_t b) {
                             return valueCentre(mEntries[a].box) < valueCentre(mEntries[b].box);
                         });
    }

    std::vector<Entry> sorted;
    sorted.reserve(n);
    for (const std::size_t index : order)
        sorted.push_back(mEntries[index]);
    mEntries.swap(sorted);
}

std::vector<std::uint32_t> QCPSpatialIndex::packLevel(std::vector<std::uint32_t> level)
{
    const std::size_t n = level.size();
    const std::size_t parentCount = (n + kFanout - 1) / kFanout;
    const std::size_t slices =
        static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<double>(parentCount))));

    auto keyOf = [this](std::uint32_t node) { return keyCentre(mNodes[node].box); };
    auto valueOf = [this](std::uint32_t node) { return valueCentre(mNodes[node].box); };
    std::stable_sort(level.begin(), level.end(),
                     [&keyOf](std::uint32_t a, std::uint32_t b) { return keyOf(a) < keyOf(b); });
    const std::size_t sliceSize = (n + slices - 1) / slices;
    for (std::size_t start = 0; start < n; start += sliceSize)
    {
        const std::size_t stop = std::min(n, start + sliceSize);
        std::stable_sort(level.begin() + static_cast<std::ptrdiff_t>(start),
                         level.begin() + static_cast<std::ptrdiff_t>(stop),
                         [&valueOf](std::uint32_t a, std::uint32_t b) {
                             return valueOf(a) < valueOf(b);
                         });
    }

    std::vector<std::uint32_t> parents;
    parents.reserve(parentCount);
    for (std::size_t start = 0; start < n; start += kFanout)
    {
        const std::uint32_t count = static_cast<std::uint32_t>(std::min<std::size_t>(kFanout, n - start));
        QCPDataBox box = mNodes[level[start]].box;
        for (std::uint32_t i = 1; i < count; ++i)
            box = united(box, mNodes[level[start + i]].box);
        const std::uint32_t first = static_cast<std::uint32_t>(mLinks.size());
        for (std::uint32_t i = 0; i < count; ++i)
            mLinks.push_back(level[start + i]);
        parents.push_back(addNode(box, first, count, false));
    }
    return parents;
}

void QCPSpatialIndex::build(std::vector<std::pair<QCPDataBox, int>> entries)
{
    mEntries.clear();
    mNodes.clear();
    mLinks.clear();
    mRoot = 0;

    // An entry with a NaN side describes nothing drawable: it is dropped rather
    // than indexed, since every comparison against NaN would be false anyway.
    mEntries.reserve(entries.size());
    for (const auto& [box, payload] : entries)
        if (!hasNan(box))
            mEntries.push_back({box, payload});
    if (mEntries.empty())
        return;

    std::vector<std::uint32_t> level;
    if (mEntries.size() <= kFanout)
    {
        level.push_back(addNode(boxOf(mEntries, 0, static_cast<std::uint32_t>(mEntries.size())), 0,
                                static_cast<std::uint32_t>(mEntries.size()), true));
    }
    else
    {
        packEntries();
        for (std::uint32_t first = 0; first < mEntries.size(); first += kFanout)
        {
            const auto count = static_cast<std::uint32_t>(
                std::min<std::size_t>(kFanout, mEntries.size() - first));
            level.push_back(addNode(boxOf(mEntries, first, count), first, count, true));
        }
    }

    while (level.size() > 1)
        level = packLevel(std::move(level));

    mRoot = level.front();
    mEntries.shrink_to_fit();
    mNodes.shrink_to_fit();
    mLinks.shrink_to_fit();
}
