#include "rangeband.h"

#include <qcustomplot.h>

#include <QApplication>
#include <QtMath>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QCustomPlot plot;
    plot.resize(640, 360);
    plot.xAxis->setLabel(QStringLiteral("key"));
    plot.yAxis->setLabel(QStringLiteral("value"));

    auto* band = new RangeBand(plot.xAxis, plot.yAxis);
    QVector<double> keys, lower, upper;
    for (int i = 0; i <= 60; ++i)
    {
        const double key = 10.0 * i / 60.0;
        keys << key;
        lower << -0.5 + 0.4 * qSin(key);
        upper << 0.5 + 0.4 * qCos(key);
    }
    band->setData(keys, lower, upper);
    band->setLabel(QStringLiteral("range band"));
    plot.rescaleAxes();
    plot.show();
    return app.exec();
}
