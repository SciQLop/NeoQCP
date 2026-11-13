#include "test-span.h"

#include <functional>

#include "../../../src/qcp.h"
#include "reference-image.h"

namespace
{

void addOrientationRows()
{
    QTest::addColumn<bool>("vertical");
    QTest::newRow("vertical") << true;
    QTest::newRow("horizontal") << false;
}

// The axis the span's range runs along, and the one its band covers.
QCPAxis* rangeAxis(QCustomPlot* plot, bool vertical)
{
    return vertical ? plot->xAxis : plot->yAxis;
}

Qt::Orientation orientationOf(bool vertical)
{
    return vertical ? Qt::Vertical : Qt::Horizontal;
}

// A point from a range pixel and a pixel along the covered side, in the span's own orientation.
QPointF pointOf(bool vertical, double rangePx, double acrossPx)
{
    return vertical ? QPointF(rangePx, acrossPx) : QPointF(acrossPx, rangePx);
}

QPointF dragOf(bool vertical, double delta)
{
    return vertical ? QPointF(delta, 0) : QPointF(0, delta);
}

double acrossMiddle(QCustomPlot* plot, bool vertical)
{
    const QRect rect = plot->axisRect()->rect();
    return vertical ? rect.center().y() : rect.center().x();
}

void removeOnRangeChange(QCustomPlot* plot, QCPItemSpan* span)
{
    QObject::connect(span, &QCPItemSpan::rangeChanged, plot,
                     [plot, span] { QVERIFY(plot->removeItem(span)); });
}

void sendMouse(QCustomPlot* plot, QEvent::Type type, QPoint pos, Qt::MouseButtons buttons)
{
    QMouseEvent e(type, pos, plot->mapToGlobal(pos), Qt::LeftButton, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(plot, &e);
}

//! The band is the default blue brush over white: more blue than red, whatever the grid does.
bool isBand(const QColor& colour)
{
    return colour.blue() > colour.red() + 15;
}

QImage renderSpan(const std::function<void(QCustomPlot&)>& setup)
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
    setup(plot);
    plot.replot();
    return plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
}

void checkReference(const QImage& image, const char* name)
{
    QString why;
    QVERIFY2(qcp::test::matchesReference(image, QStringLiteral("test-reference-m4e"),
                                         QLatin1String(name), &why),
             qPrintable(QStringLiteral("%1: %2").arg(QLatin1String(name), why)));
}

//! A span that exposes its recording: a pan must not remake it.
class CountingSpan : public QCPItemSpan
{
public:
    using QCPItemSpan::QCPItemSpan;

    int recordings() const { return mCore.recordingCount(); }

    int recordedCommands() const { return mCore.recordedCommandCount(); }
};

} // namespace

void TestSpan::init()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
    mPlot->xAxis->setRange(0, 10);
    mPlot->yAxis->setRange(0, 10);
    mPlot->replot();
}

void TestSpan::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
}

void TestSpan::createAndRange()
{
    auto* vertical = new QCPItemSpan(mPlot, Qt::Vertical);
    vertical->setRange(QCPRange(2, 5));
    QCOMPARE(vertical->range().lower, 2.0);
    QCOMPARE(vertical->range().upper, 5.0);

    auto* horizontal = new QCPItemSpan(mPlot, Qt::Horizontal);
    horizontal->setRange(QCPRange(6, 9));
    QCOMPARE(horizontal->range().lower, 6.0);
    QCOMPARE(horizontal->range().upper, 9.0);
}

void TestSpan::orientationIsKept()
{
    auto* vertical = new QCPItemSpan(mPlot, Qt::Vertical);
    auto* horizontal = new QCPItemSpan(mPlot, Qt::Horizontal);
    QCOMPARE(vertical->orientation(), Qt::Vertical);
    QCOMPARE(horizontal->orientation(), Qt::Horizontal);
}

