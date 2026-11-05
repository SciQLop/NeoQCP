#include "test-draw-context.h"

#include <painting/command-replay-rhi.h>
#include <painting/draw-commands.h>
#include <painting/draw-context.h>
#include <painting/line-extruder.h>
#include <painting/plottable-rhi-layer.h>
#include <painting/raster-fallback.h>
#include <painting/scatter-rhi-layer.h>
#include <qcustomplot.h>

#include <QFontMetrics>
#include <QPainterPath>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

namespace
{
constexpr int kWidth = 200;
constexpr int kHeight = 150;

// The axis rect covers the whole image and the value axis is not reversed, so data (x, y) lands at
// pixel (x, kHeight - y). The tests assert that once (polylineMatchesHandPainting) and then rely on
// it: every hand-painted shape is placed from the same transform the replay uses.
struct Fixture
{
    Fixture()
    {
        plot = std::make_unique<QCustomPlot>();
        plot->resize(kWidth, kHeight);
        plot->axisRect()->setAutoMargins(QCP::msNone);
        plot->axisRect()->setMargins(QMargins(0, 0, 0, 0));
        plot->xAxis->setRange(0, kWidth);
        plot->yAxis->setRange(0, kHeight);
        plot->replot();
    }

    std::unique_ptr<QCustomPlot> plot;
};

using Paint = std::function<void(QCPPainter&, const QCPTransform&)>;

QImage painted(const Paint& paint, const QCPTransform& transform)
{
    QImage image(kWidth, kHeight, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    {
        QCPPainter painter(&image);
        painter.setAntialiasing(true);
        paint(painter, transform);
    }
    return image;
}

QPointF pixelOf(const QCPTransform& transform, double x, double y)
{
    const QCP::Pixels pixels = transform.map(QCP::Data { x, y });
    return QPointF(pixels.x, pixels.y);
}

QVector<QPointF> pixelsOf(const QCPTransform& transform, const std::vector<QPointF>& points)
{
    QVector<QPointF> pixels;
    pixels.reserve(int(points.size()));
    for (const QPointF& point : points)
        pixels.append(pixelOf(transform, point.x(), point.y()));
    return pixels;
}

QImage replayOf(QCPDrawContext& context)
{
    return painted([&context](QCPPainter& painter, const QCPTransform&)
                   { qcpReplayWithPainter(context.drawList(), painter, context.frame()); },
                   context.transform());
}

std::shared_ptr<const std::vector<QPointF>> sharedPoints(std::vector<QPointF> points)
{
    return std::make_shared<const std::vector<QPointF>>(std::move(points));
}

const QPen kPen(QColor(20, 40, 160), 3.0);
const QBrush kBrush(QColor(200, 90, 30));
} // namespace

// A static_assert, as the brief asks: a command is one cache line and a half at most.
static_assert(sizeof(qcp::PolylineDataCmd) <= 96);
static_assert(sizeof(qcp::PolylinePositionsCmd) <= 96);
static_assert(sizeof(qcp::PolygonCmd) <= 96);
static_assert(sizeof(qcp::FillBetweenCmd) <= 96);
static_assert(sizeof(qcp::RectCmd) <= 96);
static_assert(sizeof(qcp::EllipseCmd) <= 96);
static_assert(sizeof(qcp::MarkersCmd) <= 96);
static_assert(sizeof(qcp::ErrorBarsCmd) <= 96);
static_assert(sizeof(qcp::TextCmd) <= 96);
static_assert(sizeof(qcp::PathCmd) <= 96);
static_assert(sizeof(qcp::ImageCmd) <= 96);
static_assert(sizeof(qcp::PainterCmd) <= 96);
static_assert(sizeof(qcp::QCPDrawCommand) <= 104);

void TestDrawContext::drawListIsCompact()
{
    // The static_asserts above are the contract; this keeps the slot visible in the report.
    QVERIFY(sizeof(qcp::QCPDrawCommand) <= 104);
}

void TestDrawContext::polylineMatchesHandPainting()
{
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    // The mapping the rest of the tests rely on: x from the left, y up from the bottom. The axis
    // rect is the widget minus rounding, so the value axis is exact to a pixel only.
    const QPointF mapped = pixelOf(context.transform(), 20, 30);
    QCOMPARE(mapped.x(), 20.0);
    QVERIFY2(std::abs(mapped.y() - 120.0) <= 1.0, qPrintable(QString::number(mapped.y())));

    const std::vector<QPointF> points {
        { 10, 10 }, { 40, 90 }, { 80, 30 }, { 120, 120 }, { 170, 60 }
    };
    context.polyline(sharedPoints(points), kPen);

    const QImage replay = replayOf(context);
    const QImage hand = painted(
        [&points](QCPPainter& painter, const QCPTransform& transform)
        {
            painter.setPen(kPen);
            painter.drawPolyline(pixelsOf(transform, points));
        },
        context.transform());
    QCOMPARE(replay, hand);
}

void TestDrawContext::polylinePositionsMatchesHandPainting()
{
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    auto* item = new QCPItemLine(fixture.plot.get());
    item->start->setPixelPosition(QPointF(150, 40));

    const std::vector<QCP::Position> positions {
        QCP::Data { 10, 10 }, QCP::Pixels { 40, 60 }, QCP::AxisRatio { 0.5, 0.4 },
        QCP::AnchorOffset { item->start, QCP::PixelOffset { 5, 7 } }
    };
    context.polyline(std::span<const QCP::Position>(positions), kPen);

    const QRectF rect = fixture.plot->axisRect()->rect();
    const QImage replay = replayOf(context);
    const QImage hand = painted(
        [&positions, &rect](QCPPainter& painter, const QCPTransform& transform)
        {
            painter.setPen(kPen);
            QVector<QPointF> pixels;
            for (const QCP::Position& position : positions)
            {
                if (const auto* data = std::get_if<QCP::Data>(&position))
                    pixels.append(pixelOf(transform, data->x, data->y));
                else if (const auto* absolute = std::get_if<QCP::Pixels>(&position))
                    pixels.append(QPointF(absolute->x, absolute->y));
                else if (const auto* ratio = std::get_if<QCP::AxisRatio>(&position))
                    pixels.append(QPointF(rect.left() + ratio->x * rect.width(),
                                          rect.top() + ratio->y * rect.height()));
                else
                {
                    const auto& anchored = std::get<QCP::AnchorOffset>(position);
                    pixels.append(anchored.anchor->pixelPosition()
                                  + QPointF(anchored.offset.dx, anchored.offset.dy));
                }
            }
            painter.drawPolyline(pixels);
        },
        context.transform());
    QCOMPARE(replay, hand);
}

void TestDrawContext::polygonMatchesHandPainting()
{
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    const std::vector<QCP::Position> positions { QCP::Data { 20, 20 }, QCP::Data { 120, 30 },
                                                 QCP::Data { 90, 110 }, QCP::Data { 30, 90 } };
    context.polygon(std::span<const QCP::Position>(positions), kPen, kBrush);

    const QImage replay = replayOf(context);
    const QImage hand = painted(
        [&positions](QCPPainter& painter, const QCPTransform& transform)
        {
            painter.setPen(kPen);
            painter.setBrush(kBrush);
            QPolygonF polygon;
            for (const QCP::Position& position : positions)
            {
                const auto& data = std::get<QCP::Data>(position);
                polygon.append(pixelOf(transform, data.x, data.y));
            }
            painter.drawPolygon(polygon);
        },
        context.transform());
    QCOMPARE(replay, hand);
}

void TestDrawContext::fillBetweenMatchesHandPainting()
{
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    const std::vector<QPointF> upper { { 10, 110 }, { 60, 130 }, { 120, 100 }, { 180, 120 } };
    const std::vector<QPointF> lower { { 10, 40 }, { 60, 20 }, { 120, 50 }, { 180, 30 } };
    context.fillBetween(sharedPoints(upper), sharedPoints(lower), kBrush);

    const QImage replay = replayOf(context);
    const QImage hand = painted(
        [&upper, &lower](QCPPainter& painter, const QCPTransform& transform)
        {
            QPolygonF polygon = pixelsOf(transform, upper);
            for (int i = int(lower.size()) - 1; i >= 0; --i)
                polygon.append(pixelOf(transform, lower[i].x(), lower[i].y()));
            painter.setPen(Qt::NoPen);
            painter.setBrush(kBrush);
            painter.drawPolygon(polygon);
        },
        context.transform());
    QCOMPARE(replay, hand);
}

void TestDrawContext::rectMatchesHandPainting()
{
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    // Two corners in two spaces: one in data, one in pixels.
    context.rect(QCP::Data { 20, 30 }, QCP::Pixels { 150, 110 }, kPen, kBrush);

    const QImage replay = replayOf(context);
    const QImage hand = painted(
        [](QCPPainter& painter, const QCPTransform& transform)
        {
            const QRectF rect = QRectF(pixelOf(transform, 20, 30), QPointF(150, 110)).normalized();
            painter.setPen(kPen);
            painter.setBrush(kBrush);
            painter.drawRect(rect);
        },
        context.transform());
    QCOMPARE(replay, hand);
}

void TestDrawContext::ellipseMatchesHandPainting()
{
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    context.ellipse(QCP::Data { 20, 30 }, QCP::Data { 150, 120 }, kPen, kBrush);

    const QImage replay = replayOf(context);
    const QImage hand = painted(
        [](QCPPainter& painter, const QCPTransform& transform)
        {
            const QRectF rect
                = QRectF(pixelOf(transform, 20, 30), pixelOf(transform, 150, 120)).normalized();
            painter.setPen(kPen);
            painter.setBrush(kBrush);
            painter.drawEllipse(rect);
        },
        context.transform());
    QCOMPARE(replay, hand);
}

void TestDrawContext::markersMatchesHandPainting()
{
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    const std::vector<QPointF> points { { 20, 20 }, { 60, 90 }, { 110, 40 }, { 160, 120 } };
    const QCPScatterStyle style(QCPScatterStyle::ssDisc, kPen, kBrush, 9.0);
    context.markers(sharedPoints(points), style);

    const QImage replay = replayOf(context);
    const QImage hand = painted(
        [&points, &style](QCPPainter& painter, const QCPTransform& transform)
        {
            style.applyTo(&painter, QPen(Qt::black));
            for (const QPointF& point : points)
                style.drawShape(&painter, pixelOf(transform, point.x(), point.y()));
        },
        context.transform());
    QCOMPARE(replay, hand);
}

void TestDrawContext::errorBarsMatchesHandPainting()
{
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    const std::vector<QPointF> centres { { 30, 50 }, { 80, 70 }, { 140, 40 } };
    const std::vector<QPointF> spans { { 6, 20 },
                                       { 6, 12 },
                                       { 6, 25 } }; // half extents, data units
    context.errorBars(sharedPoints(centres), sharedPoints(spans), kPen);

    const QImage replay = replayOf(context);
    const QImage hand = painted(
        [&centres, &spans](QCPPainter& painter, const QCPTransform& transform)
        {
            painter.setPen(kPen);
            for (std::size_t i = 0; i < centres.size(); ++i)
            {
                const QPointF centre = pixelOf(transform, centres[i].x(), centres[i].y());
                const QPointF above
                    = pixelOf(transform, centres[i].x(), centres[i].y() + spans[i].y());
                const QPointF below
                    = pixelOf(transform, centres[i].x(), centres[i].y() - spans[i].y());
                const QPointF left
                    = pixelOf(transform, centres[i].x() - spans[i].x(), centres[i].y());
                const QPointF right
                    = pixelOf(transform, centres[i].x() + spans[i].x(), centres[i].y());
                painter.drawLine(above, below);
                painter.drawLine(QPointF(left.x(), above.y()), QPointF(right.x(), above.y()));
                painter.drawLine(QPointF(left.x(), below.y()), QPointF(right.x(), below.y()));
            }
        },
        context.transform());
    QCOMPARE(replay, hand);
}

void TestDrawContext::textMatchesHandPainting()
{
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    QCPTextStyle style;
    style.font = QFont(QStringLiteral("Sans"), 11);
    style.color = QColor(10, 60, 90);
    style.alignment = Qt::AlignLeft | Qt::AlignTop;
    const QString text = QStringLiteral("QCP");
    context.text(QCP::Data { 40, 100 }, text, style);

    const QImage replay = replayOf(context);
    const QImage hand = painted(
        [&text, &style](QCPPainter& painter, const QCPTransform& transform)
        {
            const int flags = int(style.alignment);
            painter.translate(pixelOf(transform, 40, 100));
            painter.setFont(style.font);
            painter.setPen(QPen(style.color));
            painter.drawText(QFontMetrics(style.font).boundingRect(0, 0, 0, 0, flags, text), flags,
                             text);
        },
        context.transform());
    QCOMPARE(replay, hand);
}

void TestDrawContext::pathMatchesHandPainting()
{
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    QPainterPath path;
    path.moveTo(0, 0);
    path.lineTo(30, -20);
    path.cubicTo(50, -40, 70, 10, 90, 0);
    context.path(QCP::Data { 20, 110 }, path, kPen, kBrush);

    const QImage replay = replayOf(context);
    const QImage hand = painted(
        [&path](QCPPainter& painter, const QCPTransform& transform)
        {
            painter.translate(pixelOf(transform, 20, 110));
            painter.setPen(kPen);
            painter.setBrush(kBrush);
            painter.drawPath(path);
        },
        context.transform());
    QCOMPARE(replay, hand);
}

void TestDrawContext::imageMatchesHandPainting()
{
    QImage image(8, 6, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(180, 40, 40));
    image.setPixel(2, 1, QColor(40, 180, 40).rgba());

    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    context.image(QCP::Data { 30, 110 }, QCP::Pixels { 150, 40 }, image);

    const QImage replay = replayOf(context);
    const QImage hand = painted(
        [&image](QCPPainter& painter, const QCPTransform& transform)
        {
            const QRectF rect = QRectF(pixelOf(transform, 30, 110), QPointF(150, 40)).normalized();
            painter.drawImage(rect, image);
        },
        context.transform());
    QCOMPARE(replay, hand);
}

void TestDrawContext::pathDoesNotShiftLaterCommands()
{
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    QPainterPath path;
    path.moveTo(0, 0);
    path.lineTo(40, -25);
    path.lineTo(80, 10);
    context.path(QCP::Data { 20, 110 }, path, kPen, kBrush);
    context.rect(QCP::Data { 120, 20 }, QCP::Data { 180, 60 }, kPen, kBrush);

    const QImage replay = replayOf(context);
    const QImage hand = painted(
        [&path](QCPPainter& painter, const QCPTransform& transform)
        {
            painter.save();
            painter.translate(pixelOf(transform, 20, 110));
            painter.setPen(kPen);
            painter.setBrush(kBrush);
            painter.drawPath(path);
            painter.restore();
            painter.setPen(kPen);
            painter.setBrush(kBrush);
            painter.drawRect(
                QRectF(pixelOf(transform, 120, 20), pixelOf(transform, 180, 60)).normalized());
        },
        context.transform());
    QCOMPARE(replay, hand);
}

void TestDrawContext::painterCallbackStateDoesNotLeak()
{
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    const auto callback = [](QCPPainter& painter, const QCPTransform& transform)
    {
        painter.setPen(QPen(Qt::red, 1.0));
        painter.translate(50, 0);
        painter.drawLine(pixelOf(transform, 0, 0), pixelOf(transform, 10, 10));
    };
    context.painter(callback);
    context.rect(QCP::Data { 120, 20 }, QCP::Data { 180, 60 }, kPen, kBrush);

    const QImage replay = replayOf(context);
    const QImage hand = painted(
        [&callback](QCPPainter& painter, const QCPTransform& transform)
        {
            painter.save();
            callback(painter, transform);
            painter.restore();
            painter.setPen(kPen);
            painter.setBrush(kBrush);
            painter.drawRect(
                QRectF(pixelOf(transform, 120, 20), pixelOf(transform, 180, 60)).normalized());
        },
        context.transform());
    QCOMPARE(replay, hand);
}

void TestDrawContext::painterMatchesHandPainting()
{
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    const auto callback = [](QCPPainter& painter, const QCPTransform& transform)
    {
        painter.setPen(kPen);
        painter.drawLine(pixelOf(transform, 10, 10), pixelOf(transform, 180, 130));
    };
    context.painter(callback);

    const QImage replay = replayOf(context);
    const QImage hand = painted(callback, context.transform());
    QCOMPARE(replay, hand);
}

void TestDrawContext::boundsCoverEveryCommand()
{
    Fixture fixture;
    const QPen pen(Qt::black);

    {
        QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                               nullptr);
        context.polyline(sharedPoints({ { -5, 20 }, { 30, 70 } }), pen);
        const QCPDataBox& bounds = context.drawList().bounds;
        QCOMPARE(bounds.keyLower, -5.0);
        QCOMPARE(bounds.keyUpper, 30.0);
        QCOMPARE(bounds.valueLower, 20.0);
        QCOMPARE(bounds.valueUpper, 70.0);
    }
    {
        // A pixel command leaves both sides unbounded, and it does not widen an earlier box either.
        QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                               nullptr);
        context.polyline(sharedPoints({ { 10, 20 }, { 30, 70 } }), pen);
        const std::vector<QCP::Position> absolute { QCP::Pixels { 1, 2 }, QCP::Pixels { 3, 4 } };
        context.polyline(std::span<const QCP::Position>(absolute), pen);
        const QCPDataBox& bounds = context.drawList().bounds;
        QVERIFY(!std::isfinite(bounds.keyLower));
        QVERIFY(!std::isfinite(bounds.keyUpper));
        QVERIFY(!std::isfinite(bounds.valueLower));
        QVERIFY(!std::isfinite(bounds.valueUpper));
    }
    {
        // Mixed spaces bound only the side that is data on every position.
        QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                               nullptr);
        const std::vector<QCP::Position> mixed { QCP::DataXRatioY { 50, 0.5 },
                                                 QCP::DataXRatioY { 70, 0.2 } };
        context.polyline(std::span<const QCP::Position>(mixed), pen);
        const QCPDataBox& bounds = context.drawList().bounds;
        QCOMPARE(bounds.keyLower, 50.0);
        QCOMPARE(bounds.keyUpper, 70.0);
        QVERIFY(!std::isfinite(bounds.valueLower));
        QVERIFY(!std::isfinite(bounds.valueUpper));
    }
    {
        QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                               nullptr);
        const std::vector<QCP::Position> mixed { QCP::RatioXDataY { 0.25, 60 } };
        context.polyline(std::span<const QCP::Position>(mixed), pen);
        const QCPDataBox& bounds = context.drawList().bounds;
        QVERIFY(!std::isfinite(bounds.keyLower));
        QVERIFY(!std::isfinite(bounds.keyUpper));
        QCOMPARE(bounds.valueLower, 60.0);
        QCOMPARE(bounds.valueUpper, 60.0);
    }
    {
        // Two disjoint commands: the box holds both, it is their union and not their overlap.
        QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                               nullptr);
        context.polyline(sharedPoints({ { 1, 1 }, { 2, 2 } }), pen);
        context.polyline(sharedPoints({ { 8, 8 }, { 9, 9 } }), pen);
        const QCPDataBox& bounds = context.drawList().bounds;
        QCOMPARE(bounds.keyLower, 1.0);
        QCOMPARE(bounds.keyUpper, 9.0);
        QCOMPARE(bounds.valueLower, 1.0);
        QCOMPARE(bounds.valueUpper, 9.0);
    }
    {
        // An empty recording is unbounded, not a box at the origin.
        QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                               nullptr);
        const QCPDataBox& bounds = context.drawList().bounds;
        QVERIFY(!std::isfinite(bounds.keyLower));
        QVERIFY(!std::isfinite(bounds.valueLower));
    }
}

