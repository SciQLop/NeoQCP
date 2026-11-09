#include "test-item-recording.h"

#include <QElapsedTimer>
#include <functional>

#include "../../../src/qcp.h"
#include <painting/draw-commands.h>
#include "reference-image.h"

namespace
{

const QString kDir = QStringLiteral("test-reference-m4e");

//! An item whose recording the test can read.
template <typename Item>
class RecordingItem : public Item
{
public:
    using Item::Item;

    int recordings() const { return this->mCore.recordingCount(); }

    int recordedCommands() const { return this->mCore.recordedCommandCount(); }

    //! The top-left pixel of the image command the item recorded last.
    QCP::Pixels recordedImageCorner() const
    {
        for (const qcp::QCPDrawCommand& command : this->mCore.recordedList().commands)
            if (const auto* image = std::get_if<qcp::ImageCmd>(&command))
                if (const auto* pixels = std::get_if<QCP::Pixels>(&image->a))
                    return *pixels;
        return { };
    }
};

void preparePlot(QCustomPlot& plot)
{
    plot.resize(400, 300);
    QFont font(QStringLiteral("Helvetica"));
    font.setPixelSize(10);
    plot.setFont(font);
    for (QCPAxis* axis : plot.axisRect()->axes())
    {
        axis->setTickLabelFont(font);
        axis->setLabelFont(font);
    }
    plot.xAxis->setRange(0, 6);
    plot.yAxis->setRange(0, 1);
}

//! Renders a scene holding one \a Item, and reports the commands that item recorded.
template <typename Item>
QImage renderItem(int* commands, const std::function<void(QCustomPlot&, Item*)>& setup)
{
    QCustomPlot plot;
    preparePlot(plot);
    auto* item = new Item(&plot);
    setup(plot, item);
    plot.replot();
    *commands = item->recordedCommands();
    return plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
}

//! The four coloured quadrants the reference scenes draw: its size is what a pixmap item shows.
QPixmap testPixmap()
{
    QImage image(48, 36, QImage::Format_ARGB32);
    for (int y = 0; y < 36; ++y)
        for (int x = 0; x < 48; ++x)
            image.setPixel(x, y,
                           x < 24 ? (y < 18 ? qRgb(40, 120, 200) : qRgb(230, 200, 40))
                                  : (y < 18 ? qRgb(220, 60, 60) : qRgb(60, 170, 90)));
    return QPixmap::fromImage(image);
}

//! The pixels of \a image that are mostly red: what a tracer marker leaves.
int redPixelsNear(const QImage& image, const QPointF& at, int radius)
{
    int red = 0;
    for (int y = qRound(at.y()) - radius; y <= qRound(at.y()) + radius; ++y)
        for (int x = qRound(at.x()) - radius; x <= qRound(at.x()) + radius; ++x)
        {
            if (!image.rect().contains(x, y))
                continue;
            const QColor colour = image.pixelColor(x, y);
            if (colour.red() > 150 && colour.red() > colour.blue() + 60)
                ++red;
        }
    return red;
}

//! The pixels of \a image that are mostly blue: what a blue arrow head leaves.
int bluePixelsNear(const QImage& image, const QPointF& at, int radius)
{
    int blue = 0;
    for (int y = qRound(at.y()) - radius; y <= qRound(at.y()) + radius; ++y)
        for (int x = qRound(at.x()) - radius; x <= qRound(at.x()) + radius; ++x)
        {
            if (!image.rect().contains(x, y))
                continue;
            const QColor colour = image.pixelColor(x, y);
            if (colour.blue() > 100 && colour.blue() > colour.red() + 40)
                ++blue;
        }
    return blue;
}

void check(const QImage& image, const char* name)
{
    QString why;
    QVERIFY2(qcp::test::matchesReference(image, kDir, QLatin1String(name), &why),
             qPrintable(QStringLiteral("%1: %2").arg(QLatin1String(name), why)));
}

} // namespace

void TestItemRecording::aLineWithEndingsRecords()
{
    int commands = 0;
    const QImage image
        = renderItem<RecordingItem<QCPItemLine>>(&commands,
                                                 [](QCustomPlot&, RecordingItem<QCPItemLine>* line)
                                                 {
                                                     line->start->setCoords(0.5, 0.2);
                                                     line->end->setCoords(5.5, 0.8);
                                                     line->setHead(QCPLineEnding::esSpikeArrow);
                                                     line->setTail(QCPLineEnding::esDisc);
                                                     line->setPen(QPen(QColor(30, 80, 160), 2));
                                                 });
    QVERIFY2(commands > 0, "the line recorded nothing");
    check(image, "item-line-arrows.png");
}

