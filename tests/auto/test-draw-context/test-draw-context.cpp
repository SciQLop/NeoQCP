#include "test-draw-context.h"

#include "../misuse-recorder.h"

#include <painting/command-replay-rhi.h>
#include <painting/draw-commands.h>
#include <painting/draw-context.h>
#include <painting/line-extruder.h>
#include <painting/plottable-rhi-layer.h>
#include <painting/raster-fallback.h>
#include <painting/scatter-rhi-layer.h>
#include <qcustomplot.h>

#include <QElapsedTimer>
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

// A user system that keeps a window of its own: the context must answer this one, not the axes'.
class KeyWindowSystem : public QCPCoordinateSystem
{
public:
    KeyWindowSystem(QCPArea* area, QCPAxisBase* keyAxis, QCPAxisBase* valueAxis,
                    QCPKeyWindow window)
            : QCPCoordinateSystem(area, keyAxis, valueAxis)
            , mWindow(window)
    {
    }

    QCPTransform transform() const override
    {
        return QCPTransform(static_cast<const QCPAxis*>(keyAxis()),
                            static_cast<const QCPAxis*>(valueAxis()));
    }
    QRect clipRect() const override { return area()->rect(); }
    QCP::Capabilities capabilities() const override { return QCP::capStraightSegments; }
    QCPKeyWindow keyWindow() const override { return mWindow; }

private:
    QCPKeyWindow mWindow;
};

// A user system that keeps none: the base's empty window, which says unbounded and never [0, 0].
class WindowlessSystem : public QCPCoordinateSystem
{
public:
    WindowlessSystem(QCPArea* area, QCPAxisBase* keyAxis, QCPAxisBase* valueAxis)
            : QCPCoordinateSystem(area, keyAxis, valueAxis)
    {
    }

