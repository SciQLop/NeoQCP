#include "snippets.h"

#include <cmath>

DOC_SNIPPET(concepts_anatomy, 720, 480)
{
    // [concepts_anatomy]
    QVector<double> x(200), y(200);
    for (int i = 0; i < 200; ++i)
    {
        x[i] = i / 20.0;
        y[i] = std::sin(x[i]) * std::exp(-x[i] / 6);
    }
    QCPGraph* graph = customPlot->addGraph();
    graph->setData(x, y);
    graph->setName("plottable: QCPGraph");
    graph->setPen(QPen(QColor(31, 119, 180), 2));
    customPlot->legend->setVisible(true);

    // Show all four axes; the top and right ones follow the bottom and left ones.
    customPlot->axisRect()->setupFullAxesBox(true);
    customPlot->xAxis->setLabel("xAxis (bottom)");
    customPlot->yAxis->setLabel("yAxis (left)");
    customPlot->xAxis2->setLabel("xAxis2 (top, hidden by default)");
    customPlot->yAxis2->setLabel("yAxis2 (right, hidden by default)");
    customPlot->xAxis->setRange(0, 10);
    customPlot->yAxis->setRange(-1, 1.2);

    customPlot->plotLayout()->insertRow(0);
    customPlot->plotLayout()->addElement(0, 0, new QCPTextElement(customPlot,
        "layout element: QCPTextElement", QFont("sans", 12, QFont::Bold)));

    auto* note = new QCPItemText(customPlot);
    note->position->setCoords(3.9, 1.0);
    note->setText("items: QCPItemText, QCPItemLine");
    auto* arrow = new QCPItemLine(customPlot);
    arrow->start->setParentAnchor(note->bottom);
    arrow->end->setCoords(1.45, 0.79);
    arrow->setHead(QCPLineEnding::esSpikeArrow);

    auto* rectLabel = new QCPItemText(customPlot);
    rectLabel->position->setType(QCPItemPosition::ptAxisRectRatio);
    rectLabel->position->setCoords(0.98, 0.96);
    rectLabel->setPositionAlignment(Qt::AlignRight | Qt::AlignBottom);
    rectLabel->setText("axis rect: QCPAxisRect");
    customPlot->replot();
    // [/concepts_anatomy]
}

DOC_SNIPPET(concepts_layers, 600, 360)
{
    // [concepts_layers]
    QVector<double> x(400), y(400);
    for (int i = 0; i < 400; ++i)
    {
        x[i] = i / 40.0;
        y[i] = std::sin(x[i]) + 0.3 * std::sin(5 * x[i]);
    }
    QCPGraph* graph = customPlot->addGraph();
    graph->setData(x, y);
    graph->setPen(QPen(QColor(31, 119, 180), 2));
    graph->setBrush(QColor(174, 199, 232));
    customPlot->xAxis->setRange(0, 10);
    customPlot->yAxis->setRange(-1.5, 1.5);

    // Grid lines now draw over the filled graph instead of under it.
    (void)customPlot->moveLayer(customPlot->layer("grid"), customPlot->layer("main"));

    // A new layer below "main": what goes on it stays behind the data.
    (void)customPlot->addLayer("bands", customPlot->layer("main"), QCustomPlot::limBelow);
    auto* band = new QCPItemRect(customPlot);
    band->setLayer("bands");
    band->topLeft->setCoords(6, 10);
    band->bottomRight->setCoords(8, -10);
    band->setPen(Qt::NoPen);
    band->setBrush(QColor(255, 187, 120));
    customPlot->replot();
    // [/concepts_layers]
}

DOC_SNIPPET(concepts_coordinates, 600, 360)
{
    // [concepts_coordinates]
    customPlot->xAxis->setRange(0, 10);
    customPlot->yAxis->setRange(0, 5);
    customPlot->replot(); // lays out the axis rect, so pixel positions are known

    const double px = customPlot->xAxis->coordToPixel(6);
    const double py = customPlot->yAxis->coordToPixel(2.5);
    const double key = customPlot->xAxis->pixelToCoord(px); // back to 6

    auto* marker = new QCPItemTracer(customPlot);
    marker->position->setCoords(6, 2.5);
    marker->setStyle(QCPItemTracer::tsCircle);
    marker->setSize(10);
    marker->setPen(QPen(QColor(214, 39, 40), 2));
    auto* label = new QCPItemText(customPlot);
    label->position->setParentAnchor(marker->position);
    label->position->setCoords(0, -12);
    label->setPositionAlignment(Qt::AlignBottom | Qt::AlignHCenter);
    label->setText(QString("data (%1, 2.5) is pixel (%2, %3)").arg(key).arg(px).arg(py));

    QObject::connect(customPlot, &QCustomPlot::mouseMove, customPlot,
                     [customPlot](QMouseEvent* event)
                     {
                         const double x = customPlot->xAxis->pixelToCoord(event->position().x());
                         const double y = customPlot->yAxis->pixelToCoord(event->position().y());
                         customPlot->setToolTip(QString("x = %1, y = %2").arg(x).arg(y));
                     });
    customPlot->replot();
    // [/concepts_coordinates]
}
