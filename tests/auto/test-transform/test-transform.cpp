#include "test-transform.h"

#include "misuse-recorder.h"
#include <coords.h>
#include <polar/angularaxis.h>
#include <polar/polar-area.h>
#include <polar/radialaxis.h>
#include <qcustomplot.h>

#include <memory>
#include <vector>

namespace
{
// A polar area alone in the layout, laid out: its centre and radius only exist after a pass.
QCPPolarArea* polarArea(QCustomPlot& plot)
{
    plot.resize(400, 300);
    plot.plotLayout()->clear();
    auto* area = new QCPPolarArea(&plot);
    plot.plotLayout()->addElement(0, 0, area);
    plot.replot();
    return area;
}

bool nearlyEqual(double actual, double expected, double tolerance = 1e-9)
{
    if (qIsNaN(actual) || qIsNaN(expected))
        return qIsNaN(actual) && qIsNaN(expected);
    return qAbs(actual - expected) <= tolerance;
}

QPointF pixelsOf(const QCPTransform& transform, double key, double value)
{
    const QCP::Pixels pixel = transform.map(QCP::Data { key, value });
    return QPointF(pixel.x, pixel.y);
}

// The radius a coordinate should map to, written out from the radial axis' own rule: the K-6 clamp
// applies on top of it.
double expectedFraction(double coord, const QCPRange& range, bool logarithmic, bool reversed)
{
    double fraction = 0;
    if (!logarithmic)
        fraction = (reversed ? range.upper - coord : coord - range.lower) / range.size();
    else
    {
        const double ratio = range.upper / range.lower;
        fraction = reversed ? qLn(range.upper / coord) / qLn(ratio)
                            : qLn(coord / range.lower) / qLn(ratio);
    }
    return qBound(0.0, fraction, 1.05);
}

// A mapping that counts how often its batch entry point runs.
class CountingMapping : public QCPCustomMapping
{
public:
    mutable int calls = 0;
    mutable int points = 0;

    void map(std::span<const double> keys, std::span<const double> values,
             std::span<QPointF> out) const override
    {
        ++calls;
        points += int(std::min({ keys.size(), values.size(), out.size() }));
        std::fill(out.begin(), out.end(), QPointF(0, 0));
    }

    QCP::Data inverse(QPointF pixel) const override { return QCP::Data { pixel.x(), pixel.y() }; }
};
} // namespace

void TestTransform::init()
{
    test_misuse::calls.clear();
    qcp::setMisuseHandler(test_misuse::record);
}

void TestTransform::cleanup()
{
    qcp::setMisuseHandler(nullptr);
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
}

void TestTransform::polarMappingMatchesTheAxes_data()
{
    QTest::addColumn<bool>("radialLog");
    QTest::addColumn<bool>("radialReversed");
    QTest::addColumn<bool>("angularReversed");
    QTest::addColumn<double>("startAngle");
    QTest::addColumn<double>("angularLower");
    QTest::addColumn<double>("angularUpper");

    QTest::newRow("linear") << false << false << false << 0.0 << 0.0 << 360.0;
    QTest::newRow("linear reversed radial") << false << true << false << 0.0 << 0.0 << 360.0;
    QTest::newRow("log radial") << true << false << false << 0.0 << 0.0 << 360.0;
    QTest::newRow("log reversed radial") << true << true << false << 0.0 << 0.0 << 360.0;
    QTest::newRow("reversed angular") << false << false << true << 0.0 << 0.0 << 360.0;
    QTest::newRow("start angle -90") << false << false << false << -90.0 << 0.0 << 360.0;
    QTest::newRow("start angle 30, radians") << false << false << false << 30.0 << 0.0 << 2 * M_PI;
    QTest::newRow("start angle 30, radians, reversed angular, log radial")
        << true << false << true << 30.0 << 0.0 << 2 * M_PI;
}

