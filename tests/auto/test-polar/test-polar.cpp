#include "test-polar.h"
#include "coords.h"
#include "misuse-recorder.h"
#include "painting/draw-commands.h"
#include "polar/angularaxis.h"
#include "polar/polar-area.h"
#include "polar/polargraph.h"
#include "polar/polargrid.h"
#include "polar/radialaxis.h"
#include <QPointer>
#include <cmath>
#include <numbers>
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

void sendMouse(QCustomPlot* plot, QEvent::Type type, QPoint pos, Qt::MouseButtons buttons)
{
    QMouseEvent e(type, pos, plot->mapToGlobal(pos), Qt::LeftButton, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(plot, &e);
}

// The area alone in the layout, so its rect is the whole plot minus its margins.
QCPPolarArea* polarArea(QCustomPlot& plot)
{
    plot.plotLayout()->clear();
    auto* area = new QCPPolarArea(&plot);
    plot.plotLayout()->addElement(0, 0, area);
    settle(plot);
    return area;
}

QColor pixelAt(QCustomPlot& plot, QPoint pos)
{
    return QColor(plot.toPixmap(400, 300).toImage().pixel(pos));
}

// The selectable part the axis reports under pos, or spNone where it reports nothing.
QCPAxisBase::SelectablePart partAt(QCPAxisBase* axis, QPointF pos)
{
    QVariant details;
    if (axis->selectTest(pos, false, &details) <= 0)
        return QCPAxisBase::spNone;
    return details.value<QCPAxisBase::SelectablePart>();
}

// The first point of a coarse scan of rect where the axis reports that part.
QPointF findPart(QCPAxisBase* axis, const QRect& rect, QCPAxisBase::SelectablePart wanted)
{
    for (int y = rect.top(); y <= rect.bottom(); y += 2)
        for (int x = rect.left(); x <= rect.right(); x += 2)
            if (partAt(axis, QPointF(x, y)) == wanted)
                return QPointF(x, y);
    return QPointF(-1, -1);
}

// The data-space polylines of a recording: one per finite run of the graph.
std::vector<std::vector<QPointF>> recordedPolylines(const QCPDrawnCore& core)
{
    std::vector<std::vector<QPointF>> out;
    for (const auto& command : core.recordedList().commands)
        if (const auto* line = std::get_if<qcp::PolylineDataCmd>(&command))
            out.push_back(*line->points);
    return out;
}

// The number of vertices of the recorded fill, zero when it recorded none.
std::size_t recordedPolygonSize(const QCPDrawnCore& core)
{
    for (const auto& command : core.recordedList().commands)
        if (const auto* polygon = std::get_if<qcp::PolygonCmd>(&command))
            return polygon->points.size();
    return 0;
}

std::size_t recordedPolygonCount(const QCPDrawnCore& core)
{
    std::size_t count = 0;
    for (const auto& command : core.recordedList().commands)
        if (std::get_if<qcp::PolygonCmd>(&command))
            ++count;
    return count;
}

std::size_t recordedMarkerCount(const QCPDrawnCore& core)
{
    for (const auto& command : core.recordedList().commands)
        if (const auto* markers = std::get_if<qcp::MarkersCmd>(&command))
            return markers->points ? markers->points->size() : 0;
    return 0;
}

// The key where the straight segment between two (angle, fraction) points cuts the unit circle:
// the geometry of the picture, computed here rather than by the graph's own code.
double chordRimKey(double angleA, double fractionA, double angleB, double fractionB)
{
    const double radians = std::numbers::pi / 180.0;
    const auto unit = [radians](double angle, double fraction)
    { return QPointF(std::cos(angle * radians) * fraction, std::sin(angle * radians) * fraction); };
    const QPointF from = unit(angleA, fractionA);
    const QPointF delta = unit(angleB, fractionB) - from;
    const double a = QPointF::dotProduct(delta, delta);
    const double b = 2.0 * QPointF::dotProduct(from, delta);
    const double c = QPointF::dotProduct(from, from) - 1.0;
    const double root = std::sqrt(b * b - 4.0 * a * c);
    double t = (-b - root) / (2.0 * a);
    if (t < 0.0 || t > 1.0)
        t = (-b + root) / (2.0 * a);
    const QPointF crossing = from + t * delta;
    double key = std::atan2(crossing.y(), crossing.x()) / radians;
    while (key < angleA - 1e-9)
        key += 360.0;
    return key;
}

QImage imageOf(QCustomPlot& plot)
{
    return plot.toPixmap(400, 300).toImage();
}

bool isInk(const QColor& color)
{
    return color.blue() > 150 && color.red() < 120;
}

bool isRedInk(const QColor& color)
{
    return color.red() > 200 && color.green() < 80 && color.blue() < 80;
}

// How many pixels of image the graph's blue pen inked.
int inkCount(const QImage& image)
{
    int count = 0;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            if (isInk(image.pixelColor(x, y)))
                ++count;
    return count;
}

// The inked pixels of image whose angle coord is inside (from, to) and whose radius is below limit
// times the area's radius: the part of the disc a run beyond the rim must leave empty.
int inkInSector(QCPPolarArea& area, const QImage& image, double from, double to, double limit,
                double inner = 0.0)
{
    int count = 0;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
        {
            if (!isInk(image.pixelColor(x, y)))
                continue;
            double angle = 0, radius = 0;
            area.radialAxis()->pixelToCoord(QPointF(x, y), angle, radius);
            const double fraction = area.radialAxis()->coordToRadius(radius) / area.radius();
            if (angle > from && angle < to && fraction < limit && fraction > inner)
                ++count;
        }
    return count;
}
} // namespace

void TestPolar::init()
{
    test_misuse::calls.clear();
    qcp::setMisuseHandler(test_misuse::record);
}

void TestPolar::cleanup()
{
    qcp::setMisuseHandler(nullptr);
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
}

void TestPolar::areaOwnsItsAxes()
{
    QCustomPlot plot;
    plot.plotLayout()->clear();
    auto area = plot.add<QCPPolarArea>(0, 0);
    QVERIFY(area);
    QCOMPARE(plot.plotLayout()->element(0, 0), area.get());

    QVERIFY(area->angularAxis());
    QCOMPARE(area->angularAxis()->area(), area.get());
    QCOMPARE(area->radialAxisCount(), 1);
    QVERIFY(area->radialAxis(0));
    QCOMPARE(area->grid() != nullptr, true);
    QVERIFY(area->layer());
}

void TestPolar::areaLaysOutCentreAndRadius()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    plot.resize(400, 300);
    settle(plot);

    QCOMPARE(area->rect().topLeft(), QPoint(30, 30));
    QCOMPARE(area->rect().size(), QSize(340, 240));
    QCOMPARE(area->center(), QPointF(area->rect().center()));
    // Half the smaller side, as the angular axis laid itself out before the area took over.
    QCOMPARE(area->radius(), 120.0);
}

void TestPolar::deletingTheAreaMidDragIsSafe()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    plot.setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    area->angularAxis()->setRangeDraggable(true);
    area->angularAxis()->setRange(0, 360);

    QPointer<QCPPolarArea> alive(area);
    connect(
        area->angularAxis(), &QCPPolarAxisAngular::rangeChanged, area,
        [&alive] { delete alive.data(); }, Qt::DirectConnection);

    // A quarter turn across the centre: a radial move would leave the angle unchanged.
    const QPoint from = area->center().toPoint() + QPoint(int(0.6 * area->radius()), 0);
    const QPoint to = area->center().toPoint() + QPoint(0, int(0.6 * area->radius()));
    sendMouse(&plot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(&plot, QEvent::MouseMove, to, Qt::LeftButton);
    sendMouse(&plot, QEvent::MouseButtonRelease, to, Qt::NoButton);

    QVERIFY(!alive);
}

