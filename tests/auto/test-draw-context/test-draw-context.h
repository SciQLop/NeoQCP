#pragma once

#include <QtTest/QtTest>

class TestDrawContext : public QObject
{
    Q_OBJECT

private slots:
    void polylineMatchesHandPainting();
    void polylinePositionsMatchesHandPainting();
    void polygonMatchesHandPainting();
    void fillBetweenMatchesHandPainting();
    void rectMatchesHandPainting();
    void ellipseMatchesHandPainting();
    void markersMatchesHandPainting();
    void errorBarsMatchesHandPainting();
    void textMatchesHandPainting();
    void pathMatchesHandPainting();
    void pathDoesNotShiftLaterCommands();
    void imageMatchesHandPainting();
    void painterMatchesHandPainting();
    void painterCallbackStateDoesNotLeak();
    void boundsCoverEveryCommand();
    void emptyCommandDoesNotUnboundTheRecording();
    void sharedDataIsNotCopied();
    void drawListIsCompact();
    void resolveCacheSurvivesNewEntries();
    void resolveCacheIgnoresAReusedAddress();
    void resolveCacheEvictsUnusedEntries();
    void dashLengthsIncludeTheCaps();
    void emptyCustomDashIsSolid();
    void closedRingJoinsTheSeam();
    void nonSolidBrushIsNotFlattened();
    void transparentBrushNeedsNoRaster();
    void errorBarsMapTheirSpansAsExtents();
    void ellipseFanFillCoversTheDisc();
    void mixedRecordingSplitsByKind();
    void extruderContinuesArcLengthAcrossRuns();
    void extrusionCacheDropsOnADprChange();
};
