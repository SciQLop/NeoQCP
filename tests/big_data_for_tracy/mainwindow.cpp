#include <QWidget>
#include <qcustomplot.h>
#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    auto* plot = new QCustomPlot(this);
    plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom
                          | QCP::iSelectPlottables | QCP::iSelectAxes);

    constexpr int N = 50'000'000;
    std::vector<double> keys(N), vals(N);
    for (int i = 0; i < N; ++i)
    {
        double t = i * 1e-6;
        keys[i] = t;
        vals[i] = std::sin(t * 6.28 * 0.5)
                + 0.3 * std::sin(t * 6.28 * 50.0)
                + 0.1 * std::sin(t * 6.28 * 5000.0);
    }

    auto* g = new QCPGraph2(plot->xAxis, plot->yAxis);
    g->setData(std::move(keys), std::move(vals));
    g->setPen(QPen(QColor(31, 119, 180), 1));

    plot->xAxis->setLabel("Time (s)");
    plot->yAxis->setLabel("Amplitude");
    plot->rescaleAxes();
    plot->replot();

    setCentralWidget(plot);
}

MainWindow::~MainWindow()
{}
