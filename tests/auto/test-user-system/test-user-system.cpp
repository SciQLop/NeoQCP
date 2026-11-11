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
#include <QMouseEvent>
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
template <typename Rect>
Rect* bareRect(QCustomPlot& plot)
{
    plot.setBackground(QBrush(Qt::white));
    plot.plotLayout()->clear();
    auto* rect = new Rect(&plot);
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

SkewRect* skewRect(QCustomPlot& plot)
{
    return bareRect<SkewRect>(plot);
}

BendRect* bendRect(QCustomPlot& plot)
{
    return bareRect<BendRect>(plot);
}

RectangleDeclaringRect* rectangleDeclaringRect(QCustomPlot& plot)
{
    return bareRect<RectangleDeclaringRect>(plot);
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
    QVector<QPointF> dataLines(const QCPDataRange& range) const { return dataSpaceLines(range); }
    QVector<QCPDataRange> dataFillSegments(const QVector<QPointF>* lines) const
    {
        return getNonNanSegments(lines, Qt::Horizontal);
    }
    QPolygonF dataFillPolygon(const QVector<QPointF>* lines, QCPDataRange segment) const
    {
        return dataSpaceFillPolygon(lines, segment);
    }
    QVector<QCPDataRange> fillSegments(const QVector<QPointF>* lines) const
    {
        return getNonNanSegments(lines, Qt::Horizontal);
    }
    QPolygonF fillPolygon(const QVector<QPointF>* lines, QCPDataRange segment) const
    {
        return getFillPolygon(lines, segment);
    }
    void icon(QCPPainter* painter, const QRectF& rect) const { drawLegendIcon(painter, rect); }
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

//! A skew mapping that refuses every inverse: the fill must not ask it for its base point.
class NaNSkewMapping : public SkewMapping
{
public:
    using SkewMapping::SkewMapping;

    QCP::Data inverse(QPointF) const override { return QCP::Data { qQNaN(), qQNaN() }; }
};

//! Counts the batch mapping calls a hit test makes.
class CountingSkewMapping : public SkewMapping
{
public:
    using SkewMapping::SkewMapping;

    void map(std::span<const double> keys, std::span<const double> values,
             std::span<QPointF> out) const override
    {
        ++mapCalls;
        SkewMapping::map(keys, values, out);
    }

    void map(std::span<const QPointF> data, std::span<QPointF> out) const override
    {
        ++mapCalls;
        SkewMapping::map(data, out);
    }

    QCP::Pixels mapPoint(QCP::Data data) const override
    {
        ++mapCalls;
        return SkewMapping::mapPoint(data);
    }

    static inline int mapCalls = 0;
};

//! The fixture's system with a different mapping: the tests that tune what the frame answers.
template <typename Mapping>
class TunedSkewSystem : public QCPCoordinateSystem
{
public:
    TunedSkewSystem(QCPArea* area, QCPAxisBase* keyAxis, QCPAxisBase* valueAxis)
            : QCPCoordinateSystem(area, keyAxis, valueAxis)
    {
    }

    QCPTransform transform() const override
    {
        if (!mMapping || mBuiltRevision != revision())
        {
            mMapping = std::make_shared<const Mapping>(
                    qobject_cast<const QCPAxis*>(keyAxis()),
                    qobject_cast<const QCPAxis*>(valueAxis()), 1.0, rect().bottom());
            mBuiltRevision = revision();
        }
        return QCPTransform::custom(mMapping, mBuiltRevision);
    }

    QRect clipRect() const override { return rect(); }

    bool contains(QPointF pixel) const override { return rect().contains(pixel.toPoint()); }

    QCP::Capabilities capabilities() const override { return QCP::capStraightSegments; }

    [[nodiscard]] const Mapping& mapping() const
    {
        transform();
        return *mMapping;
    }

    [[nodiscard]] QCP::Pixels pixelOf(double key, double value) const
    {
        return mapping().mapPoint(QCP::Data { key, value });
    }

    void setShear(double shear)
    {
        if (qFuzzyCompare(shear, mShear))
            return;
        mShear = shear;
        parametersChanged();
    }

private:
    [[nodiscard]] QRect rect() const { return static_cast<const QCPAxisRect*>(area())->rect(); }

    mutable std::shared_ptr<const Mapping> mMapping;
    mutable quint64 mBuiltRevision = 0;
    double mShear = 1.0;
};

template <typename Mapping>
class TunedSkewRect : public QCPAxisRect
{
public:
    explicit TunedSkewRect(QCustomPlot* plot) : QCPAxisRect(plot) { }

protected:
    QCPCoordinateSystem* createSystem(QCPAxisBase* key, QCPAxisBase* value) override
    {
        return new TunedSkewSystem<Mapping>(this, key, value);
    }
};

void sendMouseClick(QWidget* widget, const QPoint& pos)
{
    QMouseEvent press(QEvent::MouseButtonPress, pos, widget->mapToGlobal(pos), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, pos, widget->mapToGlobal(pos), Qt::LeftButton,
                        Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(widget, &press);
    QApplication::sendEvent(widget, &release);
}

//! The colour a brush of \a brush over white leaves: what one fill pass paints.
QColor overWhite(const QColor& brush)
{
    const double alpha = brush.alphaF();
    return QColor(qRound(brush.red() * alpha + 255 * (1.0 - alpha)),
                  qRound(brush.green() * alpha + 255 * (1.0 - alpha)),
                  qRound(brush.blue() * alpha + 255 * (1.0 - alpha)));
}

bool colourNear(const QColor& actual, const QColor& expected, int tolerance = 3)
{
    return qAbs(actual.red() - expected.red()) <= tolerance
        && qAbs(actual.green() - expected.green()) <= tolerance
        && qAbs(actual.blue() - expected.blue()) <= tolerance;
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

void TestUserSystem::aDefaultItemOnASkewRectHasNoProjection()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    QCOMPARE(plot.xAxis, rect->axis(QCPAxis::atBottom));
    QCOMPARE(plot.yAxis, rect->axis(QCPAxis::atLeft));

    // No setSystem: the item takes the plot's default axes, which are the skew rect's.
    auto* text = new QCPItemText(&plot);
    text->position->setType(QCPItemPosition::ptPlotCoords);
    text->position->setCoords(5, 5);
    text->setClipArea(rect);
    text->setClipToAxisRect(true);

    QCOMPARE(text->position->system(), static_cast<QCPCoordinateSystem*>(system));
    QVERIFY2(!text->position->keyAxis(), "the default item position got the 2.x projection");
    QVERIFY2(!text->position->valueAxis(), "the default item position got the 2.x projection");
    const QPointF mapped = pixelOf(system->pixelOf(5, 5));
    QVERIFY2(near(text->position->pixelPosition(), mapped),
             "the default item position is not on the skew mapping");

    auto* line = new QCPItemLine(&plot);
    line->start->setParentAnchor(text->position);
    line->end->setType(QCPItemPosition::ptPlotCoords);
    line->end->setCoords(7, 2);
    line->setClipArea(rect);
    line->setClipToAxisRect(true);
    QVERIFY2(near(line->start->pixelPosition(), mapped),
             "a line anchored to the item starts at the unsheared pixel");
}

void TestUserSystem::interactiveCreationOnASkewRectLandsOnTheMapping()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    plot.setItemCreator(
            [](QCustomPlot* p, QCPAxis*, QCPAxis*) { return new QCPItemLine(p); });
    plot.setCreationModeEnabled(true);

    const QPointF click = pixelOf(system->pixelOf(5, 5));
    sendMouseClick(&plot, click.toPoint());
    QCOMPARE(plot.itemCount(), 1);
    auto* line = qobject_cast<QCPItemLine*>(plot.item(0));
    QVERIFY(line);
    QVERIFY2(near(line->start->coords(), QPointF(5, 5), 0.6),
             "interactive creation landed at the unsheared data point");
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
}

void TestUserSystem::addGraphOnABendRectIsRefused()
{
    QCustomPlot plot;
    BendRect* rect = bendRect(plot);

    plot.addGraph(rect->axis(QCPAxis::atBottom), rect->axis(QCPAxis::atLeft));
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    QCOMPARE(plot.graphCount(), 0);
    plot.clearGraphs(); // an unregistered graph in mGraphs would loop here
    QCOMPARE(plot.graphCount(), 0);
    test_misuse::calls.clear();
}

void TestUserSystem::rectAddOfARefusedGraphLeavesNothingDangling()
{
    QCustomPlot plot;
    BendRect* rect = bendRect(plot);

    auto graph = rect->add<QCPGraph>(rect->axis(QCPAxis::atBottom),
                                     rect->axis(QCPAxis::atLeft));
    QVERIFY2(!graph, "the rect handed out a graph its system refused");
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    QCOMPARE(plot.graphCount(), 0);
    QCOMPARE(plot.plottableCount(), 0);
    test_misuse::calls.clear();
}

void TestUserSystem::aRefusedPlottableIsOnNoLayer()
{
    QCustomPlot plot;
    BendRect* rect = bendRect(plot);

    auto* bars = new QCPBars(rect->axis(QCPAxis::atBottom), rect->axis(QCPAxis::atLeft));
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    QVERIFY(!bars->system());
    QVERIFY2(!bars->layer(), "a refused plottable stayed a child of a layer");
    delete bars;
    test_misuse::calls.clear();
}

void TestUserSystem::plotAddOfRefusedBarsReturnsNoHandle()
{
    QCustomPlot plot;
    bendRect(plot);

    auto bars = plot.add<QCPBars>();
    QVERIFY2(!bars, "the plot handed out a plottable its system refused");
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    QCOMPARE(plot.plottableCount(), 0);
    test_misuse::calls.clear();
}

void TestUserSystem::adoptingBarsOntoASkewDefaultRectIsRefused()
{
    QCustomPlot plot;
    skewRect(plot); // the skew rect is the plot's default rect

    auto bars = plot.add<QCPBars>();
    QVERIFY2(!bars, "the plot handed out a plottable the skew system refused");
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    QCOMPARE(plot.plottableCount(), 0);
    QCOMPARE(plot.legend ? plot.legend->itemCount() : 0, 0);
    test_misuse::calls.clear();

    QCustomPlot source;
    QCPAxisRect* sourceRect = plainRect(source);
    auto* takenBars = new QCPBars(sourceRect->axis(QCPAxis::atBottom),
                                  sourceRect->axis(QCPAxis::atLeft));
    std::unique_ptr<QCPBars> moved = source.take(QCPHandle<QCPBars>(takenBars));
    QVERIFY(moved);
    test_misuse::calls.clear();
    auto adopted = plot.add(std::move(moved));
    QVERIFY2(!adopted, "adopt accepted a plottable the skew system refused");
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    QCOMPARE(plot.plottableCount(), 0);
    test_misuse::calls.clear();
}

void TestUserSystem::aRefusedPlottableThatJoinsLaterIsRegistered()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.setBackground(Qt::white);
    QCPAxisRect* cartesian = plot.axisRect();
    cartesian->axis(QCPAxis::atBottom)->setRange(0, 10);
    cartesian->axis(QCPAxis::atLeft)->setRange(0, 10);
    auto* skew = new SkewRect(&plot);
    QVERIFY(plot.plotLayout()->addElement(1, 0, skew));
    skew->axis(QCPAxis::atBottom)->setRange(0, 10);
    skew->axis(QCPAxis::atLeft)->setRange(0, 10);
    settle(plot);
    QVERIFY(plot.legend);

    auto* bars = new QCPBars(skew->axis(QCPAxis::atBottom), skew->axis(QCPAxis::atLeft));
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    QVERIFY(!bars->system());
    QCOMPARE(plot.plottableCount(), 0);
    test_misuse::calls.clear();

    QCPCoordinateSystem* system = cartesian->system(cartesian->axis(QCPAxis::atBottom),
                                                    cartesian->axis(QCPAxis::atLeft));
    bars->setSystem(system);
    QCOMPARE(bars->system(), system);
    QCOMPARE(plot.plottableCount(), 1);
    QCOMPARE(plot.legend->itemCount(), 1);
    bars->setData(QVector<double> { 2, 5 }, QVector<double> { 4, 6 });
    settle(plot);
    const QPointF inside(cartesian->axis(QCPAxis::atBottom)->coordToPixel(3),
                         cartesian->axis(QCPAxis::atLeft)->coordToPixel(2));
    QVERIFY(cartesian->rect().contains(inside.toPoint()));
    QVERIFY2(lightnessAt(render(plot), inside) < 255, "the registered bars did not draw");
}

void TestUserSystem::aRefusedIntervalsLeavesNoLayerOrLegendRow()
{
    QCustomPlot plot;
    BendRect* rect = bendRect(plot);
    auto* layout = new QCPLaneLayout(&plot);

    auto* intervals = new QCPIntervals(rect->axis(QCPAxis::atBottom),
                                       rect->axis(QCPAxis::atLeft), layout);
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    QVERIFY(!intervals->system());
    QVERIFY2(!intervals->layer(), "a refused intervals object stayed a child of a layer");
    QVERIFY2(!intervals->labelLayer(), "a refused intervals object made its label layer");
    QCOMPARE(plot.plottableCount(), 0);
    QCOMPARE(plot.legend ? plot.legend->itemCount() : 0, 0);
    delete intervals;
    delete layout;
    test_misuse::calls.clear();
}

void TestUserSystem::aCustomSystemDeclaringARectangleStillRefusesBars()
{
    {
        QCustomPlot plot;
        RectangleDeclaringRect* rect = rectangleDeclaringRect(plot);
        QVERIFY(rect->system(rect->axis(QCPAxis::atBottom), rect->axis(QCPAxis::atLeft))
                    ->capabilities()
                    .testFlag(QCP::capRectangular));

        auto* bars = new QCPBars(rect->axis(QCPAxis::atBottom),
                                 rect->axis(QCPAxis::atLeft));
        QCOMPARE(test_misuse::calls.size(), std::size_t(1));
        QVERIFY2(!bars->system(),
                 "a non-cartesian system that declares a rectangle handed out its plottable");
        QCOMPARE(plot.plottableCount(), 0);
        delete bars;
        test_misuse::calls.clear();
    }
    {
        QCustomPlot plot;
        TwoRects rects = twoRects(plot);
        auto* bars = new QCPBars(rects.cartesian->axis(QCPAxis::atBottom),
                                 rects.cartesian->axis(QCPAxis::atLeft));
        QCPCoordinateSystem* own = bars->system();
        QVERIFY(own);
        auto* declaring = new RectangleDeclaringSystem(
                rects.skew, rects.skew->axis(QCPAxis::atBottom),
                rects.skew->axis(QCPAxis::atLeft));
        bars->setSystem(declaring);
        QCOMPARE(bars->system(), own);
        QCOMPARE(test_misuse::calls.size(), std::size_t(1));
        delete declaring;
        test_misuse::calls.clear();
    }
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

void TestUserSystem::errorBarsOnASkewRectAreRefused()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);

    auto* bars = new QCPErrorBars(rect->axis(QCPAxis::atBottom),
                                  rect->axis(QCPAxis::atLeft));
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    QVERIFY2(!bars->system(), "error bars joined a skew rect");
    QCOMPARE(plot.plottableCount(), 0);
    delete bars;
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

void TestUserSystem::aCurveHitMapsInOneBatch()
{
    QCustomPlot plot;
    auto* rect = bareRect<TunedSkewRect<CountingSkewMapping>>(plot);
    auto* system = static_cast<TunedSkewSystem<CountingSkewMapping>*>(
            rect->system(rect->axis(QCPAxis::atBottom), rect->axis(QCPAxis::atLeft)));
    auto* curve = new InspectableCurve(rect->axis(QCPAxis::atBottom),
                                       rect->axis(QCPAxis::atLeft));
    curve->setSystem(system);
    curve->setPen(QPen(Qt::black, 3));
    curve->setData(QVector<double> { 1, 2, 3, 4, 5 },
                   QVector<double> { 1, 2, 3, 4, 5 });
    settle(plot);

    const QPointF click = pixelOf(system->pixelOf(3, 3));
    CountingSkewMapping::mapCalls = 0;
    QVERIFY(curve->selectTest(click, false) >= 0.0);
    QCOMPARE(CountingSkewMapping::mapCalls, 1);
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

void TestUserSystem::aSkewGraphRescalesItsAxes()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    rect->axis(QCPAxis::atBottom)->setRange(0, 50);
    rect->axis(QCPAxis::atLeft)->setRange(0, 50);
    InspectableGraph* graph = graphOn(rect);
    graph->setData(QVector<double> { 1, 5, 9 }, QVector<double> { 20, 30, 40 });
    settle(plot);

    graph->rescaleAxes();
    QCOMPARE(rect->axis(QCPAxis::atBottom)->range(), QCPRange(1, 9));
    QCOMPARE(rect->axis(QCPAxis::atLeft)->range(), QCPRange(20, 40));

    rect->axis(QCPAxis::atBottom)->setRange(0, 50);
    rect->axis(QCPAxis::atLeft)->setRange(0, 50);
    graph->rescaleKeyAxis();
    QCOMPARE(rect->axis(QCPAxis::atBottom)->range(), QCPRange(1, 9));
    QCOMPARE(rect->axis(QCPAxis::atLeft)->range(), QCPRange(0, 50));

    graph->rescaleValueAxis(false, true);
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

void TestUserSystem::aLogImpulseIsDropped()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    rect->axis(QCPAxis::atLeft)->setScaleType(QCPAxis::stLogarithmic);
    rect->axis(QCPAxis::atLeft)->setRange(1, 10);
    InspectableGraph* graph = graphOn(rect, QCPGraph::lsImpulse);
    graph->setData(QVector<double> { 2, 3 }, QVector<double> { 4, 5 });
    settle(plot);

    // Value zero has no pixel on a log axis: the impulse pair is dropped, never handed to drawLines.
    const QVector<QPointF> line = graph->lines(QCPDataRange(0, 2));
    QVERIFY2(line.isEmpty(), "an impulse to zero on a log axis reached the line builder");
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
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
    const QVector<QPointF> dataLine = graph->dataLines(QCPDataRange(0, 2));
    const QVector<QCPDataRange> segments = graph->dataFillSegments(&dataLine);
    QCOMPARE(segments.size(), 1);
    const QPolygonF fill = graph->dataFillPolygon(&dataLine, segments.first());
    QCOMPARE(fill.size(), 4);
    const QPointF base = fill.first();
    QVERIFY2(qIsFinite(base.x()) && qIsFinite(base.y()), "the fill base is not a finite pixel");
    QVERIFY2(near(base, pixelOf(system->pixelOf(2, 1))), "the fill does not close on the range bound");
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
}

void TestUserSystem::aLogValueFillOnANegativeRangeClosesTowardZero()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    rect->axis(QCPAxis::atLeft)->setScaleType(QCPAxis::stLogarithmic);
    rect->axis(QCPAxis::atLeft)->setRange(-10, -1);
    InspectableGraph* graph = graphOn(rect);
    graph->setData(QVector<double> { 2, 4 }, QVector<double> { -4, -8 });
    graph->setBrush(QColor(0, 0, 0, 80));
    settle(plot);

    // 2.x closes toward zero, which on a negative log range is the upper bound.
    const QVector<QPointF> dataLine = graph->dataLines(QCPDataRange(0, 2));
    const QVector<QCPDataRange> segments = graph->dataFillSegments(&dataLine);
    QCOMPARE(segments.size(), 1);
    const QPolygonF fill = graph->dataFillPolygon(&dataLine, segments.first());
    QCOMPARE(fill.size(), 4);
    QVERIFY2(near(fill.first(), pixelOf(system->pixelOf(2, -1))),
             "the fill on a negative log range closes on the wrong side");
    QVERIFY2(near(fill.last(), pixelOf(system->pixelOf(4, -1))),
             "the fill on a negative log range closes on the wrong side");
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
}

