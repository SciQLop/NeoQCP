#include "test-item-recording.h"
#include "../misuse-recorder.h"

#include <QElapsedTimer>
#include <functional>
#include <limits>

#include "../../../src/qcp.h"
#include <painting/draw-commands.h>
#include "reference-image.h"
#include <signal-delivery.h>

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

    //! The identity of the picture the image command the item recorded last carries.
    qint64 recordedImageKey() const
    {
        for (const qcp::QCPDrawCommand& command : this->mCore.recordedList().commands)
            if (const auto* image = std::get_if<qcp::ImageCmd>(&command))
                return image->pixels ? image->pixels->image.cacheKey() : 0;
        return 0;
    }

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

//! An item with no position at all that records a box in data coordinates: the callout's shape.
class DataBoxItem : public QCPItem
{
public:
    void draw(QCPDrawContext& context) const override
    {
        context.rect(QCP::Data { 10, 36 }, QCP::Data { 18, 39 }, QPen(Qt::black),
                     QBrush(Qt::yellow));
    }
};

// An item that owns an anchor, so a follower can hang from it: the walk that a changed() slot
// must not run inside.
class SourceItem : public QCPItem
{
public:
    QCPItemAnchor* const at
        = createAnchor(QStringLiteral("at"), []() -> QCP::Position { return QCP::Data { 2, 3 }; });

    void draw(QCPDrawContext& context) const override
    {
        context.rect(QCP::Data { 1, 2 }, QCP::Data { 3, 4 }, QPen(Qt::red), QBrush(Qt::red));
    }
};

// The callout's recording: a text at its data tip and a box hung off it by a pixel offset, so the
// recording's data range is the tip and the box's data corners, not the pixels they land on.
class CalloutShapeItem : public QCPItem
{
public:
    void draw(QCPDrawContext& context) const override
    {
        const QCP::Data tip { 2, 3 };
        context.rect(tip + QCP::PixelOffset { 30, -110 }, QCP::Data { 5, 7 } + QCP::PixelOffset { 200, -50 },
                     QPen(Qt::black), QBrush(Qt::yellow));
        context.text(tip, QStringLiteral("a callout"), {});
    }
};

// A box that starts at key 1, which the log rule would drop: dataRect() must not.
class LogBoxItem : public QCPItem
{
public:
    void draw(QCPDrawContext& context) const override
    {
        context.rect(QCP::Data { 1, 36 }, QCP::Data { 100, 39 }, QPen(Qt::black), QBrush(Qt::yellow));
    }
};

