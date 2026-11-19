#include "test-fill-geometry.h"
#include "plottables/graph-fill.h"
#include "plottables/plottable-linestyle.h"
#include "qcustomplot.h"
#include <algorithm>
#include <cmath>
#include <limits>

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
QList<QPointF> vertices(const std::vector<float>& v)
{
    QList<QPointF> result;
    for (std::size_t i = 0; i + 1 < v.size(); i += 6)
        result << QPointF(v[i], v[i + 1]);
    return result;
}
using Triangle = std::array<QPointF, 3>;
bool before(const QPointF& p, const QPointF& q) { return p.x() != q.x() ? p.x() < q.x() : p.y() < q.y(); }
QList<Triangle> triangleSet(const std::vector<float>& v)
{
    const auto pts = vertices(v);
    QList<Triangle> result;
    for (qsizetype i = 0; i + 2 < pts.size(); i += 3)
    {
        Triangle t { pts[i], pts[i + 1], pts[i + 2] };
        std::sort(t.begin(), t.end(), before);
        result << t;
    }
    std::sort(result.begin(), result.end(), [](const Triangle& a, const Triangle& b)
              { return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), before); });
    return result;
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
    const std::vector<StripPoint> rows { {100, 0, 10}, {110, 0, 10} };
    std::vector<float> out;
    qcp::fill::appendStrip(rows, true, kRed, out);
    QVERIFY(!out.empty());
    for (std::size_t v = 0; v < out.size(); v += 6)
    {
        QVERIFY(out[v] == 0.f || out[v] == 10.f); // x is the value position
        QVERIFY(out[v + 1] == 100.f || out[v + 1] == 110.f); // y is the key position
    }
}

void TestFillGeometry::crossingEdgesSplitAtTheCrossing()
{
    const std::vector<StripPoint> rows { {100, 2, 12}, {110, 12, 2} };
    std::vector<float> out;
    qcp::fill::appendStrip(rows, false, kRed, out);
    const QList<Triangle> expected { { QPointF(100, 2), QPointF(100, 12), QPointF(105, 7) },
                                     { QPointF(105, 7), QPointF(110, 2), QPointF(110, 12) } };
    QCOMPARE(triangleSet(out), expected);
}

