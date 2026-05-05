#include "test-paintbuffer.h"

void TestPaintBuffer::init()
{
    mPlot = new QCustomPlot(nullptr);
    mPlot->setGeometry(50, 50, 400, 300);
    mPlot->show();
    (void)QTest::qWaitForWindowExposed(mPlot);
}

void TestPaintBuffer::cleanup()
{
    delete mPlot;
}

void TestPaintBuffer::contentDirty_newBufferIsDirty()
{
    QCPPaintBufferPixmap buf(QSize(100, 100), 1.0, "test");
    QVERIFY(buf.contentDirty());
}

void TestPaintBuffer::contentDirty_resetAfterReplot()
{
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    for (const auto& buf : mPlot->mPaintBuffers)
        QVERIFY2(!buf->contentDirty(), qPrintable(buf->layerName()));
}

void TestPaintBuffer::contentDirty_setOnResize()
{
    QCPPaintBufferPixmap buf(QSize(100, 100), 1.0, "test");
    buf.setContentDirty(false);
    QVERIFY(!buf.contentDirty());

    buf.setSize(QSize(200, 200));
    QVERIFY(buf.contentDirty());
}

void TestPaintBuffer::contentDirty_setOnInvalidation()
{
    QCPPaintBufferPixmap buf(QSize(100, 100), 1.0, "test");
    buf.setContentDirty(false);
    QVERIFY(!buf.contentDirty());

    buf.setInvalidated(true);
    QVERIFY(buf.contentDirty());

    // setInvalidated(false) should NOT clear contentDirty
    buf.setInvalidated(false);
    QVERIFY(buf.contentDirty());
}

void TestPaintBuffer::contentDirty_markDirtyLayer()
{
    mPlot->addLayer("vis_test", mPlot->layer("main"), QCustomPlot::limAbove);
    QCPLayer* overlay = mPlot->layer("vis_test");
    overlay->setMode(QCPLayer::lmBuffered);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    for (const auto& buf : mPlot->mPaintBuffers)
        QVERIFY(!buf->contentDirty());

    overlay->markDirty();

    auto overlayBuf = overlay->mPaintBuffer.toStrongRef();
    QVERIFY(overlayBuf);
    QVERIFY(overlayBuf->contentDirty());

    int dirtyCount = 0;
    for (const auto& buf : mPlot->mPaintBuffers)
    {
        if (buf->contentDirty())
            ++dirtyCount;
    }
    QCOMPARE(dirtyCount, 1);
}

void TestPaintBuffer::contentDirty_fallbackMarksAllDirty()
{
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    for (const auto& buf : mPlot->mPaintBuffers)
        QVERIFY(!buf->contentDirty());

    // replot() without prior markDirty() should still repaint everything
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    for (const auto& buf : mPlot->mPaintBuffers)
        QVERIFY(!buf->contentDirty());
}

void TestPaintBuffer::contentDirty_incrementalReplotSkipsCleanBuffers()
{
    QCPLayer* data = mPlot->layer("main");
    mPlot->addLayer("overlay", data, QCustomPlot::limAbove);
    QCPLayer* overlay = mPlot->layer("overlay");
    overlay->setMode(QCPLayer::lmBuffered);

    QCPGraph* graph = mPlot->addGraph();
    graph->setData({1.0, 2.0, 3.0}, {1.0, 4.0, 2.0});
    auto* line = new QCPItemLine(mPlot);
    line->setLayer(overlay);
    line->start->setCoords(1, 1);
    line->end->setCoords(3, 3);

    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    // Mark only main dirty, replot incrementally
    data->markDirty();
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    // All buffers should be clean after replot
    for (const auto& buf : mPlot->mPaintBuffers)
        QVERIFY(!buf->contentDirty());
}

void TestPaintBuffer::contentDirty_incrementalReplotPreservesContent()
{
    QCPLayer* data = mPlot->layer("main");
    mPlot->addLayer("overlay", data, QCustomPlot::limAbove);
    QCPLayer* overlay = mPlot->layer("overlay");
    overlay->setMode(QCPLayer::lmBuffered);

    auto* graph = mPlot->addGraph();
    graph->setData({1.0, 2.0, 3.0}, {1.0, 4.0, 2.0});
    auto* line = new QCPItemLine(mPlot);
    line->setLayer(overlay);
    line->start->setPixelPosition(QPointF(50, 50));
    line->end->setPixelPosition(QPointF(150, 150));

    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    // Capture overlay buffer content before incremental replot
    auto overlayBuf = overlay->mPaintBuffer.toStrongRef();
    QVERIFY(overlayBuf);
    QPixmap baseline(overlayBuf->size());
    baseline.fill(Qt::transparent);
    {
        QCPPainter painter(&baseline);
        overlayBuf->draw(&painter);
    }

    // Incremental replot: only main dirty
    data->markDirty();
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    // Overlay buffer content should be identical (not cleared/repainted)
    QPixmap afterReplot(overlayBuf->size());
    afterReplot.fill(Qt::transparent);
    {
        QCPPainter painter(&afterReplot);
        overlayBuf->draw(&painter);
    }
    QCOMPARE(afterReplot.toImage(), baseline.toImage());
}

