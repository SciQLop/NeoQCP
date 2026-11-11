#include "test-user-system.h"
#include "../misuse-recorder.h"
#include "drawable.h"
#include "painting/draw-commands.h"
#include "polar/angularaxis.h"
#include "polar/polar-area.h"
#include "polar/radialaxis.h"
#include "qcustomplot.h"
#include "skew-system.h"
#include "vector2d.h"
#include <QApplication>
#include <QBrush>
#include <QImage>
#include <QPen>
#include <QtTest/QtTest>
#include <algorithm>
#include <memory>
#include <vector>

namespace
{
void settle(QCustomPlot& plot)
{
    plot.resize(400, 300);
    plot.replot();
    for (int i = 0; i < 10 && plot.isRepaintQueued(); ++i)
        QCoreApplication::processEvents();
}

//! Draws what a scheduleRepaint asked for, without a manual replot: the test of a dirty mark.
void flushRepaints(QCustomPlot& plot)
{
    for (int i = 0; i < 10 && plot.isRepaintQueued(); ++i)
        QCoreApplication::processEvents();
}

QCPAxisRect* plainRect(QCustomPlot& plot)
{
    plot.plotLayout()->clear();
    auto* rect = new QCPAxisRect(&plot);
    plot.plotLayout()->addElement(0, 0, rect);
    settle(plot);
    return rect;
}

//! The skew rect alone on the plot, bare: every non-white pixel is the drawing under test.
SkewRect* skewRect(QCustomPlot& plot)
{
    plot.setBackground(QBrush(Qt::white));
    plot.plotLayout()->clear();
    auto* rect = new SkewRect(&plot);
    plot.plotLayout()->addElement(0, 0, rect);
    rect->axis(QCPAxis::atBottom)->setRange(0, 10);
    rect->axis(QCPAxis::atLeft)->setRange(0, 10);
    for (QCPAxis* axis : rect->axes())
    {
        axis->setVisible(false);
        axis->grid()->setVisible(false);
        axis->setTicks(false);
        axis->setSubTicks(false);
        axis->setTickLabels(false);
    }
    settle(plot);
    return rect;
}

QCPPolarArea* polarArea(QCustomPlot& plot)
{
    plot.plotLayout()->clear();
    auto* area = new QCPPolarArea(&plot);
    plot.plotLayout()->addElement(0, 0, area);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    settle(plot);
    return area;
}

QImage render(QCustomPlot& plot)
{
    return plot.toPixmap(400, 300).toImage();
}

int lightnessAt(const QImage& image, QPointF at)
{
    return QColor(image.pixel(at.toPoint())).lightness();
}

bool isRed(const QImage& image, QPointF at)
{
    const QColor colour(image.pixel(at.toPoint()));
    return colour.red() > 160 && colour.green() < 100 && colour.blue() < 100;
}

//! A curve that says what it recorded: mCore is the base's, protected.
class InspectableCurve : public QCPCurve
{
public:
    using QCPCurve::QCPCurve;

    [[nodiscard]] int recordings() const { return mCore.recordingCount(); }

    [[nodiscard]] const QCPDrawList& recording() const { return mCore.recordedList(); }
};

bool recordingHolds(const QCPDrawList& list, QPointF dataPoint)
{
    for (const qcp::QCPDrawCommand& command : list.commands)
    {
        if (const auto* line = std::get_if<qcp::PolylineDataCmd>(&command))
        {
            for (const QPointF& point : *line->points)
            {
                if (qAbs(point.x() - dataPoint.x()) < 1e-9
                    && qAbs(point.y() - dataPoint.y()) < 1e-9)
                    return true;
            }
        }
    }
    return false;
}

//! A cartesian rect above a skew one: for the tests that move a plottable between systems.
struct TwoRects
{
    QCPAxisRect* cartesian;
    SkewRect* skew;
};

TwoRects twoRects(QCustomPlot& plot)
{
    plot.setBackground(QBrush(Qt::white));
    plot.plotLayout()->clear();
    auto* cartesian = new QCPAxisRect(&plot);
    auto* skew = new SkewRect(&plot);
    plot.plotLayout()->addElement(0, 0, cartesian);
    plot.plotLayout()->addElement(1, 0, skew);
    settle(plot);
    return { cartesian, skew };
}

//! A curve on the rect's axes, moved onto the rect's skew system: the J-3 route a user takes.
InspectableCurve* curveOn(SkewRect* rect)
{
    auto* curve = new InspectableCurve(rect->axis(QCPAxis::atBottom), rect->axis(QCPAxis::atLeft));
    curve->setSystem(rect->skew());
    curve->setPen(QPen(Qt::black, 3));
    curve->setLineStyle(QCPCurve::lsLine);
    return curve;
}

QPointF pixelOf(const QCP::Pixels& pixel)
{
    return QPointF(pixel.x, pixel.y);
}
} // namespace

