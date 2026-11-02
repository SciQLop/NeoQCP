#include "test-gestures.h"

void TestGestures::init()
{
    mRangeSlotRan = false;
    mPlot = new QCustomPlot(nullptr);
    mPlot->resize(400, 300);
    mPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    mPlot->xAxis->setRange(0, 100);
    mPlot->yAxis->setRange(0, 10);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    mPlot->show();
    QVERIFY(QTest::qWaitForWindowExposed(mPlot));
    mPlot->replot(QCustomPlot::rpImmediateRefresh);   // the layout of the shown plot
}

void TestGestures::cleanup()
{
    delete mPlot;
}

QPointF TestGestures::centre() const
{
    return QRectF(mPlot->axisRect()->rect()).center();
}

void TestGestures::wheel(QPoint pixelDelta, QPoint angleDelta, Qt::KeyboardModifiers modifiers)
{
    QWheelEvent event(centre(), mPlot->mapToGlobal(centre()), pixelDelta, angleDelta, Qt::NoButton,
                      modifiers, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(mPlot, &event);
}

void TestGestures::wheelZoomsWithoutGestures()
{
    wheel({0, 40}, {0, 120});
    QVERIFY(mPlot->xAxis->range().size() < 100);
}

// A two-finger swipe moves the data with the fingers, like a drag: right and down here.
void TestGestures::swipePansWithGestures()
{
    mPlot->setInteraction(QCP::iGestures);
    const double pxPerX = mPlot->axisRect()->width() / 100.0;
    wheel({40, 0}, {40, 0});
    QCOMPARE(mPlot->xAxis->range().size(), 100.0);
    QVERIFY2(qAbs(mPlot->xAxis->range().lower + 40 / pxPerX) < 1e-6,
             qPrintable(QString::number(mPlot->xAxis->range().lower)));
    QCOMPARE(mPlot->yAxis->range().lower, 0.0);

    wheel({0, 30}, {0, 30});
    QVERIFY(mPlot->yAxis->range().lower > 0);
    QVERIFY(qAbs(mPlot->yAxis->range().size() - 10) < 1e-9);
}

void TestGestures::swipePansALogAxis()
{
    mPlot->setInteraction(QCP::iGestures);
    mPlot->xAxis->setScaleType(QCPAxis::stLogarithmic);
    mPlot->xAxis->setRange(1, 1000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    const double expectedLower = mPlot->xAxis->pixelToCoord(mPlot->xAxis->coordToPixel(1) - 40);
    wheel({40, 0}, {40, 0});
    const QCPRange r = mPlot->xAxis->range();
    QVERIFY2(qAbs(r.lower / expectedLower - 1) < 1e-9, qPrintable(QString::number(r.lower)));
    QVERIFY(qAbs(r.upper / r.lower - 1000) < 1e-6);
}

// Browsers send a trackpad pinch as a wheel event with Ctrl held.
void TestGestures::ctrlWheelZoomsAtTheCursor()
{
    mPlot->setInteraction(QCP::iGestures);
    const double atCursor = mPlot->xAxis->pixelToCoord(centre().x());
    wheel({0, 40}, {0, 120}, Qt::ControlModifier);
    QVERIFY(mPlot->xAxis->range().size() < 100);
    QVERIFY(qAbs(mPlot->xAxis->pixelToCoord(centre().x()) - atCursor) < 1e-6);
}

void TestGestures::touchTwoFingersTogetherPan()
{
    mPlot->setInteraction(QCP::iGestures);
    const double pxPerX = mPlot->axisRect()->width() / 100.0;
    const QPoint c = centre().toPoint(), a = c - QPoint(30, 0), b = c + QPoint(30, 0);
    QTest::touchEvent(mPlot, mTouchScreen.get()).press(0, a).press(1, b);
    for (int step = 1; step <= 4; ++step)
        QTest::touchEvent(mPlot, mTouchScreen.get()).move(0, a + QPoint(5 * step, 0)).move(1, b + QPoint(5 * step, 0));
    QTest::touchEvent(mPlot, mTouchScreen.get()).release(0, a + QPoint(20, 0)).release(1, b + QPoint(20, 0));
    QVERIFY(qAbs(mPlot->xAxis->range().size() - 100) < 1e-6);
    QVERIFY2(qAbs(mPlot->xAxis->range().lower + 20 / pxPerX) < 1e-6,
             qPrintable(QString::number(mPlot->xAxis->range().lower)));
}

void TestGestures::oneFingerDragPans()
{
    mPlot->setInteraction(QCP::iGestures);
    const QPoint c = centre().toPoint();
    QTest::touchEvent(mPlot, mTouchScreen.get()).press(0, c);
    for (int step = 1; step <= 4; ++step)
        QTest::touchEvent(mPlot, mTouchScreen.get()).move(0, c + QPoint(10 * step, 0));
    QTest::touchEvent(mPlot, mTouchScreen.get()).release(0, c + QPoint(40, 0));
    QVERIFY(mPlot->xAxis->range().lower < -5);
    QVERIFY(qAbs(mPlot->xAxis->range().size() - 100) < 1e-6);
}

void TestGestures::oneFingerTapIsAClick()
{
    mPlot->setInteraction(QCP::iGestures);
    QSignalSpy clicks(mPlot, &QCustomPlot::mouseRelease);
    const QPoint c = centre().toPoint();
    QTest::touchEvent(mPlot, mTouchScreen.get()).press(0, c);
    QTest::touchEvent(mPlot, mTouchScreen.get()).release(0, c);
    QCoreApplication::processEvents(); // input signals arrive on the next event-loop turn
    QCOMPARE(clicks.count(), 1);
    QCOMPARE(mPlot->xAxis->range(), QCPRange(0, 100));
}

// Spreading two fingers from 60 to 180 px apart zooms in 3x, whatever the number of touch events.
void TestGestures::touchSequencePinchZoomsByTheFingerSpread()
{
    mPlot->setInteraction(QCP::iGestures);
    const QPoint c = centre().toPoint();
    QTest::touchEvent(mPlot, mTouchScreen.get()).press(0, c - QPoint(30, 0)).press(1, c + QPoint(30, 0));
    for (int step = 1; step <= 10; ++step)
        QTest::touchEvent(mPlot, mTouchScreen.get()).move(0, c - QPoint(30 + 6 * step, 0)).move(1, c + QPoint(30 + 6 * step, 0));
    QTest::touchEvent(mPlot, mTouchScreen.get()).release(0, c - QPoint(90, 0)).release(1, c + QPoint(90, 0));
    const double size = mPlot->xAxis->range().size();
    QVERIFY2(qAbs(size - 100.0 / 3) < 1e-6, qPrintable(QString::number(size)));
}

// A browser moves each finger in its own event.
void TestGestures::touchPinchOneFingerPerEvent()
{
    mPlot->setInteraction(QCP::iGestures);
    const QPoint c = centre().toPoint();
    QPoint a = c - QPoint(30, 0), b = c + QPoint(30, 0);
    QTest::touchEvent(mPlot, mTouchScreen.get()).press(0, a);
    QTest::touchEvent(mPlot, mTouchScreen.get()).stationary(0).press(1, b);
    for (int step = 0; step < 10; ++step)
    {
        a -= QPoint(6, 0);
        QTest::touchEvent(mPlot, mTouchScreen.get()).move(0, a).stationary(1);
        b += QPoint(6, 0);
        QTest::touchEvent(mPlot, mTouchScreen.get()).stationary(0).move(1, b);
    }
    QTest::touchEvent(mPlot, mTouchScreen.get()).release(0, a).release(1, b);
    const double size = mPlot->xAxis->range().size();
    QVERIFY2(qAbs(size - 100.0 / 3) < 1e-6, qPrintable(QString::number(size)));
}

void TestGestures::nativePinchZooms()
{
    mPlot->setInteraction(QCP::iGestures);
    QNativeGestureEvent event(Qt::ZoomNativeGesture, QPointingDevice::primaryPointingDevice(), 2,
                              centre(), centre(), mPlot->mapToGlobal(centre()), 1.0, {});
    QCoreApplication::sendEvent(mPlot, &event);
    QVERIFY2(qAbs(mPlot->xAxis->range().size() - 50) < 1e-6,
             qPrintable(QString::number(mPlot->xAxis->range().size())));
}

// Before: QApplication ignores it first; left ignored, it went on to the parent through the plot,
// which a rangeChanged slot may have deleted.
void TestGestures::nativePinchIsAccepted()
{
    mPlot->setInteraction(QCP::iGestures);
    QNativeGestureEvent event(Qt::ZoomNativeGesture, QPointingDevice::primaryPointingDevice(), 2,
                              centre(), centre(), mPlot->mapToGlobal(centre()), 1.0, {});
    QCoreApplication::sendEvent(mPlot, &event);
    QVERIFY(event.isAccepted());
}

// A pinch the plot does not zoom on stays ignored, so the parent widget still gets it.
void TestGestures::unusedNativePinchIsNotAccepted()
{
    mPlot->setInteractions(QCP::iGestures);
    QNativeGestureEvent event(Qt::ZoomNativeGesture, QPointingDevice::primaryPointingDevice(), 2,
                              centre(), centre(), mPlot->mapToGlobal(centre()), 1.0, {});
    event.ignore();
    QCoreApplication::sendEvent(mPlot, &event);
    QVERIFY(!event.isAccepted());
}

void TestGestures::gesturesAcceptTouch()
{
    QVERIFY(!mPlot->testAttribute(Qt::WA_AcceptTouchEvents));
    mPlot->setInteraction(QCP::iGestures);
    QVERIFY(mPlot->testAttribute(Qt::WA_AcceptTouchEvents));
    mPlot->setInteraction(QCP::iGestures, false);
    QVERIFY(!mPlot->testAttribute(Qt::WA_AcceptTouchEvents));
}

// Gestures move what iRangeDrag and iRangeZoom allow, no more.
void TestGestures::gesturesNeedRangeInteractions()
{
    mPlot->setInteractions(QCP::iGestures);
    wheel({40, 0}, {40, 0});
    wheel({0, 40}, {0, 120}, Qt::ControlModifier);
    QCOMPARE(mPlot->xAxis->range(), QCPRange(0, 100));
}

void TestGestures::wheelAt(QPointF pos)
{
    QWheelEvent event(pos, mPlot->mapToGlobal(pos), QPoint(), QPoint(0, 120), Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(mPlot, &event);
}

void TestGestures::drag(QPoint from, QPoint to)
{
    auto send = [this](QEvent::Type type, QPoint pos, Qt::MouseButtons buttons) {
        QMouseEvent e(type, pos, mPlot->mapToGlobal(pos), Qt::LeftButton, buttons, Qt::NoModifier);
        QCoreApplication::sendEvent(mPlot, &e);
    };
    send(QEvent::MouseButtonPress, from, Qt::LeftButton);
    send(QEvent::MouseMove, (from + to) / 2, Qt::LeftButton);
    send(QEvent::MouseMove, to, Qt::LeftButton);
    send(QEvent::MouseButtonRelease, to, Qt::NoButton);
}

template <typename Slot>
void TestGestures::onFirstXRangeChange(Slot slot)
{
    connect(mPlot->xAxis, qOverload<const QCPRange&>(&QCPAxis::rangeChanged), this,
            [this, slot] {
                mRangeSlotRan = true;
                slot();
            },
            Qt::SingleShotConnection);
}

// The drag, wheel and pinch handlers loop over the axes and use the plot after each range change.
void TestGestures::deleteAxisRectOnRangeChange()
{
    onFirstXRangeChange([this] { deleteAxisRect(); });
}

void TestGestures::deleteAxisRect()
{
    const QPointer<QCPAxisRect> rect = mPlot->axisRect();
    QVERIFY(mPlot->plotLayout()->remove(rect)); // remove() deletes the element
    QVERIFY(!rect);
}

void TestGestures::deletePlotOnRangeChange()
{
    onFirstXRangeChange([this] {
        delete mPlot;
        mPlot = nullptr;
    });
}

void TestGestures::pinch()
{
    const QPoint c = centre().toPoint();
    QTest::touchEvent(mPlot, mTouchScreen.get()).press(0, c - QPoint(30, 0)).press(1, c + QPoint(30, 0));
    QTest::touchEvent(mPlot, mTouchScreen.get()).move(0, c - QPoint(40, 0)).move(1, c + QPoint(50, 0));
    if (mPlot)
        QTest::touchEvent(mPlot, mTouchScreen.get()).release(0, c - QPoint(40, 0)).release(1, c + QPoint(50, 0));
}

void TestGestures::dragSlotDeletingTheAxisRectIsSafe()
{
    deleteAxisRectOnRangeChange();
    const QPoint c = centre().toPoint();
    drag(c, c + QPoint(40, 20));
    QVERIFY(mRangeSlotRan);
    QCOMPARE(mPlot->axisRectCount(), 0);
}

void TestGestures::wheelSlotDeletingTheAxisRectIsSafe()
{
    deleteAxisRectOnRangeChange();
    wheel({0, 40}, {0, 120});
    QVERIFY(mRangeSlotRan);
    QCOMPARE(mPlot->axisRectCount(), 0);
}

void TestGestures::swipeSlotDeletingTheAxisRectIsSafe()
{
    mPlot->setInteraction(QCP::iGestures);
    deleteAxisRectOnRangeChange();
    wheel({40, 30}, {40, 30});
    QVERIFY(mRangeSlotRan);
    QCOMPARE(mPlot->axisRectCount(), 0);
}

void TestGestures::pinchSlotDeletingTheAxisRectIsSafe()
{
    mPlot->setInteraction(QCP::iGestures);
    deleteAxisRectOnRangeChange();
    pinch();
    QVERIFY(mRangeSlotRan);
    QCOMPARE(mPlot->axisRectCount(), 0);
}

void TestGestures::pinchSlotDeletingThePlotIsSafe()
{
    mPlot->setInteraction(QCP::iGestures);
    deletePlotOnRangeChange();
    pinch();
    QVERIFY(mRangeSlotRan);
}

void TestGestures::nativePinchSlotDeletingThePlotIsSafe()
{
    mPlot->setInteraction(QCP::iGestures);
    deletePlotOnRangeChange();
    QNativeGestureEvent event(Qt::ZoomNativeGesture, QPointingDevice::primaryPointingDevice(), 2,
                              centre(), centre(), mPlot->mapToGlobal(centre()), 1.0, {});
    QCoreApplication::sendEvent(mPlot, &event);
    QVERIFY(mRangeSlotRan);
}

// Dragging or wheeling on the axis itself, not inside the axis rect.
void TestGestures::axisDragSlotDeletingTheAxisIsSafe()
{
    onFirstXRangeChange([this] { QVERIFY(mPlot->axisRect()->removeAxis(mPlot->xAxis)); });
    const QPoint onAxis(int(centre().x()), mPlot->axisRect()->bottom() + 8);
    drag(onAxis, onAxis + QPoint(40, 0));
    QVERIFY(mRangeSlotRan);
    QCOMPARE(mPlot->axisRect()->axisCount(QCPAxis::atBottom), 0);
}

void TestGestures::axisWheelSlotDeletingTheAxisIsSafe()
{
    onFirstXRangeChange([this] { QVERIFY(mPlot->axisRect()->removeAxis(mPlot->xAxis)); });
    wheelAt(QPointF(centre().x(), mPlot->axisRect()->bottom() + 8));
    QVERIFY(mRangeSlotRan);
    QCOMPARE(mPlot->axisRect()->axisCount(QCPAxis::atBottom), 0);
}

void TestGestures::expectAntialiasing(QCP::AntialiasedElements on, QCP::AntialiasedElements off)
{
    QCOMPARE(mPlot->antialiasedElements().toInt(), on.toInt());
    QCOMPARE(mPlot->notAntialiasedElements().toInt(), off.toInt());
}

// Before: only the rect's release handler restored it, so the plot stayed non-antialiased.
void TestGestures::dragSlotDeletingTheAxisRectRestoresAntialiasing()
{
    mPlot->setAntialiasedElements(QCP::aePlottables);
    const auto on = mPlot->antialiasedElements(), off = mPlot->notAntialiasedElements();
    int changes = 0; // the first move turns antialiasing off, the second deletes the rect
    connect(mPlot->xAxis, qOverload<const QCPRange&>(&QCPAxis::rangeChanged), this, [this, &changes] {
        if (++changes != 2)
            return;
        mRangeSlotRan = true;
        deleteAxisRect();
    });
    const QPoint c = centre().toPoint();
    drag(c, c + QPoint(40, 20));
    QVERIFY(mRangeSlotRan);
    expectAntialiasing(on, off);
}

// Before: a press inside the wheel's restore window backed up the wheel's forced state, and the
// release after the timer put it back for good.
void TestGestures::dragRightAfterAWheelZoomRestoresAntialiasing()
{
    mPlot->setAntialiasedElements(QCP::aePlottables);
    const auto on = mPlot->antialiasedElements(), off = mPlot->notAntialiasedElements();
    wheel({0, 40}, {0, 120});
    const QPoint c = centre().toPoint();
    auto send = [this](QEvent::Type type, QPoint pos, Qt::MouseButtons buttons) {
        QMouseEvent e(type, pos, mPlot->mapToGlobal(pos), Qt::LeftButton, buttons, Qt::NoModifier);
        QCoreApplication::sendEvent(mPlot, &e);
    };
    send(QEvent::MouseButtonPress, c, Qt::LeftButton);
    QTest::qWait(400); // the wheel's restore timer fires mid-drag
    send(QEvent::MouseMove, c + QPoint(40, 20), Qt::LeftButton);
    send(QEvent::MouseButtonRelease, c + QPoint(40, 20), Qt::NoButton);
    expectAntialiasing(on, off);
}

// Before: a wheel zoom during a drag backed up the drag's forced state, and its timer, firing after
// the release, put that back for good.
void TestGestures::wheelZoomDuringADragRestoresAntialiasing()
{
    mPlot->setAntialiasedElements(QCP::aePlottables);
    const auto on = mPlot->antialiasedElements(), off = mPlot->notAntialiasedElements();
    const QPoint c = centre().toPoint();
    auto send = [this](QEvent::Type type, QPoint pos, Qt::MouseButtons buttons) {
        QMouseEvent e(type, pos, mPlot->mapToGlobal(pos), Qt::LeftButton, buttons, Qt::NoModifier);
        QCoreApplication::sendEvent(mPlot, &e);
    };
    send(QEvent::MouseButtonPress, c, Qt::LeftButton);
    send(QEvent::MouseMove, c + QPoint(40, 20), Qt::LeftButton);
    wheel({0, 40}, {0, 120});
    send(QEvent::MouseButtonRelease, c + QPoint(40, 20), Qt::NoButton);
    QTest::qWait(400); // the wheel's restore timer fires after the release
    expectAntialiasing(on, off);
}

// Before: the restore timer died with the rect.
void TestGestures::wheelSlotDeletingTheAxisRectRestoresAntialiasing()
{
    mPlot->setAntialiasedElements(QCP::aePlottables);
    const auto on = mPlot->antialiasedElements(), off = mPlot->notAntialiasedElements();
    deleteAxisRectOnRangeChange();
    wheel({0, 40}, {0, 120});
    QVERIFY(mRangeSlotRan);
    expectAntialiasing(on, off);
}
