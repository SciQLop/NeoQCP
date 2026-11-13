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
#include <cmath>
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

//! The graph component 0 is filled to, or null.
QCPGraph* fillPartner(const QCPGraph* graph)
{
    const auto fill = graph->componentFill(0);
    const auto* toGraph = fill ? std::get_if<QCPGraph::FillToGraph>(&fill->target) : nullptr;
    return toGraph ? toGraph->graph.data() : nullptr;
}

//! The multigraph with the two facts a task-2 test reads: its recording and its pipeline state.
class InspectableMultiGraph : public QCPGraph
{
public:
    using QCPGraph::QCPGraph;

    [[nodiscard]] int recordings() const { return mCore.recordingCount(); }
    [[nodiscard]] const QCPDrawList& recording() const { return mCore.recordedList(); }
    [[nodiscard]] bool hasBins() const { return mL1Cache != nullptr || mL2Result != nullptr; }
    [[nodiscard]] bool hasL1() const { return mL1Cache != nullptr; }
    [[nodiscard]] bool needsResampling() const { return mNeedsResampling; }
};

InspectableMultiGraph* multiGraphOn(SkewRect* rect)
{
    auto* graph = new InspectableMultiGraph(rect->axis(QCPAxis::atBottom),
                                            rect->axis(QCPAxis::atLeft));
    graph->setLineStyle(QCPGraph::lsLine);
    graph->setPen(QPen(Qt::black, 3));
    return graph;
}

// The colour-refusal fixture: an area that is not an axis rect, with a system a multigraph admits.
// It has no rect colour scale to join, so colour values need an explicit one.
class FlatAxis : public QCPAxisBase
{
public:
    FlatAxis(QCPArea* area, Qt::Orientation orientation) : QCPAxisBase(area, orientation) { }

    void draw(QCPPainter*) override { }
    void applyDefaultAntialiasingHint(QCPPainter*) const override { }
    [[nodiscard]] SelectablePart getPartAt(const QPointF&) const override { return spNone; }
};

class FlatSystem : public QCPCoordinateSystem
{
public:
    FlatSystem(QCPArea* area, QCPAxisBase* keyAxis, QCPAxisBase* valueAxis)
            : QCPCoordinateSystem(area, keyAxis, valueAxis)
    {
    }

    QCPTransform transform() const override { return QCPTransform(); }
    QRect clipRect() const override { return area()->rect(); }
    QCP::Capabilities capabilities() const override { return QCP::capStraightSegments; }
};

class FlatArea : public QCPArea
{
public:
    explicit FlatArea(QCustomPlot* plot)
            : QCPArea(plot)
            , mKey(new FlatAxis(this, Qt::Horizontal))
            , mValue(new FlatAxis(this, Qt::Vertical))
    {
    }

    QList<QCPHandle<QCPAxisBase>> allAxes() const override
    {
        return { QCPHandle<QCPAxisBase>(mKey), QCPHandle<QCPAxisBase>(mValue) };
    }
    void markAffectedLayersDirty(bool /*invalidate*/) override { }
    [[nodiscard]] FlatAxis* key() const { return mKey; }
    [[nodiscard]] FlatAxis* value() const { return mValue; }

protected:
    QCPCoordinateSystem* createSystem(QCPAxisBase* key, QCPAxisBase* value) override
    {
        if (!key || !value || key == value)
            return nullptr;
        return new FlatSystem(this, key, value);
    }

private:
    FlatAxis* mKey;
    FlatAxis* mValue;
};

//! A flat area whose key axis drops its plottables: rescale() must walk the override (M1).
class DroppingAxis : public FlatAxis
{
public:
    DroppingAxis(QCPArea* area, Qt::Orientation orientation) : FlatAxis(area, orientation) { }

    QList<QCPHandle<QCPAbstractPlottable>> plottables() const override
    {
        ++mPlottablesCalls;
        return { };
    }

    //! How often rescale() asked for the plottables: the override must be the one walked (M1).
    [[nodiscard]] int plottablesCalls() const { return mPlottablesCalls; }

private:
    mutable int mPlottablesCalls = 0;
};

class DroppingArea : public QCPArea
{
public:
    explicit DroppingArea(QCustomPlot* plot)
            : QCPArea(plot)
            , mKey(new DroppingAxis(this, Qt::Horizontal))
            , mValue(new FlatAxis(this, Qt::Vertical))
    {
    }

    QList<QCPHandle<QCPAxisBase>> allAxes() const override
    {
        return { QCPHandle<QCPAxisBase>(mKey), QCPHandle<QCPAxisBase>(mValue) };
    }
    void markAffectedLayersDirty(bool /*invalidate*/) override { }
    [[nodiscard]] DroppingAxis* key() const { return mKey; }
    [[nodiscard]] FlatAxis* value() const { return mValue; }

protected:
    QCPCoordinateSystem* createSystem(QCPAxisBase* key, QCPAxisBase* value) override
    {
        if (!key || !value || key == value)
            return nullptr;
        return new FlatSystem(this, key, value);
    }

private:
    DroppingAxis* mKey;
    FlatAxis* mValue;
};

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
    [[nodiscard]] QRect rect() const { return static_cast<const QCPAxisRect*>(area().get())->rect(); }

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

