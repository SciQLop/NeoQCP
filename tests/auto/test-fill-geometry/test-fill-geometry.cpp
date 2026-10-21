#include "test-fill-geometry.h"
#include "plottables/multigraph-fill.h"
#include "plottables/plottable-linestyle.h"
#include <cmath>

using qcp::fill::StripPoint;
namespace {
constexpr std::array<float, 4> kRed { 1.f, 0.f, 0.f, 1.f };
constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
int triangles(const std::vector<float>& v) { return static_cast<int>(v.size() / 18); }
double area(const std::vector<float>& v)
{
    double total = 0;
    for (std::size_t t = 0; t + 17 < v.size(); t += 18)
    {
        const double x0 = v[t], y0 = v[t + 1], x1 = v[t + 6], y1 = v[t + 7], x2 = v[t + 12], y2 = v[t + 13];
        total += std::abs((x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)) / 2;
    }
    return total;
}
}

void TestFillGeometry::twoRowsMakeOneQuad()
{
    const std::vector<StripPoint> rows { {0, 0, 10}, {10, 0, 10} };
    std::vector<float> out;
    qcp::fill::appendStrip(rows, false, kRed, out);
    QCOMPARE(triangles(out), 2);
    QCOMPARE(area(out), 100.0);
}

void TestFillGeometry::nanEdgeBreaksTheStrip()
{
    const std::vector<StripPoint> rows { {0, 0, 10}, {10, kNan, 10}, {20, 0, 10}, {30, 0, 10} };
    std::vector<float> out;
    qcp::fill::appendStrip(rows, false, kRed, out);
    QCOMPARE(area(out), 100.0); // only rows 2-3 join
}

void TestFillGeometry::breakBeforeBreaksTheStrip()
{
    const std::vector<StripPoint> rows { {0, 0, 10}, {10, 0, 10}, {20, 0, 10, true}, {30, 0, 10} };
    std::vector<float> out;
    qcp::fill::appendStrip(rows, false, kRed, out);
    QCOMPARE(area(out), 200.0);
}

void TestFillGeometry::crossingEdgesAreSplitNotDoubleBlended()
{
    // a goes 0 -> 10, b goes 10 -> 0: they cross at key 5; two triangles of area 25 each.
    const std::vector<StripPoint> rows { {0, 0, 10}, {10, 10, 0} };
    std::vector<float> out;
    qcp::fill::appendStrip(rows, false, kRed, out);
    QCOMPARE(area(out), 50.0);
}

void TestFillGeometry::verticalKeySwapsCoordinates()
{
    const std::vector<StripPoint> rows { {0, 0, 10}, {10, 0, 10} };
    std::vector<float> out;
    qcp::fill::appendStrip(rows, true, kRed, out);
    for (std::size_t v = 0; v < out.size(); v += 6)
        QVERIFY(out[v] == 0.f || out[v] == 10.f); // x is the value position
}

void TestFillGeometry::binToSpansTakesMinAndMaxOfBothEdges()
{
    const std::vector<StripPoint> rows { {0.1, 5, 6}, {0.4, 1, 9}, {0.9, 3, 4}, {1.2, 2, 2} };
    const auto spans = qcp::fill::binToSpans(rows, 1.0);
    QCOMPARE(spans.size(), std::size_t(2));
    QCOMPARE(spans[0].a, 1.0);
    QCOMPARE(spans[0].b, 9.0);
    QCOMPARE(spans[1].a, 2.0);
    QCOMPARE(spans[1].b, 2.0);
}

void TestFillGeometry::binToSpansKeepsBreaks()
{
    const std::vector<StripPoint> rows { {0.1, 0, 1}, {0.9, 0, 1}, {5.1, 0, 1, true} };
    const auto spans = qcp::fill::binToSpans(rows, 1.0);
    QCOMPARE(spans.size(), std::size_t(2));
    QVERIFY(spans[1].breakBefore);
}

void TestFillGeometry::stripPolygonsOnePerRun()
{
    const std::vector<StripPoint> rows { {0, 0, 10}, {10, 0, 10}, {20, 0, 10, true}, {30, 0, 10} };
    const auto polys = qcp::fill::stripPolygons(rows, false);
    QCOMPARE(polys.size(), 2);
    QCOMPARE(polys[0].size(), 4); // a forward (2), b back (2)
}

void TestFillGeometry::stepLeftHoldsEachValueUntilTheNextKey()
{
    const std::vector<StripPoint> rows { {0, 0, 1}, {10, 2, 3} };
    const auto s = qcp::fill::stepped(rows, qcp::fill::Step::Left);
    QCOMPARE(s.size(), std::size_t(3));
    QCOMPARE(s[1].key, 10.0);
    QCOMPARE(s[1].a, 0.0); // still the first row's value at the second key
    QCOMPARE(s[2].a, 2.0);
}

void TestFillGeometry::stepRightAndCenterMatchTheLineHelpers()
{
    const std::vector<StripPoint> rows { {0, 1, 5}, {10, 3, 6}, {14, 2, 7}, {30, 4, 9} };
    QVector<QPointF> lines;
    for (const auto& r : rows)
        lines << QPointF(r.key, r.a);
    const auto compare = [&](qcp::fill::Step step, const QVector<QPointF>& expected, qsizetype skip)
    {
        const auto s = qcp::fill::stepped(rows, step);
        QCOMPARE(static_cast<qsizetype>(s.size()), expected.size() - skip);
        for (qsizetype i = 0; i < qsizetype(s.size()); ++i)
        {
            QCOMPARE(s[i].key, expected[i + skip].x());
            QCOMPARE(s[i].a, expected[i + skip].y());
        }
    };
    // The helpers open Left/Right with a duplicated first corner; the fill drops it.
    compare(qcp::fill::Step::Left, qcp::toStepLeftLines(lines, false), 1);
    compare(qcp::fill::Step::Right, qcp::toStepRightLines(lines, false), 1);
    compare(qcp::fill::Step::Center, qcp::toStepCenterLines(lines, false), 0);
}
