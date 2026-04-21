#include "test-line-extruder.h"
#include "painting/line-extruder.h"
#include <QPolygonF>

void TestLineExtruder::horizontalSegment()
{
    QVector<QPointF> points = {{0.0, 5.0}, {10.0, 5.0}};
    QColor color(255, 0, 0, 255);
    float penWidth = 2.0f;

    auto verts = QCPLineExtruder::extrudePolyline(points, penWidth, color);

    // 2 points → 1 segment → 1 quad → 6 vertices (triangle list)
    // each vertex: x, y, r, g, b, a = 6 floats
    QCOMPARE(verts.size(), 6 * 6);

    // Verify the quad spans y = 4.0 to y = 6.0 (penWidth/2 = 1.0 offset)
    // and x = 0.0 to x = 10.0
    for (int i = 0; i < 6; ++i) {
        float y = verts[i * 6 + 1];
        QVERIFY(qFuzzyCompare(y, 4.0f) || qFuzzyCompare(y, 6.0f));
        float x = verts[i * 6 + 0];
        QVERIFY(x >= -0.01f && x <= 10.01f);
        // Check color (premultiplied: alpha=1.0 so RGB unchanged)
        QCOMPARE(verts[i * 6 + 2], 1.0f); // r
        QCOMPARE(verts[i * 6 + 3], 0.0f); // g
        QCOMPARE(verts[i * 6 + 4], 0.0f); // b
        QCOMPARE(verts[i * 6 + 5], 1.0f); // a
    }
}

void TestLineExtruder::verticalSegment()
{
    QVector<QPointF> points = {{5.0, 0.0}, {5.0, 10.0}};
    QColor color(0, 255, 0, 255);
    float penWidth = 4.0f;

    auto verts = QCPLineExtruder::extrudePolyline(points, penWidth, color);

    QCOMPARE(verts.size(), 6 * 6);

    for (int i = 0; i < 6; ++i) {
        float x = verts[i * 6 + 0];
        QVERIFY(qFuzzyCompare(x, 3.0f) || qFuzzyCompare(x, 7.0f));
    }
}

void TestLineExtruder::miterJoin()
{
    QVector<QPointF> points = {{0.0, 0.0}, {10.0, 0.0}, {10.0, 10.0}};
    QColor color(0, 0, 255, 255);
    float penWidth = 2.0f;

    auto verts = QCPLineExtruder::extrudePolyline(points, penWidth, color);

    // 3 points → 2 segments → 2 quads → 12 vertices
    QCOMPARE(verts.size(), 12 * 6);
}

void TestLineExtruder::bevelFallback()
{
    QVector<QPointF> points = {{0.0, 0.0}, {10.0, 0.0}, {0.5, 0.5}};
    QColor color(255, 255, 255, 255);
    float penWidth = 4.0f;

    auto verts = QCPLineExtruder::extrudePolyline(points, penWidth, color);

    // Bevel adds 1 extra triangle (3 verts) at the join
    // 2 quads (12) + 1 bevel triangle (3) = 15 vertices
    QCOMPARE(verts.size(), 15 * 6);
}

void TestLineExtruder::nanGap()
{
    QVector<QPointF> points = {
        {0.0, 0.0}, {10.0, 0.0},
        {qQNaN(), qQNaN()},
        {20.0, 0.0}, {30.0, 0.0}
    };
    QColor color(255, 0, 0, 255);
    float penWidth = 2.0f;

    auto verts = QCPLineExtruder::extrudePolyline(points, penWidth, color);

    // Two separate segments, each 1 quad = 6 verts → 12 total
    QCOMPARE(verts.size(), 12 * 6);
}

void TestLineExtruder::singlePoint()
{
    QVector<QPointF> points = {{5.0, 5.0}};
    auto verts = QCPLineExtruder::extrudePolyline(points, 2.0f, Qt::red);
    QVERIFY(verts.isEmpty());
}

