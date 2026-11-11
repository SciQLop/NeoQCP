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
    QCPAxisRect* const rects[] = { cartesian, skew };
    for (QCPAxisRect* rect : rects)
    {
        rect->axis(QCPAxis::atBottom)->setRange(0, 10);
        rect->axis(QCPAxis::atLeft)->setRange(0, 10);
    }
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

// The graph's geometry, read through its protected builders: what a frame that has no axis pair
// must produce, and what the renderer then streams.
class InspectableGraph : public QCPGraph
{
public:
    using QCPGraph::QCPGraph;

    QVector<QPointF> lines(const QCPDataRange& range) const
    {
        QVector<QPointF> result;
        getLines(&result, range);
        return result;
    }
    QVector<QPointF> scatters(const QCPDataRange& range) const
    {
        QVector<QPointF> result;
        getScatters(&result, range);
        return result;
    }
    QPointF fillBase(QPointF matchingDataPoint) const { return getFillBasePoint(matchingDataPoint); }
    QVector<QCPDataRange> fillSegments(const QVector<QPointF>* lines) const
    {
        return getNonNanSegments(lines, Qt::Horizontal);
    }
    QPolygonF fillPolygon(const QVector<QPointF>* lines, QCPDataRange segment) const
    {
        return getFillPolygon(lines, segment);
    }
};

InspectableGraph* graphOn(SkewRect* rect, QCPGraph::LineStyle style = QCPGraph::lsLine)
{
    auto* graph = new InspectableGraph(rect->axis(QCPAxis::atBottom), rect->axis(QCPAxis::atLeft));
    graph->setLineStyle(style);
    graph->setPen(QPen(Qt::black, 3));
    return graph;
}

bool near(const QPointF& a, const QPointF& b, double tolerance = 0.5)
{
    return QCPVector2D(a - b).length() <= tolerance;
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

void TestUserSystem::aGraphOnASkewRectHasNoProjection()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    InspectableGraph* graph = graphOn(rect);

    // A11: the mask the graph was built with is the one its system has to offer, so a graph joins
    // the skew rect at construction, and J-2 leaves it without a 2.x projection.
    QCOMPARE(graph->system(), static_cast<QCPCoordinateSystem*>(system));
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
    QVERIFY2(!graph->keyAxis(), "a projection was handed to a graph on a user system");
    QVERIFY2(!graph->valueAxis(), "a projection was handed to a graph on a user system");
    QCOMPARE(graph->keyAxisBase(), static_cast<QCPAxisBase*>(rect->axis(QCPAxis::atBottom)));

    graph->setData(QVector<double> { 2, 5 }, QVector<double> { 2, 5 });
    settle(plot);
    QVERIFY2(lightnessAt(render(plot), pixelOf(system->pixelOf(5, 5))) < 128,
             "the graph is not drawn on the skew mapping");
}

void TestUserSystem::addGraphOnASkewRectJoinsIt()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);

    QCPGraph* graph = plot.addGraph(rect->axis(QCPAxis::atBottom), rect->axis(QCPAxis::atLeft));
    QVERIFY(graph);
    QCOMPARE(graph->system(), static_cast<QCPCoordinateSystem*>(rect->skew()));
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
}

void TestUserSystem::barsBuiltOnASkewRectAreRefused()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);

    // A11: the capability is checked where the object is built, not only in setSystem: a bar needs
    // a rectangle, and a user system does not offer one.
    auto* bars = new QCPBars(rect->axis(QCPAxis::atBottom), rect->axis(QCPAxis::atLeft));
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    QVERIFY2(!bars->system(), "a bar joined a system that lacks its rectangle");
    QCOMPARE(plot.plottableCount(), 0);
    delete bars;
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

void TestUserSystem::rescaleAxesFitsASkewGraph()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    rect->axis(QCPAxis::atBottom)->setRange(0, 50);
    rect->axis(QCPAxis::atLeft)->setRange(0, 50);
    InspectableGraph* graph = graphOn(rect);
    graph->setData(QVector<double> { 1, 5, 9 }, QVector<double> { 20, 30, 40 });
    settle(plot);

    rect->axis(QCPAxis::atBottom)->rescale();
    rect->axis(QCPAxis::atLeft)->rescale();
    QCOMPARE(rect->axis(QCPAxis::atBottom)->range(), QCPRange(1, 9));
    QCOMPARE(rect->axis(QCPAxis::atLeft)->range(), QCPRange(20, 40));
}

