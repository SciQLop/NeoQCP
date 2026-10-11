#include "snippets.h"

#include <cmath>
#include <numbers>

DOC_SNIPPET(layout_title, 600, 380)
{
    // [layout_title]
    QVector<double> x(201), y(201);
    for (int i = 0; i < 201; ++i)
    {
        x[i] = i / 20.0;
        y[i] = std::exp(-0.3 * x[i]) * std::cos(2 * x[i]);
    }
    QCPGraph* graph = customPlot->addGraph();
    graph->setData(x, y);
    graph->setPen(QPen(QColor(31, 119, 180), 2));
    customPlot->rescaleAxes();

    customPlot->plotLayout()->insertRow(0); // the axis rect moves down to row 1
    auto* title = new QCPTextElement(customPlot, "Damped oscillation", QFont("sans", 12, QFont::Bold));
    customPlot->plotLayout()->addElement(0, 0, title);
    customPlot->replot();
    // [/layout_title]
}

DOC_SNIPPET(layout_legend_inside, 600, 360)
{
    // [layout_legend_inside]
    QVector<double> x(200), s(200), c(200);
    for (int i = 0; i < 200; ++i)
    {
        x[i] = i * 2 * std::numbers::pi / 199;
        s[i] = std::sin(x[i]);
        c[i] = std::cos(x[i]);
    }
    customPlot->addGraph()->setData(x, s);
    customPlot->graph(0)->setName("sin(x)");
    customPlot->graph(0)->setPen(QPen(QColor(31, 119, 180), 2));
    customPlot->addGraph()->setData(x, c);
    customPlot->graph(1)->setName("cos(x)");
    customPlot->graph(1)->setPen(QPen(QColor(255, 127, 14), 2));
    customPlot->xAxis->setRange(0, 2 * std::numbers::pi);
    customPlot->yAxis->setRange(-1.2, 1.2);

    customPlot->legend->setVisible(true);
    customPlot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignBottom | Qt::AlignLeft);

    customPlot->legend->insertRow(0);
    auto* legendTitle = new QCPTextElement(customPlot, "Signals", QFont("sans", 9, QFont::Bold));
    legendTitle->setLayer(customPlot->legend->layer()); // else the legend background hides it
    customPlot->legend->addElement(0, 0, legendTitle);
    customPlot->replot();
    // [/layout_legend_inside]
}

DOC_SNIPPET(layout_legend_outside, 600, 400)
{
    // [layout_legend_outside]
    const QColor colors[] = { QColor(31, 119, 180), QColor(255, 127, 14), QColor(44, 160, 44) };
    for (int g = 0; g < 3; ++g)
    {
        QVector<double> x(200), y(200);
        for (int i = 0; i < 200; ++i)
        {
            x[i] = i / 20.0;
            y[i] = std::sin(x[i] + g) * std::exp(-x[i] / (4 + 3 * g));
        }
        QCPGraph* graph = customPlot->addGraph();
        graph->setData(x, y);
        graph->setName(QString("channel %1").arg(g + 1));
        graph->setPen(QPen(colors[g], 2));
    }
    customPlot->rescaleAxes();
    customPlot->legend->setVisible(true);

    auto* legendRow = new QCPLayoutGrid;
    customPlot->plotLayout()->addElement(1, 0, legendRow);
    legendRow->setMargins(QMargins(5, 0, 5, 5));
    legendRow->addElement(0, 0, customPlot->legend); // takes it out of the axis rect
    customPlot->legend->setFillOrder(QCPLegend::foColumnsFirst);
    customPlot->plotLayout()->setRowStretchFactor(1, 0.001);
    customPlot->replot();
    // [/layout_legend_outside]
}

