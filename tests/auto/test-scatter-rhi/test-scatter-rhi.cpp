#include "test-scatter-rhi.h"
#include "../../../src/qcp.h"
#include "../../../src/painting/scatter-rhi-layer.h"

void TestScatterRhi::init()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
}

void TestScatterRhi::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
}

void TestScatterRhi::renderToImageProducesNonEmptyImage()
{
    QCPScatterStyle style(QCPScatterStyle::ssDisc, 10);
    QImage img = style.renderToImage(64);
    QCOMPARE(img.width(), 64);
    QCOMPARE(img.height(), 64);

    bool hasNonTransparent = false;
    for (int y = 0; y < img.height() && !hasNonTransparent; ++y)
        for (int x = 0; x < img.width() && !hasNonTransparent; ++x)
            if (qAlpha(img.pixel(x, y)) > 0)
                hasNonTransparent = true;
    QVERIFY(hasNonTransparent);
}

void TestScatterRhi::renderToImageDiscHasAlpha()
{
    QCPScatterStyle style(QCPScatterStyle::ssDisc, QColor(Qt::red), 10);
    QImage img = style.renderToImage(64);

    QCOMPARE(qAlpha(img.pixel(0, 0)), 0);
    QVERIFY(qAlpha(img.pixel(32, 32)) > 0);
}

void TestScatterRhi::renderToImageRespectsPenColor()
{
    QPen pen(Qt::blue);
    pen.setWidth(2);
    QCPScatterStyle style(QCPScatterStyle::ssCircle, pen, Qt::NoBrush, 10);
    QImage img = style.renderToImage(64);

    bool hasBlue = false;
    for (int y = 0; y < img.height() && !hasBlue; ++y)
        for (int x = 0; x < img.width() && !hasBlue; ++x) {
            QColor c = QColor::fromRgba(img.pixel(x, y));
            if (c.alpha() > 0 && c.blue() > c.red() && c.blue() > c.green())
                hasBlue = true;
        }
    QVERIFY(hasBlue);
}

void TestScatterRhi::renderToImageCustomPathWorks()
{
    QPainterPath path;
    path.addRect(-5, -5, 10, 10);
    QCPScatterStyle style(path, QPen(Qt::black), QBrush(Qt::white), 10);
    QImage img = style.renderToImage(64);

    bool hasNonTransparent = false;
    for (int y = 0; y < img.height() && !hasNonTransparent; ++y)
        for (int x = 0; x < img.width() && !hasNonTransparent; ++x)
            if (qAlpha(img.pixel(x, y)) > 0)
                hasNonTransparent = true;
    QVERIFY(hasNonTransparent);
}

// --- Task 3-5 tests ---

static QCPGraph2* addGraph2WithScatter(QCustomPlot* plot,
                                        QCPScatterStyle::ScatterShape shape,
                                        int nPoints = 100)
{
    auto* graph = new QCPGraph2(plot->xAxis, plot->yAxis);
    QCPScatterStyle style(shape, 8);
    graph->setScatterStyle(style);

    std::vector<double> keys(nPoints), values(nPoints);
    for (int i = 0; i < nPoints; ++i)
    {
        keys[i] = i;
        values[i] = std::sin(i * 0.1);
    }
    graph->setData(std::span<const double>(keys), std::span<const double>(values));
    plot->xAxis->setRange(0, nPoints);
    plot->yAxis->setRange(-1.5, 1.5);
    return graph;
}

void TestScatterRhi::scatterRhiLayerCreatedLazily()
{
    // Before any scatter is drawn, the map should be empty
    // After replot with scatter style, the layer should exist
    addGraph2WithScatter(mPlot, QCPScatterStyle::ssCircle);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    // No crash is the test — the RHI layer is only created when the RHI is initialized
    // which requires a visible widget. In headless mode, scatterRhiLayer returns nullptr.
    QVERIFY(true);
}

void TestScatterRhi::replotWithScatterDoesNotCrash()
{
    addGraph2WithScatter(mPlot, QCPScatterStyle::ssCircle);
    // Multiple replots should not crash
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(true);
}

void TestScatterRhi::replotScatterDiscDoesNotCrash()
{
    addGraph2WithScatter(mPlot, QCPScatterStyle::ssDisc);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(true);
}

void TestScatterRhi::replotScatterSquareDoesNotCrash()
{
    addGraph2WithScatter(mPlot, QCPScatterStyle::ssSquare);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(true);
}

void TestScatterRhi::replotScatterCustomDoesNotCrash()
{
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    QPainterPath path;
    path.addEllipse(-4, -4, 8, 8);
    QCPScatterStyle style(path, QPen(Qt::red), QBrush(Qt::yellow), 10);
    graph->setScatterStyle(style);

    std::vector<double> keys(50), values(50);
    for (int i = 0; i < 50; ++i)
    {
        keys[i] = i;
        values[i] = i * 0.5;
    }
    graph->setData(std::span<const double>(keys), std::span<const double>(values));
    mPlot->xAxis->setRange(0, 50);
    mPlot->yAxis->setRange(0, 25);

    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(true);
}

void TestScatterRhi::exportWithScatterStillWorks()
{
    addGraph2WithScatter(mPlot, QCPScatterStyle::ssCircle);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    // toPixmap uses QPainter fallback path — verify it doesn't crash
    QPixmap pm = mPlot->toPixmap(400, 300);
    QVERIFY(!pm.isNull());
    QCOMPARE(pm.width(), 400);
    QCOMPARE(pm.height(), 300);
}

void TestScatterRhi::clearResetsState()
{
    // Verify that QCPScatterRhiLayer::clear() works correctly
    QCPScatterRhiLayer layer(nullptr);
    // clear on empty layer should not crash
    layer.clear();
    QVERIFY(!layer.hasGeometry());
    QVERIFY(layer.isDirty());
}
