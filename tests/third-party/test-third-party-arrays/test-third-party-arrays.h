#pragma once
#include <QtTest/QtTest>

/*!
  The data hooks of third-party arrays: Eigen, xtensor and mdspan go into the data-taking calls as
  they are, through the generic reading or through a hook the user writes beside them.
*/
class TestThirdPartyArrays : public QObject
{
    Q_OBJECT
private slots:
    void anEigenVectorIsReadInPlace();
    void anEigenFloatVectorIsRead();
    void anEigenSegmentAndColumnAreRead();
    void anEigenMatrixGivesComponents();
    void anXtensorIsRead();
    void aSteppedXtensorViewIsRead();
    void anXtensor2DIsColumnsNotNumbers();
    void anMdspanGridGivesComponents();
    void aStridedViewIsRead();
    void aTransformViewOfFloatIsRead();
};
