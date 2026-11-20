#include "test-connector.h"

#include <coords.h>
#include <items/connector.h>

#include <QLineF>
#include <QPainterPath>
#include <QPointF>

#include <array>
#include <limits>
#include <utility>

using namespace QCP::Literals;

/*! The path a connector is expected to draw, built element by element so a row reads like
  matplotlib's vertices: one `quadTo(control, end)` is one curve.
*/
struct ExpectedPath
{
    QPainterPath path;

    ExpectedPath& moveTo(QPointF point)
    {
        path.moveTo(point);
        return *this;
    }

    ExpectedPath& lineTo(QPointF point)
    {
        path.lineTo(point);
        return *this;
    }

    ExpectedPath& quadTo(QPointF control, QPointF end)
    {
        path.quadTo(control, end);
        return *this;
    }
};

Q_DECLARE_METATYPE(QCPConnector)
Q_DECLARE_METATYPE(ExpectedPath)

namespace
{
constexpr QPointF kStart { 200, 100 };
constexpr QPointF kEnd { 50, 300 };
constexpr double kTolerance = 0.05;

void expectSameElements(const QPainterPath& actual, const QPainterPath& expected)
{
    QCOMPARE(actual.elementCount(), expected.elementCount());
    for (int i = 0; i < actual.elementCount(); ++i)
    {
        const QPainterPath::Element actualElement = actual.elementAt(i);
        const QPainterPath::Element expectedElement = expected.elementAt(i);
        QCOMPARE(int(actualElement.type), int(expectedElement.type));
        QVERIFY2(qAbs(actualElement.x - expectedElement.x) <= kTolerance
                     && qAbs(actualElement.y - expectedElement.y) <= kTolerance,
                 qPrintable(QStringLiteral("element %1 is (%2, %3), expected (%4, %5)")
                                .arg(i)
                                .arg(actualElement.x)
                                .arg(actualElement.y)
                                .arg(expectedElement.x)
                                .arg(expectedElement.y)));
    }
}
} // namespace

// The expected paths were computed with matplotlib 3.11's ConnectionStyle.connect() on y-negated
// points, the results negated back: one row per style, start (200, 100) to end (50, 300).
void TestConnector::connectorPathsMatchMatplotlib_data()
{
    QTest::addColumn<QCPConnector>("connector");
    QTest::addColumn<ExpectedPath>("expected");

    QTest::newRow("arc(0.3)") << QCPConnector::arc(0.3)
                              << ExpectedPath { }.moveTo(kStart).quadTo({ 65, 155 }, kEnd);
    QTest::newRow("arc(-0.3)") << QCPConnector::arc(-0.3)
                               << ExpectedPath { }.moveTo(kStart).quadTo({ 185, 245 }, kEnd);
    QTest::newRow("curve(90, 0)") << QCPConnector::curve(90, 0)
                                  << ExpectedPath { }.moveTo(kStart).quadTo({ 200, 300 }, kEnd);
    QTest::newRow("elbow(-90, 180)")
        << QCPConnector::elbow(-90, 180)
        << ExpectedPath { }.moveTo(kStart).lineTo({ 200, 300 }).lineTo(kEnd);
    QTest::newRow("elbow(-90, 180, 5px)") << QCPConnector::elbow(-90, 180, 5_px)
                                          << ExpectedPath { }
                                                 .moveTo(kStart)
                                                 .lineTo({ 200, 295 })
                                                 .quadTo({ 200, 300 }, { 195, 300 })
                                                 .lineTo(kEnd);
    QTest::newRow("elbow(-90, 180, -5px)") << QCPConnector::elbow(-90, 180, QCP::PixelLength { -5 })
                                           << ExpectedPath { }
                                                  .moveTo(kStart)
                                                  .lineTo({ 200, 305 })
                                                  .quadTo({ 200, 300 }, { 205, 300 })
                                                  .lineTo(kEnd);
    QTest::newRow("arms(-90, 30px, 0, 30px)") << QCPConnector::arms(-90, 30_px, 0, 30_px)
                                              << ExpectedPath { }
                                                     .moveTo(kStart)
                                                     .lineTo({ 200, 130 })
                                                     .quadTo({ 200, 130 }, { 200, 130 })
                                                     .lineTo({ 80, 300 })
                                                     .quadTo({ 80, 300 }, { 80, 300 })
                                                     .lineTo(kEnd);
    QTest::newRow("arms(-90, 30px, 0, 30px, 5px)") << QCPConnector::arms(-90, 30_px, 0, 30_px, 5_px)
                                                   << ExpectedPath { }
                                                          .moveTo(kStart)
                                                          .lineTo({ 200, 125 })
                                                          .quadTo({ 200, 130 }, { 197.1, 134.1 })
                                                          .lineTo({ 82.88, 295.9 })
                                                          .quadTo({ 80, 300 }, { 75, 300 })
                                                          .lineTo(kEnd);
    QTest::newRow("arms(-90, 0px, 0, 40px)") << QCPConnector::arms(-90, 0_px, 0, 40_px)
                                             << ExpectedPath { }
                                                    .moveTo(kStart)
                                                    .lineTo({ 90, 300 })
                                                    .quadTo({ 90, 300 }, { 90, 300 })
                                                    .lineTo(kEnd);
    QTest::newRow("arms(-90, -30px, 0, 30px, 5px)")
        << QCPConnector::arms(-90, QCP::PixelLength { -30 }, 0, 30_px, 5_px)
        << ExpectedPath { }
               .moveTo(kStart)
               .lineTo({ 200, 65 })
               .quadTo({ 200, 70 }, { 197.6872, 74.4329 })
               .lineTo({ 82.3128, 295.5671 })
               .quadTo({ 80, 300 }, { 75, 300 })
               .lineTo(kEnd);
    // A zero fraction leaves both runs where they are, so each of the two corners coincides with
    // the point it was pushed from: matplotlib draws all four vertices.
    const ExpectedPath unmovedBracket
        = ExpectedPath { }.moveTo(kStart).lineTo(kStart).lineTo(kEnd).lineTo(kEnd);
    QTest::newRow("bracket(0)") << QCPConnector::bracket(0) << unmovedBracket;
    QTest::newRow("bracket(0.3)")
        << QCPConnector::bracket(0.3)
        << ExpectedPath { }.moveTo(kStart).lineTo({ 140, 55 }).lineTo({ -10, 255 }).lineTo(kEnd);
    QTest::newRow("bracket(-0.2, 180)")
        << QCPConnector::bracket(-0.2, 180)
        << ExpectedPath { }.moveTo(kStart).lineTo({ 200, 150 }).lineTo({ 50, 150 }).lineTo(kEnd);
}