void TestUserSystem::aFillWithAnUnrepresentableInverseStaysFinite()
{
    QCustomPlot plot;
    auto* rect = bareRect<TunedSkewRect<NaNSkewMapping>>(plot);
    auto* system = static_cast<TunedSkewSystem<NaNSkewMapping>*>(
            rect->system(rect->axis(QCPAxis::atBottom), rect->axis(QCPAxis::atLeft)));
    auto* graph = new InspectableGraph(rect->axis(QCPAxis::atBottom),
                                       rect->axis(QCPAxis::atLeft));
    graph->setSystem(system);
    graph->setData(QVector<double> { 2, 4 }, QVector<double> { 2, 8 });
    graph->setBrush(QColor(255, 0, 0));
    settle(plot);

    // The fill is built in data space: an inverse() that answers NaN cannot leave a NaN vertex.
    const QPointF inside = pixelOf(system->pixelOf(3, 2));
    QVERIFY2(rect->rect().contains(inside.toPoint()), "the sample lies outside the rect");
    QVERIFY2(isRed(render(plot), inside),
             "the fill with an unrepresentable inverse drew nothing");
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
    const QVector<QPointF> dataLine = graph->dataLines(QCPDataRange(0, 5));
    const QVector<QCPDataRange> segments = graph->dataFillSegments(&dataLine);
    QCOMPARE(segments.size(), 2);
    for (const QCPDataRange& segment : segments)
    {
        const QPolygonF polygon = graph->dataFillPolygon(&dataLine, segment);
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

void TestUserSystem::aSelectedChannelFillSegmentPaintsOnlyItsBand()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    InspectableGraph* graph = graphOn(rect);
    InspectableGraph* other = graphOn(rect);
    graph->setData(QVector<double> { 1, 2, 3, 4, 5 }, QVector<double> { 2, 2, 2, 2, 2 });
    other->setData(QVector<double> { 1, 2, 3, 4, 5 }, QVector<double> { 8, 8, 8, 8, 8 });
    const QColor brush(255, 0, 0, 80);
    graph->setBrush(brush);
    graph->setChannelFillGraph(other);
    graph->setSelectable(QCP::stMultipleDataRanges);
    settle(plot);

    graph->setSelection(QCPDataSelection(QCPDataRange(1, 3))); // points 1-2, keys 2-3
    settle(plot);

    // Keys 4.5 is outside the selected segment: one unselected pass, one brush over white.
    const QPointF outside = pixelOf(system->pixelOf(4.5, 5));
    QVERIFY2(rect->rect().contains(outside.toPoint()), "the sample lies outside the rect");
    const QColor actual = render(plot).pixelColor(outside.toPoint());
    QVERIFY2(colourNear(actual, overWhite(brush)),
             qPrintable(QStringLiteral("unselected band pixel is %1, expected %2")
                                .arg(actual.name())
                                .arg(overWhite(brush).name())));
}

void TestUserSystem::aChannelFillAcrossSystemsDrawsNoBand()
{
    QCustomPlot plot;
    TwoRects rects = twoRects(plot);
    InspectableGraph* graph = graphOn(rects.skew);
    InspectableGraph* other = graphOn(rects.skew);
    graph->setData(QVector<double> { 2, 8 }, QVector<double> { 2, 2 });
    other->setData(QVector<double> { 2, 8 }, QVector<double> { 8, 8 });
    graph->setBrush(QColor(255, 0, 0));
    graph->setChannelFillGraph(other);
    settle(plot);

    const QPointF inside = pixelOf(rects.skew->skew()->pixelOf(5, 5));
    QVERIFY2(isRed(render(plot), inside), "the channel between the two skew graphs is not filled");

    // The target moves to the cartesian frame after the channel was set: the band would join two
    // planes, so it is not drawn.
    other->setSystem(rects.cartesian->system(rects.cartesian->axis(QCPAxis::atBottom),
                                             rects.cartesian->axis(QCPAxis::atLeft)));
    settle(plot);
    QCOMPARE(lightnessAt(render(plot), inside), 255);
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
}

void TestUserSystem::aCartesianChannelFillPartnerThatLostItsSystemDrawsNoBand()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.setBackground(Qt::white);
    settle(plot);
    plot.xAxis->setRange(0, 10);
    plot.yAxis->setRange(0, 10);

    auto* lower = new QCPGraph(plot.xAxis, plot.yAxis);
    auto* upper = new QCPGraph(plot.xAxis, plot.yAxis);
    lower->setData(QVector<double> { 0, 10 }, QVector<double> { 2, 2 });
    upper->setData(QVector<double> { 0, 10 }, QVector<double> { 8, 8 });
    lower->setBrush(QColor(255, 0, 0));
    lower->setChannelFillGraph(upper);
    settle(plot);
    const QPointF inside(plot.xAxis->coordToPixel(5), plot.yAxis->coordToPixel(5));
    QVERIFY2(isRed(render(plot), inside), "the cartesian channel is not filled");

    upper->setSystem(nullptr);
    settle(plot);
    QCOMPARE(lightnessAt(render(plot), inside), 255);
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
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

void TestUserSystem::aResizeMovesTheRevision()
{
    // The contract a caching system rests on: the area's geometry is part of the mapping, so a
    // resize moves the revision and the mapping is rebuilt against the new frame.
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    const quint64 before = system->revision();
    const QPointF oldPixel = pixelOf(system->pixelOf(5, 5));
    const QRect oldFrame = rect->rect();

    // Not settle(): it sizes the plot back to 400x300 for every other test.
    plot.resize(700, 500);
    plot.replot();
    flushRepaints(plot);

    QVERIFY(rect->rect() != oldFrame);
    QVERIFY(system->revision() != before);
    // The point (5, 5) sits at the middle of the frame, and the shear is the fixture's 1.0:
    // its pixel is the frame's middle pushed right by half the frame's height.
    const QRect frame = rect->rect();
    const QPointF expected(frame.left() + 0.5 * frame.width() + 0.5 * frame.height(),
                           frame.bottom() - 0.5 * frame.height());
    QVERIFY2(near(pixelOf(system->pixelOf(5, 5)), expected, 1.5),
             qPrintable(QStringLiteral("mapped %1, expected %2")
                                .arg(pixelOf(system->pixelOf(5, 5)).x())
                                .arg(expected.x())));
    QVERIFY(pixelOf(system->pixelOf(5, 5)) != oldPixel);
}

void TestUserSystem::anAreaIsNotAdoptedByAnotherPlot()
{
    // Moving an area silently used to leave its axes and systems in the plot it left, and every
    // later setSystem from the new plot was then refused: an area belongs to one plot.
    QCustomPlot owner;
    SkewRect* rect = skewRect(owner);
    QVERIFY(rect->layout() == owner.plotLayout());

    QCustomPlot other;
    QCPLayoutInset* inset = new QCPLayoutInset;
    QVERIFY(other.plotLayout()->addElement(inset));

    QVERIFY2(!other.plotLayout()->addElement(rect),
             "an area of another plot joined this plot's layout");
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    inset->addElement(rect, Qt::AlignCenter);
    QCOMPARE(test_misuse::calls.size(), std::size_t(2));
    test_misuse::calls.clear();

    QVERIFY2(rect->layout() == owner.plotLayout(), "the area left its own layout");
    QCOMPARE(rect->parentPlot(), &owner);
    QCOMPARE(inset->elementCount(), 0);
    settle(other);
    settle(owner);
    QVERIFY2(rect->skew()->capabilities().testFlag(QCP::capStraightSegments),
             "the area stopped working in its own plot");
}

void TestUserSystem::aNestedLayoutCannotMoveAnElementOfAnotherPlot()
{
    QCustomPlot owner;
    SkewRect* rect = skewRect(owner);

    QCustomPlot other;
    auto* sub = new QCPLayoutGrid;
    QVERIFY(sub->addElement(0, 0, rect));
    QVERIFY2(!other.plotLayout()->addElement(0, 1, sub),
             "a plot-less layout carried another plot's element into this one");
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    test_misuse::calls.clear();

    QCOMPARE(rect->parentPlot(), &owner);
    QVERIFY2(rect->layout() == sub, "the nested refusal moved the area anyway");
    QCOMPARE(sub->layout(), static_cast<QCPLayout*>(nullptr));
    delete sub; // the plot-less layout owns the area it took in, and goes with it
    settle(owner);
    settle(other);
}

void TestUserSystem::itemsFollowTheSkewMapping()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();

    auto* text = new QCPItemText(&plot);
    text->position->setSystem(system);
    text->position->setType(QCPItemPosition::ptPlotCoords);
    text->position->setCoords(2, 2);
    text->setClipToAxisRect(true);
    text->setClipArea(rect);

    auto* line = new QCPItemLine(&plot);
    for (QCPItemPosition* position : { line->start, line->end })
    {
        position->setSystem(system);
        position->setType(QCPItemPosition::ptPlotCoords);
    }
    line->start->setCoords(2, 9);
    line->end->setCoords(5, 2);
    line->setClipToAxisRect(true);
    line->setClipArea(rect);

    auto* ratio = new QCPItemText(&plot);
    ratio->position->setType(QCPItemPosition::ptAxisRectRatio);
    ratio->position->setAxisRect(rect);
    ratio->position->setCoords(0.25, 0.75);

    settle(plot);
    const QPointF textPixel = text->position->pixelPosition();
    const QPointF linePixel = line->start->pixelPosition();
    const QPointF ratioPixel = ratio->position->pixelPosition();
    QVERIFY(near(textPixel, pixelOf(system->pixelOf(2, 2)), 1.0));
    QVERIFY(near(linePixel, pixelOf(system->pixelOf(2, 9)), 1.0));
    QVERIFY(near(ratioPixel,
                 QPointF(rect->rect().left() + 0.25 * rect->rect().width(),
                         rect->rect().top() + 0.75 * rect->rect().height()),
                 1.0));

    system->setShear(0.25);
    settle(plot);
    QVERIFY2(text->position->pixelPosition() != textPixel, "the shear left the text behind");
    QVERIFY2(line->start->pixelPosition() != linePixel, "the shear left the line behind");
    QVERIFY(near(text->position->pixelPosition(), pixelOf(system->pixelOf(2, 2)), 1.0));
    QVERIFY(near(line->start->pixelPosition(), pixelOf(system->pixelOf(2, 9)), 1.0));
    // A ratio position measures itself in the frame of the area, which the shear does not move.
    QVERIFY(near(ratio->position->pixelPosition(), ratioPixel, 0.5));
}