void preparePlot(QCustomPlot& plot)
{
    plot.resize(400, 300);
    QFont font = qcp::test::referenceFont();
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
    auto item = plot.add<Item>();
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

//! Whether the whole image holds a mostly red pixel: a marker that should not be drawn at all.
bool imageHasRed(const QImage& image)
{
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
        {
            const QColor colour = image.pixelColor(x, y);
            if (colour.red() > 150 && colour.red() > colour.blue() + 60)
                return true;
        }
    return false;
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
    auto line = plot.add<RecordingItem<QCPItemLine>>();
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
    auto line = plot.add<RecordingItem<QCPItemLine>>();
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
    auto* graph = plot.add<QCPGraph>().get();
    graph->setData(QVector<double> { 1, 2, 3, 4 }, QVector<double> { 0.2, 0.3, 0.4, 0.5 });
    auto tracer = plot.add<QCPItemTracer>();
    tracer->setGraph(graph);
    tracer->setGraphKey(3.0);
    tracer->setStyle(QCPItemTracer::tsNone);
    auto text = plot.add<QCPItemText>();
    text->setText(QStringLiteral("label"));
    text->position->setCoords(1.0, 0.6);
    auto line = plot.add<RecordingItem<QCPItemLine>>();
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
    auto line = plot.add<RecordingItem<QCPItemLine>>();
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
    auto* graph = plot.add<QCPGraph>().get();
    graph->setData(QVector<double> { 1, 2, 3, 4 }, QVector<double> { 0.2, 0.2, 0.2, 0.2 });
    auto tracer = plot.add<RecordingItem<QCPItemTracer>>();
    tracer->setGraph(graph);
    tracer->setGraphKey(2.0);
    tracer->setStyle(QCPItemTracer::tsNone);
    tracer->setLayer(plot.layer(QStringLiteral("main")));
    auto curve = plot.add<RecordingItem<QCPItemCurve>>();
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
    auto* graph = plot.add<QCPGraph>().get();
    const QVector<double> keys { 1, 2, 3, 4 };
    const QVector<double> low { 0.2, 0.2, 0.2, 0.2 };
    const QVector<double> high { 0.8, 0.8, 0.8, 0.8 };
    graph->setData(keys, low);
    graph->setPen(QPen(Qt::black, 1));
    auto tracer = plot.add<RecordingItem<QCPItemTracer>>();
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

    graph->setData(keys, high);
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
    auto* graph = plot.add<QCPGraph>().get();
    const QVector<double> keys { 1, 2, 3, 4 };
    const QVector<double> low { 0.2, 0.2, 0.2, 0.2 };
    const QVector<double> high { 0.7, 0.7, 0.7, 0.7 };
    graph->setData(keys, low);
    graph->setPen(QPen(Qt::black, 1));
    auto* bars = plot.add<RecordingItem<QCPErrorBars>>().get();
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

    graph->setData(keys, high);
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
    auto rect = plot.add<RecordingItem<QCPItemRect>>();
    rect->topLeft->setType(QCPItemPosition::ptAbsolute);
    rect->topLeft->setCoords(40, 40);
    rect->bottomRight->setType(QCPItemPosition::ptAbsolute);
    rect->bottomRight->setCoords(140, 120);
    rect->setPen(QPen(QColor(200, 0, 0), 2));
    rect->setBrush(QBrush(Qt::NoBrush));
    auto pixmap = plot.add<RecordingItem<QCPItemPixmap>>();
    pixmap->setPixmap(testPixmap());
    pixmap->setScaled(true);
    pixmap->topLeft->setType(QCPItemPosition::ptViewportRatio);
    pixmap->topLeft->setCoords(0.5, 0.5);
    pixmap->bottomRight->setType(QCPItemPosition::ptViewportRatio);
    pixmap->bottomRight->setCoords(0.9, 0.9);
    // The items keep the rect they were placed in, which loses every axis: no transform is left.
    const QList<QCPHandle<QCPAxis>> axes = plot.axisRect()->axes();
    for (const QCPHandle<QCPAxis>& axis : axes)
        if (axis)
            plot.axisRect()->removeAxis(axis.get());
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
    auto rect = plot.add<RecordingItem<QCPItemRect>>();
    // The items keep the rect they were placed in, which loses every axis: no transform is left.
    const QList<QCPHandle<QCPAxis>> axes = plot.axisRect()->axes();
    for (const QCPHandle<QCPAxis>& axis : axes)
        if (axis)
            plot.axisRect()->removeAxis(axis.get());
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
        auto ellipse = plot.add<QCPItemEllipse>();
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
    auto line = plot.add<RecordingItem<QCPItemLine>>();
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
    auto pixmap = plot.add<RecordingItem<QCPItemPixmap>>();
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

void TestItemRecording::aStaticPixmapIsConvertedOnce()
{
    // The comment beside the scale cache promises a conversion only when the picture changes: the
    // hook counts them, because Qt shares the raster data and the recorded image cannot show it.
    QCustomPlot plot;
    preparePlot(plot);
    auto pixmap = plot.add<RecordingItem<QCPItemPixmap>>();
    pixmap->setPixmap(testPixmap());
    pixmap->setScaled(true);
    pixmap->topLeft->setType(QCPItemPosition::ptAxisRectRatio);
    pixmap->topLeft->setCoords(0.2, 0.2);
    pixmap->bottomRight->setType(QCPItemPosition::ptAxisRectRatio);
    pixmap->bottomRight->setCoords(0.6, 0.6);
    plot.replot();
    const quint64 conversions = QCPItemPixmap::imageConversionCount();
    const int recordings = pixmap->recordings();
    QVERIFY2(conversions > 0, "the first recording converted nothing");
    QVERIFY2(recordings > 0, "the pixmap recorded nothing");

    // A border change remakes the recording; the picture itself is the same at the same size.
    pixmap->setPen(QPen(Qt::black, 1));
    plot.replot();
    QVERIFY2(pixmap->recordings() > recordings, "the border change did not remake the recording");
    QVERIFY2(QCPItemPixmap::imageConversionCount() == conversions,
             "a recording converted the pixmap to an image again although nothing changed");

    // A new picture converts again, even at the same size: the cache follows the content.
    const qint64 picture = pixmap->recordedImageKey();
    QImage other(48, 36, QImage::Format_ARGB32);
    other.fill(QColor(20, 160, 90));
    pixmap->setPixmap(QPixmap::fromImage(other));
    plot.replot();
    QVERIFY2(QCPItemPixmap::imageConversionCount() > conversions,
             "a new picture did not convert");
    QVERIFY2(pixmap->recordedImageKey() != picture, "the recording kept the old picture");
}

void TestItemRecording::aTracerRecords()
{
    int commands = 0;
    const QImage image = renderItem<RecordingItem<QCPItemTracer>>(
        &commands,
        [](QCustomPlot& plot, RecordingItem<QCPItemTracer>* tracer)
        {
            auto* graph = plot.add<QCPGraph>().get();
            graph->setPen(QPen(Qt::blue, 0));
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
    auto* graph = plot.add<QCPGraph>().get();
    graph->setData(QVector<double> { 1, 2, 3, 4 }, QVector<double> { 0.2, 0.2, 0.2, 0.2 });
    auto tracer = plot.add<RecordingItem<QCPItemTracer>>();
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
    auto* graph = plot.add<QCPGraph>().get();
    graph->setData(QVector<double> { 1, 2, 3, 4, 5 }, QVector<double> { 0.2, 0.3, 0.4, 0.5, 0.6 });
    auto tracer = plot.add<RecordingItem<QCPItemTracer>>();
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
    auto* graph = plot.add<QCPGraph>().get();
    graph->setData(QVector<double> { 1, 2, 3, 4, 5 }, QVector<double> { 0.2, 0.3, 0.4, 0.5, 0.6 });
    auto tracer = plot.add<RecordingItem<QCPItemTracer>>();
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
    auto* graph = plot.add<QCPGraph>().get();
    graph->setData(QVector<double> { 1, 5 }, QVector<double> { 0.2, 0.6 });
    auto tracer = plot.add<RecordingItem<QCPItemTracer>>();
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

void TestItemRecording::theTracerFollowsAMultiGraphComponent()
{
    QCustomPlot plot;
    preparePlot(plot);
    auto* graph = plot.add<QCPGraph>().get();
    graph->setData(std::vector<double>{1, 2, 3, 4},
                   std::vector<std::vector<double>>{{0.1, 0.1, 0.1, 0.1},
                                                    {0.2, 0.3, 0.4, 0.5},
                                                    {0.8, 0.8, 0.8, 0.8}});
    auto tracer = plot.add<RecordingItem<QCPItemTracer>>();
    tracer->setGraph(graph, 1);
    QCOMPARE(tracer->component(), 1);
    tracer->setGraphKey(2.5);
    tracer->setInterpolating(true);
    tracer->setStyle(QCPItemTracer::tsCircle);
    tracer->setSize(9);
    tracer->setPen(QPen(Qt::red, 2));
    plot.replot();

    QCOMPARE(tracer->position->coords(), QPointF(2.5, 0.35));
    const QPointF at(plot.xAxis->coordToPixel(2.5), plot.yAxis->coordToPixel(0.35));
    QImage image = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
    QVERIFY2(redPixelsNear(image, at, 8) > 0, "the marker is not on the component's line");

    // A stepped component still follows the 2.x tracer: interpolation is linear between samples.
    graph->setComponentLineStyle(1, QCPGraph::lsStepLeft);
    plot.replot();
    QCOMPARE(tracer->position->coords(), QPointF(2.5, 0.35));

    tracer->setInterpolating(false);
    tracer->setGraphKey(2.6);
    plot.replot();
    QCOMPARE(tracer->position->coords(), QPointF(3.0, 0.4));
}

void TestItemRecording::theTracerOutsideTheDataClampsAs2xDid()
{
    QCustomPlot plot;
    preparePlot(plot);
    auto* graph = plot.add<QCPGraph>().get();
    graph->setData(std::vector<double>{1, 2, 3, 4},
                   std::vector<std::vector<double>>{{0.2, 0.3, 0.4, 0.5}});
    auto tracer = plot.add<QCPItemTracer>();
    tracer->setGraph(graph);
    tracer->setInterpolating(true);

    tracer->setGraphKey(-10.0);
    tracer->updatePosition();
    QCOMPARE(tracer->position->coords(), QPointF(1.0, 0.2));
    tracer->setGraphKey(10.0);
    tracer->updatePosition();
    QCOMPARE(tracer->position->coords(), QPointF(4.0, 0.5));
}

void TestItemRecording::theTracerOnAGapIsInert()
{
    QCustomPlot plot;
    preparePlot(plot);
    auto* graph = plot.add<QCPGraph>().get();
    graph->setData(std::vector<double>{1, 2, 3, 4},
                   std::vector<std::vector<double>>{{0.2, std::numeric_limits<double>::quiet_NaN(),
                                                    0.4, 0.5}});
    auto tracer = plot.add<QCPItemTracer>();
    tracer->setGraph(graph);
    tracer->setInterpolating(true);
    tracer->setGraphKey(2.0);
    tracer->setStyle(QCPItemTracer::tsCircle);
    tracer->setSize(9);
    tracer->setPen(QPen(Qt::red, 2));
    plot.replot();

    QVERIFY2(!std::isfinite(tracer->position->coords().y()), "the tracer left the NaN gap");
    const QImage image = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
    QVERIFY2(!imageHasRed(image), "a tracer on a gap drew a marker");
}

void TestItemRecording::aTracerWhoseComponentDisappearsIsInert()
{
    // M3: a component the graph no longer has leaves the tracer where it was; it must not draw that
    // stale sample as if the target were still there.
    QCustomPlot plot;
    preparePlot(plot);
    auto* graph = plot.add<QCPGraph>().get();
    graph->setData(std::vector<double>{1, 2, 3},
                   std::vector<std::vector<double>>{{0.2, 0.3, 0.4}, {0.6, 0.7, 0.8}});
    auto tracer = plot.add<RecordingItem<QCPItemTracer>>();
    tracer->setGraph(graph, 1);
    tracer->setStyle(QCPItemTracer::tsCircle);
    tracer->setSize(9);
    tracer->setPen(QPen(Qt::red, 2));
    plot.replot();
    QVERIFY2(tracer->recordedCommands() > 0, "the tracer drew nothing before the component went");

    graph->setData(std::vector<double>{1, 2, 3},
                   std::vector<std::vector<double>>{{0.2, 0.3, 0.4}});
    plot.replot();
    QCOMPARE(tracer->component(), 1);
    QCOMPARE(tracer->recordedCommands(), 0);
    const QImage image = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
    QVERIFY2(!imageHasRed(image), "the tracer kept marking a component that disappeared");
}

void TestItemRecording::setComponentWithoutATargetDoesNothing()
{
    // M3: without a graph there is no component to follow; the setter is a documented no-op.
    QCustomPlot plot;
    preparePlot(plot);
    auto tracer = plot.add<QCPItemTracer>();
    const auto previous = qcp::setMisuseHandler(test_misuse::record);
    test_misuse::calls.clear();
    tracer->setComponent(5);
    qcp::setMisuseHandler(previous);
    QCOMPARE(tracer->component(), 0);
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
}

void TestItemRecording::deletingTheTracedMultiGraphLeavesTheTracerInert()
{
    QCustomPlot plot;
    preparePlot(plot);
    auto* graph = plot.add<QCPGraph>().get();
    graph->setData(std::vector<double>{1, 2, 3}, std::vector<std::vector<double>>{{0.2, 0.3, 0.4}});
    auto tracer = plot.add<QCPItemTracer>();
    tracer->setGraph(graph);
    tracer->setStyle(QCPItemTracer::tsCircle);
    tracer->setSize(9);
    tracer->setPen(QPen(Qt::red, 2));
    plot.replot();

    delete graph;
    QVERIFY(!tracer->graph());
    plot.replot();
    const QImage image = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
    QVERIFY2(!imageHasRed(image), "the tracer outlived the graph it followed");
}

void TestItemRecording::takingTheTracedMultiGraphLeavesTheTracerInert()
{
    QCustomPlot plot;
    preparePlot(plot);
    auto graph = plot.add<QCPGraph>(
        std::vector<double>{1, 2, 3}, std::vector<std::vector<double>>{{0.2, 0.3, 0.4}});
    QVERIFY(graph);
    auto tracer = plot.add<QCPItemTracer>();
    tracer->setGraph(graph.get());
    tracer->setStyle(QCPItemTracer::tsCircle);
    tracer->setSize(9);
    tracer->setPen(QPen(Qt::red, 2));
    plot.replot();

    auto taken = plot.take(graph);
    QVERIFY(taken);
    QVERIFY(!tracer->graph());
    QVERIFY(!tracer->graph());
    // The tracer is now a free position at its last sample: a replot must not crash or follow the
    // taken graph that left the plot.
    plot.replot();
}

void TestItemRecording::retargetingToAnotherGraphLeavesTheFirst()
{
    QCustomPlot plot;
    preparePlot(plot);
    auto* first = plot.add<QCPGraph>().get();
    first->setData(QVector<double>{1, 2, 3}, QVector<double>{0.2, 0.3, 0.4});
    auto* second = plot.add<QCPGraph>().get();
    second->setData(std::vector<double>{1, 2, 3},
                    std::vector<std::vector<double>>{{0.6, 0.7, 0.8}, {0.1, 0.1, 0.1}});

    auto tracer = plot.add<QCPItemTracer>();
    tracer->setGraphKey(2);
    tracer->setGraph(first);
    QCOMPARE(tracer->position->value(), 0.3);
    tracer->setGraph(second, 1);
    QCOMPARE(tracer->graph().get(), second);
    QCOMPARE(tracer->component(), 1);
    QCOMPARE(tracer->position->value(), 0.1);

    // The first graph no longer carries the tracer: its data change leaves the tracer where it is.
    first->setData(QVector<double>{1, 2, 3}, QVector<double>{5, 5, 5});
    tracer->updatePosition();
    QCOMPARE(tracer->position->value(), 0.1);
}

void TestItemRecording::theTracerOpsHandleSetsALineGraph()
{
    QCustomPlot plot;
    preparePlot(plot);
    auto graph = plot.add<QCPGraph>(
        std::vector<double>{1, 2, 3},
        std::vector<std::vector<double>>{{0.1, 0.1, 0.1}, {0.6, 0.7, 0.8}});
    QVERIFY(graph);
    auto tracer = plot.add<QCPItemTracer>().setGraph(graph, 1).setGraphKey(2.0);
    QVERIFY(tracer);
    QCOMPARE(tracer->graph(), graph.get());
    QCOMPARE(tracer->component(), 1);
    tracer->updatePosition();
    QCOMPARE(tracer->position->coords(), QPointF(2.0, 0.7));
}

void TestItemRecording::theTracerRefusesAnOutOfRangeComponent()
{
    QCustomPlot plot;
    preparePlot(plot);
    auto* graph = plot.add<QCPGraph>().get();
    graph->setData(std::vector<double>{1, 2, 3},
                   std::vector<std::vector<double>>{{0.1, 0.1, 0.1}, {0.2, 0.2, 0.2}});
    auto tracer = plot.add<QCPItemTracer>();
    tracer->setGraph(graph, 1);

    const auto previous = qcp::setMisuseHandler(test_misuse::record);
    test_misuse::calls.clear();
    tracer->setComponent(2);
    tracer->setComponent(-1);
    tracer->setGraph(graph, 5);
    qcp::setMisuseHandler(previous);
    QCOMPARE(test_misuse::calls.size(), std::size_t(3));
    QCOMPARE(tracer->component(), 1);
    QCOMPARE(tracer->graph(), graph);
}

void TestItemRecording::selectionReRecords()
{
    // A selected item draws its selected pen and brush: the selection is a content change.
    QCustomPlot plot;
    preparePlot(plot);
    auto rect = plot.add<RecordingItem<QCPItemRect>>();
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

// Double-buffered streaming: replacing the data with other values of the same shape remakes the tracer.
void TestItemRecording::aSwappedGraphContainerReRecords()
{
    QCustomPlot plot;
    preparePlot(plot);
    auto* graph = plot.add<QCPGraph>().get();
    const QVector<double> keys { 1, 2, 3, 4 };
    const QVector<double> first { 0.2, 0.2, 0.2, 0.2 };
    const QVector<double> second { 0.8, 0.8, 0.8, 0.8 };
    graph->setData(keys, first);
    auto tracer = plot.add<RecordingItem<QCPItemTracer>>();
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

    graph->setData(keys, second);
    plot.replot();
    QVERIFY2(tracer->recordings() > before,
             "a swapped container with the same revision kept the recording");
    image = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
    QVERIFY2(redPixelsNear(image, high, 8) > 0, "the tracer did not follow the swapped container");
    QCOMPARE(redPixelsNear(image, low, 8), 0);
}

void TestItemRecording::updateEmitsChanged()
{
    // Once per update(), and held back while the plot is emitting: a scope delivers it as it closes.
    QCustomPlot plot;
    preparePlot(plot);
    auto item = plot.add<DataBoxItem>();
    QSignalSpy spy(item.get(), &QCPItem::changed);
    item->update();
    item->update();
    QCOMPARE(spy.count(), 2);
    {
        const QCPEmissionScope scope(&plot);
        item->update();
        QCOMPARE(spy.count(), 2);
    }
    QCOMPARE(spy.count(), 3);
}

void TestItemRecording::aSlotDeletingTheItemOnChanged()
{
    // Review Focus 1: the slot deletes the item, and nothing reads it afterwards.
    QCustomPlot plot;
    preparePlot(plot);
    const QCPHandle<DataBoxItem> item = plot.add<DataBoxItem>();
    connect(item.get(), &QCPItem::changed, &plot, [&] { item.remove(); });
    item->update();
    QVERIFY(!item);
    QCOMPARE(plot.itemCount(), 0);

    // The same for a slot that deletes the plot.
    auto* owned = new QCustomPlot;
    preparePlot(*owned);
    const QCPHandle<DataBoxItem> doomed = owned->add<DataBoxItem>();
    connect(doomed.get(), &QCPItem::changed, owned, [&] { delete owned; owned = nullptr; });
    doomed->update();
    QVERIFY(!doomed);
    QVERIFY(owned == nullptr);
}

void TestItemRecording::dataRectIsTheRecordedBox()
{
    QCustomPlot plot;
    preparePlot(plot);
    auto item = plot.add<DataBoxItem>();
    plot.replot();
    QCOMPARE(item->dataRect(), QRectF(10, 36, 8, 3));
}

void TestItemRecording::dataRectBeforeTheFirstReplot()
{
    // Review Focus 5: the box the item is about to draw, before it has drawn it.
    QCustomPlot plot;
    preparePlot(plot);
    auto item = plot.add<DataBoxItem>();
    QCOMPARE(item->dataRect(), QRectF(10, 36, 8, 3));
}

void TestItemRecording::dataRectOfAPositionlessItem()
{
    // The callout: no position of its own, everything in the function that its anchors run.
    QCustomPlot plot;
    preparePlot(plot);
    auto item = plot.add<DataBoxItem>();
    plot.replot();
    QCOMPARE(item->dataRect(), QRectF(10, 36, 8, 3));

    // An item drawn in pixels alone holds no data position, so there is no data box.
    auto pixel = plot.add<QCPItemRect>();
    for (QCPItemPosition* position : { pixel->topLeft, pixel->bottomRight })
    {
        position->setType(QCPItemPosition::ptAbsolute);
    }
    pixel->topLeft->setCoords(20, 20);
    pixel->bottomRight->setCoords(80, 60);
    plot.replot();
    QVERIFY(pixel->dataRect().isEmpty());
}

void TestItemRecording::dataRectOfACalloutRecording()
{
    // The box hangs off the tip by pixels: its data corners are what the rect reports, and 2..5 by
    // 3..7 is the box the item will draw when the frame is the plot's.
    QCustomPlot plot;
    preparePlot(plot);
    auto item = plot.add<CalloutShapeItem>();
    QCOMPARE(item->dataRect(), QRectF(2, 3, 3, 4));
    plot.replot();
    QCOMPARE(item->dataRect(), QRectF(2, 3, 3, 4));
}

void TestItemRecording::dataRectIgnoresTheLogAndReverseRules()
{
    // A reversed log key axis would make the log rule drop the side below 1, and reverse the range:
    // the data box is neither, so dataRect() stays left = key lower.
    QCustomPlot plot;
    preparePlot(plot);
    plot.xAxis->setScale(QCPScale::log());
    plot.xAxis->setRangeReversed(true);
    auto item = plot.add<LogBoxItem>();
    plot.replot();
    QCOMPARE(item->dataRect(), QRectF(1, 36, 99, 3));
}

void TestItemRecording::aFollowerSlotDeletingTheSourceOnChanged()
{
    // A slot on the follower's changed() deletes the item the walk is walking: no slot may run
    // while markFollowersDirty() reads that item's anchors.
    QCustomPlot plot;
    preparePlot(plot);
    const QCPHandle<SourceItem> source = plot.add<SourceItem>();
    const QCPHandle<QCPItemText> follower = plot.add<QCPItemText>();
    QVERIFY(follower->position->setParentAnchor(source->at));
    QObject::connect(follower.get(), &QCPItem::changed, &plot, [&] { source.remove(); });

    source->update();
    QVERIFY(!source);
    QVERIFY(static_cast<bool>(follower));
    QCOMPARE(plot.itemCount(), 1);
    plot.replot();
}

void TestItemRecording::aFollowerSlotDeletingThePlotOnChanged()
{
    auto plot = std::make_unique<QCustomPlot>();
    preparePlot(*plot);
    const QCPHandle<SourceItem> source = plot->add<SourceItem>();
    const QCPHandle<QCPItemText> follower = plot->add<QCPItemText>();
    QVERIFY(follower->position->setParentAnchor(source->at));
    QCustomPlot* raw = plot.get();
    QObject::connect(follower.get(), &QCPItem::changed, plot.get(), [raw] { delete raw; });

    source->update();
    plot.release();
    QVERIFY(!source);
    QVERIFY(!follower);
}

void TestItemRecording::aFollowerSlotDeletingAnotherFollowerOnChanged()
{
    // The walk has two followers: the first one's slot deletes the second, which the walk has not
    // reached yet.
    QCustomPlot plot;
    preparePlot(plot);
    const QCPHandle<SourceItem> source = plot.add<SourceItem>();
    const QCPHandle<QCPItemText> first = plot.add<QCPItemText>();
    const QCPHandle<QCPItemText> second = plot.add<QCPItemText>();
    QVERIFY(first->position->setParentAnchor(source->at));
    QVERIFY(second->position->setParentAnchor(source->at));
    QObject::connect(first.get(), &QCPItem::changed, &plot, [&] { second.remove(); });

    source->update();
    QVERIFY(static_cast<bool>(source));
    QVERIFY(static_cast<bool>(first));
    QVERIFY(!second);
    QCOMPARE(plot.itemCount(), 2);
    plot.replot();
}

void TestItemRecording::deletionPathsHoldTheFollowerEmissions_data()
{
    QTest::addColumn<QString>("path");
    QTest::addColumn<QString>("action");
    for (const QString& path :
         { QStringLiteral("removeItem"), QStringLiteral("clearItems"), QStringLiteral("remove()"),
           QStringLiteral("delete") })
        for (const QString& action : { QStringLiteral("remove again"),
                                       QStringLiteral("delete follower"),
                                       QStringLiteral("delete plot") })
            QTest::newRow(qPrintable(path + QStringLiteral(", ") + action)) << path << action;
}

void TestItemRecording::deletionPathsHoldTheFollowerEmissions()
{
    QFETCH(QString, path);
    QFETCH(QString, action);
    auto plot = std::make_unique<QCustomPlot>();
    QCustomPlot* raw = plot.get();
    preparePlot(*raw);
    const QCPHandle<SourceItem> source = raw->add<SourceItem>();
    const QCPHandle<QCPItemText> first = raw->add<QCPItemText>();
    const QCPHandle<QCPItemText> second = raw->add<QCPItemText>();
    QVERIFY(first->position->setParentAnchor(source->at));
    QVERIFY(second->position->setParentAnchor(source->at));
    int heard = 0;
    bool sourceSeenAlive = false;
    // A plain delete runs the slot inside the destructor, where the item is no longer a SourceItem:
    // a QCPHandle<SourceItem> would downcast it. The base handle is what such a slot can use.
    const QCPHandle<QCPItem> sourceAsItem = source;
    QObject::connect(first.get(), &QCPItem::changed, raw, [&] {
        ++heard;
        sourceSeenAlive = sourceSeenAlive || static_cast<bool>(sourceAsItem);
        if (action == QLatin1String("remove again"))
            sourceAsItem.remove();
        else if (action == QLatin1String("delete follower"))
            second.remove();
        else
        {
            delete raw;
            plot.release();
        }
    });

    if (path == QLatin1String("removeItem"))
        raw->removeItem(source.get());
    else if (path == QLatin1String("clearItems"))
        raw->clearItems();
    else if (path == QLatin1String("remove()"))
        source.remove();
    else
        delete source.get();

    // Every path delivers the follower's report: a held emission that dies with its sender proves
    // nothing about the slot.
    QVERIFY(heard > 0);
    // Through the library, the reports wait until the item is gone, so a slot finds its handle null
    // and never reaches a half-destroyed object. A plain delete has no outer call to wait for.
    if (path != QLatin1String("delete"))
        QVERIFY2(!sourceSeenAlive, "a follower's slot ran while the removed item was being destroyed");
    if (action == QLatin1String("delete plot"))
    {
        QVERIFY(!plot);
        return;
    }
    QVERIFY(!source);
    if (path == QLatin1String("clearItems"))
    {
        QVERIFY(!first);
        QVERIFY(!second);
        return;
    }
    QVERIFY2(static_cast<bool>(first), "the follower whose slot ran was removed");
    if (action == QLatin1String("delete follower"))
        QVERIFY2(!second, "the follower the slot deleted is still listed");
    else
        QVERIFY2(static_cast<bool>(second), "an untouched follower was removed");
    raw->replot();
}

void TestItemRecording::anAnchorOfAnotherPlotIsRefused()
{
    // An item measures in its own plot's frame: an anchor of another plot would resolve to pixels
    // of a frame this item never draws in, so the write is refused and reported.
    QCustomPlot plot;
    preparePlot(plot);
    QCustomPlot other;
    preparePlot(other);
    const QCPHandle<SourceItem> foreign = other.add<SourceItem>();
    const QCPHandle<QCPItemText> follower = plot.add<QCPItemText>();

    const auto previous = qcp::setMisuseHandler(test_misuse::record);
    test_misuse::calls.clear();
    const bool attached = follower->position->setParentAnchor(foreign->at);
    qcp::setMisuseHandler(previous);

    QVERIFY2(!attached, "an anchor of another plot was attached");
    QVERIFY(!follower->position->parentAnchor());
    // setParentAnchor asks the key and the value axis, and each refusal is reported on its own.
    QCOMPARE(test_misuse::calls.size(), std::size_t(2));
    QCOMPARE(plot.itemCount(), 1);
    QCOMPARE(other.itemCount(), 1);
}

void TestItemRecording::anUnplacedItemCannotFollowAnother()
{
    // No plot, no queue to hold a report in: the follower's slot would run inside the source's
    // walk and free the follower under it. The link is refused instead.
    auto source = std::make_unique<SourceItem>();
    auto follower = std::make_unique<QCPItemText>();

    const auto previous = qcp::setMisuseHandler(test_misuse::record);
    test_misuse::calls.clear();
    const bool linked = follower->position->setParentAnchor(source->at);
    qcp::setMisuseHandler(previous);

    QVERIFY2(!linked, "an unplaced item followed another one");
    QVERIFY(!follower->position->parentAnchor());
    QCOMPARE(test_misuse::calls.size(), std::size_t(2));
    QObject::connect(follower.get(), &QCPItem::changed, [&] { follower.reset(); });
    source->update();
    QVERIFY(follower != nullptr);
}

void TestItemRecording::aLinkMadeBeforeThePlotsIsRefused()
{
    // Linked while unplaced, then placed on two plots: the link would cross them.
    QCustomPlot plot;
    preparePlot(plot);
    QCustomPlot other;
    preparePlot(other);
    auto source = std::make_unique<SourceItem>();
    auto follower = std::make_unique<QCPItemText>();

    const auto previous = qcp::setMisuseHandler(test_misuse::record);
    static_cast<void>(follower->position->setParentAnchor(source->at));
    qcp::setMisuseHandler(previous);
    plot.add(std::move(source));
    const auto placed = other.add(std::move(follower));

    QVERIFY2(!placed->position->parentAnchorX() && !placed->position->parentAnchorY(),
             "a link made before the plots crosses them");
}

void TestItemRecording::anUnplacedItemMayFollowItself()
{
    // Within one item a link reaches no other item's walk: it stays allowed before the plot.
    auto line = std::make_unique<QCPItemLine>();
    QVERIFY(line->end->setParentAnchor(line->start));
    QCustomPlot plot;
    preparePlot(plot);
    const auto placed = plot.add(std::move(line));
    QCOMPARE(placed->end->parentAnchor(), static_cast<QCPItemAnchor*>(placed->start));
}