void TestFillGeometry::thinStripsAreOneQuad()
{
    const std::vector<StripPoint> rows { {0, 0, 0.5}, {10, 0, 0.5} };
    std::vector<float> out;
    qcp::fill::appendStrip(rows, false, kRed, out);
    const QList<QPointF> expected { {0, 0}, {10, 0}, {10, 0.5}, {0, 0}, {10, 0.5}, {0, 0.5} };
    QCOMPARE(vertices(out), expected);
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

void TestFillGeometry::binToSpansJoinsRowsOfALaterBin()
{
    const std::vector<StripPoint> rows { {5.1, 1, 2}, {5.4, 0, 3} };
    const auto spans = qcp::fill::binToSpans(rows, 1.0);
    QCOMPARE(spans.size(), std::size_t(1));
    QCOMPARE(spans[0].a, 0.0);
    QCOMPARE(spans[0].b, 3.0);
}

void TestFillGeometry::binToSpansPutsEachSpanAtItsBinCentre()
{
    const std::vector<StripPoint> rows { {0.1, 0, 1}, {1.2, 0, 1}, {4.5, 0, 1} };
    const auto spans = qcp::fill::binToSpans(rows, 2.0);
    QCOMPARE(spans.size(), std::size_t(2));
    QCOMPARE(spans[0].key, 1.0);
    QCOMPARE(spans[1].key, 5.0);
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

Q_DECLARE_METATYPE(qcp::fill::Step)
Q_DECLARE_METATYPE(std::vector<StripPoint>)

namespace {
bool sameValue(double x, double y) { return (std::isnan(x) && std::isnan(y)) || x == y; }
bool samePoint(const StripPoint& p, const StripPoint& q)
{
    return p.key == q.key && sameValue(p.a, q.a) && sameValue(p.b, q.b) && p.breakBefore == q.breakBefore;
}
QByteArray describe(const StripPoint& p)
{
    return QByteArray("{") + QByteArray::number(p.key) + ", " + QByteArray::number(p.a) + ", "
        + QByteArray::number(p.b) + (p.breakBefore ? ", break}" : "}");
}
// A row with a NaN edge, then a row that starts a new run.
const std::vector<StripPoint> kSteppedRows { {0, 1, 5}, {10, 3, 6}, {20, kNan, 7}, {30, 2, 8, true}, {40, 4, 9} };
}

void TestFillGeometry::steppedMovesBothEdgesAndKeepsBreaks_data()
{
    using qcp::fill::Step;
    QTest::addColumn<Step>("step");
    QTest::addColumn<std::vector<StripPoint>>("expected");
    QTest::newRow("left") << Step::Left << std::vector<StripPoint> {
        {0, 1, 5}, {10, 1, 5}, {10, 3, 6}, {20, 3, 6}, {20, kNan, 7},
        {30, 2, 8, true}, {40, 2, 8}, {40, 4, 9} };
    QTest::newRow("right") << Step::Right << std::vector<StripPoint> {
        {0, 1, 5}, {0, 3, 6}, {10, 3, 6}, {10, kNan, 7}, {20, kNan, 7},
        {30, 2, 8, true}, {30, 4, 9}, {40, 4, 9} };
    QTest::newRow("center") << Step::Center << std::vector<StripPoint> {
        {0, 1, 5}, {5, 1, 5}, {5, 3, 6}, {15, 3, 6}, {15, kNan, 7}, {20, kNan, 7},
        {30, 2, 8, true}, {35, 2, 8}, {35, 4, 9}, {40, 4, 9} };
}

void TestFillGeometry::steppedMovesBothEdgesAndKeepsBreaks()
{
    QFETCH(qcp::fill::Step, step);
    QFETCH(std::vector<StripPoint>, expected);
    const auto s = qcp::fill::stepped(kSteppedRows, step);
    QCOMPARE(s.size(), expected.size());
    for (std::size_t i = 0; i < s.size(); ++i)
        QVERIFY2(samePoint(s[i], expected[i]),
                 (QByteArray::number(qulonglong(i)) + ": " + describe(s[i]) + " != "
                  + describe(expected[i])).constData());
}

namespace {
struct LogAxisPlot
{
    QCustomPlot plot;
    LogAxisPlot(double lower, double upper)
    {
        plot.resize(400, 300);
        plot.yAxis->setScale(QCPScale::log());
        plot.yAxis->setRange(lower, upper);
        plot.replot();
    }
    QCPAxis& axis() { return *plot.yAxis; }
};
double pastTheEdge(const QCPAxis& axis, double lower, double upper)
{
    const double edge = axis.coordToPixel(lower), other = axis.coordToPixel(upper);
    return edge + 2.0 * (edge - other);
}
}

void TestFillGeometry::baselinePixelPastTheEdgeOnALogAxis()
{
    LogAxisPlot p(0.1, 100);
    QCOMPARE(qcp::fill::baselinePixel(p.axis(), 0.0), pastTheEdge(p.axis(), 0.1, 100));
    QCOMPARE(qcp::fill::baselinePixel(p.axis(), 10.0), p.axis().coordToPixel(10.0));
}

void TestFillGeometry::baselinePixelOnALogAxisBelowOne()
{
    LogAxisPlot p(0.001, 0.5);
    QCOMPARE(qcp::fill::baselinePixel(p.axis(), 0.0), pastTheEdge(p.axis(), 0.001, 0.5));
}

namespace
{
using qcp::fill::SeriesRow;

std::vector<SeriesRow> series(std::initializer_list<double> keys,
                              std::initializer_list<double> values)
{
    std::vector<SeriesRow> rows;
    int row = 0;
    for (auto k = keys.begin(), v = values.begin(); k != keys.end() && v != values.end(); ++k, ++v)
        rows.push_back({ *k, *v, false, row++ });
    return rows;
}

QByteArray describeRun(const qcp::fill::SideRun& run)
{
    QByteArray out = run.firstAbove ? "above [" : "below [";
    for (const StripPoint& row : run.rows)
        out += describe(row) + " ";
    return out + "]";
}

// The keys of a run, so a split is read at a glance.
QList<double> keysOf(const qcp::fill::SideRun& run)
{
    QList<double> keys;
    for (const StripPoint& row : run.rows)
        keys << row.key;
    return keys;
}
}

void TestFillGeometry::unionRowsInterpolatesBothLines()
{
    const auto first = series({ 0, 2 }, { 0, 2 });
    const std::vector<StripPoint> expected { { 0, 0, 5 }, { 1, 1, 5 }, { 2, 2, 5 } };
    // A line of one point is a constant: it does not limit the range, it repeats its value.
    for (const auto& second : { series({ 1 }, { 5 }), series({ 0, 1, 2 }, { 5, 5, 5 }) })
    {
        const auto rows = qcp::fill::unionRows(first, second);
        QCOMPARE(rows.size(), expected.size());
        for (std::size_t i = 0; i < rows.size(); ++i)
            QVERIFY2(samePoint(rows[i], expected[i]), describe(rows[i]).constData());
    }
}

void TestFillGeometry::unionRowsCropToTheSharedRange()
{
    const auto rows = qcp::fill::unionRows(series({ 0, 1, 2, 3 }, { 0, 10, 20, 30 }),
                                           series({ 1, 2 }, { 1, 2 }));
    QCOMPARE(rows.size(), std::size_t(2));
    QCOMPARE(rows[0].key, 1.0);
    QCOMPARE(rows[0].a, 10.0);
    QCOMPARE(rows[0].b, 1.0);
    QCOMPARE(rows[1].key, 2.0);
    QCOMPARE(rows[1].a, 20.0);
    QCOMPARE(rows[1].b, 2.0);
    // The crop ends are rows too, interpolated on the line that has no key there.
    const auto cropped = qcp::fill::unionRows(series({ 0, 1, 2, 3 }, { 0, 10, 20, 30 }),
                                              series({ 1.5, 2.5 }, { -5, 5 }));
    QCOMPARE(cropped.size(), std::size_t(3));
    QCOMPARE(cropped[0].key, 1.5);
    QCOMPARE(cropped[0].a, 15.0);
    QCOMPARE(cropped[0].b, -5.0);
    QCOMPARE(cropped[1].key, 2.0);
    QCOMPARE(cropped[2].key, 2.5);
    QCOMPARE(cropped[2].a, 25.0);
    QCOMPARE(cropped[2].b, 5.0);
}

void TestFillGeometry::unionRowsBreakAtANaN()
{
    const auto rows = qcp::fill::unionRows(series({ 0, 1, 2, 3 }, { 0, kNan, 1, 1 }),
                                           series({ 0, 1, 2, 3 }, { 0, 0, 0, 0 }));
    QCOMPARE(rows.size(), std::size_t(4));
    QVERIFY(!rows[0].breakBefore);
    QVERIFY(rows[1].breakBefore);
    QVERIFY(std::isnan(rows[1].a));
    QVERIFY(rows[2].breakBefore);
    QVERIFY(!rows[3].breakBefore);
}

void TestFillGeometry::splitAtCrossingsCutsAtTheExactPoint()
{
    const std::vector<StripPoint> rows {
        { 0, 0.8, 0 }, { 1, 0.8, 0 }, { 2, 0.2, 1 }, { 3, 0.2, 1 }
    };
    const auto runs = qcp::fill::splitAtCrossings(rows);
    QCOMPARE(runs.size(), std::size_t(2));
    QVERIFY2(runs[0].firstAbove, describeRun(runs[0]).constData());
    QCOMPARE(keysOf(runs[0]), (QList<double> { 0, 1, 1.5 }));
    QCOMPARE(runs[0].rows[2].sourceRow, -1);
    QCOMPARE(runs[0].rows[2].a, runs[0].rows[2].b);
    QVERIFY(std::abs(runs[0].rows[2].a - 0.5) < 1e-12);
    QVERIFY2(!runs[1].firstAbove, describeRun(runs[1]).constData());
    QCOMPARE(keysOf(runs[1]), (QList<double> { 1.5, 2, 3 }));
    QCOMPARE(runs[1].rows[0].key, runs[0].rows[2].key);
}

void TestFillGeometry::aCrossingOnAKeyMakesNoSliver()
{
    const std::vector<StripPoint> rows { { 0, 1, 0 }, { 1, 0, 0 }, { 2, -1, 0 } };
    const auto runs = qcp::fill::splitAtCrossings(rows);
    QCOMPARE(runs.size(), std::size_t(2));
    QVERIFY2(runs[0].firstAbove, describeRun(runs[0]).constData());
    QCOMPARE(keysOf(runs[0]), (QList<double> { 0, 1 }));
    QVERIFY2(!runs[1].firstAbove, describeRun(runs[1]).constData());
    QCOMPARE(keysOf(runs[1]), (QList<double> { 1, 2 }));
}

void TestFillGeometry::touchingLinesStayOneRun()
{
    const std::vector<StripPoint> rows { { 0, 1, 0 }, { 1, 0, 0 }, { 2, 1, 0 } };
    const auto runs = qcp::fill::splitAtCrossings(rows);
    QCOMPARE(runs.size(), std::size_t(1));
    QVERIFY(runs[0].firstAbove);
    QCOMPARE(keysOf(runs[0]), (QList<double> { 0, 1, 2 }));
}

// A row where the lines are equal belongs to the run it lands in, whichever side that run is on.
void TestFillGeometry::touchingFromBelowStaysOneRun()
{
    const std::vector<StripPoint> rows { { 0, -1, 0 }, { 1, 0, 0 }, { 2, -1, 0 } };
    const auto runs = qcp::fill::splitAtCrossings(rows);
    QCOMPARE(runs.size(), std::size_t(1));
    QVERIFY2(!runs[0].firstAbove, "a touch from below made the run above");
    QCOMPARE(keysOf(runs[0]), (QList<double> { 0, 1, 2 }));
}

void TestFillGeometry::aFlatTouchStaysOneRun()
{
    const std::vector<StripPoint> rows { { 0, -1, 0 }, { 1, 0, 0 }, { 2, 0, 0 }, { 3, -1, 0 } };
    const auto runs = qcp::fill::splitAtCrossings(rows);
    QCOMPARE(runs.size(), std::size_t(1));
    QVERIFY2(!runs[0].firstAbove, "a flat touch made the run above");
    QCOMPARE(keysOf(runs[0]), (QList<double> { 0, 1, 2, 3 }));
}

void TestFillGeometry::aBreakEndsTheRunWithoutACrossing()
{
    const std::vector<StripPoint> rows { { 0, 1, 0 }, { 1, 1, 0 }, { 2, 1, 0, true }, { 3, 1, 0 } };
    const auto runs = qcp::fill::splitAtCrossings(rows);
    QCOMPARE(runs.size(), std::size_t(2));
    QCOMPARE(keysOf(runs[0]), (QList<double> { 0, 1 }));
    QCOMPARE(keysOf(runs[1]), (QList<double> { 2, 3 }));
    QVERIFY(runs[1].firstAbove);
}

// A run that opens on a tie takes its side from its first non-tie row, not from the tie.
void TestFillGeometry::aRunStartingOnATieTakesTheNextSide()
{
    const std::vector<StripPoint> rows { { 0, 0, 0 }, { 1, 1, 0 } };
    const auto runs = qcp::fill::splitAtCrossings(rows);
    QCOMPARE(runs.size(), std::size_t(1));
    QVERIFY2(runs[0].firstAbove, "a run that starts on a tie reported below");
    QCOMPARE(keysOf(runs[0]), (QList<double> { 0, 1 }));
}

// A run of nothing but ties has no side to take: it is above, as the header documents.
void TestFillGeometry::anAllTieRunIsAbove()
{
    const std::vector<StripPoint> rows { { 0, 1, 1 }, { 1, 2, 2 } };
    const auto runs = qcp::fill::splitAtCrossings(rows);
    QCOMPARE(runs.size(), std::size_t(1));
    QVERIFY(runs[0].firstAbove);
    QCOMPARE(keysOf(runs[0]), (QList<double> { 0, 1 }));
}

// The same, for a run the break starts: the row after the break is a tie, and the one after it says
// which side the run is on.
void TestFillGeometry::aRunRestartingOnATieAfterABreakIsAbove()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const std::vector<StripPoint> rows { { 0, 1, 0 }, { 1, nan, nan }, { 2, 0, 0 }, { 3, 1, 0 } };
    const auto runs = qcp::fill::splitAtCrossings(rows);
    QCOMPARE(runs.size(), std::size_t(2));
    QVERIFY(runs[0].firstAbove);
    QCOMPARE(keysOf(runs[0]), (QList<double> { 0 }));
    QVERIFY(runs[1].firstAbove);
    QCOMPARE(keysOf(runs[1]), (QList<double> { 2, 3 }));
}

// A range query runs before the first layout, when the axis rect has no pixel to bin into: the rows
// must come back as they are, or the fill records bins its own layout will never match.
void TestFillGeometry::binnedWhenDenseIsAPixelDecision()
{
    QCustomPlot plot;
    plot.yAxis->setRange(0.0, 1000.0);
    std::vector<qcp::fill::StripPoint> rows;
    for (int i = 0; i < 100; ++i)
        rows.push_back({ double(i) * 10.0, 0.0, 1.0, false, i });

    const std::vector<qcp::fill::StripPoint> kept
        = qcp::fill::binnedWhenDense(*plot.yAxis, std::move(rows));
    QCOMPARE(kept.size(), std::size_t(100));
}
