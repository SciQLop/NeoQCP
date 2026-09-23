// One value column: QCPGraph2 vs a one-component QCPMultiGraph.
// Decides whether QCPGraph2 can be retired in favour of QCPMultiGraph (SciQLopPlots#113).
//
// Usage: single_column_perf <graph2|multigraph> <setup|full_replot|pan_replot|zoom_replot>
//                           [points] [iterations]

#include <qcustomplot.h>
#include <QApplication>
#include <QElapsedTimer>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

static std::vector<double> makeKeys(int n)
{
    std::vector<double> k(n);
    for (int i = 0; i < n; ++i)
        k[i] = i * 1e-6;
    return k;
}

static std::vector<double> makeValues(const std::vector<double>& keys)
{
    std::vector<double> v(keys.size());
    for (size_t i = 0; i < keys.size(); ++i) {
        const double t = keys[i];
        v[i] = std::sin(t * 6.28 * 0.5) + 0.3 * std::sin(t * 6.28 * 50.0)
             + 0.1 * std::sin(t * 6.28 * 5000.0);
    }
    return v;
}

static void setData(QCPGraph2* g, std::vector<double> k, std::vector<double> v)
{
    g->setData(std::move(k), std::move(v));
}

static void setData(QCPMultiGraph* g, std::vector<double> k, std::vector<double> v)
{
    std::vector<std::vector<double>> cols;
    cols.push_back(std::move(v));
    g->setData(std::move(k), std::move(cols));
}

template <typename G>
static void waitIdle(G* g)
{
    while (g->pipeline().isBusy()) {
        QThread::usleep(200);
        QApplication::processEvents();
    }
}

template <typename G>
static void invalidateLines(G* g)
{
    g->setAdaptiveSampling(!g->adaptiveSampling());
    g->setAdaptiveSampling(!g->adaptiveSampling());
}

template <typename G>
static double timeSetup(QCustomPlot* plot, G* g, int points, int iters)
{
    const auto keys = makeKeys(points);
    const auto values = makeValues(keys);
    QElapsedTimer timer;
    qint64 total = 0;
    for (int i = 0; i < iters; ++i) {
        auto k = keys;
        auto v = values;
        timer.start();
        setData(g, std::move(k), std::move(v));
        waitIdle(g);
        plot->replot(QCustomPlot::rpImmediateRefresh);
        total += timer.nsecsElapsed();
    }
    return total / 1e6 / iters;
}

template <typename Step>
static double timeLoop(int iters, Step step)
{
    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < iters; ++i)
        step(i);
    return timer.nsecsElapsed() / 1e6 / iters;
}

template <typename G>
static double run(const char* scenario, int points, int iters)
{
    QCustomPlot plot;
    plot.resize(1920, 1080);
    auto* g = new G(plot.xAxis, plot.yAxis);
    plot.show();
    while (!plot.rhi())
        QApplication::processEvents(QEventLoop::AllEvents, 50);

    if (strcmp(scenario, "setup") == 0)
        return timeSetup(&plot, g, points, iters);

    auto keys = makeKeys(points);
    auto values = makeValues(keys);
    setData(g, std::move(keys), std::move(values));
    waitIdle(g);
    plot.rescaleAxes();
    plot.replot(QCustomPlot::rpImmediateRefresh);
    QApplication::processEvents();

    auto* x = plot.xAxis;
    if (strcmp(scenario, "full_replot") == 0)
        return timeLoop(iters, [&](int) {
            invalidateLines(g);
            plot.replot(QCustomPlot::rpImmediateRefresh);
        });
    if (strcmp(scenario, "pan_replot") == 0) {
        const double step = x->range().size() * 0.005;
        return timeLoop(iters, [&](int) {
            x->setRange(x->range().lower + step, x->range().upper + step);
            plot.replot(QCustomPlot::rpImmediateRefresh);
        });
    }
    if (strcmp(scenario, "zoom_replot") == 0)
        return timeLoop(iters, [&](int i) {
            const QCPRange r = x->range();
            const double f = (i % 2) ? 1.0 / 0.98 : 0.98;
            x->setRange(r.center() - r.size() * f / 2, r.center() + r.size() * f / 2);
            plot.replot(QCustomPlot::rpImmediateRefresh);
        });
    fprintf(stderr, "unknown scenario: %s\n", scenario);
    std::exit(1);
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <graph2|multigraph> <setup|full_replot|pan_replot|zoom_replot>"
                        " [points] [iterations]\n", argv[0]);
        return 1;
    }
    const char* kind = argv[1];
    const char* scenario = argv[2];
    const int points = argc > 3 ? std::atoi(argv[3]) : 10'000'000;
    const int iters = argc > 4 ? std::atoi(argv[4]) : 20;
    const double ms = strcmp(kind, "graph2") == 0
        ? run<QCPGraph2>(scenario, points, iters)
        : run<QCPMultiGraph>(scenario, points, iters);
    printf("%s %s %d %.3f\n", kind, scenario, points, ms);
    return 0;
}
