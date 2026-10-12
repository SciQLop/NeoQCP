#include "snippets.h"

#include <QApplication>
#include <QStatusBar>

#include <cmath>
#include <memory>

DOC_SNIPPET(interactions_drag_zoom, 600, 360)
{
    // [interactions_drag_zoom]
    QVector<double> t(400), v(400);
    for (int i = 0; i < 400; ++i)
    {
        t[i] = i * 0.05;
        v[i] = std::sin(t[i]) + 0.3 * std::sin(5.3 * t[i]);
    }
    customPlot->addGraph()->setData(t, v);
    customPlot->rescaleAxes();
    customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    QCPAxisRect* rect = customPlot->axisRect();
    rect->setRangeDrag(Qt::Horizontal); // pan along x only
    rect->setRangeZoom(Qt::Horizontal); // zoom along x only
    rect->setRangeZoomFactor(0.7);      // stronger zoom per wheel step than the default 0.85
    customPlot->replot();
    // [/interactions_drag_zoom]
}

DOC_SNIPPET(interactions_drag_axes, 600, 360)
{
    // [interactions_drag_axes]
    QVector<double> t(200), temperature(200), pressure(200);
    for (int i = 0; i < 200; ++i)
    {
        t[i] = i * 0.1;
        temperature[i] = 20 + 5 * std::sin(t[i] / 3);
        pressure[i] = 1013 + 8 * std::cos(t[i] / 2);
    }
    customPlot->yAxis2->setVisible(true);
    QCPGraph* temp = customPlot->addGraph(customPlot->xAxis, customPlot->yAxis);
    QCPGraph* pres = customPlot->addGraph(customPlot->xAxis, customPlot->yAxis2);
    temp->setData(t, temperature);
    pres->setData(t, pressure);
    temp->setPen(QPen(QColor(214, 39, 40), 2));
    pres->setPen(QPen(QColor(31, 119, 180), 2));
    customPlot->yAxis->setLabel("temperature (°C)");
    customPlot->yAxis2->setLabel("pressure (hPa)");
    temp->rescaleAxes();
    pres->rescaleAxes();

    customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    const QList<QCPAxis*> horizontal { customPlot->xAxis };
    const QList<QCPAxis*> vertical { customPlot->yAxis, customPlot->yAxis2 };
    customPlot->axisRect()->setRangeDragAxes(horizontal, vertical);
    customPlot->axisRect()->setRangeZoomAxes(horizontal, vertical);
    customPlot->replot();
    // [/interactions_drag_axes]
}

DOC_SNIPPET(interactions_select, 600, 360)
{
    // [interactions_select]
    QVector<double> x(100), a(100), b(100);
    for (int i = 0; i < 100; ++i)
    {
        x[i] = i / 10.0;
        a[i] = std::sin(x[i]);
        b[i] = 0.5 * std::cos(x[i]);
    }
    QCPGraph* first = customPlot->addGraph();
    first->setData(x, a);
    first->setName("sin");
    QCPGraph* second = customPlot->addGraph();
    second->setData(x, b);
    second->setName("cos / 2");
    second->setPen(QPen(QColor(255, 127, 14)));
    customPlot->rescaleAxes();
    customPlot->legend->setVisible(true);

    customPlot->setInteractions(QCP::iSelectPlottables | QCP::iSelectAxes | QCP::iSelectLegend
                                | QCP::iMultiSelect);
    customPlot->setMultiSelectModifier(Qt::ShiftModifier);
    // What a click on the first graph, then a shift-click on the x axis, leaves selected:
    first->setSelection(QCPDataSelection(first->data()->dataRange()));
    customPlot->xAxis->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels);
    customPlot->replot();
    // [/interactions_select]
}