DOC_SNIPPET(layout_axis_rects, 720, 520)
{
    // [layout_axis_rects]
    QCPAxisRect* top = customPlot->axisRect();
    auto* bottomLeft = new QCPAxisRect(customPlot);
    auto* bottomRight = new QCPAxisRect(customPlot);
    auto* bottomRow = new QCPLayoutGrid;
    customPlot->plotLayout()->addElement(1, 0, bottomRow);
    bottomRow->addElement(0, 0, bottomLeft);
    bottomRow->addElement(0, 1, bottomRight);
    customPlot->plotLayout()->setRowStretchFactor(0, 2); // top row twice as tall
    bottomRow->setColumnSpacing(10);

    auto plotIn = [customPlot](QCPAxisRect* rect, double frequency, const QColor& color)
    {
        QVector<double> x(300), y(300);
        for (int i = 0; i < 300; ++i)
        {
            x[i] = i / 30.0;
            y[i] = std::sin(frequency * x[i]) * std::exp(-x[i] / 5);
        }
        QCPGraph* graph = customPlot->addGraph(rect->axis(QCPAxis::atBottom),
                                               rect->axis(QCPAxis::atLeft));
        graph->setData(x, y);
        graph->setPen(QPen(color, 2));
        graph->rescaleAxes();
    };
    plotIn(top, 1, QColor(31, 119, 180));
    plotIn(bottomLeft, 3, QColor(255, 127, 14));
    plotIn(bottomRight, 6, QColor(44, 160, 44));
    customPlot->replot();
    // [/layout_axis_rects]
}

DOC_SNIPPET(layout_margin_group, 600, 460)
{
    // [layout_margin_group]
    auto* lower = new QCPAxisRect(customPlot);
    customPlot->plotLayout()->addElement(1, 0, lower);

    QVector<double> t(300), ratio(300), counts(300);
    for (int i = 0; i < 300; ++i)
    {
        t[i] = i / 30.0;
        ratio[i] = std::sin(t[i]);
        counts[i] = 40000 * (1 + std::cos(0.7 * t[i]));
    }
    customPlot->addGraph()->setData(t, ratio);
    QCPGraph* lowerGraph = customPlot->addGraph(lower->axis(QCPAxis::atBottom),
                                                lower->axis(QCPAxis::atLeft));
    lowerGraph->setData(t, counts);
    lowerGraph->setPen(QPen(QColor(255, 127, 14)));
    customPlot->rescaleAxes();

    auto* group = new QCPMarginGroup(customPlot);
    customPlot->axisRect()->setMarginGroup(QCP::msLeft | QCP::msRight, group);
    lower->setMarginGroup(QCP::msLeft | QCP::msRight, group);

    QObject::connect(customPlot->xAxis, qOverload<const QCPRange&>(&QCPAxis::rangeChanged),
                     lower->axis(QCPAxis::atBottom), qOverload<const QCPRange&>(&QCPAxis::setRange));
    customPlot->replot();
    // [/layout_margin_group]
}

DOC_SNIPPET(layout_color_scale, 640, 440)
{
    // [layout_color_scale]
    auto* colorMap = new QCPColorMap(customPlot->xAxis, customPlot->yAxis);
    colorMap->data()->setSize(200, 200);
    colorMap->data()->setRange(QCPRange(-4, 4), QCPRange(-4, 4));
    for (int ix = 0; ix < 200; ++ix)
        for (int iy = 0; iy < 200; ++iy)
        {
            double x, y;
            colorMap->data()->cellToCoord(ix, iy, &x, &y);
            colorMap->data()->setCell(ix, iy, std::sin(x) * std::cos(y) * std::exp(-(x * x + y * y) / 16));
        }

    auto* colorScale = new QCPColorScale(customPlot);
    customPlot->plotLayout()->addElement(0, 1, colorScale); // right of the axis rect
    colorScale->axis()->setLabel("amplitude");
    colorMap->setColorScale(colorScale);
    colorMap->setGradient(QCPColorGradient::gpViridis);
    colorMap->rescaleDataRange(true);
    customPlot->rescaleAxes();

    auto* group = new QCPMarginGroup(customPlot);
    customPlot->axisRect()->setMarginGroup(QCP::msTop | QCP::msBottom, group);
    colorScale->setMarginGroup(QCP::msTop | QCP::msBottom, group);
    customPlot->replot();
    // [/layout_color_scale]
}