void TestSpan::setRange()
{
    auto* span = new QCPItemSpan(mPlot, Qt::Vertical);

    QSignalSpy spy(span, &QCPItemSpan::rangeChanged);
    span->setRange(QCPRange(3, 7));

    QCOMPARE(spy.count(), 1);
    auto emittedRange = spy.at(0).at(0).value<QCPRange>();
    QCOMPARE(emittedRange.lower, 3.0);
    QCOMPARE(emittedRange.upper, 7.0);
}

void TestSpan::invertedRange()
{
    auto* span = new QCPItemSpan(mPlot, Qt::Vertical);
    span->setRange(QCPRange(8, 2));
    // setRange normalizes: lower <= upper
    QCOMPARE(span->range().lower, 2.0);
    QCOMPARE(span->range().upper, 8.0);
    // draw should not crash
    mPlot->replot();
    QVERIFY(true);
}

void TestSpan::selectTestEdges_data()
{
    addOrientationRows();
}

void TestSpan::selectTestEdges()
{
    QFETCH(bool, vertical);
    auto* span = new QCPItemSpan(mPlot, orientationOf(vertical));
    span->setRange(QCPRange(2, 8));
    mPlot->replot();

    const double lowerPx = rangeAxis(mPlot, vertical)->coordToPixel(2);
    QVariant details;
    const double dist = span->selectTest(pointOf(vertical, lowerPx, acrossMiddle(mPlot, vertical)),
                                         false, &details);
    QVERIFY(dist >= 0);
    QCOMPARE(details.toInt(), static_cast<int>(QCPItemSpan::hpLowerEdge));
}

void TestSpan::selectTestFill_data()
{
    addOrientationRows();
}

void TestSpan::selectTestFill()
{
    QFETCH(bool, vertical);
    auto* span = new QCPItemSpan(mPlot, orientationOf(vertical));
    span->setRange(QCPRange(2, 8));
    mPlot->replot();

    const double midPx = rangeAxis(mPlot, vertical)->coordToPixel(5);
    QVariant details;
    const double dist = span->selectTest(pointOf(vertical, midPx, acrossMiddle(mPlot, vertical)),
                                         false, &details);
    QVERIFY(dist >= 0);
    QCOMPARE(details.toInt(), static_cast<int>(QCPItemSpan::hpFill));
}

void TestSpan::selectTestMiss_data()
{
    addOrientationRows();
}

void TestSpan::selectTestMiss()
{
    QFETCH(bool, vertical);
    auto* span = new QCPItemSpan(mPlot, orientationOf(vertical));
    span->setRange(QCPRange(2, 4));
    mPlot->replot();

    const double farPx = rangeAxis(mPlot, vertical)->coordToPixel(9);
    const double dist
        = span->selectTest(pointOf(vertical, farPx, acrossMiddle(mPlot, vertical)), false);
    QCOMPARE(dist, -1.0);
}

void TestSpan::movableFlag()
{
    auto* span = new QCPItemSpan(mPlot, Qt::Vertical);
    QVERIFY(span->movable());
    span->setMovable(false);
    QVERIFY(!span->movable());
}

void TestSpan::drawDoesNotCrash()
{
    auto* span = new QCPItemSpan(mPlot, Qt::Vertical);
    span->setRange(QCPRange(1, 9));
    mPlot->replot();
    QVERIFY(true);
}

void TestSpan::drawOnLogAxis_data()
{
    addOrientationRows();
}

void TestSpan::drawOnLogAxis()
{
    QFETCH(bool, vertical);
    QCPAxis* axis = rangeAxis(mPlot, vertical);
    axis->setScaleType(QCPAxis::stLogarithmic);
    axis->setRange(1, 1000);
    auto* span = new QCPItemSpan(mPlot, orientationOf(vertical));
    span->setRange(QCPRange(10, 100));
    mPlot->replot();
    QVERIFY(true);
}

void TestSpan::exportFallbackRendersSpan_data()
{
    addOrientationRows();
}