void TestUserSystem::theTracerFollowsASkewGraph()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    InspectableGraph* graph = graphOn(rect);
    graph->setData(QVector<double> { 2, 4, 6 }, QVector<double> { 2, 6, 4 });

    auto* tracer = new QCPItemTracer(&plot);
    tracer->setGraph(graph);
    tracer->setGraphKey(4);
    tracer->setClipToAxisRect(true);
    tracer->setClipArea(rect);
    settle(plot);

    QCOMPARE(tracer->position->system(), system);
    QVERIFY(near(tracer->position->pixelPosition(), pixelOf(system->pixelOf(4, 6)), 1.5));

    system->setShear(0.4);
    settle(plot);
    QVERIFY2(near(tracer->position->pixelPosition(), pixelOf(system->pixelOf(4, 6)), 1.5),
             "the tracer kept a pixel of the frame it was given first");
}

void TestUserSystem::anAnchoredPositionOnASkewSystemResolvesThroughTheFrame()
{
    {
        QCustomPlot plot;
        SkewRect* rect = skewRect(plot);
        SkewSystem* system = rect->skew();

        auto* anchor = new QCPItemText(&plot);
        anchor->position->setSystem(system);
        anchor->position->setType(QCPItemPosition::ptPlotCoords);
        anchor->position->setCoords(2, 2);

        auto* anchored = new QCPItemText(&plot);
        anchored->position->setSystem(system);
        QVERIFY(test_misuse::calls.empty());
        anchored->position->setParentAnchor(anchor->position);
        QVERIFY2(test_misuse::calls.empty(), "setParentAnchor reported");
        anchored->position->setType(QCPItemPosition::ptPlotCoords);
        QVERIFY2(test_misuse::calls.empty(), "setType reported");
        anchored->position->setCoords(0.5, 0.5);
        QVERIFY2(test_misuse::calls.empty(), "setCoords reported");
        settle(plot);
        QVERIFY2(test_misuse::calls.empty(), "settle reported");

        QVERIFY2(near(anchored->position->pixelPosition(),
                      pixelOf(system->pixelOf(2.5, 2.5)), 1.0),
                 "an anchored plot-coordinate position fell into the silent per-slot path");

        anchored->position->setPixelPosition(pixelOf(system->pixelOf(4, 4)));
        QVERIFY2(test_misuse::calls.empty(), "setPixelPosition reported");
        QVERIFY2(near(anchored->position->coords(), QPointF(2, 2), 0.01),
                 "setPixelPosition did not invert the anchored frame");
        QVERIFY2(near(anchored->position->pixelPosition(), pixelOf(system->pixelOf(4, 4)), 1.0),
                 "the anchored frame round trip moved the position");
    }
    // Tearing the items down detaches the anchor: that must not report either.
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
}

