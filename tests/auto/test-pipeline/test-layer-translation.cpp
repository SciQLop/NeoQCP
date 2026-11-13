#include "test-pipeline.h"
#include <qcustomplot.h>
#include <painting/draw-commands.h>
#include <painting/plottable-rhi-layer.h>
#include <QtWidgets/qtestsupport_widgets.h> // QTest::qWaitForWindowExposed

namespace {
// GPU layers keep offsets as float shader uniforms; the layer computes them in double.
QPointF asUniform(QPointF offset)
{
    return {static_cast<float>(offset.x()), static_cast<float>(offset.y())};
}

bool showAndHasRhiLT(QCustomPlot* plot)
{
    plot->show();
    if (!QTest::qWaitForWindowExposed(plot))
        return false;
    QCoreApplication::processEvents();
    return plot->rhi() != nullptr;
}
} // namespace

void TestPipeline::stallPixelOffsetMultiGraphBusy()
{
    auto* graph = new QCPGraph(mPlot->xAxis, mPlot->yAxis);
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
    auto* graph = new QCPGraph(mPlot->xAxis, mPlot->yAxis);
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
    auto* graph = new QCPGraph(mPlot->xAxis, mPlot->yAxis);
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
    auto* graph = new QCPGraph(mPlot->xAxis, mPlot->yAxis);
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
    auto* graph = new QCPGraph(mPlot->xAxis, mPlot->yAxis);
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

void TestPipeline::existingMultiGraphTranslationUnaffected()
{
    auto* graph = new QCPGraph(mPlot->xAxis, mPlot->yAxis);
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
    QCOMPARE(qcp::pointers(mPlot->layer("main")->children()).contains(colorScale), true);

    auto* graph = new QCPGraph(mPlot->xAxis, mPlot->yAxis);
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

    auto* graph = new QCPGraph(mPlot->xAxis, mPlot->yAxis);
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

void TestPipeline::colormapQuadFollowsPanWhileTranslating()
{
    // Integration: on a frame where the layer skips its repaint, draw() never runs. The image
    // command already carries the grid's data corners, so the compositor's pixel offset moves
    // it with the axes: the recording is reused, not rebuilt, and nothing lags.
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

    const quint64 recordings = cm->mCore.recordingCount();
    const auto corners = [&]
    {
        const auto* image = std::get_if<qcp::ImageCmd>(&cm->mCore.recordedList().commands.front());
        if (!image) return QRectF();
        const auto* a = std::get_if<QCP::Data>(&image->a);
        const auto* b = std::get_if<QCP::Data>(&image->b);
        if (!a || !b) return QRectF();
        return QRectF(QPointF(a->x, a->y), QPointF(b->x, b->y)).normalized();
    };
    const QRectF before = corners();
    QVERIFY(!before.isEmpty());

    mPlot->xAxis->setRange(100, 1100); // pure horizontal pan
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QCPLayer* layer = cm->layer();
    if (!layer->canSkipRepaintForTranslation())
        QSKIP("layer repainted instead of translating — nothing to compensate");

    // The repaint was skipped: the recording is the same one, and the layer's offset moves it.
    QCOMPARE(cm->mCore.recordingCount(), recordings);
    QCOMPARE(corners(), before);
    const QPointF expected = asUniform(layer->pixelOffset());
    QVERIFY2(!expected.isNull(), "the layer must translate the image with the axes");
}

void TestPipeline::plottableOffsetsRefreshedAtRenderTime()
{
    // The graph's GPU translation offsets must track the freshest axis range at
    // render time, not only the range at the last (coalesced) replot — otherwise
    // a range change landing between replot and frame leaves the graph trailing
    // render-time-updated layers (spans, grid) by up to a frame.
    if (!showAndHasRhiLT(mPlot))
        QSKIP("No QRhi available — translation offsets need render frames");

    auto* graph = new QCPGraph(mPlot->xAxis, mPlot->yAxis);
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
    QCOMPARE(prl->lastUniformOffset(), asUniform(offsetA));

    // Pan B WITHOUT a replot: the next frame must refresh the offset from the
    // new range anyway.
    mPlot->xAxis->setRange(200, 1200);
    const QPointF expectedB = mainLayer->pixelOffset();
    QVERIFY(!expectedB.isNull());
    QVERIFY(expectedB != offsetA);
    mPlot->update();
    QTRY_VERIFY_WITH_TIMEOUT(prl->lastUniformOffset() == asUniform(expectedB), 2000);
}

void TestPipeline::visibilityToggleForcesLayerRepaint()
{
    // After a pan the layer may repaint by translating its old GPU entries. Showing a
    // graph again must not take that shortcut: its entries were not kept while it was
    // hidden, so a translated layer would stay empty until the next real repaint.
    auto* graph = new QCPGraph(mPlot->xAxis, mPlot->yAxis);
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

    mPlot->xAxis->setRange(50, 1050);
    QVERIFY(mainLayer->canSkipRepaintForTranslation());
    graph->setVisible(false);
    QVERIFY2(!mainLayer->canSkipRepaintForTranslation(), "hiding must repaint, not translate");

    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    mPlot->xAxis->setRange(100, 1100);
    graph->setVisible(true);
    QVERIFY2(!mainLayer->canSkipRepaintForTranslation(), "showing must repaint, not translate");
}

void TestPipeline::pannedReuseKeepsBarsUnderTheirKeys()
{
    // A pan that repaints the layer hands the bars' kept recording a shift of its own. The next
    // clean pan translates the layer again: that shift must add to the one the bars' vertices were
    // built with, not replace it.
    if (!showAndHasRhiLT(mPlot))
        QSKIP("No QRhi available — the pan reuse path needs GPU layers");
    mPlot->xAxis->grid()->setVisible(false);
    mPlot->yAxis->grid()->setVisible(false);

    auto* bars = new QCPBars(mPlot->xAxis, mPlot->yAxis);
    bars->setWidthType(QCPBars::wtPlotCoords);
    bars->setWidth(6.0);
    bars->setPen(Qt::NoPen);
    bars->setBrush(QColor(0, 0, 255));
    bars->setData(QVector<double> { 50.0 }, QVector<double> { 0.1 });

    auto* graph = new QCPGraph(mPlot->xAxis, mPlot->yAxis);
    graph->setPen(QPen(QColor(200, 0, 0), 2));
    QVector<double> keys(101), values(101);
    for (int i = 0; i < 101; ++i)
    {
        keys[i] = i;
        values[i] = 0.9 + 0.02 * std::sin(i * 0.3);
    }
    graph->setData(std::move(keys), std::move(values));

    // The ink of the bar at its key, in the screen's own pixels.
    const auto barInk = [this] {
        const QImage frame = mPlot->grabFramebuffer();
        const double dpr = frame.devicePixelRatio();
        return frame.pixelColor(qRound(mPlot->xAxis->coordToPixel(50.0) * dpr),
                                qRound(mPlot->yAxis->coordToPixel(0.05) * dpr));
    };
    const auto isBarInk = [](const QColor& colour) {
        return colour.blue() > 200 && colour.red() < 80 && colour.green() < 80;
    };

    mPlot->xAxis->setRange(0, 100);
    mPlot->yAxis->setRange(0, 1);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTRY_VERIFY_WITH_TIMEOUT(!graph->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QCPLayer* mainLayer = mPlot->layer("main");
    QVERIFY(mainLayer);
    QVERIFY2(isBarInk(barInk()), "the bar must be drawn under its key");

    // Frame B: the pan repaints the layer because the graph's lines are remade. The bars keep
    // their recording and are shifted by this pan's own delta, so they stay under their keys.
    mPlot->xAxis->setRange(10, 110);
    graph->setLineStyle(QCPGraph::lsStepLeft);
    QVERIFY2(!mainLayer->canSkipRepaintForTranslation(), "fresh lines must repaint the layer");
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY2(isBarInk(barInk()), "the reused bars must follow the pan that repainted the layer");

    // Frame C: a clean pan, translated instead of repainted.
    mPlot->xAxis->setRange(20, 120);
    QVERIFY2(mainLayer->canSkipRepaintForTranslation(), "the clean pan must translate the layer");
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    const QColor inked = barInk();
    QVERIFY2(isBarInk(inked),
             qPrintable(QString("the bar must stay under its key after the pans, found %1")
                            .arg(inked.name())));
}
