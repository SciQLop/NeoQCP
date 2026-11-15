#pragma once
#include <QtTest/QtTest>

class QCustomPlot;

/*!
  Plottables are built without axes: a constructor takes the plottable's own arguments (its data,
  for the built-in types), and QCPAxisRect::add<T>(args...), QCPPolarArea::add<T>(args...) or
  QCustomPlot::add<T>(args...) places the result, binding it to the area's system.
*/
class TestPlottableWithoutAxes : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void aGraphTakesItsDataInTheConstructor();
    void everyBuiltInPlottableTakesItsData();
    void aUserTypeInheritsTheDataConstructor();
    void anAddWithoutBothAxesRefusesThePlottable();
    void aRefusedPlottableIsReportedOnceAndDeleted();
    void aRectAddBindsTheGivenAxisPair();
    void plotAddBindsTheFirstRectsDefaultPair();

private:
    QCustomPlot* mPlot = nullptr;
    void (*mMisuseHandler)(const char*, const char*) = nullptr;
};