void TestUserSystem::takeAndAdoptAGraphAndAnItemOnASkewRect()
{
    QCustomPlot from;
    SkewRect* rect = skewRect(from);
    InspectableGraph* graph = graphOn(rect);
    graph->setData(QVector<double> { 1, 9 }, QVector<double> { 1, 9 });
    QCOMPARE(graph->system(), rect->skew());
    auto* line = new QCPItemLine(&from);
    line->start->setSystem(rect->skew());
    line->end->setSystem(rect->skew());
    line->start->setType(QCPItemPosition::ptPlotCoords);
    line->end->setType(QCPItemPosition::ptPlotCoords);
    line->start->setCoords(1, 1);
    line->end->setCoords(9, 9);
    settle(from);

    std::unique_ptr<QCPGraph> takenGraph = from.take(QCPHandle<QCPGraph>(graph));
    std::unique_ptr<QCPItemLine> takenLine = from.take(QCPHandle<QCPItemLine>(line));
    QVERIFY(takenGraph);
    QVERIFY(takenLine);
    QVERIFY(!takenGraph->system());

    QCustomPlot to;
    auto movedGraph = to.add(std::move(takenGraph));
    auto movedLine = to.add(std::move(takenLine));
    QVERIFY(movedGraph);
    QVERIFY(movedLine);
    // Adopting rebinds to the plot's own axes: the 2.x cartesian frame, never the skew one.
    QCOMPARE(movedGraph->keyAxis(), to.xAxis);
    QCOMPARE(movedGraph->system(), to.xAxis->axisRect()->system(to.xAxis, to.yAxis));
    QCOMPARE(movedLine->start->keyAxis(), to.xAxis);
    QCOMPARE(movedLine->start->coords(), QPointF(1, 1));
    QVERIFY(movedLine->start->system() != rect->skew());
    settle(to);
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
}