void TestTransform::polarMappingMatchesTheAxes()
{
    QFETCH(bool, radialLog);
    QFETCH(bool, radialReversed);
    QFETCH(bool, angularReversed);
    QFETCH(double, startAngle);
    QFETCH(double, angularLower);
    QFETCH(double, angularUpper);

    QCustomPlot plot;
    QCPPolarArea* area = polarArea(plot);
    QCPPolarAxisAngular* angular = area->angularAxis();
    QCPPolarAxisRadial* radial = area->radialAxis();
    QVERIFY(area->radius() > 0);
    angular->setRange(angularLower, angularUpper);
    angular->setRangeReversed(angularReversed);
    angular->setAngle(startAngle);
    const QCPRange radialRange = radialLog ? QCPRange(0.5, 8.0) : QCPRange(0.0, 5.0);
    radial->setScaleType(radialLog ? QCPAxisBase::stLogarithmic : QCPAxisBase::stLinear);
    radial->setRange(radialRange.lower, radialRange.upper);
    radial->setRangeReversed(radialReversed);
    plot.replot();

    const QCPTransform transform = QCPTransform::polar(*angular, *radial);
    QVERIFY(transform.valid());
    QCOMPARE(transform.kind(), QCPTransform::Kind::Polar);

    for (int i = 0; i < 50; ++i)
    {
        const double t = 0.1 + 0.8 * i / 49.0; // stay inside the range: the clamp is not under test
        const double key = angularLower + (angularUpper - angularLower) * t;
        const double value = radialLog
            ? std::exp(qLn(radialRange.lower) + qLn(radialRange.upper / radialRange.lower) * t)
            : radialRange.lower + radialRange.size() * t;
        const QPointF expected = radial->coordToPixel(key, value);
        const QPointF actual = pixelsOf(transform, key, value);
        QVERIFY2(nearlyEqual(actual.x(), expected.x()) && nearlyEqual(actual.y(), expected.y()),
                 qPrintable(QStringLiteral("key %1 value %2: (%3, %4) != (%5, %6)")
                                .arg(key)
                                .arg(value)
                                .arg(actual.x())
                                .arg(actual.y())
                                .arg(expected.x())
                                .arg(expected.y())));
    }
}

void TestTransform::polarInverseRoundTrips()
{
    QCustomPlot plot;
    QCPPolarArea* area = polarArea(plot);
    QCPPolarAxisAngular* angular = area->angularAxis();
    QCPPolarAxisRadial* radial = area->radialAxis();
    angular->setAngle(20.0);
    radial->setRange(0.5, 5.0);
    radial->setScaleType(QCPAxisBase::stLogarithmic);
    plot.replot();

    const QCPTransform transform = QCPTransform::polar(*angular, *radial);
    QVERIFY(transform.valid());
    for (int i = 0; i < 50; ++i)
    {
        const double key = 360.0 * i / 49.0;
        const double value = qPow(5.0 / 0.5, 0.2 + 0.6 * i / 49.0) * 0.5;
        const QPointF pixel = pixelsOf(transform, key, value);
        const QCP::Data back = transform.inverse(pixel);
        QVERIFY(nearlyEqual(back.y, value, 1e-9 * qAbs(value)));
        // A pixel does not know the revolution: the angle comes back modulo the angular range,
        // so what must round-trip is the pixel.
        const QPointF again = pixelsOf(transform, back.x, back.y);
        QVERIFY(nearlyEqual(again.x(), pixel.x()));
        QVERIFY(nearlyEqual(again.y(), pixel.y()));
    }
}

void TestTransform::polarClampsTheRadius_data()
{
    QTest::addColumn<bool>("radialLog");
    QTest::newRow("linear") << false;
    QTest::newRow("log") << true;
}

void TestTransform::polarClampsTheRadius()
{
    QFETCH(bool, radialLog);

    QCustomPlot plot;
    QCPPolarArea* area = polarArea(plot);
    QCPPolarAxisAngular* angular = area->angularAxis();
    QCPPolarAxisRadial* radial = area->radialAxis();
    const QCPRange range = radialLog ? QCPRange(0.5, 8.0) : QCPRange(0.0, 5.0);
    radial->setScaleType(radialLog ? QCPAxisBase::stLogarithmic : QCPAxisBase::stLinear);
    radial->setRange(range.lower, range.upper);
    plot.replot();

    const QCPTransform transform = QCPTransform::polar(*angular, *radial);
    QVERIFY(transform.valid());
    const QPointF centre = area->center();
    const double radius = area->radius();
    const double angle = 45.0;
    const double theta = angular->coordToAngleRad(angle);
    const QPointF direction(qCos(theta), qSin(theta));

    const double below = radialLog ? range.lower * 0.25 : range.lower - 10.0;
    const QPointF atTheCentre = pixelsOf(transform, angle, below);
    QVERIFY2(nearlyEqual(atTheCentre.x(), centre.x()) && nearlyEqual(atTheCentre.y(), centre.y()),
             "a radius below the range clamps to the centre");

    const double above = radialLog ? range.lower * qPow(range.upper / range.lower, 2.0)
                                   : range.lower + range.size() * 5.0;
    const QPointF clamped = pixelsOf(transform, angle, above);
    QVERIFY(nearlyEqual(clamped.x(), centre.x() + direction.x() * 1.05 * radius));
    QVERIFY(nearlyEqual(clamped.y(), centre.y() + direction.y() * 1.05 * radius));
    // The clamp is the mapping's own rule, not something coordToPixel already did.
    QVERIFY(expectedFraction(above, range, radialLog, false) == 1.05);
}

