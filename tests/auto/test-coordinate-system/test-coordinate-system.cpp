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
#include <QSet>

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
class CountingSystem : public QCPCoordinateSystem
{
public:
    CountingSystem(QCPArea* area, QCPAxisBase* key, QCPAxisBase* value)
            : QCPCoordinateSystem(area, key, value)
    {
    }

    QCPTransform transform() const override
    {
        return QCPTransform(qobject_cast<const QCPAxis*>(keyAxis()),
                            qobject_cast<const QCPAxis*>(valueAxis()));
    }

    QRect clipRect() const override { return static_cast<QCPAxisRect*>(area().get())->rect(); }

    QCP::Capabilities capabilities() const override
    {
        return QCP::capRectangular | QCP::capStraightKey;
    }

    QCPTransform iconTransform(const QCPDataBox& box, const QRectF& rect) const override
    {
        ++mIconCalls;
        return QCPCoordinateSystem::iconTransform(box, rect);
    }

    [[nodiscard]] int iconCalls() const { return mIconCalls; }

private:
    mutable int mIconCalls = 0;
};

class RecordingPlottable : public QCPAbstractPlottable
{
public:
    //! Needs nothing of its system: it records whatever the frame gives it, disc or rectangle.
    explicit RecordingPlottable(QCPCoordinateSystem* system)
            : QCPAbstractPlottable(system, QCP::Capabilities())
    {
    }

    RecordingPlottable(QCPAxis* keyAxis, QCPAxis* valueAxis)
            : QCPAbstractPlottable(keyAxis, valueAxis, QCP::Capabilities())
    {
    }

    void draw(QCPDrawContext& context) const override
    {
        ++mDraws;
        if (ratioRect)
        {
            context.rect(QCP::AxisRatio { 0, 0 }, QCP::AxisRatio { 1, 1 }, Qt::NoPen,
                         QBrush(Qt::red));
            return;
        }
        if (mixedRect)
        {
            context.rect(QCP::DataXRatioY { 1, 0.5 }, QCP::RatioXDataY { 0.5, 1 }, Qt::NoPen,
                         QBrush(Qt::red));
            return;
        }
        if (coversItsClip) // a recording that fills whatever the frame lets it draw in
        {
            const QRectF clip = context.clip();
            context.rect(QCP::Pixels { clip.left(), clip.top() },
                         QCP::Pixels { clip.right(), clip.bottom() }, Qt::NoPen,
                         QBrush(Qt::red));
            return;
        }
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

    void systemChanged() override { ++mSystemChanges; }

    //! How many times the plottable was told its frame changed: a move into another plot is one.
    [[nodiscard]] int systemChanges() const { return mSystemChanges; }

    //! How many frames it was drawn in: zero while the area it hangs on is hidden.
    [[nodiscard]] int draws() const { return mDraws; }

    bool readsTransform = false;
    bool coversItsClip = false;
    bool ratioRect = false;
    bool mixedRect = false;

private:
    int mSystemChanges = 0;
    mutable int mDraws = 0;

    std::shared_ptr<const std::vector<QPointF>> mLine
        = std::make_shared<const std::vector<QPointF>>(
            std::initializer_list<QPointF> { { 1, 1 }, { 9, 9 } });
};

QColor pixelAt(QCustomPlot& plot, QPoint pos)
{
    return QColor(plot.toPixmap(400, 300).toImage().pixel(pos));
}

//! Catches the messages Qt would print: a library path that has nothing to rescale must say none.
QStringList gMessages;
void collectMessage(QtMsgType, const QMessageLogContext&, const QString& message)
{
    gMessages << message;
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
            : QCPAbstractPlottable1D<QCPGraphData>(system, QCP::Capabilities())
    {
    }

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
             QCP::capRectangular | QCP::capStraightKey | QCP::capColourScale | QCP::capLanes
                 | QCP::capStraightSegments);
    QVERIFY(rect->ratioTransform().valid());
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

void TestCoordinateSystem::polarAreaRatioIsTheDisc()
{
    QCustomPlot plot;
    QCPPolarArea* area = polarArea(plot);
    QCPCoordinateSystem* system = area->system(area->angularAxis(), area->radialAxis());
    QVERIFY(system);

    // The disc is a ratio of its own: x is a fraction of the turn, y a fraction of the radius. It
    // is not separable, and asking for it is not misuse.
    const QCPRatioTransform ratio = area->ratioTransform();
    QVERIFY(ratio.valid());
    QVERIFY(!ratio.separable());
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));

    // The default offset is -90 degrees, so a quarter turn lands on the +x ray, half way out.
    const QCP::Pixels pixels = ratio.map(QCP::AxisRatio { 0.25, 0.5 });
    QVERIFY(qFuzzyCompare(pixels.x + 1.0, area->center().x() + 0.5 * area->radius() + 1.0));
    QVERIFY(qFuzzyCompare(pixels.y + 1.0, area->center().y() + 1.0));

    // And back: the pixel on that ray answers the same fractions.
    const QCP::AxisRatio back = ratio.inverse(QPointF(pixels.x, pixels.y));
    QVERIFY(qFuzzyCompare(back.x + 1.0, 0.25 + 1.0));
    QVERIFY(qFuzzyCompare(back.y + 1.0, 0.5 + 1.0));
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