void TestSpan::exportFallbackRendersSpan()
{
    QFETCH(bool, vertical);
    auto* span = new QCPItemSpan(mPlot, orientationOf(vertical));
    span->setRange(QCPRange(3, 7));
    span->setBrush(QBrush(QColor(255, 0, 0, 128)));
    span->setBorderPen(Qt::NoPen);
    mPlot->replot();
    QPixmap pm = mPlot->toPixmap(400, 300);
    QVERIFY(!pm.isNull());
    QImage img = pm.toImage();
    const QPoint at = pointOf(vertical, rangeAxis(mPlot, vertical)->coordToPixel(5),
                              acrossMiddle(mPlot, vertical))
                          .toPoint();
    QColor c
        = img.pixelColor(qBound(0, at.x(), img.width() - 1), qBound(0, at.y(), img.height() - 1));
    QVERIFY2(c.red() > 50, qPrintable(QString("Expected red > 50, got %1").arg(c.red())));
}

void TestSpan::dirtyTrackingReplots()
{
    auto* span = new QCPItemSpan(mPlot, Qt::Vertical);
    span->setRange(QCPRange(2, 8));
    mPlot->replot();

    span->setBrush(QBrush(Qt::blue));
    mPlot->replot();
    mPlot->replot();
    span->setBorderPen(QPen(Qt::red, 3));
    mPlot->replot();
    span->setRange(QCPRange(1, 9));
    mPlot->replot();
    span->setSelectedBrush(QBrush(Qt::yellow));
    mPlot->replot();
    span->setSelectedBorderPen(QPen(Qt::magenta, 2));
    mPlot->replot();
    QVERIFY(true);
}

void TestSpan::multiAxisRectSpans()
{
    auto* ar2 = new QCPAxisRect(mPlot);
    mPlot->plotLayout()->addElement(0, 1, ar2);

    auto* span1 = new QCPItemSpan(mPlot, Qt::Vertical);
    span1->setRange(QCPRange(2, 8));
    span1->setBrush(QBrush(QColor(255, 0, 0, 80)));

    auto* span2 = new QCPItemSpan(mPlot, Qt::Horizontal);
    span2->lowerEdge->setAxes(ar2->axis(QCPAxis::atBottom), ar2->axis(QCPAxis::atLeft));
    span2->upperEdge->setAxes(ar2->axis(QCPAxis::atBottom), ar2->axis(QCPAxis::atLeft));
    span2->setRange(QCPRange(3, 7));
    span2->setBrush(QBrush(QColor(0, 0, 255, 80)));

    mPlot->replot();
    QPixmap pm = mPlot->toPixmap(600, 300);
    QVERIFY(!pm.isNull());
}

void TestSpan::edgesHonourTheirPositionType_data()
{
    addOrientationRows();
}

void TestSpan::edgesHonourTheirPositionType()
{
    // The edges are QCPItemPositions, so a non-plot-coords type must be honoured
    // rather than the coords being force-fed to QCPAxis::coordToPixel().
    QFETCH(bool, vertical);
    mPlot->replot();
    auto* span = new QCPItemSpan(mPlot, orientationOf(vertical));
    if (vertical)
    {
        span->lowerEdge->setTypeX(QCPItemPosition::ptAxisRectAbsolute);
        span->upperEdge->setTypeX(QCPItemPosition::ptAxisRectAbsolute);
    }
    else
    {
        span->lowerEdge->setTypeY(QCPItemPosition::ptAxisRectAbsolute);
        span->upperEdge->setTypeY(QCPItemPosition::ptAxisRectAbsolute);
    }
    span->setRange(QCPRange(100, 200));

    const QRect axRect = mPlot->axisRect()->rect();
    const QPointF centre = span->center->pixelPosition();
    const double centreAlong = vertical ? centre.x() : centre.y();
    const double expected = (vertical ? axRect.left() : axRect.top()) + 150.0;
    QCOMPARE(centreAlong, expected);
    // Inside the span (filled by the default brush).
    QVERIFY(span->selectTest(pointOf(vertical, expected, acrossMiddle(mPlot, vertical)), false)
            >= 0);
    // Outside it -- and where a coordToPixel() misreading would have put it.
    const double outside = (vertical ? axRect.left() : axRect.top()) + 300.0;
    QVERIFY(span->selectTest(pointOf(vertical, outside, acrossMiddle(mPlot, vertical)), false) < 0);
}