void TestPolar::aPolarGraphIsAPlottable()
{
    QCustomPlot plot;
    // The polar area replaces the axis rect: build the layout by hand to keep a legend in it.
    plot.plotLayout()->clear();
    plot.legend = new QCPLegend;
    plot.plotLayout()->addElement(1, 0, plot.legend);
    auto* area = new QCPPolarArea(&plot);
    plot.plotLayout()->addElement(0, 0, area);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 90, 180 },
                                          QVector<double> { 0.5, 0.6, 0.5 });
    QVERIFY(graph);
    settle(plot);

    // The plot owns it as a plottable, and its axes are the area's polar system.
    QCOMPARE(plot.plottableCount(), 1);
    QCOMPARE(plot.plottable(0), graph.get());
    QCOMPARE(graph->system(), area->system(area->angularAxis(), area->radialAxis()));
    QVERIFY(graph->system());
    QCOMPARE(graph->system()->area(), static_cast<QCPArea*>(area));
    QCOMPARE(area->graphs(), QList<QCPPolarGraph*> { graph.get() });
    QCOMPARE(graph->parentLayerable(), static_cast<QCPLayerable*>(area));

    // One legend row, the plottable one.
    QCOMPARE(plot.legend->itemCount(), 1);
    QVERIFY(qobject_cast<QCPPlottableLegendItem*>(plot.legend->item(0)));
}

void TestPolar::aPolarGraphRecords()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    const QVector<double> angle { 0, 90, 180, 270, 360 };
    const QVector<double> radius { 0.5, 0.7, 0.4, 0.8, 0.5 };
    auto graph = area->add<QCPPolarGraph>(angle, radius);
    QVERIFY(graph);
    settle(plot);

    const auto lines = recordedPolylines(graph->mCore);
    QCOMPARE(lines.size(), std::size_t(1));
    QCOMPARE(lines.front().size(), std::size_t(5));
    // Recorded in data space: the replay maps (angle, radius) through the system's frame.
    for (int i = 0; i < angle.size(); ++i)
    {
        QCOMPARE(lines.front()[std::size_t(i)].x(), angle[i]);
        QCOMPARE(lines.front()[std::size_t(i)].y(), radius[i]);
    }
    QCOMPARE(recordedPolygonSize(graph->mCore), std::size_t(0));

    graph->setBrush(QBrush(Qt::red));
    settle(plot);
    QCOMPARE(recordedPolygonSize(graph->mCore), std::size_t(5));
}

void TestPolar::aRotationDoesNotReRecord()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    QCPPolarAxisAngular* angular = area->angularAxis();
    QCPPolarAxisRadial* radial = area->radialAxis();
    angular->setRange(0, 360);
    radial->setRange(0, 1);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 90, 180 },
                                          QVector<double> { 0.8, 0.8, 0.8 });
    QVERIFY(graph);
    graph->setPen(QPen(Qt::blue, 3));
    settle(plot);

    const quint64 recordings = graph->mCore.recordingCount();
    const QPoint onLine = radial->coordToPixel(0, 0.8).toPoint();
    QVERIFY2(isInk(pixelAt(plot, onLine)), qPrintable(pixelAt(plot, onLine).name()));

    // A periodic graph records the whole container: a rotation only moves the frame.
    angular->setRange(10, 370);
    settle(plot);
    QCOMPARE(graph->mCore.recordingCount(), recordings);
    QVERIFY2(!isInk(pixelAt(plot, onLine)), qPrintable(pixelAt(plot, onLine).name()));
    const QPoint moved = radial->coordToPixel(0, 0.8).toPoint();
    QVERIFY2(moved != onLine, "sanity: the rotation moved the point");
    QVERIFY2(isInk(pixelAt(plot, moved)), qPrintable(pixelAt(plot, moved).name()));

    // The radius comes from the radial frame: a radial zoom records again.
    radial->setRange(0, 2);
    settle(plot);
    QCOMPARE(graph->mCore.recordingCount(), recordings + 1);
}

void TestPolar::aNonPeriodicGraphKeepsItsWindow()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    QCPPolarAxisAngular* angular = area->angularAxis();
    area->radialAxis()->setRange(0, 1);
    angular->setRange(0, 360);
    QVector<double> angle, radius;
    for (int i = 0; i <= 72; ++i)
    {
        angle << i * 10.0;
        radius << 0.5;
    }
    auto graph = area->add<QCPPolarGraph>(angle, radius);
    QVERIFY(graph);
    graph->setPeriodic(false);
    settle(plot);

    auto lines = recordedPolylines(graph->mCore);
    QCOMPARE(lines.size(), std::size_t(1));
    // 2.1's window: the keys inside the angular range, plus one on each side so the line enters
    // and leaves the range.
    QCOMPARE(lines.front().size(), std::size_t(38));
    QCOMPARE(lines.front().front().x(), 0.0);
    QCOMPARE(lines.front().back().x(), 370.0);

    const quint64 recordings = graph->mCore.recordingCount();
    angular->setRange(10, 370);
    settle(plot);
    QVERIFY2(graph->mCore.recordingCount() > recordings, "a rotation re-records a windowed graph");
    lines = recordedPolylines(graph->mCore);
    QCOMPARE(lines.size(), std::size_t(1));
    QCOMPARE(lines.front().size(), std::size_t(39));
    QCOMPARE(lines.front().front().x(), 0.0); // the one before the range, kept for the entry
    QCOMPARE(lines.front().back().x(), 380.0);
}

void TestPolar::aRimCrossingIsOnTheChord_data()
{
    QTest::addColumn<double>("lower");
    QTest::addColumn<double>("upper");
    QTest::addColumn<double>("deep");
    QTest::addColumn<double>("middle");
    QTest::addColumn<double>("shallow");
    QTest::addColumn<bool>("logarithmic");
    QTest::addColumn<bool>("reversed");
    // The polar-clamped series at fractions 0.2, 1.6 and 0.9 of the radial range, read on three
    // ways of turning a value into a radius.
    QTest::newRow("linear") << 0.0 << 1.0 << 0.2 << 1.6 << 0.9 << false << false;
    QTest::newRow("log radial") << 1.0 << 1000.0 << std::pow(1000.0, 0.2) << std::pow(1000.0, 1.6)
                                << std::pow(1000.0, 0.9) << true << false;
    QTest::newRow("reversed radial") << 0.0 << 1.0 << 0.8 << -0.6 << 0.1 << false << true;
}