void TestDrawContext::emptyCommandDoesNotUnboundTheRecording()
{
    // A command with no samples says nothing; it must not erase the box the others described.
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    const QPen pen(Qt::black);
    context.polyline(sharedPoints({ { 1, 2 }, { 3, 4 } }), pen);
    const auto nothing = std::make_shared<const std::vector<QPointF>>();
    context.markers(nothing, QCPScatterStyle(QCPScatterStyle::ssCircle));
    context.errorBars(nothing, nothing, pen);
    const QCPDrawList& list = context.drawList();
    QVERIFY(list.keyBounded);
    QVERIFY(list.valueBounded);
    QCOMPARE(list.bounds.keyLower, 1.0);
    QCOMPARE(list.bounds.keyUpper, 3.0);
    QCOMPARE(list.bounds.valueLower, 2.0);
    QCOMPARE(list.bounds.valueUpper, 4.0);
    QCOMPARE(int(list.commands.size()), 3);
}

void TestDrawContext::sharedDataIsNotCopied()
{
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    const auto points = sharedPoints({ { 1, 2 }, { 3, 4 } });
    const long before = points.use_count();
    context.polyline(points, QPen(Qt::black));
    QCOMPARE(points.use_count(), before + 1);

    const auto& command = std::get<qcp::PolylineDataCmd>(context.drawList().commands.front());
    QCOMPARE(command.points.get(), points.get());
}