void TestUserSystem::anAxisRangeChangeRepaintsASkewGraph()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    InspectableGraph* graph = graphOn(rect);
    graph->setData(QVector<double> { 2, 8 }, QVector<double> { 1, 1 });
    settle(plot);

    const QPointF before = pixelOf(system->pixelOf(8, 1));
    QVERIFY2(lightnessAt(render(plot), before) < 128, "the graph starts on its mapping");

    rect->axis(QCPAxis::atLeft)->setRange(0, 20);
    QVERIFY2(plot.isRepaintQueued(), "a range change left the graph's layer clean");
    flushRepaints(plot);

    const QPointF after = pixelOf(system->pixelOf(8, 1));
    QVERIFY2(QCPVector2D(after - before).length() > 5.0, "the graph did not follow the new range");
    const QImage image = render(plot);
    QVERIFY2(lightnessAt(image, after) < 128, "the graph is not drawn at the new value pixel");
    QCOMPARE(lightnessAt(image, before), 255);
}

void TestUserSystem::aGraphLineFollowsTheSkewMapping()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    InspectableGraph* graph = graphOn(rect);
    graph->setData(QVector<double> { 2, 4, 6 }, QVector<double> { 2, 6, 3 });
    settle(plot);

    const QVector<QPointF> line = graph->lines(QCPDataRange(0, 3));
    QCOMPARE(line.size(), 3);
    const QVector<QPointF> expected { pixelOf(system->pixelOf(2, 2)),
                                      pixelOf(system->pixelOf(4, 6)),
                                      pixelOf(system->pixelOf(6, 3)) };
    for (int i = 0; i < expected.size(); ++i)
        QVERIFY2(near(line.at(i), expected.at(i)), "a vertex is not the mapping of its data point");

    // The segment between two vertices is straight in pixels: the middle of the first one is drawn.
    const QImage image = render(plot);
    QVERIFY2(lightnessAt(image, (expected.at(0) + expected.at(1)) / 2.0) < 200,
             "the segment between two mapped vertices is not drawn");
}

void TestUserSystem::stepStylesAreBuiltInDataSpace()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    InspectableGraph* graph = graphOn(rect, QCPGraph::lsStepLeft);
    graph->setData(QVector<double> { 2, 4 }, QVector<double> { 2, 6 });
    settle(plot);

    // A step left goes horizontally to the previous value at the new key, then up to the value.
    const QVector<QPointF> line = graph->lines(QCPDataRange(0, 2));
    QCOMPARE(line.size(), 4);
    QVERIFY2(near(line.at(2), pixelOf(system->pixelOf(4, 2))),
             "the step's corner is not the mapping of (next key, previous value)");
    QVERIFY2(near(line.at(3), pixelOf(system->pixelOf(4, 6))),
             "the step does not end on the mapping of its own data point");
}

void TestUserSystem::anImpulseRunsToValueZero()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    InspectableGraph* graph = graphOn(rect, QCPGraph::lsImpulse);
    graph->setData(QVector<double> { 2, 3 }, QVector<double> { 4, 5 });
    settle(plot);

    const QVector<QPointF> line = graph->lines(QCPDataRange(0, 2));
    QCOMPARE(line.size(), 4);
    QVERIFY2(near(line.at(0), pixelOf(system->pixelOf(2, 0))), "the impulse does not start at zero");
    QVERIFY2(near(line.at(1), pixelOf(system->pixelOf(2, 4))), "the impulse misses its data point");
    QVERIFY2(near(line.at(2), pixelOf(system->pixelOf(3, 0))), "the impulse does not start at zero");
    QVERIFY2(near(line.at(3), pixelOf(system->pixelOf(3, 5))), "the impulse misses its data point");
}