void TestCoordinateSystem::aDeliberateNullSystemStaysQuiet()
{
    // Dropping the system on purpose makes the plottable inert: it draws nothing and says nothing.
    QCustomPlot plot;
    QCPAxisRect* rect = cartesianRect(plot);
    auto* plottable = new RecordingPlottable(
        rect->system(rect->axis(QCPAxis::atBottom), rect->axis(QCPAxis::atLeft)));
    settle(plot);
    const int draws = plottable->draws();
    test_misuse::calls.clear();

    plottable->setSystem(nullptr);
    settle(plot);

    QCOMPARE(test_misuse::calls.size(), 0);
    QCOMPARE(plottable->draws(), draws);
}

void TestCoordinateSystem::aLostSystemReportsOnce()
{
    // A system that goes away under the plottable is worth one report: the user has to know why it
    // stopped being drawn.
    QCustomPlot plot;
    QCPAxisRect* rect = cartesianRect(plot);
    QCPAxis* top = rect->addAxis(QCPAxis::atTop);
    QCPAxis* right = rect->addAxis(QCPAxis::atRight);
    auto* plottable = new RecordingPlottable(rect->system(top, right));
    settle(plot);
    test_misuse::calls.clear();

    rect->removeAxis(top); // the system using it goes with it
    settle(plot);
    settle(plot); // one report, not one per frame

    QCOMPARE(test_misuse::calls.size(), 1);
    QCOMPARE(QString::fromStdString(test_misuse::calls.front().what),
             QStringLiteral("it has no coordinate system: it is not drawn"));
    QCOMPARE(plottable->system(), nullptr);
    test_misuse::calls.clear(); // the one report above is the point of this test
}

void TestCoordinateSystem::aMixedPositionInAPolarAreaReportsOnce()
{
    // A mixed position is a key and a value, and a polar ratio maps the two together: the command
    // is dropped, with one misuse, not one per frame.
    QCustomPlot plot;
    QCPPolarArea* area = polarArea(plot);
    area->setBackground(QBrush(Qt::white));
    auto* plottable = new RecordingPlottable(area->system(area->angularAxis(), area->radialAxis()));
    plottable->mixedRect = true;
    settle(plot);
    QCOMPARE(test_misuse::calls.size(), 1);
    QCOMPARE(QString::fromStdString(test_misuse::calls.front().what),
             QStringLiteral("a position mixing plot coordinates and axis ratios needs a separable "
                            "ratio: it is not drawn"));
    const QPoint centre(qRound(area->center().x()), qRound(area->center().y()));
    QVERIFY2(plot.toPixmap().toImage().pixelColor(centre) != QColor(Qt::red),
             "a mixed rect has no key side in a disc, yet it was drawn");

    area->angularAxis()->setAngle(30); // a new frame records it again
    settle(plot);
    QCOMPARE(test_misuse::calls.size(), 1);
    test_misuse::calls.clear();
}

