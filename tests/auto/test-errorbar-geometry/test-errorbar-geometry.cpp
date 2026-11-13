#include "test-errorbar-geometry.h"
#include "plottables/graph-errorbars.h"
#include "qcustomplot.h"
#include "reference-image.h"
#include <QImage>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <vector>
using qcp::errorbars::Bar;
namespace { constexpr double kNan = std::numeric_limits<double>::quiet_NaN(); }

void TestErrorBarGeometry::nanBoundGivesAOneSidedBar()
{
    Bar b { 10, 50, kNan, 40 };
    QVERIFY(qcp::errorbars::resolve(b));
    QCOMPARE(b.lower, 50.0);
    Bar above { 10, 50, 60, kNan };
    QVERIFY(qcp::errorbars::resolve(above));
    QCOMPARE(above.upper, 50.0);
}

void TestErrorBarGeometry::twoNanPointsGiveNoBar()
{
    Bar b { 10, kNan, kNan, 40 };
    QVERIFY(!qcp::errorbars::resolve(b));
}

void TestErrorBarGeometry::missingValueStillDrawsBetweenBounds()
{
    Bar b { 10, kNan, 60, 40 };
    QVERIFY(qcp::errorbars::resolve(b));
    QCOMPARE(b.lower, 60.0);
    QCOMPARE(b.upper, 40.0);
}

void TestErrorBarGeometry::barsFitNeedsTheSpacing()
{
    const std::vector<Bar> wide { {0, 1, 2, 0}, {20, 1, 2, 0}, {40, 1, 2, 0} };
    const std::vector<Bar> bursty { {0, 1, 2, 0}, {2, 1, 2, 0}, {40, 1, 2, 0} };
    QVERIFY(qcp::errorbars::barsFit(wide, 18));
    QVERIFY(!qcp::errorbars::barsFit(bursty, 18));
    const std::vector<Bar> exactlyApart { {0, 1, 2, 0}, {18, 1, 2, 0} };
    QVERIFY(qcp::errorbars::barsFit(exactlyApart, 18));
}

namespace
{
const QString kDir = QStringLiteral("test-reference-m4e");

//! The error bars of the reference scenes, with what they recorded: the recording is what these
//! tests hold to the reference images.
class RecordingErrorBars : public QCPErrorBars
{
public:
    using QCPErrorBars::QCPErrorBars;

    [[nodiscard]] int recordings() const { return mCore.recordingCount(); }
    [[nodiscard]] int recordedCommands() const { return mCore.recordedCommandCount(); }
};

struct Scene
{
    QImage image;
    int commands = 0;
};

QImage imageOf(QCustomPlot& plot)
{
    return plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
}

//! How many of the pixels in the box around \a at are the bars' red.
int redPixelsNear(const QImage& image, const QPointF& at, int radius)
{
    int count = 0;
    for (int y = int(at.y()) - radius; y <= int(at.y()) + radius; ++y)
    {
        for (int x = int(at.x()) - radius; x <= int(at.x()) + radius; ++x)
        {
            if (x < 0 || y < 0 || x >= image.width() || y >= image.height())
                continue;
            const QColor colour = image.pixelColor(x, y);
            if (colour.red() > 120 && colour.red() > colour.green() + 60
                && colour.red() > colour.blue() + 60)
                ++count;
        }
    }
    return count;
}

//! The scene the reference suite renders: \a verticalKey and \a logValue move the axes, \a selected
//! selects the middle two bars. \a tweak runs on the graph and the bars before the replot.
Scene errorBarsScene(bool verticalKey, bool logValue, bool selected,
                     const std::function<void(QCPGraph&, RecordingErrorBars&)>& tweak = {})
{
    QCustomPlot plot;
    plot.resize(400, 300);
    QFont font = qcp::test::referenceFont();
    font.setPixelSize(10);
    plot.setFont(font);
    for (QCPAxis* axis : plot.axisRect()->axes())
    {
        axis->setTickLabelFont(font);
        axis->setLabelFont(font);
    }
    QCPAxis* key = verticalKey ? plot.yAxis : plot.xAxis;
    QCPAxis* value = verticalKey ? plot.xAxis : plot.yAxis;
    key->setRange(0, 6);
    value->setRange(logValue ? 0.5 : -1, logValue ? 10 : 1.5);
    auto* graph = new QCPGraph(key, value);
    graph->setLineStyle(QCPGraph::lsNone);
    graph->setPen(QPen(Qt::blue, 0));
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 6));
    graph->setData(QVector<double> { 0.5, 1.5, 2.5, 3.5, 4.5, 5.5 },
                   logValue ? QVector<double> { 0.7, 1.2, 2.4, 1.6, 3.4, 5.5 }
                            : QVector<double> { 0.2, -0.4, 0.6, 0.1, -0.3, 0.5 });
    auto* bars = new RecordingErrorBars(key, value);
    bars->setDataPlottable(graph);
    bars->setErrorType(QCPErrorBars::etValueError);
    bars->setData(QVector<double> { 0.2, 0.4, 0.3, 0.5, 0.2, 0.4 });
    bars->setWhiskerWidth(6);
    bars->setPen(QPen(QColor(200, 30, 30)));
    if (selected)
    {
        bars->setSelectable(QCP::stDataRange);
        bars->setSelection(QCPDataSelection(QCPDataRange(2, 4)));
    }
    if (tweak)
        tweak(*graph, *bars);
    if (logValue)
        value->setScaleType(QCPAxis::stLogarithmic);
    plot.replot();
    return { imageOf(plot), bars->recordedCommands() };
}

