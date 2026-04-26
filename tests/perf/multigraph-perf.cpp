// Headless perf scenarios for QCPMultiGraph hot paths.
// Run with `perf record` / `perf stat` from outside the container.
//
// Usage: multigraph_perf <scenario> [iterations]
//
// Scenarios:
//   l1_resampling   — L1 bin-min-max (the heavy async stage)
//   l2_resampling   — L2 viewport rebinning (sync, per-zoom)
//   full_replot     — full replot cycle (data→pixel→extrusion→composite)
//   pan_replot      — cached-line pan replot (GPU translate path)
//   adaptive        — getOptimizedLineData (adaptive sampling)
//   data_setup      — data source construction + preview build

#include <qcustomplot.h>
#include <datasource/resampled-multi-datasource.h>
#include <QApplication>
#include <QElapsedTimer>
#include <csignal>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

// Pause the process so perf can attach *after* warmup completes.
// The python wrapper sends SIGCONT to resume.
// When --no-barrier is passed, this is a no-op for quick local runs.
static bool gUseBarrier = true;

static void waitForProfiler()
{
    if (!gUseBarrier) return;
    fprintf(stderr, "  [ready — waiting for SIGCONT]\n");
    fflush(stderr);
    raise(SIGSTOP);
}

// ── Data generation ─────────────────────────────────────────────

struct TestData {
    std::vector<double> keys;
    std::vector<std::vector<double>> columns;
};

static TestData generateMultiData(int nPoints, int nCols)
{
    TestData d;
    d.keys.resize(nPoints);
    d.columns.resize(nCols);
    for (auto& col : d.columns)
        col.resize(nPoints);

    for (int i = 0; i < nPoints; ++i) {
        double t = i * 1e-6;
        d.keys[i] = t;
        for (int c = 0; c < nCols; ++c) {
            double phase = c * 0.7;
            d.columns[c][i] = std::sin(t * 6.28 * 0.5 + phase)
                             + 0.3 * std::sin(t * 6.28 * 50.0 + phase)
                             + 0.1 * std::sin(t * 6.28 * 5000.0 + phase);
        }
    }
    return d;
}

// ── Plot setup with QRhi initialization ─────────────────────────

struct PlotFixture {
    QCustomPlot* plot;
    QCPMultiGraph* mg;
};

static PlotFixture setupPlotWithRhi(TestData& data)
{
    auto* plot = new QCustomPlot();
    plot->resize(1920, 1080);

    auto* mg = new QCPMultiGraph(plot->xAxis, plot->yAxis);
    {
        std::vector<std::vector<double>> cols;
        cols.reserve(static_cast<int>(data.columns.size()));
        for (auto& col : data.columns)
            cols.push_back(std::move(col));
        mg->setData(std::move(data.keys), std::move(cols));
    }
    plot->rescaleAxes();

    // Show the widget so QRhiWidget::initialize() fires and creates the QRhi context
    plot->show();
    // Pump the event loop until QRhi is ready (or timeout after 5s)
    QElapsedTimer initTimer;
    initTimer.start();
    while (!plot->rhi() && initTimer.elapsed() < 5000)
        QApplication::processEvents(QEventLoop::AllEvents, 50);

    // Wait for async L1 pipeline to complete
    for (int w = 0; w < 100 && mg->pipeline().isBusy(); ++w) {
        QThread::msleep(50);
        QApplication::processEvents();
    }
    // First full replot with L2
    plot->replot(QCustomPlot::rpImmediateRefresh);
    QApplication::processEvents();

    return {plot, mg};
}

// ── Scenarios ───────────────────────────────────────────────────

static constexpr int kDefaultPoints = 10'000'000;
static constexpr int kDefaultCols = 8;

static void scenarioL1Resampling(int iters)
{
    auto data = generateMultiData(kDefaultPoints, kDefaultCols);

    // Build a SoA multi data source
    std::vector<std::span<const double>> spans;
    spans.reserve(kDefaultCols);
    for (auto& col : data.columns)
        spans.emplace_back(col.data(), col.size());

    auto src = std::make_shared<QCPSoAMultiDataSource<
        std::span<const double>, std::span<const double>>>(
        std::span<const double>(data.keys), std::move(spans));

    QCPRange fullRange(data.keys.front(), data.keys.back());
    int numBins = qcp::algo::kLevel1TargetBins;

    fprintf(stderr, "l1_resampling: %d pts × %d cols, %d bins, %d iters\n",
            kDefaultPoints, kDefaultCols, numBins, iters);
    waitForProfiler();

    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < iters; ++i) {
        auto result = qcp::algo::binMinMaxMultiParallel(
            *src, 0, src->size(), fullRange, numBins);
        // Prevent optimizer from eliding
        if (result.keys.empty()) abort();
    }
    double ms = timer.nsecsElapsed() / 1e6;
    fprintf(stderr, "  total: %.1f ms, per-iter: %.1f ms\n", ms, ms / iters);
}

