#include "snippets.h"

#include <cmath>
#include <numbers>

DOC_SNIPPET(polar_basic, 600, 420)
{
    // [polar_basic]
    customPlot->plotLayout()->clear();   // drop the default axis rect
    auto* angularAxis = new QCPPolarAxisAngular(customPlot);
    customPlot->plotLayout()->addElement(0, 0, angularAxis);
    QCPPolarAxisRadial* radialAxis = angularAxis->radialAxis();
    angularAxis->setRange(0, 360);   // degrees, 0 on top, clockwise
    radialAxis->setRange(0, 1.05);

    const QList<std::pair<double, QColor>> patterns {   // r = a + (1 - a) cos(angle)
        { 1.0, QColor(127, 127, 127) },                  // omnidirectional
        { 0.5, QColor(31, 119, 180) },                   // cardioid
        { 0.0, QColor(214, 39, 40) },                    // figure eight
    };
    for (const auto& [a, color] : patterns)
    {
        QVector<double> angle(361), gain(361);
        for (int i = 0; i <= 360; ++i)
        {
            angle[i] = i;
            gain[i] = std::abs(a + (1 - a) * std::cos(i * std::numbers::pi / 180));
        }
        auto* graph = new QCPPolarGraph(angularAxis, radialAxis);
        graph->setData(angle, gain);
        graph->setPen(QPen(color, 2));
    }
    customPlot->replot();
    // [/polar_basic]
}

DOC_SNIPPET(polar_radians, 600, 420)
{
    // [polar_radians]
    customPlot->plotLayout()->clear();
    auto* angularAxis = new QCPPolarAxisAngular(customPlot);
    customPlot->plotLayout()->addElement(0, 0, angularAxis);
    angularAxis->setAngle(0);              // 0 on the right ...
    angularAxis->setRangeReversed(true);   // ... and counter-clockwise, as in mathematics
    angularAxis->setRange(0, 2 * std::numbers::pi);
    auto ticker = QSharedPointer<QCPAxisTickerPi>::create();
    ticker->setTickCount(8);
    angularAxis->setTicker(ticker);
    angularAxis->grid()->setSubGridType(QCPPolarGrid::gtAll);
    angularAxis->radialAxis()->setRange(0, 1);

    QVector<double> theta(721), r(721);
    for (int i = 0; i <= 720; ++i)
    {
        theta[i] = i * std::numbers::pi / 360;   // 0 to 2 pi
        r[i] = std::abs(std::cos(3 * theta[i]));  // a six-petal rose
    }
    auto* rose = new QCPPolarGraph(angularAxis, angularAxis->radialAxis());
    rose->setData(theta, r);
    rose->setPen(QPen(QColor(148, 103, 189), 2));
    rose->setBrush(QColor(148, 103, 189, 60));
    customPlot->replot();
    // [/polar_radians]
}
