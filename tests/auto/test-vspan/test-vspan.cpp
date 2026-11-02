#include "test-vspan.h"
#include "../../../src/qcp.h"

void TestVSpan::init()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
    mPlot->xAxis->setRange(0, 10);
    mPlot->yAxis->setRange(0, 10);
    mPlot->replot();
}

void TestVSpan::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
}

void TestVSpan::createAndRange()
{
    auto* span = new QCPItemVSpan(mPlot);
    span->setRange(QCPRange(2, 5));
    QCOMPARE(span->range().lower, 2.0);
    QCOMPARE(span->range().upper, 5.0);
}

void TestVSpan::setRange()
{
    auto* span = new QCPItemVSpan(mPlot);

    QSignalSpy spy(span, &QCPItemVSpan::rangeChanged);
    span->setRange(QCPRange(3, 7));

    QCOMPARE(spy.count(), 1);
    auto emittedRange = spy.at(0).at(0).value<QCPRange>();
    QCOMPARE(emittedRange.lower, 3.0);
    QCOMPARE(emittedRange.upper, 7.0);
}

void TestVSpan::invertedRange()
{
    auto* span = new QCPItemVSpan(mPlot);
    span->setRange(QCPRange(8, 2));
    // setRange normalizes: lower <= upper
    QCOMPARE(span->range().lower, 2.0);
    QCOMPARE(span->range().upper, 8.0);
    // draw should not crash
    mPlot->replot();
    QVERIFY(true);
}

void TestVSpan::selectTestEdges()
{
    auto* span = new QCPItemVSpan(mPlot);
    span->setRange(QCPRange(2, 8));
    mPlot->replot();

    double lowerPx = mPlot->xAxis->coordToPixel(2);
    double midY = mPlot->yAxis->axisRect()->center().y();

    QVariant details;
    double dist = span->selectTest(QPointF(lowerPx, midY), false, &details);
    QVERIFY(dist >= 0);
    QCOMPARE(details.toInt(), static_cast<int>(QCPItemVSpan::hpLowerEdge));
}

void TestVSpan::selectTestFill()
{
    auto* span = new QCPItemVSpan(mPlot);
    span->setRange(QCPRange(2, 8));
    mPlot->replot();

    double midPx = mPlot->xAxis->coordToPixel(5);
    double midY = mPlot->yAxis->axisRect()->center().y();

    QVariant details;
    double dist = span->selectTest(QPointF(midPx, midY), false, &details);
    QVERIFY(dist >= 0);
    QCOMPARE(details.toInt(), static_cast<int>(QCPItemVSpan::hpFill));
}

void TestVSpan::selectTestMiss()
{
    auto* span = new QCPItemVSpan(mPlot);
    span->setRange(QCPRange(2, 4));
    mPlot->replot();

    double farPx = mPlot->xAxis->coordToPixel(9);
    double midY = mPlot->yAxis->axisRect()->center().y();

    double dist = span->selectTest(QPointF(farPx, midY), false);
    QCOMPARE(dist, -1.0);
}

void TestVSpan::movableFlag()
{
    auto* span = new QCPItemVSpan(mPlot);
    QVERIFY(span->movable());
    span->setMovable(false);
    QVERIFY(!span->movable());
}

void TestVSpan::drawDoesNotCrash()
{
    auto* span = new QCPItemVSpan(mPlot);
    span->setRange(QCPRange(1, 9));
    mPlot->replot();
    QVERIFY(true);
}

void TestVSpan::drawOnLogAxis()
{
    mPlot->xAxis->setScaleType(QCPAxis::stLogarithmic);
    mPlot->xAxis->setRange(1, 1000);
    auto* span = new QCPItemVSpan(mPlot);
    span->setRange(QCPRange(10, 100));
    mPlot->replot();
    QVERIFY(true);
}

void TestVSpan::exportFallbackRendersSpan()
{
    auto* span = new QCPItemVSpan(mPlot);
    span->setRange(QCPRange(3, 7));
    span->setBrush(QBrush(QColor(255, 0, 0, 128)));
    span->setPen(Qt::NoPen);
    span->setBorderPen(Qt::NoPen);
    mPlot->replot();
    QPixmap pm = mPlot->toPixmap(400, 300);
    QVERIFY(!pm.isNull());
    QImage img = pm.toImage();
    int centerX = static_cast<int>(mPlot->xAxis->coordToPixel(5));
    int centerY = img.height() / 2;
    centerX = qBound(0, centerX, img.width() - 1);
    centerY = qBound(0, centerY, img.height() - 1);
    QColor c = img.pixelColor(centerX, centerY);
    QVERIFY2(c.red() > 50, qPrintable(QString("Expected red > 50, got %1").arg(c.red())));
}