void TestCoordinateSystem::aPolarPlottableIsClippedToItsDisc()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.plotLayout()->clear();
    auto* area = new QCPPolarArea(&plot);
    plot.plotLayout()->addElement(0, 0, area);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    area->setBackground(QBrush(Qt::white));
    area->grid()->setVisible(false);
    area->angularAxis()->setTicks(false);
    area->angularAxis()->setTickLabels(false);
    area->radialAxis()->setTicks(false);
    area->radialAxis()->setTickLabels(false);
    auto* sheet = new RecordingPlottable(area->system(area->angularAxis(), area->radialAxis()));
    sheet->coversItsClip = true;
    settle(plot);

    // The plottable fills its clip, which is the disc's bounding square: what it draws must stop
    // at the rim, wherever the replay goes through the painter (an export, or a screen without a
    // GPU layer).
    const QImage image = plot.toPixmap().toImage();
    const QPointF centre = area->center();
    const double radius = area->radius();
    QCOMPARE(image.pixelColor(qRound(centre.x()), qRound(centre.y())), QColor(Qt::red));
    const QPoint corner(qRound(centre.x() + 0.9 * radius), qRound(centre.y() - 0.9 * radius));
    QCOMPARE(image.pixelColor(corner), QColor(Qt::white));
}

void TestCoordinateSystem::theIconAsksTheSystem()
{
    // The legend icon replays through the system's icon transform: the default fits the recording
    // into the rect as the 2.x icon always did, but the system decides, not the drawing core.
    QCustomPlot plot;
    plot.resize(400, 300);
    QCPAxisRect* rect = plot.axisRect();
    CountingSystem system(rect, rect->axis(QCPAxis::atBottom), rect->axis(QCPAxis::atLeft));
    auto* plottable = new RecordingPlottable(&system);
    QCOMPARE(plottable->system(), &system);
    plot.legend->setVisible(true);
    settle(plot);
    QVERIFY2(system.iconCalls() > 0, "the legend icon was not replayed through the system");
}

void TestCoordinateSystem::aNonCartesianPlottableRescalesQuietly()
{
    QCustomPlot plot;
    QCPPolarArea* area = polarArea(plot);
    auto* plottable = new RecordingPlottable(polarSystem(area));
    settle(plot);
    const QCPRange angular = area->angularAxis()->range();
    const QCPRange radial = area->radialAxis()->range();

    gMessages.clear();
    const QtMessageHandler previous = qInstallMessageHandler(collectMessage);
    plottable->rescaleKeyAxis();
    plottable->rescaleValueAxis();
    plottable->rescaleAxes();
    qInstallMessageHandler(previous);

    QVERIFY2(gMessages.isEmpty(), qPrintable(gMessages.join(QLatin1Char('\n'))));
    QCOMPARE(area->angularAxis()->range(), angular);
    QCOMPARE(area->radialAxis()->range(), radial);
}

