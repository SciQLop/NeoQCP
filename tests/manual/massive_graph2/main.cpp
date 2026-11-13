#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QThread>
#include <cmath>
#include <vector>

#include "qcustomplot.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle("MultiGraph 500M pts");
    window.resize(1200, 700);

    auto* plot = new QCustomPlot;
    plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    window.setCentralWidget(plot);
    window.show();

    constexpr int N = 500'000'000;
    auto* thread = QThread::create([plot]() {
        std::vector<double> keys(N);
        std::vector<double> vals(N);
        for (int i = 0; i < N; ++i)
        {
            double t = i * 1e-6;
            keys[i] = t;
            vals[i] = std::sin(t * 6.28 * 0.5)
                    + 0.3 * std::sin(t * 6.28 * 50.0)
                    + 0.1 * std::sin(t * 6.28 * 5000.0);
        }

        QMetaObject::invokeMethod(plot, [plot, keys = std::move(keys), vals = std::move(vals)]() mutable {
            std::vector<std::vector<double>> columns(1);
            columns[0] = std::move(vals);
            auto* g = new QCPGraph(plot->xAxis, plot->yAxis);
            g->setData(std::move(keys), std::move(columns));
            g->setPen(QPen(QColor(31, 119, 180), 1));
            g->setName("500M points");

            plot->xAxis->setLabel("Time (s)");
            plot->yAxis->setLabel("Amplitude");
            plot->legend->setVisible(true);
            plot->rescaleAxes();
            plot->replot();
        });
    });

    thread->setParent(&window);
    thread->start();

    return app.exec();
}
