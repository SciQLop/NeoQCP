#include "test-rspan.h"
#include "../../../src/qcp.h"
#include "../misuse-recorder.h"

void TestRSpan::init()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
    mPlot->xAxis->setRange(0, 10);
    mPlot->yAxis->setRange(0, 10);
    mPlot->replot();
}

void TestRSpan::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
}

void TestRSpan::createAndRanges()
{
    auto span = mPlot->add<QCPItemRSpan>();
    span->setKeyRange(QCPRange(2, 8));
    span->setValueRange(QCPRange(3, 7));

    QCOMPARE(span->keyRange().lower, 2.0);
    QCOMPARE(span->keyRange().upper, 8.0);
    QCOMPARE(span->valueRange().lower, 3.0);
    QCOMPARE(span->valueRange().upper, 7.0);
}

void TestRSpan::selectTestEdges()
{
    auto span = mPlot->add<QCPItemRSpan>();
    span->setKeyRange(QCPRange(2, 8));
    span->setValueRange(QCPRange(2, 8));
    mPlot->replot();

    const double midY = (mPlot->yAxis->coordToPixel(2) + mPlot->yAxis->coordToPixel(8)) * 0.5;
    const double midX = (mPlot->xAxis->coordToPixel(2) + mPlot->xAxis->coordToPixel(8)) * 0.5;

    // left edge
    {
        QVariant details;
        double dist = span->selectTest(
            QPointF(mPlot->xAxis->coordToPixel(2), midY), false, &details);
        QVERIFY(dist >= 0);
        QCOMPARE(details.toInt(), static_cast<int>(QCPItemRSpan::hpLeft));
    }

    // right edge
    {
        QVariant details;
        double dist = span->selectTest(
            QPointF(mPlot->xAxis->coordToPixel(8), midY), false, &details);
        QVERIFY(dist >= 0);
        QCOMPARE(details.toInt(), static_cast<int>(QCPItemRSpan::hpRight));
    }

    // top edge
    {
        QVariant details;
        double dist = span->selectTest(
            QPointF(midX, mPlot->yAxis->coordToPixel(8)), false, &details);
        QVERIFY(dist >= 0);
        QCOMPARE(details.toInt(), static_cast<int>(QCPItemRSpan::hpTop));
    }

    // bottom edge
    {
        QVariant details;
        double dist = span->selectTest(
            QPointF(midX, mPlot->yAxis->coordToPixel(2)), false, &details);
        QVERIFY(dist >= 0);
        QCOMPARE(details.toInt(), static_cast<int>(QCPItemRSpan::hpBottom));
    }
}

void TestRSpan::selectTestFill()
{
    auto span = mPlot->add<QCPItemRSpan>();
    span->setKeyRange(QCPRange(2, 8));
    span->setValueRange(QCPRange(2, 8));
    mPlot->replot();

    const double midX = mPlot->xAxis->coordToPixel(5);
    const double midY = mPlot->yAxis->coordToPixel(5);

    QVariant details;
    double dist = span->selectTest(QPointF(midX, midY), false, &details);
    QVERIFY(dist >= 0);
    QCOMPARE(details.toInt(), static_cast<int>(QCPItemRSpan::hpFill));
}

void TestRSpan::drawDoesNotCrash()
{
    auto span = mPlot->add<QCPItemRSpan>();
    span->setKeyRange(QCPRange(1, 9));
    span->setValueRange(QCPRange(1, 9));
    mPlot->replot();
    QVERIFY(true);
}

void TestRSpan::exportFallbackRendersSpan()
{
    auto span = mPlot->add<QCPItemRSpan>();
    span->setKeyRange(QCPRange(3, 7));
    span->setValueRange(QCPRange(3, 7));
    span->setBrush(QBrush(QColor(0, 0, 255, 128)));
    span->setBorderPen(Qt::NoPen);
    mPlot->replot();
    QPixmap pm = mPlot->toPixmap(400, 300);
    QVERIFY(!pm.isNull());
    QImage img = pm.toImage();
    int centerX = static_cast<int>(mPlot->xAxis->coordToPixel(5));
    int centerY = static_cast<int>(mPlot->yAxis->coordToPixel(5));
    centerX = qBound(0, centerX, img.width() - 1);
    centerY = qBound(0, centerY, img.height() - 1);
    QColor c = img.pixelColor(centerX, centerY);
    QVERIFY2(c.blue() > 50, qPrintable(QString("Expected blue > 50, got %1").arg(c.blue())));
}