static void scenarioL2Resampling(int iters)
{
    auto data = generateMultiData(kDefaultPoints, kDefaultCols);

    std::vector<std::span<const double>> spans;
    for (auto& col : data.columns)
        spans.emplace_back(col.data(), col.size());

    auto src = std::make_shared<QCPSoAMultiDataSource<
        std::span<const double>, std::span<const double>>>(
        std::span<const double>(data.keys), std::move(spans));

    // Build L1 cache first
    QCPRange fullRange(data.keys.front(), data.keys.back());
    std::any cache;
    auto l1result = qcp::algo::buildL1CacheMulti(*src, ViewportParams{}, cache);
    auto* l1Cache = std::any_cast<qcp::algo::MultiGraphResamplerCache>(&cache);
    if (!l1Cache || l1Cache->level1.keys.empty()) {
        fprintf(stderr, "l2_resampling: L1 cache build failed\n");
        return;
    }

    // Simulate viewport at ~50% zoom (middle half of data)
    double mid = (fullRange.lower + fullRange.upper) * 0.5;
    double halfSpan = fullRange.size() * 0.25;
    ViewportParams vp;
    vp.keyRange = QCPRange(mid - halfSpan, mid + halfSpan);
    vp.plotWidthPx = 1920;
    vp.valueLogScale = false;

    fprintf(stderr, "l2_resampling: L1=%d bins, viewport 50%%, %d px, %d iters\n",
            qcp::algo::kLevel1TargetBins, vp.plotWidthPx, iters);
    waitForProfiler();

    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < iters; ++i) {
        auto l2 = qcp::algo::resampleL2Multi(*l1Cache, vp);
        if (!l2) abort();
    }
    double ms = timer.nsecsElapsed() / 1e6;
    fprintf(stderr, "  total: %.1f ms, per-iter: %.1f ms\n", ms, ms / iters);
}

static void scenarioAdaptive(int iters)
{
    auto data = generateMultiData(kDefaultPoints, kDefaultCols);

    std::vector<std::span<const double>> spans;
    for (auto& col : data.columns)
        spans.emplace_back(col.data(), col.size());

    auto src = std::make_shared<QCPSoAMultiDataSource<
        std::span<const double>, std::span<const double>>>(
        std::span<const double>(data.keys), std::move(spans));

    QCustomPlot plot;
    plot.resize(1920, 1080);
    plot.xAxis->setRange(data.keys.front(), data.keys.back());
    plot.yAxis->setRange(-1.5, 1.5);
    plot.show();
    QApplication::processEvents(QEventLoop::AllEvents, 100);

    int begin = 0;
    int end = src->size();
    int pixelWidth = 1920;

    fprintf(stderr, "adaptive: %d pts × %d cols, %d px, %d iters\n",
            kDefaultPoints, kDefaultCols, pixelWidth, iters);
    waitForProfiler();

    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < iters; ++i) {
        for (int c = 0; c < kDefaultCols; ++c) {
            auto lines = src->getOptimizedLineData(
                c, begin, end, pixelWidth,
                plot.xAxis, plot.yAxis);
            if (lines.isEmpty()) abort();
        }
    }
    double ms = timer.nsecsElapsed() / 1e6;
    fprintf(stderr, "  total: %.1f ms, per-iter: %.1f ms\n", ms, ms / iters);
}

static void scenarioAdaptiveMulti(int iters)
{
    auto data = generateMultiData(kDefaultPoints, kDefaultCols);

    std::vector<std::span<const double>> spans;
    spans.reserve(data.columns.size());
    for (const auto& col : data.columns)
        spans.emplace_back(col.data(), col.size());

    auto src = std::make_shared<QCPSoAMultiDataSource<
        std::span<const double>, std::span<const double>>>(
        std::span<const double>(data.keys), std::move(spans));

    QCustomPlot plot;
    plot.resize(1920, 1080);
    plot.xAxis->setRange(data.keys.front(), data.keys.back());
    plot.yAxis->setRange(-1.5, 1.5);
    plot.show();
    QApplication::processEvents(QEventLoop::AllEvents, 100);

    int begin = 0;
    int end = src->size();
    int pixelWidth = 1920;

    fprintf(stderr, "adaptive_multi: %d pts × %d cols, %d px, %d iters\n",
            kDefaultPoints, kDefaultCols, pixelWidth, iters);
    waitForProfiler();

    QVector<QVector<QPointF>> results(kDefaultCols);
    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < iters; ++i) {
        src->getOptimizedLineDataAll(begin, end, pixelWidth,
                                      plot.xAxis, plot.yAxis,
                                      results.data(), kDefaultCols);
        if (results[0].isEmpty()) abort();
    }
    double ms = timer.nsecsElapsed() / 1e6;
    fprintf(stderr, "  total: %.1f ms, per-iter: %.1f ms\n", ms, ms / iters);
}