// A slot deleting the span from its own signal: the span must not touch itself afterwards.
void TestSpan::setRangeSlotDeletingTheSpanIsSafe()
{
    auto* span = new QCPItemSpan(mPlot, Qt::Vertical);
    removeOnRangeChange(mPlot, span);
    span->setRange(QCPRange(3, 7));
    QCOMPARE(mPlot->itemCount(), 0);
}

void TestSpan::dragSlotDeletingTheSpanIsSafe()
{
    auto* span = new QCPItemSpan(mPlot, Qt::Vertical);
    span->setRange(QCPRange(3, 7));
    mPlot->replot();
    removeOnRangeChange(mPlot, span);
    const QPoint from
        = pointOf(true, mPlot->xAxis->coordToPixel(5), acrossMiddle(mPlot, true)).toPoint();
    sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseMove, from + QPoint(20, 0), Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseButtonRelease, from + QPoint(20, 0), Qt::NoButton);
    QCOMPARE(mPlot->itemCount(), 0);
}

void TestSpan::pixelModeDragMovesByPixels_data()
{
    addOrientationRows();
}

// Pixels-mode edges are axis-rect pixels: a 40 px drag moves them 40 px, not 40 px of data units.
void TestSpan::pixelModeDragMovesByPixels()
{
    QFETCH(bool, vertical);
    rangeAxis(mPlot, vertical)->setRange(0, 1000);
    mPlot->replot();
    auto* span = new QCPItemSpan(mPlot, orientationOf(vertical));
    if (vertical)
    {
        span->lowerEdge->setTypeX(QCPItemPosition::ptAxisRectAbsolute);
        span->upperEdge->setTypeX(QCPItemPosition::ptAxisRectAbsolute);
    }
    else
    {
        span->lowerEdge->setTypeY(QCPItemPosition::ptAxisRectAbsolute);
        span->upperEdge->setTypeY(QCPItemPosition::ptAxisRectAbsolute);
    }
    span->setRange(QCPRange(100, 200));
    const QRect axRect = mPlot->axisRect()->rect();
    const QPoint from = pointOf(vertical, (vertical ? axRect.left() : axRect.top()) + 150,
                                acrossMiddle(mPlot, vertical))
                            .toPoint();
    const QPoint to = (from + dragOf(vertical, 40)).toPoint();
    sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseMove, to, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseButtonRelease, to, Qt::NoButton);
    QCOMPARE(span->range().lower, 140.0);
    QCOMPARE(span->range().upper, 240.0);
}

void TestSpan::dataDragMovesByMouseDelta_data()
{
    addOrientationRows();
}

void TestSpan::dataDragMovesByMouseDelta()
{
    QFETCH(bool, vertical);
    auto* span = new QCPItemSpan(mPlot, orientationOf(vertical));
    span->setRange(QCPRange(3, 7));
    mPlot->replot();
    const auto* axis = rangeAxis(mPlot, vertical);
    const double lower = axis->pixelToCoord(axis->coordToPixel(3) + 40);
    const double upper = axis->pixelToCoord(axis->coordToPixel(7) + 40);
    const QPoint from
        = pointOf(vertical, axis->coordToPixel(5), acrossMiddle(mPlot, vertical)).toPoint();
    const QPoint to = (from + dragOf(vertical, 40)).toPoint();
    sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseMove, to, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseButtonRelease, to, Qt::NoButton);
    QCOMPARE(span->range().lower, lower);
    QCOMPARE(span->range().upper, upper);
}

void TestSpan::logAxisDragFollowsTheMouseInPixels_data()
{
    addOrientationRows();
}

