#include <QtTest/QtTest>
#include "qcustomplot.h"

class TestTheme : public QObject
{
    Q_OBJECT
private slots:
    void lightFactoryDefaults();
    void darkFactoryColors();
    void changedSignalEmitted();
    void init();
    void cleanup();
    void applyThemePropagatesBackground();
    void applyThemePropagatesAxisColors();
    void applyThemePropagatesGridColors();
    void applyThemePropagatesLegendColors();
    void applyThemePropagatesTextElementColor();
    void applyThemePreservesPenStyle();
    void perElementOverrideAfterTheme();
    void themeSharing();
    void setThemeNull_revertsToOwned();
    void qssProxyProperties();
    void coalescing();
    void applyThemePropagatesColorScaleAxis();
    void applyThemeReachesTheRectsOwnScale();
    void busyIndicatorDefaults();
    void busyIndicatorLightFactory();

private:
    QCustomPlot* mPlot;
};
