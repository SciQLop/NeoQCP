#include "test-errorbar-geometry.h"
#include "plottables/multigraph-errorbars.h"
#include <cmath>
#include <limits>
#include <vector>
using qcp::errorbars::Bar;
namespace { constexpr double kNan = std::numeric_limits<double>::quiet_NaN(); }

void TestErrorBarGeometry::nanBoundGivesAOneSidedBar()
{
    Bar b { 10, 50, kNan, 40 };
    QVERIFY(qcp::errorbars::resolve(b));
    QCOMPARE(b.lower, 50.0);
    Bar above { 10, 50, 60, kNan };
    QVERIFY(qcp::errorbars::resolve(above));
    QCOMPARE(above.upper, 50.0);
}

void TestErrorBarGeometry::twoNanPointsGiveNoBar()
{
    Bar b { 10, kNan, kNan, 40 };
    QVERIFY(!qcp::errorbars::resolve(b));
}

void TestErrorBarGeometry::missingValueStillDrawsBetweenBounds()
{
    Bar b { 10, kNan, 60, 40 };
    QVERIFY(qcp::errorbars::resolve(b));
    QCOMPARE(b.lower, 60.0);
    QCOMPARE(b.upper, 40.0);
}

void TestErrorBarGeometry::barsFitNeedsTheSpacing()
{
    const std::vector<Bar> wide { {0, 1, 2, 0}, {20, 1, 2, 0}, {40, 1, 2, 0} };
    const std::vector<Bar> bursty { {0, 1, 2, 0}, {2, 1, 2, 0}, {40, 1, 2, 0} };
    QVERIFY(qcp::errorbars::barsFit(wide, 18));
    QVERIFY(!qcp::errorbars::barsFit(bursty, 18));
    const std::vector<Bar> exactlyApart { {0, 1, 2, 0}, {18, 1, 2, 0} };
    QVERIFY(qcp::errorbars::barsFit(exactlyApart, 18));
}