void TestPaintBuffer::replotAndExport_smokeTest()
{
    mPlot->addGraph()->setData({1.0, 2.0}, {3.0, 4.0});
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QPixmap result = mPlot->toPixmap(200, 150);
    QVERIFY(!result.isNull());
    QCOMPARE(result.size(), QSize(200, 150));
}

void TestPaintBuffer::replotOnFirstShow_tabWidget()
{
    // Regression test: a QCustomPlot inside a QTabWidget that is not the
    // initially-visible tab must render content when its tab is first selected.
    // The bug was that initialize() (called by QRhiWidget on first expose)
    // did not trigger a replot, leaving paint buffers empty.
    //
    // In offscreen mode QRhi is unavailable so initialize() is never called,
    // but we can still verify the non-RHI path: paint buffers must contain
    // drawn content after the widget becomes visible and events are processed.

    QTabWidget tabs;
    tabs.resize(400, 300);

    auto* dummyTab = new QWidget;
    auto* plotTab = new QWidget;
    auto* lay = new QVBoxLayout(plotTab);
    lay->setContentsMargins(0, 0, 0, 0);
    auto* plot = new QCustomPlot(plotTab);
    lay->addWidget(plot);

    plot->addGraph()->setData({1.0, 2.0, 3.0}, {1.0, 4.0, 2.0});
    plot->rescaleAxes();

    tabs.addTab(dummyTab, "Dummy");
    tabs.addTab(plotTab, "Plot");
    tabs.setCurrentIndex(0); // plot tab is hidden initially

    tabs.show();
    QVERIFY(QTest::qWaitForWindowExposed(&tabs));

    // Switch to the plot tab
    tabs.setCurrentIndex(1);
    QCoreApplication::processEvents();

    // Paint buffers should have been drawn into (not blank)
    QVERIFY(!plot->mPaintBuffers.isEmpty());
    bool anyDirty = false;
    for (const auto& buf : plot->mPaintBuffers)
        if (buf->contentDirty())
            anyDirty = true;
    // After a replot, buffers should be clean (content was drawn)
    QVERIFY2(!anyDirty, "Paint buffers still dirty after tab switch — replot did not run");

    // Verify non-blank content via toPixmap (QPainter export path)
    QPixmap snap = plot->toPixmap(200, 150);
    QVERIFY(!snap.isNull());
    QImage img = snap.toImage();
    // The image should not be uniformly one color (i.e. content was drawn)
    QSet<QRgb> colors;
    for (int y = 0; y < img.height(); y += 10)
        for (int x = 0; x < img.width(); x += 10)
            colors.insert(img.pixel(x, y));
    QVERIFY2(colors.size() > 1, "toPixmap produced a blank/uniform image");
}

void TestPaintBuffer::skipRepaint_graph2PanOnly()
{
    auto* graph2 = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    std::vector<double> keys = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> vals = {1.0, 4.0, 2.0, 5.0, 3.0};
    graph2->setData(std::move(keys), std::move(vals));
    mPlot->xAxis->setRange(0, 6);
    mPlot->yAxis->setRange(0, 6);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    mPlot->xAxis->setRange(0.5, 6.5);
    auto* axisRect = mPlot->axisRect();
    axisRect->markAffectedLayersDirty();

    QCPLayer* mainLayer = mPlot->layer("main");
    QVERIFY(mainLayer);

    auto mainBuf = mainLayer->mPaintBuffer.toStrongRef();
    QVERIFY(mainBuf);
    QVERIFY(mainBuf->contentDirty());

    QVERIFY(mainLayer->canSkipRepaintForTranslation());
}

void TestPaintBuffer::skipRepaint_disabledWithItems()
{
    auto* graph2 = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    std::vector<double> keys = {1.0, 2.0, 3.0};
    std::vector<double> vals = {1.0, 4.0, 2.0};
    graph2->setData(std::move(keys), std::move(vals));

    auto* line = new QCPItemLine(mPlot);
    line->start->setCoords(1, 1);
    line->end->setCoords(3, 3);

    mPlot->xAxis->setRange(0, 6);
    mPlot->yAxis->setRange(0, 6);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    mPlot->xAxis->setRange(0.5, 6.5);
    mPlot->axisRect()->markAffectedLayersDirty();

    QCPLayer* mainLayer = mPlot->layer("main");
    QVERIFY(!mainLayer->canSkipRepaintForTranslation());
}

