#ifndef TEST_TRANSFORM_H
#define TEST_TRANSFORM_H

#include <QtTest/QtTest>

class TestTransform : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void polarMappingMatchesTheAxes_data();
    void polarMappingMatchesTheAxes();
    void polarInverseRoundTrips();
    void polarClampsTheRadius_data();
    void polarClampsTheRadius();
    void polarEqualityFollowsTheRanges();
    void polarRefusesAxesOfTwoAreas();
    void customMapsOncePerSpan();
    void customEqualityUsesTheRevision();
    void cartesianInverseMatchesPixelToCoord_data();
    void cartesianInverseMatchesPixelToCoord();
};

#endif // TEST_TRANSFORM_H
