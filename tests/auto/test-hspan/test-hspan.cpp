#include "test-hspan.h"
#include "../../../src/qcp.h"

void TestHSpan::init()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
    mPlot->xAxis->setRange(0, 10);
    mPlot->yAxis->setRange(0, 10);
    mPlot->replot();
}

void TestHSpan::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
}

void TestHSpan::createAndRange()
{
    auto* span = new QCPItemHSpan(mPlot);
    span->setRange(QCPRange(2, 5));
    QCOMPARE(span->range().lower, 2.0);
    QCOMPARE(span->range().upper, 5.0);
}

void TestHSpan::setRange()
{
    auto* span = new QCPItemHSpan(mPlot);

    QSignalSpy spy(span, &QCPItemHSpan::rangeChanged);
    span->setRange(QCPRange(3, 7));

    QCOMPARE(spy.count(), 1);
    auto emittedRange = spy.at(0).at(0).value<QCPRange>();
    QCOMPARE(emittedRange.lower, 3.0);
    QCOMPARE(emittedRange.upper, 7.0);
}

void TestHSpan::invertedRange()
{
    auto* span = new QCPItemHSpan(mPlot);
    span->setRange(QCPRange(8, 2));
    // setRange normalizes: lower <= upper
    QCOMPARE(span->range().lower, 2.0);
    QCOMPARE(span->range().upper, 8.0);
    // draw should not crash
    mPlot->replot();
    QVERIFY(true);
}

void TestHSpan::selectTestEdges()
{
    auto* span = new QCPItemHSpan(mPlot);
    span->setRange(QCPRange(2, 8));
    mPlot->replot();

    double lowerPx = mPlot->yAxis->coordToPixel(2);
    double midX = mPlot->xAxis->axisRect()->center().x();

    QVariant details;
    double dist = span->selectTest(QPointF(midX, lowerPx), false, &details);
    QVERIFY(dist >= 0);
    QCOMPARE(details.toInt(), static_cast<int>(QCPItemHSpan::hpLowerEdge));
}

void TestHSpan::selectTestFill()
{
    auto* span = new QCPItemHSpan(mPlot);
    span->setRange(QCPRange(2, 8));
    mPlot->replot();

    double midPx = mPlot->yAxis->coordToPixel(5);
    double midX = mPlot->xAxis->axisRect()->center().x();

    QVariant details;
    double dist = span->selectTest(QPointF(midX, midPx), false, &details);
    QVERIFY(dist >= 0);
    QCOMPARE(details.toInt(), static_cast<int>(QCPItemHSpan::hpFill));
}

void TestHSpan::selectTestMiss()
{
    auto* span = new QCPItemHSpan(mPlot);
    span->setRange(QCPRange(2, 4));
    mPlot->replot();

    double farPx = mPlot->yAxis->coordToPixel(9);
    double midX = mPlot->xAxis->axisRect()->center().x();

    double dist = span->selectTest(QPointF(midX, farPx), false);
    QCOMPARE(dist, -1.0);
}

void TestHSpan::movableFlag()
{
    auto* span = new QCPItemHSpan(mPlot);
    QVERIFY(span->movable());
    span->setMovable(false);
    QVERIFY(!span->movable());
}

void TestHSpan::drawDoesNotCrash()
{
    auto* span = new QCPItemHSpan(mPlot);
    span->setRange(QCPRange(1, 9));
    mPlot->replot();
    QVERIFY(true);
}

void TestHSpan::drawOnLogAxis()
{
    mPlot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    mPlot->yAxis->setRange(1, 1000);
    auto* span = new QCPItemHSpan(mPlot);
    span->setRange(QCPRange(10, 100));
    mPlot->replot();
    QVERIFY(true);
}

void TestHSpan::exportFallbackRendersSpan()
{
    auto* span = new QCPItemHSpan(mPlot);
    span->setRange(QCPRange(3, 7));
    span->setBrush(QBrush(QColor(0, 255, 0, 128)));
    span->setPen(Qt::NoPen);
    span->setBorderPen(Qt::NoPen);
    mPlot->replot();
    QPixmap pm = mPlot->toPixmap(400, 300);
    QVERIFY(!pm.isNull());
    QImage img = pm.toImage();
    int centerX = img.width() / 2;
    int centerY = static_cast<int>(mPlot->yAxis->coordToPixel(5));
    centerX = qBound(0, centerX, img.width() - 1);
    centerY = qBound(0, centerY, img.height() - 1);
    QColor c = img.pixelColor(centerX, centerY);
    QVERIFY2(c.green() > 50, qPrintable(QString("Expected green > 50, got %1").arg(c.green())));
}

