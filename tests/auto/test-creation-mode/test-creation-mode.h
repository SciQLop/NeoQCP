#pragma once
#include <QtTest/QtTest>

class QCustomPlot;

class TestCreationMode : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    // State machine basics
    void initialStateIsIdle();
    void noCreatorSetClickDoesNothing();

    // Click-move-click creation
    void vspanCreationClickMoveClick();
    void itemCreatedInASecondAxisRectIsClippedToIt();
    void hspanCreationClickMoveClick();
    void rspanCreationClickMoveClick();

    // Cancellation
    void cancelWithEscape();
    void cancelWithRightClick();
    void cancelCleansUpItem();

    // Modifier trigger and batch mode
    void modifierTriggerCreatesItem();
    void modifierNotHeldDoesNothing();
    void batchModeStaysActiveAfterCommit();

    // Priority and fallback
    void creationTakesPriorityOverSelectionRect();
    void fallbackTwoPositionItem();

    // Cursor and edge cases
    void cursorChangesInCreationMode();
    void clickOutsideAxisRectDoesNothing();
    void creatorReturnsNullDoesNotCrash();
    void disableCreationModeDuringDrawingCancels();
    void batchModeIgnoresExistingItems();
    void multiAxisRectCreation();

private:
    QCustomPlot* mPlot = nullptr;
};
