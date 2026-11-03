// First: it defines QT_WIDGETS_LIB, which QTest::touchEvent(QWidget*) needs and the macOS build
// does not set. After <QtTest>, qtesttouch.h would already be read without it.
#include <QtTest/qtest_widgets.h>
#include <QtTest/QtTest>
#include <memory>
#include <QtGui/qtestsupport_gui.h>          // QTest::createTouchDevice
#include <QtWidgets/qtestsupport_widgets.h>  // QTest::qWaitForWindowExposed
#include "qcustomplot.h"

class TestGestures : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void wheelZoomsWithoutGestures();
    void swipePansWithGestures();
    void swipePansALogAxis();
    void ctrlWheelZoomsAtTheCursor();
    void touchTwoFingersTogetherPan();
    void oneFingerDragPans();
    void oneFingerTapIsAClick();
    void tapSlotReadsItsMouseEvent();
    void touchSequencePinchZoomsByTheFingerSpread();
    void touchPinchOneFingerPerEvent();
    void nativePinchZooms();
    void nativePinchIsAccepted();
    void unusedNativePinchIsNotAccepted();
    void gesturesAcceptTouch();
    void gesturesNeedRangeInteractions();
    void dragSlotDeletingTheAxisRectIsSafe();
    void wheelSlotDeletingTheAxisRectIsSafe();
    void swipeSlotDeletingTheAxisRectIsSafe();
    void pinchSlotDeletingTheAxisRectIsSafe();
    void pinchSlotDeletingThePlotIsSafe();
    void nativePinchSlotDeletingThePlotIsSafe();
    void axisDragSlotDeletingTheAxisIsSafe();
    void axisWheelSlotDeletingTheAxisIsSafe();
    void dragSlotDeletingTheAxisRectRestoresAntialiasing();
    void wheelSlotDeletingTheAxisRectRestoresAntialiasing();
    void dragRightAfterAWheelZoomRestoresAntialiasing();
    void wheelZoomDuringADragRestoresAntialiasing();

private:
    QPointF centre() const;
    void wheel(QPoint pixelDelta, QPoint angleDelta, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void wheelAt(QPointF pos);
    void drag(QPoint from, QPoint to);
    template <typename Slot> void onFirstXRangeChange(Slot slot);
    void deleteAxisRectOnRangeChange();
    void deleteAxisRect();
    void expectAntialiasing(QCP::AntialiasedElements on, QCP::AntialiasedElements off);
    void deletePlotOnRangeChange();
    void pinch();

    bool mRangeSlotRan = false;

    QCustomPlot* mPlot;
    std::unique_ptr<QPointingDevice> mTouchScreen{QTest::createTouchDevice()};
};