void TestItemRecording::aStraightLineRecords()
{
    int commands = 0;
    const QImage image = renderItem<RecordingItem<QCPItemStraightLine>>(
        &commands,
        [](QCustomPlot&, RecordingItem<QCPItemStraightLine>* line)
        {
            line->point1->setCoords(1.0, 0.1);
            line->point2->setCoords(4.0, 0.9);
            line->setPen(QPen(QColor(160, 40, 120), 2));
        });
    QVERIFY2(commands > 0, "the straight line recorded nothing");
    check(image, "item-straightline.png");
}

void TestItemRecording::aCurveRecords()
{
    int commands = 0;
    const QImage image = renderItem<RecordingItem<QCPItemCurve>>(
        &commands,
        [](QCustomPlot&, RecordingItem<QCPItemCurve>* curve)
        {
            curve->start->setCoords(0.5, 0.2);
            curve->startDir->setCoords(2.0, 0.9);
            curve->endDir->setCoords(4.0, 0.1);
            curve->end->setCoords(5.5, 0.8);
            curve->setPen(QPen(QColor(20, 140, 90), 2));
        });
    QVERIFY2(commands > 0, "the curve recorded nothing");
    check(image, "item-curve.png");
}

void TestItemRecording::aBracketRecords()
{
    int commands = 0;
    const QImage image = renderItem<RecordingItem<QCPItemBracket>>(
        &commands,
        [](QCustomPlot&, RecordingItem<QCPItemBracket>* bracket)
        {
            bracket->left->setCoords(1.0, 0.75);
            bracket->right->setCoords(5.0, 0.25);
            bracket->setLength(30);
            bracket->setPen(QPen(Qt::black, 2));
        });
    QVERIFY2(commands > 0, "the bracket recorded nothing");
    check(image, "item-bracket.png");
}

void TestItemRecording::aPannedPlainLineIsNotReRecorded()
{
    // A line without endings is data on both sides: a pan moves it, it does not remake it.
    QCustomPlot plot;
    preparePlot(plot);
    auto* line = new RecordingItem<QCPItemLine>(&plot);
    line->start->setCoords(1, 0.2);
    line->end->setCoords(4, 0.8);
    plot.replot();
    const int before = line->recordings();
    QCOMPARE(before, 1);

    plot.xAxis->setRange(1, 7);
    plot.replot();
    QCOMPARE(line->recordings(), before);
}

void TestItemRecording::aPannedArrowIsRightAfterThePan()
{
    // An ending is pixels: the recording reads the transform and is remade, so the head sits on the
    // end's new pixel and points the new way.
    QCustomPlot plot;
    preparePlot(plot);
    auto* line = new RecordingItem<QCPItemLine>(&plot);
    line->start->setCoords(1, 0.2);
    line->end->setCoords(4, 0.8);
    line->setHead(QCPLineEnding::esFlatArrow);
    line->setPen(QPen(QColor(30, 80, 160), 2));
    plot.replot();
    const int before = line->recordings();

    plot.xAxis->setRange(1, 7);
    plot.replot();
    QVERIFY2(line->recordings() > before, "the arrowed line kept a stale recording");

    const QImage image = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
    const QPointF end = line->end->pixelPosition();
    const QColor atEnd = image.pixelColor(qRound(end.x()) - 1, qRound(end.y()));
    QVERIFY2(atEnd.blue() > 100 && atEnd.blue() > atEnd.red() + 40,
             qPrintable(QStringLiteral("the arrow head is not on the end: %1").arg(atEnd.name())));
}

void TestItemRecording::anAnchoredArrowFollowsThePan()
{
    // Both ends are anchored, so the recording bakes pixel endings. The shaft is re-resolved on
    // replay; the head must be remade too, or a pan leaves it behind.
    QCustomPlot plot;
    preparePlot(plot);
    auto* graph = new QCPGraph(plot.xAxis, plot.yAxis);
    graph->setData(QVector<double> { 1, 2, 3, 4 }, QVector<double> { 0.2, 0.3, 0.4, 0.5 });
    auto* tracer = new QCPItemTracer(&plot);
    tracer->setGraph(graph);
    tracer->setGraphKey(3.0);
    tracer->setStyle(QCPItemTracer::tsNone);
    auto* text = new QCPItemText(&plot);
    text->setText(QStringLiteral("label"));
    text->position->setCoords(1.0, 0.6);
    auto* line = new RecordingItem<QCPItemLine>(&plot);
    line->start->setParentAnchor(text->bottom);
    line->end->setParentAnchor(tracer->position);
    line->setHead(QCPLineEnding(QCPLineEnding::esDisc, 14));
    line->setPen(QPen(QColor(30, 80, 160), 2));
    plot.replot();
    const int before = line->recordings();
    const QPointF oldEnd = line->end->pixelPosition();

    plot.xAxis->setRange(1, 7);
    plot.replot();
    QVERIFY2(line->recordings() > before, "the arrowed line kept a stale recording");
    const QImage image = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
    const QPointF end = line->end->pixelPosition();
    QVERIFY2(end != oldEnd, "the pan did not move the anchored end");
    QVERIFY2(bluePixelsNear(image, end, 7) > 40,
             qPrintable(QStringLiteral("the arrow head is not on the anchored end: %1,%2")
                            .arg(end.x())
                            .arg(end.y())));
    QVERIFY2(bluePixelsNear(image, oldEnd, 7) < 20,
             qPrintable(QStringLiteral("the arrow head stayed at the old end: %1,%2")
                            .arg(oldEnd.x())
                            .arg(oldEnd.y())));
}

