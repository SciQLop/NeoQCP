// Target program (milestone 6): a fill between two lines, coloured by which is on top.
// https://matplotlib.org/stable/gallery/lines_bars_and_markers/fill_between_demo.html
// (section "Selectively filling horizontal regions", the interpolate=True panel)

#include <qcustomplot.h>

#include <QApplication>

#include <vector>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QCustomPlot plot;
    plot.resize(640, 400);

    const std::vector<double> x { 0, 1, 2, 3 };
    const std::vector<double> y1 { 0.8, 0.8, 0.2, 0.2 };
    std::vector<double> y2 { 0, 0, 1, 1 };

    const QColor c0(31, 119, 180), c1(255, 127, 14);
    auto ax = plot.axisRect();
    auto first = ax->line(x, y1, { .pen = QPen(c0, 1.5, Qt::DashLine),
                                   .scatterStyle = QCPScatterStyle(QCPScatterStyle::ssDisc, c0, 7) });
    auto second = ax->line(x, y2, { .pen = QPen(c1, 1.5, Qt::DashLine),
                                    .scatterStyle = QCPScatterStyle(QCPScatterStyle::ssDisc, c1, 7) });
    ax->fillBetween(first, second, { .brush = QColor(44, 160, 44, 76), .belowBrush = QColor(214, 39, 40, 76) });
    ax->setTitle("interpolation=True");

    // Up and Down move the orange line; the fill follows its new data.
    QObject::connect(&plot, &QCustomPlot::keyPress, &plot, [&](QKeyEvent* event) {
        const double step = event->key() == Qt::Key_Up ? 0.1 : event->key() == Qt::Key_Down ? -0.1 : 0.0;
        for (double& value : y2)
            value += step;
        second->setData(x, y2);
    });

    plot.rescaleAxes();
    plot.show();
    return app.exec();
}
