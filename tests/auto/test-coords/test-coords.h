#ifndef TEST_COORDS_H
#define TEST_COORDS_H

#include <QtTest/QtTest>

class TestCoords : public QObject
{
    Q_OBJECT
private slots:
    void mapMatchesCoordToPixel();
    void batchMatchesScalar();
    void interleavedBatchMatchesScalar();
    void translationDetected();
    void dragPanOnATimeRangeIsATranslation();
    void panDeltaIsMeasuredInsideTheRange();
    void positionSetMatchesSetters();
    void refusedAnchorLeavesPositionUntouched();
    void anchorOffsetIsPixelsWhateverTheOldType();
    void ratioTransformMatchesPixels();
    void mixingSpacesDoesNotCompile();
};

#endif // TEST_COORDS_H