// A span is a half extent, not a position: the corners are built in data space and mapped once, so
// a pan offset or a log axis cannot leak into the result.
void TestDrawContext::errorBarsMapTheirSpansAsExtents()
{
    Fixture fixture;
    fixture.plot->xAxis->setRange(10, 20);
    fixture.plot->yAxis->setRange(100, 200);
    fixture.plot->replot();
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);

    const auto centres = sharedPoints({ { 12.0, 120.0 } });
    const auto spans = sharedPoints({ { 1.0, 10.0 } });
    context.errorBars(centres, spans, kPen);

    std::vector<QPointF> bar;
    qcpErrorBarPixels(context.frame(), std::span<const QPointF>(*centres),
                      std::span<const QPointF>(*spans), bar);
    QCOMPARE(int(bar.size()), 9);

    const QCPTransform& transform = context.transform();
    const QPointF top = pixelOf(transform, 12.0, 130.0);
    const QPointF bottom = pixelOf(transform, 12.0, 110.0);
    QVERIFY2(std::abs(bar[0].y() - top.y()) < 1e-6,
             "the top cap must sit at the centre plus the span, mapped like a data point");
    QVERIFY2(std::abs(bar[0].x() - top.x()) < 1e-6, "the bar is vertical");
    QVERIFY2(std::abs(bar[1].y() - bottom.y()) < 1e-6, "the bottom cap mirrors the top one");
}