void TestItemRecording::aRatioArrowFollowsTheAxisRect()
{
    // Both ends are axis-rect ratios: the shaft re-resolves against the new rect, the head must
    // too. A longer tick label grows the left margin without changing the axis ranges.
    QCustomPlot plot;
    preparePlot(plot);
    auto* line = new RecordingItem<QCPItemLine>(&plot);
    line->start->setType(QCPItemPosition::ptAxisRectRatio);
    line->end->setType(QCPItemPosition::ptAxisRectRatio);
    line->start->setCoords(0.1, 0.85);
    line->end->setCoords(0.6, 0.15);
    line->setHead(QCPLineEnding(QCPLineEnding::esDisc, 14));
    line->setPen(QPen(QColor(30, 80, 160), 2));
    plot.replot();
    const int before = line->recordings();
    const QPointF oldEnd = line->end->pixelPosition();

    auto ticker = QSharedPointer<QCPAxisTickerText>::create();
    ticker->addTick(0, QStringLiteral("123456789"));
    ticker->addTick(1, QStringLiteral("987654321"));
    plot.yAxis->setTicker(ticker);
    plot.replot();
    const QPointF after = line->end->pixelPosition();
    QVERIFY2(after != oldEnd, "the longer tick labels did not move the axis rect");
    QVERIFY2(line->recordings() > before, "the ratio arrow kept a stale recording");
    const QImage image = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
    QVERIFY2(bluePixelsNear(image, after, 7) > 40,
             qPrintable(QStringLiteral("the arrow head stayed at the old axis rect: %1,%2 -> %3,%4")
                            .arg(oldEnd.x())
                            .arg(oldEnd.y())
                            .arg(after.x())
                            .arg(after.y())));
    QVERIFY2(bluePixelsNear(image, oldEnd, 7) < 20,
             qPrintable(QStringLiteral("the arrow head is still at the old axis rect: %1,%2")
                            .arg(oldEnd.x())
                            .arg(oldEnd.y())));
}

void TestItemRecording::aFollowerFollowsItsAnchorItem()
{
    // The curve hangs from a tracer by its end. New graph data moves the tracer, and nothing calls
    // the curve: its recording must depend on the item it hangs from, or it draws the old pixels.
    QCustomPlot plot;
    preparePlot(plot);
    auto* graph = new QCPGraph(plot.xAxis, plot.yAxis);
    graph->setData(QVector<double> { 1, 2, 3, 4 }, QVector<double> { 0.2, 0.2, 0.2, 0.2 });
    auto* tracer = new RecordingItem<QCPItemTracer>(&plot);
    tracer->setGraph(graph);
    tracer->setGraphKey(2.0);
    tracer->setStyle(QCPItemTracer::tsNone);
    tracer->setLayer(plot.layer(QStringLiteral("main")));
    auto* curve = new RecordingItem<QCPItemCurve>(&plot);
    curve->start->setCoords(1.0, 0.9);
    curve->startDir->setCoords(2.0, 0.9);
    curve->endDir->setCoords(2.0, 0.1);
    curve->end->setParentAnchor(tracer->position);
    // The graph's layer is repainted, so this test is about the dependency and not the scheduling.
    curve->setLayer(plot.layer(QStringLiteral("main")));
    plot.replot();

    const int before = curve->recordings();
    const QPointF oldEnd = curve->end->pixelPosition();
    graph->setData(QVector<double> { 1, 2, 3, 4 }, QVector<double> { 0.8, 0.8, 0.8, 0.8 });
    plot.replot();

    QVERIFY2(curve->recordings() > before, "the follower kept a stale recording");
    const QPointF end = curve->end->pixelPosition();
    QVERIFY2(end != oldEnd, "the anchored end did not follow the tracer");
    QVERIFY2(qAbs(end.y() - plot.yAxis->coordToPixel(0.8)) < 1.0,
             "the anchored end is not on the tracer's new sample");
}