void TestUserSystem::aLogValueFillStopsAtTheRangeBound()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    rect->axis(QCPAxis::atLeft)->setScaleType(QCPAxis::stLogarithmic);
    rect->axis(QCPAxis::atLeft)->setRange(1, 10);
    InspectableGraph* graph = graphOn(rect);
    graph->setData(QVector<double> { 2, 4 }, QVector<double> { 4, 8 });
    graph->setBrush(QColor(0, 0, 0, 80));
    settle(plot);

    // Value zero has no pixel on a log axis: the fill closes on the lower range bound instead, and
    // never builds a vertex at infinity.
    const QPointF base = graph->fillBase(pixelOf(system->pixelOf(4, 8)));
    QVERIFY2(qIsFinite(base.x()) && qIsFinite(base.y()), "the fill base is not a finite pixel");
    QVERIFY2(near(base, pixelOf(system->pixelOf(4, 1))), "the fill does not close on the range bound");
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
}

void TestUserSystem::aNaNSplitsLineAndFill()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    InspectableGraph* graph = graphOn(rect);
    graph->setData(QVector<double> { 2, 3, 4, 5, 6 },
                   QVector<double> { 2, 4, qQNaN(), 6, 7 });
    graph->setBrush(QColor(0, 0, 0, 80));
    settle(plot);

    const QVector<QPointF> line = graph->lines(QCPDataRange(0, 5));
    QCOMPARE(line.size(), 5);
    QVERIFY2(qIsNaN(line.at(2).y()), "the gap is not a gap in the mapped line");
    const QVector<QCPDataRange> segments = graph->fillSegments(&line);
    QCOMPARE(segments.size(), 2);
    for (const QCPDataRange& segment : segments)
    {
        const QPolygonF polygon = graph->fillPolygon(&line, segment);
        QCOMPARE(polygon.size(), segment.size() + 2);
        for (const QPointF& vertex : polygon)
            QVERIFY2(qIsFinite(vertex.x()) && qIsFinite(vertex.y()),
                     "a fill polygon vertex is not finite");
    }
    QVERIFY2(near(line.at(1), pixelOf(system->pixelOf(3, 4))), "the point before the gap moved");
}

void TestUserSystem::aScatterLandsOnTheMapping()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    InspectableGraph* graph = graphOn(rect, QCPGraph::lsNone);
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 6));
    graph->setData(QVector<double> { 2, 5 }, QVector<double> { 3, 4 });
    settle(plot);

    const QVector<QPointF> scatters = graph->scatters(QCPDataRange(0, 2));
    QCOMPARE(scatters.size(), 2);
    QVERIFY2(near(scatters.at(0), pixelOf(system->pixelOf(2, 3))), "a scatter misses its mapping");
    QVERIFY2(near(scatters.at(1), pixelOf(system->pixelOf(5, 4))), "a scatter misses its mapping");
    QVERIFY2(lightnessAt(render(plot), scatters.at(1)) < 128, "the scatter is not drawn");
}

void TestUserSystem::aGraphIsHitAndRectSelectedThroughTheMapping()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    InspectableGraph* graph = graphOn(rect);
    graph->setData(QVector<double> { 2, 5, 8 }, QVector<double> { 2, 5, 8 });
    settle(plot);

    const QPointF mapped = pixelOf(system->pixelOf(5, 5));
    const double hit = graph->selectTest(mapped, false);
    QVERIFY2(hit >= 0.0, "the click on the sheared line is outside the system");
    QVERIFY2(hit < plot.selectionTolerance(), "the click on the sheared line misses");

    const QPointF below = mapped + QPointF(0, 50);
    QVERIFY2(rect->rect().contains(below.toPoint()), "the miss probe left the rect");
    QVERIFY2(graph->selectTest(below, false) > plot.selectionTolerance(),
             "a pixel off the sheared line is a hit");

    const QRectF band(mapped - QPointF(4, 4), QSizeF(8, 8));
    const QCPDataSelection selection = graph->selectTestRect(band, false);
    QCOMPARE(selection.dataRangeCount(), 1);
    QCOMPARE(selection.dataRange(0), QCPDataRange(1, 2));
}

