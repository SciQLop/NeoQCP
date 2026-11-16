#include "test-scale.h"
#include "../../../src/qcp.h"
#include "../misuse-recorder.h"
#include "qcustomplot.h"
#include <array>
#include <cmath>
#include <memory>

namespace
{
//! The brief quotes matplotlib's values to five decimals; the formulas are exact to within 1e-4.
bool close(double got, double expected)
{
    return std::abs(got - expected) <= 1e-4;
}

//! A round trip through the scale space and back gives the value: no widening for large magnitudes.
bool sameValue(double got, double expected)
{
    return std::abs(got - expected) <= 1e-9 * std::max(1.0, std::abs(expected));
}
} // namespace

void TestScale::initTestCase()
{
    qcp::setMisuseHandler(test_misuse::record);
}

void TestScale::init()
{
    test_misuse::calls.clear();
}

void TestScale::cleanupTestCase()
{
    qcp::setMisuseHandler(nullptr);
}

void TestScale::linearIsIdentity()
{
    QCOMPARE(QCPScale().kind(), QCPScale::Kind::Linear);
    QCOMPARE(QCPScale(), QCPScale::linear());

    const QCPScale scale = QCPScale::linear();
    QVERIFY(scale.isLinear());
    QVERIFY(!scale.isLog());
    QVERIFY(!scale.isSymlog());
    QCOMPARE(scale.toScale(3.5), 3.5);
    QCOMPARE(scale.fromScale(3.5), 3.5);
    QCOMPARE(scale.sanitized(QCPRange(-3, 1)), QCPRange(-3, 1));
}

void TestScale::logIsLogBase()
{
    const QCPScale scale = QCPScale::log();
    QVERIFY(scale.isLog());
    QVERIFY(!scale.isLinear());
    QVERIFY(!scale.isSymlog());
    QCOMPARE(scale.base(), 10.0);
    QVERIFY(close(scale.toScale(1000), 3.0));
    QVERIFY(close(QCPScale::log(2).toScale(8), 3.0));
    QVERIFY(close(scale.toScale(-1000), 3.0)); // the log scale is defined on |v|
    QVERIFY(close(scale.fromScale(3), 1000.0)); // the inverse is the positive value
    QCOMPARE(scale.sanitized(QCPRange(-5, 5)), QCPRange(-5, 5).sanitizedForLogScale());
}

void TestScale::symlogMatchesMatplotlib()
{
    const QCPScale scale = QCPScale::symlog(2);
    QVERIFY(scale.isSymlog());
    QVERIFY(!scale.isLinear());
    QVERIFY(!scale.isLog());
    QCOMPARE(scale.threshold(), 2.0);
    QCOMPARE(scale.base(), 10.0);
    QCOMPARE(scale.linearScale(), 1.0);

    QVERIFY(close(scale.toScale(1), 1.11111));
    QVERIFY(close(scale.toScale(20), 4.22222));
    QVERIFY(close(scale.toScale(-20), -4.22222));
    QVERIFY(close(scale.toScale(100), 5.62020));
    QCOMPARE(scale.toScale(0), 0.0);
    QVERIFY(close(scale.toScale(2), 2.22222));
    // The log side meets the band at the threshold: no jump between the two formulas.
    QVERIFY(close(scale.toScale(std::nextafter(2.0, 3.0)), 2.22222));
    QVERIFY(close(scale.toScale(-20), -scale.toScale(20)));
}

void TestScale::roundTrips()
{
    const QCPScale symlog = QCPScale::symlog(2);
    for (const double value : { -1e6, -20.0, -2.0, -0.5, 0.0, 0.5, 2.0, 20.0, 1e6 })
        QVERIFY(sameValue(symlog.fromScale(symlog.toScale(value)), value));

    const QCPScale log = QCPScale::log();
    for (const double value : { 0.5, 2.0, 20.0, 1000.0, 1e6 })
        QVERIFY(sameValue(log.fromScale(log.toScale(value)), value));
}

void TestScale::badParametersAreRefused()
{
    const auto refused = [](auto make, const QCPScale& expected)
    {
        const size_t before = test_misuse::calls.size();
        const QCPScale scale = make();
        QCOMPARE(test_misuse::calls.size(), before + 1);
        QCOMPARE(scale, expected);
    };

    refused([] { return QCPScale::symlog(0.0); }, QCPScale::symlog());
    refused([] { return QCPScale::symlog(qQNaN()); }, QCPScale::symlog());
    refused([] { return QCPScale::symlog(qInf()); }, QCPScale::symlog());
    refused([] { return QCPScale::symlog(2, 10, 0.0); }, QCPScale::symlog());
    refused([] { return QCPScale::symlog(2, 10, -1.0); }, QCPScale::symlog());
    refused([] { return QCPScale::symlog(2, 1); }, QCPScale::symlog());
    refused([] { return QCPScale::log(1.0); }, QCPScale::log());
    refused([] { return QCPScale::log(qQNaN()); }, QCPScale::log());
}