void TestPolar::aRimCrossingIsOnTheChord()
{
    QFETCH(double, lower);
    QFETCH(double, upper);
    QFETCH(double, deep);
    QFETCH(double, middle);
    QFETCH(double, shallow);
    QFETCH(bool, logarithmic);
    QFETCH(bool, reversed);

    QCustomPlot plot;
    auto* area = polarArea(plot);
    QCPPolarAxisAngular* angular = area->angularAxis();
    QCPPolarAxisRadial* radial = area->radialAxis();
    angular->setRange(0, 360);
    radial->setRange(lower, upper);
    radial->setScaleType(logarithmic ? QCPAxisBase::stLogarithmic : QCPAxisBase::stLinear);
    radial->setRangeReversed(reversed);
    // Out of the disc, then back in: the series the polar-clamped picture draws.
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 30, 60 },
                                          QVector<double> { deep, middle, shallow });
    QVERIFY(graph);
    settle(plot);

    // The crossings sit where the straight chords between the data points cut the rim.
    const double entry = chordRimKey(0, 0.2, 30, 1.6);
    const double exit = chordRimKey(30, 1.6, 60, 0.9);
    QVERIFY2(std::abs(entry - 27.6) < 0.5, qPrintable(QString::number(entry)));
    QVERIFY2(std::abs(exit - 51.5) < 0.5, qPrintable(QString::number(exit)));

    std::vector<double> crossings;
    const double rimValue = radial->radiusToCoord(area->radius());
    for (const auto& line : recordedPolylines(graph->mCore))
        for (const QPointF& point : line)
            if (std::abs(point.y() - rimValue) < 1e-9)
                crossings.push_back(point.x());
    QCOMPARE(crossings.size(), std::size_t(2));
    QVERIFY2(std::abs(crossings[0] - entry) < 0.5, qPrintable(QString::number(crossings[0])));
    QVERIFY2(std::abs(crossings[1] - exit) < 0.5, qPrintable(QString::number(crossings[1])));
}

void TestPolar::aValueTheRadialScaleCannotTakeIsAGap()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    QCPPolarAxisRadial* radial = area->radialAxis();
    area->angularAxis()->setRange(0, 360);
    radial->setRange(0.1, 10);
    radial->setScaleType(QCPAxisBase::stLogarithmic);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 60, 120, 180, 240, 300 },
                                          QVector<double> { 1, 2, 0, -1, 3, 4 });
    QVERIFY(graph);
    graph->setBrush(QBrush(Qt::red));
    settle(plot);

    // A value the logarithmic radius cannot take is a gap, exactly like a NaN value: the run
    // stops before it and starts again after it.
    const auto lines = recordedPolylines(graph->mCore);
    QCOMPARE(lines.size(), std::size_t(2));
    QCOMPARE(lines[0].size(), std::size_t(2));
    QCOMPARE(lines[0].back().x(), 60.0);
    QCOMPARE(lines[1].size(), std::size_t(2));
    QCOMPARE(lines[1].front().x(), 240.0);
    QCOMPARE(recordedPolygonCount(graph->mCore), std::size_t(2));

    // And every recorded point maps: the frame reports no NaN for anything recorded.
    const QCPTransform frame = graph->system()->transform();
    QVERIFY(frame.valid());
    for (const auto& command : graph->mCore.recordedList().commands)
    {
        if (const auto* line = std::get_if<qcp::PolylineDataCmd>(&command))
            for (const QPointF& point : *line->points)
            {
                const QCP::Pixels pixel = frame.map(QCP::Data { point.x(), point.y() });
                QVERIFY2(std::isfinite(pixel.x) && std::isfinite(pixel.y),
                         qPrintable(QString::number(point.y())));
            }
        if (const auto* polygon = std::get_if<qcp::PolygonCmd>(&command))
            for (const QCP::Position& position : polygon->points)
                if (const auto* data = std::get_if<QCP::Data>(&position))
                {
                    const QCP::Pixels pixel = frame.map(*data);
                    QVERIFY2(std::isfinite(pixel.x) && std::isfinite(pixel.y),
                             qPrintable(QString::number(data->x)));
                }
    }
}

void TestPolar::aGraphFirstRecordedAtZeroSizeAppearsAfterAResize()
{
    QCustomPlot plot;
    plot.resize(0, 0);
    plot.plotLayout()->clear();
    auto* area = new QCPPolarArea(&plot);
    plot.plotLayout()->addElement(0, 0, area);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 90, 180 },
                                          QVector<double> { 0.6, 0.6, 0.6 });
    QVERIFY(graph);
    graph->setPen(QPen(Qt::blue, 3));

    // The first replot happens while the plot has no size at all. A zero-size widget gets no paint,
    // so nothing is recorded yet: the guard is that the graph still draws once the plot has a size.
    plot.replot();
    QCoreApplication::processEvents();

    plot.resize(400, 300);
    plot.replot();
    QCoreApplication::processEvents();
    const QImage image = imageOf(plot);
    QVERIFY2(inkCount(image) > 0, "the graph never came back after the plot was given a size");
}

namespace
{
// A plottable on polar axes that needs a rectangular frame the polar system cannot carry.
class RectangularPolarGraph : public QCPPolarGraph
{
public:
    RectangularPolarGraph(QCPPolarAxisAngular* angular, QCPPolarAxisRadial* radial)
        : QCPPolarGraph(angular, radial, QCP::capRectangular)
    {
    }
};

int redInkCount(const QImage& image)
{
    int count = 0;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            if (isRedInk(image.pixelColor(x, y)))
                ++count;
    return count;
}
} // namespace

void TestPolar::clickingAPolarGraphSelectsThePoint()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    QVector<double> keys, values;
    for (int i = 0; i < 16; ++i)
    {
        keys << i * 22.5;
        values << 0.5;
    }
    auto graph = area->add<QCPPolarGraph>(keys, values);
    QVERIFY(graph);
    graph->setPen(QPen(Qt::blue, 3));
    graph->setSelectable(QCP::stSingleData);
    plot.setInteractions(QCP::iSelectPlottables);
    settle(plot);

    // A click on the eighth point selects exactly that point.
    const QPoint eighth = area->radialAxis()->coordToPixel(7 * 22.5, 0.5).toPoint();
    sendMouse(&plot, QEvent::MouseButtonPress, eighth, Qt::LeftButton);
    sendMouse(&plot, QEvent::MouseButtonRelease, eighth, Qt::NoButton);
    QVERIFY(graph->selected());
    QCOMPARE(graph->selection().dataRangeCount(), 1);
    QCOMPARE(graph->selection().dataRange(0), QCPDataRange(7, 8));

    // The details of a hit are that point too, and a click outside the disc misses.
    QVariant details;
    QVERIFY(graph->selectTest(eighth, false, &details) >= 0);
    QCOMPARE(details.value<QCPDataSelection>().dataRange(0), QCPDataRange(7, 8));
    QVERIFY(graph->selectTest(QPointF(5, 5), false, &details) < 0);
}

void TestPolar::rubberBandSelectsPolarPoints()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    QVector<double> keys, values;
    for (int i = 0; i < 16; ++i)
    {
        keys << i * 22.5;
        values << 0.5;
    }
    auto graph = area->add<QCPPolarGraph>(keys, values);
    QVERIFY(graph);
    graph->setSelectable(QCP::stMultipleDataRanges);
    settle(plot);

    // The band covers points 2 to 5 and nothing else.
    QPointF low, high;
    for (int i = 2; i <= 5; ++i)
    {
        const QPointF pixel = area->radialAxis()->coordToPixel(keys[i], values[i]);
        low = i == 2 ? pixel : QPointF(qMin(low.x(), pixel.x()), qMin(low.y(), pixel.y()));
        high = i == 2 ? pixel : QPointF(qMax(high.x(), pixel.x()), qMax(high.y(), pixel.y()));
    }
    QRectF band(low, high);
    band.adjust(-1, -1, 1, 1);
    QVERIFY(graph->interface1D());
    const QCPDataSelection inside = graph->interface1D()->selectTestRect(band, false);
    QCOMPARE(inside.dataRangeCount(), 1);
    QCOMPARE(inside.dataRange(0), QCPDataRange(2, 6));
}

