#include "test-errorbar-geometry.h"
#include "plottables/multigraph-errorbars.h"
#include <cmath>
#include <limits>
#include <vector>
using qcp::errorbars::Bar;
namespace { constexpr double kNan = std::numeric_limits<double>::quiet_NaN(); }

void TestErrorBarGeometry::oneBarIsAStemAndTwoCaps()
{
    const std::vector<Bar> bars { {10, 50, 60, 40} };
    const auto runs = qcp::errorbars::barRuns(bars, 3, false);
    QCOMPARE(runs.size(), 3 * 2 + 2);
    QCOMPARE(runs[0], QPointF(10, 60));
    QCOMPARE(runs[1], QPointF(10, 40));
    QVERIFY(std::isnan(runs[2].x()));
    QCOMPARE(runs[3], QPointF(7, 60));
    QCOMPARE(runs[4], QPointF(13, 60));
    QVERIFY(std::isnan(runs[5].x()));
    QCOMPARE(runs[6], QPointF(7, 40));
    QCOMPARE(runs[7], QPointF(13, 40));
}

void TestErrorBarGeometry::nanBoundGivesAOneSidedBar()
{
    Bar b { 10, 50, kNan, 40 };
    QVERIFY(qcp::errorbars::resolve(b));
    QCOMPARE(b.lower, 50.0);
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

void TestErrorBarGeometry::verticalKeyGivesHorizontalBars()
{
    const std::vector<Bar> bars { {10, 50, 60, 40} };
    const auto runs = qcp::errorbars::barRuns(bars, 3, true);
    QCOMPARE(runs[0], QPointF(60, 10));
    QCOMPARE(runs[1], QPointF(40, 10));
}

void TestErrorBarGeometry::barsFitNeedsTheSpacing()
{
    const std::vector<Bar> wide { {0, 1, 2, 0}, {20, 1, 2, 0}, {40, 1, 2, 0} };
    const std::vector<Bar> bursty { {0, 1, 2, 0}, {2, 1, 2, 0}, {40, 1, 2, 0} };
    QVERIFY(qcp::errorbars::barsFit(wide, 18));
    QVERIFY(!qcp::errorbars::barsFit(bursty, 18));
}

void TestErrorBarGeometry::noInfinitiesEverReachTheRuns()
{
    const std::vector<Bar> bars { {10, 50, std::numeric_limits<double>::infinity(), 40} };
    for (const QPointF& p : qcp::errorbars::barRuns(bars, 3, false))
        QVERIFY(std::isnan(p.x()) || (std::isfinite(p.x()) && std::isfinite(p.y())));
}

void TestErrorBarGeometry::barsAreSeparatedByOneNanNotTrailing()
{
    const std::vector<Bar> bars { {10, 50, 60, 40}, {20, kNan, kNan, 40}, {30, 50, 60, 40} };
    const auto runs = qcp::errorbars::barRuns(bars, 3, false);
    QCOMPARE(runs.size(), 8 + 1 + 8);
    QVERIFY(std::isnan(runs[8].x()));
    QVERIFY(std::isfinite(runs.last().x()));
}