// A fan from the centre covers the whole ring, which the baseline tessellator did not.
void TestDrawContext::ellipseFanFillCoversTheDisc()
{
    const QPointF centre(100.0, 75.0);
    const double radius = 40.0;
    std::vector<QPointF> ring;
    for (int i = 0; i < 32; ++i)
    {
        const double angle = 2.0 * M_PI * i / 32.0;
        ring.emplace_back(centre.x() + radius * std::cos(angle),
                          centre.y() + radius * std::sin(angle));
    }
    const std::vector<float> verts
        = qcpFanFillVertices(centre, std::span<const QPointF>(ring), QColor(200, 90, 30));
    QCOMPARE(int(verts.size()), int(ring.size()) * 3 * 6);

    const auto inside = [&verts](const QPointF& p)
    {
        for (std::size_t v = 0; v + 17 < verts.size(); v += 18)
        {
            const QPointF a(verts[v], verts[v + 1]);
            const QPointF b(verts[v + 6], verts[v + 7]);
            const QPointF c(verts[v + 12], verts[v + 13]);
            const auto side = [](const QPointF& p0, const QPointF& p1, const QPointF& q)
            { return (p1.x() - p0.x()) * (q.y() - p0.y()) - (p1.y() - p0.y()) * (q.x() - p0.x()); };
            const double d1 = side(a, b, p);
            const double d2 = side(b, c, p);
            const double d3 = side(c, a, p);
            if ((d1 >= 0 && d2 >= 0 && d3 >= 0) || (d1 <= 0 && d2 <= 0 && d3 <= 0))
                return true;
        }
        return false;
    };
    for (int i = 0; i < 8; ++i)
    {
        const double angle = 2.0 * M_PI * i / 8.0;
        const QPointF sample(centre.x() + 0.8 * radius * std::cos(angle),
                             centre.y() + 0.8 * radius * std::sin(angle));
        QVERIFY2(inside(sample), "a point at 0.8 r must be inside the fan fill");
    }
}