void TestItemRecording::aTracerOnAnotherLayerFollowsItsGraph()
{
    QCustomPlot plot;
    preparePlot(plot);
    auto* graph = new QCPGraph(plot.xAxis, plot.yAxis);
    auto low = QSharedPointer<QCPGraphDataContainer>::create();
    auto high = QSharedPointer<QCPGraphDataContainer>::create();
    low->set(QVector<QCPGraphData> { { 1, 0.2 }, { 2, 0.2 }, { 3, 0.2 }, { 4, 0.2 } });
    high->set(QVector<QCPGraphData> { { 1, 0.8 }, { 2, 0.8 }, { 3, 0.8 }, { 4, 0.8 } });
    graph->setData(low);
    graph->setPen(QPen(Qt::black, 1));
    auto* tracer = new RecordingItem<QCPItemTracer>(&plot);
    tracer->setGraph(graph);
    tracer->setGraphKey(3.0);
    tracer->setStyle(QCPItemTracer::tsCircle);
    tracer->setSize(9);
    tracer->setPen(QPen(Qt::red, 2));
    QCOMPARE(tracer->layer()->name(), QStringLiteral("annotations"));
    QCOMPARE(graph->layer()->name(), QStringLiteral("main"));
    plot.replot();
    const QPointF atLow(plot.xAxis->coordToPixel(3.0), plot.yAxis->coordToPixel(0.2));
    QImage image = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
    QVERIFY2(redPixelsNear(image, atLow, 8) > 0, "the tracer did not draw its marker");
    // Read after the pixmap: toPixmap() changes the viewport, which re-records view dependent
    // drawings by itself.
    const int before = tracer->recordings();

    graph->setData(high);
    plot.replot();
    QVERIFY2(tracer->recordings() > before,
             "a tracer on another layer than its graph kept the recording");
    image = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
    const QPointF atHigh(plot.xAxis->coordToPixel(3.0), plot.yAxis->coordToPixel(0.8));
    QVERIFY2(redPixelsNear(image, atHigh, 8) > 0, "the tracer did not follow the graph's new data");
    QCOMPARE(redPixelsNear(image, atLow, 8), 0);
}

void TestItemRecording::anErrorBarOnAnotherLayerFollowsItsGraph()
{
    QCustomPlot plot;
    preparePlot(plot);
    auto* graph = new QCPGraph(plot.xAxis, plot.yAxis);
    auto low = QSharedPointer<QCPGraphDataContainer>::create();
    auto high = QSharedPointer<QCPGraphDataContainer>::create();
    low->set(QVector<QCPGraphData> { { 1, 0.2 }, { 2, 0.2 }, { 3, 0.2 }, { 4, 0.2 } });
    high->set(QVector<QCPGraphData> { { 1, 0.7 }, { 2, 0.7 }, { 3, 0.7 }, { 4, 0.7 } });
    graph->setData(low);
    graph->setPen(QPen(Qt::black, 1));
    auto* bars = new RecordingItem<QCPErrorBars>(plot.xAxis, plot.yAxis);
    bars->setDataPlottable(graph);
    for (int i = 0; i < 4; ++i)
        bars->addData(0.15, 0.15);
    bars->setPen(QPen(QColor(30, 80, 160), 2));
    QVERIFY(bars->setLayer(QStringLiteral("annotations")));
    QCOMPARE(graph->layer()->name(), QStringLiteral("main"));
    plot.replot();
    const QPointF atLowWhisker(plot.xAxis->coordToPixel(2.0), plot.yAxis->coordToPixel(0.05));
    QImage image = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
    QVERIFY2(bluePixelsNear(image, atLowWhisker, 4) > 0,
             "the error bar did not draw its whisker");
    // Read after the pixmap: toPixmap() changes the viewport, which re-records view dependent
    // drawings by itself.
    const int before = bars->recordings();

    graph->setData(high);
    plot.replot();
    QVERIFY2(bars->recordings() > before,
             "error bars on another layer than their graph kept the recording");
    image = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
    const QPointF atHighWhisker(plot.xAxis->coordToPixel(2.0), plot.yAxis->coordToPixel(0.55));
    QVERIFY2(bluePixelsNear(image, atHighWhisker, 4) > 0,
             "the error bar did not follow the graph's new data");
    QCOMPARE(bluePixelsNear(image, atLowWhisker, 4), 0);
}