void TestUserSystem::init()
{
    test_misuse::calls.clear();
    qcp::setMisuseHandler(test_misuse::record);
}

void TestUserSystem::cleanup()
{
    qcp::setMisuseHandler(nullptr);
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
}

void TestUserSystem::theCapabilityMasks()
{
    QCustomPlot plot;
    QCPAxisRect* rect = plainRect(plot);
    QCPAxis* key = rect->axis(QCPAxis::atBottom);
    QCPAxis* value = rect->axis(QCPAxis::atLeft);
    QCPCoordinateSystem* cartesian = rect->system(key, value);
    QVERIFY(cartesian);
    QVERIFY2(cartesian->capabilities()
                 == (QCP::capRectangular | QCP::capStraightKey | QCP::capColourScale | QCP::capLanes
                     | QCP::capStraightSegments),
             "a cartesian system offers every capability");

    std::unique_ptr<QCPCurve> curve(new QCPCurve(key, value));
    QVERIFY2(curve->requiredCapabilities() == QCP::capStraightSegments,
             "a curve needs only a straight segment between two data points");
    std::unique_ptr<QCPBars> bars(new QCPBars(key, value));
    QVERIFY2(bars->requiredCapabilities() == (QCP::capRectangular | QCP::capStraightKey),
             "bars still need the rectangle and a straight key axis");

    QCustomPlot polarPlot;
    QCPPolarArea* area = polarArea(polarPlot);
    QCPCoordinateSystem* polar = area->system(area->angularAxis(), area->radialAxis());
    QVERIFY(polar);
    QVERIFY2(polar->capabilities() == QCP::Capabilities(), "a polar system offers nothing");
}

void TestUserSystem::aGraphJoinedToASkewSystemIsRefused()
{
    QCustomPlot plot;
    TwoRects rects = twoRects(plot);
    auto* graph = new QCPGraph(rects.cartesian->axis(QCPAxis::atBottom),
                               rects.cartesian->axis(QCPAxis::atLeft));
    QCPCoordinateSystem* own = graph->system();
    QVERIFY(own);

    // Task 1 (A9): a graph still needs a rectangle, so joining a skew system is refused. Task 2
    // gives the graph a data-space path on any such system, and its test becomes
    // aGraphOnASkewRectHasNoProjection.
    graph->setSystem(rects.skew->skew());
    QCOMPARE(graph->system(), own);
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    test_misuse::calls.clear();
}

void TestUserSystem::anItemPositionOnASkewSystemHasNoProjection()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();

    auto* item = new QCPItemText(&plot);
    item->position->setSystem(system);
    item->position->setType(QCPItemPosition::ptPlotCoords);
    item->position->setCoords(5, 5);
    item->setClipArea(rect);
    item->setClipToAxisRect(true);

    QCOMPARE(item->position->system(), system);
    QVERIFY2(!item->position->keyAxis(), "a custom system hands no 2.x key projection");
    QVERIFY2(!item->position->valueAxis(), "a custom system hands no 2.x value projection");
    QVERIFY(item->position->area() == rect);

    settle(plot);
    const QPointF mapped = pixelOf(system->pixelOf(5, 5));
    const QPointF pixel = item->position->pixelPosition();
    QVERIFY2(qAbs(pixel.x() - mapped.x()) < 1e-6 && qAbs(pixel.y() - mapped.y()) < 1e-6,
             "the position resolves through the mapping");
}