void TestVSpan::dirtyTrackingReplots()
{
    auto* span = new QCPItemVSpan(mPlot);
    span->setRange(QCPRange(2, 8));
    mPlot->replot();

    span->setBrush(QBrush(Qt::blue));
    mPlot->replot();
    span->setPen(QPen(Qt::green));
    mPlot->replot();
    span->setBorderPen(QPen(Qt::red, 3));
    mPlot->replot();
    span->setRange(QCPRange(1, 9));
    mPlot->replot();
    span->setSelectedBrush(QBrush(Qt::yellow));
    mPlot->replot();
    span->setSelectedPen(QPen(Qt::cyan));
    mPlot->replot();
    span->setSelectedBorderPen(QPen(Qt::magenta, 2));
    mPlot->replot();
    QVERIFY(true);
}

void TestVSpan::multiAxisRectSpans()
{
    auto* ar2 = new QCPAxisRect(mPlot);
    mPlot->plotLayout()->addElement(0, 1, ar2);

    auto* span1 = new QCPItemVSpan(mPlot);
    span1->setRange(QCPRange(2, 8));
    span1->setBrush(QBrush(QColor(255, 0, 0, 80)));

    auto* span2 = new QCPItemVSpan(mPlot);
    span2->lowerEdge->setAxes(ar2->axis(QCPAxis::atBottom), ar2->axis(QCPAxis::atLeft));
    span2->upperEdge->setAxes(ar2->axis(QCPAxis::atBottom), ar2->axis(QCPAxis::atLeft));
    span2->setRange(QCPRange(3, 7));
    span2->setBrush(QBrush(QColor(0, 0, 255, 80)));

    mPlot->replot();
    QPixmap pm = mPlot->toPixmap(600, 300);
    QVERIFY(!pm.isNull());
}

void TestVSpan::spanRhiLayerNullWithoutRhi()
{
    // In offscreen test environment, no QRhi is available
    // spanRhiLayer() returns nullptr, spans use QPainter fallback
    QVERIFY(mPlot->spanRhiLayer() == nullptr);
}

void TestVSpan::edgesHonourTheirPositionType()
{
    // The edges are QCPItemPositions, so a non-plot-coords type must be honoured
    // rather than the coords being force-fed to QCPAxis::coordToPixel().
    mPlot->replot();
    QCPItemVSpan* span = new QCPItemVSpan(mPlot);
    span->lowerEdge->setTypeX(QCPItemPosition::ptAxisRectAbsolute);
    span->upperEdge->setTypeX(QCPItemPosition::ptAxisRectAbsolute);
    span->setRange(QCPRange(100, 200));

    const QRect axRect = mPlot->axisRect()->rect();
    const double midY = axRect.center().y();

    QCOMPARE(span->center->pixelPosition().x(), axRect.left() + 150.0);
    // Inside the span (filled by the default brush).
    QVERIFY(span->selectTest(QPointF(axRect.left() + 150.0, midY), false) >= 0);
    // Outside it -- and where a coordToPixel() misreading would have put it.
    QVERIFY(span->selectTest(QPointF(axRect.left() + 300.0, midY), false) < 0);
}

namespace {
void removeOnRangeChange(QCustomPlot* plot, QCPItemVSpan* span)
{
    QObject::connect(span, &QCPItemVSpan::rangeChanged, plot,
                     [plot, span] { QVERIFY(plot->removeItem(span)); });
}

void sendMouse(QCustomPlot* plot, QEvent::Type type, QPoint pos, Qt::MouseButtons buttons)
{
    QMouseEvent e(type, pos, plot->mapToGlobal(pos), Qt::LeftButton, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(plot, &e);
}
} // namespace

// A slot deleting the span from its own signal: the span must not touch itself afterwards.
void TestVSpan::setRangeSlotDeletingTheSpanIsSafe()
{
    auto* span = new QCPItemVSpan(mPlot);
    removeOnRangeChange(mPlot, span);
    span->setRange(QCPRange(3, 7));
    QCOMPARE(mPlot->itemCount(), 0);
}

void TestVSpan::dragSlotDeletingTheSpanIsSafe()
{
    auto* span = new QCPItemVSpan(mPlot);
    span->setRange(QCPRange(3, 7));
    mPlot->replot();
    removeOnRangeChange(mPlot, span);
    const int y = mPlot->axisRect()->rect().center().y();
    const QPoint from(qRound(mPlot->xAxis->coordToPixel(5)), y);
    sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseMove, from + QPoint(20, 0), Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseButtonRelease, from + QPoint(20, 0), Qt::NoButton);
    QCOMPARE(mPlot->itemCount(), 0);
}