//! A mapping whose local value direction changes with the pixel height: the per-bar cap case.
class CurvedMapping : public SkewMapping
{
public:
    using SkewMapping::SkewMapping;

protected:
    [[nodiscard]] double extraShift(double pixelY) const override
    {
        return 20.0 * std::sin(pixelY / 20.0);
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

void TestUserSystem::theDefaultAxisClearsBeforeItsSystemDies()
{
    QCustomPlot plot;
    QCPAxisRect* rect = plot.axisRect();
    QCPAxis* key = rect->axis(QCPAxis::atBottom);
    QVERIFY(key == plot.xAxis);
    QCPCoordinateSystem* system = rect->system(key, rect->axis(QCPAxis::atLeft));
    QVERIFY(system);
    bool sawClearedAxis = false;
    QObject::connect(system, &QObject::destroyed, [&] { sawClearedAxis = !plot.xAxis; });
    QVERIFY(rect->removeAxis(key));
    QVERIFY2(sawClearedAxis,
             "a slot on the system's destroyed() sees the default axis cleared, as in 2.x");
    QVERIFY(!plot.xAxis);
}

void TestUserSystem::rescaleWalksTheOverriddenPlottables()
{
    QCustomPlot plot;
    plot.plotLayout()->clear();
    auto* cartesian = new QCPAxisRect(&plot);
    auto* flat = new DroppingArea(&plot);
    plot.plotLayout()->addElement(0, 0, cartesian);
    plot.plotLayout()->addElement(1, 0, flat);
    cartesian->axis(QCPAxis::atBottom)->setRange(0, 10);
    cartesian->axis(QCPAxis::atLeft)->setRange(0, 10);
    settle(plot);
    QCPCoordinateSystem* flatSystem = flat->system(flat->key(), flat->value());
    QVERIFY(flatSystem);

    auto* graph = new QCPGraph(cartesian->axis(QCPAxis::atBottom),
                               cartesian->axis(QCPAxis::atLeft));
    graph->setData(QVector<double> { 0.0, 10.0 }, QVector<double> { 0.0, 1.0 });
    graph->setSystem(flatSystem);
    QCOMPARE(graph->system(), flatSystem);

    // The axis drops every plottable: rescale() must ask it, not walk the systems behind it.
    flat->key()->setRange(0, 100);
    const int before = flat->key()->plottablesCalls();
    flat->key()->rescale();
    QCOMPARE(flat->key()->plottablesCalls(), before + 1);
    QCOMPARE(flat->key()->range(), QCPRange(0, 100));
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
    std::unique_ptr<QCPGraph> multi(new QCPGraph(key, value));
    QVERIFY2(multi->requiredCapabilities() == QCP::capStraightSegments,
             "the multigraph needs only a straight segment between two data points");
    std::unique_ptr<QCPWaterfallGraph> waterfall(new QCPWaterfallGraph(key, value));
    QVERIFY2(waterfall->requiredCapabilities()
                 == (QCP::capRectangular | QCP::capStraightKey),
             "the waterfall's pixel offsets still need the rectangle and a straight key axis");

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
    InspectableMultiGraph* graph = multiGraphOn(rect);

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
    // Inside the bar at key 2, between the ticks: at key 3 the probe missed the bars and read
    // a dotted grid line, whose dash phase differs by platform.
    const QPointF inside(cartesian->axis(QCPAxis::atBottom)->coordToPixel(2.1),
                         cartesian->axis(QCPAxis::atLeft)->coordToPixel(3));
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
            rect->system(rect->axis(QCPAxis::atBottom), rect->axis(QCPAxis::atLeft)).get());
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
    InspectableMultiGraph* graph = multiGraphOn(rect);
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
    InspectableMultiGraph* graph = multiGraphOn(rect);
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
    InspectableMultiGraph* graph = multiGraphOn(rect);
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

void TestUserSystem::aLogImpulseRunsToTheBoundNearerZero()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    rect->axis(QCPAxis::atLeft)->setScaleType(QCPAxis::stLogarithmic);
    rect->axis(QCPAxis::atLeft)->setRange(1, 10);
    InspectableMultiGraph* graph = multiGraphOn(rect);
    graph->setLineStyle(QCPGraph::lsImpulse);
    graph->setData(QVector<double> { 2, 3 }, std::vector<QVector<double>> { QVector<double> { 4, 5 } });
    settle(plot);

    // Value zero has no pixel on a log axis: the impulse runs to the range bound nearer zero (2.x's
    // log baseline), never to zero.
    const QCPDrawList& recording = graph->recording();
    QVERIFY2(recordingHolds(recording, QPointF(2, 1)) && recordingHolds(recording, QPointF(3, 1)),
             "the impulse does not run to the bound nearer zero");
    QVERIFY2(!recordingHolds(recording, QPointF(2, 0)), "an impulse reached value zero on a log axis");
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
}

void TestUserSystem::aFillWithAnUnrepresentableInverseStaysFinite()
{
    QCustomPlot plot;
    auto* rect = bareRect<TunedSkewRect<NaNSkewMapping>>(plot);
    auto* system = static_cast<TunedSkewSystem<NaNSkewMapping>*>(
            rect->system(rect->axis(QCPAxis::atBottom), rect->axis(QCPAxis::atLeft)).get());
    auto* graph = new InspectableMultiGraph(rect->axis(QCPAxis::atBottom),
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


void TestUserSystem::aGraphIsHitAndRectSelectedThroughTheMapping()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    InspectableMultiGraph* graph = multiGraphOn(rect);
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
    InspectableMultiGraph* graph = multiGraphOn(rect);
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
    InspectableMultiGraph* graph = multiGraphOn(rect);
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
    InspectableMultiGraph* graph = multiGraphOn(rects.skew);
    auto* cartesian = new QCPGraph(rects.cartesian->axis(QCPAxis::atBottom),
                                   rects.cartesian->axis(QCPAxis::atLeft));

    // A5: on a user system both ends of a channel must be mapped by the same system, otherwise the
    // polygon would join two different frames.
    graph->setComponentFillToGraph(0, cartesian, 0);
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    QVERIFY2(!fillPartner(graph), "a channel joined a graph of another frame");
    test_misuse::calls.clear();

    InspectableMultiGraph* other = multiGraphOn(rects.skew);
    graph->setComponentFillToGraph(0, other, 0);
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
    QCOMPARE(fillPartner(graph), other);

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
    InspectableMultiGraph* graph = multiGraphOn(rect);
    InspectableMultiGraph* other = multiGraphOn(rect);
    graph->setData(QVector<double> { 1, 2, 3, 4, 5 }, QVector<double> { 2, 2, 2, 2, 2 });
    other->setData(QVector<double> { 1, 2, 3, 4, 5 }, QVector<double> { 8, 8, 8, 8, 8 });
    const QColor brush(255, 0, 0, 80);
    graph->setBrush(brush);
    graph->setComponentFillToGraph(0, other, 0);
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
    InspectableMultiGraph* graph = multiGraphOn(rects.skew);
    InspectableMultiGraph* other = multiGraphOn(rects.skew);
    graph->setData(QVector<double> { 2, 8 }, QVector<double> { 2, 2 });
    other->setData(QVector<double> { 2, 8 }, QVector<double> { 8, 8 });
    graph->setBrush(QColor(255, 0, 0));
    graph->setComponentFillToGraph(0, other, 0);
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
    lower->setComponentFillToGraph(0, upper, 0);
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
    QVERIFY(lower->setComponentFillToGraph(0, upper, 0));
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
    InspectableMultiGraph* graph = multiGraphOn(rect);
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

void TestUserSystem::theTracerFollowsAMultiGraphOnASkewRect()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    InspectableMultiGraph* graph = multiGraphOn(rect);
    graph->setData(QVector<double> { 2, 4, 6 },
                   std::vector<QVector<double>> { QVector<double> { 1, 1, 1 },
                                                  QVector<double> { 2, 6, 4 } });
    auto* tracer = new QCPItemTracer(&plot);
    tracer->setGraph(graph, 1);
    tracer->setGraphKey(4);
    tracer->setClipToAxisRect(true);
    tracer->setClipArea(rect);
    settle(plot);

    QCOMPARE(tracer->position->system(), system);
    QCOMPARE(tracer->component(), 1);
    QVERIFY(near(tracer->position->pixelPosition(), pixelOf(system->pixelOf(4, 6)), 1.5));

    system->setShear(0.4);
    settle(plot);
    QVERIFY2(near(tracer->position->pixelPosition(), pixelOf(system->pixelOf(4, 6)), 1.5),
             "the multigraph tracer kept a pixel of the frame it was given first");
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
    InspectableMultiGraph* graph = multiGraphOn(rect);
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
    InspectableMultiGraph* graph = multiGraphOn(rect);
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
    InspectableMultiGraph* graph = multiGraphOn(rect);
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
    InspectableMultiGraph* graph = multiGraphOn(rect);
    graph->setData(QVector<double> { 2, 5 }, std::vector<QVector<double>> { QVector<double> { 2, 5 } });
    settle(plot);

    QImage icon(40, 30, QImage::Format_ARGB32_Premultiplied);
    icon.fill(Qt::white);
    QCPPainter painter(&icon);
    graph->drawComponentLegendIcon(&painter, 0, QLineF(0, 15, 40, 15));
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
    const QList<QCPHandle<QCPLayerable>> children = layer->children();
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

void TestUserSystem::aMultiGraphJoinsASkewRect()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    InspectableMultiGraph* graph = multiGraphOn(rect);

    // P-3: a line needs a straight segment, and nothing of the 2.x rectangle.
    QCOMPARE(graph->requiredCapabilities(), QCP::capStraightSegments);
    QCOMPARE(graph->system(), static_cast<QCPCoordinateSystem*>(rect->skew()));
    QVERIFY2(!graph->keyAxis(), "a projection was handed to a multigraph on a user system");
    QVERIFY2(!graph->valueAxis(), "a projection was handed to a multigraph on a user system");
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));

