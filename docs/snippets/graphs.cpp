#include "snippets.h"

#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>
#include <random>
#include <utility>
#include <vector>

DOC_SNIPPET(graphs_line_styles, 600, 400)
{
    // [graphs_line_styles]
    QVector<double> x(15), y(15);
    for (int i = 0; i < 15; ++i)
    {
        x[i] = i;
        y[i] = std::sin(i / 2.0);
    }
    const QList<std::pair<QCPGraph::LineStyle, QString>> styles {
        { QCPGraph::lsImpulse, "lsImpulse" },     { QCPGraph::lsLine, "lsLine" },
        { QCPGraph::lsStepLeft, "lsStepLeft" },   { QCPGraph::lsStepRight, "lsStepRight" },
        { QCPGraph::lsStepCenter, "lsStepCenter" } };
    const QList<QColor> colors { QColor(31, 119, 180), QColor(255, 127, 14), QColor(44, 160, 44),
                                 QColor(214, 39, 40), QColor(148, 103, 189) };
    for (int s = 0; s < styles.size(); ++s)
    {
        QVector<double> shifted = y;
        for (double& v : shifted)
            v += 3 * s;   // stack the graphs; impulses (s = 0) start from value 0
        QCPGraph* graph = customPlot->addGraph();
        graph->setData(x, shifted);
        graph->setLineStyle(styles[s].first);
        graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 5));
        graph->setPen(QPen(colors[s], 1.5));
        graph->setName(styles[s].second);
    }
    customPlot->xAxis->setRange(-0.5, 20);
    customPlot->yAxis->setRange(-1.5, 14);
    customPlot->legend->setVisible(true);
    customPlot->replot();
    // [/graphs_line_styles]
}

DOC_SNIPPET(graphs_scatter, 600, 360)
{
    // [graphs_scatter]
    std::mt19937 rng(1);
    std::normal_distribution<double> noise(0.0, 0.15);
    QVector<double> x(60), measured(60), modelX(300), model(300);
    for (int i = 0; i < 60; ++i)
    {
        x[i] = i / 6.0;
        measured[i] = std::exp(-x[i] / 4) * std::cos(x[i]) + noise(rng);
    }
    for (int i = 0; i < 300; ++i)
    {
        modelX[i] = i / 30.0;
        model[i] = std::exp(-modelX[i] / 4) * std::cos(modelX[i]);
    }
    QCPGraph* points = customPlot->addGraph();
    points->setData(x, measured);
    points->setLineStyle(QCPGraph::lsNone);
    points->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, QColor(31, 119, 180),
                                            QColor(31, 119, 180, 60), 7));
    points->setName("measurement");
    QCPGraph* fit = customPlot->addGraph();
    fit->setData(modelX, model);
    fit->setPen(QPen(QColor(214, 39, 40), 2));
    fit->setName("model");
    customPlot->rescaleAxes();
    customPlot->legend->setVisible(true);
    customPlot->replot();
    // [/graphs_scatter]
}

DOC_SNIPPET(graphs_fill_under, 600, 360)
{
    // [graphs_fill_under]
    QVector<double> f(400), power(400);
    for (int i = 0; i < 400; ++i)
    {
        f[i] = i / 4.0;
        power[i] = std::exp(-std::pow((f[i] - 30) / 6, 2))
                   + 0.5 * std::exp(-std::pow((f[i] - 65) / 10, 2));
    }
    QCPGraph* spectrum = customPlot->addGraph();
    spectrum->setData(f, power);
    spectrum->setPen(QPen(QColor(31, 119, 180), 1.5));
    spectrum->setBrush(QColor(31, 119, 180, 70));   // a brush fills down to value 0
    customPlot->xAxis->setLabel("frequency (Hz)");
    customPlot->yAxis->setLabel("power");
    customPlot->xAxis->setRange(0, 100);
    customPlot->yAxis->setRange(0, 1.2);
    customPlot->replot();
    // [/graphs_fill_under]
}