void TestConnector::connectorPathsMatchMatplotlib()
{
    QFETCH(QCPConnector, connector);
    QFETCH(ExpectedPath, expected);

    expectSameElements(connector.path(kStart, kEnd), expected.path);
}

void TestConnector::arcZeroIsStraight()
{
    const QPointF start { 30, 220 };
    const QPointF end { 280, 40 };
    const QPainterPath path = QCPConnector::arc(0).path(start, end);

    for (const double fraction : { 0.25, 0.5, 0.75 })
    {
        const QPointF onSegment = start + fraction * (end - start);
        QVERIFY2(QLineF(path.pointAtPercent(fraction), onSegment).length() <= 1e-6,
                 qPrintable(QStringLiteral("at %1 the path is %2, %3 on the segment")
                                .arg(fraction)
                                .arg(path.pointAtPercent(fraction).x())
                                .arg(path.pointAtPercent(fraction).y())));
    }
}

void TestConnector::parallelDirectionsGiveAStraightPath()
{
    const QPointF start { 0, 0 };
    const QPointF end { 100, 0 };
    // The last three pairs point opposite ways: their directions' cross product is close to zero
    // (sin of 180 degrees is ~1e-16), not exactly zero.
    const std::array<std::pair<double, double>, 4> anglePairs {
        std::pair { 90.0, 90.0 },
        std::pair { 0.0, 180.0 },
        std::pair { -90.0, 90.0 },
        std::pair { 45.0, 225.0 },
    };

    for (const auto& [startAngle, endAngle] : anglePairs)
        for (const QCPConnector& connector : { QCPConnector::curve(startAngle, endAngle),
                                               QCPConnector::elbow(startAngle, endAngle) })
        {
            const QPainterPath path = connector.path(start, end);
            QVERIFY2(path.elementCount() == 2,
                     qPrintable(QStringLiteral("angles %1 and %2 give %3 elements")
                                    .arg(startAngle)
                                    .arg(endAngle)
                                    .arg(path.elementCount())));
            QCOMPARE(int(path.elementAt(0).type), int(QPainterPath::MoveToElement));
            QCOMPARE(int(path.elementAt(1).type), int(QPainterPath::LineToElement));
            QCOMPARE(QPointF(path.elementAt(0)), start);
            QCOMPARE(QPointF(path.elementAt(1)), end);
        }
}

void TestConnector::aZeroLengthConnectorIsEmpty()
{
    const QPointF at { 120, 80 };
    const std::array<QCPConnector, 9> connectors {
        QCPConnector { },
        QCPConnector::arc(0.3),
        QCPConnector::curve(90, 0),
        QCPConnector::curve(0, 180),
        QCPConnector::elbow(-90, 180),
        QCPConnector::elbow(-90, 180, 5_px),
        QCPConnector::arms(-90, 30_px, 0, 30_px, 5_px),
        QCPConnector::bracket(0.3),
        QCPConnector::bracket(0.3, 45),
    };

    for (const QCPConnector& connector : connectors)
        QVERIFY(connector.path(at, at).isEmpty());
}

void TestConnector::aNonFiniteConnectorIsEmpty()
{
    const QPointF start { 200, 100 };
    const QPointF end { 50, 300 };
    const double notANumber = std::numeric_limits<double>::quiet_NaN();

    QVERIFY(QCPConnector::arc(0.3).path(QPointF { notANumber, 100 }, end).isEmpty());
    QVERIFY(QCPConnector::arc(0.3).path(start, QPointF { 50, notANumber }).isEmpty());
    QVERIFY(QCPConnector::arc(notANumber).path(start, end).isEmpty());
    QVERIFY(QCPConnector::curve(notANumber, 0).path(start, end).isEmpty());
    QVERIFY(QCPConnector::elbow(-90, notANumber).path(start, end).isEmpty());
    QVERIFY(QCPConnector::arms(notANumber, 30_px, 0, 30_px, 5_px).path(start, end).isEmpty());
    QVERIFY(QCPConnector::bracket(notANumber).path(start, end).isEmpty());
    QVERIFY(QCPConnector::bracket(0.3, notANumber).path(start, end).isEmpty());
}

void TestConnector::connectorsCompareEqual()
{
    QVERIFY(QCPConnector::arc(0.3) == QCPConnector::arc(0.3));
    QVERIFY(QCPConnector::arc(0.3) != QCPConnector::arc(-0.3));
    QVERIFY(QCPConnector { } == QCPConnector::arc(0));
    QVERIFY(QCPConnector::bracket(0.3) != QCPConnector::bracket(0.3, 0));
}