void TestItemRecording::aPixelOnlyItemDrawsWithoutAxes()
{
    // A plot with no axes still holds items placed in pixels: 2.x drew them from pixelPosition().
    // A recording whose every position is axis free must go through, even without a transform.
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.plotLayout()->clear();
    auto* rect = new RecordingItem<QCPItemRect>(&plot);
    rect->topLeft->setType(QCPItemPosition::ptAbsolute);
    rect->topLeft->setCoords(40, 40);
    rect->bottomRight->setType(QCPItemPosition::ptAbsolute);
    rect->bottomRight->setCoords(140, 120);
    rect->setPen(QPen(QColor(200, 0, 0), 2));
    rect->setBrush(QBrush(Qt::NoBrush));
    auto* pixmap = new RecordingItem<QCPItemPixmap>(&plot);
    pixmap->setPixmap(testPixmap());
    pixmap->setScaled(true);
    pixmap->topLeft->setType(QCPItemPosition::ptViewportRatio);
    pixmap->topLeft->setCoords(0.5, 0.5);
    pixmap->bottomRight->setType(QCPItemPosition::ptViewportRatio);
    pixmap->bottomRight->setCoords(0.9, 0.9);
    plot.replot();
    QVERIFY2(rect->recordings() > 0, "the pixel rectangle has no recording without axes");
    QVERIFY2(pixmap->recordings() > 0, "the viewport ratio pixmap has no recording without axes");
    const QImage image = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
    QVERIFY2(redPixelsNear(image, QPointF(40, 80), 4) > 0,
             "the pixel rectangle did not draw without axes");
    // The pixmap spans 0.5..0.9 of the 400x300 viewport, and its top left quadrant is blue.
    QVERIFY2(bluePixelsNear(image, QPointF(240, 180), 8) > 20,
             "the viewport ratio pixmap did not draw without axes");
}

void TestItemRecording::aDataItemWithoutAxesDrawsNothing()
{
    // A position in plot coordinates has no pixel without its axes: the recording stays refused, so
    // no NaN box ever reaches the replay.
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.plotLayout()->clear();
    auto* rect = new RecordingItem<QCPItemRect>(&plot);
    rect->topLeft->setType(QCPItemPosition::ptPlotCoords);
    rect->topLeft->setCoords(1, 1);
    rect->bottomRight->setType(QCPItemPosition::ptPlotCoords);
    rect->bottomRight->setCoords(2, 2);
    rect->setPen(QPen(QColor(200, 0, 0), 2));
    rect->setBrush(QBrush(Qt::NoBrush));
    plot.replot();
    QCOMPARE(rect->recordings(), 0);
    const QImage image = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
    QCOMPARE(redPixelsNear(image, QPointF(200, 150), 200), 0);
}

void TestItemRecording::aHugeDataEllipseExports()
{
    // 2.x wrapped the data-space drawEllipse in a try/catch ("Item too large for memory"): a box
    // spanning a billion pixels made Qt allocate before it clipped. This port replays the box, so a
    // guard would belong to the replay; the export is measured here so a regression shows up.
    for (const double extent : { 1e9, 1e12 })
    {
        QCustomPlot plot;
        plot.resize(400, 300);
        plot.xAxis->setRange(0, 1);
        plot.yAxis->setRange(0, 1);
        auto* ellipse = new QCPItemEllipse(&plot);
        ellipse->topLeft->setCoords(0, 0);
        ellipse->bottomRight->setCoords(extent, extent);
        ellipse->setPen(QPen(Qt::darkBlue, 2));
        ellipse->setBrush(QBrush(QColor(200, 90, 30, 60)));
        plot.replot();

        QElapsedTimer timer;
        timer.start();
        const QImage image
            = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
        const qint64 elapsed = timer.elapsed();
        QVERIFY2(elapsed < 2000, qPrintable(QStringLiteral("a %1-wide ellipse took %2 ms")
                                                .arg(extent)
                                                .arg(elapsed)));
        QCOMPARE(image.size(), QSize(400, 300));
    }
}

void TestItemRecording::everyStyleSetterReRecords()
{
    QCustomPlot plot;
    preparePlot(plot);
    auto* line = new RecordingItem<QCPItemLine>(&plot);
    line->start->setCoords(1, 0.2);
    line->end->setCoords(4, 0.8);
    plot.replot();

    const auto reRecords = [&](const char* what, const std::function<void()>& change)
    {
        const int before = line->recordings();
        change();
        plot.replot();
        QVERIFY2(
            line->recordings() == before + 1,
            qPrintable(QStringLiteral("%1 did not remake the recording").arg(QLatin1String(what))));
    };

    reRecords("setPen", [&] { line->setPen(QPen(Qt::red, 3)); });
    reRecords("setSelectedPen", [&] { line->setSelectedPen(QPen(Qt::green, 3)); });
    reRecords("setHead", [&] { line->setHead(QCPLineEnding::esDisc); });
    reRecords("setTail", [&] { line->setTail(QCPLineEnding::esBar); });
    reRecords("setSelected", [&] { line->setSelected(true); });
}

