#include "snippets.h"

#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>
#include <random>
#include <span>
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
        auto graph = customPlot->add<QCPGraph>()
            .setData(x, shifted)
            .setLineStyle(styles[s].first)
            .setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 5))
            .setPen(QPen(colors[s], 1.5))
            .setName(styles[s].second);
    }
    customPlot->xAxis->setRange(-0.5, 20);
    customPlot->yAxis->setRange(-1.5, 14);
    customPlot->legend->setVisible(true);
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
    auto points = customPlot->add<QCPGraph>()
        .setData(x, measured)
        .setLineStyle(QCPGraph::lsNone);
    points->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, QColor(31, 119, 180),
                                            QColor(31, 119, 180, 60), 7));
    points->setName("measurement");
    auto fit = customPlot->add<QCPGraph>()
        .setData(modelX, model)
        .setPen(QPen(QColor(214, 39, 40), 2))
        .setName("model");
    customPlot->rescaleAxes();
    customPlot->legend->setVisible(true);
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
    auto spectrum = customPlot->add<QCPGraph>()
        .setData(f, power)
        .setPen(QPen(QColor(31, 119, 180), 1.5));
    spectrum->setBrush(QColor(31, 119, 180, 70));   // a brush fills down to value 0
    customPlot->xAxis->setLabel("frequency (Hz)");
    customPlot->yAxis->setLabel("power");
    customPlot->xAxis->setRange(0, 100);
    customPlot->yAxis->setRange(0, 1.2);
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
    auto low = customPlot->add<QCPGraph>()
        .setData(t, lower)
        .setPen(Qt::NoPen);
    low->removeFromLegend();
    auto high = customPlot->add<QCPGraph>()
        .setData(t, upper)
        .setPen(Qt::NoPen)
        .setBrush(QColor(255, 127, 14, 70));
    high->setChannelFillGraph(low.get());   // fill between high and low, with high's brush
    high->setName("±1σ");
    auto centre = customPlot->add<QCPGraph>()
        .setData(t, mean)
        .setPen(QPen(QColor(255, 127, 14), 2))
        .setName("mean");
    customPlot->rescaleAxes();
    customPlot->legend->setVisible(true);
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
    auto graph = customPlot->add<QCPGraph2>();
    graph->setData(std::move(time), std::move(counts));   // moved in, not copied
    graph->setPen(QPen(QColor(31, 119, 180), 1));
    customPlot->xAxis->setLabel("time (s)");
    customPlot->yAxis->setLabel("counts");
    customPlot->rescaleAxes();
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
    auto graph = customPlot->add<QCPGraph2>();
    graph->viewData(time.data(), level.data(), static_cast<int>(time.size()));
    graph->setPen(QPen(QColor(44, 160, 44), 2));

    // Later, after writing into the buffers in place:
    for (int i = 600; i < 700; ++i)
        level[i] = 0;
    graph->dataChanged();

    customPlot->rescaleAxes();
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

    auto overview = customPlot->add<QCPGraph2>();
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
    auto graph = customPlot->add<QCPGraph2>()
        .setData(std::move(t), std::move(v))
        .setLineStyle(QCPGraph2::lsNone)
        .setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 5));
    graph->setScatterColorValues(std::move(color));
    graph->setScatterColorGradient(QCPColorGradient::gpViridis);
    customPlot->rescaleAxes();
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
    auto b = customPlot->add<QCPMultiGraph>();
    b->setData(std::move(time), std::move(field));   // one column per component
    b->setName("B (nT)");
    b->setComponentNames({ "Bx", "By", "Bz" });
    b->setComponentPens({ QPen(QColor(31, 119, 180), 1.5), QPen(QColor(44, 160, 44), 1.5),
                          QPen(QColor(214, 39, 40), 1.5) });
    customPlot->xAxis->setLabel("time (s)");
    customPlot->rescaleAxes();
    customPlot->legend->setVisible(true);
    // [/graphs_multigraph]
}

DOC_SNIPPET(graphs_multigraph_fill, 600, 300)
{
    // [graphs_multigraph_fill]
    const int n = 400;
    std::vector<double> time(n);
    std::vector<std::vector<double>> rates(2, std::vector<double>(n));
    for (int i = 0; i < n; ++i)
    {
        time[i] = i * 0.05;
        rates[0][i] = 3 + 2 * std::sin(time[i] / 2) + std::sin(time[i] * 3);
        rates[1][i] = 2 + 1.5 * std::cos(time[i] / 3);
    }
    auto graph = customPlot->add<QCPMultiGraph>()
        .setData(std::move(time), std::move(rates))
        .setName("count rate");
    graph->setComponentNames({ "protons", "electrons" });
    graph->setComponentPens({ QPen(QColor(31, 119, 180), 1.5), QPen(QColor(255, 127, 14), 1.5) });
    graph->setComponentFill(0, 0.0);   // down to 0, in the pen colour, see-through
    graph->setComponentFill(1, 0.0, QBrush(QColor(255, 127, 14, 40)));
    customPlot->rescaleAxes();
    customPlot->yAxis->setRangeLower(0);
    customPlot->legend->setVisible(true);
    // [/graphs_multigraph_fill]
}

