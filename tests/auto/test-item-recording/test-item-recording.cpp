#include "test-item-recording.h"

#include <functional>

#include "../../../src/qcp.h"
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