DOC_SNIPPET(interactions_selection_types, 600, 360)
{
    // [interactions_selection_types]
    QVector<double> x(60), y(60);
    for (int i = 0; i < 60; ++i)
    {
        x[i] = i;
        y[i] = 10 * std::sin(i / 6.0);
    }
    QCPGraph* graph = customPlot->addGraph();
    graph->setData(x, y);
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 5));
    graph->setSelectable(QCP::stDataRange);

    auto* decorator = new QCPSelectionDecoratorBracket;
    decorator->setPen(QPen(QColor(214, 39, 40), 2.5));
    decorator->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, QColor(214, 39, 40), 7),
                               QCPScatterStyle::spPen | QCPScatterStyle::spShape
                                   | QCPScatterStyle::spSize);
    decorator->setBracketPen(QPen(Qt::black, 1.5));
    decorator->setBracketHeight(30);
    decorator->setTangentToData(true);
    graph->setSelectionDecorator(decorator); // the graph owns it now

    graph->setSelection(QCPDataSelection(QCPDataRange(20, 36)));
    customPlot->rescaleAxes();
    customPlot->setInteractions(QCP::iSelectPlottables);
    customPlot->replot();
    // [/interactions_selection_types]
}

DOC_SNIPPET(interactions_selection_mean, 600, 360)
{
    // [interactions_selection_mean]
    QVector<double> x(200), y(200);
    for (int i = 0; i < 200; ++i)
    {
        x[i] = i * 0.05;
        y[i] = std::sin(x[i]) + 0.2 * std::sin(7 * x[i]);
    }
    QCPGraph* graph = customPlot->addGraph();
    graph->setData(x, y);
    graph->setSelectable(QCP::stMultipleDataRanges);
    graph->setSelection(QCPDataSelection(QCPDataRange(10, 50))
                        + QCPDataSelection(QCPDataRange(120, 150)));
    customPlot->rescaleAxes();

    double sum = 0;
    for (const QCPDataRange& range : graph->selection().dataRanges())
        for (int i = range.begin(); i < range.end(); ++i)
            sum += graph->data()->at(i)->value;
    const double mean = sum / graph->selection().dataPointCount();

    auto* meanLine = new QCPItemStraightLine(customPlot);
    meanLine->point1->setCoords(0, mean);
    meanLine->point2->setCoords(1, mean);
    meanLine->setPen(QPen(QColor(44, 160, 44), 1.5, Qt::DashLine));
    customPlot->replot();
    // [/interactions_selection_mean]
}

DOC_SNIPPET(interactions_selection_rect, 600, 360)
{
    // [interactions_selection_rect]
    const int n = 300;
    QVector<double> x(n), y(n);
    for (int i = 0; i < n; ++i)
    {
        x[i] = i * 0.033;
        y[i] = std::sin(i * 1.7) * std::cos(i * 0.37);
    }
    QCPGraph* cloud = customPlot->addGraph();
    cloud->setData(x, y);
    cloud->setLineStyle(QCPGraph::lsNone);
    cloud->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, QColor(31, 119, 180), 5));
    cloud->setSelectable(QCP::stMultipleDataRanges);
    cloud->selectionDecorator()->setScatterStyle(
        QCPScatterStyle(QCPScatterStyle::ssDisc, QColor(214, 39, 40), 5), QCPScatterStyle::spPen);
    customPlot->rescaleAxes();

    customPlot->setInteractions(QCP::iSelectPlottables | QCP::iMultiSelect);
    customPlot->setSelectionRectMode(QCP::srmSelect);
    customPlot->selectionRect()->setBrush(QColor(31, 119, 180, 30));

    // What dragging from (3, 0.7) to (6, -0.2) selects:
    QCPDataSelection inside;
    for (int i = 0; i < n; ++i)
        if (x[i] >= 3 && x[i] <= 6 && y[i] >= -0.2 && y[i] <= 0.7)
            inside.addDataRange(QCPDataRange(i, i + 1));
    cloud->setSelection(inside);
    auto* dragged = new QCPItemRect(customPlot);
    dragged->topLeft->setCoords(3, 0.7);
    dragged->bottomRight->setCoords(6, -0.2);
    dragged->setPen(customPlot->selectionRect()->pen());
    dragged->setBrush(customPlot->selectionRect()->brush());
    customPlot->replot();
    // [/interactions_selection_rect]
}