    // The add<T>() route joins it too: what the capability mask is for.
    auto added = plot.add<QCPGraph>(
            QVector<double> { 1, 2 },
            std::vector<QVector<double>> { QVector<double> { 1, 2 } });
    QVERIFY(added);
    QCOMPARE(added->requiredCapabilities(), QCP::capStraightSegments);
    QCOMPARE(added->system(), static_cast<QCPCoordinateSystem*>(rect->skew()));
    settle(plot);
}

void TestUserSystem::aWaterfallIsRefusedOnASkewRect()
{
    // A guard (P-3): the offsets are pixel stacking, so a waterfall keeps the rectangle mask.
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    auto* waterfall = new QCPWaterfallGraph(rect->axis(QCPAxis::atBottom),
                                             rect->axis(QCPAxis::atLeft));
    QCOMPARE(waterfall->requiredCapabilities(),
             QCP::capRectangular | QCP::capStraightKey);
    QVERIFY2(!waterfall->system(), "a waterfall joined a skew rect");
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    test_misuse::calls.clear();
}

void TestUserSystem::aMultiGraphLineFollowsTheSkewMapping()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    InspectableMultiGraph* graph = multiGraphOn(rect);
    graph->setData(QVector<double> { 2, 4, 6 },
                   std::vector<QVector<double>> { QVector<double> { 2, 6, 3 },
                                                  QVector<double> { 7, 1, 5 } });
    settle(plot);

    const QCPDrawList& recording = graph->recording();
    QVERIFY2(!recording.commands.empty(), "the multigraph recorded nothing on the skew rect");
    for (const QPointF& point : { QPointF(2, 2), QPointF(4, 6), QPointF(6, 3),
                                  QPointF(2, 7), QPointF(4, 1), QPointF(6, 5) })
        QVERIFY2(recordingHolds(recording, point),
                 "a vertex is not recorded in data space, column by column");

    const QImage image = render(plot);
    for (const QPointF& point : { QPointF(2, 2), QPointF(4, 6), QPointF(6, 3),
                                  QPointF(2, 7), QPointF(4, 1), QPointF(6, 5) })
        QVERIFY2(lightnessAt(image, pixelOf(system->pixelOf(point.x(), point.y()))) < 200,
                 "a column's vertex is not drawn on the skew mapping");
}

void TestUserSystem::aMultiGraphScatterLandsOnTheMapping()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    InspectableMultiGraph* graph = multiGraphOn(rect);
    graph->setLineStyle(QCPGraph::lsNone);
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 6));
    graph->setData(QVector<double> { 2, 5 }, std::vector<QVector<double>> { QVector<double> { 3, 4 } });
    settle(plot);

    QVERIFY2(!graph->recording().commands.empty(), "the scatter-only multigraph recorded nothing");
    const QImage image = render(plot);
    QVERIFY2(lightnessAt(image, pixelOf(system->pixelOf(2, 3))) < 160,
             "a scatter misses its skew mapping");
    QVERIFY2(lightnessAt(image, pixelOf(system->pixelOf(5, 4))) < 160,
             "a scatter misses its skew mapping");
}

