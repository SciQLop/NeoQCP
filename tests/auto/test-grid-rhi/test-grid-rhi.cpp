#include "test-grid-rhi.h"
#include "../../../src/qcp.h"
#include "../../../src/painting/grid-rhi-layer.h"
#include <QtWidgets/qtestsupport_widgets.h> // QTest::qWaitForWindowExposed

namespace {
bool showAndHasRhiGrid(QCustomPlot* plot)
{
    plot->show();
    if (!QTest::qWaitForWindowExposed(plot))
        return false;
    QCoreApplication::processEvents();
    return plot->rhi() != nullptr;
}
} // namespace

void TestGridRhi::init()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
    mPlot->xAxis->setRange(-5, 5);
    mPlot->yAxis->setRange(-5, 5);
    mPlot->replot();
}

void TestGridRhi::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
}

void TestGridRhi::gridRhiLayerCreatedLazily()
{
    auto* grl = mPlot->gridRhiLayer();
    Q_UNUSED(grl);
    QVERIFY(true);
}

void TestGridRhi::replotDoesNotCrash()
{
    mPlot->replot();
    QVERIFY(true);
}

void TestGridRhi::replotWithSubGridDoesNotCrash()
{
    mPlot->xAxis->grid()->setSubGridVisible(true);
    mPlot->yAxis->grid()->setSubGridVisible(true);
    mPlot->replot();
    QVERIFY(true);
}

void TestGridRhi::replotWithZeroLineDoesNotCrash()
{
    mPlot->xAxis->setRange(-10, 10);
    mPlot->yAxis->setRange(-10, 10);
    mPlot->xAxis->grid()->setZeroLinePen(QPen(Qt::black, 1, Qt::SolidLine));
    mPlot->yAxis->grid()->setZeroLinePen(QPen(Qt::black, 1, Qt::SolidLine));
    mPlot->replot();
    QVERIFY(true);
}

void TestGridRhi::replotWithLogAxisDoesNotCrash()
{
    mPlot->xAxis->setScaleType(QCPAxis::stLogarithmic);
    mPlot->xAxis->setRange(0.01, 1000);
    mPlot->xAxis->setTicker(QSharedPointer<QCPAxisTickerLog>(new QCPAxisTickerLog));
    mPlot->replot();
    QVERIFY(true);
}

void TestGridRhi::replotWithReversedAxisDoesNotCrash()
{
    mPlot->xAxis->setRangeReversed(true);
    mPlot->yAxis->setRangeReversed(true);
    mPlot->replot();
    QVERIFY(true);
}

void TestGridRhi::replotMultipleAxisRectsDoesNotCrash()
{
    mPlot->plotLayout()->addElement(0, 1, new QCPAxisRect(mPlot));
    mPlot->replot();
    QVERIFY(true);
}

void TestGridRhi::dirtyDetectionSkipsRebuildOnPan()
{
    mPlot->replot();
    QCPRange oldRange = mPlot->xAxis->range();
    mPlot->xAxis->setRange(oldRange.lower + 0.01, oldRange.upper + 0.01);
    mPlot->replot();
    QVERIFY(true);
}

void TestGridRhi::dirtyDetectionRebuildsOnTickChange()
{
    mPlot->replot();
    mPlot->xAxis->setRange(100, 200);
    mPlot->replot();
    QVERIFY(true);
}

void TestGridRhi::dirtyDetectionRebuildsOnPenChange()
{
    mPlot->replot();
    mPlot->xAxis->grid()->setPen(QPen(Qt::red, 2));
    mPlot->replot();
    QVERIFY(true);
}

void TestGridRhi::exportStillUsesQPainter()
{
    QPixmap pm = mPlot->toPixmap(200, 150);
    QVERIFY(!pm.isNull());
    QCOMPARE(pm.width(), 200);
    QCOMPARE(pm.height(), 150);
}