void TestLineExtruder::emptyInput()
{
    QVector<QPointF> points;
    auto verts = QCPLineExtruder::extrudePolyline(points, 2.0f, Qt::red);
    QVERIFY(verts.isEmpty());
}

void TestLineExtruder::twoPoints()
{
    QVector<QPointF> points = {{0.0, 0.0}, {10.0, 0.0}};
    auto verts = QCPLineExtruder::extrudePolyline(points, 2.0f, Qt::red);
    QCOMPARE(verts.size(), 6 * 6);
}

void TestLineExtruder::consecutiveNaNs()
{
    QVector<QPointF> points = {
        {0.0, 0.0}, {10.0, 0.0},
        {qQNaN(), qQNaN()}, {qQNaN(), qQNaN()},
        {20.0, 0.0}, {30.0, 0.0}
    };
    auto verts = QCPLineExtruder::extrudePolyline(points, 2.0f, Qt::red);
    QCOMPARE(verts.size(), 12 * 6);
}

void TestLineExtruder::nanAtStart()
{
    QVector<QPointF> points = {{qQNaN(), qQNaN()}, {0.0, 0.0}, {10.0, 0.0}};
    auto verts = QCPLineExtruder::extrudePolyline(points, 2.0f, Qt::red);
    QCOMPARE(verts.size(), 6 * 6);
}

void TestLineExtruder::nanAtEnd()
{
    QVector<QPointF> points = {{0.0, 0.0}, {10.0, 0.0}, {qQNaN(), qQNaN()}};
    auto verts = QCPLineExtruder::extrudePolyline(points, 2.0f, Qt::red);
    QCOMPARE(verts.size(), 6 * 6);
}

void TestLineExtruder::duplicatePoints()
{
    QVector<QPointF> points = {{5.0, 5.0}, {5.0, 5.0}, {15.0, 5.0}};
    auto verts = QCPLineExtruder::extrudePolyline(points, 2.0f, Qt::red);
    QVERIFY(!verts.isEmpty());
}

void TestLineExtruder::zeroWidthPen()
{
    QVector<QPointF> points = {{0.0, 0.0}, {10.0, 0.0}};
    auto verts = QCPLineExtruder::extrudePolyline(points, 0.0f, Qt::red);
    QVERIFY(verts.isEmpty());
}

void TestLineExtruder::infGap()
{
    constexpr double inf = std::numeric_limits<double>::infinity();
    QVector<QPointF> points = {
        {0.0, 0.0}, {10.0, 0.0},
        {inf, inf},
        {20.0, 0.0}, {30.0, 0.0}
    };
    auto verts = QCPLineExtruder::extrudePolyline(points, 2.0f, Qt::red);
    QCOMPARE(verts.size(), 12 * 6);
}

void TestLineExtruder::infAtStart()
{
    constexpr double inf = std::numeric_limits<double>::infinity();
    QVector<QPointF> points = {{inf, inf}, {0.0, 0.0}, {10.0, 0.0}};
    auto verts = QCPLineExtruder::extrudePolyline(points, 2.0f, Qt::red);
    QCOMPARE(verts.size(), 6 * 6);
}

void TestLineExtruder::infAtEnd()
{
    constexpr double inf = std::numeric_limits<double>::infinity();
    QVector<QPointF> points = {{0.0, 0.0}, {10.0, 0.0}, {inf, inf}};
    auto verts = QCPLineExtruder::extrudePolyline(points, 2.0f, Qt::red);
    QCOMPARE(verts.size(), 6 * 6);
}

void TestLineExtruder::mixedNanInf()
{
    constexpr double inf = std::numeric_limits<double>::infinity();
    QVector<QPointF> points = {
        {0.0, 0.0}, {10.0, 0.0},
        {qQNaN(), inf},
        {20.0, 0.0}, {30.0, 0.0},
        {inf, qQNaN()},
        {40.0, 0.0}, {50.0, 0.0}
    };
    auto verts = QCPLineExtruder::extrudePolyline(points, 2.0f, Qt::red);
    QCOMPARE(verts.size(), 18 * 6);
}

