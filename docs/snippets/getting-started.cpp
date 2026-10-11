#include "snippets.h"

#include <cmath>

DOC_SNIPPET(first_plot, 600, 360)
{
    // [first_plot]
    QVector<double> x(101), y(101);
    for (int i = 0; i < 101; ++i)
    {
        x[i] = i / 50.0 - 1;   // -1 to 1
        y[i] = x[i] * x[i];
    }
    customPlot->addGraph();
    customPlot->graph(0)->setData(x, y);
    customPlot->xAxis->setLabel("x");
    customPlot->yAxis->setLabel("y");
    customPlot->xAxis->setRange(-1, 1);
    customPlot->yAxis->setRange(0, 1);
    customPlot->replot();
    // [/first_plot]
}

DOC_SNIPPET(first_plot_interactive, 600, 360)
{
    // [first_plot_interactive]
    QVector<double> t(500), signal(500);
    for (int i = 0; i < 500; ++i)
    {
        t[i] = i / 50.0;
        signal[i] = std::sin(t[i]) * std::exp(-t[i] / 5);
    }
    QCPGraph* graph = customPlot->addGraph();
    graph->setData(t, signal);
    graph->setName("damped sine");
    graph->setPen(QPen(QColor(31, 119, 180), 2));
    customPlot->rescaleAxes();
    customPlot->legend->setVisible(true);
    // Drag to pan, wheel to zoom, click to select.
    customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
    customPlot->replot();
    // [/first_plot_interactive]
}