//! A plot with one flat graph and its error bars, already replotted.
std::unique_ptr<QCustomPlot> makeBarsPlot(RecordingErrorBars** out)
{
    auto plot = std::make_unique<QCustomPlot>();
    plot->resize(400, 300);
    plot->xAxis->setRange(0, 6);
    plot->yAxis->setRange(-1, 1.5);
    auto* graph = new QCPGraph(plot->xAxis, plot->yAxis);
    graph->setLineStyle(QCPGraph::lsNone);
    graph->setData(QVector<double> { 0.5, 1.5, 2.5 }, QVector<double> { 0.2, 0.2, 0.2 });
    auto* bars = new RecordingErrorBars(plot->xAxis, plot->yAxis);
    bars->setDataPlottable(graph);
    bars->setErrorType(QCPErrorBars::etValueError);
    bars->setData(QVector<double> { 0.4, 0.4, 0.4 });
    bars->setWhiskerWidth(6);
    bars->setPen(QPen(QColor(200, 30, 30)));
    plot->replot();
    *out = bars;
    return plot;
}
} // namespace

//! The dark pixels in a box around \a at: how much ink a marker leaves.
int inkNear(const QImage& image, const QPointF& at, int radius)
{
    int ink = 0;
    for (int y = int(at.y()) - radius; y <= int(at.y()) + radius; ++y)
        for (int x = int(at.x()) - radius; x <= int(at.x()) + radius; ++x)
            if (image.valid(x, y) && image.pixelColor(x, y).lightness() < 240)
                ++ink;
    return ink;
}

void TestErrorBarGeometry::recordedErrorBarsMatchTheirReferences_data()
{
    QTest::addColumn<QString>("kind");
    QTest::addColumn<bool>("verticalKey");
    QTest::addColumn<bool>("logValue");
    QTest::addColumn<bool>("selected");
    QTest::newRow("plain") << QStringLiteral("errorbars-plain.png") << false << false << false;
    QTest::newRow("vertical-key")
        << QStringLiteral("errorbars-vertical-key.png") << true << false << false;
    QTest::newRow("log") << QStringLiteral("errorbars-log.png") << false << true << false;
    QTest::newRow("selected")
        << QStringLiteral("errorbars-selected.png") << false << false << true;
}

void TestErrorBarGeometry::recordedErrorBarsMatchTheirReferences()
{
    QFETCH(QString, kind);
    QFETCH(bool, verticalKey);
    QFETCH(bool, logValue);
    QFETCH(bool, selected);

    const Scene scene = errorBarsScene(verticalKey, logValue, selected);
    QVERIFY2(scene.commands > 0, "the error bars recorded nothing");
    QString why;
    QVERIFY2(qcp::test::matchesReference(scene.image, kDir, kind, &why),
             qPrintable(QStringLiteral("%1: %2").arg(kind, why)));
}

