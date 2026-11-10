#include "test-coordinate-system.h"
#include "coordinate-system-polar.h"
#include "coordinate-system.h"
#include "misuse-recorder.h"
#include "polar/angularaxis.h"
#include "polar/polar-area.h"
#include "polar/radialaxis.h"
#include "qcustomplot.h"
#include <QtTest/QtTest>

namespace
{
void settle(QCustomPlot& plot)
{
    plot.resize(400, 300);
    plot.replot();
    for (int i = 0; i < 10 && plot.isRepaintQueued(); ++i)
        QCoreApplication::processEvents();
}

QCPAxisRect* cartesianRect(QCustomPlot& plot)
{
    plot.plotLayout()->clear();
    auto* rect = new QCPAxisRect(&plot);
    plot.plotLayout()->addElement(0, 0, rect);
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
//! A plottable with one recording: enough to be drawn, hit and counted.
class RecordingPlottable : public QCPAbstractPlottable
{
public:
    explicit RecordingPlottable(QCPCoordinateSystem* system)
            : QCPAbstractPlottable(system)
    {
    }

    RecordingPlottable(QCPAxis* keyAxis, QCPAxis* valueAxis)
            : QCPAbstractPlottable(keyAxis, valueAxis)
    {
    }

    void draw(QCPDrawContext& context) const override
    {
        if (readsTransform) // a recording that bakes the frame: a new frame re-records it
        {
            const QCP::Pixels corner = context.transform().map(QCP::Data { 6, 6 });
            context.rect(QCP::Data { 2, 2 }, QCP::Pixels { corner.x, corner.y }, QPen(Qt::black),
                         QBrush(Qt::darkRed));
            return;
        }
        context.polyline(mLine, QPen(Qt::black, 2));
    }

    [[nodiscard]] int recordings() const { return mCore.recordingCount(); }

    bool readsTransform = false;

private:
    std::shared_ptr<const std::vector<QPointF>> mLine
        = std::make_shared<const std::vector<QPointF>>(
            std::initializer_list<QPointF> { { 1, 1 }, { 9, 9 } });
};

} // namespace

void TestCoordinateSystem::init()
{
    test_misuse::calls.clear();
    qcp::setMisuseHandler(test_misuse::record);
}

void TestCoordinateSystem::cleanup()
{
    qcp::setMisuseHandler(nullptr);
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
}

void TestCoordinateSystem::onePairOneSystem()
{
    QCustomPlot plot;
    QCPAxisRect* rect = cartesianRect(plot);

    QCPCoordinateSystem* once
        = rect->system(rect->axis(QCPAxis::atBottom), rect->axis(QCPAxis::atLeft));
    QCPCoordinateSystem* twice
        = rect->system(rect->axis(QCPAxis::atBottom), rect->axis(QCPAxis::atLeft));
    QVERIFY(once);
    QCOMPARE(once, twice);
    QCOMPARE(rect->systems().size(), 1);

    // The pair is ordered: the same two axes the other way round is another frame.
    QCPCoordinateSystem* swapped
        = rect->system(rect->axis(QCPAxis::atLeft), rect->axis(QCPAxis::atBottom));
    QVERIFY(swapped);
    QVERIFY(swapped != once);
    QCOMPARE(rect->systems().size(), 2);
    QVERIFY(!once->transform().keyIsVertical());
    QVERIFY(swapped->transform().keyIsVertical());

    QCOMPARE(once->area(), static_cast<QCPArea*>(rect));
    QVERIFY(once->usesAxis(rect->axis(QCPAxis::atBottom)));
    QVERIFY(!once->usesAxis(rect->axis(QCPAxis::atRight)));
}

void TestCoordinateSystem::aPairMustBelongToTheArea()
{
    QCustomPlot plot;
    QCPAxisRect* rect = cartesianRect(plot);
    auto* other = new QCPAxisRect(&plot);

    // An axis of another area, and two axes of one orientation: both refused, nothing created.
    QCOMPARE(rect->system(other->axis(QCPAxis::atBottom), rect->axis(QCPAxis::atLeft)), nullptr);
    QCOMPARE(rect->system(rect->axis(QCPAxis::atBottom), rect->axis(QCPAxis::atTop)), nullptr);
    QCOMPARE(test_misuse::calls.size(), std::size_t(2));
    QCOMPARE(rect->systems().size(), 0);
    test_misuse::calls.clear();
}

void TestCoordinateSystem::cartesianTransformIsTheAxes()
{
    QCustomPlot plot;
    QCPAxisRect* rect = cartesianRect(plot);
    QCPAxis* key = rect->axis(QCPAxis::atBottom);
    QCPAxis* value = rect->axis(QCPAxis::atLeft);
    key->setRange(2, 12);
    value->setRange(-3, 7);

    QCPCoordinateSystem* system = rect->system(key, value);
    QVERIFY(system);
    QVERIFY(system->transform() == QCPTransform(key, value));
    QCOMPARE(system->clipRect(), rect->rect());
    QCOMPARE(system->capabilities(),
             QCP::capRectangular | QCP::capStraightKey | QCP::capColourScale | QCP::capLanes);
    QVERIFY(system->ratioTransform().valid());
}

void TestCoordinateSystem::polarSystemMapsLikeTheArea()
{
    QCustomPlot plot;
    QCPPolarArea* area = polarArea(plot);
    QCPCoordinateSystem* system = area->system(area->angularAxis(), area->radialAxis());
    QVERIFY(system);
    QVERIFY(system->transform() == QCPTransform::polar(*area->angularAxis(), *area->radialAxis()));
    QCOMPARE(system->capabilities(), QCP::Capabilities());

    // The disc is the clip, and a corner of the area is outside it.
    QVERIFY(system->clipPath().contains(area->center()));
    QVERIFY(!system->contains(area->rect().topLeft()));
    QVERIFY(system->contains(area->center() + QPointF(area->radius() * 0.5, 0)));
    // The ring rotates: the angular range covers one turn, so a key is an arc at the outer radius.
    const QCPKeyWindow window = system->keyWindow();
    QCOMPARE(window.pixelsPerKey, area->radius() * 2.0 * M_PI / 360.0);
}

void TestCoordinateSystem::polarSystemRefusesAxisRatios()
{
    QCustomPlot plot;
    QCPPolarArea* area = polarArea(plot);
    QCPCoordinateSystem* system = area->system(area->angularAxis(), area->radialAxis());
    QVERIFY(system);

    // Items and spans are cartesian in this milestone: a polar frame has no axis ratio.
    const QCPRatioTransform ratio = system->ratioTransform();
    QVERIFY(!ratio.valid());
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    test_misuse::calls.clear();
}

void TestCoordinateSystem::revisionBumpsOnAnyMappingChange()
{
    QCustomPlot plot;
    QCPAxisRect* rect = cartesianRect(plot);
    QCPAxis* key = rect->axis(QCPAxis::atBottom);
    QCPCoordinateSystem* system = rect->system(key, rect->axis(QCPAxis::atLeft));
    QVERIFY(system);
    QSignalSpy changed(system, &QCPCoordinateSystem::changed);
    const quint64 start = system->revision();

    key->setRange(1, 2);
    QCOMPARE(system->revision(), start + 1);
    QCOMPARE(changed.count(), 1);

    // A layout pass that does not move the area changes nothing: recordings stay good.
    settle(plot);
    QCOMPARE(system->revision(), start + 1);
    QCOMPARE(changed.count(), 1);

    key->setScaleType(QCPAxis::stLogarithmic);
    QCOMPARE(system->revision(), start + 2);
    QCOMPARE(changed.count(), 2);

    // Deferred, but still once: the delivery comes when the scope closes.
    {
        const QCPEmissionScope scope(&plot);
        key->setRange(10, 100);
        QCOMPARE(system->revision(), start + 3);
        QCOMPARE(changed.count(), 2);
    }
    QCOMPARE(changed.count(), 3);

    // The area's geometry is part of the mapping: a resize moves every pixel.
    plot.resize(600, 500);
    plot.replot();
    QCOMPARE(system->revision(), start + 4);
    QCOMPARE(changed.count(), 4);

    // The reversal and the link change the mapping without a range change.
    key->setRangeReversed(true);
    QCOMPARE(system->revision(), start + 5);
    QCOMPARE(changed.count(), 5);

    QCustomPlot polarPlot;
    QCPPolarArea* area = polarArea(polarPlot);
    QCPCoordinateSystem* polar = area->system(area->angularAxis(), area->radialAxis());
    QVERIFY(polar);
    QSignalSpy polarChanged(polar, &QCPCoordinateSystem::changed);
    const quint64 polarStart = polar->revision();
    area->angularAxis()->setAngle(30);
    QCOMPARE(polar->revision(), polarStart + 1);
    QCOMPARE(polarChanged.count(), 1);
    area->radialAxis()->setRange(0, 2);
    QCOMPARE(polar->revision(), polarStart + 2);
    QCOMPARE(polarChanged.count(), 2);
}

void TestCoordinateSystem::aPlottableTakesASystem()
{
    QCustomPlot plot;
    QCPAxisRect* rect = cartesianRect(plot);
    QCPAxis* x = rect->axis(QCPAxis::atBottom);
    QCPAxis* y = rect->axis(QCPAxis::atLeft);
    QCPCoordinateSystem* system = rect->system(x, y);

    auto* plottable = new RecordingPlottable(system);
    QCOMPARE(plottable->system(), system);
    QCOMPARE(plottable->keyAxis(), x);
    QCOMPARE(plottable->valueAxis(), y);
    QCOMPARE(system->plottables().size(), 1);

    // The axis pair resolves to the same one, and the projections follow the system.
    auto* sibling = new RecordingPlottable(x, y);
    QCOMPARE(sibling->system(), system);
    QCOMPARE(rect->systems().size(), 1);
}

void TestCoordinateSystem::twoConstructorsShareTheSystem()
{
    QCustomPlot plot;
    QCPAxisRect* rect = cartesianRect(plot);
    QCPAxis* x = rect->axis(QCPAxis::atBottom);
    QCPAxis* y = rect->axis(QCPAxis::atLeft);

    std::unique_ptr<QCPGraph> graph(new QCPGraph(x, y));
    std::unique_ptr<QCPCurve> curve(new QCPCurve(x, y));
    QVERIFY(graph->system());
    QCOMPARE(graph->system(), curve->system());
    QCOMPARE(curve->system(), rect->system(x, y));
    QCOMPARE(rect->systems().size(), 1);
}

void TestCoordinateSystem::movingBetweenSystemsReRecords()
{
    QCustomPlot plot;
    QCPAxisRect* rect = cartesianRect(plot);
    QCPAxis* x = rect->axis(QCPAxis::atBottom);
    QCPAxis* other = rect->addAxis(QCPAxis::atBottom);
    QCPAxis* y = rect->axis(QCPAxis::atLeft);
    auto* plottable = new RecordingPlottable(x, y);
    plottable->readsTransform = true;
    plot.replot();
    const int before = plottable->recordings();
    QVERIFY(before > 0);

    plottable->setSystem(rect->system(other, y));
    QCOMPARE(plottable->keyAxis(), other);
    plot.replot();
    QCOMPARE(plottable->recordings(), before + 1); // the new frame's recording

    const int moved = plottable->recordings();
    other->setRange(-2, 8);
    plot.replot();
    QCOMPARE(plottable->recordings(), moved + 1); // the key axis it reads now

    const int settled = plottable->recordings();
    x->setRange(-3, 12);
    plot.replot();
    QCOMPARE(plottable->recordings(), settled); // the one it left
}

void TestCoordinateSystem::aPlottableOfADeletedSystemIsInert()
{
    QCustomPlot plot;
    QCPAxisRect* rect = cartesianRect(plot);
    QCPAxis* x = rect->axis(QCPAxis::atBottom);
    auto* plottable = new RecordingPlottable(x, rect->axis(QCPAxis::atLeft));
    plottable->readsTransform = true;
    plot.replot();
    const int before = plottable->recordings();
    QVERIFY(plottable->system());

    delete x; // unlists from the rect: its systems detach their plottables, then die
    QVERIFY(!plottable->system());
    QVERIFY(!plottable->keyAxis());
    plot.replot(); // nothing drawn, nothing recorded - and one report that it is not
    test_misuse::calls.clear();
    QCOMPARE(plottable->recordings(), before);
    const QPoint centre = rect->rect().center();
    QVERIFY(!plottable->hitTest(QCP::Pixels { double(centre.x()), double(centre.y()) }, nullptr)
                 .has_value());
}

void TestCoordinateSystem::crossPlotSystemIsRefused()
{
    QCustomPlot plot;
    QCPAxisRect* rect = cartesianRect(plot);
    auto* plottable = new RecordingPlottable(rect->axis(QCPAxis::atBottom),
                                             rect->axis(QCPAxis::atLeft));
    QCPCoordinateSystem* own = plottable->system();
    QVERIFY(own);

    QCustomPlot elsewhere;
    QCPAxisRect* otherRect = cartesianRect(elsewhere);
    plottable->setSystem(
        otherRect->system(otherRect->axis(QCPAxis::atBottom), otherRect->axis(QCPAxis::atLeft)));

    QCOMPARE(plottable->system(), own);
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    test_misuse::calls.clear(); // the refusal is the point of this test
}

void TestCoordinateSystem::deletingAnAxisDeletesItsSystems()
{
    QCustomPlot plot;
    QCPAxisRect* rect = cartesianRect(plot);
    QCPAxis* extra = rect->addAxis(QCPAxis::atRight);
    QCPCoordinateSystem* kept
        = rect->system(rect->axis(QCPAxis::atBottom), rect->axis(QCPAxis::atLeft));
    QCPCoordinateSystem* doomed = rect->system(rect->axis(QCPAxis::atBottom), extra);
    QVERIFY(kept && doomed);
    QCOMPARE(rect->systems().size(), 2);

    // A plottable of each: the one whose system dies is detached first, then the system goes.
    auto* stayer = new RecordingPlottable(kept);
    auto* leaver = new RecordingPlottable(doomed);
    QCOMPARE(stayer->system(), kept);
    QCOMPARE(leaver->system(), doomed);

    rect->removeAxis(extra);
    QCOMPARE(rect->systems().size(), 1);
    QCOMPARE(rect->systems().first(), kept);
    QCOMPARE(stayer->system(), kept);
    QVERIFY(!leaver->system());
}