void TestPolar::aSelectedPolarGraphLooksSelected()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 90, 180, 270 },
                                          QVector<double> { 0.6, 0.6, 0.6, 0.6 });
    QVERIFY(graph);
    graph->setPen(QPen(Qt::blue, 3));
    auto* decorator = new QCPSelectionDecorator;
    decorator->setPen(QPen(Qt::red, 3));
    graph->setSelectionDecorator(decorator);
    graph->setSelection(QCPDataSelection(QCPDataRange(0, 4)));
    settle(plot);
    QVERIFY2(redInkCount(imageOf(plot)) > 0, "the decorator's pen never reached the line");

    graph->setSelection(QCPDataSelection());
    settle(plot);
    QCOMPARE(redInkCount(imageOf(plot)), 0);
}

void TestPolar::aPolarGraphJoinsAGroup()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 180 },
                                          QVector<double> { 0.5, 0.5 });
    QVERIFY(graph);
    QCPPlottableGroup* group = plot.addPlottableGroup("polar");
    QVERIFY(group->addMember(graph.get()));
    QCOMPARE(graph->group(), group);
    QCOMPARE(group->members(), QList<QCPAbstractPlottable*> { graph.get() });
    settle(plot);
}

void TestPolar::areaAddTakesAnyPolarCapablePlottable()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);

    // The area's add takes a polar graph without a special case.
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 180 },
                                          QVector<double> { 0.5, 0.5 });
    QVERIFY(graph);
    QCOMPARE(plot.plottableCount(), 1);

    // A plottable needing a frame the polar system does not carry is refused, once.
    auto refused = area->add<RectangularPolarGraph>(QVector<double> { 0, 180 },
                                                    QVector<double> { 0.5, 0.5 });
    QVERIFY(!refused);
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    test_misuse::calls.clear();
    QCOMPARE(plot.plottableCount(), 1);
}

void TestPolar::aPolarGraphRefusesAForeignSystem()
{
    QCustomPlot plot;
    plot.resize(400, 600);
    auto* area = new QCPPolarArea(&plot);
    plot.plotLayout()->addElement(1, 0, area);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 90, 180 },
                                          QVector<double> { 0.5, 0.6, 0.5 });
    QVERIFY(graph);
    graph->setPen(QPen(Qt::blue, 3));
    settle(plot);
    QVERIFY(graph->system());

    // A cartesian frame is not one a polar graph can draw in: it says so once and keeps its own.
    QCPCoordinateSystem* cartesian = plot.axisRect()->system(plot.xAxis, plot.yAxis);
    QVERIFY(cartesian);
    graph->setSystem(cartesian);
    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    QVERIFY(graph->system() != cartesian);
    QCOMPARE(graph->system()->area(), static_cast<QCPArea*>(area));

    settle(plot);
    QVERIFY2(isInk(pixelAt(plot, area->radialAxis()->coordToPixel(0, 0.5).toPoint())),
             "the refused system stopped the graph from drawing");
    test_misuse::calls.clear();
}

void TestPolar::aLegendIconShowsTheGraphsStyles()
{
    QCustomPlot plot;
    plot.plotLayout()->clear();
    plot.legend = new QCPLegend;
    plot.plotLayout()->addElement(1, 0, plot.legend);
    auto* area = new QCPPolarArea(&plot);
    plot.plotLayout()->addElement(0, 0, area);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 90 },
                                          QVector<double> { 0.4, 0.4 });
    QVERIFY(graph);
    graph->setPen(QPen(Qt::red, 3));
    settle(plot);
    QVERIFY2(redInkCount(imageOf(plot)) > 0, "the icon did not take the graph's pen");

    // An empty graph still has an icon: it says what the graph would look like.
    graph->setData(QVector<double> { }, QVector<double> { });
    settle(plot);
    QVERIFY2(redInkCount(imageOf(plot)) > 0, "an empty graph lost its icon");
}

void TestPolar::rescaledAxesFollowTheData()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    QCPPolarAxisRadial* radial = area->radialAxis();
    area->angularAxis()->setRange(0, 360);
    radial->setRange(0, 1);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 120, 240 },
                                          QVector<double> { 2, 3, 4 });
    QVERIFY(graph);

    // The rescale reads the data, not the recording, whose radial extent is the rim it clamps to.
    graph->rescaleAxes();
    QCOMPARE(radial->range().lower, 2.0);
    QCOMPARE(radial->range().upper, 4.0);
    graph->rescaleAxes();
    QCOMPARE(radial->range().lower, 2.0);
    QCOMPARE(radial->range().upper, 4.0);
}

void TestPolar::rescaledKeysFollowTheDataInOneCall()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    QCPPolarAxisAngular* angular = area->angularAxis();
    area->radialAxis()->setRange(0, 1);
    angular->setRange(0, 360);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 720, 810, 900 },
                                          QVector<double> { 0.5, 0.5, 0.5 });
    QVERIFY(graph);
    graph->setPeriodic(false);

    // 2.1's rescale asked the data: one call frames the keys, even far outside the current range.
    graph->rescaleKeyAxis();
    QCOMPARE(angular->range().lower, 720.0);
    QCOMPARE(angular->range().upper, 900.0);
}

void TestPolar::theVisiblePartOfALeavingSegmentIsHittable()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 30 },
                                          QVector<double> { 0.2, 1.6 });
    QVERIFY(graph);
    graph->setPen(QPen(Qt::blue, 3));
    settle(plot);

    // The segment leaves the disc: the recording holds its visible part, from (0, 0.2) to the rim
    // crossing, then the hidden part along the rim to (30, 1.6). Click the visible part, near the rim.
    const auto lines = recordedPolylines(graph->mCore);
    QCOMPARE(lines.size(), std::size_t(1));
    QVERIFY(lines.front().size() >= 2);
    const QCPTransform frame = graph->system()->transform();
    const QCP::Pixels start
        = frame.map(QCP::Data { lines.front().front().x(), lines.front().front().y() });
    const QCP::Pixels crossing = frame.map(QCP::Data { lines.front()[1].x(), lines.front()[1].y() });
    const QPointF onTheChord(start.x + (crossing.x - start.x) * 0.9,
                             start.y + (crossing.y - start.y) * 0.9);
    QVERIFY2(isInk(pixelAt(plot, onTheChord.toPoint())), "sanity: the click point is on the line");

    QVariant details;
    QVERIFY2(graph->selectTest(onTheChord, false, &details) >= 0,
             "a visible line cannot be clicked");
    // The details are the nearest in-range point: the far end of the segment is outside the disc.
    QCOMPARE(details.value<QCPDataSelection>().dataRange(0), QCPDataRange(0, 1));
}

void TestPolar::hiddenGeometryIsNotHittable()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    QCPPolarAxisRadial* radial = area->radialAxis();
    area->angularAxis()->setRange(0, 360);
    radial->setRange(0, 1);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 45, 90 },
                                          QVector<double> { 2, 2, 2 });
    QVERIFY(graph);
    graph->setPen(QPen(Qt::blue, 4));
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc));
    settle(plot);

    // The run is drawn on the rim, hidden outside the disc: the space just inside it is not the graph.
    QVERIFY(graph->selectTest(radial->coordToPixel(45, 0.98), false) < 0);
    plot.resize(200, 150);
    plot.replot();
    QCoreApplication::processEvents();
    QVERIFY(graph->selectTest(radial->coordToPixel(45, 0.98), false) < 0);
}