// The painter replay can be asked for the half that belongs to it: on the screen the GPU kinds are
// drawn by the RHI pass, so painting them here too would double-draw them.
void TestDrawContext::mixedRecordingSplitsByKind()
{
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    context.polyline(sharedPoints({ { 10.0, 10.0 }, { 180.0, 140.0 } }), QPen(Qt::green, 5.0));
    context.text(QCP::Data { 100.0, 20.0 }, QStringLiteral("hi"), QCPTextStyle { });

    const QCPDrawFrame& frame = context.frame();
    const QImage all = painted(
        [&](QCPPainter& painter, const QCPTransform&)
        { qcpReplayWithPainter(context.drawList(), painter, frame, QCPReplayKinds::All); },
        context.transform());
    // PainterOnly needs the fallback that says which GPU kinds the layer refused: an empty one
    // means "none of them", so every GPU kind stays with the RHI pass.
    const qcp::QCPRasterFallback nothingRefused;
    const QImage painterOnly = painted(
        [&](QCPPainter& painter, const QCPTransform&)
        {
            qcpReplayWithPainter(context.drawList(), painter, frame, QCPReplayKinds::PainterOnly,
                                 &nothingRefused);
        },
        context.transform());

    const auto greenPixels = [](const QImage& image)
    {
        int count = 0;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
            {
                const QColor c = image.pixelColor(x, y);
                if (c.green() > 150 && c.red() < 100 && c.blue() < 100)
                    ++count;
            }
        return count;
    };
    QVERIFY2(greenPixels(all) > 100, "the all-kinds replay paints the polyline");
    QVERIFY2(greenPixels(painterOnly) == 0,
             "the painter-only replay leaves the polyline to the GPU");
    QVERIFY2(all != painterOnly, "the two kinds do different work");
}

