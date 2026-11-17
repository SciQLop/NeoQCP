#include "test-scale.h"
#include "../../../src/qcp.h"
#include "../misuse-recorder.h"
#include "qcustomplot.h"
#include <array>
#include <cmath>
#include <limits>
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

    // A pan is an offset in scale space and a zoom is a scaling there, on every scale.
    for (const QCPScale& scale : { QCPScale::linear(), QCPScale::log(), QCPScale::symlog(2) })
    {
        const QCPRange range = scale.isLog() ? QCPRange(0.5, 20.0) : QCPRange(-4.0, 8.0);
        const double span = scale.toScale(range.upper) - scale.toScale(range.lower);
        const QCPRange moved = scale.shifted(range, 0.5 * span);
        QVERIFY(sameValue(scale.toScale(moved.lower), scale.toScale(range.lower) + 0.5 * span));
        QVERIFY(sameValue(scale.toScale(moved.upper) - scale.toScale(moved.lower), span));

        const double center = 0.5 * (range.lower + range.upper);
        const QCPRange zoomed = scale.scaled(range, 2.0, center);
        QVERIFY(sameValue(scale.toScale(zoomed.upper) - scale.toScale(zoomed.lower), 2.0 * span));
        QVERIFY(sameValue(scale.toScale(zoomed.lower) - scale.toScale(center),
                          -2.0 * (scale.toScale(center) - scale.toScale(range.lower))));
    }

    // The log scale is defined on |v|: a range below zero keeps its sign through a pan and a
    // zoom. Recorded at BASE 8dff255: a pan by half the span moves both ends by that offset in
    // log10|v| (span -3 -> -1.5, still negative), and a zoom by 2 about the read centre doubles
    // the span with both ends moving further from zero.
    const QCPRange negative(-1000.0, -1.0);
    const double negativeSpan = log.toScale(negative.upper) - log.toScale(negative.lower);
    QVERIFY(sameValue(negativeSpan, -3.0));
    const QCPRange panned = log.shifted(negative, 0.5 * negativeSpan);
    QVERIFY(panned.lower < 0.0 && panned.upper < 0.0);
    QVERIFY(close(panned.lower, -31.622776601683793));
    QVERIFY(close(panned.upper, -0.03162277660168379));
    QVERIFY(sameValue(log.toScale(panned.upper) - log.toScale(panned.lower), negativeSpan));

    const double negativeCentre = 0.5 * (negative.lower + negative.upper);
    const QCPRange zoomedNegative = log.scaled(negative, 2.0, negativeCentre);
    QVERIFY(zoomedNegative.lower < negative.lower);
    QVERIFY(zoomedNegative.upper > negative.upper);
    QVERIFY(sameValue(log.toScale(zoomedNegative.upper) - log.toScale(zoomedNegative.lower),
                      2.0 * negativeSpan));
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

