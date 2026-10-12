#include "snippets.h"

#include <cmath>
#include <memory>
#include <numbers>
#include <random>
#include <vector>

DOC_SNIPPET(performance_million_points, 600, 360)
{
    // [performance_million_points]
    const int n = 5'000'000;
    std::vector<double> t(n);
    std::vector<float> signal(n);
    std::minstd_rand noise(42);
    for (int i = 0; i < n; ++i)
    {
        t[i] = i * 1e-3; // 1 kHz for 5000 s
        const double wave = std::sin(2 * std::numbers::pi * t[i] / 1000.0);
        const double jitter = noise() / double(noise.max()) - 0.5;
        const double spike = (i % 1'000'000 == 500'000) ? 3.0 : 0.0; // one sample each
        signal[i] = float(wave + 0.4 * jitter + spike);
    }
    auto* graph = new QCPGraph2(customPlot->xAxis, customPlot->yAxis);
    graph->setData(std::move(t), std::move(signal)); // moved in, never copied
    graph->setPen(QPen(QColor(31, 119, 180)));
    customPlot->xAxis->setLabel("time (s)");
    customPlot->xAxis->setRange(0, 5000);
    customPlot->yAxis->setRange(-1.5, 4);
    customPlot->replot();
    // [/performance_million_points]
}

DOC_SNIPPET(performance_multigraph, 600, 360)
{
    // [performance_multigraph]
    const int n = 2'000'000;
    std::vector<double> t(n);
    std::vector<std::vector<float>> b(3, std::vector<float>(n));
    for (int i = 0; i < n; ++i)
    {
        t[i] = i * 0.01;
        const double phase = 2 * std::numbers::pi * t[i] / 4000.0;
        b[0][i] = float(10 * std::cos(phase) + std::sin(0.37 * t[i]));
        b[1][i] = float(10 * std::sin(phase) + std::sin(0.53 * t[i]));
        b[2][i] = float(3 * std::sin(5 * phase) + std::sin(0.71 * t[i]));
    }
    auto* field = new QCPMultiGraph(customPlot->xAxis, customPlot->yAxis);
    field->setData(std::move(t), std::move(b)); // one key column, three value columns
    field->setComponentNames({ "Bx", "By", "Bz" });
    field->setComponentColors({ QColor(31, 119, 180), QColor(255, 127, 14), QColor(44, 160, 44) });
    customPlot->xAxis->setLabel("time (s)");
    customPlot->yAxis->setLabel("B (nT)");
    customPlot->legend->setVisible(true);
    customPlot->rescaleAxes();
    customPlot->replot();
    // [/performance_multigraph]
}

DOC_SNIPPET(performance_shared_source, 600, 420)
{
    // [performance_shared_source]
    const int n = 1'000'000;
    std::vector<double> t(n);
    std::vector<float> v(n);
    for (int i = 0; i < n; ++i)
    {
        t[i] = i * 1e-3;
        v[i] = float(std::sin(t[i]) * std::sin(t[i] / 97.0) + 0.1 * std::sin(13.0 * t[i]));
    }
    auto source = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<float>>>(
        std::move(t), std::move(v));

    auto* overview = new QCPGraph2(customPlot->xAxis, customPlot->yAxis);
    overview->setDataSource(source);

    auto* detailRect = new QCPAxisRect(customPlot);
    customPlot->plotLayout()->addElement(1, 0, detailRect);
    // A new axis rect lands on the "main" layer: move it, so a pan can translate "main".
    detailRect->setLayer("background");
    for (QCPAxis* axis : detailRect->axes())
    {
        axis->setLayer("axes");
        axis->grid()->setLayer("grid");
    }
    auto* detail = new QCPGraph2(detailRect->axis(QCPAxis::atBottom),
                                 detailRect->axis(QCPAxis::atLeft));
    detail->setDataSource(source); // the same samples, no second copy
    detail->setPen(QPen(QColor(214, 39, 40)));

    customPlot->xAxis->setRange(0, 1000);
    customPlot->yAxis->setRange(-1.3, 1.3);
    detailRect->axis(QCPAxis::atBottom)->setRange(300, 320);
    detailRect->axis(QCPAxis::atLeft)->setRange(-1.3, 1.3);
    customPlot->replot();
    // [/performance_shared_source]
}

DOC_SNIPPET(performance_cursor_layer, 600, 360)
{
    QVector<double> x(2000), y(2000);
    for (int i = 0; i < 2000; ++i)
    {
        x[i] = i / 100.0;
        y[i] = std::sin(x[i]) * std::cos(3.1 * x[i]);
    }
    customPlot->addGraph()->setData(x, y);
    customPlot->xAxis->setRange(0, 20);
    customPlot->yAxis->setRange(-1.1, 1.1);
    // [performance_cursor_layer]
    if (customPlot->addLayer("cursor"))
        customPlot->layer("cursor")->setMode(QCPLayer::lmBuffered);

    auto* cursor = new QCPItemStraightLine(customPlot);
    cursor->setLayer("cursor");
    cursor->setPen(QPen(QColor(214, 39, 40), 1, Qt::DashLine));
    cursor->point1->setCoords(7.5, 0);
    cursor->point2->setCoords(7.5, 1);

    QObject::connect(customPlot, &QCustomPlot::mouseMove, cursor,
                     [customPlot, cursor](QMouseEvent* event) {
                         const double key = customPlot->xAxis->pixelToCoord(event->position().x());
                         cursor->point1->setCoords(key, 0);
                         cursor->point2->setCoords(key, 1);
                         customPlot->layer("cursor")->replot(); // only this layer is redrawn
                     });
    customPlot->replot();
    // [/performance_cursor_layer]
}

DOC_CODE(performance_swap_delays)
{
    // [performance_swap_delays]
    customPlot->setDataSwapDebounceMs(50);  // show new data sooner
    customPlot->setDataSwapMaxWaitMs(500);  // never hold it back more than half a second
    // [/performance_swap_delays]
}

DOC_CODE(performance_rescale_when_ready)
{
    auto* graph = new QCPGraph2(customPlot->xAxis, customPlot->yAxis);
    graph->setData(std::vector<double> { 0, 1 }, std::vector<double> { 0, 1 });
    customPlot->replot();
    std::vector<double> keys { 0, 10 }, values { -5, 5 };
    // [performance_rescale_when_ready]
    graph->setData(std::move(keys), std::move(values));
    QObject::connect(graph, &QCPAbstractPlottable::busyChanged, customPlot,
                     [customPlot](bool busy) {
                         if (!busy)
                         {
                             customPlot->rescaleAxes();
                             customPlot->replot();
                         }
                     });
    // [/performance_rescale_when_ready]
}

DOC_CODE(performance_axis_rect_layers)
{
    // [performance_axis_rect_layers]
    auto* rect = new QCPAxisRect(customPlot);
    rect->setLayer("background");
    for (QCPAxis* axis : rect->axes())
    {
        axis->setLayer("axes");
        axis->grid()->setLayer("grid");
    }
    // [/performance_axis_rect_layers]
    customPlot->plotLayout()->addElement(1, 0, rect);
}