void TestScale::hashMatchesEquality()
{
    QCOMPARE(qHash(QCPScale::symlog(2)), qHash(QCPScale::symlog(2)));
    QCOMPARE(qHash(QCPScale()), qHash(QCPScale::linear()));
    QVERIFY(qHash(QCPScale::log()) != qHash(QCPScale::symlog()));
    QVERIFY(qHash(QCPScale::linear(), 7) != qHash(QCPScale::linear(), 0));
}

namespace
{
//! The pixel mapping of one axis, or the colour of one value: the value BASE recorded.
bool samePixel(double got, double expected)
{
    return std::abs(got - expected) <= 1e-9 * std::max(1.0, std::abs(expected));
}

void settle(QCustomPlot& plot)
{
    plot.resize(400, 300);
    plot.replot();
    for (int i = 0; i < 10 && plot.isRepaintQueued(); ++i)
        QCoreApplication::processEvents();
}
} // namespace

// An equal scale is not a change: the axis keeps one scale value and announces each real change once.
void TestScale::anAxisTakesAScale()
{
    QCustomPlot plot;
    QCOMPARE(plot.xAxis->scale(), QCPScale::linear());

    int changes = 0;
    QCPScale announced;
    connect(plot.xAxis, &QCPAxis::scaleChanged, this,
            [&](const QCPScale& scale)
            {
                ++changes;
                announced = scale;
            });

    plot.xAxis->setScale(QCPScale::log());
    QVERIFY(plot.xAxis->scale().isLog());
    QCOMPARE(changes, 1);
    QCOMPARE(announced, QCPScale::log());

    plot.xAxis->setScale(QCPScale::log());
    QCOMPARE(changes, 1);
}

// A log axis puts the same values on the same pixels as the scale enum did.
void TestScale::aLogAxisMapsAsBefore()
{
    QCustomPlot plot;
    plot.xAxis->setScale(QCPScale::log());
    plot.xAxis->setRange(0.1, 1e4);
    settle(plot);

    const std::array<double, 20> pixels = { 18,
                                            37.315789473684205,
                                            56.631578947368418,
                                            75.94736842105263,
                                            95.263157894736835,
                                            114.57894736842104,
                                            133.89473684210526,
                                            153.21052631578948,
                                            172.52631578947367,
                                            191.84210526315789,
                                            211.15789473684208,
                                            230.47368421052633,
                                            249.78947368421052,
                                            269.10526315789474,
                                            288.42105263157896,
                                            307.73684210526318,
                                            327.05263157894734,
                                            346.36842105263156,
                                            365.68421052631578,
                                            385 };
    for (size_t i = 0; i < pixels.size(); ++i)
    {
        const double value = 0.1 * std::pow(1e5, double(i) / 19.0);
        QVERIFY2(samePixel(plot.xAxis->coordToPixel(value), pixels[i]),
                 qPrintable(QStringLiteral("value %1").arg(value, 0, 'g', 17)));
    }

    // A negative log range: the pixels of the values, on the same geometry.
    plot.xAxis->setRange(-1000, -1);
    settle(plot);
    const std::array<double, 5> values = { -1000, -500, -100, -10, -1 };
    const std::array<double, 5> negativePixels = { 18, 54.826002802893697, 140.33333333333331,
                                                  262.66666666666663, 385 };
    for (size_t i = 0; i < values.size(); ++i)
        QVERIFY2(samePixel(plot.xAxis->coordToPixel(values[i]), negativePixels[i]),
                 qPrintable(QStringLiteral("value %1").arg(values[i], 0, 'g', 17)));
}

// A log colour scale gives the same colours as the gradient's logarithmic flag did.
void TestScale::aLogColourScaleMapsAsBefore()
{
    QCPColorGradient gradient(QCPColorGradient::gpCold);
    const QCPRange range(0.1, 1e4);
    const std::array<QRgb, 10> expected = { QRgb(4278190130u), QRgb(4278191480u), QRgb(4278193850u),
                                            QRgb(4278199519u), QRgb(4278339831u), QRgb(4278681340u),
                                            QRgb(4279678975u), QRgb(4281201407u), QRgb(4287430655u),
                                            QRgb(4294967295u) };
    for (size_t i = 0; i < expected.size(); ++i)
    {
        const double value = 0.1 * std::pow(1e5, double(i) / 9.0);
        QCOMPARE(gradient.color(value, range, QCPScale::log()), expected[i]);
    }
    // The linear default is unchanged: no scale argument still means linear.
    QCOMPARE(gradient.color(range.lower, range), gradient.color(range.lower, range, QCPScale::linear()));
}

// A data scale chosen before the add is not the joined colour scale's to overwrite.
void TestScale::aDataScaleSetBeforeTheAddWins()
{
    QCustomPlot plot;
    auto map = std::make_unique<QCPColorMap2>();
    map->setDataScale(QCPScale::log());

    auto added = plot.add(std::move(map));
    QVERIFY(added.get());
    QCOMPARE(added->dataScale(), QCPScale::log());

    const auto joinedScale = plot.axisRect()->colorScale();
    QVERIFY(joinedScale.get());
    QVERIFY(joinedScale->dataScale().isLinear());
}
