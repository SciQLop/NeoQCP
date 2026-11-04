#include <QtTest/QtTest>

class TestSpatialIndex : public QObject
{
    Q_OBJECT
private slots:
    void emptyIndex();
    void singleBox();
    void edgeTouchIsAHit();
    void infiniteSides();
    void nanEntriesDropped();
    void randomMatchesBruteForce();
    void rebuildReplaces();
};
