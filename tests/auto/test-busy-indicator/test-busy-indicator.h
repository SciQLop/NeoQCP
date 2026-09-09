#pragma once
#include <QtTest/QtTest>

class QCustomPlot;

class TestBusyIndicator : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void externalBusyDefault();
    void setBusyEmitsBusyChanged();
    void debounceShowDelay();
    void debounceHideDelay();
    void fastToggleNoVisualChange();
    void perPlottableOverridesTheme();
    void resetFallsBackToTheme();
    void pipelineBusyContributesToEffective();
    void busyPlottableDrawsFaded();
    void notBusyPlottableDrawsFullOpacity();
    void legendShowsPrefixWhenBusy();
    void legendSizeHintAccountsForPrefix();
    void groupLegendShowsBusyPrefix();
    void fullLifecycleExternalBusy();
    void visualBusyToggleForcesLayerRepaint();
    void gpuEntriesCarryFadeAlpha();

private:
    QCustomPlot* mPlot = nullptr;
};