void TestUserSystem::anAxisDeletedMidDragLeavesThemInert()
{
    // A slot that answers a mapping change by deleting an axis, exactly as a drag could: whatever
    // the slot does, the plottables of the dying system draw nothing and say so once.
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    QCPAxis* keyAxis = rect->axis(QCPAxis::atBottom);
    InspectableGraph* graph = graphOn(rect);
    graph->setData(QVector<double> { 2, 5 }, QVector<double> { 2, 5 });
    InspectableCurve* curve = curveOn(rect);
    curve->setData(QVector<double> { 2, 5 }, QVector<double> { 2, 5 });
    auto* text = new QCPItemText(&plot);
    text->position->setSystem(system);
    text->position->setType(QCPItemPosition::ptPlotCoords);
    text->position->setCoords(5, 5);
    text->setClipToAxisRect(true);
    text->setClipArea(rect);
    settle(plot);

    QObject::connect(system, &QCPCoordinateSystem::changed, system,
                     [rect, keyAxis] { rect->removeAxis(keyAxis); });
    system->setShear(0.5);

    QVERIFY2(!rect->axes().contains(keyAxis), "the slot did not delete the axis");
    QVERIFY(!graph->system());
    QVERIFY(!curve->system());
    QVERIFY(!text->position->system());
    settle(plot);
    // One report per plottable: the graph and the curve. The inert item is silent, as a pixel
    // read of a position without a frame is.
    QCOMPARE(test_misuse::calls.size(), std::size_t(2));
    test_misuse::calls.clear();
}