void TestRSpan::dirtyTrackingReplots()
{
    auto span = mPlot->add<QCPItemRSpan>();
    span->setKeyRange(QCPRange(2, 8));
    span->setValueRange(QCPRange(2, 8));
    mPlot->replot();

    span->setBrush(QBrush(Qt::blue));
    mPlot->replot();
    mPlot->replot();
    span->setBorderPen(QPen(Qt::red, 3));
    mPlot->replot();
    span->setKeyRange(QCPRange(1, 9));
    mPlot->replot();
    span->setValueRange(QCPRange(1, 9));
    mPlot->replot();
    QVERIFY(true);
}

void TestRSpan::drawOnLogAxis()
{
    mPlot->xAxis->setScale(QCPScale::log());
    mPlot->xAxis->setRange(1, 1000);
    mPlot->yAxis->setScale(QCPScale::log());
    mPlot->yAxis->setRange(1, 1000);
    auto span = mPlot->add<QCPItemRSpan>();
    span->setKeyRange(QCPRange(10, 100));
    span->setValueRange(QCPRange(10, 100));
    mPlot->replot();
    QVERIFY(true);
}

void TestRSpan::edgesHonourTheirPositionType()
{
    // The edges are QCPItemPositions, so a non-plot-coords type must be honoured
    // rather than the coords being force-fed to QCPAxis::coordToPixel().
    mPlot->replot();
    QCPItemRSpan* span = mPlot->add<QCPItemRSpan>();
    span->leftEdge->setTypeX(QCPItemPosition::ptAxisRectAbsolute);
    span->rightEdge->setTypeX(QCPItemPosition::ptAxisRectAbsolute);
    span->topEdge->setTypeY(QCPItemPosition::ptAxisRectAbsolute);
    span->bottomEdge->setTypeY(QCPItemPosition::ptAxisRectAbsolute);
    span->setKeyRange(QCPRange(100, 200));
    span->setValueRange(QCPRange(50, 150));

    const QRect axRect = mPlot->axisRect()->rect();

    QCOMPARE(span->center->pixelPosition().x(), axRect.left() + 150.0);
    QCOMPARE(span->center->pixelPosition().y(), axRect.top() + 100.0);
    QVERIFY(span->selectTest(QPointF(axRect.left() + 150.0, axRect.top() + 100.0), false) >= 0);
    QVERIFY(span->selectTest(QPointF(axRect.left() + 400.0, axRect.top() + 100.0), false) < 0);
}

namespace {
void sendMouse(QCustomPlot* plot, QEvent::Type type, QPoint pos, Qt::MouseButtons buttons)
{
    QMouseEvent e(type, pos, plot->mapToGlobal(pos), Qt::LeftButton, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(plot, &e);
}
} // namespace

// A fill drag emits keyRangeChanged then valueRangeChanged: a slot deleting the span on the first
// must stop the second.
void TestRSpan::fillDragSlotDeletingTheSpanIsSafe()
{
    auto span = mPlot->add<QCPItemRSpan>();
    span->setKeyRange(QCPRange(3, 7));
    span->setValueRange(QCPRange(3, 7));
    mPlot->replot();
    QSignalSpy valueChanges(span, &QCPItemRSpan::valueRangeChanged);
    connect(span, &QCPItemRSpan::keyRangeChanged, mPlot,
            [this, span] { QVERIFY(mPlot->removeItem(span)); });
    const QPoint from(qRound(mPlot->xAxis->coordToPixel(5)), qRound(mPlot->yAxis->coordToPixel(5)));
    sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseMove, from + QPoint(20, 20), Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseButtonRelease, from + QPoint(20, 20), Qt::NoButton);
    QCOMPARE(mPlot->itemCount(), 0);
    QCOMPARE(valueChanges.count(), 0);
}