void TestPolar::theBandIgnoresPointsOutsideTheRange()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 120, 240 },
                                          QVector<double> { -1, 0.5, 0.5 });
    QVERIFY(graph);
    settle(plot);

    // The point below the range is drawn at the centre, but it is outside the range: a band around
    // the centre does not select it.
    const QPointF centre = area->center();
    const QCPDataSelection inside
        = graph->interface1D()->selectTestRect(QRectF(centre - QPointF(6, 6), QSizeF(12, 12)), false);
    QVERIFY(!inside.contains(QCPDataSelection(QCPDataRange(0, 1))));
}

void TestPolar::rubberBandDragSelectsPolarPoints()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    QVector<double> angle, radius;
    for (int i = 0; i < 16; ++i)
    {
        angle << i * 22.5;
        radius << 0.5;
    }
    auto graph = area->add<QCPPolarGraph>(angle, radius);
    QVERIFY(graph);
    graph->setSelectable(QCP::stMultipleDataRanges);
    graph->setPen(QPen(Qt::blue, 3));
    plot.setInteractions(QCP::iSelectPlottables);
    plot.setSelectionRectMode(QCP::srmSelect);
    settle(plot);

    QPointF low, high;
    for (int i = 2; i != 6; ++i)
    {
        const QPointF pixel = area->radialAxis()->coordToPixel(i * 22.5, 0.5);
        low = i == 2 ? pixel : QPointF(qMin(low.x(), pixel.x()), qMin(low.y(), pixel.y()));
        high = i == 2 ? pixel : QPointF(qMax(high.x(), pixel.x()), qMax(high.y(), pixel.y()));
    }
    const QPointF from = low - QPointF(4, 4);
    const QPointF to = high + QPointF(4, 4);
    sendMouse(&plot, QEvent::MouseButtonPress, from.toPoint(), Qt::LeftButton);
    sendMouse(&plot, QEvent::MouseMove, to.toPoint(), Qt::LeftButton);
    sendMouse(&plot, QEvent::MouseButtonRelease, to.toPoint(), Qt::NoButton);

    QVERIFY2(graph->selection().contains(QCPDataSelection(QCPDataRange(2, 6))),
             "the rubber band did not select the points it covered");
    QVERIFY(!graph->selection().contains(QCPDataSelection(QCPDataRange(8, 9))));
}

void TestPolar::aMarkerTakesTheGraphPen()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 45 }, QVector<double> { 0.5 });
    QVERIFY(graph);
    graph->setPen(QPen(Qt::blue, 3));
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 10));
    area->grid()->setVisible(false);
    area->angularAxis()->setVisible(false);
    area->radialAxis()->setVisible(false);
    settle(plot);

    // The style carries no pen of its own: the marker takes the graph's, not Qt's default black.
    const QColor pixel = pixelAt(plot, area->radialAxis()->coordToPixel(45, 0.5).toPoint());
    QVERIFY2(isInk(pixel), qPrintable(pixel.name()));
}

void TestPolar::aMarkerOutsideTheRangeIsNotDrawn()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(1, 10);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 45 }, QVector<double> { 0.5 });
    QVERIFY(graph);
    graph->setLineStyle(QCPPolarGraph::lsNone);
    graph->setScatterStyle(
        QCPScatterStyle(QCPScatterStyle::ssDisc, QPen(Qt::blue), QBrush(Qt::blue), 10));
    area->grid()->setVisible(false);
    area->angularAxis()->setVisible(false);
    area->radialAxis()->setVisible(false);
    settle(plot);

    // 2.1's scatter clip: a value outside the radial range (0.5 on 1..10) gets no marker, so nothing
    // is inked at the centre where it would land.
    QVERIFY2(!isInk(pixelAt(plot, area->center().toPoint())), "a marker outside the range drew");
    QCOMPARE(inkInSector(*area, imageOf(plot), -1, 361, 1.0), 0);
}

void TestPolar::aSelectedMarkerTakesTheDecoratorStyle()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 90, 180, 270 },
                                          QVector<double> { 0.6, 0.6, 0.6, 0.6 });
    QVERIFY(graph);
    graph->setLineStyle(QCPPolarGraph::lsNone);
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 10));
    graph->setPen(QPen(Qt::blue, 3));
    auto* decorator = new QCPSelectionDecorator;
    decorator->setPen(QPen(Qt::red, 3));
    decorator->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 10));
    graph->setSelectionDecorator(decorator);
    area->grid()->setVisible(false);
    area->angularAxis()->setVisible(false);
    area->radialAxis()->setVisible(false);
    settle(plot);
    QCOMPARE(redInkCount(imageOf(plot)), 0);

    graph->setSelection(QCPDataSelection(QCPDataRange(0, 4)));
    settle(plot);
    QVERIFY2(redInkCount(imageOf(plot)) > 0, "the selected markers kept the unselected style");

    graph->setSelection(QCPDataSelection());
    settle(plot);
    QCOMPARE(redInkCount(imageOf(plot)), 0);
}

void TestPolar::aLineSegmentTakesTheSelectedStyleOnlyWhenBothEndsAreSelected()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    QVector<double> angle, radius;
    for (int i = 0; i < 20; ++i)
    {
        angle << i * 18.0;
        radius << 0.6;
    }
    auto graph = area->add<QCPPolarGraph>(angle, radius);
    QVERIFY(graph);
    graph->setPen(QPen(Qt::blue, 3));
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 8));
    auto* decorator = new QCPSelectionDecorator;
    decorator->setPen(QPen(Qt::red, 3));
    decorator->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 8));
    graph->setSelectionDecorator(decorator);
    graph->setSelection(QCPDataSelection(QCPDataRange(7, 8)));
    area->grid()->setVisible(false);
    area->angularAxis()->setVisible(false);
    area->radialAxis()->setVisible(false);
    settle(plot);

    // 2.x styles a segment when both of its ends are selected: the two segments that touch point 7
    // keep the graph's pen, and only the marker takes the decorated one.
    for (const auto& command : graph->mCore.recordedList().commands)
        if (const auto* line = std::get_if<qcp::PolylineDataCmd>(&command))
            QVERIFY2(line->pen.color() != QColor(Qt::red),
                     "a segment with one unselected end took the selection pen");
    QVERIFY2(redInkCount(imageOf(plot)) > 0, "the selected marker did not take the decorated style");
}

void TestPolar::aPointBelowTheRangeSitsAtTheCentre()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 90 }, QVector<double> { -1, 2 });
    QVERIFY(graph);
    graph->setPen(QPen(Qt::blue, 3));
    area->grid()->setVisible(false);
    area->angularAxis()->setVisible(false);
    area->radialAxis()->setVisible(false);
    settle(plot);

    // The point below the range sits at the centre, as the replay draws it, so the visible segment
    // runs along the 90 degree spoke: nothing is drawn towards 180 degrees.
    QCOMPARE(inkInSector(*area, imageOf(plot), 170, 190, 0.97, 0.08), 0);
    QVERIFY(isInk(pixelAt(plot, area->radialAxis()->coordToPixel(90, 0.5).toPoint())));
}

void TestPolar::aRunLeavingTheRimFollowsIt_data()
{
    QTest::addColumn<double>("lower");
    QTest::addColumn<double>("upper");
    QTest::addColumn<double>("beyond");
    QTest::addColumn<double>("justInside");
    QTest::addColumn<bool>("logarithmic");
    QTest::addColumn<bool>("reversed");
    QTest::newRow("linear") << 0.0 << 1.0 << 1.6 << 0.98 << false << false;
    QTest::newRow("log radial") << 1.0 << 1000.0 << std::pow(1000.0, 1.6)
                                << std::pow(1000.0, 0.98) << true << false;
    QTest::newRow("reversed radial") << 0.0 << 1.0 << -0.6 << 0.02 << false << true;
}