void TestErrorBarGeometry::barsFollowTheirPlottablesData()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.xAxis->setRange(0, 6);
    plot.yAxis->setRange(-1, 1.5);
    auto* graph = new QCPGraph(plot.xAxis, plot.yAxis);
    graph->setLineStyle(QCPGraph::lsNone);
    graph->setData(QVector<double> { 0.5, 1.5, 2.5, 3.5, 4.5, 5.5 },
                   QVector<double> { 0.2, 0.2, 0.2, 0.2, 0.2, 0.2 });
    auto* bars = new RecordingErrorBars(plot.xAxis, plot.yAxis);
    bars->setDataPlottable(graph);
    bars->setErrorType(QCPErrorBars::etValueError);
    bars->setData(QVector<double> { 0.4, 0.4, 0.4, 0.4, 0.4, 0.4 });
    bars->setWhiskerWidth(6);
    bars->setPen(QPen(QColor(200, 30, 30)));
    plot.replot();
    const int recorded = bars->recordings();
    // The whisker of the first bar sits at its value plus the error.
    const QPointF first(plot.xAxis->coordToPixel(0.5), plot.yAxis->coordToPixel(0.6));
    QVERIFY2(redPixelsNear(imageOf(plot), first, 4) > 0,
             "the first bar's whisker is not at its key");

    // The graph's keys move by one: the bars adopt them, so the recording has to be remade.
    graph->setData(QVector<double> { 1.5, 2.5, 3.5, 4.5, 5.5, 6.5 },
                   QVector<double> { 0.2, 0.2, 0.2, 0.2, 0.2, 0.2 });
    plot.replot();
    QVERIFY2(bars->recordings() > recorded, "the bars kept a recording made from the old keys");
    const QPointF moved(plot.xAxis->coordToPixel(1.5), plot.yAxis->coordToPixel(0.6));
    QVERIFY2(redPixelsNear(imageOf(plot), moved, 4) > 0, "the bar did not follow its data");
    QCOMPARE(redPixelsNear(imageOf(plot), first, 4), 0);
}

void TestErrorBarGeometry::attachDetachReattach()
{
    RecordingErrorBars* bars = nullptr;
    auto plot = makeBarsPlot(&bars);
    auto* other = new QCPGraph(plot->xAxis, plot->yAxis);
    other->setLineStyle(QCPGraph::lsNone);
    other->setData(QVector<double> { 2.5, 3.5, 4.5 }, QVector<double> { 0.2, 0.2, 0.2 });
    plot->replot();
    const QPointF onFirst(plot->xAxis->coordToPixel(0.5), plot->yAxis->coordToPixel(0.6));
    QVERIFY2(redPixelsNear(imageOf(*plot), onFirst, 4) > 0, "the bar is not on its graph");

    bars->setDataPlottable(nullptr);
    plot->replot();
    QCOMPARE(bars->recordedCommands(), 0);
    QCOMPARE(redPixelsNear(imageOf(*plot), onFirst, 4), 0);

    bars->setDataPlottable(other);
    plot->replot();
    QVERIFY2(bars->recordedCommands() > 0, "the bars recorded nothing for the second graph");
    const QPointF onSecond(plot->xAxis->coordToPixel(2.5), plot->yAxis->coordToPixel(0.6));
    QVERIFY2(redPixelsNear(imageOf(*plot), onSecond, 4) > 0, "the bars did not move to the new graph");
    QCOMPARE(redPixelsNear(imageOf(*plot), onFirst, 4), 0);
}

void TestErrorBarGeometry::everyStyleSetterReRecords()
{
    RecordingErrorBars* bars = nullptr;
    auto plot = makeBarsPlot(&bars);
    const auto reRecords = [&](const char* what, const std::function<void()>& change)
    {
        const int before = bars->recordings();
        change();
        plot->replot();
        QVERIFY2(bars->recordings() == before + 1, what);
    };
    reRecords("setErrorType did not remake the recording",
              [&] { bars->setErrorType(QCPErrorBars::etKeyError); });
    reRecords("setWhiskerWidth did not remake the recording", [&] { bars->setWhiskerWidth(10); });
    reRecords("setSymbolGap did not remake the recording", [&] { bars->setSymbolGap(6); });
    reRecords("setPen did not remake the recording", [&] { bars->setPen(QPen(Qt::red, 3)); });
    bars->setSelectable(QCP::stDataRange);
    plot->replot();
    reRecords("a selection change did not remake the recording",
              [&] { bars->setSelection(QCPDataSelection(QCPDataRange(0, 1))); });
}