void TestRSpan::pixelModeDragMovesByPixels()
{
    mPlot->xAxis->setRange(0, 1000);
    mPlot->yAxis->setRange(0, 1000);
    mPlot->replot();
    auto span = mPlot->add<QCPItemRSpan>();
    for (auto* edge : {span->leftEdge, span->rightEdge, span->topEdge, span->bottomEdge})
    {
        edge->setTypeX(QCPItemPosition::ptAxisRectAbsolute);
        edge->setTypeY(QCPItemPosition::ptAxisRectAbsolute);
    }
    span->setKeyRange(QCPRange(100, 200));
    span->setValueRange(QCPRange(50, 100));
    const QRect axRect = mPlot->axisRect()->rect();
    const QPoint from(axRect.left() + 150, axRect.top() + 75);
    sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseMove, from + QPoint(40, 30), Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseButtonRelease, from + QPoint(40, 30), Qt::NoButton);
    QCOMPARE(span->keyRange().lower, 140.0);
    QCOMPARE(span->keyRange().upper, 240.0);
    QCOMPARE(span->valueRange().lower, 80.0);
    QCOMPARE(span->valueRange().upper, 130.0);
}

void TestRSpan::dataDragMovesByMouseDelta()
{
    auto span = mPlot->add<QCPItemRSpan>();
    span->setKeyRange(QCPRange(3, 7));
    span->setValueRange(QCPRange(2, 6));
    mPlot->replot();
    const QCPAxis* x = mPlot->xAxis;
    const QCPAxis* y = mPlot->yAxis;
    const QPoint from(qRound(x->coordToPixel(5)), qRound(y->coordToPixel(4)));
    sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseMove, from + QPoint(40, 30), Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseButtonRelease, from + QPoint(40, 30), Qt::NoButton);
    QCOMPARE(span->keyRange().lower, x->pixelToCoord(x->coordToPixel(3) + 40));
    QCOMPARE(span->keyRange().upper, x->pixelToCoord(x->coordToPixel(7) + 40));
    QCOMPARE(span->valueRange().lower, y->pixelToCoord(y->coordToPixel(2) + 30));
    QCOMPARE(span->valueRange().upper, y->pixelToCoord(y->coordToPixel(6) + 30));
}

// A drag is measured from the press, not from the previous move, for every edge and the body.
void TestRSpan::severalMovesKeepTheWholeDrag()
{
    mPlot->xAxis->setRange(0, 1000);
    mPlot->yAxis->setRange(0, 1000);
    mPlot->replot();
    const QRect axRect = mPlot->axisRect()->rect();
    const struct
    {
        const char* name;
        QPoint from;
        bool movesLeft, movesRight, movesTop, movesBottom;
    } parts[] {
        { "left edge", { 100, 75 }, true, false, false, false },
        { "right edge", { 200, 75 }, false, true, false, false },
        { "top edge", { 150, 50 }, false, false, true, false },
        { "bottom edge", { 150, 100 }, false, false, false, true },
        { "body", { 150, 75 }, true, true, true, true },
    };
    for (const auto& row : parts)
    {
        auto span = mPlot->add<QCPItemRSpan>();
        for (auto* edge : { span->leftEdge, span->rightEdge, span->topEdge, span->bottomEdge })
        {
            edge->setTypeX(QCPItemPosition::ptAxisRectAbsolute);
            edge->setTypeY(QCPItemPosition::ptAxisRectAbsolute);
        }
        span->setKeyRange(QCPRange(100, 200));
        span->setValueRange(QCPRange(50, 100));
        const QPoint from = axRect.topLeft() + row.from;
        sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);
        for (const QPoint step : { QPoint(10, 5), QPoint(25, 12), QPoint(55, 30) })
            sendMouse(mPlot, QEvent::MouseMove, from + step, Qt::LeftButton);
        sendMouse(mPlot, QEvent::MouseButtonRelease, from + QPoint(55, 30), Qt::NoButton);

        QVERIFY2(span->keyRange().lower == (row.movesLeft ? 155.0 : 100.0),
                 qPrintable(QStringLiteral("%1: the left edge follows the whole drag")
                                .arg(QLatin1String(row.name))));
        QVERIFY2(span->keyRange().upper == (row.movesRight ? 255.0 : 200.0),
                 qPrintable(QStringLiteral("%1: the right edge follows the whole drag")
                                .arg(QLatin1String(row.name))));
        QVERIFY2(span->valueRange().lower == (row.movesTop ? 80.0 : 50.0),
                 qPrintable(QStringLiteral("%1: the top edge follows the whole drag")
                                .arg(QLatin1String(row.name))));
        QVERIFY2(span->valueRange().upper == (row.movesBottom ? 130.0 : 100.0),
                 qPrintable(QStringLiteral("%1: the bottom edge follows the whole drag")
                                .arg(QLatin1String(row.name))));
        QVERIFY(mPlot->removeItem(span));
    }
}