void TestPolar::aRunLeavingTheRimFollowsIt()
{
    QFETCH(double, lower);
    QFETCH(double, upper);
    QFETCH(double, beyond);
    QFETCH(double, justInside);
    QFETCH(bool, logarithmic);
    QFETCH(bool, reversed);

    QCustomPlot plot;
    auto* area = polarArea(plot);
    QCPPolarAxisAngular* angular = area->angularAxis();
    QCPPolarAxisRadial* radial = area->radialAxis();
    angular->setRange(0, 360);
    radial->setRange(lower, upper);
    radial->setScaleType(logarithmic ? QCPAxisBase::stLogarithmic : QCPAxisBase::stLinear);
    radial->setRangeReversed(reversed);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 90, 140 },
                                          QVector<double> { beyond, justInside });
    QVERIFY(graph);
    graph->setPen(QPen(Qt::blue, 8));
    area->grid()->setVisible(false);
    angular->setVisible(false);
    radial->setVisible(false);
    settle(plot);

    // Leaving the disc follows the rim before it comes back in: the sector between the beyond point
    // and the crossing keeps no ink, not even the sliver a thick pen would leave.
    QCOMPARE(inkInSector(*area, imageOf(plot), 95, 125, 0.995), 0);
    // And the rim is used: the run records points on it between the two data points.
    const auto lines = recordedPolylines(graph->mCore);
    QVERIFY(!lines.empty());
    int onTheRim = 0;
    for (const QPointF& point : lines.front())
        if (point.x() > 90 && point.x() < 140 && point.y() == radial->radiusToCoord(area->radius()))
            ++onTheRim;
    QVERIFY2(onTheRim >= 1, "the run left the disc without going round it");
}

void TestPolar::aThickPenLeavesNoSliverAlongTheRim()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 90 },
                                          QVector<double> { 2, 2 });
    QVERIFY(graph);
    graph->setPen(QPen(Qt::blue, 8));
    area->grid()->setVisible(false);
    area->angularAxis()->setVisible(false);
    area->radialAxis()->setVisible(false);
    settle(plot);

    // Both points are beyond the rim: the run goes round it at the clamp radius, so the pen's width
    // stays outside the disc instead of bleeding in along a chord.
    QCOMPARE(inkInSector(*area, imageOf(plot), 2, 88, 0.995), 0);
}

void TestPolar::aCrossingTakesTheBranchNearestTheBeyondPoint()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 300 },
                                          QVector<double> { 0.5, 2 });
    QVERIFY(graph);
    graph->setPen(QPen(Qt::NoPen));
    graph->setBrush(QBrush(Qt::red));
    area->grid()->setVisible(false);
    area->angularAxis()->setVisible(false);
    area->radialAxis()->setVisible(false);
    settle(plot);

    // The crossing belongs to the branch the beyond point is on: the fill does not wrap the long way
    // round the disc.
    QVERIFY2(!isRedInk(pixelAt(plot, area->radialAxis()->coordToPixel(180, 0.5).toPoint())),
             "the fill wrapped through 180 degrees");
}

void TestPolar::aClampedRunFollowsTheArc_data()
{
    QTest::addColumn<double>("lower");
    QTest::addColumn<double>("upper");
    QTest::addColumn<double>("beyond");
    QTest::addColumn<bool>("logarithmic");
    QTest::addColumn<bool>("reversed");
    QTest::newRow("linear") << 0.0 << 1.0 << 2.0 << false << false;
    QTest::newRow("log radial") << 1.0 << 1000.0 << 1e6 << true << false;
    QTest::newRow("reversed radial") << 0.0 << 1.0 << -1.0 << false << true;
}

void TestPolar::aClampedRunFollowsTheArc()
{
    QFETCH(double, lower);
    QFETCH(double, upper);
    QFETCH(double, beyond);
    QFETCH(bool, logarithmic);
    QFETCH(bool, reversed);

    QCustomPlot plot;
    auto* area = polarArea(plot);
    QCPPolarAxisAngular* angular = area->angularAxis();
    QCPPolarAxisRadial* radial = area->radialAxis();
    angular->setRange(0, 360);
    radial->setRange(lower, upper);
    radial->setScaleType(logarithmic ? QCPAxisBase::stLogarithmic : QCPAxisBase::stLinear);
    radial->setRangeReversed(reversed);
    // A run that stays beyond the rim from angle 0 to angle 90.
    auto graph
        = area->add<QCPPolarGraph>(QVector<double> { 0, 90 }, QVector<double> { beyond, beyond });
    QVERIFY(graph);
    graph->setPen(QPen(Qt::blue, 3));
    area->grid()->setVisible(false);
    angular->setVisible(false);
    radial->setVisible(false);
    settle(plot);

    // The run is recorded beyond the rim and densified there, never as one chord through the disc.
    const auto lines = recordedPolylines(graph->mCore);
    QCOMPARE(lines.size(), std::size_t(1));
    QVERIFY(lines.front().size() >= 4);
    int densified = 0;
    for (const QPointF& point : lines.front())
    {
        const double fraction = radial->coordToRadius(point.y()) / area->radius();
        QVERIFY2(fraction >= 1.0 - 1e-9, qPrintable(QString::number(fraction)));
        if (point.x() > 0 && point.x() < 90)
        {
            ++densified;
            QVERIFY2(fraction > 1.0, "a point between the two data points came inside the rim");
        }
    }
    QVERIFY2(densified >= 2, "the run beyond the rim was recorded as one chord");
    // And the clip hides it: the run goes round the rim, so no pixel lies inside the disc between the
    // two angles, nor in the band a pen a few pixels wide would reach if a chord ran there.
    QCOMPARE(inkInSector(*area, imageOf(plot), -1, 91, 0.995), 0);
}

void TestPolar::aSparseInRangePairIsAChord()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 90 }, QVector<double> { 0.9, 0.9 });
    QVERIFY(graph);
    graph->setPen(QPen(Qt::blue, 4));
    area->grid()->setVisible(false);
    area->angularAxis()->setVisible(false);
    area->radialAxis()->setVisible(false);
    settle(plot);

    // 2.1's chord: two in-range points 90 degrees apart stay two points.
    const auto lines = recordedPolylines(graph->mCore);
    QCOMPARE(lines.size(), std::size_t(1));
    QCOMPARE(lines.front().size(), std::size_t(2));
    const QPoint middle
        = area->radialAxis()->coordToPixel(45, 0.9 * std::cos(std::numbers::pi / 4)).toPoint();
    const QColor pixel = pixelAt(plot, middle);
    QVERIFY2(isInk(pixel), qPrintable(pixel.name()));
}

void TestPolar::aNaNBreaksTheLine()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 90, 180, 270, 360 },
                                          QVector<double> { 0.5, qQNaN(), 0.6, 0.7, 0.8 });
    QVERIFY(graph);
    settle(plot);

    const auto lines = recordedPolylines(graph->mCore);
    QCOMPARE(lines.size(), std::size_t(2));
    QCOMPARE(lines[0].size(), std::size_t(1));
    QCOMPARE(lines[1].size(), std::size_t(3));
}