void TestUserSystem::aMultiGraphStepAndImpulseAreBuiltInDataSpace()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    InspectableMultiGraph* graph = multiGraphOn(rect);
    graph->setData(QVector<double> { 2, 4 },
                   std::vector<QVector<double>> { QVector<double> { 2, 6 },
                                                  QVector<double> { 3, 7 } });
    graph->setComponentLineStyle(0, QCPGraph::lsStepLeft);
    graph->setComponentLineStyle(1, QCPGraph::lsImpulse);
    settle(plot);

    const QCPDrawList& recording = graph->recording();
    // The step turns at (next key, previous value); the impulse runs from value zero to its point.
    for (const QPointF& point : { QPointF(4, 2), QPointF(2, 0), QPointF(2, 3), QPointF(4, 0),
                                  QPointF(4, 7) })
        QVERIFY2(recordingHolds(recording, point),
                 "a step or impulse vertex is not recorded in data space");
}

void TestUserSystem::aLargeMultiGraphOnASkewRectDrawsEveryPoint()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    InspectableMultiGraph* graph = multiGraphOn(rect);
    const int count = 100000;
    QVector<double> keys(count), values(count);
    for (int i = 0; i < count; ++i)
    {
        const double t = double(i) / double(count - 1);
        keys[i] = 5.0 * t;
        values[i] = 2.0 + 2.0 * std::sin(6.283185307179586 * t);
    }
    values.last() = 2.0;
    graph->setData(keys, std::vector<QVector<double>> { values });

    // B2: the non-cartesian branch draws the displayed source, it never waits for a resample.
    QVERIFY(graph->canProduceContent());
    QVERIFY(!graph->needsResampling());
    QVERIFY(!graph->hasBins());
    settle(plot);

    QVERIFY2(!graph->recording().commands.empty(), "the 100k multigraph drew a blank frame");
    const QPointF last = pixelOf(system->pixelOf(keys.last(), values.last()));
    QVERIFY2(rect->rect().contains(last.toPoint()), "the last point left the rect");
    QVERIFY2(lightnessAt(render(plot), last) < 200, "the last point of a 100k graph is not inked");
}

void TestUserSystem::switchingToASkewRectDropsTheBins()
{
    QCustomPlot plot;
    TwoRects rects = twoRects(plot);
    auto* graph = new InspectableMultiGraph(rects.cartesian->axis(QCPAxis::atBottom),
                                            rects.cartesian->axis(QCPAxis::atLeft));
    const int count = 100000;
    QVector<double> keys(count), values(count);
    for (int i = 0; i < count; ++i)
    {
        keys[i] = 10.0 * double(i) / double(count - 1);
        values[i] = 2.0 + 2.0 * std::sin(6.283185307179586 * double(i) / double(count - 1));
    }
    graph->setData(keys, std::vector<QVector<double>> { values });
    settle(plot);
    QTRY_VERIFY_WITH_TIMEOUT(graph->hasL1(), 30000);
    QVERIFY2(graph->needsResampling(), "a 100k cartesian graph does not resample any more");

    graph->setSystem(rects.skew->skew());
    QVERIFY2(!graph->hasBins(), "the L1/L2 bins survived a move to a user system");
    QVERIFY2(!graph->needsResampling(), "a user system still asks the pipeline to resample");
    settle(plot);
    QVERIFY2(!graph->recording().commands.empty(), "the moved graph drew nothing");

    graph->setSystem(rects.cartesian->system(rects.cartesian->axis(QCPAxis::atBottom),
                                             rects.cartesian->axis(QCPAxis::atLeft)));
    QVERIFY2(graph->needsResampling(), "a move back to cartesian kept the pipeline bypassed");
    QTRY_VERIFY_WITH_TIMEOUT(graph->hasL1(), 30000);
}

void TestUserSystem::aMultiGraphIsHitAndRectSelectedThroughTheMapping()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    InspectableMultiGraph* graph = multiGraphOn(rect);
    graph->setData(QVector<double> { 2, 5, 8 },
                   std::vector<QVector<double>> { QVector<double> { 2, 5, 8 },
                                                  QVector<double> { 8, 5, 2 } });
    settle(plot);

    const QPointF mapped = pixelOf(system->pixelOf(5, 5));
    QVariant details;
    const double hit = graph->selectTest(mapped, false, &details);
    QVERIFY2(hit >= 0.0, "the click on the sheared line is outside the system");
    QVERIFY2(hit < plot.selectionTolerance(), "the click on the sheared line misses");
    QVERIFY(details.canConvert<QVariantMap>());

    const QPointF below = mapped + QPointF(0, 60);
    QVERIFY2(rect->rect().contains(below.toPoint()), "the miss probe left the rect");
    QVERIFY2(graph->selectTest(below, false) > plot.selectionTolerance(),
             "a pixel off the sheared line is a hit");

    const QRectF band(mapped - QPointF(5, 5), QSizeF(10, 10));
    const QCPDataSelection selection = graph->selectTestRect(band, false);
    QCOMPARE(selection.dataRangeCount(), 1);
    QCOMPARE(selection.dataRange(0), QCPDataRange(1, 2));
}

void TestUserSystem::aMultiGraphDataPixelPositionIsTheMapping()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    InspectableMultiGraph* graph = multiGraphOn(rect);
    graph->setData(QVector<double> { 2, 5, 8 },
                   std::vector<QVector<double>> { QVector<double> { 3, 6, 4 },
                                                  QVector<double> { 7, 1, 9 } });
    settle(plot);

    const QVector<double> keys { 2, 5, 8 };
    const QVector<double> first { 3, 6, 4 };
    for (int i = 0; i < keys.size(); ++i)
        QVERIFY2(near(graph->dataPixelPosition(i),
                      pixelOf(system->pixelOf(keys.at(i), first.at(i))), 0.5),
                 "dataPixelPosition is not the system's mapping of the first component");
}

void TestUserSystem::aShearChangeRedrawsAMultiGraph()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    InspectableMultiGraph* graph = multiGraphOn(rect);
    graph->setData(QVector<double> { 2, 5 }, std::vector<QVector<double>> { QVector<double> { 2, 5 } });
    settle(plot);

    const QPointF before = pixelOf(system->pixelOf(5, 5));
    QVERIFY2(lightnessAt(render(plot), before) < 200, "the multigraph starts off its mapping");

    system->setShear(0.5);
    QVERIFY2(plot.isRepaintQueued(), "a shear change left every layer clean");
    flushRepaints(plot);

    const QPointF after = pixelOf(system->pixelOf(5, 5));
    QVERIFY2(QCPVector2D(after - before).length() > 5.0, "the multigraph did not move");
    const QImage image = render(plot);
    QVERIFY2(lightnessAt(image, after) < 200, "the multigraph is not replayed through the new shear");
    QCOMPARE(lightnessAt(image, before), 255);
}