void TestTransform::polarEqualityFollowsTheRanges()
{
    QCustomPlot plot;
    QCPPolarArea* area = polarArea(plot);
    QCPPolarAxisAngular* angular = area->angularAxis();
    QCPPolarAxisRadial* radial = area->radialAxis();
    plot.replot();

    const QCPTransform before = QCPTransform::polar(*angular, *radial);
    QVERIFY(before == QCPTransform::polar(*angular, *radial));

    angular->setAngle(45.0);
    const QCPTransform rotated = QCPTransform::polar(*angular, *radial);
    QVERIFY(!(rotated == before));

    radial->setRange(1.0, 3.0);
    const QCPTransform zoomed = QCPTransform::polar(*angular, *radial);
    QVERIFY(!(zoomed == before));
    QVERIFY(!before.isTranslationOf(rotated));
    QVERIFY(!before.isTranslationOf(zoomed));
    QVERIFY(!before.isTranslationOf(before));
}

void TestTransform::customMapsOncePerSpan()
{
    auto mapping = std::make_shared<CountingMapping>();
    const QCPTransform transform = QCPTransform::custom(mapping, 7);
    QVERIFY(transform.valid());
    QCOMPARE(transform.kind(), QCPTransform::Kind::Custom);

    std::vector<double> keys(1000, 1.0);
    std::vector<double> values(1000, 2.0);
    std::vector<QPointF> out(1000);
    transform.map(std::span<const double>(keys), std::span<const double>(values),
                  std::span<QPointF>(out));
    QCOMPARE(mapping->calls, 1);
    QCOMPARE(mapping->points, 1000);

    transform.map(std::span<const QPointF>(out), std::span<QPointF>(out));
    QCOMPARE(mapping->calls, 2);

    transform.map(QCP::Data { 1.0, 2.0 });
    QCOMPARE(mapping->calls, 3);
    QCOMPARE(transform.inverse(QPointF(3, 4)).x, 3.0);
}

void TestTransform::customEqualityUsesTheRevision()
{
    auto first = std::make_shared<CountingMapping>();
    auto second = std::make_shared<CountingMapping>();
    QVERIFY(QCPTransform::custom(first, 1) == QCPTransform::custom(first, 1));
    QVERIFY(!(QCPTransform::custom(first, 1) == QCPTransform::custom(first, 2)));
    QVERIFY(!(QCPTransform::custom(first, 1) == QCPTransform::custom(second, 1)));
    QVERIFY(!(QCPTransform::custom(first, 1) == QCPTransform()));
}

void TestTransform::cartesianInverseMatchesPixelToCoord_data()
{
    QTest::addColumn<bool>("verticalKey");
    QTest::addColumn<bool>("logValue");
    QTest::newRow("horizontal key") << false << false;
    QTest::newRow("vertical key") << true << false;
    QTest::newRow("log value") << false << true;
    QTest::newRow("vertical key, log value") << true << true;
}

void TestTransform::cartesianInverseMatchesPixelToCoord()
{
    QFETCH(bool, verticalKey);
    QFETCH(bool, logValue);

    QCustomPlot plot;
    plot.resize(640, 480);
    QCPAxis* keyAxis = verticalKey ? plot.yAxis : plot.xAxis;
    QCPAxis* valueAxis = verticalKey ? plot.xAxis : plot.yAxis;
    keyAxis->setRange(0, 10);
    if (logValue)
    {
        valueAxis->setScaleType(QCPAxisBase::stLogarithmic);
        valueAxis->setRange(1.0, 100.0);
    }
    else
        valueAxis->setRange(-5, 5);
    plot.replot();

    const QCPTransform transform(keyAxis, valueAxis);
    QVERIFY(transform.valid());
    QCOMPARE(transform.kind(), QCPTransform::Kind::Cartesian);

    for (int i = 0; i < 50; ++i)
    {
        const double t = 0.1 + 0.8 * i / 49.0;
        const double key = 0.0 + 10.0 * t;
        const double value = logValue ? qPow(100.0, t) : -5.0 + 10.0 * t;
        const QPointF pixel = pixelsOf(transform, key, value);
        const QCP::Data back = transform.inverse(pixel);
        QVERIFY(nearlyEqual(back.x, keyAxis->pixelToCoord(verticalKey ? pixel.y() : pixel.x())));
        QVERIFY(nearlyEqual(back.y, valueAxis->pixelToCoord(verticalKey ? pixel.x() : pixel.y())));
    }
}

void TestTransform::polarRefusesAxesOfTwoAreas()
{
    // One disc has one centre: an angular axis of one area and a radial one of another have no
    // common frame, so the mapping is refused rather than built from half of each.
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.plotLayout()->clear();
    auto* left = new QCPPolarArea(&plot);
    auto* right = new QCPPolarArea(&plot);
    plot.plotLayout()->addElement(0, 0, left);
    plot.plotLayout()->addElement(0, 1, right);
    plot.replot();

    const QCPTransform crossed = QCPTransform::polar(*left->angularAxis(), *right->radialAxis());
    QCOMPARE(int(test_misuse::calls.size()), 1);
    QVERIFY(!crossed.valid());
    test_misuse::calls.clear();
}
