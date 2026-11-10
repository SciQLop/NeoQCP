#include "test-coordinate-system.h"
#include "coordinate-system-polar.h"
#include "coordinate-system.h"
#include "misuse-recorder.h"
#include "polar/angularaxis.h"
#include "polar/polar-area.h"
#include "polar/radialaxis.h"
#include "qcustomplot.h"
#include <QtTest/QtTest>
#include <QPicture>

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

QColor pixelAt(QCustomPlot& plot, QPoint pos)
{
    return QColor(plot.toPixmap(400, 300).toImage().pixel(pos));
}

//! The frame a polar area gives its one angular and one radial axis.
QCPCoordinateSystem* polarSystem(QCPPolarArea* area)
{
    return area->system(area->angularAxis(), area->radialAxis());
}

//! A plottable that needs nothing of its system and records the arc its data describes.
class ArcPlottable : public QCPAbstractPlottable1D<QCPGraphData>
{
public:
    explicit ArcPlottable(QCPCoordinateSystem* system)
            : QCPAbstractPlottable1D<QCPGraphData>(system)
    {
    }

    [[nodiscard]] QCP::Capabilities requiredCapabilities() const override { return { }; }

    void setArc(QVector<QCPGraphData> points) { mDataContainer->set(points); }

    [[nodiscard]] int recordings() const { return mCore.recordingCount(); }

