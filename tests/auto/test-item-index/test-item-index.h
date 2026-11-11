#include "qcustomplot.h"
#include <QtTest/QtTest>

class TestItemIndex : public QObject
{
    Q_OBJECT
private slots:
    void randomItemsMatchUnindexed();
    void dragOneItemKeepsTheTree();
    void clickTestsOnlyItemsNearTheClick();
    void scaleTypeChangeRebuildsTheIndex();
    void emptyClickVisitsOnlySelectedItems();
    void aRetargetedItemFollowsItsNewArea();
    void aDataExtentDoesNotBuildATransform();
    void itemsOfTwoSystemsOfOneRectAreIndexedApart();
    void anItemOnAPolarSystemIsAlwaysACandidate();
    void aDeletedAxisDropsItsGroup();
};
