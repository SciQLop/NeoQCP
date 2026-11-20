// Target program (milestone 6): an inset zoomed on a sub-range, with its indicator.
// https://matplotlib.org/stable/gallery/subplots_axes_and_figures/zoom_inset_axes.html

#include <qcustomplot.h>

#include <QApplication>

#include <cmath>
#include <numbers>
#include <vector>

double bivariateNormal(double x, double y, double sx, double sy, double mx, double my)
{
    const double u = (x - mx) / sx, v = (y - my) / sy;
    return std::exp(-0.5 * (u * u + v * v)) / (2 * std::numbers::pi * sx * sy);
}

// matplotlib's sample axes_grid/bivariate_normal.npy (15 x 15, step 0.5 from (-3, -4)), pasted
// at row 30, column 30 of a 150 x 150 zero image.
double image(int column, int row)
{
    const int i = column - 30, j = row - 30;
    if (i < 0 || i >= 15 || j < 0 || j >= 15)
        return 0;
    const double x = -3 + 0.5 * i, y = -4 + 0.5 * j;
    return 10 * (bivariateNormal(x, y, 1.0, 1.0, 0, 0) - bivariateNormal(x, y, 1.5, 0.5, 1, 1));
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QCustomPlot plot;
    plot.resize(560, 540);

    // The image covers x in [-3, 4] and y in [-4, 3]; one cell centre per pixel of it.
    std::vector<double> x, y, z;
    for (int i = 0; i < 150; ++i)
    {
        x.push_back(-3 + (i + 0.5) * 7.0 / 150);
        y.push_back(-4 + (i + 0.5) * 7.0 / 150);
    }
    for (int column = 0; column < 150; ++column)
        for (int row = 0; row < 150; ++row)
            z.push_back(image(column, row));

    const QCPColorMapOptions look { .gradient = QCPColorGradient::gpViridis, .colorBar = false };
    auto ax = plot.axisRect();
    ax->colorMap(x, y, z, look);
    ax->xAxis()->setRange(-3, 4);
    ax->yAxis()->setRange(-4, 3);

    auto inset = ax->addInset(QRectF(0.5, 0.03, 0.47, 0.47));
    inset->colorMap(x, y, z, look);
    inset->xAxis()->setRange(-1.5, -0.9);
    inset->yAxis()->setRange(-2.5, -1.9);
    inset->xAxis()->setTickLabels(false);
    inset->yAxis()->setTickLabels(false);

    ax->zoomIndicator(inset, { .pen = QPen(Qt::black) });

    plot.setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    plot.show();
    return app.exec();
}