void TestItemRecording::aRectRecords()
{
    int commands = 0;
    const QImage image = renderItem<RecordingItem<QCPItemRect>>(
        &commands,
        [](QCustomPlot&, RecordingItem<QCPItemRect>* rect)
        {
            rect->topLeft->setCoords(1.0, 0.8);
            rect->bottomRight->setCoords(4.5, 0.2);
            rect->setPen(QPen(QColor(200, 120, 20), 2));
            rect->setBrush(QColor(200, 120, 20, 60));
        });
    QVERIFY2(commands > 0, "the rect recorded nothing");
    check(image, "item-rect.png");
}

void TestItemRecording::anEllipseRecords()
{
    int commands = 0;
    const QImage image = renderItem<RecordingItem<QCPItemEllipse>>(
        &commands,
        [](QCustomPlot&, RecordingItem<QCPItemEllipse>* ellipse)
        {
            ellipse->topLeft->setCoords(1.5, 0.85);
            ellipse->bottomRight->setCoords(4.5, 0.15);
            ellipse->setPen(QPen(QColor(120, 60, 180), 2));
            ellipse->setBrush(QColor(120, 60, 180, 60));
        });
    QVERIFY2(commands > 0, "the ellipse recorded nothing");
    check(image, "item-ellipse.png");
}