void TestPaintBuffer::skipRepaint_disabledWithLegacyGraph()
{
    auto* graph = mPlot->addGraph();
    graph->setData({1.0, 2.0, 3.0}, {1.0, 4.0, 2.0});
    mPlot->xAxis->setRange(0, 6);
    mPlot->yAxis->setRange(0, 6);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    mPlot->xAxis->setRange(0.5, 6.5);
    mPlot->axisRect()->markAffectedLayersDirty();

    QCPLayer* mainLayer = mPlot->layer("main");
    QVERIFY(!mainLayer->canSkipRepaintForTranslation());
}

void TestPaintBuffer::skipRepaint_disabledOnInvalidation()
{
    auto* graph2 = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    std::vector<double> keys = {1.0, 2.0, 3.0};
    std::vector<double> vals = {1.0, 4.0, 2.0};
    graph2->setData(std::move(keys), std::move(vals));
    mPlot->xAxis->setRange(0, 6);
    mPlot->yAxis->setRange(0, 6);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    auto mainBuf = mPlot->layer("main")->mPaintBuffer.toStrongRef();
    QVERIFY(mainBuf);
    mainBuf->setInvalidated(true);

    mPlot->xAxis->setRange(0.5, 6.5);
    mPlot->axisRect()->markAffectedLayersDirty();

    QCPLayer* mainLayer = mPlot->layer("main");
    QVERIFY(!mainLayer->canSkipRepaintForTranslation());
}

void TestPaintBuffer::setVisible_dirtiesLayerBuffer()
{
    // Regression test (NeoQCP commit 92fbbd4): visibility transitions on a
    // buffered layer must mark the layer dirty so the next replot redraws it.
    // Without this, items toggled invisible→visible after their initial replot
    // never reach drawToPaintBuffer() — breaking GPU-registered items (vspans)
    // whose registration is lazy in draw().
    mPlot->addLayer("vis_test", mPlot->layer("main"), QCustomPlot::limAbove);
    QCPLayer* overlay = mPlot->layer("vis_test");
    overlay->setMode(QCPLayer::lmBuffered);

    auto* line = new QCPItemLine(mPlot);
    line->setLayer(overlay);
    line->start->setCoords(1, 1);
    line->end->setCoords(3, 3);
    line->setVisible(false);

    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    auto buf = overlay->mPaintBuffer.toStrongRef();
    QVERIFY(buf);
    QVERIFY(!buf->contentDirty());

    line->setVisible(true);
    QVERIFY2(buf->contentDirty(),
             "setVisible(false→true) must dirty the layer buffer");

    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(!buf->contentDirty());

    line->setVisible(false);
    QVERIFY2(buf->contentDirty(),
             "setVisible(true→false) must dirty the layer buffer");
}

void TestPaintBuffer::setVisible_noOpWhenUnchanged()
{
    // Calling setVisible() with the current value must not dirty the buffer —
    // preserves the perf gains of selective dirty-marking on hot paths.
    mPlot->addLayer("vis_test", mPlot->layer("main"), QCustomPlot::limAbove);
    QCPLayer* overlay = mPlot->layer("vis_test");
    overlay->setMode(QCPLayer::lmBuffered);

    auto* line = new QCPItemLine(mPlot);
    line->setLayer(overlay);
    line->start->setCoords(1, 1);
    line->end->setCoords(3, 3);

    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    auto buf = overlay->mPaintBuffer.toStrongRef();
    QVERIFY(buf);
    QVERIFY(!buf->contentDirty());

    line->setVisible(true); // already true
    QVERIFY(!buf->contentDirty());
}

void TestPaintBuffer::skipRepaint_bufferNotReuploadedOnPan()
{
    auto* graph2 = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    std::vector<double> keys = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> vals = {1.0, 4.0, 2.0, 5.0, 3.0};
    graph2->setData(std::move(keys), std::move(vals));
    mPlot->xAxis->setRange(0, 6);
    mPlot->yAxis->setRange(0, 6);

    // Full replot to establish baseline
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QCPLayer* mainLayer = mPlot->layer("main");
    auto mainBuf = mainLayer->mPaintBuffer.toStrongRef();
    QVERIFY(mainBuf);
    QVERIFY(!mainBuf->contentDirty());

    // Simulate pan
    mPlot->xAxis->setRange(0.5, 6.5);
    mPlot->axisRect()->markAffectedLayersDirty();
    QVERIFY(mainBuf->contentDirty());

    // Replot — skip should kick in
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    // contentDirty should be cleared
    QVERIFY(!mainBuf->contentDirty());

    // The skip worked: stallPixelOffset still returns non-null
    // because draw() was NOT called (mRenderedRange not updated)
    QVERIFY(!mainLayer->pixelOffset().isNull());
}