void TestUserSystem::anAxisDeletedMidDragLeavesAMultiGraphInert()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    QCPAxis* keyAxis = rect->axis(QCPAxis::atBottom);
    InspectableMultiGraph* graph = multiGraphOn(rect);
    graph->setData(QVector<double> { 2, 5 }, std::vector<QVector<double>> { QVector<double> { 2, 5 } });
    settle(plot);
    test_misuse::calls.clear(); // the assertion is about the removal's report

    QObject::connect(system, &QCPCoordinateSystem::changed, system,
                     [rect, keyAxis] { rect->removeAxis(keyAxis); });
    system->setShear(0.5);

    QVERIFY2(!rect->axes().contains(keyAxis), "the slot did not delete the axis");
    QVERIFY2(!graph->system(), "the multigraph kept a system whose axis was deleted");
    settle(plot);
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    test_misuse::calls.clear();
}

void TestUserSystem::aMultiGraphFillFollowsTheSkewMapping()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    InspectableMultiGraph* graph = multiGraphOn(rect);
    graph->setData(QVector<double> { 2, 4, 6 }, std::vector<QVector<double>> { QVector<double> { 2, 6, 3 } });
    const QColor brush(255, 0, 0, 80);
    QVERIFY2(graph->setComponentFill(0, 0.0, brush), "a fill was refused on a skew rect");
    settle(plot);

    // The fill runs from the line down to value zero, in data space: its base pixel follows the
    // skew mapping like any other vertex.
    const QImage image = render(plot);
    const QPointF inside = pixelOf(system->pixelOf(4, 1));
    QVERIFY2(rect->rect().contains(inside.toPoint()), "the fill sample lies outside the rect");
    QVERIFY2(colourNear(image.pixelColor(inside.toPoint()), overWhite(brush)),
             "the fill is not drawn on the skew mapping");
    QCOMPARE(lightnessAt(image, pixelOf(system->pixelOf(4, 7.5))), 255);
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
}

void TestUserSystem::aMultiGraphFillBetweenColumnsOnASkewRect()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    InspectableMultiGraph* graph = multiGraphOn(rect);
    graph->setData(QVector<double> { 2, 4, 6 },
                   std::vector<QVector<double>> { QVector<double> { 2, 2, 2 },
                                                  QVector<double> { 8, 8, 8 } });
    const QColor brush(220, 40, 40, 80);
    QVERIFY2(graph->setComponentFillBetween(0, 1, brush),
             "a fill between columns was refused on a skew rect");
    settle(plot);

    const QImage image = render(plot);
    const QPointF inside = pixelOf(system->pixelOf(4, 5));
    QVERIFY(rect->rect().contains(inside.toPoint()));
    QVERIFY2(colourNear(image.pixelColor(inside.toPoint()), overWhite(brush)),
             "the band between the two columns is not drawn on the skew mapping");
    QCOMPARE(lightnessAt(image, pixelOf(system->pixelOf(4, 0.5))), 255);
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
}

void TestUserSystem::aFillToAnotherGraphOnASkewRect()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    InspectableMultiGraph* owner = multiGraphOn(rect);
    auto* partner = new QCPGraph(rect->axis(QCPAxis::atBottom),
                                      rect->axis(QCPAxis::atLeft));
    owner->setData(QVector<double> { 2, 4, 6 }, QVector<double> { 2, 6, 3 });
    partner->setData(QVector<double> { 2, 4, 6 }, QVector<double> { 8, 9, 5 });
    owner->setComponentPens({ QPen(Qt::NoPen) });
    partner->setComponentPens({ QPen(Qt::NoPen) });

    const QColor brush(40, 220, 40, 80);
    QVERIFY2(owner->setComponentFillToGraph(0, partner, 0, brush),
             "a fill to another graph was refused on a skew rect");
    settle(plot);

    const QImage image = render(plot);
    const QPointF inside = pixelOf(system->pixelOf(4, 7));
    QVERIFY(rect->rect().contains(inside.toPoint()));
    QVERIFY2(colourNear(image.pixelColor(inside.toPoint()), overWhite(brush)),
             "the band to another graph is not drawn on the skew mapping");
    QCOMPARE(lightnessAt(image, pixelOf(system->pixelOf(4, 0.5))), 255);
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));

    // The replay maps the band like the lines: both graphs on the one skew system.
    bool sawBand = false;
    for (const qcp::QCPDrawCommand& command : owner->recording().commands)
    {
        const auto* fill = std::get_if<qcp::FillBetweenCmd>(&command);
        if (!fill)
            continue;
        sawBand = true;
        QCOMPARE(fill->upper->size(), fill->lower->size());
    }
    QVERIFY2(sawBand, "the fill to another graph recorded no band");
}

void TestUserSystem::aMultiGraphFillOnALogAxisStopsAtTheBoundNearerZero()
{
    for (const bool negative : { false, true })
    {
        QCustomPlot plot;
        SkewRect* rect = skewRect(plot);
        QCPAxis* value = rect->axis(QCPAxis::atLeft);
        value->setScaleType(QCPAxis::stLogarithmic);
        value->setRange(negative ? QCPRange(-10, -1) : QCPRange(1, 10));
        InspectableMultiGraph* graph = multiGraphOn(rect);
        graph->setData(QVector<double> { 2, 4 },
                       std::vector<QVector<double>> {
                           QVector<double> { negative ? -4.0 : 4.0, negative ? -8.0 : 8.0 } });
        QVERIFY(graph->setComponentFill(0, 0.0, QColor(255, 0, 0, 80)));
        settle(plot);

        const double baseline = negative ? -1.0 : 1.0;
        bool sawFill = false;
        for (const qcp::QCPDrawCommand& command : graph->recording().commands)
        {
            const auto* fill = std::get_if<qcp::FillBetweenCmd>(&command);
            if (!fill)
                continue;
            sawFill = true;
            for (const QPointF& point : *fill->lower)
                if (std::isfinite(point.y()))
                    QCOMPARE(point.y(), baseline);
            for (const QPointF& point : *fill->upper)
                QVERIFY2(point.y() != 0.0, "the fill closed on zero, which a log axis cannot place");
        }
        QVERIFY2(sawFill, "the log fill recorded no band");
        QCOMPARE(test_misuse::calls.size(), std::size_t(0));
    }
}