void TestUserSystem::barsAreRefusedOnASkewRect()
{
    QCustomPlot plot;
    TwoRects rects = twoRects(plot);
    auto* bars = new QCPBars(rects.cartesian->axis(QCPAxis::atBottom),
                             rects.cartesian->axis(QCPAxis::atLeft));
    QCPCoordinateSystem* own = bars->system();
    QVERIFY(own);

    bars->setSystem(rects.skew->skew());
    QCOMPARE(bars->system(), own);
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    test_misuse::calls.clear();
}

void TestUserSystem::aCurvePointLandsOnTheSkewMapping()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    InspectableCurve* curve = curveOn(rect);
    QCOMPARE(curve->system(), static_cast<QCPCoordinateSystem*>(system));
    curve->setData(QVector<double> { 2, 5 }, QVector<double> { 2, 5 });
    settle(plot);

    QVERIFY2(recordingHolds(curve->recording(), QPointF(5, 5)),
             "the curve records the data point itself, not a pixel");

    const QPointF mapped = pixelOf(system->pixelOf(5, 5));
    QVERIFY2(rect->rect().contains(mapped.toPoint()), "the sheared point is inside the rect");
    const QImage image = render(plot);
    QVERIFY2(lightnessAt(image, mapped) < 128, "the curve is not drawn on the sheared pixel");

    const QPointF cartesian(rect->axis(QCPAxis::atBottom)->coordToPixel(5),
                            rect->axis(QCPAxis::atLeft)->coordToPixel(5));
    QVERIFY(rect->rect().contains(cartesian.toPoint()));
    QCOMPARE(lightnessAt(image, cartesian), 255);
}

void TestUserSystem::aCurveIsHitThroughTheMapping()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    InspectableCurve* curve = curveOn(rect);
    curve->setData(QVector<double> { 2, 5 }, QVector<double> { 2, 5 });
    settle(plot);

    const double hit = curve->selectTest(pixelOf(system->pixelOf(5, 5)), false);
    QVERIFY2(hit >= 0.0, "the sheared point is not even inside the rect");
    QVERIFY2(hit < plot.selectionTolerance(), "the click on the sheared point misses");

    const QPointF cartesian(rect->axis(QCPAxis::atBottom)->coordToPixel(5),
                            rect->axis(QCPAxis::atLeft)->coordToPixel(5));
    QVERIFY(rect->rect().contains(cartesian.toPoint()));
    QVERIFY2(curve->selectTest(cartesian, false) > plot.selectionTolerance(),
             "the unsheared pixel hits: the hit test ignored the mapping");
}

void TestUserSystem::aCurveIsRectSelectedThroughTheMapping()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    InspectableCurve* curve = curveOn(rect);
    curve->setData(QVector<double> { 2, 5, 8 }, QVector<double> { 2, 5, 8 });
    settle(plot);

    const QPointF mapped = pixelOf(system->pixelOf(5, 5));
    const QRectF band(mapped - QPointF(4, 4), QSizeF(8, 8));
    const QCPDataSelection inside = curve->selectTestRect(band, false);
    QCOMPARE(inside.dataRangeCount(), 1);
    QCOMPARE(inside.dataRange(0), QCPDataRange(1, 2));
}