void TestCoordinateSystem::systemRevisionsAreNeverShared()
{
    // A recording keys on the system and its revision: a system that reuses the address of a
    // deleted one must not answer with its revision, or the recording would replay the old frame.
    QList<quint64> revisions;
    for (int i = 0; i < 200; ++i)
    {
        QCustomPlot plot;
        plot.resize(400, 300);
        revisions.append(plot.axisRect()->system(plot.xAxis, plot.yAxis)->revision());
    }
    QCOMPARE(QSet<quint64>(revisions.begin(), revisions.end()).size(), revisions.size());
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

    // What each class needs: the cartesian pair, plus the flags its drawing assumes. A
    // multigraph needs the straight segment only (P-3); a polar system offers none of these.
    const QCP::Capabilities required = plottable->requiredCapabilities();
    if (kind == QStringLiteral("multigraph"))
        QCOMPARE(required, QCP::capStraightSegments);
    else
    {
        QVERIFY(required.testFlag(QCP::capRectangular));
        QVERIFY(required.testFlag(QCP::capStraightKey));
        QCOMPARE(required.testFlag(QCP::capColourScale), kind == QStringLiteral("colormap"));
        QCOMPARE(required.testFlag(QCP::capLanes), kind == QStringLiteral("intervals"));
    }

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

void TestCoordinateSystem::aMisusedPairIsOwnedAndInert()
{
    QCustomPlot plot;
    plot.plotLayout()->clear();
    auto* left = new QCPAxisRect(&plot);
    auto* right = new QCPAxisRect(&plot);
    plot.plotLayout()->addElement(0, 0, left);
    plot.plotLayout()->addElement(0, 1, right);
    settle(plot);

    // Axes of two rects: 2.x drew this clipped to both rects. Now it is no frame, but the plot
    // still owns it, and the one misuse report is the diagnostic.
    auto* across = new QCPGraph(left->axis(QCPAxis::atBottom), right->axis(QCPAxis::atLeft));
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    test_misuse::calls.clear();
    QVERIFY(!across->system());
    QCOMPARE(plot.graphCount(), 1);
    QCOMPARE(plot.graph(0), across);
    QCOMPARE(plot.plottableCount(), 1);

    // Two axes of one orientation: same story, not just a debug line.
    auto* flat = new QCPGraph(left->axis(QCPAxis::atBottom), left->axis(QCPAxis::atTop));
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    test_misuse::calls.clear();
    QVERIFY(!flat->system());
    QCOMPARE(plot.graphCount(), 2);
    QCOMPARE(plot.graph(1), flat);
    QCOMPARE(plot.plottableCount(), 2);

    // Inert: no layer put it anywhere to draw, and a replot is quiet.
    flat->setData({ 0, 1 }, { 0, 1 });
    settle(plot);
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
    QCOMPARE(plot.graphCount(), 2);
}

void TestCoordinateSystem::aChangedSlotMayDeleteTheAxis()
{
    // The systems' changed() is public: a slot may delete the axis that is being set. The setter
    // must be done with the axis by then, not in the middle of it.
    QCustomPlot plot;
    plot.resize(400, 300);
    QCPAxisRect* rect = plot.axisRect();
    QCPAxis* axis = plot.xAxis;
    QCPCoordinateSystem* system = rect->system(plot.xAxis, plot.yAxis);
    QVERIFY(system);
    bool deleted = false;
    connect(system, &QCPCoordinateSystem::changed, axis,
            [rect, axis, &deleted]
            {
                deleted = true;
                rect->removeAxis(axis);
            });

    axis->setRange(0, 10);

    QVERIFY(deleted);
    QVERIFY(!qcp::pointers(rect->axes()).contains(axis));
}

void TestCoordinateSystem::aMovedPlottableFollowsItsNewArea()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    QCPAxisRect* first = plot.axisRect();
    auto* second = new QCPAxisRect(&plot);
    plot.plotLayout()->addElement(0, 1, second);
    auto* plottable
        = new RecordingPlottable(first->system(first->axis(QCPAxis::atBottom),
                                               first->axis(QCPAxis::atLeft)));
    settle(plot);
    QVERIFY(plottable->draws() > 0);

    plottable->setSystem(second->system(second->axis(QCPAxis::atBottom),
                                        second->axis(QCPAxis::atLeft)));
    first->setVisible(false);
    const int before = plottable->draws();
    settle(plot);

    // It draws in the area it moved to: the one it left cannot hide it any more.
    QCOMPARE(plottable->draws(), before + 1);
}