void TestPolar::aScatterOnlyGraphRecordsNoLine()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 90, 180 },
                                          QVector<double> { 0.5, 0.6, 0.7 });
    QVERIFY(graph);
    graph->setLineStyle(QCPPolarGraph::lsNone);
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 8));
    graph->setBrush(QBrush(Qt::red)); // a fill without a line has nothing to fill
    settle(plot);

    QCOMPARE(recordedPolylines(graph->mCore).size(), std::size_t(0));
    QCOMPARE(recordedPolygonSize(graph->mCore), std::size_t(0));
    QCOMPARE(recordedMarkerCount(graph->mCore), std::size_t(3));
}

void TestPolar::aFilledArcClosesWithAChord()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 45, 90 },
                                          QVector<double> { 0.5, 0.5, 0.5 });
    QVERIFY(graph);
    graph->setPen(QPen(Qt::NoPen)); // only the fill talks
    graph->setBrush(QBrush(Qt::red));
    area->grid()->setVisible(false);
    area->angularAxis()->setVisible(false);
    area->radialAxis()->setVisible(false);
    settle(plot);

    // The fill is the recorded line closed last to first: its vertices, no centre point.
    QCOMPARE(recordedPolygonSize(graph->mCore), std::size_t(3));
    const QColor centre = pixelAt(plot, area->center().toPoint());
    QVERIFY2(!isRedInk(centre), qPrintable(centre.name()));
    const QPoint inside = area->radialAxis()->coordToPixel(45, 0.45).toPoint();
    const QColor painted = pixelAt(plot, inside);
    QVERIFY2(isRedInk(painted), qPrintable(painted.name()));
}

void TestPolar::deletingTheRadialAxisLeavesTheGraphInert()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 180 }, QVector<double> { 0.5, 0.5 });
    QVERIFY(graph);
    settle(plot);

    delete area->radialAxis(0);
    QCOMPARE(area->radialAxisCount(), 0);
    QVERIFY2(graph->system() == nullptr, "the system of a deleted radial axis outlived it");
    settle(plot);
    test_misuse::calls.clear(); // drawing an inert graph reports it once: that is the inert rule

    // Inert, not deleted: the data is still there and the plot still draws.
    QCOMPARE(graph->data()->size(), 2);
    delete area; // a layout element unregisters itself: remove() would delete it
    settle(plot);
}

void TestPolar::deletingTheGraphFromADragSlot()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    plot.setInteractions(QCP::iRangeDrag);
    QCPPolarAxisRadial* radial = area->radialAxis();
    radial->setRangeDraggable(true);
    area->angularAxis()->setRange(0, 360);
    radial->setRange(0, 1);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 180 }, QVector<double> { 0.5, 0.5 });
    QVERIFY(graph);
    QPointer<QCPPolarGraph> alive(graph.get());
    connect(
        radial, &QCPAxisBase::rangeChanged, area, [&alive] { delete alive.data(); },
        Qt::DirectConnection);

    const QPoint from = radial->coordToPixel(0, 0.3).toPoint();
    const QPoint to = radial->coordToPixel(0, 0.6).toPoint();
    sendMouse(&plot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(&plot, QEvent::MouseMove, to, Qt::LeftButton);
    sendMouse(&plot, QEvent::MouseButtonRelease, to, Qt::NoButton);

    QVERIFY(!alive);
    settle(plot);
}

void TestPolar::deletingTheAreaDoesNotDeleteTheGraph()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 180 }, QVector<double> { 0.5, 0.5 });
    QVERIFY(graph);
    QPointer<QCPPolarGraph> alive(graph.get());
    settle(plot);

    delete area; // a layout element unregisters itself: remove() would delete it

    QVERIFY2(alive, "the area deleted a graph the plot owns");
    QVERIFY(graph->system() == nullptr);
    QCOMPARE(plot.plottableCount(), 1);
    settle(plot);
    test_misuse::calls.clear(); // drawing a graph without a system reports it once
    QCOMPARE(graph->data()->size(), 2);
}

void TestPolar::backgroundStaysInsideTheDisc()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    area->setBackground(QBrush(Qt::red));
    area->grid()->setVisible(false);
    area->radialAxis()->setVisible(false); // nothing drawn over the centre
    settle(plot);

    // The area's background is the disc, not the rect: the corner stays the plot's paper.
    const QColor corner = pixelAt(plot, area->rect().topLeft() + QPoint(5, 5));
    const bool cornerIsRed = corner.red() > 200 && corner.green() < 60 && corner.blue() < 60;
    QVERIFY2(!cornerIsRed, qPrintable(corner.name()));

    const QColor centre = pixelAt(plot, area->center().toPoint());
    const bool centreIsRed = centre.red() > 200 && centre.green() < 60 && centre.blue() < 60;
    QVERIFY2(centreIsRed, qPrintable(centre.name()));
}

void TestPolar::insetAndBackgroundAreAreaFeatures()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    settle(plot);

    auto* legend = new QCPLegend;
    area->insetLayout()->addElement(legend, Qt::AlignRight | Qt::AlignTop);
    area->setBackground(QBrush(Qt::red));
    area->grid()->setVisible(false);
    settle(plot);

    // The inset layout aligns the legend's outer rect to the area's: QRect::right() is inclusive.
    QVERIFY(qAbs(legend->outerRect().right() - area->rect().right()) <= 1);
    QVERIFY(qAbs(legend->outerRect().top() - area->rect().top()) <= 1);
    // The background follows the disc, so the corner outside it keeps the paper.
    const QColor corner = pixelAt(plot, area->rect().topLeft() + QPoint(5, 5));
    QVERIFY2(!(corner.red() > 200 && corner.green() < 60 && corner.blue() < 60),
             qPrintable(corner.name()));
    const QColor middle = pixelAt(plot, area->center().toPoint() + QPoint(40, 40));
    QVERIFY2(middle.red() > 200 && middle.green() < 60 && middle.blue() < 60,
             qPrintable(middle.name()));
}

void TestPolar::theAreaKeepsTheOldMinimumMargins()
{
    // The angular axis asked for at least 30 px of margin; the area must not hand out less, or the
    // tick labels that used to fit get clipped.
    QCustomPlot plot;
    auto* area = polarArea(plot);
    QCOMPARE(area->minimumMargins(), QMargins(30, 30, 30, 30));
}

void TestPolar::theAngularAxisRefusesALogScale()
{
    // The ring's geometry is linear in the angle: a logarithmic scale would change the tick vector
    // and nothing else, so it is refused.
    QCustomPlot plot;
    auto* area = polarArea(plot);
    QCPPolarAxisAngular* angular = area->angularAxis();
    angular->setScaleType(QCPAxisBase::stLogarithmic);
    QCOMPARE(angular->scaleType(), QCPAxisBase::stLinear);
    QCOMPARE(int(test_misuse::calls.size()), 1);
    test_misuse::calls.clear();
}