    QCPTransform transform() const override
    {
        return QCPTransform(static_cast<const QCPAxis*>(keyAxis()),
                            static_cast<const QCPAxis*>(valueAxis()));
    }
    QRect clipRect() const override { return area()->rect(); }
    QCP::Capabilities capabilities() const override { return QCP::capStraightSegments; }
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

std::shared_ptr<const std::vector<QRgb>> sharedColours(std::vector<QRgb> colours)
{
    return std::make_shared<const std::vector<QRgb>>(std::move(colours));
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
// A batch is a header: its arrays live behind one pointer, so a recording that holds no batch pays
// nothing for the ones that exist, and a batch costs a command slot like any other.
static_assert(sizeof(qcp::RectsCmd) <= 96);
static_assert(sizeof(qcp::EllipsesCmd) <= 96);

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

// The painter culls a command outside the clip by its box. A shape that crosses the view but whose
// last point lies outside it is still drawn: the box spans every point, not the last one alone.
void TestDrawContext::shapeEndingOutsideTheClipIsDrawn()
{
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    const double far = 10.0 * kWidth;
    const std::vector<QCP::Position> corners { QCP::Data { 20, 20 }, QCP::Data { 120, 30 },
                                               QCP::Data { far, far } };
    context.polygon(std::span<const QCP::Position>(corners), kPen, kBrush);
    const QCPScatterStyle style(QCPScatterStyle::ssDisc, kPen, kBrush, 9.0);
    context.markers(sharedPoints({ { 60, 90 }, { far, far } }), style);

    const QImage replay = replayOf(context);
    const QPointF vertex = pixelOf(context.transform(), 20, 20);
    const QPointF marker = pixelOf(context.transform(), 60, 90);
    QVERIFY2(replay.pixelColor(vertex.toPoint()) != QColor(Qt::white),
             "the polygon was culled on its last vertex");
    QVERIFY2(replay.pixelColor(marker.toPoint()) != QColor(Qt::white),
             "the markers were culled on their last point");
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
        const QCPDataBox& bounds = context.drawList().data.bounds;
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
        const QCPDataBox& bounds = context.drawList().data.bounds;
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
        const QCPDataBox& bounds = context.drawList().data.bounds;
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
        const QCPDataBox& bounds = context.drawList().data.bounds;
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
        const QCPDataBox& bounds = context.drawList().data.bounds;
        QCOMPARE(bounds.keyLower, 1.0);
        QCOMPARE(bounds.keyUpper, 9.0);
        QCOMPARE(bounds.valueLower, 1.0);
        QCOMPARE(bounds.valueUpper, 9.0);
    }
    {
        // An empty recording is unbounded, not a box at the origin.
        QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                               nullptr);
        const QCPDataBox& bounds = context.drawList().data.bounds;
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
    QVERIFY(list.data.keyBounded);
    QVERIFY(list.data.valueBounded);
    QCOMPARE(list.data.bounds.keyLower, 1.0);
    QCOMPARE(list.data.bounds.keyUpper, 3.0);
    QCOMPARE(list.data.bounds.valueLower, 2.0);
    QCOMPARE(list.data.bounds.valueUpper, 4.0);
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
    QVERIFY2(qcp::rasterNeedsOf(command).fill, "a gradient fill was offered to the GPU");

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

void TestDrawContext::mixedPositionsKeepKeyAndValueOnAVerticalKeyAxis()
{
    Fixture fixture;
    // Ranges far apart, so a pixel taken from the wrong space cannot hide behind a rounding.
    fixture.plot->xAxis->setRange(0, 2);
    fixture.plot->yAxis->setRange(0, 10);
    QCPAxis* keyAxis = fixture.plot->yAxis;
    QCPAxis* valueAxis = fixture.plot->xAxis;
    const QRect rect = fixture.plot->axisRect()->rect();
    const QCPDrawFrame frame = qcpMakeFrame(keyAxis, valueAxis, fixture.plot->axisRect(), rect);
    QCPItemLine item(fixture.plot.get());
    item.start->setAxes(keyAxis, valueAxis);
    const auto check = [&](const QCP::Position& position, QPointF expected, const char* what)
    {
        item.start->set(position);
        const QPointF recorded = qcpResolvePosition(frame, position);
        const QPointF legacy = item.start->pixelPosition();
        const auto near
            = [](QPointF a, QPointF b) { return std::hypot(a.x() - b.x(), a.y() - b.y()) <= 0.5; };
        QVERIFY2(
            near(recorded, expected) && near(legacy, expected),
            qPrintable(QStringLiteral("%1: expected (%2, %3), recorded (%4, %5), item (%6, %7)")
                           .arg(QLatin1String(what))
                           .arg(expected.x())
                           .arg(expected.y())
                           .arg(recorded.x())
                           .arg(recorded.y())
                           .arg(legacy.x())
                           .arg(legacy.y())));
    };
    // The data number is the key, wherever the key axis is: on a vertical key axis it is a screen
    // y, and the ratio runs along the value axis, across the rect's width.
    check(QCP::DataXRatioY { 7.0, 0.8 },
          QPointF(rect.left() + 0.8 * rect.width(), keyAxis->coordToPixel(7.0)),
          "key data, value ratio");
    check(QCP::RatioXDataY { 0.25, 1.5 },
          QPointF(valueAxis->coordToPixel(1.5), rect.top() + 0.25 * rect.height()),
          "key ratio, value data");
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
    QVERIFY2(!qcp::rasterNeedsOf(command).fill, "an invisible solid brush was rasterised");
    QVERIFY2(!qcp::needsRaster(command), "an invisible fill needs no raster at all");
}

// A bar chart records ten thousand rects a frame: as commands they cost a variant slot, a cache key
// and a visit each. A run of data-space rects is one batch instead, and its box is kept as it
// appends rather than re-scanned.
void TestDrawContext::tenThousandDataRectsAreOneCommand()
{
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    for (int i = 0; i < 10000; ++i)
        context.rect(QCP::Data { 0.01 * i, 0.0 }, QCP::Data { 0.01 * i + 0.005, 1.0 }, kPen,
                     kBrush);

    const QCPDrawList& list = context.drawList();
    QCOMPARE(int(list.commands.size()), 1);
    QVERIFY2(std::holds_alternative<qcp::RectsCmd>(list.commands.front()),
             "a run of data-space rects was not recorded as one batch");
    const auto& batch = std::get<qcp::RectsCmd>(list.commands.front());
    QCOMPARE(int(batch.boxes->aKey.size()), 10000);
    QCOMPARE(int(batch.boxes->aValue.size()), 10000);
    QCOMPARE(int(batch.boxes->bKey.size()), 10000);
    QCOMPARE(int(batch.boxes->bValue.size()), 10000);
    QVERIFY2(batch.boxes->fills.empty(),
             "one colour belongs to the batch's brush, not to a fill per rect");
    QVERIFY2(batch.sortedByKey, "the keys arrived in ascending order");
    QCOMPARE(batch.pen, kPen);
    QCOMPARE(batch.brush, kBrush);

    // The batch holds the box the recorder widened, and the recording's two boxes agree with it.
    QCOMPARE(batch.bounds.keyLower, 0.0);
    QCOMPARE(batch.bounds.keyUpper, 99.995);
    QCOMPARE(batch.bounds.valueLower, 0.0);
    QCOMPARE(batch.bounds.valueUpper, 1.0);
    QVERIFY(list.data.keyBounded && list.data.valueBounded);
    QCOMPARE(list.data.bounds.keyUpper, 99.995);
    QCOMPARE(list.index.bounds.keyUpper, 99.995);

    // One command, one cache slot, one visit: the painter draws one rect per bar, exactly as the
    // ten thousand RectCmds did. The hand painting clips like the replay does - a stroke that sits
    // on the axis rect's edge is cut there by the clip, so both must be compared under one.
    const QImage replay = replayOf(context);
    const QImage hand = painted(
        [](QCPPainter& painter, const QCPTransform& transform)
        {
            painter.setClipRect(QRectF(0, 0, kWidth, kHeight));
            painter.setPen(kPen);
            painter.setBrush(kBrush);
            for (int i = 0; i < 10000; ++i)
                painter.drawRect(QRectF(pixelOf(transform, 0.01 * i, 0.0),
                                        pixelOf(transform, 0.01 * i + 0.005, 1.0))
                                     .normalized());
        },
        context.transform());
    QCOMPARE(replay, hand);
}

void TestDrawContext::aPixelCornerStaysASingleRect()
{
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    context.rect(QCP::Data { 1, 1 }, QCP::Data { 2, 2 }, kPen, kBrush);
    // One corner in pixels: no data box to widen, so it stays the single command it always was, and
    // it breaks the run a batch would continue.
    context.rect(QCP::Data { 3, 3 }, QCP::Pixels { 50, 50 }, kPen, kBrush);
    context.rect(QCP::Pixels { 60, 60 }, QCP::Pixels { 90, 90 }, kPen, kBrush);
    // A data rect after a pixel one cannot join it either: the run starts again with it.
    context.rect(QCP::Data { 4, 4 }, QCP::Data { 5, 5 }, kPen, kBrush);
    context.rect(QCP::Data { 6, 6 }, QCP::Data { 7, 7 }, kPen, kBrush);

    const QCPDrawList& list = context.drawList();
    QCOMPARE(int(list.commands.size()), 4);
    for (int i = 0; i < 3; ++i)
        QVERIFY2(std::holds_alternative<qcp::RectCmd>(list.commands[std::size_t(i)]),
                 qPrintable(QStringLiteral("command %1 is not a single rect").arg(i)));
    const auto& batch = std::get<qcp::RectsCmd>(list.commands.back());
    QCOMPARE(int(batch.boxes->aKey.size()), 2);
    QCOMPARE(batch.boxes->aKey.front(), 4.0);
    QCOMPARE(batch.boxes->aKey.back(), 6.0);
    // The pixel rects left both sides of the box unbounded, so the batch's own box is all it has.
    QCOMPARE(batch.bounds.keyLower, 4.0);
    QCOMPARE(batch.bounds.keyUpper, 7.0);
}

void TestDrawContext::perRectColoursStayOneBatch()
{
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    const QBrush red(QColor(200, 90, 30));
    const QBrush blue(QColor(30, 90, 200));
    for (int i = 0; i < 10; ++i)
        context.rect(QCP::Data { 10.0 * i, 20.0 }, QCP::Data { 10.0 * i + 8.0, 120.0 }, kPen,
                     i % 2 ? blue : red);

    const QCPDrawList& list = context.drawList();
    QCOMPARE(int(list.commands.size()), 1);
    const auto& batch = std::get<qcp::RectsCmd>(list.commands.front());
    QCOMPARE(int(batch.boxes->fills.size()), 10);
    QCOMPARE(batch.boxes->fills[0], red.color().rgba());
    QCOMPARE(batch.boxes->fills[1], blue.color().rgba());

    // A per-rect colour is a solid brush: the painter paints each rect its own, not the batch's.
    const QImage image = replayOf(context);
    const auto centre = [&](int rect)
    {
        const QPointF pixel = pixelOf(context.transform(), 10.0 * rect + 4.0, 70.0);
        return image.pixelColor(int(pixel.x()), int(pixel.y()));
    };
    QVERIFY2(centre(0).red() > centre(0).blue() + 100, "the first bar kept the red brush");
    QVERIFY2(centre(1).blue() > centre(1).red() + 100, "the second bar kept the blue brush");
}

// A fill the GPU cannot take becomes one image per command, so a batch that needed one would put
// every fill under every stroke. Overlapping translucent or patterned boxes would then paint in a
// different order than a command per box does, so such a box keeps its own command.
void TestDrawContext::rasterBrushRectsStayOneCommandEach()
{
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    QLinearGradient fade(0, 0, 0, 1);
    fade.setCoordinateMode(QGradient::ObjectBoundingMode);
    fade.setColorAt(0.0, QColor(200, 40, 40, 160));
    fade.setColorAt(1.0, QColor(40, 40, 200, 160));
    const QBrush gradient(fade);

    // Three boxes that overlap: the order of their fills and strokes is visible where they do.
    std::vector<QRectF> boxes;
    for (int i = 0; i < 3; ++i)
    {
        const double left = 20.0 + 25.0 * i;
        context.rect(QCP::Data { left, 20.0 }, QCP::Data { left + 60.0, 120.0 }, kPen, gradient);
        boxes.emplace_back(left, 20.0, 60.0, 100.0);
    }

    const QCPDrawList& list = context.drawList();
    QCOMPARE(int(list.commands.size()), 3);
    for (const qcp::QCPDrawCommand& command : list.commands)
    {
        QVERIFY2(
            std::holds_alternative<qcp::RectCmd>(command),
            "a rect whose fill needs the raster joined a batch and lost its place in the order");
        QVERIFY2(qcp::needsRaster(command), "a gradient fill is an image, not a vertex colour");
    }

    // One draw per box, fill and stroke interleaved: the replay is the hand painting of the three,
    // so the overlaps come out in the order they were recorded.
    const QImage replay = replayOf(context);
    const QImage hand = painted(
        [&boxes, &gradient](QCPPainter& painter, const QCPTransform& transform)
        {
            painter.setClipRect(QRectF(0, 0, kWidth, kHeight));
            painter.setPen(kPen);
            painter.setBrush(gradient);
            for (const QRectF& box : boxes)
                painter.drawRect(QRectF(pixelOf(transform, box.left(), box.top()),
                                        pixelOf(transform, box.right(), box.bottom()))
                                     .normalized());
        },
        context.transform());
    QCOMPARE(replay, hand);
}

void TestDrawContext::ellipsesStayOneBatch()
{
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    for (int i = 0; i < 5; ++i)
        context.ellipse(QCP::Data { 20.0 + 35.0 * i, 30.0 }, QCP::Data { 40.0 + 35.0 * i, 90.0 },
                        kPen, kBrush);

    const QCPDrawList& list = context.drawList();
    QCOMPARE(int(list.commands.size()), 1);
    QVERIFY2(std::holds_alternative<qcp::EllipsesCmd>(list.commands.front()),
             "a run of data-space ellipses was not recorded as one batch");
    const auto& batch = std::get<qcp::EllipsesCmd>(list.commands.front());
    QCOMPARE(int(batch.boxes->aKey.size()), 5);
    QCOMPARE(batch.bounds.valueUpper, 90.0);

    // The disc is filled and the corner of its box is not: the replay draws the ellipses, not
    // rects.
    const QImage image = replayOf(context);
    const QPointF centre = pixelOf(context.transform(), 30.0, 60.0);
    const QPointF corner = pixelOf(context.transform(), 21.0, 31.0);
    QCOMPARE(image.pixelColor(int(centre.x()), int(centre.y())).green(), kBrush.color().green());
    QCOMPARE(image.pixelColor(int(corner.x()), int(corner.y())), QColor(Qt::white));
}

// The brief's export question: a batch must export as one vector rect per bar, not as one shape.
// Hand painting the ten rects is exactly what a RectCmd replay does, so the images must be equal.
void TestDrawContext::batchExportsOneRectPerBar()
{
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    std::vector<QRectF> boxes;
    for (int i = 0; i < 10; ++i)
    {
        const double left = 4.0 + 19.0 * i;
        context.rect(QCP::Data { left, 20.0 }, QCP::Data { left + 15.0, 120.0 }, kPen, kBrush);
        boxes.emplace_back(QPointF(left, 20.0), QPointF(left + 15.0, 120.0));
    }
    QCOMPARE(int(context.drawList().commands.size()), 1);

    const QImage replay = replayOf(context);
    const QImage hand = painted(
        [&boxes](QCPPainter& painter, const QCPTransform& transform)
        {
            painter.setPen(kPen);
            painter.setBrush(kBrush);
            for (const QRectF& box : boxes)
                painter.drawRect(QRectF(pixelOf(transform, box.left(), box.top()),
                                        pixelOf(transform, box.right(), box.bottom()))
                                     .normalized());
        },
        context.transform());
    QCOMPARE(replay, hand);
}

void TestDrawContext::colouredPolylineIsOneCommand()
{
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    const auto colours = sharedColours(
        { qRgb(255, 0, 0), qRgb(0, 255, 0), qRgb(0, 0, 255), qRgb(255, 0, 0), qRgb(0, 255, 0) });
    context.polyline(
        sharedPoints({ { 10, 10 }, { 40, 90 }, { 80, 30 }, { 120, 120 }, { 170, 60 } }), colours,
        kPen);

    // The colours are payload, not commands: a line of five points is one command whatever it is
    // painted with.
    QCOMPARE(int(context.drawList().commands.size()), 1);
    const auto* command = std::get_if<qcp::PolylineDataCmd>(&context.drawList().commands.front());
    QVERIFY(command);
    QVERIFY2(command->colours == colours, "the command keeps the caller's colour vector");
    QCOMPARE(int(command->colours->size()), 5);
}

void TestDrawContext::segmentTakesItsFirstVertexColour()
{
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    // A horizontal line across the widget, red then green then blue: each half of it is one colour.
    context.polyline(sharedPoints({ { 20, 75 }, { 100, 75 }, { 180, 75 } }),
                     sharedColours({ qRgb(255, 0, 0), qRgb(0, 255, 0), qRgb(0, 0, 255) }), kPen);

    const QImage replay = replayOf(context);
    const QColor first = replay.pixelColor(60, 75);
    const QColor second = replay.pixelColor(140, 75);
    QVERIFY2(first.red() > 150 && first.green() < 90 && first.blue() < 90,
             qPrintable(QStringLiteral("the first segment is red, not %1").arg(first.name())));
    QVERIFY2(second.green() > 150 && second.red() < 90 && second.blue() < 90,
             qPrintable(QStringLiteral("the second segment is green, not %1").arg(second.name())));
}

void TestDrawContext::colouredMarkersKeepTheirColours()
{
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    const QCPScatterStyle style(QCPScatterStyle::ssDisc, QPen(Qt::black), QBrush(Qt::black), 11.0);
    context.markers(sharedPoints({ { 40, 40 }, { 100, 75 }, { 160, 110 } }),
                    sharedColours({ qRgb(255, 0, 0), qRgb(0, 255, 0), qRgb(0, 0, 255) }), style);

    const QImage replay = replayOf(context);
    // Data (x, y) lands at (x, height - y): each disc is checked at its centre.
    const QColor first = replay.pixelColor(40, 110);
    const QColor second = replay.pixelColor(100, 75);
    const QColor third = replay.pixelColor(160, 40);
    QVERIFY2(first.red() > 150 && first.green() < 90 && first.blue() < 90,
             qPrintable(QStringLiteral("the first marker is red, not %1").arg(first.name())));
    QVERIFY2(second.green() > 150 && second.red() < 90 && second.blue() < 90,
             qPrintable(QStringLiteral("the second marker is green, not %1").arg(second.name())));
    QVERIFY2(third.blue() > 150 && third.red() < 90 && third.green() < 90,
             qPrintable(QStringLiteral("the third marker is blue, not %1").arg(third.name())));
}

void TestDrawContext::mismatchedColourCountIsRefused()
{
    Fixture fixture;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    test_misuse::calls.clear();
    qcp::setMisuseHandler(test_misuse::record);
    const auto points = sharedPoints({ { 10, 10 }, { 40, 90 }, { 80, 30 } });
    // Two colours for three points: which vertex carries which colour is a guess, so nothing is
    // recorded rather than a line painted from a half-read vector.
    context.polyline(points, sharedColours({ qRgb(255, 0, 0), qRgb(0, 255, 0) }), kPen);
    context.markers(points, sharedColours({ qRgb(255, 0, 0), qRgb(0, 255, 0) }),
                    QCPScatterStyle(QCPScatterStyle::ssDisc, 7.0));
    qcp::setMisuseHandler(nullptr);

    QCOMPARE(int(context.drawList().commands.size()), 0);
    QCOMPARE(test_misuse::calls.size(), size_t(2));
}

// The R2 recording cache translates kept geometry on a pan: a coloured line must be built once,
// like a plain one, or a pan of a colour-by-scalar line extrudes its vertices again on every frame.
void TestDrawContext::colouredPolylinePanReusesTheKeptGeometry()
{
    Fixture fixture;
    QCPPlottableRhiLayer plottables(nullptr);
    QCPScatterRhiLayer scatter(nullptr);
    QCPResolveCache resolve;
    qcp::QCPRasterFallback fallback;
    QCPRecordingCache geometry;
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr);
    std::vector<QRgb> colours;
    std::vector<QPointF> points;
    for (int i = 0; i < 20; ++i)
    {
        points.emplace_back(2.0 + 9.0 * i, 20.0 + 50.0 * (i % 3));
        colours.push_back(qRgb(255 * (i % 3 == 0), 255 * (i % 3 == 1), 255 * (i % 3 == 2)));
    }
    context.polyline(sharedPoints(points), sharedColours(colours), kPen);

    const QSize outputSize(kWidth, kHeight);
    const QCPDrawList& list = context.drawList();
    plottables.clear();
    qcpReplayWithRhi(list, plottables, scatter, context.frame(), resolve, fallback, 1.0, outputSize,
                     0, &geometry);
    const quint64 extruded = QCPLineExtruder::extruderCallCount();
    QVERIFY2(extruded > 0, "the first frame must extrude the coloured line");

    fixture.plot->xAxis->setRange(2.5, 202.5); // a translation, same pixels per key
    plottables.clear();
    qcpReplayWithRhi(list, plottables, scatter, context.frame(), resolve, fallback, 1.0, outputSize,
                     0, &geometry);
    QCOMPARE(QCPLineExtruder::extruderCallCount(), extruded);
    QVERIFY2(plottables.hasGeometry(), "the kept vertices are submitted again, with the pan delta");
}

// The brief's export question: a data ellipse whose box spans a billion pixels. The axes show a unit
// square, so the ellipse covers the frame many times over; 2.x caught the allocation failure and hid
// the item. The replay must keep the frame affordable whether the ellipse is batched or alone.
void TestDrawContext::aHugeEllipseKeepsTheFrame()
{
    Fixture fixture; // one pixel per unit, so 1e9 data units are 1e9 pixels
    const QBrush otherBrush(QColor(10, 200, 90));
    for (const double extent : { 1e9, 1e12 })
    {
        QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis,
                               fixture.plot->axisRect(), nullptr);
        context.ellipse(QCP::Data { 0.0, 0.0 }, QCP::Data { extent, extent }, kPen, kBrush);
        // A corner in pixels cannot join the batch, so this one replays as an EllipseCmd of its own.
        context.ellipse(QCP::Data { 0.0, 0.0 }, QCP::Pixels { extent, extent }, kPen, otherBrush);
        context.ellipse(QCP::Data { 150.0, 40.0 }, QCP::Data { 180.0, 80.0 }, kPen,
                        QBrush(QColor(200, 90, 30)));

        QElapsedTimer timer;
        timer.start();
        const QImage image = replayOf(context);
        QVERIFY2(timer.elapsed() < 2000,
                 qPrintable(QStringLiteral("a %1-wide ellipse took %2 ms")
                                .arg(extent)
                                .arg(timer.elapsed())));
        // The small ellipse after the huge ones still paints: the replay survived them.
        const QPointF centre = pixelOf(context.transform(), 165.0, 60.0);
        QCOMPARE(image.pixelColor(int(centre.x()), int(centre.y())), QColor(200, 90, 30));
    }
}

