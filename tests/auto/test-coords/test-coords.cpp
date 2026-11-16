#include "test-coords.h"

#include <coords.h>
#include <painting/draw-commands.h>
#include <painting/draw-context.h>
#include <qcustomplot.h>

#include <cmath>
#include <memory>
#include <random>
#include <type_traits>
#include <vector>

using namespace QCP::Literals;

namespace
{
template <typename A, typename B>
concept Addable = requires(A a, B b) { a + b; };

template <typename A, typename B>
concept Subtractable = requires(A a, B b) { a - b; };

// A laid-out plot: coordToPixel reads the axis rect, which only exists after one layout pass.
std::unique_ptr<QCustomPlot> layoutPlot()
{
    auto plot = std::make_unique<QCustomPlot>();
    plot->resize(640, 480);
    plot->replot();
    return plot;
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

constexpr int kCanvasWidth = 200;
constexpr int kCanvasHeight = 150;

// A plot whose axis rect covers the whole 200x150 canvas, so data (x, y) lands at pixel (x, 150 -
// y) and an image read back shows exactly what a recording drew.
std::unique_ptr<QCustomPlot> canvasPlot()
{
    auto plot = std::make_unique<QCustomPlot>();
    plot->resize(kCanvasWidth, kCanvasHeight);
    plot->axisRect()->setAutoMargins(QCP::msNone);
    plot->axisRect()->setMargins(QMargins(0, 0, 0, 0));
    plot->xAxis->setRange(0, kCanvasWidth);
    plot->yAxis->setRange(0, kCanvasHeight);
    plot->replot();
    return plot;
}

QCPDrawFrame frameOf(const QCustomPlot& plot)
{
    return QCPDrawFrame { QCPTransform(plot.xAxis, plot.yAxis), QCPRatioTransform(plot.axisRect()),
                          QRectF(plot.axisRect()->rect()) };
}

QImage replayed(const QCPDrawList& list, const QCPDrawFrame& frame)
{
    QImage image(kCanvasWidth, kCanvasHeight, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    {
        QCPPainter painter(&image);
        painter.setAntialiasing(false);
        qcpReplayWithPainter(list, painter, frame);
    }
    return image;
}

struct ScaleCase
{
    const char* name;
    bool logarithmic;
    bool reversed;
    bool verticalKey;
};

const ScaleCase kCases[] = {
    { "linear", false, false, false },      { "log", true, false, false },
    { "reversed", false, true, false },     { "log reversed", true, true, false },
    { "vertical key", false, false, true },
};

// Inside the range, outside it, and (for a log axis) the two sign cases the axis draws 200 px
// outside the rect.
std::vector<double> sampleValues(std::mt19937& rng)
{
    std::vector<double> values;
    std::uniform_real_distribution<double> inside(1.0, 1000.0);
    std::uniform_real_distribution<double> spread(-1e4, 1e4);
    for (int i = 0; i < 1000; ++i)
        values.push_back(inside(rng));
    for (int i = 0; i < 50; ++i)
        values.push_back(spread(rng));
    values.push_back(0.0);
    values.push_back(-0.0);
    values.push_back(1.0);
    values.push_back(-1.0);
    return values;
}

void configureLogAxes(QCPAxis* keyAxis, QCPAxis* valueAxis, const ScaleCase& testCase)
{
    const auto scale = testCase.logarithmic ? QCPScale::log() : QCPScale::linear();
    keyAxis->setScale(scale);
    valueAxis->setScale(scale);
    keyAxis->setRange(1, 1000);
    valueAxis->setRange(1, 1000);
    keyAxis->setRangeReversed(testCase.reversed);
    valueAxis->setRangeReversed(testCase.reversed);
}
} // namespace

void TestCoords::mapMatchesCoordToPixel()
{
    std::mt19937 rng(42);
    for (const ScaleCase& testCase : kCases)
    {
        auto plot = layoutPlot();
        QCPAxis* keyAxis = plot->xAxis;
        QCPAxis* valueAxis = plot->yAxis;
        if (testCase.verticalKey)
        {
            keyAxis = plot->axisRect()->addAxis(QCPAxis::atLeft);
            valueAxis = plot->axisRect()->addAxis(QCPAxis::atBottom);
        }
        configureLogAxes(keyAxis, valueAxis, testCase);
        plot->replot();

        const QCPTransform transform(keyAxis, valueAxis);
        QVERIFY2(transform.valid(), testCase.name);

        for (double sample : sampleValues(rng))
        {
            // A vertical key axis puts the value on x, exactly as QCPItemPosition::pixelPosition
            // does.
            const QPointF expected = testCase.verticalKey
                ? QPointF(valueAxis->coordToPixel(sample), keyAxis->coordToPixel(sample))
                : QPointF(keyAxis->coordToPixel(sample), valueAxis->coordToPixel(sample));
            const QPointF actual = pixelsOf(transform, sample, sample);
            QVERIFY2(nearlyEqual(actual.x(), expected.x()),
                     qPrintable(QStringLiteral("%1: key %2 mapped to %3 px, axis says %4")
                                    .arg(QLatin1String(testCase.name))
                                    .arg(sample)
                                    .arg(actual.x())
                                    .arg(expected.x())));
            QVERIFY2(nearlyEqual(actual.y(), expected.y()),
                     qPrintable(QStringLiteral("%1: value %2 mapped to %3 px, axis says %4")
                                    .arg(QLatin1String(testCase.name))
                                    .arg(sample)
                                    .arg(actual.y())
                                    .arg(expected.y())));
        }
    }
}

void TestCoords::batchMatchesScalar()
{
    std::mt19937 rng(11);
    for (const ScaleCase& testCase : kCases)
    {
        auto plot = layoutPlot();
        QCPAxis* keyAxis = plot->xAxis;
        QCPAxis* valueAxis = plot->yAxis;
        if (testCase.verticalKey)
        {
            keyAxis = plot->axisRect()->addAxis(QCPAxis::atLeft);
            valueAxis = plot->axisRect()->addAxis(QCPAxis::atBottom);
        }
        configureLogAxes(keyAxis, valueAxis, testCase);
        plot->replot();

        const QCPTransform transform(keyAxis, valueAxis);
        std::vector<double> keys(10000), values(10000);
        std::vector<QPointF> out(10000);
        std::uniform_real_distribution<double> inside(1.0, 1000.0);
        for (std::size_t i = 0; i < keys.size(); ++i)
        {
            keys[i] = inside(rng);
            values[i] = inside(rng);
        }

        transform.map(keys, values, out);
        for (std::size_t i = 0; i < keys.size(); ++i)
        {
            const QPointF scalar = pixelsOf(transform, keys[i], values[i]);
            QVERIFY2(nearlyEqual(out[i].x(), scalar.x()) && nearlyEqual(out[i].y(), scalar.y()),
                     qPrintable(QStringLiteral("%1: batch %2,%3 vs scalar %4,%5")
                                    .arg(QLatin1String(testCase.name))
                                    .arg(out[i].x())
                                    .arg(out[i].y())
                                    .arg(scalar.x())
                                    .arg(scalar.y())));
        }
    }
}

void TestCoords::interleavedBatchMatchesScalar()
{
    std::mt19937 rng(13);
    // Every scale case, so the interleaved batch shares the same four log/linear instantiations.
    for (const ScaleCase& testCase : kCases)
    {
        auto plot = layoutPlot();
        QCPAxis* keyAxis = plot->xAxis;
        QCPAxis* valueAxis = plot->yAxis;
        if (testCase.verticalKey)
        {
            keyAxis = plot->axisRect()->addAxis(QCPAxis::atLeft);
            valueAxis = plot->axisRect()->addAxis(QCPAxis::atBottom);
        }
        configureLogAxes(keyAxis, valueAxis, testCase);
        plot->replot();

        const QCPTransform transform(keyAxis, valueAxis);
        std::vector<QPointF> data(10000);
        std::uniform_real_distribution<double> inside(1.0, 1000.0);
        for (QPointF& point : data)
            point = QPointF(inside(rng), inside(rng));
        std::vector<QPointF> batch(data.size());
        transform.map(std::span<const QPointF>(data), std::span<QPointF>(batch));

        for (std::size_t i = 0; i < data.size(); ++i)
        {
            const QPointF one = pixelsOf(transform, data[i].x(), data[i].y());
            QVERIFY2(nearlyEqual(batch[i].x(), one.x()),
                     qPrintable(QStringLiteral("%1: x at %2").arg(testCase.name).arg(i)));
            QVERIFY2(nearlyEqual(batch[i].y(), one.y()),
                     qPrintable(QStringLiteral("%1: y at %2").arg(testCase.name).arg(i)));
        }
    }
}

void TestCoords::translationDetected()
{
    auto plot = layoutPlot();
    plot->xAxis->setRange(0, 100);
    plot->yAxis->setRange(0, 10);
    plot->replot();

    const QCPTransform before(plot->xAxis, plot->yAxis);

    plot->xAxis->setRange(25, 125); // same width, shifted window
    plot->replot();
    QPointF delta;
    QVERIFY2(before.isTranslationOf(QCPTransform(plot->xAxis, plot->yAxis), &delta),
             "a shifted range is a translation");
    const QPointF expected
        = pixelsOf(before, 0, 0) - pixelsOf(QCPTransform(plot->xAxis, plot->yAxis), 0, 0);
    QVERIFY(nearlyEqual(delta.x(), expected.x()));
    QVERIFY(nearlyEqual(delta.y(), expected.y()));
    QVERIFY(!qFuzzyIsNull(delta.x()));

    plot->xAxis->setRange(0, 50); // zoomed: the data-to-pixel slope changed
    plot->replot();
    QVERIFY(!before.isTranslationOf(QCPTransform(plot->xAxis, plot->yAxis), &delta));

    plot->xAxis->setRange(1, 100);
    plot->xAxis->setScale(QCPScale::log());
    plot->replot();
    QVERIFY2(!before.isTranslationOf(QCPTransform(plot->xAxis, plot->yAxis), &delta),
             "a log axis is not treated as a translation");

    plot->xAxis->setScale(QCPScale::linear());
    plot->xAxis->setRange(0, 100);
    plot->xAxis->setRangeReversed(true);
    plot->replot();
    QVERIFY(!before.isTranslationOf(QCPTransform(plot->xAxis, plot->yAxis), &delta));
}

void TestCoords::dragPanOnATimeRangeIsATranslation()
{
    auto plot = layoutPlot();
    plot->yAxis->setRange(0, 10);

    // A one-hour window at a 1.7e9 epoch, dragged by a tenth of its span.
    plot->xAxis->setRange(1.7e9, 1.7e9 + 3600.0);
    plot->replot();
    const QCPTransform hour(plot->xAxis, plot->yAxis);
    const double hourDiff = 0.1 * (1.7e9 + 3600.0 - 1.7e9);
    plot->xAxis->setRange(1.7e9 + hourDiff, 1.7e9 + 3600.0 + hourDiff);
    plot->replot();
    QPointF delta;
    QVERIFY2(hour.isTranslationOf(QCPTransform(plot->xAxis, plot->yAxis), &delta),
             "a dragged hour is a translation");

    // A drag adds the same diff to both ends, but the addition rounds: with a span that is not a
    // multiple of the double grid at this epoch, the two spans differ by an ulp. The cached
    // triangles still move by less than a thousandth of a pixel, so this is a translation too.
    const double start = 1.7e9;
    const double span = 1.234567890123;
    plot->xAxis->setRange(start, start + span);
    plot->replot();
    const QCPTransform before(plot->xAxis, plot->yAxis);
    const double diff = -558.2751146554947; // rounds the two spans one ulp apart
    plot->xAxis->setRange(start + diff, start + span + diff);
    plot->replot();
    QVERIFY2(before.isTranslationOf(QCPTransform(plot->xAxis, plot->yAxis), &delta),
             "a dragged time window stays a translation when its span moves by one ulp");
}

void TestCoords::panDeltaIsMeasuredInsideTheRange()
{
    // A drag can move the two ends by one ulp apart (see dragPanOnATimeRangeIsATranslation): the
    // slopes then differ by a thousandth of a pixel across the window, but by tens of pixels at
    // data 0, 1.7e9 keys away. The delta must be the one the visible pixels moved by.
    auto plot = layoutPlot();
    plot->yAxis->setRange(0, 10);
    const double start = 1.7e9;
    const double span = 99.9;
    plot->xAxis->setRange(start, start + span);
    plot->replot();
    const QCPTransform before(plot->xAxis, plot->yAxis);

    const double draggedSpan = span * (1.0 + 4e-9); // one ulp of the epoch, as a drag can leave it
    plot->xAxis->setRange(start + 1.0, start + 1.0 + draggedSpan);
    plot->replot();
    const QCPTransform after(plot->xAxis, plot->yAxis);

    QPointF delta;
    QVERIFY2(before.isTranslationOf(after, &delta),
             "the two slopes are a thousandth of a pixel apart");
    for (const double key : { start, start + span * 0.5, start + span })
    {
        const QPointF moved = pixelsOf(before, key, 5.0) - pixelsOf(after, key, 5.0);
        QVERIFY2(qAbs(moved.x() - delta.x()) <= 1e-3,
                 qPrintable(QStringLiteral("at %1 the axis moved %2 px, the delta says %3")
                                .arg(key)
                                .arg(moved.x())
                                .arg(delta.x())));
        QVERIFY2(qAbs(moved.y() - delta.y()) <= 1e-3,
                 qPrintable(QStringLiteral("value moved %1 px, the delta says %2")
                                .arg(moved.y())
                                .arg(delta.y())));
    }
}

// A log mapping is a line in log space: two log snapshots of the same span are translations, two of
// different spans are not. Comparing the linear slope (zero on a log snapshot) made every log pair a
// translation, so a log zoom was replayed as a pan of the old pixels.
void TestCoords::aLogScaleZoomIsNotATranslation()
{
    auto plot = layoutPlot();
    // Fix the rect: a wider tick label would otherwise re-lay the margins out between snapshots and
    // move the pixels for a reason that has nothing to do with the axis mapping.
    plot->axisRect()->setAutoMargins(QCP::msNone);
    plot->xAxis->setScale(QCPScale::log());
    plot->yAxis->setRange(0, 10);

    plot->xAxis->setRange(1, 100);
    plot->replot();
    const QCPTransform before(plot->xAxis, plot->yAxis);
    QVERIFY(before.valid());

    plot->xAxis->setRange(10, 1000); // a pan: the same ratio, ten times the values
    plot->replot();
    const QCPTransform panned(plot->xAxis, plot->yAxis);
    QVERIFY2(before.isTranslationOf(panned), "a log pan keeps the log span, so it is a translation");

    plot->xAxis->setRange(2, 400); // a zoom: the ratio changed
    plot->replot();
    QVERIFY2(!before.isTranslationOf(QCPTransform(plot->xAxis, plot->yAxis)),
             "a log zoom changes the log span: the data-to-pixel scale changed");

    plot->xAxis->setRange(4, 400); // the same ratio again, so this is a pan too: every value times 0.4
    plot->replot();
    QVERIFY2(panned.isTranslationOf(QCPTransform(plot->xAxis, plot->yAxis)),
             "a pan multiplies every value by one factor, so both edges stay the same distance apart");

    plot->xAxis->setRange(10, 400); // one edge held, the ratio changed
    plot->replot();
    QVERIFY2(!panned.isTranslationOf(QCPTransform(plot->xAxis, plot->yAxis)),
             "a log zoom is not a translation even when both edges keep their sign");

    // A value axis too: the same rule, one axis over.
    plot->xAxis->setScale(QCPScale::linear());
    plot->xAxis->setRange(0, 100);
    plot->yAxis->setScale(QCPScale::log());
    plot->yAxis->setRange(1, 100);
    plot->replot();
    const QCPTransform logValue(plot->xAxis, plot->yAxis);
    plot->yAxis->setRange(10, 1000); // the same ratio: a pan
    plot->replot();
    QVERIFY2(logValue.isTranslationOf(QCPTransform(plot->xAxis, plot->yAxis)),
             "a log value pan is a translation");
    plot->yAxis->setRange(20, 1000); // the ratio changed: a zoom
    plot->replot();
    QVERIFY2(!logValue.isTranslationOf(QCPTransform(plot->xAxis, plot->yAxis)),
             "a log value zoom is not a translation");
}

void TestCoords::positionSetMatchesSetters()
{
    auto plot = layoutPlot();
    auto item = plot->add<QCPItemLine>();
    auto reference = plot->add<QCPItemLine>();
    QCPItemPosition* position = item->start;
    QCPItemPosition* expected = reference->start;

    auto sameInitialState = [&]
    {
        position->setParentAnchor(nullptr, false);
        expected->setParentAnchor(nullptr, false);
        position->setTypeX(QCPItemPosition::ptAbsolute);
        position->setTypeY(QCPItemPosition::ptAbsolute);
        expected->setTypeX(QCPItemPosition::ptAbsolute);
        expected->setTypeY(QCPItemPosition::ptAbsolute);
    };
    sameInitialState();
    position->setAxes(plot->xAxis, plot->yAxis);
    position->setAxisRect(plot->axisRect());
    expected->setAxes(plot->xAxis, plot->yAxis);
    expected->setAxisRect(plot->axisRect());
    plot->xAxis->setRange(-10, 10);
    plot->yAxis->setRange(-5, 5);
    plot->replot();

    auto samePixels = [&](const char* what)
    {
        const bool same = position->pixelPosition() == expected->pixelPosition();
        if (!same)
            qWarning("%s: set() gave %f,%f, the 2.x setters gave %f,%f", what,
                     position->pixelPosition().x(), position->pixelPosition().y(),
                     expected->pixelPosition().x(), expected->pixelPosition().y());
        return same;
    };

    sameInitialState();
    position->set(QCP::Data { 3.5, -2.25 });
    expected->setType(QCPItemPosition::ptPlotCoords);
    expected->setCoords(3.5, -2.25);
    QVERIFY2(samePixels("Data"), "Data must equal ptPlotCoords + coords");

    sameInitialState();
    position->set(QCP::Pixels { 120.0, 90.0 });
    expected->setType(QCPItemPosition::ptAbsolute);
    expected->setCoords(120.0, 90.0);
    QVERIFY2(samePixels("Pixels"), "Pixels must equal ptAbsolute + coords");

    sameInitialState();
    position->set(QCP::AxisRatio { 0.25, 0.75 });
    expected->setType(QCPItemPosition::ptAxisRectRatio);
    expected->setCoords(0.25, 0.75);
    QVERIFY2(samePixels("AxisRatio"), "AxisRatio must equal ptAxisRectRatio + coords");

    sameInitialState();
    position->set(QCP::DataXRatioY { 4.0, 0.25 });
    expected->setTypeX(QCPItemPosition::ptPlotCoords);
    expected->setTypeY(QCPItemPosition::ptAxisRectRatio);
    expected->setCoords(4.0, 0.25);
    QVERIFY2(samePixels("DataXRatioY"), "DataXRatioY must mix plot coords and a rect ratio");

    sameInitialState();
    position->set(QCP::RatioXDataY { 0.1, -3.0 });
    expected->setTypeX(QCPItemPosition::ptAxisRectRatio);
    expected->setTypeY(QCPItemPosition::ptPlotCoords);
    expected->setCoords(0.1, -3.0);
    QVERIFY2(samePixels("RatioXDataY"), "RatioXDataY must mix a rect ratio and plot coords");

    sameInitialState();
    position->set(QCP::AnchorOffset { item->end, QCP::PixelOffset { 12.0, -7.0 } });
    expected->setParentAnchor(reference->end, false);
    expected->setCoords(12.0, -7.0);
    QVERIFY2(samePixels("AnchorOffset"), "AnchorOffset must equal a parent anchor and an offset");

    // asPosition() reads the same spaces back.
    position->set(QCP::Data { -1.5, 2.5 });
    QCP::Position read = position->asPosition();
    const auto data = std::get_if<QCP::Data>(&read);
    QVERIFY(data);
    QCOMPARE(data->x, -1.5);
    QCOMPARE(data->y, 2.5);

    position->set(QCP::AxisRatio { 0.5, 0.25 });
    read = position->asPosition();
    const auto ratio = std::get_if<QCP::AxisRatio>(&read);
    QVERIFY(ratio);
    QCOMPARE(ratio->x, 0.5);
    QCOMPARE(ratio->y, 0.25);

    position->set(QCP::DataXRatioY { 6.0, 0.4 });
    read = position->asPosition();
    const auto mixed = std::get_if<QCP::DataXRatioY>(&read);
    QVERIFY(mixed);
    QCOMPARE(mixed->x, 6.0);
    QCOMPARE(mixed->y, 0.4);

    position->set(QCP::Pixels { 33.0, 44.0 });
    read = position->asPosition();
    const auto pixels = std::get_if<QCP::Pixels>(&read);
    QVERIFY(pixels);
    QCOMPARE(pixels->x, 33.0);
    QCOMPARE(pixels->y, 44.0);

    position->set(QCP::AnchorOffset { item->end, QCP::PixelOffset { 5.0, 6.0 } });
    read = position->asPosition();
    const auto anchored = std::get_if<QCP::AnchorOffset>(&read);
    QVERIFY(anchored);
    QCOMPARE(anchored->anchor, item->end);
    QCOMPARE(anchored->offset.dx, 5.0);
    QCOMPARE(anchored->offset.dy, 6.0);
}

void TestCoords::refusedAnchorLeavesPositionUntouched()
{
    auto plot = layoutPlot();
    auto a = plot->add<QCPItemLine>();
    auto b = plot->add<QCPItemLine>();
    // b->end follows a->start, so pointing a->start at b->end would close a cycle.
    QVERIFY(b->end->setParentAnchor(a->start));
    a->start->setType(QCPItemPosition::ptPlotCoords);
    a->start->setCoords(3.0, 4.0);

    a->start->set(QCP::AnchorOffset { b->end, QCP::PixelOffset { 5.0, 6.0 } });

    QCOMPARE(a->start->typeX(), QCPItemPosition::ptPlotCoords);
    QCOMPARE(a->start->typeY(), QCPItemPosition::ptPlotCoords);
    QCOMPARE(a->start->coords().x(), 3.0);
    QCOMPARE(a->start->coords().y(), 4.0);
    QVERIFY(!a->start->parentAnchorX());
    QVERIFY(!a->start->parentAnchorY());
}

void TestCoords::ratioTransformMatchesPixels()
{
    auto plot = layoutPlot();
    const QCPRatioTransform transform(plot->axisRect());
    QVERIFY(transform.valid());
    const QCP::Pixels centre = transform.map(QCP::AxisRatio { 0.5, 0.5 });
    QVERIFY(nearlyEqual(centre.x, plot->axisRect()->left() + plot->axisRect()->width() * 0.5));
    QVERIFY(nearlyEqual(centre.y, plot->axisRect()->top() + plot->axisRect()->height() * 0.5));

    // 2.x agreement: an item position of type ptAxisRectRatio lands on the same pixels.
    auto item = plot->add<QCPItemText>();
    item->position->setAxisRect(plot->axisRect());
    item->position->setType(QCPItemPosition::ptAxisRectRatio);
    item->position->setCoords(0.5, 0.5);
    QCOMPARE(item->position->pixelPosition(), QPointF(centre.x, centre.y));
}

void TestCoords::mixingSpacesDoesNotCompile()
{
    // A requires-expression over invented parameters is a hard error, not false, so the absence of
    // an operator is asserted through a concept whose parameters are dependent.
    static_assert(!Addable<QCP::Data, QCP::Pixels>,
                  "a data point and a pixel point are different spaces: no operator may join them");
    static_assert(!Addable<QCP::Pixels, QCP::Data>);
    static_assert(Addable<QCP::Pixels, QCP::PixelOffset>);
    static_assert(Addable<QCPItemAnchor*, QCP::PixelOffset>);
    // A pixel offset is the one thing that joins a space: it rides on a data point instead of
    // mapping it, which is what DataOffset is. Two points of one space still do not add up, and
    // only pixels subtract into the offset between them.
    static_assert(Addable<QCP::Data, QCP::PixelOffset>);
    static_assert(Addable<QCP::DataOffset, QCP::PixelOffset>);
    static_assert(!Addable<QCP::Pixels, QCP::Pixels>);
    static_assert(!Addable<QCP::DataOffset, QCP::Data>);
    static_assert(Subtractable<QCP::Pixels, QCP::Pixels>);
    static_assert(!Subtractable<QCP::Data, QCP::Data>);
    QVERIFY(true);
}

void TestCoords::anchorOffsetIsPixelsWhateverTheOldType()
{
    // The offset of an anchored position is in pixels, even when the position used to be a ratio
    // of the axis rect: otherwise (5, 6) would mean five axis-rect widths.
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.replot();
    auto anchorItem = plot.add<QCPItemLine>();
    anchorItem->start->setCoords(2, 3);
    auto item = plot.add<QCPItemLine>();
    item->start->setType(QCPItemPosition::ptAxisRectRatio);
    item->start->setCoords(0.5, 0.5);

    item->start->set(QCP::AnchorOffset { anchorItem->start, { 5, 6 } });
    const QPointF expected = anchorItem->start->pixelPosition() + QPointF(5, 6);
    QCOMPARE(item->start->pixelPosition(), expected);
    QCOMPARE(item->start->typeX(), QCPItemPosition::ptAbsolute);
    QCOMPARE(item->start->typeY(), QCPItemPosition::ptAbsolute);
}

void TestCoords::pixelLengthLiteralIsAPixelLength()
{
    static_assert(std::is_same_v<decltype(5_px), QCP::PixelLength>,
                  "the literal must produce the unit type, not a bare double");
    static_assert(std::is_same_v<decltype(2.5_px), QCP::PixelLength>);
    QCOMPARE((5_px).px, 5.0);
    QCOMPARE((2.5_px).px, 2.5);
    // A default-constructed length is zero, so a width left out draws nothing rather than anything.
    QCOMPARE(QCP::PixelLength {}.px, 0.0);
}

void TestCoords::offsetsAddUp()
{
    const QCP::PixelOffset delta = QCP::Pixels { 10, 20 } - QCP::Pixels { 4, 5 };
    QCOMPARE(delta.dx, 6.0);
    QCOMPARE(delta.dy, 15.0);

    const QCP::DataOffset once = QCP::Data { 1, 2 } + QCP::PixelOffset { 3, 4 };
    QCOMPARE(once.at.x, 1.0);
    QCOMPARE(once.at.y, 2.0);
    QCOMPARE(once.offset.dx, 3.0);
    QCOMPARE(once.offset.dy, 4.0);

    const QCP::DataOffset twice = once + QCP::PixelOffset { 1, 1 };
    QCOMPARE(twice.at.x, 1.0);
    QCOMPARE(twice.at.y, 2.0);
    QCOMPARE(twice.offset.dx, 4.0);
    QCOMPARE(twice.offset.dy, 5.0);

    // It is a position alternative, so a command can carry it like any other.
    const QCP::Position position = twice;
    QVERIFY(std::holds_alternative<QCP::DataOffset>(position));
}

void TestCoords::dataOffsetRecordingFollowsTheData()
{
    auto plot = canvasPlot();
    QCPDrawContext context(plot->xAxis, plot->yAxis, plot->axisRect(), nullptr);

    const QCP::Position topLeft = QCP::Data { 50, 100 } + QCP::PixelOffset { -20, 10 };
    const QCP::Position bottomRight = QCP::Data { 80, 120 } + QCP::PixelOffset { 10, -5 };
    context.rect(topLeft, bottomRight, Qt::NoPen, QBrush(Qt::red));

    const QCPDrawList& list = context.drawList();
    QCOMPARE(list.commands.size(), std::size_t(1));
    const qcp::CommandKind kind = qcp::commandKind(list.commands.front());
    QVERIFY2(kind.dataOnly, "a DataOffset recording moves with the data: a pan may reuse it as is");
    QVERIFY2(!kind.staticOnly, "the offset is pixels, but the added data point still moves");

    // The resolution the layout promises: the data point's pixel, plus the offset.
    const QPointF resolvedTopLeft = qcpResolvePosition(context.frame(), topLeft);
    QVERIFY(nearlyEqual(resolvedTopLeft.x(), pixelsOf(context.transform(), 50, 100).x() - 20));
    QVERIFY(nearlyEqual(resolvedTopLeft.y(), pixelsOf(context.transform(), 50, 100).y() + 10));

    // Inside the offset box only: without the offset this pixel would be outside it.
    const QImage recorded = replayed(list, context.frame());
    QCOMPARE(recorded.pixelColor(35, 55), QColor(Qt::red));
    QCOMPARE(recorded.pixelColor(5, 5), QColor(Qt::white));

    // A pan of ten keys to the left moves the box with it, and the offset stays what it was.
    plot->xAxis->setRange(10, 210);
    plot->replot();
    const QCPDrawFrame panned = frameOf(*plot);
    const QPointF pannedTopLeft = qcpResolvePosition(panned, topLeft);
    QCOMPARE(pannedTopLeft - resolvedTopLeft, QPointF(-10, 0));

    const QImage afterPan = replayed(list, panned);
    QCOMPARE(afterPan.pixelColor(30, 56), QColor(Qt::red));
    QCOMPARE(afterPan.pixelColor(86, 55), QColor(Qt::white));
}

void TestCoords::dataOffsetItemPositionFollowsTheData()
{
    auto plot = layoutPlot();
    auto item = plot->add<QCPItemLine>();
    QCPItemPosition* position = item->start;
    position->setAxisRect(plot->axisRect());
    position->setAxes(plot->xAxis, plot->yAxis);
    plot->xAxis->setRange(0, 200);
    plot->yAxis->setRange(0, 150);
    plot->replot();

    const QCP::DataOffset at = QCP::Data { 40, 60 } + QCP::PixelOffset { 7, -9 };
    position->set(at);
    auto expectedPixel
        = [&] { return pixelsOf(QCPTransform(plot->xAxis, plot->yAxis), 40, 60) + QPointF(7, -9); };
    QCOMPARE(position->pixelPosition(), expectedPixel());

    const QCP::Position back = position->asPosition();
    QVERIFY2(std::holds_alternative<QCP::DataOffset>(back),
             "the item must hand the recording a DataOffset, or the annotation freezes on a pan");
    const QCP::DataOffset roundTrip = std::get<QCP::DataOffset>(back);
    QCOMPARE(roundTrip.at.x, 40.0);
    QCOMPARE(roundTrip.at.y, 60.0);
    QCOMPARE(roundTrip.offset.dx, 7.0);
    QCOMPARE(roundTrip.offset.dy, -9.0);

    const QPointF beforePan = position->pixelPosition();
    const QPointF dataBeforePan = pixelsOf(QCPTransform(plot->xAxis, plot->yAxis), 40, 60);
    plot->xAxis->setRange(20, 220);
    plot->replot();
    const QPointF dataAfterPan = pixelsOf(QCPTransform(plot->xAxis, plot->yAxis), 40, 60);
    QVERIFY2(dataAfterPan != dataBeforePan, "the pan must move where the data point lands");
    // The position moved by exactly what the data moved by: the offset it carries did not change.
    QCOMPARE(position->pixelPosition() - beforePan, dataAfterPan - dataBeforePan);
    QCOMPARE(position->pixelPosition(), expectedPixel());

    // The plain setters speak for the data point alone: the next position is a plain Data.
    position->setCoords(10, 10);
    QVERIFY(std::holds_alternative<QCP::Data>(position->asPosition()));

    // setType drops the offset: it was written in the space the type leaves.
    position->set(QCP::Data { 40, 60 } + QCP::PixelOffset { 7, -9 });
    position->setType(QCPItemPosition::ptAxisRectRatio);
    QVERIFY2(std::holds_alternative<QCP::AxisRatio>(position->asPosition()),
             "a type change drops the offset with the space it was written in");

    // A pixel write names the pixel the offset was part of, so it drops the offset too.
    position->set(QCP::Data { 40, 60 } + QCP::PixelOffset { 7, -9 });
    position->setPixelPosition(position->pixelPosition());
    QVERIFY2(std::holds_alternative<QCP::Data>(position->asPosition()),
             "a pixel write drops the offset: it named the pixel, not the data point");

    // An anchored position is the anchor's pixel: no offset rides on it.
    position->set(QCP::Data { 40, 60 } + QCP::PixelOffset { 7, -9 });
    position->setParentAnchor(item->end);
    QVERIFY2(!std::holds_alternative<QCP::DataOffset>(position->asPosition()),
             "an anchored position cannot carry the offset");
}

void TestCoords::dataOffsetSurvivesAwkwardAxes()
{
    // Log, reversed and vertical-key axes change the mapping; the offset is pixels whatever the map
    // does, so the resolution is the plain Data resolution plus the offset.
    for (const ScaleCase& testCase : kCases)
    {
        auto plot = layoutPlot();
        QCPAxis* keyAxis = plot->xAxis;
        QCPAxis* valueAxis = plot->yAxis;
        if (testCase.verticalKey)
        {
            keyAxis = plot->axisRect()->addAxis(QCPAxis::atLeft);
            valueAxis = plot->axisRect()->addAxis(QCPAxis::atBottom);
        }
        configureLogAxes(keyAxis, valueAxis, testCase);
        plot->replot();

        const QCPDrawFrame frame { QCPTransform(keyAxis, valueAxis),
                                   QCPRatioTransform(plot->axisRect()),
                                   QRectF(plot->axisRect()->rect()) };
        const QCP::PixelOffset offset { 5, -7 };
        for (const double sample : { 1.0, 10.0, 100.0, 1000.0 })
        {
            const QPointF plain = qcpResolvePosition(frame, QCP::Data { sample, sample });
            const QPointF shifted
                = qcpResolvePosition(frame, QCP::Data { sample, sample } + offset);
            QVERIFY2(nearlyEqual(shifted.x(), plain.x() + offset.dx), testCase.name);
            QVERIFY2(nearlyEqual(shifted.y(), plain.y() + offset.dy), testCase.name);
            QVERIFY2(std::isfinite(shifted.x()) && std::isfinite(shifted.y()),
                     "a point inside the range stays finite");
        }
    }

    // A polar area maps through its system's transform, the offset last.
    {
        auto plot = std::make_unique<QCustomPlot>();
        plot->resize(640, 480);
        plot->plotLayout()->clear();
        auto* area = new QCPPolarArea(plot.get());
        plot->plotLayout()->addElement(0, 0, area);
        area->angularAxis()->setRange(0, 360);
        area->radialAxis()->setRange(0, 1);
        plot->replot();

        QCPCoordinateSystem* system = area->system(area->angularAxis(), area->radialAxis());
        const QCP::DataOffset at = QCP::Data { 45, 0.4 } + QCP::PixelOffset { 4, 6 };
        auto item = plot->add<QCPItemLine>();
        item->start->setSystem(system);
        item->start->set(at);
        const QPointF expected = pixelsOf(system->transform(), 45, 0.4) + QPointF(4, 6);
        QVERIFY2(nearlyEqual(item->start->pixelPosition().x(), expected.x()), "polar key");
        QVERIFY2(nearlyEqual(item->start->pixelPosition().y(), expected.y()), "polar value");

        const QCPDrawFrame frame { system->transform(), QCPRatioTransform { },
                                   QRectF(area->rect()) };
        const QPointF resolved = qcpResolvePosition(frame, at);
        QVERIFY(nearlyEqual(resolved.x(), expected.x()));
        QVERIFY(nearlyEqual(resolved.y(), expected.y()));
    }

    // A data point that is not a number stays not a number: the offset cannot rescue it, so no
    // point at infinity is drawn, exactly as for a NaN Data.
    {
        auto plot = canvasPlot();
        QCPDrawContext context(plot->xAxis, plot->yAxis, plot->axisRect(), nullptr);
        context.rect(QCP::Data { qQNaN(), 100 }, QCP::Data { 80, 120 }, Qt::NoPen, QBrush(Qt::red));
        const QPointF nowhere = qcpResolvePosition(
            context.frame(), QCP::Data { qQNaN(), 100 } + QCP::PixelOffset { 3, 3 });
        QVERIFY(qIsNaN(nowhere.x()));
        QVERIFY(!std::isinf(nowhere.x()) && !std::isinf(nowhere.y()));

        QCPDrawContext shifted(plot->xAxis, plot->yAxis, plot->axisRect(), nullptr);
        shifted.rect(QCP::Data { qQNaN(), 100 } + QCP::PixelOffset { 3, 3 }, QCP::Data { 80, 120 },
                     Qt::NoPen, QBrush(Qt::red));
        // The NaN corner keeps the whole box undrawable, as a NaN Data corner does today.
        const QImage nanData = replayed(context.drawList(), context.frame());
        QCOMPARE(nanData, replayed(shifted.drawList(), shifted.frame()));
        QCOMPARE(nanData.pixelColor(40, 40), QColor(Qt::white));
    }
}

void TestCoords::offsetOnlyChangeRepaintsTheItem()
{
    // The offset is part of what the item draws, so a change to it alone must re-record the item.
    // The coordinates cannot say so: they did not move.
    auto plot = canvasPlot();
    auto item = plot->add<QCPItemRect>();
    item->topLeft->setAxes(plot->xAxis, plot->yAxis);
    item->bottomRight->setAxes(plot->xAxis, plot->yAxis);
    item->topLeft->set(QCP::Data { 40, 60 } + QCP::PixelOffset { 5, 5 });
    item->bottomRight->set(QCP::Data { 80, 100 } + QCP::PixelOffset { 5, 5 });
    const auto painted = [&plot] { return plot->toPixmap(kCanvasWidth, kCanvasHeight).toImage(); };
    plot->replot();
    const QImage nudged = painted();

    item->topLeft->set(QCP::Data { 40, 60 } + QCP::PixelOffset { 30, 30 });
    plot->replot();
    QVERIFY2(painted() != nudged, "a second offset on the same data point must repaint the item");

    // A setter that clears the offset while finding the coordinates unchanged repaints too.
    const QImage withOffset = painted();
    item->topLeft->setCoords(40, 60);
    plot->replot();
    QVERIFY2(painted() != withOffset, "clearing the offset must repaint the item");

    // And so does a pixel write that lands where the offset put the position.
    item->topLeft->set(QCP::Data { 40, 60 } + QCP::PixelOffset { 30, 30 });
    plot->replot();
    const QImage beforeThePixelWrite = painted();
    item->topLeft->setPixelPosition(pixelsOf(QCPTransform(plot->xAxis, plot->yAxis), 40, 60));
    plot->replot();
    QVERIFY2(painted() != beforeThePixelWrite, "a pixel write that drops the offset must repaint");
}

void TestCoords::mixedImageCornersAreClassifiedOnce()
{
    // An image with one plain data corner and one riding on the data moves with the data: both
    // replays must read that the same way, or the image is drawn twice or never (B1).
    auto plot = canvasPlot();
    QCPDrawContext context(plot->xAxis, plot->yAxis, plot->axisRect(), nullptr);
    QImage image(4, 4, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    context.image(QCP::Data { 20, 20 }, QCP::Data { 60, 60 } + QCP::PixelOffset { 5, 5 }, image);

    const qcp::QCPDrawCommand& command = context.drawList().commands.front();
    const qcp::CommandKind kind = qcp::commandKind(command);
    QVERIFY2(kind.dataOnly, "a corner that rides on the data moves with it");
    QVERIFY(!kind.staticOnly);
    QVERIFY2(kind.gpu == qcp::isGpuCommand(command),
             "the GPU and the painter pass must agree on who draws the image");
    QVERIFY2(kind.gpu, "an image of data corners belongs to the GPU pass, once");
}

void TestCoords::aPointThatMapsNowhereLandsAtTheOrigin()
{
    // A NaN point maps nowhere, and the offset is a displacement of that point: it cannot rescue
    // it. Both pixelPosition() paths clamp the result to the origin, so a hit test or an anchor
    // never sees (dx, dy) where the drawing has nothing. (A log axis is not this case: it puts a
    // point of the range's opposite sign 200 pixels outside the rect, as QCPAxis::coordToPixel
    // does, and that point keeps its offset.)
    auto plot = layoutPlot();
    auto item = plot->add<QCPItemLine>();
    item->start->setAxisRect(plot->axisRect());
    item->start->setAxes(plot->xAxis, plot->yAxis);
    plot->replot();
    item->start->set(QCP::Data { qQNaN(), qQNaN() } + QCP::PixelOffset { 7, 9 });
    QCOMPARE(item->start->pixelPosition(), QPointF(0, 0));

    // The frame-resolved path a polar position takes agrees.
    auto polar = std::make_unique<QCustomPlot>();
    polar->resize(640, 480);
    polar->plotLayout()->clear();
    auto* area = new QCPPolarArea(polar.get());
    polar->plotLayout()->addElement(0, 0, area);
    polar->replot();
    QCPCoordinateSystem* system = area->system(area->angularAxis(), area->radialAxis());
    auto polarItem = polar->add<QCPItemLine>();
    polarItem->start->setSystem(system);
    polarItem->start->set(QCP::Data { qQNaN(), qQNaN() } + QCP::PixelOffset { 7, 9 });
    QCOMPARE(polarItem->start->pixelPosition(), QPointF(0, 0));

    // A log axis puts a non-positive point 200 pixels outside the rect and keeps the offset: the
    // pixel is finite, and the offset is a displacement of it like any other.
    plot->xAxis->setScale(QCPScale::log());
    plot->yAxis->setScale(QCPScale::log());
    plot->xAxis->setRange(1, 1000);
    plot->yAxis->setRange(1, 1000);
    plot->replot();
    item->start->set(QCP::Data { -5, -10 });
    const QPointF offRect = item->start->pixelPosition();
    item->start->set(QCP::Data { -5, -10 } + QCP::PixelOffset { 7, 9 });
    QVERIFY2(std::isfinite(offRect.x()) && std::isfinite(offRect.y()),
             "a log axis maps a non-positive point off the rect, not to NaN");
    QCOMPARE(item->start->pixelPosition(), offRect + QPointF(7, 9));
}
