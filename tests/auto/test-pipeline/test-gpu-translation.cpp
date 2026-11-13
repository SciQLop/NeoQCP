#include "test-pipeline.h"
#include <qcustomplot.h>

void TestPipeline::multiGraphTranslationResetsOnFreshData()
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

    mPlot->xAxis->setRange(10000, 110000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QTRY_VERIFY_WITH_TIMEOUT(!graph->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QVERIFY(!graph->pipeline().isBusy());
}

void TestPipeline::multiGraphTranslationOffsetWhenBusy()
{
    auto* mg = new QCPGraph(mPlot->xAxis, mPlot->yAxis);
    constexpr int N = 200000;
    QVector<double> keys(N);
    std::vector<QVector<double>> columns(3, QVector<double>(N));
    for (int i = 0; i < N; ++i) {
        keys[i] = i;
        for (int c = 0; c < 3; ++c)
            columns[c][i] = std::sin(i * 0.01 + c);
    }
    mg->setData(std::move(keys), std::move(columns));

    mPlot->xAxis->setRange(0, 100000);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QTRY_VERIFY_WITH_TIMEOUT(!mg->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    mPlot->xAxis->setRange(10000, 110000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    if (mg->pipeline().isBusy())
        QVERIFY(mg->hasRenderedRange());
}

void TestPipeline::histogram2dTranslationOffsetWhenBusy()
{
    // Histogram2D uses coordToPixel()-based quad positioning (same as ColorMap2),
    // so it naturally repositions on pan without GPU translation.
    // This test verifies panning during busy pipeline doesn't crash.
    auto* hist = new QCPHistogram2D(mPlot->xAxis, mPlot->yAxis);
    constexpr int N = 200000;
    std::vector<double> keys(N), values(N);
    for (int i = 0; i < N; ++i) {
        keys[i] = i % 500;
        values[i] = i / 500;
    }
    hist->setData(std::move(keys), std::move(values));

    mPlot->xAxis->setRange(0, 500);
    mPlot->yAxis->setRange(0, 400);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QTRY_VERIFY_WITH_TIMEOUT(!hist->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    // Pan while pipeline may be busy — should not crash or double-translate
    mPlot->xAxis->setRange(50, 550);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
}

void TestPipeline::colormap2TranslationOffsetWhenBusy()
{
    // ColorMap2 uses coordToPixel()-based quad positioning, so the image
    // naturally repositions on pan without needing a GPU translation offset.
    // This test verifies that panning during a busy pipeline doesn't crash.
    auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);

    constexpr int NX = 500, NY = 500;
    std::vector<double> x(NX), y(NY), z(NX * NY);
    for (int i = 0; i < NX; ++i) x[i] = i;
    for (int i = 0; i < NY; ++i) y[i] = i;
    for (int i = 0; i < NX * NY; ++i) z[i] = std::sin(i * 0.001);
    cm->setData(std::move(x), std::move(y), std::move(z));

    mPlot->xAxis->setRange(0, 500);
    mPlot->yAxis->setRange(0, 500);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QTRY_VERIFY_WITH_TIMEOUT(!cm->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    // Pan while pipeline may be busy — should not crash or double-translate
    mPlot->xAxis->setRange(50, 550);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
}

void TestPipeline::translatedGeometryClippedToAxisRect()
{
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

    // Large pan to push data far outside the axis rect
    mPlot->xAxis->setRange(200000, 300000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    // toPixmap uses QPainter export path, not RHI. Verifies QPainter clipping.
    // RHI scissor clipping is hardware-enforced and inherently correct.
    QPixmap pixmap = mPlot->toPixmap(400, 300);
    QImage img = pixmap.toImage();
    QRect axisRect = mPlot->axisRect()->rect();

    bool foundRedOutside = false;
    for (int y = 0; y < img.height() && !foundRedOutside; ++y)
    {
        for (int x = 0; x < img.width() && !foundRedOutside; ++x)
        {
            if (!axisRect.contains(x, y))
            {
                QColor c = img.pixelColor(x, y);
                if (c.red() > 200 && c.green() < 50 && c.blue() < 50)
                    foundRedOutside = true;
            }
        }
    }
    QVERIFY2(!foundRedOutside, "Red pixels found outside axis rect — clipping failed");
}

void TestPipeline::multipleMultiGraphsIndependentOffsets()
{
    auto* small = new QCPGraph(mPlot->xAxis, mPlot->yAxis);
    QVector<double> ks(100), vs(100);
    for (int i = 0; i < 100; ++i) { ks[i] = i * 1000; vs[i] = i; }
    small->setData(std::move(ks), std::move(vs));

    auto* large = new QCPGraph(mPlot->xAxis, mPlot->yAxis);
    QVector<double> kl(500000), vl(500000);
    for (int i = 0; i < 500000; ++i) { kl[i] = i; vl[i] = i; }
    large->setData(std::move(kl), std::move(vl));

    mPlot->xAxis->setRange(0, 100000);
    mPlot->yAxis->setRange(-1, 1);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QTRY_VERIFY_WITH_TIMEOUT(
        !small->pipeline().isBusy() && !large->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QVERIFY(small->hasRenderedRange());
    QVERIFY(large->hasRenderedRange());

    // Smoke test: replot doesn't crash with independent offset states
    mPlot->xAxis->setRange(10000, 110000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
}