void TestUserSystem::aMultiGraphNaNSplitsLineAndFill()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    InspectableMultiGraph* graph = multiGraphOn(rect);
    graph->setData(QVector<double> { 2, 3, 4, 5, 6 },
                   std::vector<QVector<double>> { QVector<double> { 2, 4, qQNaN(), 6, 7 } });
    QVERIFY(graph->setComponentFill(0, 0.0, QColor(255, 0, 0, 80)));
    settle(plot);

    bool sawNaN = false;
    for (const qcp::QCPDrawCommand& command : graph->recording().commands)
        if (const auto* line = std::get_if<qcp::PolylineDataCmd>(&command))
            for (const QPointF& point : *line->points)
                if (qIsNaN(point.y()))
                    sawNaN = true;
    QVERIFY2(sawNaN, "the NaN row is not a gap in the recorded line");

    // The fill pinches shut at the gap: no band is painted across the missing row.
    const QImage image = render(plot);
    QCOMPARE(lightnessAt(image, pixelOf(system->pixelOf(4, 1))), 255);
    QVERIFY2(colourNear(image.pixelColor(pixelOf(system->pixelOf(2.5, 1)).toPoint()),
                        overWhite(QColor(255, 0, 0, 80))),
             "the fill before the gap is missing");
    QVERIFY2(colourNear(image.pixelColor(pixelOf(system->pixelOf(5.5, 1)).toPoint()),
                        overWhite(QColor(255, 0, 0, 80))),
             "the fill after the gap is missing");
}

void TestUserSystem::multiGraphErrorBarCapsFollowTheMapping()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    SkewSystem* system = rect->skew();
    InspectableMultiGraph* graph = multiGraphOn(rect);
    graph->setData(QVector<double> { 2, 4 },
                   std::vector<QVector<double>> { QVector<double> { 4, 4 },
                                                  QVector<double> { 2, 2 },
                                                  QVector<double> { 6, 6 } });
    QVERIFY2(graph->setComponentErrorBars(0, 1, 2, 12.0),
             "error bars were refused on a skew rect");
    graph->setComponentVisible(1, false); // the bounds are the bars' inputs, not lines of their own
    graph->setComponentVisible(2, false);
    settle(plot);

    bool sawBars = false;
    for (const qcp::QCPDrawCommand& command : graph->recording().commands)
    {
        const auto* bars = std::get_if<qcp::ErrorBarsCmd>(&command);
        if (!bars)
            continue;
        sawBars = true;
        QVERIFY2(bars->ends, "the skew bars did not use the ends form");
        QVERIFY2(!bars->alongKeys, "a value error bar runs along the values");
        QCOMPARE(bars->whiskerWidth, 12.0);
        QCOMPARE(bars->symbolGap, 0.0);
    }
    QVERIFY2(sawBars, "the skew graph recorded no error bars");

    // A cap is 12 pixels long and perpendicular to the mapped bar, for each bar.
    const QImage image = render(plot);
    for (const double key : { 2.0, 4.0 })
    {
        const QPointF centre = pixelOf(system->pixelOf(key, 4));
        const QPointF upper = pixelOf(system->pixelOf(key, 6));
        const QPointF along = (upper - centre) / QCPVector2D(upper - centre).length();
        const QPointF across(-along.y(), along.x());
        int inked = 0;
        for (double t = -8.0; t <= 8.0; t += 1.0)
            if (lightnessAt(image, upper + across * t) < 200)
                ++inked;
        QVERIFY2(inked >= 8 && inked <= 17,
                 qPrintable(QStringLiteral("a cap covers %1 pixels, not about 12").arg(inked)));
        // A whisker drawn along a fixed screen direction would not be perpendicular to the shear.
        QVERIFY2(lightnessAt(image, upper + QPointF(8, 0)) > 200,
                 "the whisker is not perpendicular to the mapped bar");
    }
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
}

void TestUserSystem::multiGraphErrorBarCapsPerBarOnANonlinearMapping()
{
    // A9/I2: the cap direction comes from the bar's own ends, not from the first bar. The curved
    // mapping makes the local value direction differ between the two bars.
    QCustomPlot plot;
    plot.setBackground(QBrush(Qt::white));
    plot.plotLayout()->clear();
    auto* rect = new TunedSkewRect<CurvedMapping>(&plot);
    plot.plotLayout()->addElement(0, 0, rect);
    rect->axis(QCPAxis::atBottom)->setRange(0, 10);
    rect->axis(QCPAxis::atLeft)->setRange(0, 10);
    settle(plot);

    auto* graph = new InspectableMultiGraph(rect->axis(QCPAxis::atBottom),
                                            rect->axis(QCPAxis::atLeft));
    graph->setData(std::vector<double>{2, 6},
                   std::vector<std::vector<double>>{{1, 9}, {0.8, 8.8}, {1.2, 9.2}});
    QVERIFY(graph->setComponentErrorBars(0, 1, 2, 12.0));
    graph->setComponentVisible(1, false);
    graph->setComponentVisible(2, false);
    settle(plot);

    const qcp::ErrorBarsCmd* bars = nullptr;
    for (const qcp::QCPDrawCommand& command : graph->recording().commands)
        if (const auto* candidate = std::get_if<qcp::ErrorBarsCmd>(&command))
            bars = candidate;
    QVERIFY2(bars && bars->ends, "the data-space bars did not use the ends form");

    const QCPDrawFrame frame { graph->system()->transform(), QCPRatioTransform(),
                               rect->rect() };
    std::vector<QPointF> poly;
    qcpErrorBarPixels(frame, *bars, poly);
    QCOMPARE(int(poly.size()), 24); // two bars, two sides: backbone, cap, gap each

    const auto unit = [](QPointF v)
    {
        const double length = std::hypot(v.x(), v.y());
        return length > 0.0 ? v / length : QPointF();
    };
    const auto dot = [](const QPointF& a, const QPointF& b)
    { return a.x() * b.x() + a.y() * b.y(); };

    const auto barDirection = [&poly, &unit](int bar)
    { return unit(poly[std::size_t(bar) * 12 + 7] - poly[std::size_t(bar) * 12 + 1]); };
    const QPointF first = barDirection(0);
    const QPointF second = barDirection(1);
    QVERIFY2(std::abs(dot(first, second)) < 0.95,
             "the fixture's two bars do not have different local directions");

    for (int bar = 0; bar < 2; ++bar)
    {
        const std::size_t base = std::size_t(bar) * 12;
        const QPointF along = unit(poly[base + 7] - poly[base + 1]);
        for (const std::size_t cap : { std::size_t(3), std::size_t(9) })
        {
            const QPointF across = unit(poly[base + cap + 1] - poly[base + cap]);
            QVERIFY2(std::abs(dot(across, along)) < 0.1,
                     qPrintable(QStringLiteral("bar %1's cap is not perpendicular to its own "
                                               "direction (dot %2)")
                                    .arg(bar)
                                    .arg(dot(across, along))));
        }
    }
}

