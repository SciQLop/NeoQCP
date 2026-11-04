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

namespace
{
// STR (Leutenegger, Edgington, Lopez, ICDE 1997): sort by key centre, cut into slices of whole
// nodes, sort each slice by value centre. Sorts (centre, index) pairs: the centres are computed once,
// not in every comparison, and no stability is needed.
template <typename KeyOf, typename ValueOf>
std::vector<std::uint32_t> strOrder(std::size_t n, std::size_t fanout, KeyOf keyOf, ValueOf valueOf)
{
    const std::size_t groups = (n + fanout - 1) / fanout;
    const auto slices = static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<double>(groups))));
    const std::size_t sliceSize = slices * fanout;

    std::vector<std::pair<double, std::uint32_t>> keyed(n);
    for (std::size_t i = 0; i < n; ++i)
        keyed[i] = {keyOf(i), static_cast<std::uint32_t>(i)};
    std::sort(keyed.begin(), keyed.end());
    for (std::size_t start = 0; start < n; start += sliceSize)
    {
        const std::size_t stop = std::min(n, start + sliceSize);
        for (std::size_t i = start; i < stop; ++i)
            keyed[i].first = valueOf(keyed[i].second);
        std::sort(keyed.begin() + static_cast<std::ptrdiff_t>(start),
                  keyed.begin() + static_cast<std::ptrdiff_t>(stop));
    }
    std::vector<std::uint32_t> order(n);
    for (std::size_t i = 0; i < n; ++i)
        order[i] = keyed[i].second;
    return order;
}
} // namespace

void QCPSpatialIndex::packEntries()
{
    const std::vector<std::uint32_t> order = strOrder(
        mEntries.size(), kFanout, [this](std::size_t i) { return keyCentre(mEntries[i].box); },
        [this](std::size_t i) { return valueCentre(mEntries[i].box); });
    std::vector<Entry> sorted;
    sorted.reserve(mEntries.size());
    for (const std::uint32_t index : order)
        sorted.push_back(mEntries[index]);
    mEntries.swap(sorted);
}

std::vector<std::uint32_t> QCPSpatialIndex::packLevel(std::vector<std::uint32_t> level)
{
    const std::size_t n = level.size();
    const std::size_t parentCount = (n + kFanout - 1) / kFanout;
    const std::vector<std::uint32_t> order = strOrder(
        n, kFanout, [&](std::size_t i) { return keyCentre(mNodes[level[i]].box); },
        [&](std::size_t i) { return valueCentre(mNodes[level[i]].box); });
    std::vector<std::uint32_t> sortedLevel(n);
    for (std::size_t i = 0; i < n; ++i)
        sortedLevel[i] = level[order[i]];
    level.swap(sortedLevel);

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
}