static void scenarioDataSetup(int iters)
{
    fprintf(stderr, "data_setup: %d pts × %d cols, %d iters\n",
            kDefaultPoints, kDefaultCols, iters);
    waitForProfiler();

    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < iters; ++i) {
        auto data = generateMultiData(kDefaultPoints, kDefaultCols);

        std::vector<std::span<const double>> spans;
        for (auto& col : data.columns)
            spans.emplace_back(col.data(), col.size());

        auto src = std::make_shared<QCPSoAMultiDataSource<
            std::span<const double>, std::span<const double>>>(
            std::span<const double>(data.keys), std::move(spans));
        if (!src) abort();
    }
    double ms = timer.nsecsElapsed() / 1e6;
    fprintf(stderr, "  total: %.1f ms, per-iter: %.1f ms\n", ms, ms / iters);
}

static void scenarioFullReplot(int iters)
{
    auto data = generateMultiData(kDefaultPoints, kDefaultCols);
    auto [plot, mg] = setupPlotWithRhi(data);

    fprintf(stderr, "full_replot: %d pts × %d cols, %d iters (rhi=%s)\n",
            kDefaultPoints, kDefaultCols, iters,
            plot->rhi() ? "yes" : "no");
    waitForProfiler();

    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < iters; ++i) {
        // Force full cache invalidation
        mg->setAdaptiveSampling(mg->adaptiveSampling());
        plot->replot(QCustomPlot::rpImmediateRefresh);
    }
    double ms = timer.nsecsElapsed() / 1e6;
    fprintf(stderr, "  total: %.1f ms, per-iter: %.1f ms\n", ms, ms / iters);
    delete plot;
}

static void scenarioPanReplot(int iters)
{
    auto data = generateMultiData(kDefaultPoints, kDefaultCols);
    auto [plot, mg] = setupPlotWithRhi(data);

    // Small pan steps — should hit the GPU translation fast path
    double panStep = plot->xAxis->range().size() * 0.005; // 0.5% per step

    fprintf(stderr, "pan_replot: %d pts × %d cols, pan step=%.4f, %d iters (rhi=%s)\n",
            kDefaultPoints, kDefaultCols, panStep, iters,
            plot->rhi() ? "yes" : "no");
    waitForProfiler();

    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < iters; ++i) {
        QCPRange r = plot->xAxis->range();
        plot->xAxis->setRange(r.lower + panStep, r.upper + panStep);
        plot->replot(QCustomPlot::rpImmediateRefresh);
    }
    double ms = timer.nsecsElapsed() / 1e6;
    fprintf(stderr, "  total: %.1f ms, per-iter: %.1f ms\n", ms, ms / iters);
    delete plot;
}

// ── Main ────────────────────────────────────────────────────────

struct Scenario {
    const char* name;
    void (*fn)(int);
    int defaultIters;
};

static const Scenario scenarios[] = {
    {"l1_resampling", scenarioL1Resampling, 5},
    {"l2_resampling", scenarioL2Resampling, 100},
    {"adaptive",      scenarioAdaptive,      5},
    {"adaptive_multi", scenarioAdaptiveMulti, 5},
    {"data_setup",    scenarioDataSetup,      3},
    {"full_replot",   scenarioFullReplot,     20},
    {"pan_replot",    scenarioPanReplot,      200},
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    // Strip --no-barrier before scenario parsing
    std::vector<char*> args(argv, argv + argc);
    args.erase(std::remove_if(args.begin(), args.end(), [](const char* a) {
        if (strcmp(a, "--no-barrier") == 0) { gUseBarrier = false; return true; }
        return false;
    }), args.end());
    argc = static_cast<int>(args.size());
    argv = args.data();

    if (argc < 2) {
        fprintf(stderr, "Usage: %s [--no-barrier] <scenario> [iterations]\n", argv[0]);
        fprintf(stderr, "Scenarios:\n");
        for (auto& s : scenarios)
            fprintf(stderr, "  %-18s (default %d iters)\n", s.name, s.defaultIters);
        return 1;
    }

    const char* name = argv[1];
    int iters = -1;
    if (argc >= 3)
        iters = std::atoi(argv[2]);

    for (auto& s : scenarios) {
        if (strcmp(s.name, name) == 0) {
            int n = (iters > 0) ? iters : s.defaultIters;
            s.fn(n);
            return 0;
        }
    }

    fprintf(stderr, "Unknown scenario: %s\n", name);
    return 1;
}
