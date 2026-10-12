#include <QApplication>
#include <qcustomplot.h>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QCustomPlot customPlot;
    customPlot.resize(600, 360);

    QVector<double> x(101), y(101);
    for (int i = 0; i < 101; ++i)
    {
        x[i] = i / 50.0 - 1;   // -1 to 1
        y[i] = x[i] * x[i];
    }
    customPlot.addGraph();
    customPlot.graph(0)->setData(x, y);
    customPlot.xAxis->setLabel("x");
    customPlot.yAxis->setLabel("y");
    customPlot.xAxis->setRange(-1, 1);
    customPlot.yAxis->setRange(0, 1);
    customPlot.replot();

    customPlot.show();
    return app.exec();
}
