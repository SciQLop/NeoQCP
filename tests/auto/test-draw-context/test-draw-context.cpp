#include "test-draw-context.h"

#include <painting/draw-commands.h>
#include <painting/draw-context.h>
#include <qcustomplot.h>

#include <QFontMetrics>
#include <cmath>
#include <QPainterPath>
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
    return painted(
        [&context](QCPPainter& painter, const QCPTransform&) {
            qcpReplayWithPainter(context.drawList(), painter, context.frame());
        },
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

    const std::vector<QPointF> points { { 10, 10 }, { 40, 90 }, { 80, 30 }, { 120, 120 },
                                        { 170, 60 } };
    context.polyline(sharedPoints(points), kPen);

    const QImage replay = replayOf(context);
    const QImage hand = painted(
        [&points](QCPPainter& painter, const QCPTransform& transform) {
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

    const std::vector<QCP::Position> positions { QCP::Data { 10, 10 }, QCP::Pixels { 40, 60 },
                                                 QCP::AxisRatio { 0.5, 0.4 },
                                                 QCP::AnchorOffset { item->start,
                                                                     QCP::PixelOffset { 5, 7 } } };
    context.polyline(std::span<const QCP::Position>(positions), kPen);

    const QRectF rect = fixture.plot->axisRect()->rect();
    const QImage replay = replayOf(context);
    const QImage hand = painted(
        [&positions, &rect](QCPPainter& painter, const QCPTransform& transform) {
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
        [&positions](QCPPainter& painter, const QCPTransform& transform) {
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
        [&upper, &lower](QCPPainter& painter, const QCPTransform& transform) {
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
        [](QCPPainter& painter, const QCPTransform& transform) {
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
        [](QCPPainter& painter, const QCPTransform& transform) {
            const QRectF rect =
                QRectF(pixelOf(transform, 20, 30), pixelOf(transform, 150, 120)).normalized();
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
        [&points, &style](QCPPainter& painter, const QCPTransform& transform) {
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
    const std::vector<QPointF> spans { { 6, 20 }, { 6, 12 }, { 6, 25 } }; // half extents, data units
    context.errorBars(sharedPoints(centres), sharedPoints(spans), kPen);

    const QImage replay = replayOf(context);
    const QImage hand = painted(
        [&centres, &spans](QCPPainter& painter, const QCPTransform& transform) {
            painter.setPen(kPen);
            for (std::size_t i = 0; i < centres.size(); ++i)
            {
                const QPointF centre = pixelOf(transform, centres[i].x(), centres[i].y());
                const QPointF above = pixelOf(transform, centres[i].x(), centres[i].y() + spans[i].y());
                const QPointF below = pixelOf(transform, centres[i].x(), centres[i].y() - spans[i].y());
                const QPointF left = pixelOf(transform, centres[i].x() - spans[i].x(), centres[i].y());
                const QPointF right =
                    pixelOf(transform, centres[i].x() + spans[i].x(), centres[i].y());
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
        [&text, &style](QCPPainter& painter, const QCPTransform& transform) {
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
        [&path](QCPPainter& painter, const QCPTransform& transform) {
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
        [&image](QCPPainter& painter, const QCPTransform& transform) {
            const QRectF rect =
                QRectF(pixelOf(transform, 30, 110), QPointF(150, 40)).normalized();
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
        [&path](QCPPainter& painter, const QCPTransform& transform) {
            painter.save();
            painter.translate(pixelOf(transform, 20, 110));
            painter.setPen(kPen);
            painter.setBrush(kBrush);
            painter.drawPath(path);
            painter.restore();
            painter.setPen(kPen);
            painter.setBrush(kBrush);
            painter.drawRect(QRectF(pixelOf(transform, 120, 20), pixelOf(transform, 180, 60))
                                 .normalized());
        },
        context.transform());
    QCOMPARE(replay, hand);
}

void TestDrawContext::painterCallbackStateDoesNotLeak()
{
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    const auto callback = [](QCPPainter& painter, const QCPTransform& transform) {
        painter.setPen(QPen(Qt::red, 1.0));
        painter.translate(50, 0);
        painter.drawLine(pixelOf(transform, 0, 0), pixelOf(transform, 10, 10));
    };
    context.painter(callback);
    context.rect(QCP::Data { 120, 20 }, QCP::Data { 180, 60 }, kPen, kBrush);

    const QImage replay = replayOf(context);
    const QImage hand = painted(
        [&callback](QCPPainter& painter, const QCPTransform& transform) {
            painter.save();
            callback(painter, transform);
            painter.restore();
            painter.setPen(kPen);
            painter.setBrush(kBrush);
            painter.drawRect(QRectF(pixelOf(transform, 120, 20), pixelOf(transform, 180, 60))
                                 .normalized());
        },
        context.transform());
    QCOMPARE(replay, hand);
}

void TestDrawContext::painterMatchesHandPainting()
{
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    const auto callback = [](QCPPainter& painter, const QCPTransform& transform) {
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
        QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis,
                               fixture.plot->axisRect(), nullptr);
        context.polyline(sharedPoints({ { -5, 20 }, { 30, 70 } }), pen);
        const QCPDataBox& bounds = context.drawList().bounds;
        QCOMPARE(bounds.keyLower, -5.0);
        QCOMPARE(bounds.keyUpper, 30.0);
        QCOMPARE(bounds.valueLower, 20.0);
        QCOMPARE(bounds.valueUpper, 70.0);
    }
    {
        // A pixel command leaves both sides unbounded, and it does not widen an earlier box either.
        QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis,
                               fixture.plot->axisRect(), nullptr);
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
        QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis,
                               fixture.plot->axisRect(), nullptr);
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
        QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis,
                               fixture.plot->axisRect(), nullptr);
        const std::vector<QCP::Position> mixed { QCP::RatioXDataY { 0.25, 60 } };
        context.polyline(std::span<const QCP::Position>(mixed), pen);
        const QCPDataBox& bounds = context.drawList().bounds;
        QVERIFY(!std::isfinite(bounds.keyLower));
        QVERIFY(!std::isfinite(bounds.keyUpper));
        QCOMPARE(bounds.valueLower, 60.0);
        QCOMPARE(bounds.valueUpper, 60.0);
    }
    {
        // An empty recording is unbounded, not a box at the origin.
        QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis,
                               fixture.plot->axisRect(), nullptr);
        const QCPDataBox& bounds = context.drawList().bounds;
        QVERIFY(!std::isfinite(bounds.keyLower));
        QVERIFY(!std::isfinite(bounds.valueLower));
    }
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
