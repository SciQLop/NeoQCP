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