void TestUserSystem::deletingTheSkewRectLeavesThemInert()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    InspectableGraph* graph = graphOn(rect);
    graph->setData(QVector<double> { 2, 5 }, QVector<double> { 2, 5 });
    InspectableCurve* curve = curveOn(rect);
    curve->setData(QVector<double> { 2, 5 }, QVector<double> { 2, 5 });
    auto* text = new QCPItemText(&plot);
    text->position->setSystem(rect->skew());
    text->position->setType(QCPItemPosition::ptPlotCoords);
    text->position->setCoords(5, 5);
    settle(plot);

    delete plot.plotLayout()->takeAt(0); // the rect, with its axes and its systems

    QVERIFY(!graph->system());
    QVERIFY(!curve->system());
    QVERIFY(!text->position->system());
    settle(plot);
    // One report per plottable: the graph and the curve. The inert item is silent, as a pixel
    // read of a position without a frame is.
    QCOMPARE(test_misuse::calls.size(), std::size_t(2));
    test_misuse::calls.clear();
    QCOMPARE(text->position->pixelPosition(), QPointF(0, 0));
}

void TestUserSystem::switchingBetweenCartesianAndSkewReRecords()
{
    QCustomPlot plot;
    TwoRects rects = twoRects(plot);
    auto* curve = new InspectableCurve(rects.cartesian->axis(QCPAxis::atBottom),
                                       rects.cartesian->axis(QCPAxis::atLeft));
    curve->setLineStyle(QCPCurve::lsLine);
    curve->setPen(QPen(Qt::black, 3));
    curve->setData(QVector<double> { 2, 5 }, QVector<double> { 2, 5 });
    settle(plot);

    const int cartesianRecordings = curve->recordings();
    curve->setSystem(rects.skew->skew());
    settle(plot);
    QCOMPARE(curve->system(), rects.skew->skew());
    QVERIFY2(curve->recordings() == cartesianRecordings + 1,
             "a system switch reused the recording of the frame it left");
    QVERIFY(lightnessAt(render(plot), pixelOf(rects.skew->skew()->pixelOf(5, 5))) < 128);

    curve->setSystem(rects.cartesian->system(rects.cartesian->axis(QCPAxis::atBottom),
                                             rects.cartesian->axis(QCPAxis::atLeft)));
    settle(plot);
    QVERIFY(curve->recordings() == cartesianRecordings + 2);
    QCOMPARE(curve->keyAxis(), rects.cartesian->axis(QCPAxis::atBottom));
}