DOC_SNIPPET(graphs_multigraph_band, 600, 300)
{
    // [graphs_multigraph_band]
    const int n = 300;
    std::vector<double> time(n);
    std::vector<std::vector<double>> columns(3, std::vector<double>(n));   // mean, low, high
    for (int i = 0; i < n; ++i)
    {
        time[i] = i * 0.1;
        const double spread = 0.4 + 0.3 * std::sin(time[i] / 4);
        columns[0][i] = std::sin(time[i] / 3);
        columns[1][i] = columns[0][i] - spread;
        columns[2][i] = columns[0][i] + spread;
    }
    auto model = customPlot->add<QCPMultiGraph>().setData(std::move(time), std::move(columns));
    model->setComponentNames({ "model", "low", "high" });
    const QColor blue(31, 119, 180);
    model->setComponentPens({ QPen(blue, 2), QPen(blue.lighter(150), 0.5), QPen(blue.lighter(150), 0.5) });
    model->setComponentFillBetween(1, 2);   // from "low" up to "high"
    model->setComponentInLegend(1, false);  // the edges still draw, but have no legend row
    model->setComponentInLegend(2, false);
    customPlot->rescaleAxes();
    customPlot->legend->setVisible(true);
    // [/graphs_multigraph_band]
}

DOC_SNIPPET(graphs_multigraph_error_bars, 600, 300)
{
    // [graphs_multigraph_error_bars]
    const int n = 25;
    std::vector<double> x(n);
    std::vector<std::vector<double>> columns(3, std::vector<double>(n));   // value, lower, upper
    std::mt19937 rng(3);
    std::normal_distribution<double> noise(0.0, 0.15);
    for (int i = 0; i < n; ++i)
    {
        x[i] = i;
        const double error = 0.1 + 0.02 * i;
        columns[0][i] = std::exp(-i / 10.0) + noise(rng);
        columns[1][i] = columns[0][i] - error;   // the bounds, not the error sizes
        columns[2][i] = columns[0][i] + error;
    }
    auto measured = customPlot->add<QCPMultiGraph>().setData(std::move(x), std::move(columns));
    measured->setComponentNames({ "measured", "lower", "upper" });
    measured->setComponentLineStyle(0, QCPMultiGraph::lsNone);
    measured->component(0).scatterStyle = QCPScatterStyle(QCPScatterStyle::ssDisc, 6);
    measured->setComponentErrorBars(0, 1, 2);   // also takes "lower" and "upper" out of the legend
    measured->setComponentVisible(1, false);     // draw the bounds as bars only, not as lines
    measured->setComponentVisible(2, false);
    customPlot->xAxis->setRange(-1, n);
    customPlot->yAxis->setRange(-0.6, 1.6);
    customPlot->legend->setVisible(true);
    // [/graphs_multigraph_error_bars]
}

DOC_CODE(graphs_multigraph_add_data)
{
    // [graphs_multigraph_add_data]
    auto stream = customPlot->add<QCPMultiGraph>()
        .setData(std::vector<double>{ 0.0, 1.0 },
                 std::vector<std::vector<double>>{ { 10.0, 11.0 }, { 4.0, 5.0 } });
    // Appends rows to the source setData() built (double values only). Keys out of order are merged.
    stream->addData(QVector<double>{ 3.0, 2.0 },
                    std::vector<QVector<double>>{ { 13.0, 12.0 }, { 6.0, 7.0 } });
    // One row, one value per component.
    const double row[] = { 14.0, 8.0 };
    stream->addData(4.0, std::span<const double>(row, 2));
    // [/graphs_multigraph_add_data]
}

DOC_SNIPPET(graphs_multigraph_fill_to_graph, 600, 320)
{
    // [graphs_multigraph_fill_to_graph]
    const int n = 241;
    std::vector<double> time(n);
    std::vector<std::vector<double>> model(1, std::vector<double>(n));
    for (int i = 0; i < n; ++i)
    {
        time[i] = i / 24.0;
        model[0][i] = 2.0 + std::sin(time[i]);
    }
    auto curve = customPlot->add<QCPMultiGraph>().setData(std::move(time), std::move(model));
    curve->setName("model");
    curve->setComponentPens({ QPen(QColor(31, 119, 180), 2) });

    // The reference has its own, coarser grid and stops at key 9.
    std::vector<double> referenceKeys { 0.0, 1.5, 3.0, 4.5, 6.0, 7.5, 9.0 };
    std::vector<double> referenceValues { 3.4, 2.7, 3.6, 3.0, 3.5, 4.0, 3.2 };
    auto reference = customPlot->add<QCPMultiGraph>()
        .setData(referenceKeys, std::vector<std::vector<double>>{ referenceValues });
    reference->setName("reference");
    reference->setComponentPens({ QPen(QColor(214, 39, 40), 1.5) });

    // The band is cropped to 0..9 and the reference is interpolated onto the model's keys.
    curve->setComponentFillToGraph(0, reference.get(), 0, QBrush(QColor(214, 39, 40, 60)));
    customPlot->xAxis->setLabel("time (s)");
    customPlot->rescaleAxes();
    customPlot->legend->setVisible(true);
    // [/graphs_multigraph_fill_to_graph]
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
    auto classic = customPlot->add<QCPGraph>()
        .setData(t, withNan)
        .setPen(QPen(QColor(31, 119, 180), 2))
        .setName("QCPGraph, NaN values");
    auto zeroCopy = customPlot->add<QCPGraph2>()
        .setData(std::move(keys), std::move(values))
        .setPen(QPen(QColor(214, 39, 40), 2))
        .setName("QCPGraph2, missing keys");
    customPlot->xAxis->setRange(0, 10);
    customPlot->yAxis->setRange(-1.5, 5);
    customPlot->legend->setVisible(true);
    // [/graphs_gaps]
}