void TestUserSystem::aStepCentreOnALogKeyUserSystemTurnsInLogSpace()
{
    // I4: on the data-space branch the step centre must use the system key axis' log scale.
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    QCPAxis* key = rect->axis(QCPAxis::atBottom);
    key->setScaleType(QCPAxis::stLogarithmic);
    key->setRange(1, 100);
    rect->axis(QCPAxis::atLeft)->setRange(-1, 3);
    auto* graph = multiGraphOn(rect);
    graph->setData(QVector<double> { 1, 10, 100 },
                   std::vector<QVector<double>> { QVector<double> { 1, 2, 1 } });
    graph->setLineStyle(QCPGraph::lsStepCenter);
    QVERIFY(graph->setComponentFill(0, 0.0, QBrush(Qt::red)));
    settle(plot);

    bool lineFound = false;
    for (const qcp::QCPDrawCommand& command : graph->recording().commands)
        if (const auto* line = std::get_if<qcp::PolylineDataCmd>(&command))
            for (const QPointF& point : *line->points)
                lineFound = lineFound || std::abs(point.x() - std::sqrt(10.0)) < 1e-9;
    QVERIFY2(lineFound, "the step centre turned at the arithmetic midpoint, not in log space");

    bool fillFound = false;
    for (const qcp::QCPDrawCommand& command : graph->recording().commands)
        if (const auto* fill = std::get_if<qcp::FillBetweenCmd>(&command))
            for (const QPointF& point : *fill->upper)
                fillFound = fillFound || std::abs(point.x() - std::sqrt(10.0)) < 1e-9;
    QVERIFY2(fillFound, "the fill stepped at the arithmetic midpoint, not in log space");
}

void TestUserSystem::colourByScalarJoinsTheSkewRectsScale()
{
    QCustomPlot plot;
    SkewRect* rect = skewRect(plot);
    InspectableMultiGraph* graph = multiGraphOn(rect);
    graph->setData(QVector<double> { 2, 4, 6 }, std::vector<QVector<double>> { QVector<double> { 2, 6, 3 } });
    graph->setColorValues(std::vector<double> { 0.0, 0.5, 1.0 });
    settle(plot);

    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
    QVERIFY2(graph->hasColorValues(), "colour values were refused on a skew rect");
    QVERIFY2(graph->colorScale(), "the multigraph did not join the skew rect's colour scale");
    QCOMPARE(graph->colorScale(), rect->colorScale().get());

    std::shared_ptr<const std::vector<QRgb>> colours;
    for (const qcp::QCPDrawCommand& command : graph->recording().commands)
        if (const auto* line = std::get_if<qcp::PolylineDataCmd>(&command); line && line->colours)
        {
            colours = line->colours;
            break;
        }
    QVERIFY2(colours && colours->size() >= 2, "the coloured line recorded one colour");
    QVERIFY2((*colours).front() != (*colours).back(),
             "the whole line took one colour from the scalar mapper");
}

void TestUserSystem::colourScaleFollowsASystemMove()
{
    // A9/I6: moving a coloured graph inside one plot re-joins the new area's colour scale.
    QCustomPlot plot;
    TwoRects rects = twoRects(plot);
    QCPColorScale* scaleA = rects.cartesian->colorScale().get();
    QCPColorScale* scaleB = rects.skew->colorScale().get();
    QVERIFY(scaleA && scaleB);
    scaleA->setDataRange(QCPRange(0, 1));
    scaleB->setDataRange(QCPRange(0, 100));

    auto* graph = new InspectableMultiGraph(rects.cartesian->axis(QCPAxis::atBottom),
                                            rects.cartesian->axis(QCPAxis::atLeft));
    graph->setData(QVector<double> { 1, 2, 3 },
                   std::vector<QVector<double>> { QVector<double> { 1, 2, 1 } });
    graph->setColorValues(std::vector<double> { 0.0, 0.5, 1.0 });
    QCOMPARE(graph->colorScale(), scaleA);

    graph->setSystem(rects.skew->skew());
    QCOMPARE(graph->colorScale(), scaleB);
    QCOMPARE(graph->dataRange(), QCPRange(0, 100));
    QVERIFY2(!scaleA->users().contains(graph), "the graph stayed a user of the old rect's scale");

    // A skew rect is an axis rect too: a second move keeps following.
    graph->setSystem(rects.cartesian->system(rects.cartesian->axis(QCPAxis::atBottom),
                                             rects.cartesian->axis(QCPAxis::atLeft)));
    QCOMPARE(graph->colorScale(), scaleA);
    QCOMPARE(graph->dataRange(), QCPRange(0, 1));
}