void TestUserSystem::theLegendIconOfASkewGraphDraws()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    InspectableGraph* graph = graphOn(rect);
    graph->setData(QVector<double> { 2, 5 }, QVector<double> { 2, 5 });
    settle(plot);

    QImage icon(40, 30, QImage::Format_ARGB32_Premultiplied);
    icon.fill(Qt::white);
    QCPPainter painter(&icon);
    graph->icon(&painter, QRectF(0, 0, 40, 30));
    painter.end();

    int inked = 0;
    for (int y = 0; y < icon.height(); ++y)
        for (int x = 0; x < icon.width(); ++x)
            if (QColor(icon.pixel(x, y)).lightness() < 128)
                ++inked;
    QVERIFY2(inked > 0, "the legend icon of a graph on a user system drew nothing");
}

void TestUserSystem::aSkewItemIsAlwaysAnIndexCandidate()
{
    // The documented limit of J-6: the item index culls cartesian systems only, so an item on a
    // user system is always handed to the hit test - correct, and linear in the items.
    QCustomPlot plot;
    TwoRects rects = twoRects(plot);
    auto* skewItem = new QCPItemLine(&plot);
    skewItem->start->setSystem(rects.skew->skew());
    skewItem->end->setSystem(rects.skew->skew());
    auto* cartesianItem = new QCPItemLine(&plot);
    cartesianItem->start->setSystem(rects.cartesian->system(rects.cartesian->axis(QCPAxis::atBottom),
                                                            rects.cartesian->axis(QCPAxis::atLeft)));
    cartesianItem->end->setSystem(cartesianItem->start->system());
    settle(plot);

    QCPLayer* layer = skewItem->layer();
    QCOMPARE(cartesianItem->layer(), layer);
    const std::vector<std::uint8_t>& candidates = layer->itemCandidates(QRectF(0, 0, 1, 1));
    const QList<QCPLayerable*> children = layer->children();
    QCOMPARE(int(candidates.size()), children.size());
    bool sawSkew = false;
    bool sawCartesian = false;
    for (int i = 0; i < children.size(); ++i)
    {
        if (children.at(i) == skewItem)
        {
            sawSkew = true;
            QCOMPARE(int(candidates[std::size_t(i)]), 1);
        }
        if (children.at(i) == cartesianItem)
        {
            sawCartesian = true;
            QCOMPARE(int(candidates[std::size_t(i)]), 0);
        }
    }
    QVERIFY(sawSkew && sawCartesian);
}

void TestUserSystem::aPolarGraphIsRectSelected()
{
    // A pin: QCPPolarGraph has had its own selectTestRect since before 4j. The user-system branch
    // this milestone added to QCPAbstractPlottable1D never serves it.
    QCustomPlot plot;
    QCPPolarArea* area = polarArea(plot);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 90, 180 },
                                          QVector<double> { 1.0, 0.5, 1.0 });
    settle(plot);

    const QPointF pixel = graph->coordsToPixels(90, 0.5);
    const QCPDataSelection selection =
            graph->selectTestRect(QRectF(pixel - QPointF(6, 6), QSizeF(12, 12)), true);
    QCOMPARE(selection.dataRangeCount(), 1);
    QCOMPARE(selection.dataRange(0), QCPDataRange(1, 2));
}