DOC_SNIPPET(graphs_channel_fill, 600, 360)
{
    // [graphs_channel_fill]
    QVector<double> t(200), mean(200), upper(200), lower(200);
    for (int i = 0; i < 200; ++i)
    {
        t[i] = i / 20.0;
        mean[i] = std::sin(t[i]);
        const double spread = 0.15 + 0.05 * t[i];
        upper[i] = mean[i] + spread;
        lower[i] = mean[i] - spread;
    }
    QCPGraph* low = customPlot->addGraph();
    low->setData(t, lower);
    low->setPen(Qt::NoPen);
    low->removeFromLegend();
    QCPGraph* high = customPlot->addGraph();
    high->setData(t, upper);
    high->setPen(Qt::NoPen);
    high->setBrush(QColor(255, 127, 14, 70));
    high->setChannelFillGraph(low);   // fill between high and low, with high's brush
    high->setName("±1σ");
    QCPGraph* centre = customPlot->addGraph();
    centre->setData(t, mean);
    centre->setPen(QPen(QColor(255, 127, 14), 2));
    centre->setName("mean");
    customPlot->rescaleAxes();
    customPlot->legend->setVisible(true);
    customPlot->replot();
    // [/graphs_channel_fill]
}

DOC_SNIPPET(graphs_graph2_owning, 600, 360)
{
    // [graphs_graph2_owning]
    const int n = 5000;
    std::vector<double> time(n);
    std::vector<std::int16_t> counts(n);   // raw ADC counts, kept as int16
    std::mt19937 rng(42);
    std::normal_distribution<double> noise(0.0, 40.0);
    for (int i = 0; i < n; ++i)
    {
        time[i] = i * 1e-3;
        const double signal = 1000 * std::sin(2 * std::numbers::pi * 5 * time[i])
                              * std::exp(-time[i]);
        counts[i] = static_cast<std::int16_t>(signal + noise(rng));
    }
    auto* graph = new QCPGraph2(customPlot->xAxis, customPlot->yAxis);
    graph->setData(std::move(time), std::move(counts));   // moved in, not copied
    graph->setPen(QPen(QColor(31, 119, 180), 1));
    customPlot->xAxis->setLabel("time (s)");
    customPlot->yAxis->setLabel("counts");
    customPlot->rescaleAxes();
    customPlot->replot();
    // [/graphs_graph2_owning]
}

DOC_SNIPPET(graphs_graph2_view, 600, 360)
{
    // [graphs_graph2_view]
    // The graph reads these buffers at every replot: they must outlive it.
    static std::vector<double> time(1000);
    static std::vector<double> level(1000);
    for (int i = 0; i < 1000; ++i)
    {
        time[i] = i * 0.01;
        level[i] = std::tanh(std::sin(time[i]) * 3);
    }
    auto* graph = new QCPGraph2(customPlot->xAxis, customPlot->yAxis);
    graph->viewData(time.data(), level.data(), static_cast<int>(time.size()));
    graph->setPen(QPen(QColor(44, 160, 44), 2));

    // Later, after writing into the buffers in place:
    for (int i = 600; i < 700; ++i)
        level[i] = 0;
    graph->dataChanged();

    customPlot->rescaleAxes();
    customPlot->replot();
    // [/graphs_graph2_view]
}

DOC_SNIPPET(graphs_graph2_shared, 600, 420)
{
    // [graphs_graph2_shared]
    std::vector<double> t(20000), b(20000);
    for (int i = 0; i < 20000; ++i)
    {
        t[i] = i * 0.005;
        b[i] = std::sin(t[i] / 5) + 0.2 * std::sin(t[i] * 7);
    }
    auto source = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::move(t), std::move(b));

    auto* overview = new QCPGraph2(customPlot->xAxis, customPlot->yAxis);
    overview->setDataSource(source);

    auto* detailRect = new QCPAxisRect(customPlot);
    customPlot->plotLayout()->addElement(1, 0, detailRect);
    QCPAxis* detailX = detailRect->axis(QCPAxis::atBottom);
    auto* detail = new QCPGraph2(detailX, detailRect->axis(QCPAxis::atLeft));
    detail->setDataSource(source);   // same points, no second copy

    overview->setPen(QPen(QColor(31, 119, 180), 1));
    detail->setPen(QPen(QColor(214, 39, 40), 1.5));
    customPlot->rescaleAxes();
    detailX->setRange(40, 45);
    detail->rescaleValueAxis(false, true);
    customPlot->replot();
    // [/graphs_graph2_shared]
}