// Pixels-mode edges are axis-rect pixels: a 40 px drag moves them 40 px, not 40 px of data units.
void TestVSpan::pixelModeDragMovesByPixels()
{
    mPlot->xAxis->setRange(0, 1000);
    mPlot->replot();
    auto* span = new QCPItemVSpan(mPlot);
    span->lowerEdge->setTypeX(QCPItemPosition::ptAxisRectAbsolute);
    span->upperEdge->setTypeX(QCPItemPosition::ptAxisRectAbsolute);
    span->setRange(QCPRange(100, 200));
    const QRect axRect = mPlot->axisRect()->rect();
    const QPoint from(axRect.left() + 150, axRect.center().y());
    sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseMove, from + QPoint(40, 0), Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseButtonRelease, from + QPoint(40, 0), Qt::NoButton);
    QCOMPARE(span->range().lower, 140.0);
    QCOMPARE(span->range().upper, 240.0);
}

void TestVSpan::dataDragMovesByMouseDelta()
{
    auto* span = new QCPItemVSpan(mPlot);
    span->setRange(QCPRange(3, 7));
    mPlot->replot();
    const QCPAxis* axis = mPlot->xAxis;
    const double lower = axis->pixelToCoord(axis->coordToPixel(3) + 40);
    const double upper = axis->pixelToCoord(axis->coordToPixel(7) + 40);
    const QPoint from(qRound(axis->coordToPixel(5)), mPlot->axisRect()->rect().center().y());
    sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseMove, from + QPoint(40, 0), Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseButtonRelease, from + QPoint(40, 0), Qt::NoButton);
    QCOMPARE(span->range().lower, lower);
    QCOMPARE(span->range().upper, upper);
}

// On a log axis the drag follows the mouse in pixels: the edges move by different data amounts.
void TestVSpan::logAxisDragFollowsTheMouseInPixels()
{
    mPlot->xAxis->setScaleType(QCPAxis::stLogarithmic);
    mPlot->xAxis->setRange(1, 1000);
    mPlot->replot();
    auto* span = new QCPItemVSpan(mPlot);
    span->setRange(QCPRange(10, 100));
    mPlot->replot();
    const QCPAxis* axis = mPlot->xAxis;
    const double lower = axis->pixelToCoord(axis->coordToPixel(10) + 40);
    const double upper = axis->pixelToCoord(axis->coordToPixel(100) + 40);
    const QPoint from(qRound(axis->coordToPixel(30)), mPlot->axisRect()->rect().center().y());
    sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseMove, from + QPoint(40, 0), Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseButtonRelease, from + QPoint(40, 0), Qt::NoButton);
    QCOMPARE(span->range().lower, lower);
    QCOMPARE(span->range().upper, upper);
}

void TestVSpan::mixedEdgesEachMoveInTheirOwnUnit()
{
    auto* span = new QCPItemVSpan(mPlot);
    span->lowerEdge->setTypeX(QCPItemPosition::ptAxisRectAbsolute);
    span->lowerEdge->setCoords(100, 0);
    span->upperEdge->setCoords(7, 1);
    mPlot->replot();
    const QCPAxis* axis = mPlot->xAxis;
    const QRect axRect = mPlot->axisRect()->rect();
    const double upper = axis->pixelToCoord(axis->coordToPixel(7) + 40);
    const QPoint from(axRect.left() + 150, axRect.center().y());
    sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseMove, from + QPoint(40, 0), Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseButtonRelease, from + QPoint(40, 0), Qt::NoButton);
    QCOMPARE(span->lowerEdge->coords().x(), 140.0);
    QCOMPARE(span->upperEdge->coords().x(), upper);
}
