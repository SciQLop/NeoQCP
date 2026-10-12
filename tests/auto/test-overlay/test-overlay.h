#include <QtTest/QtTest>
#include <QtWidgets/qtestsupport_widgets.h>
#include "qcustomplot.h"

class TestOverlay : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void showMessageStoresText();
    void clearMessageHidesOverlay();
    void showMessageEmitsSignal();
    void compactRectIsSingleLine();
    void fitContentRectFitsText();
    void fullWidgetRectCoversWidget();
    void positionTop();
    void positionBottom();
    void positionLeft();
    void positionRight();
    void opacityRoundtrip();
    void showMessageTriggersReplot();
    void collapseToggle();
    void clickPassThroughCompact();
    void clickBlockedFullWidget();
    void overlayStaysTopmost();
    void overlaySurvivesClear();
    void overlayAccessorCreatesLazily();
    void overlayExcludedFromExport();
    void updateWhileVisibleRedrawsLayer_data();
    void updateWhileVisibleRedrawsLayer();

private:
    QCustomPlot* mPlot;
};