DOC_SNIPPET(graphs_graph2_scatter_color, 600, 360)
{
    // [graphs_graph2_scatter_color]
    const int n = 1500;
    std::vector<double> t(n), v(n);
    std::vector<float> color(n);
    std::mt19937 rng(5);
    std::normal_distribution<double> noise(0.0, 1.0);
    for (int i = 0; i < n; ++i)
    {
        t[i] = i * 0.01;
        const double residual = noise(rng);
        v[i] = std::sin(t[i]) * 3 + residual;
        color[i] = static_cast<float>(std::min(1.0, std::abs(residual) / 2.5));   // 0..1
    }
    auto* graph = new QCPGraph2(customPlot->xAxis, customPlot->yAxis);
    graph->setData(std::move(t), std::move(v));
    graph->setLineStyle(QCPGraph2::lsNone);
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 5));
    graph->setScatterColorValues(std::move(color));
    graph->setScatterColorGradient(QCPColorGradient::gpViridis);
    customPlot->rescaleAxes();
    customPlot->replot();
    // [/graphs_graph2_scatter_color]
}

DOC_SNIPPET(graphs_multigraph, 600, 360)
{
    // [graphs_multigraph]
    const int n = 3000;
    std::vector<double> time(n);
    std::vector<std::vector<float>> field(3, std::vector<float>(n));
    for (int i = 0; i < n; ++i)
    {
        time[i] = i * 0.01;
        field[0][i] = static_cast<float>(5 * std::sin(time[i] / 3));
        field[1][i] = static_cast<float>(4 * std::cos(time[i] / 3));
        field[2][i] = static_cast<float>(1.5 * std::sin(time[i] * 2));
    }
    auto* b = new QCPMultiGraph(customPlot->xAxis, customPlot->yAxis);
    b->setData(std::move(time), std::move(field));   // one column per component
    b->setName("B (nT)");
    b->setComponentNames({ "Bx", "By", "Bz" });
    b->setComponentPens({ QPen(QColor(31, 119, 180), 1.5), QPen(QColor(44, 160, 44), 1.5),
                          QPen(QColor(214, 39, 40), 1.5) });
    customPlot->xAxis->setLabel("time (s)");
    customPlot->rescaleAxes();
    customPlot->legend->setVisible(true);
    customPlot->replot();
    // [/graphs_multigraph]
}

DOC_SNIPPET(graphs_gaps, 600, 360)
{
    // [graphs_gaps]
    QVector<double> t, withNan;
    std::vector<double> keys, values;
    for (int i = 0; i <= 200; ++i)
    {
        const double x = i * 0.05;
        t.append(x);
        withNan.append(x > 3 && x < 4 ? qQNaN() : std::sin(x));   // NaN breaks the line
        if (x < 6 || x > 7.5)   // no samples at all between 6 and 7.5: a key gap
        {
            keys.push_back(x);
            values.push_back(std::sin(x) + 2.5);
        }
    }
    QCPGraph* classic = customPlot->addGraph();
    classic->setData(t, withNan);
    classic->setPen(QPen(QColor(31, 119, 180), 2));
    classic->setName("QCPGraph, NaN values");
    auto* zeroCopy = new QCPGraph2(customPlot->xAxis, customPlot->yAxis);
    zeroCopy->setData(std::move(keys), std::move(values));
    zeroCopy->setPen(QPen(QColor(214, 39, 40), 2));
    zeroCopy->setName("QCPGraph2, missing keys");
    customPlot->xAxis->setRange(0, 10);
    customPlot->yAxis->setRange(-1.5, 5);
    customPlot->legend->setVisible(true);
    customPlot->replot();
    // [/graphs_gaps]
}
