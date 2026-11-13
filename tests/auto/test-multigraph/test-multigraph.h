#pragma once
#include <QtTest/QtTest>

class QCustomPlot;
class QCPGraph;

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
    void addDataAppendsToAnOwnedSource();
    void addDataInsertsUnsortedKeysInOrder();
    void addDataOnAViewIsAMisuse();
    void addDataOnASharedSourceIsAMisuse();
    void addDataOnAWaterfallIsAMisuse();
    void addDataOnAFreshWaterfallIsAMisuse();
    void addDataWithAPendingSwapAppendsToTheStagedSource();
    void aBusyAppendGoesThroughTheDeferredSwap();
    void removeDataBeforeKeepsTheRest();
    void removeDataAfterKeepsTheRest();
    void removeDataRangeKeepsTheRest();
    void removeOnAViewIsAMisuse();
    void aRollingWindowStaysBounded();
    void aBusyRemoveGoesThroughTheDeferredSwap();
    void removeWithAPendingSwapEditsTheStagedSource();
    void aSwapStagedBeforeASystemSwitchShowsOnTheUserSystem();
    void theGapThresholdIsSetOnTheInstalledSource();
    void setDataWithAShortValueColumnIsAMisuse();
    void setDataWithANonFiniteKeyIsAMisuse();
    void addDataColumnCountMismatchIsAMisuse();
    void aPipelineSeesTheAppend();
    void oneColumnSetDataTakesQVectors();
    void bracedListsSetOneColumn();
    void addDataSinglePointOnAMultiColumnGraphIsAMisuse();
    void addDataOnAUserSystemRedrawsWithoutPipeline();
    void aSourceStagedInDataSpaceGetsAPipelineJobAfterReturningToCartesian();
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
    void aOneComponentGraphRowShowsTheGraphName();
    void anEmptyGraphRowDrawsThePenLine();
    void anExplicitComponentNameWins();
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
    void anAlmostTransparentFillStillPaints();
    void hidingAnEdgeRemovesTheFillAndShowingBringsItBack();
    void fillBreaksAtAKeyGap();
    void fillBreaksAtAKeyGapPastTheFirstRow();
    void fillOnALogAxisClampsAZeroBaseline();
    void bandEdgeAtZeroOnALogAxisReachesTheEdge();
    void bandEdgeAtZeroOnALogAxisIsClampedLikeABaseline();
    void fillsFollowTheViewOnTheGpu();
    void bandEdgeAtZeroOnALogAxisStaysPastTheEdgeUnderAGpuPan();
    void defaultFillFollowsTheOwnerColourOnTheGpu();
    void autoScaleIncludesHiddenLineBandEdges();
    void fillFollowsADownsampledLine();
    void settingAFillRepaintsTheLayer();
    void clearingAFillRepaintsTheLayer();
    void changingAFillBrushRepaintsTheLayerInTheNewBrush();
    void aFillWithoutItsComponentYetIsGray();
    void aRebuiltGpuFillIsNotDrawnTwice();
    void aFillOnAColumnOnlyTheStagedSourceHasIsNotDrawn();
    void denseFillRowsAreBinnedAboveFourPerPixel();
    void fillIsOneRibbonCommand();
    void fillBetweenComponentsIsOneRibbon();
    void aFillToAnotherGraphFillsTheBand();
    void aFillToAnotherGraphCropsAndInterpolates();
    void noOverlapNoBand();
    void theFillDropsWhenThePartnerDies();
    void aDeadPartnersFillIsGone();
    void theFillDropsWhenThePartnerLosesTheComponent();
    void aFillToAGraphOfAnotherSystemIsAMisuse();
    void aFillToAGraphOnAnotherValueAxis();
    void theBandFollowsTheOtherValueAxis();
    void aPartnerMovedToAnotherSystemDrawsNoBand();
    void aSelectedRangeSplitsTheBand();
    void aDenseFillSelectionUsesItsKeyRange();
    void aSteppedFillSelectionUsesItsKeyRange();
    void theFillDoesNotReturnWhenThePartnerComponentComesBack();
    void aPartnerDataChangeRedrawsTheBand();
    void aPartnerOnAnotherLayerRedrawsTheBand();
    void mutualFillsDoNotRecurse();
    void clearingAGraphFillDropsThePartnerDependency();
    void retargetingAGraphFillDropsTheOldPartnerDependency();
    void theBandStaysOnTheGpuRibbon();
    void fillToGraphSeriesCropsUnequalGrids();
    void fillToGraphSeriesNoOverlap();
    void fillToGraphSeriesNaNBreaksRuns();
    void fillsDrawUnderErrorBarsUnderLines();
    void fillsDrawUnderLinesOnTheGpu();

    // Error bars
    void sparseErrorBarsAreDrawnAsBars();
    void denseErrorBarsBecomeABand();
    void errorBoundsBelowZeroOnALogAxisReachTheEdge();
    void errorBandBoundsBelowZeroOnALogAxisAreClampedLikeABaseline();
    void selectedErrorBarsUseTheSelectedPen();
    void dashedErrorBarsAreDrawn();
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
    void errorBarsAreDrawnOnTheGpuPath();
    void errorBarsHideTheirBoundsFromTheLegend();
    void errorBoundsSetBeforeDataStayOutOfTheLegend();
    void legendIconOfErrorBarsHasABarGlyph();
    void legendIconOfErrorBarsHasACapAtEachEnd();
    void legendIconOfErrorBarsUsesTheBarsPen();
    void legendIconHasOneSegmentPerComponent();
    void legendSwatchIsTwelvePixelsTall();
    void errorBarsAreOneCommand();
    void verticalKeyErrorBarsRunAlongTheValue();
    void clickSelectionRedrawsTheComponent();
    void adaptiveSamplingOffRecordsEveryRow();
    void logValueImpulsesFollowTheValueRange();
    void logKeyCentreStepSitsHalfwayOnScreen();
    void logKeyCentreStepFillSitsHalfwayOnScreen();
    void anEmptyViewDoesNotOutliveAZoomOut();
    void aZoomRecordsOnce();
    void aCrossingFillIsTwoLobesOnTheGpu();
    void legendIconMarkerTakesTheComponentPen();
    void denseErrorBarsAreABand();
    void legendIconShowsEachComponent();
    void settingErrorBarsRepaintsTheLayer();
    void clearingErrorBarsRepaintsTheLayer();
    void errorBarsOfDifferentColumnsCoexist();
    void replacingErrorBarsKeepsOnlyTheNewOnes();
    void clearedErrorBoundsComeBackToTheLegend();
    void replacedErrorBoundsComeBackToTheLegend();
    void aBoundTheUserHidFromTheLegendStaysHidden();
    void errorBoundsOfBarsDroppedByANarrowerSourceComeBackToTheLegend();
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
    void lineCacheFollowsTheScaleOfANewKeyAxis();
    void lineCacheFollowsTheScaleOfANewValueAxis();
    void lineCacheIsRebuiltWhenMovedToALogKeyAxis();
    void lineCacheIsRebuiltWhenMovedToALogValueAxis();
    void theKeyAxisItLeftIsNoLongerListenedTo();
    void aNarrowerThenWiderStagedSourceKeepsTheCachedExtrusionsInRange();
    void aWiderStagedSourceKeepsTheCachedLinesInRange();
    void zoomedOutLinesAreThinnedToThePixelWidth_data();
    void zoomedOutLinesAreThinnedToThePixelWidth();

    // Recorded lines and markers
    void panningABigGraphDoesNotRecord();
    void panPastTheWindowRecordsFreshLines();
    void theSettleAfterAPanRecordsAgain();
    void logKeyAxisLinesMatchTheData();
    void logKeyAxisBigGraphIsThinned();
    void exportBeforeTheFirstResampleHasTheLines();
    void colouredLineIsOneCommandPerComponent();
    void waterfallDrawsItsOffsets();

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
    void aHugeStrokeThenASmallOneGivesTheScratchBack();
    void selectedStepLineKeepsItsSteps();
    void committedDataIsDrawnOnScreen();
    void panIsRedrawnFreshAfterTheDebounce();
    void panIsRedrawnFreshWhileAnotherLayerIsDirty();
    void resampledLinesFollowANewKeyAxis();

    void noSetterIsIgnored_data();
    void noSetterIsIgnored();
    void setPenAfterComponentColoursRepensEveryComponent();
    void setBrushAfterAComponentFillRefillsEveryComponent();
    void setPenBeforeDataColoursTheNewComponents();
    void setBrushBeforeDataFillsTheNewComponents();
    void setScatterStyleBeforeDataReachesTheNewComponents();

    // R1: selection as 2.x
    void setSelectionDrawsTheSelectedSegments();
    void theDecoratorPenIsUsed();
    void theBracketDecoratorDraws();
    void clearingTheSelectionClearsEveryComponent();
    void aPartialSelectionKeepsTheRestNormal();
    // R2: the legend row is a plottable's row
    void itemWithPlottableFindsAGraphRow();
    void aGraphRowCastsToAPlottableRow();
    // R3: the brush colours a fill to another graph
    void setBrushBeforeAFillToGraphKeepsTheBand();
    void setBrushAfterAFillToGraphRecoloursIt();
    void aFillToGraphTakesTheGraphBrush();
    // R4: a negative log range closes toward zero
    void aNegativeLogRangeFillsToTheUpperBound();
    void aNegativeLogRangeImpulsesStartAtTheUpperBound();

    // 5c final review
    void aClickOnADefaultGraphSelectsTheWholeGraph();
    void anAdditiveClickOnAWholeSelectedGraphDeselectsIt();
    void anAdditiveClickOnASelectedRowDeselectsIt();
    void aRectSelectionHonoursStSingleData();
    void makingAGraphUnselectableClearsEveryComponent();
    void setBrushNoBrushRemovesTheBandToAnotherGraph();
    void aPartnerMovedToAnotherRectDropsTheBandWithoutACrash();
    void aPartnerTakenFromThePlotDropsTheFill();
    void theDataAccessorsAnswerZeroOutsideTheData();
    void aBracketOnASelectionBeyondTheDataDoesNotReadPastIt();
    void manySelectedRangesMarkEachRow();
    void theTwoXPropertiesAnswer();
    void aGraphIsSelectableBeyondTheRectWhenAsked();
    void aFillBrushWithoutAFillDoesNotDirty();
    void removeDataWithAnEmptyIntervalRemovesNothing();

private:
    // The staged source stays staged, as while its pipeline prepares it: exports commit ready ones.
    static void stillPreparing(QCPGraph* mg);
    static bool marksOwner(QCPGraph* partner, QCPGraph* owner);
    QCustomPlot* mPlot = nullptr;
};