void TestErrorBarGeometry::anInPlaceEditThroughDataIsDrawn()
{
    RecordingErrorBars* bars = nullptr;
    auto plot = makeBarsPlot(&bars);
    const int before = bars->recordings();
    // The 2.x idiom: the container is mutable, and the caller edits a value in place.
    (*bars->data())[1].errorPlus = 0.9;
    plot->replot();
    QVERIFY2(bars->recordings() == before + 1,
             "an edit through data() did not remake the recording");
    // The second bar now reaches 0.2 + 0.9, and its whisker is drawn there.
    const QImage image = imageOf(*plot);
    const QPointF end(plot->xAxis->coordToPixel(1.5), plot->yAxis->coordToPixel(1.1));
    QVERIFY2(redPixelsNear(image, end, 3) > 0, "the new error is not drawn");
}

void TestErrorBarGeometry::aReplotWithoutADataAccessDoesNotReRecord()
{
    RecordingErrorBars* bars = nullptr;
    auto plot = makeBarsPlot(&bars);
    const int before = bars->recordings();
    plot->replot();
    QCOMPARE(bars->recordings(), before);
}

// The dependency once summed its inputs, so a key/value swap on the attached plottable (the same two
// axis pointers in the other order) left it unchanged and the bars kept their old orientation.
void TestErrorBarGeometry::swappingThePlottablesAxesReRecords()
{
    RecordingErrorBars* bars = nullptr;
    auto plot = makeBarsPlot(&bars);
    auto* graph = qobject_cast<QCPGraph*>(bars->dataPlottable());
    QVERIFY(graph);
    const int before = bars->recordings();
    graph->setKeyAxis(plot->yAxis);
    graph->setValueAxis(plot->xAxis);
    plot->replot();
    QCOMPARE(bars->recordings(), before + 1);
}

// The vector overloads replace the container's contents without going through data(), and the
// dependency hash only saw the count: the same number of bars with other errors kept the old
// recording, so the plot kept drawing the old whiskers while the click test read the new ones.
void TestErrorBarGeometry::aSameCountSetDataReRecords()
{
    RecordingErrorBars* bars = nullptr;
    auto plot = makeBarsPlot(&bars);
    const int recorded = bars->recordings();
    const QPointF oldEnd(plot->xAxis->coordToPixel(0.5), plot->yAxis->coordToPixel(0.6));
    QVERIFY2(redPixelsNear(imageOf(*plot), oldEnd, 2) > 0, "the first whisker is not at 0.6");

    bars->setData(QVector<double> { 1.0, 1.0, 1.0 });
    plot->replot();
    QVERIFY2(bars->recordings() > recorded, "a same-count setData kept the old recording");
    const QPointF newEnd(plot->xAxis->coordToPixel(0.5), plot->yAxis->coordToPixel(1.2));
    QVERIFY2(redPixelsNear(imageOf(*plot), newEnd, 2) > 0, "the new error is not drawn");

    // Every addData overload appends to the same container: it must remake the recording too, even
    // when the attached plottable has no data point for the new bar.
    int before = bars->recordings();
    bars->addData(QVector<double> { 0.5 });
    plot->replot();
    QVERIFY2(bars->recordings() > before, "a vector addData kept the old recording");
    before = bars->recordings();
    bars->addData(0.5, 0.5);
    plot->replot();
    QVERIFY2(bars->recordings() > before, "an addData kept the old recording");
}

// A decorator is part of what a plottable records: its pen and brush already re-recorded, its
// scatter style and the decorator itself did not.
void TestErrorBarGeometry::aDecoratorChangeReRecords()
{
    RecordingErrorBars* bars = nullptr;
    auto plot = makeBarsPlot(&bars);
    int before = bars->recordings();
    bars->setSelectionDecorator(new QCPSelectionDecoratorBracket);
    plot->replot();
    QVERIFY2(bars->recordings() > before, "a new selection decorator kept the old recording");
    before = bars->recordings();
    bars->selectionDecorator()->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCross, 9));
    plot->replot();
    QVERIFY2(bars->recordings() > before, "a decorator scatter style kept the old recording");
    before = bars->recordings();
    bars->selectionDecorator()->setUsedScatterProperties(QCPScatterStyle::spShape);
    plot->replot();
    QVERIFY2(bars->recordings() > before,
             "a decorator used-properties change kept the old recording");
}