// Colour runs share their end points, so the arc length has to carry over or every run restarts the
// dash pattern and the line looks striped at each colour change.
void TestDrawContext::extruderContinuesArcLengthAcrossRuns()
{
    const std::vector<QPointF> first { { 0, 0 }, { 10, 0 }, { 20, 0 } };
    const std::vector<QPointF> second { { 20, 0 }, { 30, 0 }, { 40, 0 } };
    std::vector<float> verts;
    const float afterFirst = QCPLineExtruder::appendPolylineWithDistance(
        std::span<const QPointF>(first), 2.0f, Qt::black, verts);
    QVERIFY2(std::abs(afterFirst - 20.0f) < 1e-4f, "the first run is 20 px long");
    const float afterSecond = QCPLineExtruder::appendPolylineWithDistance(
        std::span<const QPointF>(second), 2.0f, Qt::black, verts, afterFirst);
    QVERIFY2(std::abs(afterSecond - 40.0f) < 1e-4f,
             "the second run continues the arc length instead of restarting it");

    // A NaN gap restarts the pattern: the length reported is the last segment's, not the sum.
    const std::vector<QPointF> gapped {
        { 0, 0 }, { 50, 0 }, { qQNaN(), qQNaN() }, { 50, 0 }, { 60, 0 }
    };
    const float afterGap = QCPLineExtruder::appendPolylineWithDistance(
        std::span<const QPointF>(gapped), 2.0f, Qt::black, verts);
    QVERIFY2(std::abs(afterGap - 10.0f) < 1e-4f, "a NaN gap restarts the arc length at zero");
}

