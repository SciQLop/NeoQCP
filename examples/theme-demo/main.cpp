#include <QApplication>
#include <QHBoxLayout>
#include <QWidget>
#include "qcustomplot.h"

static void populatePlot(QCustomPlot* plot)
{
    plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables
                          | QCP::iSelectAxes | QCP::iSelectLegend | QCP::iSelectItems
                          | QCP::iMultiSelect);

    auto graph1 = plot->add<QCPGraph>();
    auto graph2 = plot->add<QCPGraph>();
    graph1->setPen(QPen(QColor("#e74c3c"), 2));
    graph2->setPen(QPen(QColor("#3498db"), 2));
    graph1->setSelectable(QCP::stWhole);
    graph2->setSelectable(QCP::stWhole);

    QVector<double> x(200), y1(200), y2(200);
    for (int i = 0; i < 200; ++i) {
        x[i] = i / 25.0 - 4;
        y1[i] = qExp(-x[i] * x[i] / 2.0) * qSin(5 * x[i]);
        y2[i] = qExp(-x[i] * x[i] / 4.0) * qCos(3 * x[i]);
    }
    graph1->setData(x, y1);
    graph2->setData(x, y2);

    plot->xAxis->setLabel("x");
    plot->yAxis->setLabel("y");
    plot->xAxis->setSelectableParts(QCPAxis::spAxis | QCPAxis::spTickLabels | QCPAxis::spAxisLabel);
    plot->yAxis->setSelectableParts(QCPAxis::spAxis | QCPAxis::spTickLabels | QCPAxis::spAxisLabel);

    plot->legend->setVisible(true);
    plot->legend->setSelectableParts(QCPLegend::spItems | QCPLegend::spLegendBox);
    graph1->setName("exp\u00B7sin");
    graph2->setName("exp\u00B7cos");

    auto* title = new QCPTextElement(plot, "Click to select elements");
    title->setSelectable(QCP::stWhole);
    plot->plotLayout()->insertRow(0);
    plot->plotLayout()->addElement(0, 0, title);

    auto textItem = plot->add<QCPItemText>();
    textItem->position->setCoords(0, 0.5);
    textItem->setText("Selectable\nAnnotation");
    textItem->setSelectable(QCP::stWhole);
    textItem->setPen(QPen(Qt::gray));
    textItem->setPadding(QMargins(4, 4, 4, 4));

    plot->rescaleAxes();
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    auto* window = new QWidget;
    window->setWindowTitle("QCPTheme Demo \u2014 Light vs Dark (click elements to see selection colors)");
    window->resize(1200, 500);

    auto* layout = new QHBoxLayout(window);

    auto* lightPlot = new QCustomPlot;
    auto* darkPlot = new QCustomPlot;

    populatePlot(lightPlot);
    populatePlot(darkPlot);

    lightPlot->setTheme(QCPTheme::light(lightPlot));
    darkPlot->setTheme(QCPTheme::dark(darkPlot));

    layout->addWidget(lightPlot);
    layout->addWidget(darkPlot);

    window->show();
    return app.exec();
}