// A recorded decorator callback runs later, from the recording: clearing the decorator while it is
// selected must not leave the callback pointing at the deleted decorator.
void TestErrorBarGeometry::clearingTheDecoratorWhileSelectedIsSafe()
{
    RecordingErrorBars* bars = nullptr;
    auto plot = makeBarsPlot(&bars);
    bars->setSelectable(QCP::stDataRange);
    bars->setSelection(QCPDataSelection(QCPDataRange(0, 2)));
    bars->setSelectionDecorator(new QCPSelectionDecoratorBracket);
    plot->replot();
    const int before = bars->recordings();
    bars->setSelectionDecorator(nullptr);
    // A pan inside the recording's key window would reuse it: the recording must be remade instead.
    plot->xAxis->setRange(0.05, 6.05);
    plot->replot();
    QVERIFY2(bars->recordings() > before, "clearing the decorator kept the old recording");
}

// The statistical box bakes the decorator's scatter style into the selected outliers.
void TestErrorBarGeometry::aDecoratorScatterStyleReachesTheSelectedOutliers()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.xAxis->setRange(0, 6);
    plot.yAxis->setRange(0, 10);
    auto* box = new QCPStatisticalBox(plot.xAxis, plot.yAxis);
    box->addData(2.0, 1.0, 3.0, 4.0, 6.0, 7.0, QVector<double> { 9.0 });
    box->setOutlierStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 5));
    box->setSelectable(QCP::stDataRange);
    box->setSelection(QCPDataSelection(QCPDataRange(0, 1)));
    plot.replot();
    const QPointF outlier(plot.xAxis->coordToPixel(2.0), plot.yAxis->coordToPixel(9.0));
    const int small = inkNear(imageOf(plot), outlier, 12);

    box->selectionDecorator()->setScatterStyle(
        QCPScatterStyle(QCPScatterStyle::ssCross, 20),
        QCPScatterStyle::spShape | QCPScatterStyle::spSize | QCPScatterStyle::spPen);
    plot.replot();
    const int big = inkNear(imageOf(plot), outlier, 12);
    QVERIFY2(small > 0, "the selected outlier is not drawn");
    QVERIFY2(big > small, "the decorator's scatter style is not on the selected outlier");
}

// A graph whose source is replaced by another one of the same size must remake the bars' recording.
void TestErrorBarGeometry::aSwappedPlottableContainerReRecords()
{
    RecordingErrorBars* bars = nullptr;
    auto plot = makeBarsPlot(&bars);
    auto* graph = qobject_cast<QCPGraph*>(bars->dataPlottable());
    QVERIFY(graph);
    graph->setData(QVector<double> { 0.5, 1.5, 2.5 }, QVector<double> { 0.2, 0.2, 0.2 });
    plot->replot();
    const int recorded = bars->recordings();
    const QPointF oldEnd(plot->xAxis->coordToPixel(0.5), plot->yAxis->coordToPixel(0.6));
    QVERIFY2(redPixelsNear(imageOf(*plot), oldEnd, 2) > 0, "the whisker is not at the first value");

    graph->setData(QVector<double> { 0.5, 1.5, 2.5 }, QVector<double> { 0.8, 0.8, 0.8 });
    plot->replot();
    QVERIFY2(bars->recordings() > recorded, "a replaced source kept the recording");
    const QPointF newEnd(plot->xAxis->coordToPixel(0.5), plot->yAxis->coordToPixel(1.2));
    QVERIFY2(redPixelsNear(imageOf(*plot), newEnd, 2) > 0,
             "the bars did not follow the replaced source");
}

// A curve sorts by t, not by key: culling it with a key window drops the bars whose t is outside it.
void TestErrorBarGeometry::aCurveRecordsEveryBar()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.xAxis->setRange(-1, 1);
    plot.yAxis->setRange(-1, 1);
    auto* curve = new QCPCurve(plot.xAxis, plot.yAxis);
    const int n = 41;
    QVector<QCPCurveData> points;
    for (int i = 0; i < n; ++i)
        points.append(QCPCurveData(i, -1.0 + 2.0 * i / (n - 1), 0.2));
    curve->data()->set(points, true);
    QVERIFY(!curve->sortKeyIsMainKey());
    auto* bars = new RecordingErrorBars(plot.xAxis, plot.yAxis);
    bars->setDataPlottable(curve);
    bars->setErrorType(QCPErrorBars::etValueError);
    bars->setData(QVector<double>(n, 0.3));
    bars->setWhiskerWidth(6);
    bars->setPen(QPen(QColor(200, 30, 30)));
    plot.replot();

    // The bar at t = 30 sits at key 0.5, far outside the key window in t units.
    const QPointF end(plot.xAxis->coordToPixel(0.5), plot.yAxis->coordToPixel(0.5));
    QVERIFY2(redPixelsNear(imageOf(plot), end, 3) > 0,
             "a bar whose t is outside the key window was culled");
}

