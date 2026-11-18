// Target program (milestone 6): two panels sharing x.
// https://matplotlib.org/stable/gallery/subplots_axes_and_figures/shared_axis_demo.html

#include <qcustomplot.h>

#include <QApplication>

#include <cmath>
#include <numbers>
#include <vector>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QCustomPlot plot;
    plot.resize(640, 480);

    constexpr double pi = std::numbers::pi;
    std::vector<double> t1, y1, t2, y2;
    for (int i = 0; i <= 200; ++i)
    {
        t1.push_back(8.0 * i / 200);
        y1.push_back(std::sin(2 * pi * t1.back()));
        t2.push_back(2 + 8.0 * i / 200);
        y2.push_back(20 * std::pow(std::cos(2 * pi * t2.back()), 2) * std::exp(-0.3 * t2.back()));
    }

    plot.setAxisRectGrid(2, 1, { .shareX = true });
    auto ax1 = plot.axisRect(0, 0);
    auto ax2 = plot.axisRect(1, 0);
    const QPen blue(QColor(31, 119, 180), 1.5);

    ax1->line(t1, y1, { .pen = blue });
    ax1->yAxis()->setLabel("Signal 1");

    ax2->line(t2, y2, { .pen = blue });
    ax2->yAxis()->setLabel("Signal 2");
    ax2->xAxis()->setLabel("Time (s)");

    plot.setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    plot.rescaleAxes();
    plot.show();
    return app.exec();
}