void TestUserSystem::aShearChangeRepaints()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    InspectableCurve* curve = curveOn(rect);
    curve->setData(QVector<double> { 2, 5 }, QVector<double> { 2, 5 });

    auto* item = new QCPItemLine(&plot);
    for (QCPItemPosition* position : item->positions())
    {
        position->setSystem(system);
        position->setType(QCPItemPosition::ptPlotCoords);
    }
    item->start->setCoords(6, 2);
    item->end->setCoords(7, 2);
    item->setPen(QPen(Qt::red, 3));
    item->setClipArea(rect);
    item->setClipToAxisRect(true);
    settle(plot);

    const QPointF curveBefore = pixelOf(system->pixelOf(5, 5));
    const QPointF itemBefore = pixelOf(system->pixelOf(6.5, 2));
    QImage image = render(plot);
    QVERIFY2(lightnessAt(image, curveBefore) < 128, "the curve starts on its mapping");
    QVERIFY2(isRed(image, itemBefore), "the item starts on its mapping");

    const int recordings = curve->recordings();
    system->setShear(0.5);
    QVERIFY2(plot.isRepaintQueued(), "a shear change left every layer clean");
    flushRepaints(plot);

    const QPointF curveAfter = pixelOf(system->pixelOf(5, 5));
    const QPointF itemAfter = pixelOf(system->pixelOf(6.5, 2));
    QVERIFY2(QCPVector2D(curveAfter - curveBefore).length() > 5.0, "the curve did not move");
    QVERIFY2(QCPVector2D(itemAfter - itemBefore).length() > 5.0, "the item did not move");

    image = render(plot);
    QVERIFY2(lightnessAt(image, curveAfter) < 128,
             "the curve is not replayed through the new shear");
    QCOMPARE(lightnessAt(image, curveBefore), 255);
    QVERIFY2(isRed(image, itemAfter), "the item is not replayed through the new shear");
    QCOMPARE(lightnessAt(image, itemBefore), 255);

    // A curve records in data space: a new frame replays the recording, it does not remake it.
    QCOMPARE(curve->recordings(), recordings);
}

void TestUserSystem::rescaleAxesFitsASkewCurve()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    rect->axis(QCPAxis::atLeft)->setRange(0, 50);
    InspectableCurve* curve = curveOn(rect);
    curve->setData(QVector<double> { 1, 5, 9 }, QVector<double> { 20, 30, 40 });
    settle(plot);

    rect->axis(QCPAxis::atBottom)->rescale();
    QCOMPARE(rect->axis(QCPAxis::atBottom)->range().lower, 1.0);
    QCOMPARE(rect->axis(QCPAxis::atBottom)->range().upper, 9.0);

    rect->axis(QCPAxis::atLeft)->rescale();
    QCOMPARE(rect->axis(QCPAxis::atLeft)->range().lower, 20.0);
    QCOMPARE(rect->axis(QCPAxis::atLeft)->range().upper, 40.0);
}

void TestUserSystem::anAxisListsASkewItem()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();

    auto* item = new QCPItemLine(&plot);
    for (QCPItemPosition* position : item->positions())
    {
        position->setSystem(system);
        position->setType(QCPItemPosition::ptPlotCoords);
    }
    item->start->setCoords(2, 2);
    item->end->setCoords(5, 5);

    QVERIFY2(rect->axis(QCPAxis::atBottom)->items().contains(item),
             "the key axis does not list an item bound to its system");
    QVERIFY2(rect->axis(QCPAxis::atLeft)->items().contains(item),
             "the value axis does not list an item bound to its system");
    QVERIFY2(!rect->axis(QCPAxis::atTop)->items().contains(item),
             "an axis the system does not map lists the item");
}

void TestUserSystem::anAxisRangeChangeRepaintsASkewCurve()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    InspectableCurve* curve = curveOn(rect);
    curve->setData(QVector<double> { 2, 8 }, QVector<double> { 1, 1 });
    settle(plot);

    const QPointF before = pixelOf(system->pixelOf(8, 1));
    QVERIFY2(lightnessAt(render(plot), before) < 128, "the curve starts on its mapping");

    // The value axis alone moves it: the two ends of the key range keep the point inside the rect.
    rect->axis(QCPAxis::atLeft)->setRange(0, 20);
    QVERIFY2(plot.isRepaintQueued(), "a range change left the curve's layer clean");
    flushRepaints(plot);

    const QPointF after = pixelOf(system->pixelOf(8, 1));
    QVERIFY2(QCPVector2D(after - before).length() > 5.0, "the curve did not follow the new range");
    const QImage image = render(plot);
    QVERIFY2(lightnessAt(image, after) < 128, "the curve is not drawn at the new value pixel");
    QCOMPARE(lightnessAt(image, before), 255);
}