DOC_SNIPPET(interactions_data_locator, 600, 360)
{
    // [interactions_data_locator]
    QVector<double> t(300), v(300);
    for (int i = 0; i < 300; ++i)
    {
        t[i] = i * 0.03;
        v[i] = std::sin(t[i]) * std::exp(-t[i] / 6);
    }
    QCPGraph* graph = customPlot->addGraph();
    graph->setData(t, v);
    customPlot->rescaleAxes();

    QCPDataLocator locator;
    if (locator.locateAtKey(graph, 3.7))
    {
        auto* tracer = new QCPItemTracer(customPlot);
        tracer->position->setCoords(locator.key(), locator.value());
        tracer->setStyle(QCPItemTracer::tsCircle);
        tracer->setSize(9);
        tracer->setPen(QPen(QColor(214, 39, 40), 2));
        auto* label = new QCPItemText(customPlot);
        label->position->setCoords(locator.key(), locator.value());
        label->setPositionAlignment(Qt::AlignLeft | Qt::AlignBottom);
        label->setPadding(QMargins(8, 0, 0, 8));
        label->setText(QString("index %1: t = %2, v = %3")
                           .arg(locator.dataIndex())
                           .arg(locator.key(), 0, 'f', 2)
                           .arg(locator.value(), 0, 'f', 3));
    }
    customPlot->replot();
    // [/interactions_data_locator]
}

DOC_CODE(interactions_rect_mode_switch)
{
    // [interactions_rect_mode_switch]
    QObject::connect(customPlot, &QCustomPlot::mousePress, [customPlot](QMouseEvent* event) {
        customPlot->setSelectionRectMode(event->modifiers() & Qt::ShiftModifier ? QCP::srmZoom
                                                                                : QCP::srmNone);
    });
    // [/interactions_rect_mode_switch]
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(300, 180), QPointF(300, 180),
                      Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier);
    QApplication::sendEvent(customPlot, &press);
    if (customPlot->selectionRectMode() != QCP::srmZoom)
        qFatal("interactions_rect_mode_switch: Shift did not switch to zoom");
}

DOC_CODE(interactions_signals)
{
    customPlot->addGraph()->setData(QVector<double> { 0, 1, 2 }, QVector<double> { 1, 0, 1 });
    // [interactions_signals]
    QObject::connect(customPlot->xAxis, qOverload<const QCPRange&>(&QCPAxis::rangeChanged),
                     [](const QCPRange& range) { qDebug() << "x now spans" << range.lower << range.upper; });

    QObject::connect(customPlot, &QCustomPlot::plottableClick,
                     [](QCPAbstractPlottable* plottable, int dataIndex, QMouseEvent*) {
                         qDebug() << plottable->name() << "clicked at point" << dataIndex;
                     });

    QObject::connect(customPlot, &QCustomPlot::selectionChangedByUser, [customPlot] {
        for (QCPGraph* graph : customPlot->selectedGraphs())
            qDebug() << graph->name() << "has" << graph->selection().dataPointCount() << "selected points";
    });
    // [/interactions_signals]
    customPlot->xAxis->setRange(0, 2);
}

DOC_CODE(interactions_locate_on_move)
{
    QCPGraph* graph = customPlot->addGraph();
    graph->setData(QVector<double> { 0, 1, 2 }, QVector<double> { 0, 1, 0 });
    customPlot->xAxis->setRange(-1, 3);
    customPlot->yAxis->setRange(-1, 2);
    customPlot->replot();
    auto* statusBar = new QStatusBar(customPlot);
    // [interactions_locate_on_move]
    auto locator = std::make_shared<QCPDataLocator>();
    locator->setPlottable(graph);
    QObject::connect(customPlot, &QCustomPlot::mouseMove, [=](QMouseEvent* event) {
        if (locator->locate(event->position()))
            statusBar->showMessage(QString("t = %1, v = %2").arg(locator->key()).arg(locator->value()));
    });
    // [/interactions_locate_on_move]
    const QPointF pos(customPlot->xAxis->coordToPixel(1), customPlot->yAxis->coordToPixel(1));
    QMouseEvent move(QEvent::MouseMove, pos, pos, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(customPlot, &move);
    if (statusBar->currentMessage() != "t = 1, v = 1")
        qFatal("interactions_locate_on_move: got \"%s\"", qPrintable(statusBar->currentMessage()));
}

DOC_CODE(interactions_busy)
{
    QCPGraph* graph = customPlot->addGraph();
    // [interactions_busy]
    graph->setBusy(true);
    // ... when the new data is in:
    graph->setBusy(false);
    // [/interactions_busy]
}
