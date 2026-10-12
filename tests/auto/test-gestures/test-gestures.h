#include <QtTest/QtTest>
#include <QtGui/qtestsupport_gui.h>          // QTest::createTouchDevice, not in QtTest on macOS
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
    void touchSequencePinchZoomsByTheFingerSpread();
    void touchPinchOneFingerPerEvent();
    void nativePinchZooms();
    void gesturesAcceptTouch();
    void gesturesNeedRangeInteractions();

private:
    QPointF centre() const;
    void wheel(QPoint pixelDelta, QPoint angleDelta, Qt::KeyboardModifiers modifiers = Qt::NoModifier);

    QCustomPlot* mPlot;
    QPointingDevice* mTouchScreen = QTest::createTouchDevice();
};
