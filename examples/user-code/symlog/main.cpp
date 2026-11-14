// Target program (milestone 6): symmetric log axes, linear near zero. Does not compile yet.
// https://matplotlib.org/stable/gallery/scales/symlog_demo.html

#include <qcustomplot.h>

#include <QApplication>

#include <cmath>
#include <vector>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QCustomPlot plot;
    plot.resize(640, 720);

    std::vector<double> x, y, wave;
    for (int i = 0; i < 10000; ++i)
    {
        x.push_back(-50.0 + 0.01 * i);
        y.push_back(0.01 * i);
        wave.push_back(std::sin(x.back() / 3.0));
    }

    auto ax0 = plot.axisRect();
    auto ax1 = plot.addAxisRect();
    auto ax2 = plot.addAxisRect();
    const QPen blue(QColor(31, 119, 180), 1.5);

    ax0->line(x, y, { .pen = blue });
    ax0->xAxis()->setScale(QCPScale::symlog());
    ax0->xAxis()->grid()->setSubGridVisible(true);
    ax0->yAxis()->setLabel("symlogx");

    ax1->line(y, x, { .pen = blue });
    ax1->yAxis()->setScale(QCPScale::symlog());
    ax1->yAxis()->setLabel("symlogy");
    ax1->xAxis()->grid()->setVisible(false);
    ax1->yAxis()->grid()->setVisible(false);

    ax2->line(x, wave, { .pen = blue });
    ax2->xAxis()->setScale(QCPScale::symlog());
    ax2->yAxis()->setScale(QCPScale::symlog(0.015));
    ax2->yAxis()->setLabel("symlog both");

    plot.setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    plot.rescaleAxes();
    plot.show();
    return app.exec();
}
