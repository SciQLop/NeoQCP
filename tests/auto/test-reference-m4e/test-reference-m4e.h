#ifndef TEST_REFERENCE_M4E_H
#define TEST_REFERENCE_M4E_H

#include <QtTest/QtTest>

//! The images milestone 4e's ports must reproduce. Taken on the code before the ports.
class TestReferenceM4e : public QObject
{
    Q_OBJECT

private slots:
    void errorBarsPlain();
    void errorBarsVerticalKey();
    void errorBarsLog();
    void errorBarsSelected();
    void errorBarsLegend();

    void financialCandle();
    void financialOhlc();
    void financialVerticalKey();
    void financialLog();
    void financialSelected();
    void financialLegend();

    void statisticalBoxPlain();
    void statisticalBoxVerticalKey();
    void statisticalBoxSelected();
    void statisticalBoxLegend();

    void intervalsPlain();
    void intervalsLanes();
    void intervalsSelected();
    void intervalsLegend();

    void itemLineArrows();
    void itemStraightLine();
    void itemCurve();
    void itemBracket();
    void itemRect();
    void itemEllipse();
    void itemPixmap();
    void itemPixmapScaled();
    void itemTracer();

    void spanHorizontal();
    void spanVertical();
    void spanRect();
    void spanSelected();
};

#endif // TEST_REFERENCE_M4E_H