// P-2: on a user system the key window is the system's, and a system that keeps none says
// "unbounded" with an empty window. Only a cartesian frame still answers from its key axis.
void TestDrawContext::theKeyWindowComesFromTheSystem()
{
    Fixture fixture;
    const QCPKeyWindow ownWindow { QCPRange(10.0, 60.0), 4.0 };
    KeyWindowSystem system(fixture.plot->axisRect(), fixture.plot->xAxis, fixture.plot->yAxis,
                           ownWindow);
    QCPDrawContext context(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                           nullptr, false, QRectF(), &system);
    const QCPKeyWindow answered = context.keyWindow();
    QCOMPARE(answered.keys.lower, ownWindow.keys.lower);
    QCOMPARE(answered.keys.upper, ownWindow.keys.upper);
    QCOMPARE(answered.pixelsPerKey, ownWindow.pixelsPerKey);
    QVERIFY2(context.drawList().keyWindow.has_value(),
             "a window the system names is recorded, so a replay can tell whether it is still valid");
    QCOMPARE(context.drawList().keyWindow->keys.upper, ownWindow.keys.upper);

    WindowlessSystem windowless(fixture.plot->axisRect(), fixture.plot->xAxis, fixture.plot->yAxis);
    QCPDrawContext defaultContext(fixture.plot->xAxis, fixture.plot->yAxis,
                                  fixture.plot->axisRect(), nullptr, false, QRectF(), &windowless);
    const QCPKeyWindow empty = defaultContext.keyWindow();
    QVERIFY2(!(empty.keys.size() > 0.0),
             "a system without a window answers an empty one, never the axes' [0, 0]");
    QCOMPARE(empty.pixelsPerKey, 0.0);
    QVERIFY2(!defaultContext.drawList().keyWindow.has_value(),
             "an empty window is unbounded, so there is no window to record");

    // The cartesian frame (no system) keeps today's axis computation.
    QCPDrawContext cartesian(fixture.plot->xAxis, fixture.plot->yAxis, fixture.plot->axisRect(),
                             nullptr);
    const QCPKeyWindow fromAxes = cartesian.keyWindow();
    QVERIFY(fromAxes.keys.lower < fixture.plot->xAxis->range().lower);
    QVERIFY(fromAxes.keys.upper > fixture.plot->xAxis->range().upper);
    QVERIFY(fromAxes.pixelsPerKey > 0.0);
    QVERIFY(cartesian.drawList().keyWindow.has_value());
}