// 2.x drew no backbone when the gap swallowed the whole error: the backbone must not run backwards
// from the end of the gap towards the centre.
void TestErrorBarGeometry::aBackboneShorterThanTheGapIsNotDrawn()
{
    RecordingErrorBars* bars = nullptr;
    auto plot = makeBarsPlot(&bars);
    // An error of one pixel against the default ten-pixel gap.
    const double onePixel = plot->yAxis->range().size() / plot->yAxis->axisRect()->height();
    bars->setData(QVector<double>(3, onePixel));
    plot->replot();

    const QPointF centre(plot->xAxis->coordToPixel(0.5), plot->yAxis->coordToPixel(0.2));
    const QPointF insideTheGap(centre.x(), centre.y() - 4.0);
    QVERIFY2(redPixelsNear(imageOf(*plot), centre, 2) > 0, "the bar is not at its key");
    QCOMPARE(redPixelsNear(imageOf(*plot), insideTheGap, 1), 0);
}

// A grouped bar is drawn away from its key: the error bars must take their centre from the
// plottable's data pixel, not from the raw key and value.
void TestErrorBarGeometry::aGroupedBarsErrorBarSitsOnItsOwnBar()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.xAxis->setRange(0, 2);
    plot.yAxis->setRange(0, 1);
    auto* first = new QCPBars(plot.xAxis, plot.yAxis);
    auto* second = new QCPBars(plot.xAxis, plot.yAxis);
    first->setData(QVector<double> { 1.0 }, QVector<double> { 0.3 });
    second->setData(QVector<double> { 1.0 }, QVector<double> { 0.3 });
    auto* group = new QCPBarsGroup(&plot);
    first->setBarsGroup(group);
    second->setBarsGroup(group);
    for (QCPBars* bar : { first, second })
    {
        auto* bars = new RecordingErrorBars(plot.xAxis, plot.yAxis);
        bars->setDataPlottable(bar);
        bars->setErrorType(QCPErrorBars::etValueError);
        bars->setData(QVector<double> { 0.1 });
        bars->setWhiskerWidth(6);
        bars->setPen(QPen(QColor(200, 30, 30)));
    }
    plot.replot();

    const double endY = plot.yAxis->coordToPixel(0.4);
    QVERIFY2(first->dataPixelPosition(0).x() != second->dataPixelPosition(0).x(),
             "the group did not move the bars apart");
    for (QCPBars* bar : { first, second })
    {
        const QPointF at(bar->dataPixelPosition(0).x(), endY);
        QVERIFY2(redPixelsNear(imageOf(plot), at, 2) > 0,
                 "the error bar is not over its own bar");
    }
}

// A stacked bar is drawn from the top of the stack: its error bars start there too.
void TestErrorBarGeometry::aStackedBarsErrorBarSitsOnTheTopOfTheStack()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.xAxis->setRange(0, 2);
    plot.yAxis->setRange(0, 1);
    auto* below = new QCPBars(plot.xAxis, plot.yAxis);
    below->setData(QVector<double> { 1.0 }, QVector<double> { 0.3 });
    auto* above = new QCPBars(plot.xAxis, plot.yAxis);
    above->setData(QVector<double> { 1.0 }, QVector<double> { 0.3 });
    above->moveAbove(below);
    auto* bars = new RecordingErrorBars(plot.xAxis, plot.yAxis);
    bars->setDataPlottable(above);
    bars->setErrorType(QCPErrorBars::etValueError);
    bars->setData(QVector<double> { 0.1 });
    bars->setWhiskerWidth(6);
    bars->setPen(QPen(QColor(200, 30, 30)));
    plot.replot();

    const QPointF onTheStack(plot.xAxis->coordToPixel(1.0), plot.yAxis->coordToPixel(0.7));
    const QPointF onTheRawValue(plot.xAxis->coordToPixel(1.0), plot.yAxis->coordToPixel(0.4));
    QVERIFY2(redPixelsNear(imageOf(plot), onTheStack, 2) > 0,
             "the error bar is not on the top of the stack");
    QCOMPARE(redPixelsNear(imageOf(plot), onTheRawValue, 2), 0);
}