// On a log axis the drag follows the mouse in pixels: the edges move by different data amounts.
void TestSpan::logAxisDragFollowsTheMouseInPixels()
{
    QFETCH(bool, vertical);
    QCPAxis* axis = rangeAxis(mPlot, vertical);
    axis->setScaleType(QCPAxis::stLogarithmic);
    axis->setRange(1, 1000);
    mPlot->replot();
    auto* span = new QCPItemSpan(mPlot, orientationOf(vertical));
    span->setRange(QCPRange(10, 100));
    mPlot->replot();
    const double lower = axis->pixelToCoord(axis->coordToPixel(10) + 40);
    const double upper = axis->pixelToCoord(axis->coordToPixel(100) + 40);
    const QPoint from
        = pointOf(vertical, axis->coordToPixel(30), acrossMiddle(mPlot, vertical)).toPoint();
    const QPoint to = (from + dragOf(vertical, 40)).toPoint();
    sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseMove, to, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseButtonRelease, to, Qt::NoButton);
    QCOMPARE(span->range().lower, lower);
    QCOMPARE(span->range().upper, upper);
}

void TestSpan::mixedEdgesEachMoveInTheirOwnUnit_data()
{
    addOrientationRows();
}

void TestSpan::mixedEdgesEachMoveInTheirOwnUnit()
{
    QFETCH(bool, vertical);
    auto* span = new QCPItemSpan(mPlot, orientationOf(vertical));
    if (vertical)
    {
        span->lowerEdge->setTypeX(QCPItemPosition::ptAxisRectAbsolute);
        span->lowerEdge->setCoords(100, 0);
        span->upperEdge->setCoords(7, 1);
    }
    else
    {
        span->lowerEdge->setTypeY(QCPItemPosition::ptAxisRectAbsolute);
        span->lowerEdge->setCoords(0, 200);
        span->upperEdge->setCoords(1, 7);
    }
    mPlot->replot();
    const auto* axis = rangeAxis(mPlot, vertical);
    const QRect axRect = mPlot->axisRect()->rect();
    const double upper = axis->pixelToCoord(axis->coordToPixel(7) + 40);
    const QPoint from = pointOf(vertical, (vertical ? axRect.left() : axRect.top()) + 150,
                                acrossMiddle(mPlot, vertical))
                            .toPoint();
    const QPoint to = (from + dragOf(vertical, 40)).toPoint();
    sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseMove, to, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseButtonRelease, to, Qt::NoButton);
    const QPointF lower = span->lowerEdge->coords();
    // The pixel-mode edge starts at 100 (vertical) or 200 (horizontal), then follows the mouse.
    QCOMPARE(vertical ? lower.x() : lower.y(), vertical ? 140.0 : 240.0);
    const QPointF upperEdge = span->upperEdge->coords();
    QCOMPARE(vertical ? upperEdge.x() : upperEdge.y(), upper);
}

void TestSpan::dragMovesAnEdge_data()
{
    addOrientationRows();
}

// A drag on one edge moves that edge only.
void TestSpan::dragMovesAnEdge()
{
    QFETCH(bool, vertical);
    auto* span = new QCPItemSpan(mPlot, orientationOf(vertical));
    span->setRange(QCPRange(3, 7));
    mPlot->replot();
    const auto* axis = rangeAxis(mPlot, vertical);
    const double lower = axis->pixelToCoord(axis->coordToPixel(3) - 20);
    const QPoint from
        = pointOf(vertical, axis->coordToPixel(3), acrossMiddle(mPlot, vertical)).toPoint();
    const QPoint to = (from + dragOf(vertical, -20)).toPoint();
    sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseMove, to, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseButtonRelease, to, Qt::NoButton);
    QCOMPARE(span->range().lower, lower);
    QCOMPARE(span->range().upper, 7.0);
}

void TestSpan::aSpanIsRecorded()
{
    auto* span = new CountingSpan(mPlot, Qt::Vertical);
    span->setRange(QCPRange(2, 8));
    QCOMPARE(span->recordedCommands(), 0);
    mPlot->replot();
    QCOMPARE(span->recordings(), 1);
    QVERIFY2(span->recordedCommands() > 0, "the span does not record");
}

void TestSpan::spanFollowsADataPanAndKeepsItsRatioSide_data()
{
    addOrientationRows();
}