void TestDrawContext::extrusionCacheDropsOnADprChange()
{
    // A cosmetic pen's extrusion width is 1/dpr, so the triangles kept for one device pixel ratio
    // must not be reused at another: they would be twice too wide or too thin.
    Fixture fixture;
    QCPPlottableRhiLayer plottables(nullptr);
    QCPScatterRhiLayer scatter(nullptr);
    QCPResolveCache resolve;
    qcp::QCPRasterFallback fallback;
    QCPRecordingCache geometry;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    QPen pen(Qt::black);
    pen.setCosmetic(true);
    context.polyline(sharedPoints({ { 1, 1 }, { 2, 2 }, { 3, 3 } }), pen);
    const QSize outputSize(kWidth, kHeight);
    const QCPDrawList& list = context.drawList();
    const QCPDrawFrame& frame = context.frame();

    const quint64 before = QCPLineExtruder::extruderCallCount();
    plottables.clear();
    qcpReplayWithRhi(list, plottables, scatter, frame, resolve, fallback, 1.0, outputSize, 0,
                     &geometry);
    const quint64 afterDpr1 = QCPLineExtruder::extruderCallCount();
    QVERIFY2(afterDpr1 > before, "the first replay extrudes");

    plottables.clear();
    qcpReplayWithRhi(list, plottables, scatter, frame, resolve, fallback, 1.0, outputSize, 0,
                     &geometry);
    QCOMPARE(QCPLineExtruder::extruderCallCount(), afterDpr1);

    plottables.clear();
    qcpReplayWithRhi(list, plottables, scatter, frame, resolve, fallback, 2.0, outputSize, 0,
                     &geometry);
    QVERIFY2(QCPLineExtruder::extruderCallCount() > afterDpr1,
             "a new device pixel ratio rebuilds the cosmetic pen's triangles");
}

void TestDrawContext::resolveCacheSurvivesNewEntries()
{
    // A span handed out before another recording was cached must still point at its own pixels.
    Fixture fixture;
    const QCPTransform transform(fixture.plot->xAxis, fixture.plot->yAxis);
    const auto first = sharedPoints({ { 1, 2 }, { 3, 4 } });
    const auto second = sharedPoints({ { 5, 6 }, { 7, 8 }, { 9, 10 } });
    QCPResolveCache cache;
    const std::span<const QPointF> a = cache.pixels(first, 1, transform);
    QCOMPARE(int(a.size()), 2);
    const std::span<const QPointF> b = cache.pixels(second, 1, transform);
    QCOMPARE(int(b.size()), 3);
    // Reading the first span after the second entry was added: ASan sees a dangling pointer here
    // when the storage is a plain vector.
    QCOMPARE(int(a.size()), 2);
    const QPointF expected = pixelOf(transform, 1, 2);
    QCOMPARE(a[0], expected);
}

void TestDrawContext::resolveCacheIgnoresAReusedAddress()
{
    Fixture fixture;
    const QCPTransform transform(fixture.plot->xAxis, fixture.plot->yAxis);
    const auto first = sharedPoints({ { 1, 2 }, { 3, 4 } });
    QCPResolveCache cache;
    const std::span<const QPointF> a = cache.pixels(first, 1, transform);
    QCOMPARE(int(a.size()), 2);
    const auto* address = first.get();
    // Freed, and a different recording of another size takes its place.
    auto* replaced = new std::vector<QPointF> { { 9, 9 }, { 9, 9 }, { 9, 9 }, { 9, 9 } };
    auto second = std::shared_ptr<const std::vector<QPointF>>(replaced);
    cache.beginReplay();
    const std::span<const QPointF> b = cache.pixels(second, 1, transform);
    QCOMPARE(int(b.size()), 4);
    QCOMPARE(b[0], pixelOf(transform, 9, 9));
    Q_UNUSED(address);
}

void TestDrawContext::resolveCacheEvictsUnusedEntries()
{
    Fixture fixture;
    const QCPTransform transform(fixture.plot->xAxis, fixture.plot->yAxis);
    QCPResolveCache cache;
    for (int i = 0; i < 100; ++i)
    {
        cache.beginReplay();
        const auto recording = sharedPoints({ { double(i), 1 }, { 2, 3 } });
        QCOMPARE(int(cache.pixels(recording, 1, transform).size()), 2);
        QVERIFY(cache.entryCount() <= 1);
    }
}