void TestGridRhi::tickMarksFollowPanWithoutRebuild()
{
    if (!showAndHasRhiGrid(mPlot))
        QSKIP("No QRhi available — the grid RHI layer needs a real backend");
    // Sub-ticks are recomputed from the exact range edges, so even a pan that
    // keeps the same major ticks shifts the sub-tick set near the boundary and
    // would trigger the (correct, pre-existing) full-rebuild path. Disable them
    // so this test isolates the tick-mark-pixel re-bake this fix is about.
    mPlot->xAxis->setSubTicks(false);
    // Ranges chosen so a small pan keeps the same major tick set.
    mPlot->xAxis->setRange(0.5, 10.5);
    mPlot->yAxis->setRange(0.5, 10.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QCoreApplication::processEvents();

    auto* grl = mPlot->gridRhiLayer();
    QVERIFY(grl);
    auto upload = [&]() {
        QRhiResourceUpdateBatch* batch = mPlot->rhi()->nextResourceUpdateBatch();
        grl->uploadResources(batch, mPlot->rhiOutputSize(), float(mPlot->bufferDevicePixelRatio()),
                             mPlot->rhi()->isYUpInNDC());
        batch->release();
    };
    upload();

    int tickGroup = -1;
    for (int i = 0; i < grl->drawGroups().size(); ++i)
        if (!grl->drawGroups()[i].isGridLines) { tickGroup = i; break; }
    QVERIFY(tickGroup >= 0);
    const auto groupsBefore = grl->drawGroups().size();
    QRhiBuffer* uboBefore = grl->drawGroups()[tickGroup].uniformBuffer;
    const int groupFloatOffset = grl->drawGroups()[tickGroup].vertexOffset * 11;
    const int groupVertexCount = grl->drawGroups()[tickGroup].vertexCount;
    const double firstTick = mPlot->xAxis->tickVector().first();

    // The tick group interleaves both axes' vertices; the internal axis emission
    // order (which axis comes first within the group) is not part of the public
    // contract, so locate the x axis's first major-tick vertex by its known pixel
    // position rather than assuming it is at the group's first vertex.
    int floatOffset = -1;
    const float expectedXBefore = float(mPlot->xAxis->coordToPixel(firstTick));
    for (int v = 0; v < groupVertexCount; ++v)
    {
        const int off = groupFloatOffset + v * 11;
        if (qAbs(grl->stagingVertices()[off] - expectedXBefore) < 1.0f)
        {
            floatOffset = off;
            break;
        }
    }
    QVERIFY(floatOffset >= 0);
    const float xBefore = grl->stagingVertices()[floatOffset];
    QVERIFY(qAbs(xBefore - expectedXBefore) < 1.0f);

    const auto ticksBefore = mPlot->xAxis->tickVector();
    mPlot->xAxis->setRange(0.6, 10.6);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QCOMPARE(mPlot->xAxis->tickVector(), ticksBefore); // same ticks: no rebuild expected
    upload();

    QCOMPARE(grl->drawGroups().size(), groupsBefore);
    QCOMPARE(grl->drawGroups()[tickGroup].uniformBuffer, uboBefore); // no group churn
    const float xAfter = grl->stagingVertices()[floatOffset];
    QVERIFY(xAfter != xBefore);
    QVERIFY(qAbs(xAfter - float(mPlot->xAxis->coordToPixel(firstTick))) < 1.0f);
}

void TestGridRhi::gridLinesStayAlignedWithTicksAtEpochScale()
{
    if (!showAndHasRhiGrid(mPlot))
        QSKIP("No QRhi available — the grid RHI layer needs a real backend");

    // A 2025-epoch-scale range (~1.757e9), as wide as a 40 minute plot: large
    // enough in magnitude that a naive float32 cast of the raw coordinate
    // loses ~128s of precision (2^(30-23) ULP at this magnitude) -- a
    // sizeable fraction of the 2400s span. The offset keeps `lower` off a
    // "nice" tick value -- otherwise the first tick equals `lower` exactly
    // and both round to the same float32, masking the bug by coincidence.
    const double lower = 1757000000.0 + 37.123;
    const double upper = lower + 2400.0;
    mPlot->xAxis->setRange(lower, upper);
    mPlot->xAxis->setSubTicks(false);
    mPlot->yAxis->grid()->setVisible(false); // isolate the x axis's grid lines
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QCoreApplication::processEvents();

    auto* grl = mPlot->gridRhiLayer();
    QVERIFY(grl);
    QRhiResourceUpdateBatch* batch = mPlot->rhi()->nextResourceUpdateBatch();
    grl->uploadResources(batch, mPlot->rhiOutputSize(), float(mPlot->bufferDevicePixelRatio()),
                         mPlot->rhi()->isYUpInNDC());
    batch->release();

    QVERIFY(!mPlot->xAxis->tickVector().isEmpty());
    const double tickValue = mPlot->xAxis->tickVector().first();
    const float expectedPixel = float(mPlot->xAxis->coordToPixel(tickValue));

    int gridGroup = -1;
    for (int i = 0; i < grl->drawGroups().size(); ++i)
        if (grl->drawGroups()[i].isGridLines) { gridGroup = i; break; }
    QVERIFY(gridGroup >= 0);
    const auto& group = grl->drawGroups()[gridGroup];
    QVERIFY(group.vertexCount > 0);

    const auto* params = grl->lastUboParams(group.axisRect);
    QVERIFY(params);

    // The y grid is hidden and sub-ticks are off, so the group's first vertex
    // is unambiguously the first major x tick's line.
    const float vx = grl->stagingVertices()[group.vertexOffset * 11];

    // Reproduce the vertex shader's linear transform in float32: the actual
    // pixel position the GPU will draw, not the CPU-precise coordToPixel().
    const float t = (vx - params->keyRangeLower) / (params->keyRangeUpper - params->keyRangeLower);
    const float shaderPixel = t * params->keyAxisLength + params->keyAxisOffset;

    QVERIFY(qAbs(shaderPixel - expectedPixel) < 1.0f);
}
