#pragma once

#include <QtTest/QtTest>

class TestDrawContext : public QObject
{
    Q_OBJECT

private slots:
    void polylineMatchesHandPainting();
    void polylinePositionsMatchesHandPainting();
    void polygonMatchesHandPainting();
    void shapeEndingOutsideTheClipIsDrawn();
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
    void mixedPositionsKeepKeyAndValueOnAVerticalKeyAxis();
    void transparentBrushNeedsNoRaster();
    void errorBarsMapTheirSpansAsExtents();
    void ellipseFanFillCoversTheDisc();
    void mixedRecordingSplitsByKind();
    void extruderContinuesArcLengthAcrossRuns();
    void extrusionCacheDropsOnADprChange();
    void tenThousandDataRectsAreOneCommand();
    void aPixelCornerStaysASingleRect();
    void perRectColoursStayOneBatch();
    void rasterBrushRectsStayOneCommandEach();
    void ellipsesStayOneBatch();
    void aHugeEllipseKeepsTheFrame();
    void batchExportsOneRectPerBar();
    void colouredPolylineIsOneCommand();
    void segmentTakesItsFirstVertexColour();
    void colouredMarkersKeepTheirColours();
    void mismatchedColourCountIsRefused();
    void colouredPolylinePanReusesTheKeptGeometry();
    void theKeyWindowComesFromTheSystem();
};