    void draw(QCPDrawContext& context) const override
    {
        std::vector<QPointF> arc;
        for (const QCPGraphData& point : *mDataContainer)
            arc.emplace_back(point.key, point.value);
        context.polyline(std::make_shared<const std::vector<QPointF>>(std::move(arc)),
                         QPen(Qt::black, 3));
    }
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

void TestCoordinateSystem::barsAreRefusedInAPolarSystem_data()
{
    QTest::addColumn<QString>("kind");
    QTest::newRow("bars") << QStringLiteral("bars");
    QTest::newRow("colormap") << QStringLiteral("colormap");
    QTest::newRow("multigraph") << QStringLiteral("multigraph");
    QTest::newRow("intervals") << QStringLiteral("intervals");
}

void TestCoordinateSystem::barsAreRefusedInAPolarSystem()
{
    QFETCH(QString, kind);
    QCustomPlot plot;
    plot.plotLayout()->clear();
    auto* rect = new QCPAxisRect(&plot);
    plot.plotLayout()->addElement(0, 0, rect);
    auto* area = new QCPPolarArea(&plot);
    plot.plotLayout()->addElement(0, 1, area);
    settle(plot);

    QCPAxis* key = rect->axis(QCPAxis::atBottom);
    QCPAxis* value = rect->axis(QCPAxis::atLeft);
    QCPAbstractPlottable* plottable = nullptr;
    if (kind == QStringLiteral("bars"))
        plottable = new QCPBars(key, value);
    else if (kind == QStringLiteral("colormap"))
        plottable = new QCPColorMap(key, value);
    else if (kind == QStringLiteral("multigraph"))
        plottable = new QCPMultiGraph(key, value);
    else
        plottable = new QCPIntervals(key, value, new QCPLaneLayout(&plot));

    // What each class needs: the cartesian pair, plus the flags its drawing assumes.
    const QCP::Capabilities required = plottable->requiredCapabilities();
    QVERIFY(required.testFlag(QCP::capRectangular));
    QVERIFY(required.testFlag(QCP::capStraightKey));
    // A line maps its scalar through a colour scale, so a multi-graph needs one as well.
    QCOMPARE(required.testFlag(QCP::capColourScale),
             kind == QStringLiteral("colormap") || kind == QStringLiteral("multigraph"));
    QCOMPARE(required.testFlag(QCP::capLanes),
             kind == QStringLiteral("multigraph") || kind == QStringLiteral("intervals"));

    // The polar system offers none of them: the join is refused and the plottable stays put.
    QCPCoordinateSystem* own = plottable->system();
    QVERIFY(own);
    plottable->setSystem(polarSystem(area));
    QCOMPARE(plottable->system(), own);
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    test_misuse::calls.clear(); // the refusal is the point of this test
}

void TestCoordinateSystem::aRecordingPlottableWithNoNeedsJoinsPolar()
{
    QCustomPlot plot;
    plot.setBackground(QBrush(Qt::white));
    QCPPolarArea* area = polarArea(plot);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    area->angularAxis()->setVisible(false);
    area->radialAxis()->setVisible(false);
    area->grid()->setVisible(false);
    settle(plot);

    auto* arc = new ArcPlottable(polarSystem(area));
    QVERIFY(arc->system());
    QCOMPARE(arc->requiredCapabilities(), QCP::Capabilities());
    // The angular axis starts at -90 degrees: data coordinates are its own scale, so the arc is
    // asked for in angle coordinates, as a plottable reads them.
    const auto angleCoord = [area](double degrees)
    { return area->angularAxis()->angleRadToCoord(degrees / 180.0 * M_PI); };
    arc->setArc(QVector<QCPGraphData> { { angleCoord(40), 0.5 }, { angleCoord(50), 0.5 } });
    settle(plot);

    // The pixels are the ones the axes themselves give that polar coordinate.
    const auto atAngle = [area](double degrees)
    {
        return area->radialAxis()
            ->coordToPixel(area->angularAxis()->angleRadToCoord(degrees / 180.0 * M_PI), 0.5)
            .toPoint();
    };
    QVERIFY2(pixelAt(plot, atAngle(45.0)).lightness() < 128, "the arc is not drawn on its polar pixels");
    QCOMPARE(pixelAt(plot, atAngle(-45.0)).lightness(), 255);
}

void TestCoordinateSystem::relationshipsAcrossAreasAreRefused()
{
    QCustomPlot plot;
    plot.plotLayout()->clear();
    auto* left = new QCPAxisRect(&plot);
    auto* right = new QCPAxisRect(&plot);
    plot.plotLayout()->addElement(0, 0, left);
    plot.plotLayout()->addElement(0, 1, right);
    settle(plot);
    const auto pairOf = [](QCPAxisRect* rect)
    {
        return std::pair<QCPAxis*, QCPAxis*> { rect->axis(QCPAxis::atBottom),
                                               rect->axis(QCPAxis::atLeft) };
    };

    // A channel fill spans one frame: two areas are refused.
    auto [leftKey, leftValue] = pairOf(left);
    auto [rightKey, rightValue] = pairOf(right);
    auto* here = new QCPGraph(leftKey, leftValue);
    auto* there = new QCPGraph(rightKey, rightValue);
    here->setChannelFillGraph(there);
    QVERIFY(!here->channelFillGraph());
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    test_misuse::calls.clear();

    // So does a stacking, whether by moveAbove or by a bars group.
    auto* mine = new QCPBars(leftKey, leftValue);
    auto* theirs = new QCPBars(rightKey, rightValue);
    mine->moveAbove(theirs);
    QVERIFY(!mine->barAbove());
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    test_misuse::calls.clear();

    auto* group = new QCPBarsGroup(&plot);
    group->append(mine);
    QCOMPARE(group->size(), 1);
    group->append(theirs);
    QCOMPARE(group->size(), 1);
    QVERIFY(!theirs->barsGroup());
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    test_misuse::calls.clear();
    // insert() asks setBarsGroup first: a refused bar has no index, and the group must survive.
    group->insert(0, theirs);
    QCOMPARE(group->size(), 1);
    QVERIFY(!theirs->barsGroup());
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    test_misuse::calls.clear();

    // Whiskers draw their data plottable's points: they must be in its frame.
    auto* whiskers = new QCPErrorBars(leftKey, leftValue);
    whiskers->setDataPlottable(there);
    QVERIFY(!whiskers->dataPlottable());
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    test_misuse::calls.clear(); // the three refusals are the point of this test
}

void TestCoordinateSystem::rescaleAxesVisitsEveryArea()
{
    QCustomPlot plot;
    QCPPolarArea* area = polarArea(plot);
    const QCPRange angularBefore = area->angularAxis()->range();
    auto* plottable = new RecordingPlottable(polarSystem(area));
    settle(plot); // the drawn data range comes from the recording
    QVERIFY(plottable->drawnDataRange().has_value());

    plot.rescaleAxes();

    // The radial axis takes the drawn data range; the angular axis keeps its turn.
    QCOMPARE(area->radialAxis()->range(), QCPRange(1, 9));
    QCOMPARE(area->angularAxis()->range(), angularBefore);
}

void TestCoordinateSystem::axisPlottablesKeepTheirOrder()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    QCPAxisRect* rect1 = plot.axisRect();
    auto* rect2 = new QCPAxisRect(&plot);
    plot.plotLayout()->addElement(0, 1, rect2);
    QCPGraph* g1 = plot.addGraph(rect1->axis(QCPAxis::atBottom), rect1->axis(QCPAxis::atLeft));
    QCPGraph* g2 = plot.addGraph(rect1->axis(QCPAxis::atBottom), rect1->axis(QCPAxis::atLeft));
    QCPGraph* g3 = plot.addGraph(rect2->axis(QCPAxis::atBottom), rect2->axis(QCPAxis::atLeft));
    // Selection by system, but the plot's own order, as the axes have always answered.
    QCOMPARE(plot.xAxis->plottables(), (QList<QCPAbstractPlottable*> { g1, g2 }));
    QCOMPARE(rect1->plottables(), (QList<QCPAbstractPlottable*> { g1, g2 }));
    QCOMPARE(rect2->plottables(), (QList<QCPAbstractPlottable*> { g3 }));
    QCOMPARE(rect1->axis(QCPAxis::atLeft)->graphs(), (QList<QCPGraph*> { g1, g2 }));
}

void TestCoordinateSystem::aBracketDecorationNeedsAKeyAxis()
{
    QCustomPlot plot;
    QCPPolarArea* area = polarArea(plot);
    auto* arc = new ArcPlottable(polarSystem(area));
    arc->setArc(QVector<QCPGraphData> { { 0, 0.5 }, { 90, 0.5 } });
    auto* bracket = new QCPSelectionDecoratorBracket;
    arc->setSelectionDecorator(bracket);
    arc->setSelectable(QCP::stSingleData);
    arc->setSelection(QCPDataSelection(QCPDataRange(0, 2)));
    settle(plot);

    // A bracket is a cartesian caption: without a key axis there is no direction to face, and
    // drawing one would pin it to the origin of the image.
    QPicture picture;
    QCPPainter painter(&picture);
    bracket->drawDecoration(&painter, arc->selection());
    painter.end();
    QVERIFY(picture.boundingRect().isEmpty());
}
