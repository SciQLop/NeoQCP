#include "test-data-locator.h"
#include "data-locator.h"
#include <plottables/plottable-multigraph.h>

void TestDataLocator::init()
{
    mPlot = new QCustomPlot(nullptr);
    mPlot->resize(400, 300);
}

void TestDataLocator::cleanup()
{
    delete mPlot;
}

void TestDataLocator::locateOnGraph()
{
    auto* graph = mPlot->addGraph();
    mPlot->xAxis->setRange(0, 10);
    mPlot->yAxis->setRange(0, 10);
    graph->addData(5.0, 5.0);
    graph->addData(6.0, 7.0);
    mPlot->replot();

    QPointF pixelPos = graph->coordsToPixels(5.0, 5.0);

    QCPDataLocator locator;
    locator.setPlottable(graph);
    bool found = locator.locate(pixelPos);

    QVERIFY(found);
    QVERIFY(locator.isValid());
    QCOMPARE(locator.key(), 5.0);
    QCOMPARE(locator.value(), 5.0);
    QCOMPARE(locator.plottable(), graph);
    QCOMPARE(locator.hitPlottable(), graph);
    QVERIFY(locator.dataIndex() >= 0);
}

void TestDataLocator::locateOnGraph2()
{
    auto* g2 = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    mPlot->xAxis->setRange(0, 10);
    mPlot->yAxis->setRange(0, 10);
    g2->setData(std::vector<double>{3.0, 5.0, 7.0},
                std::vector<double>{2.0, 8.0, 4.0});
    mPlot->replot();

    QPointF pixelPos = g2->coordsToPixels(5.0, 8.0);

    QCPDataLocator locator;
    locator.setPlottable(g2);
    bool found = locator.locate(pixelPos);

    QVERIFY(found);
    QVERIFY(locator.isValid());
    QCOMPARE(locator.key(), 5.0);
    QCOMPARE(locator.value(), 8.0);
    QCOMPARE(locator.hitPlottable(), g2);
}

void TestDataLocator::locateOnCurve()
{
    auto* curve = new QCPCurve(mPlot->xAxis, mPlot->yAxis);
    mPlot->xAxis->setRange(0, 10);
    mPlot->yAxis->setRange(0, 10);
    curve->addData(4.0, 6.0);
    curve->addData(5.0, 7.0);
    mPlot->replot();

    QPointF pixelPos = curve->coordsToPixels(4.0, 6.0);

    QCPDataLocator locator;
    locator.setPlottable(curve);
    bool found = locator.locate(pixelPos);

    QVERIFY(found);
    QVERIFY(locator.isValid());
    QCOMPARE(locator.key(), 4.0);
    QCOMPARE(locator.value(), 6.0);
    QCOMPARE(locator.hitPlottable(), curve);
}

void TestDataLocator::locateOnColorMap()
{
    auto* colorMap = new QCPColorMap(mPlot->xAxis, mPlot->yAxis);
    colorMap->data()->setSize(4, 4);
    colorMap->data()->setRange(QCPRange(0, 10), QCPRange(0, 10));
    colorMap->data()->setCell(1, 2, 42.0);
    mPlot->xAxis->setRange(0, 10);
    mPlot->yAxis->setRange(0, 10);
    mPlot->replot();

    double cellKey, cellValue;
    colorMap->data()->cellToCoord(1, 2, &cellKey, &cellValue);
    QPointF pixelPos = colorMap->coordsToPixels(cellKey, cellValue);

    QCPDataLocator locator;
    locator.setPlottable(colorMap);
    bool found = locator.locate(pixelPos);

    QVERIFY(found);
    QVERIFY(locator.isValid());
    QCOMPARE(locator.data(), 42.0);
    QCOMPARE(locator.hitPlottable(), colorMap);
}

void TestDataLocator::locateOnEmptyPlottable()
{
    auto* graph = mPlot->addGraph();
    mPlot->replot();

    QCPDataLocator locator;
    locator.setPlottable(graph);
    bool found = locator.locate(QPointF(200, 150));

    QVERIFY(!found);
    QVERIFY(!locator.isValid());
}

void TestDataLocator::locateWithNullPlottable()
{
    QCPDataLocator locator;
    bool found = locator.locate(QPointF(200, 150));

    QVERIFY(!found);
    QVERIFY(!locator.isValid());
}

void TestDataLocator::locateOnMultiGraphSelectsClosestComponent()
{
    // Bug: locateMultiGraph always returned component 0's value regardless
    // of which component was closest to the cursor position.
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mPlot->xAxis->setRange(0, 4);
    mPlot->yAxis->setRange(-50, 50);

    // Component 0 at y=10, component 1 at y=-30
    std::vector<double> keys = {1.0, 2.0, 3.0};
    std::vector<std::vector<double>> values = {
        {10.0, 10.0, 10.0},   // component 0
        {-30.0, -30.0, -30.0} // component 1
    };
    mg->setData(std::move(keys), std::move(values));
    mPlot->replot();

    // Locate near component 1 (y=-30) at x=2
    QPointF pixelNearComp1 = mg->coordsToPixels(2.0, -30.0);

    QCPDataLocator locator;
    locator.setPlottable(mg);
    bool found = locator.locate(pixelNearComp1);

    QVERIFY(found);
    QVERIFY(locator.isValid());
    // Must return component 1's value (-30), not component 0's (10)
    QCOMPARE(locator.value(), -30.0);
}

void TestDataLocator::locateAtKeyOnAMultiGraphWithoutColumns()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double>{1.0, 2.0, 3.0}, std::vector<std::vector<double>>{});
    QCOMPARE(mg->dataSource()->columnCount(), 0);

    QCPDataLocator locator;
    QVERIFY(!locator.locateAtKey(mg, 2.0));
    QVERIFY(!locator.isValid());
}
