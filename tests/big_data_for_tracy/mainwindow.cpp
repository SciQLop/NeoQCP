#include <QWidget>
#include <qcustomplot.h>
#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{

    auto plotBuilder = [this](bool openGL=false, std::size_t points= 10'000'000){
        auto plot = new QCustomPlot(this);
        plot->setOpenGl(openGL, 4);

        plot->addGraph();

        // Simulate a large data set
        for (auto i = 0UL; i < points; ++i) {
            plot->graph(0)->addData(i, qSin(i * 0.001));
        }
        plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables | QCP::iSelectAxes | QCP::iSelectLegend);
        plot->xAxis->rescale();
        plot->yAxis->rescale();
        plot->plotLayout()->insertRow(0);
        plot->plotLayout()->addElement(0,0, new  QCPTextElement(plot,openGL?"With OpenGL":"Without OpenGL"));
        plot->replot();
        return plot;
    };
    setCentralWidget(new QWidget(this));
    mLayout = new QVBoxLayout(centralWidget());
    mLayout->setContentsMargins(0, 0, 0, 0);
    mLayout->setSpacing(0);
    mLayout->addWidget(plotBuilder(true, 10'000'000));
    mLayout->addWidget(plotBuilder(false, 10'000'000));

}

MainWindow::~MainWindow()
{}