void TestUserSystem::aShearedPointOutsideTheKeyWindowIsHit()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    InspectableGraph* graph = graphOn(rect);
    graph->setData(QVector<double> { 1 }, QVector<double> { 9 });
    settle(plot);

    // The shear moves the pixel of a high point far to the right: a hit test that narrows the data
    // by the key under the click searches the wrong end of the data and finds nothing.
    const QPointF mapped = pixelOf(system->pixelOf(1, 9));
    QVERIFY2(rect->rect().contains(mapped.toPoint()), "the sheared point left the rect");
    // A 2.x frame reads this pixel through its key axis; under the shear that read is nowhere near
    // the point's own key, so a hit test narrowed to the key under the click searches the wrong end
    // of the data and finds nothing.
    const double cartesianKey = rect->axis(QCPAxis::atBottom)->pixelToCoord(mapped.x());
    QVERIFY2(qAbs(cartesianKey - 1.0) > 5.0, "the shear no longer moves the click away from its key");
    const double hit = graph->selectTest(mapped, false);
    QVERIFY2(hit >= 0.0, "the click is outside the system");
    QVERIFY2(hit < plot.selectionTolerance(), "a point outside the key window was not hit");
}

void TestUserSystem::hoveringASkewGraphDoesNotCrash()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    InspectableGraph* graph = graphOn(rect);
    graph->setData(QVector<double> { 2, 5 }, QVector<double> { 2, 5 });
    settle(plot);

    for (int i = 0; i < 20; ++i)
    {
        const QPointF pixel(20 + i * 18, 30 + i * 10);
        plot.plottableAt(pixel, false);
        graph->selectTest(pixel, true);
        graph->selectTest(pixelOf(system->pixelOf(5, 5)), true);
    }
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
}

void TestUserSystem::channelFillNeedsOneSystem()
{
    QCustomPlot plot;
    TwoRects rects = twoRects(plot);
    InspectableGraph* graph = graphOn(rects.skew);
    auto* cartesian = new QCPGraph(rects.cartesian->axis(QCPAxis::atBottom),
                                   rects.cartesian->axis(QCPAxis::atLeft));

    // A5: on a user system both ends of a channel must be mapped by the same system, otherwise the
    // polygon would join two different frames.
    graph->setChannelFillGraph(cartesian);
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    QVERIFY2(!graph->channelFillGraph(), "a channel joined a graph of another frame");
    test_misuse::calls.clear();

    InspectableGraph* other = graphOn(rects.skew);
    graph->setChannelFillGraph(other);
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
    QCOMPARE(graph->channelFillGraph(), other);

    graph->setData(QVector<double> { 2, 8 }, QVector<double> { 2, 2 });
    other->setData(QVector<double> { 2, 8 }, QVector<double> { 8, 8 });
    graph->setBrush(QColor(255, 0, 0));
    settle(plot);
    const QPointF inside = pixelOf(rects.skew->skew()->pixelOf(5, 5));
    QVERIFY2(rects.skew->rect().contains(inside.toPoint()), "the sample lies outside the rect");
    QVERIFY2(isRed(render(plot), inside), "the channel between the two graphs is not filled");
}

void TestUserSystem::aCartesianChannelFillAcrossValueAxesStillWorks()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.setBackground(Qt::white);
    settle(plot);
    QCPAxis* right = plot.axisRect()->add<QCPAxis>(QCPAxis::atRight).get();
    plot.xAxis->setRange(0, 10);
    plot.yAxis->setRange(0, 10);
    right->setRange(0, 10);

    auto* lower = new QCPGraph(plot.xAxis, plot.yAxis);
    auto* upper = new QCPGraph(plot.xAxis, right);
    lower->setData(QVector<double> { 0, 10 }, QVector<double> { 2, 2 });
    upper->setData(QVector<double> { 0, 10 }, QVector<double> { 8, 8 });
    lower->setBrush(QColor(255, 0, 0));
    lower->setChannelFillGraph(upper);
    settle(plot);

    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
    const QPointF inside(plot.xAxis->coordToPixel(5), plot.yAxis->coordToPixel(5));
    QVERIFY2(isRed(render(plot), inside),
             "the 2.x channel fill between two value axes stopped filling");
}