void TestRSpan::logAxisDragFollowsTheMouseInPixels()
{
    mPlot->xAxis->setScale(QCPScale::log());
    mPlot->xAxis->setRange(1, 1000);
    mPlot->replot();
    auto span = mPlot->add<QCPItemRSpan>();
    span->setKeyRange(QCPRange(10, 100));
    span->setValueRange(QCPRange(2, 6));
    mPlot->replot();
    const QCPAxis* x = mPlot->xAxis;
    const QCPAxis* y = mPlot->yAxis;
    const QPoint from(qRound(x->coordToPixel(30)), qRound(y->coordToPixel(4)));
    sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseMove, from + QPoint(40, 0), Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseButtonRelease, from + QPoint(40, 0), Qt::NoButton);
    QCOMPARE(span->keyRange().lower, x->pixelToCoord(x->coordToPixel(10) + 40));
    QCOMPARE(span->keyRange().upper, x->pixelToCoord(x->coordToPixel(100) + 40));
}

void TestRSpan::mixedEdgesEachMoveInTheirOwnUnit()
{
    auto span = mPlot->add<QCPItemRSpan>();
    span->leftEdge->setTypeX(QCPItemPosition::ptAxisRectAbsolute);
    span->leftEdge->setCoords(100, span->leftEdge->coords().y());
    span->rightEdge->setCoords(7, span->rightEdge->coords().y());
    span->setValueRange(QCPRange(2, 6));
    mPlot->replot();
    const QCPAxis* x = mPlot->xAxis;
    const QCPAxis* y = mPlot->yAxis;
    const QRect axRect = mPlot->axisRect()->rect();
    const QPoint from(axRect.left() + 150, qRound(y->coordToPixel(4)));
    sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseMove, from + QPoint(40, 0), Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseButtonRelease, from + QPoint(40, 0), Qt::NoButton);
    QCOMPARE(span->leftEdge->coords().x(), 140.0);
    QCOMPARE(span->rightEdge->coords().x(), x->pixelToCoord(x->coordToPixel(7) + 40));
}

void TestRSpan::anEdgeWithAPixelOffsetIsRefused()
{
    // One rule for both spans: an edge is a range on one axis and the whole axis rect on the other,
    // which no pixel nudge can shift. Refused, once, and the edge keeps what it was given.
    auto span = mPlot->add<QCPItemRSpan>();
    span->setKeyRange(QCPRange(2, 8));
    span->setValueRange(QCPRange(2, 8));
    span->leftEdge->set(QCP::Data { 3, 3 } + QCP::PixelOffset { 12, 0 });

    test_misuse::calls.clear();
    qcp::setMisuseHandler(test_misuse::record);
    mPlot->replot();
    qcp::setMisuseHandler(nullptr);

    QCOMPARE(test_misuse::calls.size(), size_t(1));
    QVERIFY2(std::holds_alternative<QCP::DataOffset>(span->leftEdge->asPosition()),
             "the edge keeps the position it was given");
}
