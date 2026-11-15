#pragma once
#include <QtTest/QtTest>

class QCustomPlot;

/*!
  Items are built without a plot: a constructor creates its positions and anchors, a user item may
  bind them in a member initialiser, and QCPAxisRect::add<T>(args...) or QCustomPlot::add<T>(args...)
  places the result and binds it to the area it lands in.
*/
class TestItemWithoutPlot : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void aUserItemIsBuiltWithoutAPlot();
    void aRectAddPlacesAnItemBuiltFromItsArguments();
    void everyBuiltInItemIsBuiltWithoutAPlot();
    void plotAddStillUsesTheFirstRect();
    void aRectAddBindsTheRectsAxes();
    void aSystemGivenBeforeTheAddIsKept();
    void aClipFlagGivenBeforeTheAddIsKept();
    void aRectAddWithoutBothAxesRefusesTheItem();
    void aSpanOrientationReachesTheRectAdd();

private:
    QCustomPlot* mPlot = nullptr;
    void (*mMisuseHandler)(const char*, const char*) = nullptr;
};
