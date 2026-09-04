#include "test-pipeline.h"
#include <qcustomplot.h>
#include <painting/colormap-rhi-layer.h>
#include <painting/plottable-rhi-layer.h>
#include <QtWidgets/qtestsupport_widgets.h> // QTest::qWaitForWindowExposed

namespace {
bool showAndHasRhiLT(QCustomPlot* plot)
{
    plot->show();
    if (!QTest::qWaitForWindowExposed(plot))
        return false;
    QCoreApplication::processEvents();
    return plot->rhi() != nullptr;
}
} // namespace

void TestPipeline::stallPixelOffsetGraph2Busy()
{
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    QVector<double> keys(200000), values(200000);
    for (int i = 0; i < 200000; ++i) {
        keys[i] = i;
        values[i] = std::sin(i * 0.01);
    }
    graph->setData(std::move(keys), std::move(values));

    mPlot->xAxis->setRange(0, 100000);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QTRY_VERIFY_WITH_TIMEOUT(!graph->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    // Should be zero when not busy
    QCOMPARE(graph->stallPixelOffset(), QPointF(0, 0));

    // Pan to trigger busy state
    mPlot->xAxis->setRange(10000, 110000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    if (graph->pipeline().isBusy())
    {
        QPointF offset = graph->stallPixelOffset();
        QVERIFY(!offset.isNull());
    }
}

void TestPipeline::stallPixelOffsetIdleIsZero()
{
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    QVector<double> keys(100), values(100);
    for (int i = 0; i < 100; ++i) {
        keys[i] = i;
        values[i] = i;
    }
    graph->setData(std::move(keys), std::move(values));

    mPlot->xAxis->setRange(0, 100);
    mPlot->yAxis->setRange(0, 100);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QTRY_VERIFY_WITH_TIMEOUT(!graph->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QCOMPARE(graph->stallPixelOffset(), QPointF(0, 0));
}

void TestPipeline::layerPixelOffsetFromBusyChild()
{
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    QVector<double> keys(200000), values(200000);
    for (int i = 0; i < 200000; ++i) {
        keys[i] = i;
        values[i] = std::sin(i * 0.01);
    }
    graph->setData(std::move(keys), std::move(values));

    mPlot->xAxis->setRange(0, 100000);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QTRY_VERIFY_WITH_TIMEOUT(!graph->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QCPLayer* mainLayer = mPlot->layer("main");
    QVERIFY(mainLayer);

    // Idle: layer offset should be zero
    QCOMPARE(mainLayer->pixelOffset(), QPointF(0, 0));

    // Pan to trigger busy state
    mPlot->xAxis->setRange(10000, 110000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    if (graph->pipeline().isBusy())
    {
        QPointF layerOffset = mainLayer->pixelOffset();
        QPointF plottableOffset = graph->stallPixelOffset();
        // Layer offset should match the plottable's offset
        QCOMPARE(layerOffset, plottableOffset);
    }
}

void TestPipeline::layerPixelOffsetZeroWhenNoAsyncChildren()
{
    // main layer with no plottables should return zero
    QCPLayer* mainLayer = mPlot->layer("main");
    QVERIFY(mainLayer);
    QCOMPARE(mainLayer->pixelOffset(), QPointF(0, 0));
}

void TestPipeline::layerTranslationClippedToAxisRect()
{
    // GPU scissor clipping is hardware-enforced and cannot be verified via toPixmap()
    // (which uses the QPainter export path, bypassing RHI). This test is a smoke test:
    // verify that a large pan with layer translation enabled doesn't crash or assert.
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    QVector<double> keys(200000), values(200000);
    for (int i = 0; i < 200000; ++i) {
        keys[i] = i;
        values[i] = 1.0;
    }
    graph->setData(std::move(keys), std::move(values));
    graph->setPen(QPen(Qt::red, 2));

    mPlot->xAxis->setRange(0, 100000);
    mPlot->yAxis->setRange(-2, 2);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QTRY_VERIFY_WITH_TIMEOUT(!graph->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    // Large pan — layer offset will be very large, scissor must prevent GPU artifacts
    mPlot->xAxis->setRange(200000, 300000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    // Verify the layer has a nonzero offset (translation is active)
    QCPLayer* mainLayer = mPlot->layer("main");
    if (graph->pipeline().isBusy())
        QVERIFY(!mainLayer->pixelOffset().isNull());

    // Another replot should not crash with the large offset + scissor
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
}

void TestPipeline::bufferedMainLayerRendersSameAsLogical()
{
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    QVector<double> keys(100), values(100);
    for (int i = 0; i < 100; ++i) {
        keys[i] = i;
        values[i] = std::sin(i * 0.1);
    }
    graph->setData(std::move(keys), std::move(values));

    mPlot->xAxis->setRange(0, 100);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QTRY_VERIFY_WITH_TIMEOUT(!graph->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QCOMPARE(mPlot->layer("main")->mode(), QCPLayer::lmBuffered);

    QPixmap pixmap = mPlot->toPixmap(400, 300);
    QVERIFY(!pixmap.isNull());
}

void TestPipeline::existingGraph2TranslationUnaffected()
{
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    QVector<double> keys(200000), values(200000);
    for (int i = 0; i < 200000; ++i) {
        keys[i] = i;
        values[i] = std::sin(i * 0.01);
    }
    graph->setData(std::move(keys), std::move(values));

    mPlot->xAxis->setRange(0, 100000);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QTRY_VERIFY_WITH_TIMEOUT(!graph->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QVERIFY(graph->hasRenderedRange());

    mPlot->xAxis->setRange(10000, 110000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    if (graph->pipeline().isBusy())
    {
        QVERIFY(graph->hasRenderedRange());
        QPointF offset = graph->stallPixelOffset();
        QVERIFY(!offset.isNull());
    }
}

void TestPipeline::hiddenElementDoesNotBlockTranslation()
{
    // A QCPColorScale is created up-front by every SciQLopPlot and stays hidden
    // until a colormap needs it. It lives on the "main" layer, so a hidden one
    // used to veto the pan translation fast path for every line graph.
    auto* colorScale = new QCPColorScale(mPlot);
    colorScale->setVisible(false);
    QCOMPARE(mPlot->layer("main")->children().contains(colorScale), true);

    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    QVector<double> keys(200000), values(200000);
    for (int i = 0; i < 200000; ++i)
    {
        keys[i] = i;
        values[i] = std::sin(i * 0.01);
    }
    graph->setData(std::move(keys), std::move(values));

    mPlot->xAxis->setRange(0, 100000);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTRY_VERIFY_WITH_TIMEOUT(!graph->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    mPlot->xAxis->setRange(10000, 110000); // pure horizontal pan
    QVERIFY(!graph->stallPixelOffset().isNull());
    QVERIFY(mPlot->layer("main")->canTranslateInsteadOfRepaint());
}

void TestPipeline::visibleElementStillBlocksTranslation()
{
    // The veto must stay in place for elements that actually paint: the layer's
    // whole texture is shifted and scissor-clipped to the plot area, so anything
    // else drawing on that layer would be dropped from the frame.
    auto* colorScale = new QCPColorScale(mPlot);
    mPlot->plotLayout()->addElement(0, 1, colorScale);
    colorScale->setVisible(true);

    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    QVector<double> keys(200000), values(200000);
    for (int i = 0; i < 200000; ++i)
    {
        keys[i] = i;
        values[i] = std::sin(i * 0.01);
    }
    graph->setData(std::move(keys), std::move(values));

    mPlot->xAxis->setRange(0, 100000);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTRY_VERIFY_WITH_TIMEOUT(!graph->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    mPlot->xAxis->setRange(10000, 110000);
    QVERIFY(!mPlot->layer("main")->canTranslateInsteadOfRepaint());
}

void TestPipeline::colormapQuadOffsetTranslatesRect()
{
    // Unit-level contract: the quad the compositor draws is the rect set by draw()
    // shifted by the pan offset. No GPU needed — the offset is CPU-side geometry.
    QCPColormapRhiLayer layer(nullptr);
    layer.setQuadRect(QRectF(10, 20, 100, 50));
    QCOMPARE(layer.quadRect(), QRectF(10, 20, 100, 50));
    QCOMPARE(layer.effectiveQuadRect(), QRectF(10, 20, 100, 50));

    layer.setPixelOffset(-30, 7);
    QCOMPARE(layer.pixelOffset(), QPointF(-30, 7));
    QCOMPARE(layer.quadRect(), QRectF(10, 20, 100, 50));            // unchanged
    QCOMPARE(layer.effectiveQuadRect(), QRectF(-20, 27, 100, 50));  // shifted

    layer.setPixelOffset(0, 0);
    QCOMPARE(layer.effectiveQuadRect(), QRectF(10, 20, 100, 50));
}

void TestPipeline::colormapQuadFollowsPanWhileTranslating()
{
    // Integration: on a frame where the colormap layer skips its repaint, draw()
    // never runs, so setQuadRect() is never called and the quad holds the PREVIOUS
    // range's pixel rect. The compositor must be handed the layer's pixel offset,
    // or the image stays put while the axes move — colormaps visibly lagging
    // behind line plots during a pan.
    if (!showAndHasRhiLT(mPlot)) QSKIP("No QRhi available — quad geometry needs draw()");

    auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
    const int nx = 4000, ny = 64;
    std::vector<double> x(nx), y(ny), z(static_cast<size_t>(nx) * ny, 1.0);
    for (int i = 0; i < nx; ++i) x[i] = i;
    for (int j = 0; j < ny; ++j) y[j] = j;
    cm->setDataSource(std::make_shared<QCPSoADataSource2D<
        std::vector<double>, std::vector<double>, std::vector<double>>>(
        std::move(x), std::move(y), std::move(z)));

    mPlot->xAxis->setRange(0, 1000);
    mPlot->yAxis->setRange(0, 63);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTRY_VERIFY_WITH_TIMEOUT(!cm->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    auto* crl = cm->rhiLayer();
    QVERIFY(crl);
    const QRectF before = crl->quadRect();
    QVERIFY(!before.isEmpty());

    mPlot->xAxis->setRange(100, 1100); // pure horizontal pan
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QCPLayer* layer = cm->layer();
    if (!layer->canSkipRepaintForTranslation())
        QSKIP("layer repainted instead of translating — nothing to compensate");

    // The repaint was skipped, so the raw quad is stale by construction...
    QCOMPARE(crl->quadRect(), before);
    // ...and the offset must make up the difference.
    const QPointF expected = layer->pixelOffset();
    QVERIFY(!expected.isNull());
    QCOMPARE(crl->pixelOffset(), expected);
    QCOMPARE(crl->effectiveQuadRect(), before.translated(expected));
}

void TestPipeline::plottableOffsetsRefreshedAtRenderTime()
{
    // The graph's GPU translation offsets must track the freshest axis range at
    // render time, not only the range at the last (coalesced) replot — otherwise
    // a range change landing between replot and frame leaves the graph trailing
    // render-time-updated layers (spans, grid) by up to a frame.
    if (!showAndHasRhiLT(mPlot))
        QSKIP("No QRhi available — translation offsets need render frames");

    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    QVector<double> keys(1000), values(1000);
    for (int i = 0; i < 1000; ++i)
    {
        keys[i] = i;
        values[i] = std::sin(i * 0.01);
    }
    graph->setData(std::move(keys), std::move(values));

    mPlot->xAxis->setRange(0, 1000);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTRY_VERIFY_WITH_TIMEOUT(!graph->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QCPLayer* mainLayer = mPlot->layer("main");
    QVERIFY(mainLayer);

    // Pan A with a replot: translation path engages (replot-time offset set).
    mPlot->xAxis->setRange(100, 1100);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    if (!mainLayer->canSkipRepaintForTranslation())
        QSKIP("layer repainted instead of translating — nothing to verify");

    auto* prl = mPlot->plottableRhiLayer(mainLayer);
    QVERIFY(prl);
    const QPointF offsetA = mainLayer->pixelOffset();
    QVERIFY(!offsetA.isNull());
    QCOMPARE(prl->lastUniformOffset(), offsetA);

    // Pan B WITHOUT a replot: the next frame must refresh the offset from the
    // new range anyway.
    mPlot->xAxis->setRange(200, 1200);
    const QPointF expectedB = mainLayer->pixelOffset();
    QVERIFY(!expectedB.isNull());
    QVERIFY(expectedB != offsetA);
    mPlot->update();
    QTRY_VERIFY_WITH_TIMEOUT(prl->lastUniformOffset() == expectedB, 2000);
}