void TestItemRecording::aPixmapRecords()
{
    int commands = 0;
    const QImage image = renderItem<RecordingItem<QCPItemPixmap>>(
        &commands,
        [](QCustomPlot&, RecordingItem<QCPItemPixmap>* pixmap)
        {
            pixmap->setPixmap(testPixmap());
            pixmap->topLeft->setCoords(1.0, 0.8);
            pixmap->bottomRight->setCoords(4.5, 0.2);
            pixmap->setScaled(false, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        });
    QVERIFY2(commands > 0, "the pixmap recorded nothing");
    check(image, "item-pixmap.png");
}

void TestItemRecording::aScaledPixmapRecords()
{
    int commands = 0;
    const QImage image = renderItem<RecordingItem<QCPItemPixmap>>(
        &commands,
        [](QCustomPlot&, RecordingItem<QCPItemPixmap>* pixmap)
        {
            pixmap->setPixmap(testPixmap());
            pixmap->topLeft->setCoords(1.0, 0.8);
            pixmap->bottomRight->setCoords(4.5, 0.2);
            pixmap->setScaled(true, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        });
    QVERIFY2(commands > 0, "the scaled pixmap recorded nothing");
    check(image, "item-pixmap-scaled.png");
}

void TestItemRecording::aRatioPixmapFollowsTheAxisRect()
{
    // A pixmap placed by axis-rect ratio records pixels, and those pixels are only right for the
    // rect they were taken in: a layout change that moves the rect must remake the recording.
    QCustomPlot plot;
    preparePlot(plot);
    auto* pixmap = new RecordingItem<QCPItemPixmap>(&plot);
    pixmap->setPixmap(testPixmap());
    pixmap->setScaled(true);
    pixmap->topLeft->setType(QCPItemPosition::ptAxisRectRatio);
    pixmap->topLeft->setCoords(0.2, 0.2);
    pixmap->bottomRight->setType(QCPItemPosition::ptAxisRectRatio);
    pixmap->bottomRight->setCoords(0.6, 0.6);
    plot.replot();
    const int recordings = pixmap->recordings();
    const QRect before = plot.axisRect()->rect();

    // A value-axis label widens the left margin, so the rect the ratios are taken in shrinks.
    plot.yAxis->setLabel(QStringLiteral("value (arbitrary units)"));
    plot.replot();
    const QRect after = plot.axisRect()->rect();
    QVERIFY2(after != before, "the axis label did not move the axis rect");
    QVERIFY2(pixmap->recordings() > recordings,
             "the ratio pixmap kept a stale recording through a layout change");

    const QCP::Pixels corner = pixmap->recordedImageCorner();
    QVERIFY2(std::abs(corner.x - (after.left() + 0.2 * after.width())) <= 1.0
                 && std::abs(corner.y - (after.top() + 0.2 * after.height())) <= 1.0,
             qPrintable(QStringLiteral("the recording kept the pixel %1,%2 instead of the ratio's "
                                       "%3,%4")
                            .arg(corner.x)
                            .arg(corner.y)
                            .arg(after.left() + 0.2 * after.width())
                            .arg(after.top() + 0.2 * after.height())));
}

void TestItemRecording::aTracerRecords()
{
    int commands = 0;
    const QImage image = renderItem<RecordingItem<QCPItemTracer>>(
        &commands,
        [](QCustomPlot& plot, RecordingItem<QCPItemTracer>* tracer)
        {
            auto* graph = new QCPGraph(plot.xAxis, plot.yAxis);
            QVector<double> keys, values;
            for (int i = 0; i <= 60; ++i)
            {
                keys << i * 0.1;
                values << 0.5 + 0.35 * qSin(i * 0.1);
            }
            graph->setData(keys, values);
            tracer->setGraph(graph);
            tracer->setGraphKey(3.2);
            tracer->setStyle(QCPItemTracer::tsCircle);
            tracer->setSize(9);
            tracer->setPen(QPen(Qt::red, 2));
        });
    QVERIFY2(commands > 0, "the tracer recorded nothing");
    check(image, "item-tracer.png");
}

void TestItemRecording::aTracerFollowsItsGraph()
{
    // The marker is where the graph's data says: new data moves it, without a call on the tracer.
    QCustomPlot plot;
    preparePlot(plot);
    auto* graph = new QCPGraph(plot.xAxis, plot.yAxis);
    graph->setData(QVector<double> { 1, 2, 3, 4 }, QVector<double> { 0.2, 0.2, 0.2, 0.2 });
    auto* tracer = new RecordingItem<QCPItemTracer>(&plot);
    tracer->setGraph(graph);
    tracer->setGraphKey(3.0);
    tracer->setStyle(QCPItemTracer::tsCircle);
    tracer->setSize(9);
    tracer->setPen(QPen(Qt::red, 2));
    plot.replot();

    const QPointF low(plot.xAxis->coordToPixel(3.0), plot.yAxis->coordToPixel(0.2));
    const QPointF high(plot.xAxis->coordToPixel(3.0), plot.yAxis->coordToPixel(0.8));
    QImage image = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
    QVERIFY2(redPixelsNear(image, low, 8) > 0, "the tracer did not draw its marker");

    graph->setData(QVector<double> { 1, 2, 3, 4 }, QVector<double> { 0.8, 0.8, 0.8, 0.8 });
    plot.replot();

    image = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
    QVERIFY2(redPixelsNear(image, high, 8) > 0, "the marker did not follow the graph's new data");
    QCOMPARE(redPixelsNear(image, low, 8), 0);
}

void TestItemRecording::aPannedTracerIsNotReRecorded()
{
    // A tracer on plot coordinates is data: a pan only shifts its marker.
    QCustomPlot plot;
    preparePlot(plot);
    auto* graph = new QCPGraph(plot.xAxis, plot.yAxis);
    graph->setData(QVector<double> { 1, 2, 3, 4, 5 }, QVector<double> { 0.2, 0.3, 0.4, 0.5, 0.6 });
    auto* tracer = new RecordingItem<QCPItemTracer>(&plot);
    tracer->setGraph(graph);
    tracer->setGraphKey(3.0);
    tracer->setStyle(QCPItemTracer::tsCircle);
    tracer->setSize(9);
    plot.replot();
    const int before = tracer->recordings();
    QCOMPARE(before, 1);

    plot.xAxis->setRange(1, 7);
    plot.replot();
    QCOMPARE(tracer->recordings(), before);
}

void TestItemRecording::aTracerFollowsSetGraphKey()
{
    QCustomPlot plot;
    preparePlot(plot);
    auto* graph = new QCPGraph(plot.xAxis, plot.yAxis);
    graph->setData(QVector<double> { 1, 2, 3, 4, 5 }, QVector<double> { 0.2, 0.3, 0.4, 0.5, 0.6 });
    auto* tracer = new RecordingItem<QCPItemTracer>(&plot);
    tracer->setGraph(graph);
    tracer->setGraphKey(2.0);
    tracer->setStyle(QCPItemTracer::tsCircle);
    tracer->setSize(9);
    tracer->setPen(QPen(Qt::red, 2));
    plot.replot();

    const auto markerAt = [&plot](double key, double value)
    {
        return QPointF(plot.xAxis->coordToPixel(key), plot.yAxis->coordToPixel(value));
    };
    QImage image = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
    QVERIFY2(redPixelsNear(image, markerAt(2.0, 0.3), 8) > 0, "the marker is not at its key");

    tracer->setGraphKey(4.0);
    plot.replot();
    image = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
    QVERIFY2(redPixelsNear(image, markerAt(4.0, 0.5), 8) > 0, "the marker did not follow the new key");
    QCOMPARE(redPixelsNear(image, markerAt(2.0, 0.3), 8), 0);
}

void TestItemRecording::aTracerFollowsInterpolation()
{
    // Between two samples the tracer snaps to the nearer one, or interpolates when asked to.
    QCustomPlot plot;
    preparePlot(plot);
    auto* graph = new QCPGraph(plot.xAxis, plot.yAxis);
    graph->setData(QVector<double> { 1, 5 }, QVector<double> { 0.2, 0.6 });
    auto* tracer = new RecordingItem<QCPItemTracer>(&plot);
    tracer->setGraph(graph);
    tracer->setGraphKey(2.0);
    tracer->setStyle(QCPItemTracer::tsCircle);
    tracer->setSize(9);
    tracer->setPen(QPen(Qt::red, 2));
    plot.replot();

    const auto markerAt = [&plot](double key, double value)
    {
        return QPointF(plot.xAxis->coordToPixel(key), plot.yAxis->coordToPixel(value));
    };
    QImage image = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
    // Not interpolating, the tracer sits on the nearer sample's key, not on the queried one.
    QVERIFY2(redPixelsNear(image, markerAt(1.0, 0.2), 8) > 0, "the marker did not snap to the nearer sample");

    tracer->setInterpolating(true);
    plot.replot();
    image = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
    QVERIFY2(redPixelsNear(image, markerAt(2.0, 0.3), 8) > 0, "the marker did not interpolate");
    QCOMPARE(redPixelsNear(image, markerAt(1.0, 0.2), 8), 0);
}

void TestItemRecording::selectionReRecords()
{
    // A selected item draws its selected pen and brush: the selection is a content change.
    QCustomPlot plot;
    preparePlot(plot);
    auto* rect = new RecordingItem<QCPItemRect>(&plot);
    rect->topLeft->setCoords(1.0, 0.8);
    rect->bottomRight->setCoords(4.5, 0.2);
    rect->setPen(QPen(QColor(200, 120, 20), 2));
    rect->setBrush(QColor(200, 120, 20, 60));
    rect->setSelectedPen(QPen(Qt::blue, 4));
    plot.replot();
    const int before = rect->recordings();

    rect->setSelected(true);
    plot.replot();
    QCOMPARE(rect->recordings(), before + 1);
}

// Double-buffered streaming: two containers refilled in lockstep carry the same revision, so a swap
// is only visible through the container's identity.
void TestItemRecording::aSwappedGraphContainerReRecords()
{
    QCustomPlot plot;
    preparePlot(plot);
    auto* graph = new QCPGraph(plot.xAxis, plot.yAxis);
    auto first = QSharedPointer<QCPGraphDataContainer>::create();
    auto second = QSharedPointer<QCPGraphDataContainer>::create();
    first->set(QVector<QCPGraphData> { { 1, 0.2 }, { 2, 0.2 }, { 3, 0.2 }, { 4, 0.2 } });
    second->set(QVector<QCPGraphData> { { 1, 0.8 }, { 2, 0.8 }, { 3, 0.8 }, { 4, 0.8 } });
    QCOMPARE(first->revision(), second->revision());
    graph->setData(first);
    auto* tracer = new RecordingItem<QCPItemTracer>(&plot);
    tracer->setGraph(graph);
    tracer->setGraphKey(3.0);
    tracer->setStyle(QCPItemTracer::tsCircle);
    tracer->setSize(9);
    tracer->setPen(QPen(Qt::red, 2));
    // Items live on the annotations layer, which a plottable's data change does not repaint: put the
    // tracer where the graph is, so this test is about the dependency and not the layer scheduling.
    tracer->setLayer(plot.layer(QStringLiteral("main")));
    plot.replot();
    const int before = tracer->recordings();
    const QPointF low(plot.xAxis->coordToPixel(3.0), plot.yAxis->coordToPixel(0.2));
    const QPointF high(plot.xAxis->coordToPixel(3.0), plot.yAxis->coordToPixel(0.8));
    QImage image = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
    QVERIFY2(redPixelsNear(image, low, 8) > 0, "the tracer did not draw its marker");

    graph->setData(second);
    plot.replot();
    QVERIFY2(tracer->recordings() > before,
             "a swapped container with the same revision kept the recording");
    image = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
    QVERIFY2(redPixelsNear(image, high, 8) > 0, "the tracer did not follow the swapped container");
    QCOMPARE(redPixelsNear(image, low, 8), 0);
}