void TestHSpan::dirtyTrackingReplots()
{
    auto* span = new QCPItemHSpan(mPlot);
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

void TestHSpan::edgesHonourTheirPositionType()
{
    // The edges are QCPItemPositions, so a non-plot-coords type must be honoured
    // rather than the coords being force-fed to QCPAxis::coordToPixel().
    mPlot->replot();
    QCPItemHSpan* span = new QCPItemHSpan(mPlot);
    span->lowerEdge->setTypeY(QCPItemPosition::ptAxisRectAbsolute);
    span->upperEdge->setTypeY(QCPItemPosition::ptAxisRectAbsolute);
    span->setRange(QCPRange(100, 200));

    const QRect axRect = mPlot->axisRect()->rect();
    const double midX = axRect.center().x();

    QCOMPARE(span->center->pixelPosition().y(), axRect.top() + 150.0);
    QVERIFY(span->selectTest(QPointF(midX, axRect.top() + 150.0), false) >= 0);
    QVERIFY(span->selectTest(QPointF(midX, axRect.top() + 300.0), false) < 0);
}

namespace {
void removeOnRangeChange(QCustomPlot* plot, QCPItemHSpan* span)
{
    QObject::connect(span, &QCPItemHSpan::rangeChanged, plot,
                     [plot, span] { QVERIFY(plot->removeItem(span)); });
}

void sendMouse(QCustomPlot* plot, QEvent::Type type, QPoint pos, Qt::MouseButtons buttons)
{
    QMouseEvent e(type, pos, plot->mapToGlobal(pos), Qt::LeftButton, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(plot, &e);
}
} // namespace

// A slot deleting the span from its own signal: the span must not touch itself afterwards.
void TestHSpan::setRangeSlotDeletingTheSpanIsSafe()
{
    auto* span = new QCPItemHSpan(mPlot);
    removeOnRangeChange(mPlot, span);
    span->setRange(QCPRange(3, 7));
    QCOMPARE(mPlot->itemCount(), 0);
}

void TestHSpan::dragSlotDeletingTheSpanIsSafe()
{
    auto* span = new QCPItemHSpan(mPlot);
    span->setRange(QCPRange(3, 7));
    mPlot->replot();
    removeOnRangeChange(mPlot, span);
    const int x = mPlot->axisRect()->rect().center().x();
    const QPoint from(x, qRound(mPlot->yAxis->coordToPixel(5)));
    sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseMove, from + QPoint(0, 20), Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseButtonRelease, from + QPoint(0, 20), Qt::NoButton);
    QCOMPARE(mPlot->itemCount(), 0);
}

void TestHSpan::pixelModeDragMovesByPixels()
{
    mPlot->yAxis->setRange(0, 1000);
    mPlot->replot();
    auto* span = new QCPItemHSpan(mPlot);
    span->lowerEdge->setTypeY(QCPItemPosition::ptAxisRectAbsolute);
    span->upperEdge->setTypeY(QCPItemPosition::ptAxisRectAbsolute);
    span->setRange(QCPRange(100, 200));
    const QRect axRect = mPlot->axisRect()->rect();
    const QPoint from(axRect.center().x(), axRect.top() + 150);
    sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseMove, from + QPoint(0, 40), Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseButtonRelease, from + QPoint(0, 40), Qt::NoButton);
    QCOMPARE(span->range().lower, 140.0);
    QCOMPARE(span->range().upper, 240.0);
}

void TestHSpan::dataDragMovesByMouseDelta()
{
    auto* span = new QCPItemHSpan(mPlot);
    span->setRange(QCPRange(3, 7));
    mPlot->replot();
    const auto* axis = mPlot->yAxis;
    const double lower = axis->pixelToCoord(axis->coordToPixel(3) + 40);
    const double upper = axis->pixelToCoord(axis->coordToPixel(7) + 40);
    const QPoint from(mPlot->axisRect()->rect().center().x(), qRound(axis->coordToPixel(5)));
    sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseMove, from + QPoint(0, 40), Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseButtonRelease, from + QPoint(0, 40), Qt::NoButton);
    QCOMPARE(span->range().lower, lower);
    QCOMPARE(span->range().upper, upper);
}

// On a log axis the drag follows the mouse in pixels: the edges move by different data amounts.
void TestHSpan::logAxisDragFollowsTheMouseInPixels()
{
    mPlot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    mPlot->yAxis->setRange(1, 1000);
    mPlot->replot();
    auto* span = new QCPItemHSpan(mPlot);
    span->setRange(QCPRange(10, 100));
    mPlot->replot();
    const auto* axis = mPlot->yAxis;
    const double lower = axis->pixelToCoord(axis->coordToPixel(10) + 40);
    const double upper = axis->pixelToCoord(axis->coordToPixel(100) + 40);
    const QPoint from(mPlot->axisRect()->rect().center().x(), qRound(axis->coordToPixel(30)));
    sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseMove, from + QPoint(0, 40), Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseButtonRelease, from + QPoint(0, 40), Qt::NoButton);
    QCOMPARE(span->range().lower, lower);
    QCOMPARE(span->range().upper, upper);
}

void TestHSpan::mixedEdgesEachMoveInTheirOwnUnit()
{
    auto* span = new QCPItemHSpan(mPlot);
    span->lowerEdge->setTypeY(QCPItemPosition::ptAxisRectAbsolute);
    span->lowerEdge->setCoords(0, 200);
    span->upperEdge->setCoords(1, 7);
    mPlot->replot();
    const auto* axis = mPlot->yAxis;
    const QRect axRect = mPlot->axisRect()->rect();
    const double upper = axis->pixelToCoord(axis->coordToPixel(7) + 40);
    const QPoint from(axRect.center().x(), axRect.top() + 150);
    sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseMove, from + QPoint(0, 40), Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseButtonRelease, from + QPoint(0, 40), Qt::NoButton);
    QCOMPARE(span->lowerEdge->coords().y(), 240.0);
    QCOMPARE(span->upperEdge->coords().y(), upper);
}
