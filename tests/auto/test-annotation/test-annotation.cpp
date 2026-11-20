#include "test-annotation.h"

#include <core.h>
#include <items/connector.h>
#include <items/item-annotation.h>
#include <layoutelements/layoutelement-axisrect.h>
#include <painting/draw-commands.h>

#include <QElapsedTimer>
#include <QLineF>
#include <QMouseEvent>
#include <QPainterPath>
#include <QPointF>
#include <QPolygonF>

#include <cmath>
#include <limits>
#include <vector>

using namespace QCP::Literals;

//! Reads an annotation's recording through QCPItemAnnotation's friendship: nothing else may.
struct AnnotationRecording
{
    static const std::vector<qcp::QCPDrawCommand>& commands(const QCPItemAnnotation& item)
    {
        return item.mCore.recordedList().commands;
    }

    static int recordings(const QCPItemAnnotation& item) { return item.mCore.recordingCount(); }
};

namespace
{
constexpr int kWidth = 400;
constexpr int kHeight = 300;

void preparePlot(QCustomPlot& plot)
{
    plot.resize(kWidth, kHeight);
    plot.xAxis->setRange(0, 1);
    plot.yAxis->setRange(0, 1);
}

//! What an annotation records: its connector, its head, and its text, with where each one sits.
struct Recorded
{
    const qcp::PolylinePositionsCmd* line = nullptr;
    const qcp::PathCmd* head = nullptr;
    const qcp::TextCmd* text = nullptr;
    int lineIndex = -1;
    int textIndex = -1;
};

Recorded scan(const QCPItemAnnotation& item)
{
    Recorded recorded;
    const auto& commands = AnnotationRecording::commands(item);
    for (int i = 0; i < int(commands.size()); ++i)
    {
        const qcp::QCPDrawCommand& command = commands[std::size_t(i)];
        if (const auto* line = std::get_if<qcp::PolylinePositionsCmd>(&command))
        {
            recorded.line = line;
            recorded.lineIndex = i;
        }
        else if (const auto* head = std::get_if<qcp::PathCmd>(&command))
            recorded.head = head;
        else if (const auto* text = std::get_if<qcp::TextCmd>(&command))
        {
            recorded.text = text;
            recorded.textIndex = i;
        }
    }
    return recorded;
}

//! A recorded position that is pixels, which is what a connector and its head are recorded in.
QPointF pixelOf(const QCP::Position& position)
{
    const QCP::Pixels* pixels = std::get_if<QCP::Pixels>(&position);
    return pixels ? QPointF(pixels->x, pixels->y) : QPointF(qQNaN(), qQNaN());
}

QPointF firstOf(const qcp::PolylinePositionsCmd& line)
{
    return pixelOf(line.points.front());
}

QPointF lastOf(const qcp::PolylinePositionsCmd& line)
{
    return pixelOf(line.points.back());
}

QPointF secondToLastOf(const qcp::PolylinePositionsCmd& line)
{
    return pixelOf(line.points[line.points.size() - 2]);
}

QPointF firstOf(const qcp::TrimmedConnector& trimmed)
{
    return trimmed.line.first();
}

QPointF lastOf(const qcp::TrimmedConnector& trimmed)
{
    return trimmed.line.last();
}

//! An angle in QPainterPath's degrees, from a screen vector: counter-clockwise with y up the
//! screen.
double angleOf(QPointF screenDirection)
{
    return std::atan2(-screenDirection.y(), screenDirection.x()) * 180.0 / M_PI;
}

void click(QCustomPlot& plot, QPointF at)
{
    const QPointF global = plot.mapToGlobal(at);
    QMouseEvent press(QEvent::MouseButtonPress, at, global, Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(&plot, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, at, global, Qt::LeftButton, Qt::NoButton,
                        Qt::NoModifier);
    QCoreApplication::sendEvent(&plot, &release);
}
} // namespace

void TestAnnotation::trimCutsAtTheBoxAndShrinks()
{
    QPainterPath path;
    path.moveTo(0, 0);
    path.lineTo(100, 0);

    const qcp::TrimmedConnector trimmed = qcp::trimConnector(path, QRectF(-10, -10, 20, 20), 5);

    QVERIFY2(trimmed.line.size() > 1, "a straight path cut at both ends keeps its run");
    QVERIFY2(qAbs(firstOf(trimmed).x() - 15) < 0.01 && qAbs(firstOf(trimmed).y()) < 0.01,
             "the line starts 5 px past where the path leaves the box");
    QVERIFY2(qAbs(lastOf(trimmed).x() - 95) < 0.01 && qAbs(lastOf(trimmed).y()) < 0.01,
             "the line ends 5 px short of the path's end");
    QVERIFY2(qAbs(trimmed.endAngleDegrees) < 0.01, "a run along +x ends at 0 degrees");
}

void TestAnnotation::theCutIsAtTheFirstExit()
{
    // Out of the box, back in, and out the other side: the connector leaves the text at the first
    // exit, as matplotlib's split_path_inout does.
    QPainterPath path;
    path.moveTo(0, 0);
    path.lineTo(30, 0);
    path.lineTo(0, 0);
    path.lineTo(-40, 0);

    const qcp::TrimmedConnector trimmed = qcp::trimConnector(path, QRectF(-10, -10, 20, 20), 0);

    QVERIFY2(trimmed.line.size() > 1, "the path leaves the box, so a connector is left");
    QVERIFY2(qAbs(firstOf(trimmed).x() - 10) < 0.05,
             "the line starts at the first exit, not at the second one");
    QVERIFY2(qAbs(lastOf(trimmed).x() + 40) < 0.05, "the line runs to the path's end");
}

void TestAnnotation::aTargetInsideTheBoxDrawsNoConnector()
{
    QCustomPlot plot;
    preparePlot(plot);
    plot.replot(); // lay the rect out: the mapping below is the one the recording will use
    const QPointF text(plot.xAxis->coordToPixel(0.5), plot.yAxis->coordToPixel(0.5));
    const QCPHandle<QCPItemAnnotation> item = plot.axisRect()->annotation(
        QCP::Pixels { text.x() + 2, text.y() + 2 }, QCP::Data { 0.5, 0.5 }, QStringLiteral("label"),
        { .head = QCPLineEnding::esLineArrow });
    QVERIFY(item);
    plot.replot();

    const Recorded recorded = scan(*item);
    QVERIFY2(!recorded.line, "a target inside the text box leaves no connector to draw");
    QVERIFY2(recorded.text, "the text still draws");
}

void TestAnnotation::emptyTextStartsAtThePosition()
{
    QCustomPlot plot;
    preparePlot(plot);
    const QCPHandle<QCPItemAnnotation> item = plot.axisRect()->annotation(
        QCP::Pixels { 300, 200 }, QCP::Pixels { 100, 100 }, QString());
    QVERIFY(item);
    plot.replot();

    const Recorded recorded = scan(*item);
    QVERIFY2(recorded.line, "an empty text has no box to cut, so the connector draws");
    QCOMPARE(firstOf(*recorded.line), QPointF(100, 100));
    QVERIFY2(!recorded.text, "an empty text records no text command");
}

void TestAnnotation::theTipEndsShrinkPixelsFromTheTarget()
{
    QCustomPlot plot;
    preparePlot(plot);
    const QCPHandle<QCPItemAnnotation> item
        = plot.axisRect()->annotation(QCP::Data { 0.3, 0.2 }, QCP::Data { 0.8, 0.6 },
                                      QStringLiteral("label"), { .shrink = 5_px });
    QVERIFY(item);
    plot.replot();

    const Recorded recorded = scan(*item);
    QVERIFY(recorded.line);
    const QPointF target(plot.xAxis->coordToPixel(0.3), plot.yAxis->coordToPixel(0.2));
    QVERIFY2(qAbs(QLineF(lastOf(*recorded.line), target).length() - 5) < 0.5,
             "the tip stops 5 px short of the target");
}

void TestAnnotation::theHeadPointsAlongThePath()
{
    QCustomPlot plot;
    preparePlot(plot);
    const QCPHandle<QCPItemAnnotation> item = plot.axisRect()->annotation(
        QCP::Data { 0.2, 0.2 }, QCP::Data { 0.8, 0.7 }, QStringLiteral("label"),
        { .connector = QCPConnector::curve(90, 0),
          .head = QCPLineEnding(QCPLineEnding::esSpikeArrow) });
    QVERIFY(item);
    plot.replot();

    const Recorded recorded = scan(*item);
    QVERIFY2(recorded.line, "a curve from a box to a target draws");
    QVERIFY2(recorded.head, "the head is recorded after the line");
    const QPointF tip = lastOf(*recorded.line);
    QCOMPARE(pixelOf(recorded.head->origin), tip);

    // The arrow's outline sits behind its tip, along the direction it points; comparing its centre
    // with the path's own last segment says whether the head is turned along the path.
    const QPointF behind = recorded.head->path.boundingRect().center();
    const double headAngle = angleOf(-behind);
    const double pathAngle = angleOf(tip - secondToLastOf(*recorded.line));
    QVERIFY2(qAbs(std::remainder(headAngle - pathAngle, 360.0)) < 1.0,
             qPrintable(QStringLiteral("the head points at %1 degrees, the path arrives at %2")
                            .arg(headAngle)
                            .arg(pathAngle)));
}

void TestAnnotation::aPanMovesTheTipNotTheText()
{
    QCustomPlot plot;
    preparePlot(plot);
    const QCPHandle<QCPItemAnnotation> item = plot.axisRect()->annotation(
        QCP::Data { 0.9, 0.2 }, QCP::Pixels { 60, 60 }, QStringLiteral("label"));
    QVERIFY(item);
    plot.replot();

    const Recorded before = scan(*item);
    QVERIFY(before.line && before.text);
    const QPointF tipBefore = lastOf(*before.line);
    const QPointF textBefore = pixelOf(before.text->at);

    plot.xAxis->setRange(0.1, 1.1);
    plot.replot();

    const Recorded after = scan(*item);
    QVERIFY(after.line && after.text);
    const QPointF tipAfter = lastOf(*after.line);
    QVERIFY2(tipAfter.x() < tipBefore.x() - 1, "the tip follows the data target across the pan");
    QCOMPARE(pixelOf(after.text->at), textBefore);
}

void TestAnnotation::aFarTargetDrawsInBoundedTime()
{
    QCustomPlot plot;
    preparePlot(plot);
    const QCPHandle<QCPItemAnnotation> item = plot.axisRect()->annotation(
        QCP::Data { 1e12, 1e12 }, QCP::Data { 0.5, 0.5 }, QStringLiteral("label"),
        { .head = QCPLineEnding::esLineArrow });
    QVERIFY(item);

    QElapsedTimer timer;
    timer.start();
    plot.replot();
    QVERIFY2(timer.elapsed() < 1000, "a target far outside the frame costs no time");

    const Recorded recorded = scan(*item);
    QVERIFY(recorded.text);
    if (recorded.line)
        for (const QCP::Position& position : recorded.line->points)
        {
            const QPointF point = pixelOf(position);
            QVERIFY2(std::isfinite(point.x()) && std::isfinite(point.y()),
                     "no coordinate that is not finite reaches the recording");
        }
}

void TestAnnotation::aLongConnectorSamplesWithinTheCap()
{
    QPainterPath path;
    path.moveTo(0, 0);
    path.lineTo(10000, 0);

    const qcp::TrimmedConnector trimmed = qcp::trimConnector(path, QRectF(), 0);

    QVERIFY2(trimmed.line.size() <= 4096,
             "a path longer than the cap is sampled coarser, not unbounded");
    QCOMPARE(trimmed.line.first(), QPointF(0, 0));
    QCOMPARE(trimmed.line.last(), QPointF(10000, 0));

    QPainterPath far;
    far.moveTo(0, 0);
    far.lineTo(4.4e14, 0);
    const qcp::TrimmedConnector huge = qcp::trimConnector(far, QRectF(), 0);

    QVERIFY2(huge.line.size() <= 4096,
             "a path of any length is sampled in a bounded number of points");
    QVERIFY(huge.line.size() > 1);
}

void TestAnnotation::theConnectorDrawsUnderTheText()
{
    QCustomPlot plot;
    preparePlot(plot);
    const QCPHandle<QCPItemAnnotation> item = plot.axisRect()->annotation(
        QCP::Data { 0.3, 0.2 }, QCP::Data { 0.8, 0.6 }, QStringLiteral("label"),
        { .head = QCPLineEnding::esLineArrow });
    QVERIFY(item);
    plot.replot();

    const Recorded recorded = scan(*item);
    QVERIFY(recorded.line && recorded.text);
    QVERIFY2(recorded.lineIndex < recorded.textIndex, "the text draws over the connector");
}

void TestAnnotation::aClickOnTheConnectorSelects()
{
    QCustomPlot plot;
    preparePlot(plot);
    plot.setInteractions(QCP::iSelectItems);
    const QCPHandle<QCPItemAnnotation> item = plot.axisRect()->annotation(
        QCP::Data { 0.2, 0.8 }, QCP::Pixels { 200, 150 }, QStringLiteral("label"));
    QVERIFY(item);
    item->setSelectable(QCP::stWhole);
    plot.replot();

    const Recorded recorded = scan(*item);
    QVERIFY(recorded.line);
    const QPointF from = firstOf(*recorded.line);
    const QPointF to = lastOf(*recorded.line);
    const QPointF middle = (from + to) / 2;
    const QPointF along = to - from;
    const double span = std::hypot(along.x(), along.y());
    const QPointF beside = middle + QPointF(-along.y(), along.x()) / span; // one pixel off the line

    click(plot, beside);
    QVERIFY2(item->selected(), "a click one pixel off the connector selects the annotation");

    plot.replot();
    const Recorded selected = scan(*item);
    QVERIFY(selected.line);
    QCOMPARE(selected.line->pen, item->selectedPen());
}

void TestAnnotation::theShortcutAppliesEveryOption()
{
    QCustomPlot plot;
    preparePlot(plot);
    const QCPConnector connector = QCPConnector::elbow(-90, 180, 5_px);
    const QPen pen(Qt::darkGreen, 2);
    const QCPLineEnding head(QCPLineEnding::esLineArrow, 6, 8);
    const QCPTextStyle style { .pen = QPen(Qt::darkRed),
                               .brush = Qt::white,
                               .padding = QMarginsF(4, 2, 4, 2) };
    const QCPHandle<QCPItemAnnotation> item = plot.axisRect()->annotation(
        QCP::Data { 0.2, 0.2 }, QCP::Data { 0.8, 0.8 }, QStringLiteral("label"),
        { .connector = connector, .pen = pen, .head = head, .shrink = 3_px, .textStyle = style });
    QVERIFY(item);

    QCOMPARE(item->text(), QStringLiteral("label"));
    QCOMPARE(item->connector(), connector);
    QCOMPARE(item->pen(), pen);
    QCOMPARE(item->head(), head);
    QCOMPARE(item->shrink().px, 3.0);
    QVERIFY2(item->textStyle() == style, "every field of the text style came through");
    QCOMPARE(item->position->pixelPosition(),
             QPointF(plot.xAxis->coordToPixel(0.8), plot.yAxis->coordToPixel(0.8)));
    QCOMPARE(item->target->pixelPosition(),
             QPointF(plot.xAxis->coordToPixel(0.2), plot.yAxis->coordToPixel(0.2)));
}

void TestAnnotation::settersRecordAgain()
{
    QCustomPlot plot;
    preparePlot(plot);
    const QCPHandle<QCPItemAnnotation> item = plot.axisRect()->annotation(
        QCP::Data { 0.3, 0.2 }, QCP::Data { 0.8, 0.6 }, QStringLiteral("label"));
    QVERIFY(item);

    const auto recordings = [&]
    {
        plot.replot();
        return AnnotationRecording::recordings(*item);
    };
    const int initial = recordings();

    item->setPen(QPen(Qt::red));
    QCOMPARE(recordings(), initial + 1);
    item->setPen(QPen(Qt::red));
    QCOMPARE(recordings(), initial + 1); // the same value again: nothing to redraw
    item->setText(QStringLiteral("other"));
    QCOMPARE(recordings(), initial + 2);
    item->setConnector(QCPConnector::arc(0.2));
    QCOMPARE(recordings(), initial + 3);
    item->setHead(QCPLineEnding::esLineArrow);
    QCOMPARE(recordings(), initial + 4);
    item->setShrink(2_px);
    QCOMPARE(recordings(), initial + 5);
    item->setSelectedPen(QPen(Qt::magenta));
    QCOMPARE(recordings(), initial + 6);
    item->setTextStyle(QCPTextStyle { .pen = QPen(Qt::blue) });
    QCOMPARE(recordings(), initial + 7);
}
