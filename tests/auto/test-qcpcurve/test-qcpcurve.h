#include <QtTest/QtTest>
#include "qcustomplot.h"

class TestQCPCurve : public QObject
{
  Q_OBJECT
private slots:
  void init();
  void cleanup();
  
  void dataManipulation();
  void dataSharing();

  void curveIsOnePolylineCommand();
  void curveRefusesAStepStyle();
  void curveColoursByValue();
  void curveFillIsAPolygon();
  void selectedSegmentUsesTheSelectedPen();
  void clickOnASegmentSelectsTheCurve();
  void nanBreaksTheCurveLine();
  void noSetterIsIgnored_data();
  void noSetterIsIgnored();
  
private:
  QCustomPlot *mPlot;
  QCPCurve *mCurve;
};





