#include "test-annotation.h"

#include <construct.h>
#include <core.h>
#include <items/connector.h>
#include <items/item-annotation.h>
#include <items/item-text.h>
#include <layoutelements/layoutelement-axisrect.h>
#include <painting/draw-commands.h>
#include <theme.h>

#include <QElapsedTimer>
#include <QLineF>
#include <QMouseEvent>
#include <QPainterPath>
#include <QPointF>
#include <QPolygonF>

#include <algorithm>
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

//! The first recorded point at least \a distance away from \a from: a leg's direction needs a
//! baseline longer than one sample step to be read off.
QPointF firstPointAway(const qcp::PolylinePositionsCmd& line, QPointF from, double distance)
{
    for (const QCP::Position& position : line.points)
        if (QLineF(from, pixelOf(position)).length() >= distance)
            return pixelOf(position);
    return pixelOf(line.points.back());
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
    QVERIFY2(trimmed.line.size() == 2,
             "a straight path keeps its own two vertices between the cuts, not a sample per pixel");
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
        { .connector = QCPConnector::elbow(-90, 45),
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

void TestAnnotation::aFarPixelTargetDrawsInBoundedTime()
{
    // A pixel target is never hidden by the rect, so this connector really is drawn, a trillion
    // pixels long: it must still cost a bounded time and every sampled point must be a number.
    QCustomPlot plot;
    preparePlot(plot);
    const QCPHandle<QCPItemAnnotation> item = plot.axisRect()->annotation(
        QCP::Pixels { 1e12, 1e12 }, QCP::Pixels { 100, 100 }, QStringLiteral("label"),
        { .head = QCPLineEnding::esLineArrow });
    QVERIFY(item);

    QElapsedTimer timer;
    timer.start();
    plot.replot();
    QVERIFY2(timer.elapsed() < 1000, "a target a trillion pixels away costs no time");

    const Recorded recorded = scan(*item);
    QVERIFY2(recorded.line, "a target in pixels is not hidden by the rect");
    for (const QCP::Position& point : recorded.line->points)
    {
        const QPointF pixel = pixelOf(point);
        QVERIFY2(std::isfinite(pixel.x()) && std::isfinite(pixel.y()),
                 "every sampled point of a huge connector is a number");
    }
}

void TestAnnotation::aSelectedFarTargetDrawsInBoundedTime()
{
    // The selected pen is two pixels wide, so the recording also carries a head and a thick outline
    // built from a trillion-pixel direction: both still have to be numbers, and the replot bounded.
    QCustomPlot plot;
    preparePlot(plot);
    plot.setInteractions(QCP::iSelectItems);
    const QCPHandle<QCPItemAnnotation> item = plot.axisRect()->annotation(
        QCP::Pixels { 1e12, 1e12 }, QCP::Pixels { 100, 100 }, QStringLiteral("label"),
        { .head = QCPLineEnding::esLineArrow });
    QVERIFY(item);
    item->setSelectable(QCP::stWhole);
    item->setSelected(true);

    QElapsedTimer timer;
    timer.start();
    plot.replot();
    QVERIFY2(timer.elapsed() < 1000,
             "a selected connector to a trillion-pixel target costs no time");

    const Recorded recorded = scan(*item);
    QVERIFY2(recorded.line, "a selected annotation to a pixel target draws");
    QCOMPARE(recorded.line->pen, item->selectedPen());
    for (const QCP::Position& point : recorded.line->points)
    {
        const QPointF pixel = pixelOf(point);
        QVERIFY2(std::isfinite(pixel.x()) && std::isfinite(pixel.y()),
                 "every point of a selected huge connector is a number");
    }
    QVERIFY2(recorded.head, "the head of a selected far connector is recorded");
    const QPointF tip = pixelOf(recorded.head->origin);
    QVERIFY(std::isfinite(tip.x()) && std::isfinite(tip.y()));
    const QRectF head = recorded.head->path.boundingRect();
    QVERIFY(std::isfinite(head.width()) && std::isfinite(head.height()));
}

void TestAnnotation::aLongArcTrimsInLinearTime()
{
    // A trim walks the path's own flattened polyline, not one point per pixel: a 20000 px arc is a
    // few hundred vertices, where sampling the path per pixel would be 20000 twice per call.
    const QPainterPath arc = QCPConnector::arc(0.3).path(QPointF(0, 0), QPointF(20000, 0));
    const QRectF box(-20, -20, 40, 40);

    qcp::TrimmedConnector trimmed;
    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < 50; ++i)
        trimmed = qcp::trimConnector(arc, box, 5);
    const qint64 elapsed = timer.elapsed();

    QVERIFY2(trimmed.line.size() > 1, "the long arc keeps a run past the box");
    QVERIFY2(elapsed < 100,
             qPrintable(QStringLiteral("50 trims of a 20000 px arc took %1 ms").arg(elapsed)));
    for (const QPointF& point : trimmed.line)
        QVERIFY2(std::isfinite(point.x()) && std::isfinite(point.y()),
                 "every kept point of the long arc is a number");

    const QPainterPath far = QCPConnector::arc(0.3).path(QPointF(0, 0), QPointF(1e12, 1e12));
    const qcp::TrimmedConnector huge = qcp::trimConnector(far, box, 5);
    QVERIFY(huge.line.size() > 1);
    for (const QPointF& point : huge.line)
        QVERIFY2(std::isfinite(point.x()) && std::isfinite(point.y()),
                 "a far pixel target keeps every point finite");
}

void TestAnnotation::changingTextFlagsRemeasuresTheText()
{
    // Both items share one glyph cache, keyed on everything qcpMeasureText reads: the text's flags
    // can change the size, so a new alignment must measure again.
    QCustomPlot plot;
    preparePlot(plot);
    const QCPHandle<QCPItemText> text = plot.add<QCPItemText>();
    text->setPosition(QCP::Pixels { 100, 100 });
    text->setText(QStringLiteral("a\tb"));
    text->setMathText(false);
    text->setTextAlignment(Qt::AlignLeft);
    plot.replot();

    const quint64 measured = qcpTextMeasureCount();
    text->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    plot.replot();
    QVERIFY2(qcpTextMeasureCount() > measured, "another alignment re-measures the text");
}

void TestAnnotation::changingTextFlagsRemeasuresTheAnnotation()
{
    QCustomPlot plot;
    preparePlot(plot);
    const QCPHandle<QCPItemAnnotation> item = plot.axisRect()->annotation(
        QCP::Data { 0.3, 0.2 }, QCP::Data { 0.8, 0.6 }, QStringLiteral("a\tb"),
        { .textStyle = { .textFlags = int(Qt::AlignLeft), .mathText = false } });
    QVERIFY(item);
    plot.replot();

    const quint64 measured = qcpTextMeasureCount();
    item->setTextStyle(
        QCPTextStyle { .textFlags = int(Qt::AlignLeft | Qt::TextExpandTabs), .mathText = false });
    plot.replot();
    QVERIFY2(qcpTextMeasureCount() > measured, "another textFlags re-measures the label");
}

void TestAnnotation::aDefaultPenTakesTheThemeForeground()
{
    // An annotation on a dark theme has no colour of its own: the label takes the theme's
    // foreground, and so does the connector, or a black arrow would come with light text.
    QCustomPlot plot;
    preparePlot(plot);
    QCPTheme* theme = QCPTheme::dark();
    plot.setTheme(theme);
    const QCPHandle<QCPItemAnnotation> plain = plot.axisRect()->annotation(
        QCP::Data { 0.3, 0.2 }, QCP::Data { 0.8, 0.6 }, QStringLiteral("label"));
    const QCPHandle<QCPItemAnnotation> explicitPen
        = plot.axisRect()->annotation(QCP::Data { 0.3, 0.75 }, QCP::Data { 0.8, 0.95 },
                                      QStringLiteral("label"), { .pen = QPen(Qt::darkGreen) });
    QVERIFY(plain);
    QVERIFY(explicitPen);
    QCOMPARE(plain->pen(), QPen()); // the shortcut's default pen, no colour asked for
    plot.replot();

    const Recorded plainLine = scan(*plain);
    QVERIFY2(plainLine.line, "a default annotation draws its connector on a dark theme");
    QCOMPARE(plainLine.line->pen.color(), theme->foreground());

    const Recorded kept = scan(*explicitPen);
    QVERIFY(kept.line);
    QCOMPARE(kept.line->pen.color(), QColor(Qt::darkGreen));
}

void TestAnnotation::aFarDataTargetRecordsNothing()
{
    QCustomPlot plot;
    preparePlot(plot);
    const QCPHandle<QCPItemAnnotation> item = plot.axisRect()->annotation(
        QCP::Data { 1e12, 1e12 }, QCP::Data { 0.5, 0.5 }, QStringLiteral("label"),
        { .head = QCPLineEnding::esLineArrow });
    QVERIFY(item);
    plot.replot();

    QVERIFY2(AnnotationRecording::commands(*item).empty(),
             "a data target a trillion units away is outside the rect, so nothing is recorded");
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

void TestAnnotation::theTargetCutIsAtTheFirstApproach()
{
    // The path comes within the cut of its own end half way along, then leaves and comes back.
    // matplotlib's split_path_inout walks forward from the start: the cut is that first approach.
    QPainterPath path;
    path.moveTo(0, 0);
    path.lineTo(0, 20);
    path.lineTo(20, 20);
    path.lineTo(2, 2);
    path.lineTo(20, 2);
    path.lineTo(2, 0);

    const QPointF end(2, 0);
    const qcp::TrimmedConnector trimmed = qcp::trimConnector(path, QRectF(), 3);

    QVERIFY2(trimmed.line.size() > 1, "the run survives the cuts");
    QVERIFY2(QLineF(lastOf(trimmed), end).length() > 2.95,
             qPrintable(QStringLiteral("the line stops 3 px from the end, not %1")
                            .arg(QLineF(lastOf(trimmed), end).length())));
    QVERIFY2(QLineF(lastOf(trimmed), end).length() < 3.05, "and no further than the cut");
    const bool keptTheReturn = std::any_of(trimmed.line.cbegin(), trimmed.line.cend(),
                                           [](QPointF point)
                                           { return point.x() > 15 && qAbs(point.y() - 2) < 0.5; });
    QVERIFY2(!keptTheReturn, "the cut takes the first approach, so the last leg is not drawn");
}

void TestAnnotation::aTargetAtTheTopOfTheRangeStillDraws()
{
    QCustomPlot plot;
    preparePlot(plot);
    plot.replot();
    const QCPHandle<QCPItemAnnotation> top = plot.axisRect()->annotation(
        QCP::Data { 0.5, 1.0 }, QCP::Data { 0.2, 0.5 }, QStringLiteral("top"));
    const QCPHandle<QCPItemAnnotation> right = plot.axisRect()->annotation(
        QCP::Data { 1.0, 0.5 }, QCP::Data { 0.2, 0.2 }, QStringLiteral("right"));
    QVERIFY(top);
    QVERIFY(right);
    plot.replot();

    QVERIFY2(scan(*top).line,
             "a target at the top of the y range maps one pixel above the rect and still draws");
    QVERIFY2(scan(*right).line, "a target at the top of the x range still draws");
}

void TestAnnotation::aTargetThreePixelsOutsideHides()
{
    QCustomPlot plot;
    preparePlot(plot);
    plot.replot();
    const QRect rect = plot.axisRect()->rect();
    const double past = 1.0 + 3.0 / rect.width();
    const QCPHandle<QCPItemAnnotation> item = plot.axisRect()->annotation(
        QCP::Data { past, 0.5 }, QCP::Data { 0.2, 0.5 }, QStringLiteral("label"));
    QVERIFY(item);
    plot.replot();

    QVERIFY2(AnnotationRecording::commands(*item).empty(),
             "three pixels outside the rect is outside it");
}

void TestAnnotation::aTargetOnlyPartlyInDataIsNotHidden()
{
    // Only a target wholly in data is hidden: with y in pixels there is no data place to judge.
    QCustomPlot plot;
    preparePlot(plot);
    plot.replot();
    const QRect rect = plot.axisRect()->rect();
    const QCPHandle<QCPItemAnnotation> item = plot.axisRect()->annotation(
        QCP::Data { 0.5, 0.5 }, QCP::Data { 0.2, 0.5 }, QStringLiteral("label"));
    QVERIFY(item);
    item->target->setTypeY(QCPItemPosition::ptAbsolute);
    item->target->setCoords(0.5, rect.bottom() + 50);
    plot.replot();

    QVERIFY2(scan(*item).text, "a target in pixels below the rect hides nothing");
}

void TestAnnotation::aTargetOutsideTheRectHidesTheAnnotation()
{
    QCustomPlot plot;
    preparePlot(plot);
    const QCPHandle<QCPItemAnnotation> item = plot.axisRect()->annotation(
        QCP::Data { 1.5, 0.5 }, QCP::Data { 0.5, 0.5 }, QStringLiteral("label"),
        { .head = QCPLineEnding::esLineArrow });
    QVERIFY(item);
    plot.replot();

    QVERIFY2(AnnotationRecording::commands(*item).empty(),
             "a data target outside the rect hides the annotation, text and all");
}

void TestAnnotation::aLabelPastTheEdgeDrawsWhole()
{
    QCustomPlot plot;
    preparePlot(plot);
    plot.replot();
    const double edge = plot.axisRect()->rect().right() - 4;
    const double middle = plot.axisRect()->rect().center().y();
    const QCPHandle<QCPItemAnnotation> item = plot.axisRect()->annotation(
        QCP::Data { 0.2, 0.5 }, QCP::Pixels { edge, middle }, QStringLiteral("a long label"));
    QVERIFY(item);
    plot.replot();

    const Recorded recorded = scan(*item);
    QVERIFY2(recorded.text, "the label hangs past the rect edge and still draws");
    QVERIFY2(recorded.line, "and so does its connector, uncut by the rect");
    const QPointF target(plot.xAxis->coordToPixel(0.2), plot.yAxis->coordToPixel(0.5));
    QVERIFY2(QLineF(lastOf(*recorded.line), target).length() < 0.5,
             "the connector runs all the way to its target");
}

void TestAnnotation::theTextTakesTheThemeColour()
{
    QCustomPlot plot;
    preparePlot(plot);
    const QCPHandle<QCPItemAnnotation> item
        = plot.axisRect()->annotation(QCP::Data { 0.3, 0.2 }, QCP::Data { 0.8, 0.6 },
                                      QStringLiteral("label"), { .pen = QPen(Qt::darkGreen) });
    QVERIFY(item);
    plot.replot();

    const Recorded plain = scan(*item);
    QVERIFY(plain.text);
    QCOMPARE(plain.text->style->color, plot.theme()->foreground());

    plot.setInteractions(QCP::iSelectItems);
    item->setSelectable(QCP::stWhole);
    item->setSelected(true);
    plot.replot();

    const Recorded selected = scan(*item);
    QVERIFY(selected.text);
    QCOMPARE(selected.line->pen, item->selectedPen());
    QVERIFY2(selected.text->style->color != item->selectedPen().color(),
             "the label keeps its own colour when the connector turns to the selected pen");
}

void TestAnnotation::rotatedTextKeepsTheConnectorInScreenSpace()
{
    QCustomPlot plot;
    preparePlot(plot);
    const QCPHandle<QCPItemAnnotation> item = plot.axisRect()->annotation(
        QCP::Data { 0.3, 0.2 }, QCP::Data { 0.8, 0.7 }, QStringLiteral("label"),
        { .connector = QCPConnector::elbow(-90, 180), .textStyle = { .rotationDegrees = 30 } });
    QVERIFY(item);
    plot.replot();

    const Recorded recorded = scan(*item);
    QVERIFY(recorded.line);
    QVERIFY2(recorded.line->points.size() > 2, "an elbow has two legs to tell apart");
    const QPointF first = firstOf(*recorded.line);
    const QPointF second = firstPointAway(*recorded.line, first, 5);
    QVERIFY2(qAbs(first.x() - second.x()) < 0.5,
             qPrintable(QStringLiteral("the first leg leaves straight up on screen, not %1 degrees "
                                       "around with the text")
                            .arg(angleOf(second - first))));
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
    item->setText(QStringLiteral("other"));
    QCOMPARE(recordings(), initial + 2);
    item->setConnector(QCPConnector::arc(0.2));
    QCOMPARE(recordings(), initial + 3);
    item->setConnector(QCPConnector::arc(0.2));
    QCOMPARE(recordings(), initial + 3);
    item->setHead(QCPLineEnding::esLineArrow);
    QCOMPARE(recordings(), initial + 4);
    item->setHead(QCPLineEnding::esLineArrow);
    QCOMPARE(recordings(), initial + 4);
    item->setShrink(2_px);
    QCOMPARE(recordings(), initial + 5);
    item->setShrink(2_px);
    QCOMPARE(recordings(), initial + 5);
    const double none = std::numeric_limits<double>::quiet_NaN();
    item->setShrink(QCP::PixelLength { none });
    QCOMPARE(recordings(), initial + 6);
    item->setShrink(QCP::PixelLength { none });
    QCOMPARE(recordings(), initial + 6); // NaN equals itself here: the same value again
    item->setSelectedPen(QPen(Qt::magenta));
    QCOMPARE(recordings(), initial + 7);
    item->setSelectedPen(QPen(Qt::magenta));
    QCOMPARE(recordings(), initial + 7);
    item->setTextStyle(QCPTextStyle { .pen = QPen(Qt::blue) });
    QCOMPARE(recordings(), initial + 8);
    item->setTextStyle(QCPTextStyle { .pen = QPen(Qt::blue) });
    QCOMPARE(recordings(), initial + 8);
}