void TestCoordinateSystem::aTakenPlottableFollowsItsNewPlot()
{
    // A plottable taken out of one plot and added to another joins that plot's system, and hears
    // about it the way setSystem tells it: a graph2 reconnects to the new axes' signals.
    QCustomPlot from;
    QCustomPlot to;
    from.resize(400, 300);
    to.resize(400, 300);

    auto handle = from.add(std::unique_ptr<RecordingPlottable>(new RecordingPlottable(nullptr)));
    QVERIFY(handle);
    RecordingPlottable* recorded = handle.get();
    const int before = recorded->systemChanges();
    auto taken = from.take(handle);
    QVERIFY(taken);
    QVERIFY(to.add(std::move(taken)));
    QCOMPARE(recorded->systemChanges(), before + 1); // the move announced the new frame
    QCOMPARE(recorded->system()->area(), to.axisRect());
    QCOMPARE(recorded->keyAxis(), to.xAxis);
    QCOMPARE(to.plottableCount(), 1);
    QCOMPARE(from.plottableCount(), 0);

    auto graph = from.add<QCPGraph2>().setData(QVector<double> { 1, 5, 9 },
                                               QVector<double> { 1, 5, 9 });
    QVERIFY(graph);
    auto movedGraph = from.take(graph);
    QVERIFY(movedGraph);
    QVERIFY(to.add(std::move(movedGraph)));
    settle(to);
    QCOMPARE(graph->keyAxis(), to.xAxis);
    QCOMPARE(graph->system(), to.axisRect()->system(to.xAxis, to.yAxis));
    QVERIFY2(graph->hasRenderedRange(), "the graph did not draw in its new plot");
}

void TestCoordinateSystem::dualValueAxesKeepTheirRelationships()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    QCPAxis* right = plot.axisRect()->addAxis(QCPAxis::atRight);
    right->setRange(0, 1);

    // A group lays its members out in key pixels only, so two value axes are fine.
    auto* low = new QCPBars(plot.xAxis, plot.yAxis);
    auto* high = new QCPBars(plot.xAxis, right);
    low->setData({ 1, 2, 3 }, { 1, 1, 1 });
    high->setData({ 1, 2, 3 }, { 1, 1, 1 });
    low->setPen(Qt::NoPen);
    high->setPen(Qt::NoPen);
    low->setBrush(Qt::red);
    high->setBrush(Qt::blue);
    auto* group = new QCPBarsGroup(&plot);
    group->append(low);
    group->append(high);
    QCOMPARE(group->size(), 2);
    QCOMPARE(high->barsGroup(), group);
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));

    // A channel fill works in pixels, between two key axes of one orientation.
    auto* here = new QCPGraph(plot.xAxis, plot.yAxis);
    auto* there = new QCPGraph(plot.xAxis, right);
    here->setData({ 1, 3 }, { 0.2, 0.2 });
    there->setData({ 1, 3 }, { 0.8, 0.8 });
    here->setChannelFillGraph(there);
    QCOMPARE(here->channelFillGraph(), there);
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));

    // Both really paint: the two bar colours beside each other at a bar's key.
    settle(plot);
    const QImage image = plot.toPixmap().toImage();
    int red = 0;
    int blue = 0;
    const int y = qRound(plot.yAxis->coordToPixel(0.5));
    for (int x = qRound(plot.xAxis->coordToPixel(1.7)); x <= qRound(plot.xAxis->coordToPixel(2.3));
         ++x)
    {
        const QColor colour = image.pixelColor(x, y);
        red += colour == QColor(Qt::red);
        blue += colour == QColor(Qt::blue);
    }
    QVERIFY(red > 0);
    QVERIFY(blue > 0);
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
    QCOMPARE(qcp::pointers(plot.xAxis->plottables()), (QList<QCPAbstractPlottable*> { g1, g2 }));
    QCOMPARE(qcp::pointers(rect1->plottables()), (QList<QCPAbstractPlottable*> { g1, g2 }));
    QCOMPARE(qcp::pointers(rect2->plottables()), (QList<QCPAbstractPlottable*> { g3 }));
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