void sendMouse(QCustomPlot* plot, QEvent::Type type, QPoint pos, Qt::MouseButtons buttons)
{
    QMouseEvent e(type, pos, plot->mapToGlobal(pos), Qt::LeftButton, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(plot, &e);
}

void dragFromTo(QCustomPlot* plot, QPoint from, QPoint to)
{
    sendMouse(plot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(plot, QEvent::MouseMove, to, Qt::LeftButton);
    sendMouse(plot, QEvent::MouseButtonRelease, to, Qt::NoButton);
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

// A scale brings the ticker that suits it, on every setScale; a ticker set after it wins, one set
// before it is replaced.
void TestScale::setScaleBringsItsTicker()
{
    QCustomPlot plot;

    plot.xAxis->setScale(QCPScale::log(2));
    const auto logTicker = dynamic_cast<QCPAxisTickerLog*>(plot.xAxis->ticker().data());
    QVERIFY(logTicker != nullptr);
    QCOMPARE(logTicker->logBase(), 2.0);

    plot.yAxis->setScale(QCPScale::symlog(0.5, 4));
    const auto symlogTicker = dynamic_cast<QCPAxisTickerSymlog*>(plot.yAxis->ticker().data());
    QVERIFY(symlogTicker != nullptr);
    QVERIFY(symlogTicker->scale() == QCPScale::symlog(0.5, 4));

    plot.yAxis->setScale(QCPScale::linear());
    QVERIFY(dynamic_cast<QCPAxisTickerLog*>(plot.yAxis->ticker().data()) == nullptr);
    QVERIFY(dynamic_cast<QCPAxisTickerSymlog*>(plot.yAxis->ticker().data()) == nullptr);

    // a ticker set after the scale wins
    const auto dateTime = QSharedPointer<QCPAxisTickerDateTime>::create();
    plot.xAxis->setScale(QCPScale::log());
    plot.xAxis->setTicker(dateTime);
    QCOMPARE(plot.xAxis->ticker(), dateTime);

    // one set before it is replaced, also when the scale does not change
    plot.yAxis->setTicker(dateTime);
    plot.yAxis->setScale(QCPScale::linear());
    QVERIFY(plot.yAxis->ticker() != dateTime);
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

// A symlog axis spreads the band and the log side on one pixel scale: zero is the middle, the
// threshold sits where the scale space puts it, and the two signs mirror.
void TestScale::symlogMapsBothSides()
{
    QCustomPlot plot;
    plot.xAxis->setScale(QCPScale::symlog(2));
    plot.xAxis->setRange(-100, 100);
    settle(plot);

    const double left = plot.xAxis->coordToPixel(-100);
    const double right = plot.xAxis->coordToPixel(100);
    const double width = right - left;
    QVERIFY(width > 0.0);
    QVERIFY(samePixel(plot.xAxis->coordToPixel(0), left + 0.5 * width));
    // A linear fallback puts x = 2 at 0.51 of the width: the band makes it 0.6977.
    QVERIFY2(close((plot.xAxis->coordToPixel(2) - left) / width, 0.6977),
             qPrintable(QStringLiteral("x = 2 at %1").arg((plot.xAxis->coordToPixel(2) - left) / width)));
    for (const double x : { 1.0, 2.0, 10.0, 100.0 })
        QVERIFY2(samePixel(plot.xAxis->coordToPixel(-x) - left, right - plot.xAxis->coordToPixel(x)),
                 qPrintable(QStringLiteral("x = %1").arg(x)));
    // The inverse mapping agrees with the forward one on both sides of the band.
    for (const double x : { -100.0, -10.0, -1.0, 0.0, 1.0, 2.0, 50.0, 100.0 })
        QVERIFY2(samePixel(plot.xAxis->pixelToCoord(plot.xAxis->coordToPixel(x)), x),
                 qPrintable(QStringLiteral("x = %1").arg(x)));
}

// A drag moves a symlog range by a scale-space offset, a wheel zoom about zero keeps zero where it
// is, and neither leaves a NaN behind, even for a range that straddles the band.
void TestScale::dragAndZoomWorkInScaleSpace()
{
    QCustomPlot plot;
    plot.setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    plot.xAxis->setScale(QCPScale::symlog(2));
    plot.xAxis->setRange(-100, 100);
    settle(plot);

    const QCPScale scale = plot.xAxis->scale();
    const QCPRange before = plot.xAxis->range();
    const double spanBefore = scale.toScale(before.upper) - scale.toScale(before.lower);
    const QPoint from(qRound(plot.xAxis->coordToPixel(0)), 150);
    dragFromTo(&plot, from, from + QPoint(60, 0));
    settle(plot);

    const QCPRange afterDrag = plot.xAxis->range();
    QVERIFY(std::isfinite(afterDrag.lower) && std::isfinite(afterDrag.upper));
    QVERIFY(afterDrag.lower != before.lower);
    QVERIFY2(samePixel(scale.toScale(afterDrag.upper) - scale.toScale(afterDrag.lower), spanBefore),
             "a symlog drag must keep the scale-space span");

    // A wheel zoom about zero: zero stays on its pixel, and the range shrinks on both sides.
    plot.xAxis->setRange(-100, 100);
    settle(plot);
    const double centrePixel = plot.xAxis->coordToPixel(0.0);
    const QPointF centre(centrePixel, 150);
    QWheelEvent wheel(centre, plot.mapToGlobal(centre.toPoint()), QPoint(), QPoint(0, 120),
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(&plot, &wheel);
    settle(plot);
    const QCPRange afterZoom = plot.xAxis->range();
    QVERIFY(afterZoom.size() < before.size());
    QVERIFY(std::isfinite(afterZoom.lower) && std::isfinite(afterZoom.upper));
    QVERIFY2(samePixel(plot.xAxis->coordToPixel(0.0), centrePixel),
             "the zoom centre must keep its pixel");

    // A zoom about x = 10: that value keeps its place in the axis rect, and both sides of it scale
    // by the same factor in the scale's own space. A linear fallback would hold the linear span,
    // which is a different factor on each side of the band. The place is measured as a fraction of
    // the rect: a wheel zoom switches the plot's antialiasing off, and that re-lays the margins out.
    const auto placeOf = [&plot](double value)
    {
        const QRect rect = plot.xAxis->axisRect()->rect();
        return (plot.xAxis->coordToPixel(value) - rect.left()) / double(rect.width());
    };
    plot.xAxis->setRange(-100, 100);
    settle(plot);
    const double anchor = 10.0;
    const double anchorPlace = placeOf(anchor);
    const double spanAcross = scale.toScale(100.0) - scale.toScale(-100.0);
    const QPointF anchorPoint(plot.xAxis->coordToPixel(anchor), 150);
    QWheelEvent aboutAnchor(anchorPoint, plot.mapToGlobal(anchorPoint.toPoint()), QPoint(),
                            QPoint(0, 120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase,
                            false);
    QCoreApplication::sendEvent(&plot, &aboutAnchor);
    settle(plot);
    const QCPRange aboutTen = plot.xAxis->range();
    QVERIFY(std::isfinite(aboutTen.lower) && std::isfinite(aboutTen.upper));
    QVERIFY2(samePixel(placeOf(anchor), anchorPlace),
             "a zoom about x = 10 must keep x = 10 where it was on the axis");
    const double factor
        = (scale.toScale(aboutTen.upper) - scale.toScale(aboutTen.lower)) / spanAcross;
    QVERIFY2(factor < 1.0, "the zoom must shrink the range");
    const double below = scale.toScale(anchor) - scale.toScale(aboutTen.lower);
    const double above = scale.toScale(aboutTen.upper) - scale.toScale(anchor);
    QVERIFY2(samePixel(below, factor * (scale.toScale(anchor) - scale.toScale(-100.0))),
             "a symlog zoom scales the keys below its centre by the same factor in scale space");
    QVERIFY2(samePixel(above, factor * (scale.toScale(100.0) - scale.toScale(anchor))),
             "a symlog zoom scales the keys above its centre by the same factor in scale space");

    // A drag of a range that straddles the band: finite bounds, a finite mapping every way.
    plot.xAxis->setRange(-5, 3);
    settle(plot);
    const QPoint straddle(qRound(plot.xAxis->coordToPixel(-1)), 150);
    dragFromTo(&plot, straddle, straddle + QPoint(40, 0));
    settle(plot);
    const QCPRange straddled = plot.xAxis->range();
    for (const auto& edge : { straddled.lower, straddled.upper })
        QVERIFY2(std::isfinite(edge), "a drag across the band gave a non-finite bound");
    for (const double pixel : { plot.xAxis->coordToPixel(straddled.lower),
                                plot.xAxis->coordToPixel(straddled.upper),
                                plot.xAxis->pixelToCoord(plot.xAxis->coordToPixel(straddled.lower)),
                                plot.xAxis->pixelToCoord(plot.xAxis->coordToPixel(straddled.upper)) })
        QVERIFY2(std::isfinite(pixel), "a drag across the band gave a non-finite mapping");
}

// The radial axis takes a symlog scale through the same scale-space mapping, so r = +2 and r = -2
// sit symmetrically on the ring and no point of the graph maps to a NaN.
void TestScale::aRadialAxisTakesSymlog()
{
    QCustomPlot plot;
    plot.plotLayout()->clear();
    auto* area = new QCPPolarArea(&plot);
    plot.plotLayout()->addElement(0, 0, area);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setScale(QCPScale::symlog(2));
    area->radialAxis()->setRange(-10, 10);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 90, 180, 270 },
                                          QVector<double> { -2, 2, -2, 2 });
    QVERIFY(graph);
    settle(plot);

    QCPPolarAxisRadial* radial = area->radialAxis();
    QCOMPARE(radial->scale(), QCPScale::symlog(2));

    const QPointF centre = radial->coordToPixel(0.0, 0.0);
    const QPointF positive = radial->coordToPixel(0.0, 2.0);
    const QPointF negative = radial->coordToPixel(0.0, -2.0);
    for (const QPointF& point : { centre, positive, negative })
        QVERIFY2(std::isfinite(point.x()) && std::isfinite(point.y()),
                 "a symlog radial mapping gave a NaN");
    const auto distance = [](const QPointF& a, const QPointF& b)
    { return std::hypot(a.x() - b.x(), a.y() - b.y()); };
    QVERIFY2(samePixel(distance(positive, centre), distance(negative, centre)),
             "r = 2 and r = -2 must sit the same distance from the centre");
    QVERIFY(radial->coordToRadius(2.0) > radial->coordToRadius(0.0));
    QVERIFY(radial->coordToRadius(0.0) > radial->coordToRadius(-2.0));

    // An asymmetric radius pins the mapping: a linear fallback puts r = 8 at 0.9 of the radius and
    // r = -8 at 0.1, the scale's own space puts them at 0.97323 and 0.02677.
    const double edge = radial->coordToRadius(10.0);
    QVERIFY(edge > 0.0);
    QVERIFY2(close(radial->coordToRadius(8.0) / edge, 0.97323),
             qPrintable(QStringLiteral("r = 8 at %1 of the radius")
                            .arg(radial->coordToRadius(8.0) / edge)));
    QVERIFY2(close(radial->coordToRadius(-8.0) / edge, 0.02677),
             qPrintable(QStringLiteral("r = -8 at %1 of the radius")
                            .arg(radial->coordToRadius(-8.0) / edge)));

    // The polar snapshot maps through the same scale space, and comes back.
    const QCPTransform transform = QCPTransform::polar(*area->angularAxis(), *radial);
    QVERIFY(transform.valid());
    const QCP::Pixels mapped = transform.map(QCP::Data { 90.0, 8.0 });
    const QPointF axisPixel = radial->coordToPixel(90.0, 8.0);
    QVERIFY2(samePixel(mapped.x, axisPixel.x()) && samePixel(mapped.y, axisPixel.y()),
             "the polar snapshot must map r = 8 where the radial axis maps it");
    QVERIFY2(samePixel(transform.inverse(axisPixel).y, 8.0),
             "the polar snapshot must map its own pixel back to r = 8");
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

// Both bulk colorize() overloads read the data scale the single-value color() reads: on a log
// scale they map the same values to the same literals.
void TestScale::colorizeOverloadsAgreeOnALogScale()
{
    QCPColorGradient gradient(QCPColorGradient::gpCold);
    const QCPRange range(0.1, 1e4);
    const QCPScale scale = QCPScale::log();
    std::array<double, 10> values {};
    std::array<QRgb, 10> opaque {};
    std::array<QRgb, 10> withAlpha {};
    std::array<unsigned char, 10> alpha {};
    for (size_t i = 0; i < values.size(); ++i)
    {
        values[i] = 0.1 * std::pow(1e5, double(i) / 9.0);
        alpha[i] = 255;
    }
    gradient.colorize(values.data(), range, opaque.data(), int(values.size()), 1, scale);
    gradient.colorize(values.data(), alpha.data(), range, withAlpha.data(), int(values.size()), 1,
                      scale);
    for (size_t i = 0; i < values.size(); ++i)
    {
        const QRgb expected = gradient.color(values[i], range, scale);
        QCOMPARE(opaque[i], expected);
        QCOMPARE(withAlpha[i], expected);
    }
}

// The colour lookup follows a symlog data scale: each z maps through the scale, so z = 1 sits at
// the scale-space fraction 0.60870 where the linear path gives 0.50005, and the two sides mirror.
// A log or symlog map sends a value at or across zero to an infinity. A colour lookup has to answer
// with the end level that infinity points past, never with an integer cast of it.
void TestScale::anInfiniteColourPositionTakesAnEndLevel()
{
    QCPColorGradient gradient(QCPColorGradient::gpGrayscale);
    const QCPRange range(1.0, 1000.0);
    const QCPScale log = QCPScale::log();
    const QRgb bottom = gradient.color(1.0, range, log);
    const QRgb top = gradient.color(1000.0, range, log);

    QCOMPARE(gradient.color(std::numeric_limits<double>::infinity(), range, log), top);
    QCOMPARE(gradient.color(0.0, range, log), bottom);
    QCOMPARE(gradient.color(-5.0, range, log), bottom);

    const double row[] = { 1000.0, 1.0, 0.0, -5.0 };
    QRgb line[4] = {};
    gradient.colorize(row, range, line, 4, 1, log);
    QCOMPARE(line[0], top);
    QCOMPARE(line[1], bottom);
    QCOMPARE(line[2], bottom);
    QCOMPARE(line[3], bottom);
}

void TestScale::aSymlogColourScaleIsSymmetric()
{
    QCPColorGradient gradient(QCPColorGradient::gpGrayscale);
    gradient.setLevelCount(1001);
    const QCPRange range(-1e4, 1e4);
    const QCPScale scale = QCPScale::symlog(1);

    // The grey level of a colour is its gradient entry: 0 the first, 255 the last.
    const auto fraction = [&](double z) { return qRed(gradient.color(z, range, scale)) / 255.0; };
    const auto at = [&](double z) { return QStringLiteral("z = %1 at fraction %2").arg(z).arg(fraction(z)); };
    const auto mirrors = [&](double z) { return qAbs(fraction(-z) - (1.0 - fraction(z))) <= 0.006; };

    QVERIFY2(qAbs(fraction(0.0) - 0.5) <= 0.006, qPrintable(at(0.0)));
    QVERIFY2(qAbs(fraction(1.0) - 0.60870) <= 0.006,
             qPrintable(at(1.0) + QStringLiteral(", not the scale-space 0.60870")));
    QVERIFY2(qAbs(fraction(10.0) - 0.70652) <= 0.006,
             qPrintable(at(10.0) + QStringLiteral(", not the scale-space 0.70652")));
    for (const double z : { 1.0, 10.0, 1e3, 1e4 })
        QVERIFY2(mirrors(z), qPrintable(QStringLiteral("z = %1 does not mirror").arg(z)));

    // The bulk overloads a map's renderer uses read the same lookup.
    std::array<double, 7> values { -1e4, -10.0, -1.0, 0.0, 1.0, 10.0, 1e4 };
    std::array<QRgb, 7> opaque {};
    std::array<QRgb, 7> withAlpha {};
    std::array<unsigned char, 7> alpha {};
    alpha.fill(255);
    gradient.colorize(values.data(), range, opaque.data(), int(values.size()), 1, scale);
    gradient.colorize(values.data(), alpha.data(), range, withAlpha.data(), int(values.size()), 1,
                      scale);
    for (size_t i = 0; i < values.size(); ++i)
    {
        const QRgb expected = gradient.color(values[i], range, scale);
        QCOMPARE(opaque[i], expected);
        QCOMPARE(withAlpha[i], expected);
    }
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
