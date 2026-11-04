#include "test-spatial-index.h"

#include "spatial-index.h"

#include <algorithm>
#include <limits>
#include <random>
#include <vector>

namespace
{
constexpr double kInf = std::numeric_limits<double>::infinity();

bool overlaps(const QCPDataBox& a, const QCPDataBox& b)
{
    return a.keyLower <= b.keyUpper && b.keyLower <= a.keyUpper && a.valueLower <= b.valueUpper
        && b.valueLower <= a.valueUpper;
}

std::vector<int> sortedHits(const QCPSpatialIndex& index, const QCPDataBox& query)
{
    std::vector<int> hits;
    index.query(query, [&hits](int payload) { hits.push_back(payload); });
    std::sort(hits.begin(), hits.end());
    return hits;
}
} // namespace

void TestSpatialIndex::emptyIndex()
{
    QCPSpatialIndex index;
    QCOMPARE(index.size(), 0);
    bool hit = false;
    index.query({0, 1, 0, 1}, [&hit](int) { hit = true; });
    QVERIFY(!hit);
}

void TestSpatialIndex::singleBox()
{
    QCPSpatialIndex index;
    index.build({{{10, 20, 30, 40}, 7}});
    QCOMPARE(index.size(), 1);
    QCOMPARE(sortedHits(index, {15, 16, 35, 36}), std::vector<int>{7});
    QCOMPARE(sortedHits(index, {0, 9, 30, 40}), std::vector<int>{});
    QCOMPARE(sortedHits(index, {20, 25, 40, 45}), std::vector<int>{7});
}

void TestSpatialIndex::edgeTouchIsAHit()
{
    QCPSpatialIndex index;
    index.build({{{0, 1, 0, 1}, 1}, {{2, 3, 2, 3}, 2}});
    QCOMPARE(sortedHits(index, {1, 2, 1, 2}), (std::vector<int>{1, 2}));
    QCOMPARE(sortedHits(index, {1, 2, 0.5, 1}), (std::vector<int>{1}));
    QCOMPARE(sortedHits(index, {3, 4, 3, 4}), (std::vector<int>{2}));
    QCOMPARE(sortedHits(index, {1.0001, 1.9999, 0, 1}), (std::vector<int>{}));
}

void TestSpatialIndex::infiniteSides()
{
    QCPSpatialIndex index;
    index.build({{{10, 20, 30, 40}, 1}, {{-kInf, kInf, 0, 1}, 2}, {{-kInf, kInf, -kInf, kInf}, 3}});
    QCOMPARE(index.size(), 3);
    QCOMPARE(sortedHits(index, {15, 16, 35, 36}), (std::vector<int>{1, 3}));
    QCOMPARE(sortedHits(index, {0, 1, 0, 1}), (std::vector<int>{2, 3}));
    QCOMPARE(sortedHits(index, {1e9, 1e9, 1e9, 1e9}), (std::vector<int>{3}));
    QCOMPARE(sortedHits(index, {-1e9, 1e9, 35, 36}), (std::vector<int>{1, 3}));
}

void TestSpatialIndex::nanEntriesDropped()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    QCPSpatialIndex index;
    index.build({{{0, 1, 0, 1}, 1}, {{nan, 1, 0, 1}, 2}, {{0, 1, 0, nan}, 3}, {{5, 6, 5, 6}, 4}});
    QCOMPARE(index.size(), 2);
    QCOMPARE(sortedHits(index, {-10, 10, -10, 10}), (std::vector<int>{1, 4}));
}

void TestSpatialIndex::randomMatchesBruteForce()
{
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> position(-1000.0, 1000.0);
    std::uniform_real_distribution<double> extent(0.0, 200.0);
    std::uniform_real_distribution<double> chance(0.0, 1.0);
    std::uniform_int_distribution<int> pick;

    for (const int count : {1, 15, 16, 17, 257, 20000})
    {
        std::vector<std::pair<QCPDataBox, int>> entries;
        entries.reserve(count);
        for (int i = 0; i < count; ++i)
        {
            const double key = position(rng);
            const double value = position(rng);
            entries.push_back({{key, key + extent(rng), value, value + extent(rng)}, i});
        }
        // A few items are unbounded on one axis, and one on both, like a span
        // that covers everything or a position on an axis rect ratio.
        const int unbounded = std::max(1, count / 50);
        for (int i = 0; i < unbounded; ++i)
        {
            QCPDataBox& box = entries[static_cast<std::size_t>(pick(rng) % count)].first;
            switch (i % 3)
            {
            case 0:
                box.keyLower = -kInf;
                box.keyUpper = kInf;
                break;
            case 1:
                box.valueUpper = kInf;
                break;
            default:
                box.keyLower = -kInf;
                box.valueLower = -kInf;
                break;
            }
        }
        entries.back().first = {-kInf, kInf, -kInf, kInf};

        QCPSpatialIndex index;
        index.build(entries);
        QCOMPARE(index.size(), count);

        for (int q = 0; q < 1000; ++q)
        {
            const double key = position(rng);
            const double value = position(rng);
            const QCPDataBox query = chance(rng) < 0.2
                ? QCPDataBox{key, key, value, value}
                : QCPDataBox{key, key + extent(rng), value, value + extent(rng)};
            std::vector<int> expected;
            for (const auto& [box, payload] : entries)
                if (overlaps(box, query))
                    expected.push_back(payload);
            std::sort(expected.begin(), expected.end());
            const std::vector<int> hits = sortedHits(index, query);
            QVERIFY2(hits == expected, qPrintable(QStringLiteral("count %1, query %2")
                                                      .arg(count)
                                                      .arg(q)));
        }
    }
}

void TestSpatialIndex::rebuildReplaces()
{
    QCPSpatialIndex index;
    index.build({{{0, 1, 0, 1}, 1}});
    QCOMPARE(sortedHits(index, {-10, 10, -10, 10}), std::vector<int>{1});

    index.build({{{0, 1, 0, 1}, 5}, {{0, 1, 0, 1}, 6}});
    QCOMPARE(index.size(), 2);
    QCOMPARE(sortedHits(index, {-10, 10, -10, 10}), (std::vector<int>{5, 6}));

    index.build({});
    QCOMPARE(index.size(), 0);
    QCOMPARE(sortedHits(index, {-10, 10, -10, 10}), std::vector<int>{});
}