void TestUserSystem::setColorScaleOnTheBaseIsRefused()
{
    // M4: the Q_PROPERTY setter is declared on the base and non-virtual before the fix, so a
    // property write (or a base-pointer call) could remove the only scale a non-rect area needs.
    QCustomPlot plot;
    plot.plotLayout()->clear();
    auto* cartesian = new QCPAxisRect(&plot);
    auto* flat = new FlatArea(&plot);
    plot.plotLayout()->addElement(0, 0, cartesian);
    plot.plotLayout()->addElement(1, 0, flat);
    cartesian->axis(QCPAxis::atBottom)->setRange(0, 10);
    cartesian->axis(QCPAxis::atLeft)->setRange(0, 10);
    settle(plot);
    QCPCoordinateSystem* flatSystem = flat->system(flat->key(), flat->value());
    QVERIFY(flatSystem);

    auto* graph = new InspectableMultiGraph(cartesian->axis(QCPAxis::atBottom),
                                            cartesian->axis(QCPAxis::atLeft));
    graph->setData(QVector<double> { 2, 5 }, std::vector<QVector<double>> { QVector<double> { 2, 5 } });
    auto* scale = new QCPColorScale(&plot);
    graph->setColorScale(scale);
    graph->setColorValues(std::vector<double> { 0.0, 1.0 });
    graph->setSystem(flatSystem);
    QCOMPARE(graph->system(), flatSystem);
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));

    QCPAbstractColorMapped* base = graph;
    base->setColorScale(nullptr);
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    QCOMPARE(graph->colorScale(), scale);
    test_misuse::calls.clear();

    graph->setProperty("colorScale", QVariant::fromValue<QCPColorScale*>(nullptr));
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    QCOMPARE(graph->colorScale(), scale);
    test_misuse::calls.clear();
}

void TestUserSystem::aMultiGraphColourWithoutAScaleIsRefused()
{
    // A straight-segment area that is not an axis rect has no rect colour scale to join.
    QCustomPlot plot;
    plot.plotLayout()->clear();
    auto* cartesian = new QCPAxisRect(&plot);
    auto* flat = new FlatArea(&plot);
    plot.plotLayout()->addElement(0, 0, cartesian);
    plot.plotLayout()->addElement(1, 0, flat);
    cartesian->axis(QCPAxis::atBottom)->setRange(0, 10);
    cartesian->axis(QCPAxis::atLeft)->setRange(0, 10);
    settle(plot);
    QCPCoordinateSystem* flatSystem = flat->system(flat->key(), flat->value());
    QVERIFY(flatSystem);

    auto* graph = new InspectableMultiGraph(cartesian->axis(QCPAxis::atBottom),
                                            cartesian->axis(QCPAxis::atLeft));
    graph->setData(QVector<double> { 2, 5 }, std::vector<QVector<double>> { QVector<double> { 2, 5 } });
    graph->setSystem(flatSystem);
    QCOMPARE(graph->system(), flatSystem);
    test_misuse::calls.clear();

    graph->setColorValues(std::vector<double> { 0.0, 1.0 });
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    QVERIFY2(!graph->hasColorValues(), "colour values joined an area with no colour scale");
    test_misuse::calls.clear();

    auto* scale = new QCPColorScale(&plot);
    graph->setColorScale(scale);
    graph->setColorValues(std::vector<double> { 0.0, 1.0 });
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
    QVERIFY2(graph->hasColorValues(), "an explicit colour scale did not admit the values");

    // The join refuses too: colour values, no scale, non-rect area.
    auto* other = new InspectableMultiGraph(cartesian->axis(QCPAxis::atBottom),
                                            cartesian->axis(QCPAxis::atLeft));
    other->setData(QVector<double> { 2, 5 }, std::vector<QVector<double>> { QVector<double> { 2, 5 } });
    other->setColorValues(std::vector<double> { 0.0, 1.0 });
    QVERIFY2(other->colorScale(), "the cartesian graph did not join its rect's scale");
    other->setColorScale(nullptr);
    QVERIFY(!other->colorScale());
    QCPCoordinateSystem* own = other->system();
    other->setSystem(flatSystem);
    QCOMPARE(other->system(), own);
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    test_misuse::calls.clear();
}

void TestUserSystem::aDecoratedMultiGraphJoinsASkewRect()
{
    // Fills, error bars and colour values all follow the graph to a straight-segment user system.
    QCustomPlot plot;
    TwoRects rects = twoRects(plot);
    QCPAxis* key = rects.cartesian->axis(QCPAxis::atBottom);
    QCPAxis* value = rects.cartesian->axis(QCPAxis::atLeft);
    auto* graph = new InspectableMultiGraph(key, value);
    graph->setData(QVector<double> { 2, 3.5, 5 },
                   std::vector<QVector<double>> { QVector<double> { 2, 4, 3 },
                                                  QVector<double> { 5, 1, 6 },
                                                  QVector<double> { 1, 0.5, 0 },
                                                  QVector<double> { 3, 2, 4 } });
    QVERIFY(graph->setComponentFill(0, 0.0, QColor(40, 110, 200, 70)));
    QVERIFY(graph->setComponentFillBetween(1, 0, QColor(220, 160, 60, 70)));
    QVERIFY(graph->setComponentErrorBars(1, 2, 3, 6.0));
    graph->setColorValues(std::vector<double> { 0.0, 0.5, 1.0 });
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));

    graph->setSystem(rects.skew->skew());
    QCOMPARE(graph->system(), static_cast<QCPCoordinateSystem*>(rects.skew->skew()));
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
    settle(plot);
    QVERIFY2(!graph->recording().commands.empty(),
             "the decorated multigraph drew nothing on the skew rect");

    // The decoration must be there, not merely something: a fill, the bars and a coloured vertex.
    bool sawFill = false;
    bool sawBars = false;
    bool sawColours = false;
    for (const qcp::QCPDrawCommand& command : graph->recording().commands)
    {
        sawFill = sawFill || std::get_if<qcp::FillBetweenCmd>(&command) != nullptr;
        sawBars = sawBars || std::get_if<qcp::ErrorBarsCmd>(&command) != nullptr;
        if (const auto* line = std::get_if<qcp::PolylineDataCmd>(&command))
            sawColours = sawColours || (line->colours && line->colours->size() > 1
                                        && line->colours->front() != line->colours->back());
    }
    QVERIFY2(sawFill, "the skew frame recorded no fill");
    QVERIFY2(sawBars, "the skew frame recorded no error bars");
    QVERIFY2(sawColours, "the skew frame recorded no coloured vertices");
}