void TestLineExtruder::bothPointsInf()
{
    constexpr double inf = std::numeric_limits<double>::infinity();
    QVector<QPointF> points = {{inf, 0.0}, {inf, 10.0}};
    auto verts = QCPLineExtruder::extrudePolyline(points, 2.0f, Qt::red);
    QVERIFY(verts.isEmpty());
}

void TestLineExtruder::negativeInf()
{
    constexpr double inf = std::numeric_limits<double>::infinity();
    QVector<QPointF> points = {
        {0.0, 0.0}, {10.0, 0.0},
        {-inf, -inf},
        {20.0, 0.0}, {30.0, 0.0}
    };
    auto verts = QCPLineExtruder::extrudePolyline(points, 2.0f, Qt::red);
    QCOMPARE(verts.size(), 12 * 6);
}

void TestLineExtruder::singleInfCoord()
{
    constexpr double inf = std::numeric_limits<double>::infinity();
    QVector<QPointF> points = {
        {0.0, 0.0}, {10.0, 0.0},
        {inf, 5.0},
        {20.0, 0.0}, {30.0, 0.0}
    };
    auto verts = QCPLineExtruder::extrudePolyline(points, 2.0f, Qt::red);
    QCOMPARE(verts.size(), 12 * 6);
}

void TestLineExtruder::overflowToInf()
{
    constexpr double big = 1e200;
    QVector<QPointF> points = {
        {0.0, 0.0}, {10.0, 0.0},
        {big, big},
        {20.0, 0.0}, {30.0, 0.0}
    };
    auto verts = QCPLineExtruder::extrudePolyline(points, 2.0f, Qt::red);
    // big*big overflows to Inf in normalized() — must not crash
    QVERIFY(!verts.isEmpty());
}

void TestLineExtruder::fillHorizontalBaseline()
{
    // Polygon: base0(0,10), curve(0,0), curve(5,0), curve(10,0), base1(10,10)
    // Horizontal baseline at y=10, curve at y=0
    QPolygonF poly;
    poly << QPointF(0, 10) << QPointF(0, 0) << QPointF(5, 0) << QPointF(10, 0) << QPointF(10, 10);
    QColor color(0, 0, 255, 255);

    auto verts = QCPLineExtruder::tessellateFillPolygon(poly, color);

    // 3 curve points → first cap (3 verts) + 2 trapezoid quads (12 verts) + last cap (3 verts) = 18
    QCOMPARE(verts.size(), 18 * 6);
}

void TestLineExtruder::fillVerticalBaseline()
{
    // Vertical baseline at x=10
    QPolygonF poly;
    poly << QPointF(10, 0) << QPointF(0, 0) << QPointF(0, 5) << QPointF(0, 10) << QPointF(10, 10);
    QColor color(255, 0, 0, 255);

    auto verts = QCPLineExtruder::tessellateFillPolygon(poly, color);

    QCOMPARE(verts.size(), 18 * 6);
}

void TestLineExtruder::fillTooFewPoints()
{
    QPolygonF poly;
    poly << QPointF(0, 0) << QPointF(5, 5) << QPointF(10, 0);
    auto verts = QCPLineExtruder::tessellateFillPolygon(poly, Qt::red);
    QVERIFY(verts.isEmpty()); // < 4 points
}

void TestLineExtruder::fillMinimalTrapezoid()
{
    // 4 points: base0, c0, c1, base1 → 2 curve points
    QPolygonF poly;
    poly << QPointF(0, 10) << QPointF(0, 0) << QPointF(10, 0) << QPointF(10, 10);
    auto verts = QCPLineExtruder::tessellateFillPolygon(poly, Qt::red);

    // 2 curve points → first cap (3) + 1 quad (6) + last cap (3) = 12
    QCOMPARE(verts.size(), 12 * 6);
}