// The band's data side follows the keys; its ratio side keeps covering the whole axis rect.
void TestSpan::spanFollowsADataPanAndKeepsItsRatioSide()
{
    QFETCH(bool, vertical);
    auto* span = new CountingSpan(mPlot, orientationOf(vertical));
    span->setRange(QCPRange(4, 6));
    mPlot->replot();
    QCOMPARE(span->recordings(), 1);

    QCPAxis* along = rangeAxis(mPlot, vertical);
    QCPAxis* across = vertical ? mPlot->yAxis : mPlot->xAxis;
    along->setRange(2, 12);
    mPlot->replot();
    QCOMPARE(span->recordings(), 1);

    QImage image = mPlot->toPixmap(400, 300).toImage();
    const QRect rect = mPlot->axisRect()->rect();
    const int middle = qRound(acrossMiddle(mPlot, vertical));
    const QPoint inside = pointOf(vertical, along->coordToPixel(5), middle).toPoint();
    const QPoint outside = pointOf(vertical, along->coordToPixel(3), middle).toPoint();
    QVERIFY2(isBand(image.pixelColor(inside)), "the band did not follow the key pan");
    QVERIFY2(!isBand(image.pixelColor(outside)), "the band stayed where it was");

    across->setRange(across->range().lower + 5, across->range().upper + 5);
    mPlot->replot();
    QCOMPARE(span->recordings(), 1);
    image = mPlot->toPixmap(400, 300).toImage();
    const QPoint nearStart
        = pointOf(vertical, along->coordToPixel(5), vertical ? rect.top() + 3 : rect.left() + 3)
              .toPoint();
    const QPoint nearEnd
        = pointOf(vertical, along->coordToPixel(5), vertical ? rect.bottom() - 3 : rect.right() - 3)
              .toPoint();
    QVERIFY2(isBand(image.pixelColor(nearStart)), "the ratio side did not keep the whole height");
    QVERIFY2(isBand(image.pixelColor(nearEnd)), "the ratio side did not keep the whole height");
}

void TestSpan::bothOrientationsMatchTheirReferences()
{
    const QImage horizontal = renderSpan(
        [](QCustomPlot& plot)
        {
            plot.xAxis->setRange(0, 6);
            plot.yAxis->setRange(0, 1);
            auto* span = new QCPItemSpan(&plot, Qt::Horizontal);
            span->setRange(QCPRange(0.3, 0.6));
            span->setBrush(QBrush(QColor(40, 90, 200, 50)));
            span->setBorderPen(QPen(QColor(40, 90, 200), 2));
        });
    checkReference(horizontal, "span-h.png");

    const QImage vertical = renderSpan(
        [](QCustomPlot& plot)
        {
            plot.xAxis->setRange(0, 6);
            plot.yAxis->setRange(0, 1);
            auto* span = new QCPItemSpan(&plot, Qt::Vertical);
            span->setRange(QCPRange(2.0, 4.0));
            span->setBrush(QBrush(QColor(200, 60, 40, 50)));
            span->setBorderPen(QPen(QColor(200, 60, 40), 2));
        });
    checkReference(vertical, "span-v.png");

    const QImage selected = renderSpan(
        [](QCustomPlot& plot)
        {
            plot.xAxis->setRange(0, 6);
            plot.yAxis->setRange(0, 1);
            auto* span = new QCPItemSpan(&plot, Qt::Vertical);
            span->setRange(QCPRange(1.5, 3.5));
            span->setBrush(QBrush(QColor(200, 60, 40, 50)));
            span->setBorderPen(QPen(QColor(200, 60, 40), 2));
            span->setSelected(true);
        });
    checkReference(selected, "span-selected.png");

    const QImage rectangle = renderSpan(
        [](QCustomPlot& plot)
        {
            plot.xAxis->setRange(0, 6);
            plot.yAxis->setRange(0, 1);
            auto* span = new QCPItemRSpan(&plot);
            span->leftEdge->setCoords(1.5, 0);
            span->rightEdge->setCoords(4.5, 1);
            span->topEdge->setCoords(0, 0.8);
            span->bottomEdge->setCoords(6, 0.3);
            span->setBrush(QBrush(QColor(30, 150, 90, 50)));
            span->setBorderPen(QPen(QColor(30, 150, 90), 2));
        });
    checkReference(rectangle, "span-r.png");
}
