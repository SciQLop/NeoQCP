#include "test-color-by-scalar.h"
#include "qcustomplot.h"
#include "datasource/soa-multi-datasource.h"

using SoA = QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>;

void TestColorByScalar::init()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
}

void TestColorByScalar::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
}

std::shared_ptr<QCPAbstractMultiDataSource> TestColorByScalar::makeSource(
    std::vector<double> keys, std::vector<std::vector<double>> columns)
{
    return std::make_shared<SoA>(std::move(keys), std::move(columns));
}

void TestColorByScalar::setLineStyleInvalidatesLineCache()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setDataSource(makeSource({0, 1, 2, 3}, {{0, 1, 0, 1}}));
    mPlot->xAxis->setRange(0, 3);
    mPlot->yAxis->setRange(-1, 2);
    mPlot->replot();
    QVERIFY(!mg->mLineCacheDirty);

    mg->setLineStyle(QCPMultiGraph::lsStepLeft);
    QVERIFY(mg->mLineCacheDirty);
    QVERIFY(mg->mCachedLines.isEmpty());
}