void TestPolar::angularWheelZoomRequeuesTheRepaint()
{
    // With only the angular axis zoomable, the wheel changes what the ring and the graphs draw:
    // the area must ask for that repaint itself.
    QCustomPlot plot;
    auto* area = polarArea(plot);
    QCPPolarAxisAngular* angular = area->angularAxis();
    angular->setRangeZoomable(true);
    area->radialAxis()->setRangeZoomable(false);
    plot.setInteractions(QCP::iRangeZoom);
    angular->setRange(0, 360);
    settle(plot);
    QVERIFY(!plot.isRepaintQueued());

    const QPointF cursor = area->center() + QPointF(0.5 * area->radius(), 0);
    QWheelEvent wheel(cursor, plot.mapToGlobal(cursor.toPoint()), QPoint(), QPoint(0, 120),
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(&plot, &wheel);

    QVERIFY2(angular->range().size() < 360,
             qPrintable(QString::number(angular->range().size())));
    QVERIFY2(plot.isRepaintQueued(), "an angular-only wheel zoom scheduled no repaint");
}

void TestPolar::polarAxisPartsAreClickable()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    QCPPolarAxisAngular* angular = area->angularAxis();
    QCPPolarAxisRadial* radial = area->radialAxis();
    angular->setRange(0, 360);
    radial->setRange(0, 1);
    settle(plot);

    // The angular axis is the circle: the point straight right of the centre is on its backbone.
    const QPointF onCircle = area->center() + QPointF(area->radius(), 0);
    QCOMPARE(partAt(angular, onCircle), QCPAxisBase::spAxis);
    QCOMPARE(angular->selectTest(onCircle, false), plot.selectionTolerance() * 0.99);

    const QPointF angularLabel = findPart(angular, area->rect(), QCPAxisBase::spTickLabels);
    QVERIFY2(angularLabel.x() >= 0, "no angular tick label is hittable");

    // The radial axis is a spoke from the centre, at its angle measured against the angular axis.
    const double spokeRad = (radial->angle() + angular->angle()) / 180.0 * M_PI;
    const QPointF onSpoke = radial->coordToPixel(angular->angleRadToCoord(spokeRad), 0.5);
    QCOMPARE(partAt(radial, onSpoke), QCPAxisBase::spAxis);

    const QPointF radialLabel = findPart(radial, area->rect(), QCPAxisBase::spTickLabels);
    QVERIFY2(radialLabel.x() >= 0, "no radial tick label is hittable");

    // The area's corner is far outside the circle and every label box.
    const QPointF corner = area->rect().topLeft() + QPoint(2, 2);
    QCOMPARE(partAt(angular, corner), QCPAxisBase::spNone);
    QCOMPARE(partAt(radial, corner), QCPAxisBase::spNone);

    // Unselectable parts are refused when only selectable axes are wanted.
    angular->setSelectableParts(QCPPolarAxisAngular::spNone);
    QCOMPARE(angular->selectTest(onCircle, true, nullptr), -1.0);
    QVERIFY(angular->selectTest(onCircle, false, nullptr) > 0.0);
    radial->setSelectableParts(QCPPolarAxisRadial::spNone);
    QCOMPARE(radial->selectTest(onSpoke, true, nullptr), -1.0);
    QVERIFY(radial->selectTest(onSpoke, false, nullptr) > 0.0);
}

void TestPolar::radialDragAndWheelWork()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    QCPPolarAxisAngular* angular = area->angularAxis();
    QCPPolarAxisRadial* radial = area->radialAxis();
    plot.setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    angular->setRange(0, 360);
    radial->setRange(0, 1);
    settle(plot);

    // A wheel zooms the radial range about the radius under the cursor.
    const QPointF cursor = area->center() + QPointF(0.5 * area->radius(), 0);
    double angleBefore = 0, radiusBefore = 0, angleAfter = 0, radiusAfter = 0;
    angular->pixelToCoord(cursor, angleBefore, radiusBefore);
    QWheelEvent wheel(cursor, plot.mapToGlobal(cursor.toPoint()), QPoint(), QPoint(0, 120),
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(&plot, &wheel);
    angular->pixelToCoord(cursor, angleAfter, radiusAfter);
    QVERIFY2(qAbs(radial->range().size() - 0.85) < 1e-9,
             qPrintable(QString::number(radial->range().size())));
    QVERIFY2(qAbs(radiusAfter - radiusBefore) < 1e-9,
             qPrintable(QString::number(radiusAfter - radiusBefore)));
    QCOMPARE(angleAfter, angleBefore);

    // A drag along the spoke pans it by the radius the cursor moved.
    radial->setRange(0, 1);
    settle(plot);
    const QPointF from = radial->coordToPixel(0.0, 0.3);
    const QPointF to = radial->coordToPixel(0.0, 0.6);
    const QCPRange angularBefore = angular->range();
    sendMouse(&plot, QEvent::MouseButtonPress, from.toPoint(), Qt::LeftButton);
    sendMouse(&plot, QEvent::MouseMove, to.toPoint(), Qt::LeftButton);
    sendMouse(&plot, QEvent::MouseButtonRelease, to.toPoint(), Qt::NoButton);
    QVERIFY2(qAbs(radial->range().size() - 1.0) < 1e-9,
             qPrintable(QString::number(radial->range().size())));
    QVERIFY2(qAbs(radial->range().lower + 0.3) < 1e-9,
             qPrintable(QString::number(radial->range().lower)));
    QCOMPARE(angular->range(), angularBefore);
}

void TestPolar::hidingTheAngularAxisKeepsTheGraphs()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 180 }, QVector<double> { 0.5, 0.5 });
    QVERIFY(graph);
    graph->setPen(QPen(Qt::blue, 4));
    area->angularAxis()->setVisible(false);
    area->radialAxis()->setVisible(false);
    area->grid()->setVisible(false);
    settle(plot);

    // The graphs belong to the area, not to the ring: hiding the ring keeps the data.
    QCOMPARE(graph->parentLayerable(), static_cast<QCPLayerable*>(area));
    QVERIFY(graph->realVisibility());
    const QColor pixel = pixelAt(plot, (area->center() + QPointF(0, 0.5 * area->radius())).toPoint());
    QVERIFY2(pixel.blue() > 150 && pixel.red() < 120, qPrintable(pixel.name()));
}

void TestPolar::deletingAPolarChildIsSafe()
{
    // The area holds its children by raw pointer: a direct delete must unlist, or the next replot
    // and the area's own teardown reach freed memory.
    {
        QCustomPlot plot;
        auto* area = polarArea(plot);
        auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 180 }, QVector<double> { 1, 1 });
        QVERIFY(graph);
        delete area->angularAxis();
        QCOMPARE(area->angularAxis(), nullptr);
        settle(plot); // the graph lost its system: it reports that once, then stays inert
        test_misuse::calls.clear();
        QVERIFY(plot.plotLayout()->remove(area));
        settle(plot);
    }
    {
        QCustomPlot plot;
        auto* area = polarArea(plot);
        delete area->radialAxis(0);
        QCOMPARE(area->radialAxisCount(), 0);
        settle(plot);
        QVERIFY(plot.plotLayout()->remove(area));
        settle(plot);
    }
    {
        QCustomPlot plot;
        auto* area = polarArea(plot);
        delete area->grid();
        QCOMPARE(area->grid(), nullptr);
        settle(plot);
        QVERIFY(plot.plotLayout()->remove(area));
        settle(plot);
    }
}

void TestPolar::deletingTheAngularAxisMidDragIsSafe()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    plot.setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    QCPPolarAxisAngular* angular = area->angularAxis();
    angular->setRangeDraggable(true);
    angular->setRangeZoomable(true);
    angular->setRange(0, 360);

    QPointer<QCPPolarAxisAngular> alive(angular);
    connect(
        angular, &QCPPolarAxisAngular::rangeChanged, area, [&alive] { delete alive.data(); },
        Qt::DirectConnection);

    const QPoint from = area->center().toPoint() + QPoint(int(0.6 * area->radius()), 0);
    const QPoint to = area->center().toPoint() + QPoint(0, int(0.6 * area->radius()));
    sendMouse(&plot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(&plot, QEvent::MouseMove, to, Qt::LeftButton);
    sendMouse(&plot, QEvent::MouseButtonRelease, to, Qt::NoButton);

    QVERIFY(!alive);
    QCOMPARE(area->angularAxis(), nullptr);
    settle(plot);
}