void TestDrawContext::dashLengthsIncludeTheCaps()
{
    // QPen's default SquareCap extends every dash by half the width at each end: QPainter draws
    // 5w on / 1w off for DashLine, not 4/2.
    const float width = 2.0f;
    const qcp::DashPattern dash = qcp::dashPatternOf(QPen(Qt::black, width, Qt::DashLine), width);
    QCOMPARE(dash.count, 2);
    QCOMPARE(dash.lengths[0], 5.0f * width);
    QCOMPARE(dash.lengths[1], 1.0f * width);

    const qcp::DashPattern dot = qcp::dashPatternOf(QPen(Qt::black, width, Qt::DotLine), width);
    QCOMPARE(dot.lengths[0], 2.0f * width);
    QCOMPARE(dot.lengths[1], 1.0f * width);

    QPen flat(Qt::black, width, Qt::DashLine);
    flat.setCapStyle(Qt::FlatCap);
    const qcp::DashPattern unchanged = qcp::dashPatternOf(flat, width);
    QCOMPARE(unchanged.lengths[0], 4.0f * width);
    QCOMPARE(unchanged.lengths[1], 2.0f * width);
}

void TestDrawContext::emptyCustomDashIsSolid()
{
    QPen pen(Qt::black, 2.0, Qt::CustomDashLine);
    pen.setDashPattern({ 0, 0 });
    QVERIFY(!qcp::isDashed(pen));
    std::vector<float> verts;
    qcp::extrudeStroke(std::span<const QPointF>(std::array<QPointF, 3> {
                           QPointF(0, 0), QPointF(10, 0), QPointF(20, 0) }),
                       pen, 2.0f, verts);
    QVERIFY(!verts.empty());
    QCOMPARE(verts.size() % 6, std::size_t(0)); // 6 floats: the solid format
}

void TestDrawContext::closedRingJoinsTheSeam()
{
    const std::array<QPointF, 5> ring { QPointF(0, 0), QPointF(10, 0), QPointF(10, 10),
                                        QPointF(0, 10), QPointF(0, 0) };
    std::vector<float> closed;
    qcp::extrudeStroke(std::span<const QPointF>(ring), QPen(Qt::black, 2.0), 2.0f, closed, true);
    // The miter at the seam reaches halfWidth * sqrt(2) outside the corner; a butt end does not.
    bool hasMiter = false;
    for (std::size_t i = 0; i < closed.size(); i += 6)
    {
        const float x = std::abs(closed[i]);
        const float y = std::abs(closed[i + 1]);
        // halfWidth is 1: a miter lands on the diagonal corner (1.0, 1.0); a butt end has one
        // coordinate at 1 and the other at 0.
        if (x > 0.9f && x < 1.1f && y > 0.9f && y < 1.1f)
            hasMiter = true;
    }
    QVERIFY2(hasMiter, "the ring's closing corner has no join");
}

void TestDrawContext::nonSolidBrushIsNotFlattened()
{
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    QLinearGradient gradient(0, 0, 1, 0);
    gradient.setColorAt(0, Qt::red);
    gradient.setColorAt(1, Qt::blue);
    context.rect(QCP::Data { 20, 20 }, QCP::Data { 120, 100 }, QPen(Qt::NoPen), QBrush(gradient));
    const qcp::QCPDrawCommand& command = context.drawList().commands.front();
    // The GPU has no gradient: the command becomes an image, it is never flattened to one colour.
    QVERIFY2(qcp::fillNeedsRaster(command), "a gradient fill was offered to the GPU");

    const auto paintedCount = [&](const qcp::QCPRasterFallback* fallback)
    {
        QImage image(kWidth, kHeight, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::white);
        {
            QCPPainter painter(&image);
            painter.setAntialiasing(true);
            qcpReplayWithPainter(context.drawList(), painter, context.frame(),
                                 QCPReplayKinds::PainterOnly, fallback);
        }
        int painted = 0;
        for (int y = 25; y < 95; y += 5)
            for (int x = 25; x < 115; x += 5)
                if (image.pixelColor(x, y) != QColor(Qt::white))
                    ++painted;
        return painted;
    };

    // The image path owns the fill: the painter pass leaves it alone.
    qcp::QCPRasterFallback untouched;
    QCOMPARE(paintedCount(&untouched), 0);
    // A full layer hands the command back to the painter, gradient intact.
    qcp::QCPRasterFallback fallback;
    fallback.painterHandles(context.drawList().id, 0);
    QVERIFY2(paintedCount(&fallback) > 50, "the gradient fill disappeared");
}

void TestDrawContext::transparentBrushNeedsNoRaster()
{
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    context.rect(QCP::Data { 20, 20 }, QCP::Data { 120, 100 }, QPen(Qt::NoPen),
                 QBrush(QColor(255, 0, 0, 0)));
    const qcp::QCPDrawCommand& command = context.drawList().commands.front();
    QVERIFY2(std::holds_alternative<qcp::RectCmd>(command),
             "the recording dropped the command instead of recording it");
    QVERIFY2(!qcp::fillNeedsRaster(command), "an invisible solid brush was rasterised");
    QVERIFY2(!qcp::needsRaster(command), "an invisible fill needs no raster at all");
}
