#pragma once
#include <QtTest/QtTest>

class QCustomPlot;

class TestMultiGraph : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    // Data source wiring
    void creation();
    void setDataSourceShared();
    void setDataSourceUnique();
    void setDataConvenience();
    void viewDataConvenience();
    void componentCountMatchesDataSource();
    void componentValueAt();
    void dataCountDelegate();
    void dataMainKeyDelegate();
    void dataMainValueColumn0();
    void findBeginEndDelegate();
    void keyRangeDelegate();
    void valueRangeUnion();

    // Selection
    void selectTestFindsClosestComponent();
    void selectTestReturnsComponentInDetails();
    void selectTestRectDoesNotMutateState();
    void selectTestRectPerComponent();
    void selectEventSingleComponent();
    void selectEventRectSelection();
    void selectEventAdditive();
    void deselectEventClearsAll();
    void componentSelectionUpdatesBase();
    void anInLegendIndexPastTheEndIsIgnored();

    // Legend
    void addToLegendCreatesGroupItem();
    void removeFromLegendWorks();
    void legendSkipsComponentsOutOfTheLegend();
    void legendClickMapsRowsToInLegendComponents();
    void legendOneInLegendComponentHasNoExpander();
    void legendHeaderClickSelectsTheOnlyInLegendComponent();
    void legendSelectTestSurvivesNullMultiGraph();
    void legendExpandCollapse();
    void legendGroupSelectsAll();
    void legendHeaderDoesNotLeakClassNameWhenEmpty();
    void legendHeaderShowsNameWhileEmpty();
    void legendCollapsedFitsExpanderMarker();
    void legendOneComponentHasNoExpander();
    void legendIconDrawsScatterMarkers();
    void legendIconOfABandIsAFilledSwatch();
    void legendIconDrawsTheLineOverTheSwatch();
    void collapsedLegendRowShowsTheBandSwatch();
    void legendCollapsedFitsBusyIndicator();

    // Fills
    void fillRejectsBadColumns();
    void fillBeforeDataIsKept();
    void fillIsDroppedWhenItsColumnDisappears();
    void fillDefaultBrushIsTheOwnerColourAtQuarterAlpha();
    void fillReplacesAndClears();
    void fillValidatesAgainstTheStagedSource();
    void fillToZeroColoursBelowTheCurveOnly();
    void hiddenEdgeLinesKeepTheirFill();
    void hidingAnEdgeRemovesTheFillAndShowingBringsItBack();
    void fillBreaksAtAKeyGap();
    void fillOnALogAxisClampsAZeroBaseline();
    void bandEdgeAtZeroOnALogAxisReachesTheEdge();
    void bandEdgeAtZeroOnALogAxisIsClampedLikeABaseline();
    void fillCacheIsRebuiltOnZoom();
    void bandEdgeAtZeroOnALogAxisStaysPastTheEdgeUnderAGpuPan();
    void defaultFillFollowsTheOwnerColourOnTheGpu();
    void autoScaleIncludesHiddenLineBandEdges();
    void fillFollowsADownsampledLine();
    void settingAFillRepaintsTheLayer();
    void clearingAFillRepaintsTheLayer();
    void aFillWithoutItsComponentYetIsGray();
    void aRebuiltGpuFillIsNotDrawnTwice();
    void aFillOnAColumnOnlyTheStagedSourceHasIsNotDrawn();
    void denseFillRowsAreBinnedAboveFourPerPixel();
    void paintFillDrawsNoOutline();
    void paintFillHonoursTheFillAntialiasing();
    void paintFillRestoresThePainter();

    // Error bars
    void sparseErrorBarsAreDrawnAsBars();
    void denseErrorBarsBecomeABand();
    void errorBoundsBelowZeroOnALogAxisReachTheEdge();
    void errorBandBoundsBelowZeroOnALogAxisAreClampedLikeABaseline();
    void selectedErrorBarsUseTheSelectedPen();
    void errorBarsAreDroppedWhenABoundColumnDisappears();
    void anEmptySourceKeepsFillsAndErrorBars();
    void anEmptySourceKeepsTheComponents();
    void anEmptyWiderSourceDoesNotGrowTheComponents();
    void aStagedEmptyWiderSourceDoesNotGrowTheComponents();
    void aFirstEmptySourceAcceptsItsColumns();
    void aNullSourceClearsTheComponents();
    void aNarrowerSourceDropsDecorationsSetDuringAnEmptyWindow();
    void keptComponentsStayEditableDuringAnEmptyWindow();
    void errorBarsRejectBadColumns();
    void capWidthChangeShowsOnTheNextFrame();
    void errorBarsAreCachedOnTheGpu();
    void errorBarsHideTheirBoundsFromTheLegend();
    void errorBoundsSetBeforeDataStayOutOfTheLegend();
    void legendIconOfErrorBarsHasABarGlyph();
    void legendIconOfErrorBarsHasACapAtEachEnd();
    void legendIconOfErrorBarsUsesTheBarsPen();
    void legendIconHasOneSegmentPerComponent();
    void legendSwatchIsTwelvePixelsTall();
    void settingErrorBarsRepaintsTheLayer();
    void clearingErrorBarsRepaintsTheLayer();
    void errorBarsOfDifferentColumnsCoexist();
    void replacingErrorBarsKeepsOnlyTheNewOnes();
    void errorBoundOfZeroOnALogAxisStopsAtTheEdge();
    void errorBoundsInsideALogAxisAreNotClamped();
    void barsCrowdedAtAnEdgeOfTheViewForceABand();
    void barsOutsideTheViewDoNotForceABand();
    void aOneSidedErrorBandKeepsItsValueEdge();
    void errorBarsOnResampledDataAreABand();
    void denseErrorBandIsTranslucent();
    void hiddenComponentHidesItsErrorBars();
    void errorBarsOnAColumnOnlyTheStagedSourceHasAreNotDrawn();

    // Line cache
    void cacheExtendsBeyondVisibleRange();
    void lineCacheWindowFollowsTheKeyScale_data();
    void lineCacheWindowFollowsTheKeyScale();
    void aNarrowerThenWiderStagedSourceKeepsTheCachedExtrusionsInRange();
    void aWiderStagedSourceKeepsTheCachedLinesInRange();
    void zoomedOutLinesAreThinnedToThePixelWidth_data();
    void zoomedOutLinesAreThinnedToThePixelWidth();

    // Rendering
    void renderVerticalKeyAxisUsesHeight();
    void renderBasicDoesNotCrash();
    void renderWithSelection();
    void renderHiddenComponent();
    void renderEmptySource();
    void renderAllLineStyles();
    void renderZoomRepaintsWhenAnotherBufferIsDirty();
    void renderSetRangeRepaintsWhenAnotherBufferIsDirty();
    void componentVisibilityToggleForcesLayerRepaint();
    void componentShownAfterARebuildIsDrawnAgain();

    // Line style is per component, like the marker
    void componentLineStyleOnlyChangesThatComponent();
    void componentLineStyleChangeIsDrawnOnTheGpu();
    void setLineStyleAppliesToEveryComponent();
    void newComponentsTakeTheGraphLineStyle();
    void plainReextrusionOnPenChangeNotPan();
    void selectedStepLineKeepsItsSteps();
    void committedDataIsDrawnOnScreen();
    void selectedStepGraph2KeepsItsSteps();
    void panIsRedrawnFreshAfterTheDebounce();
    void panIsRedrawnFreshWhileAnotherLayerIsDirty();

private:
    QCustomPlot* mPlot = nullptr;
};
