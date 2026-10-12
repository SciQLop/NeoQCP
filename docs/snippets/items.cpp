#include "snippets.h"

#include <QImage>

#include <cmath>

DOC_SNIPPET(items_annotations, 600, 360)
{
    // [items_annotations]
    QVector<double> x(400), y(400);
    for (int i = 0; i < 400; ++i)
    {
        x[i] = i / 40.0;
        y[i] = std::exp(-(x[i] - 3) * (x[i] - 3)) + 0.5 * std::exp(-2 * (x[i] - 7) * (x[i] - 7));
    }
    customPlot->addGraph()->setData(x, y);
    customPlot->xAxis->setRange(0, 10);
    customPlot->yAxis->setRange(-0.1, 1.3);

    auto* label = new QCPItemText(customPlot);
    label->position->setCoords(5.2, 1.1); // plot coordinates
    label->setText("main peak");
    label->setPadding(QMargins(6, 3, 6, 3));
    label->setPen(QPen(Qt::gray));
    label->setBrush(Qt::white);
    auto* arrow = new QCPItemLine(customPlot);
    arrow->start->setParentAnchor(label->left);
    arrow->end->setCoords(3.1, 1.0);
    arrow->setHead(QCPLineEnding::esSpikeArrow);

    auto* note = new QCPItemText(customPlot);
    note->position->setCoords(8.8, 0.95);
    note->setText("second peak");
    auto* curve = new QCPItemCurve(customPlot);
    curve->start->setParentAnchor(note->bottom);
    curve->startDir->setParentAnchor(note->bottom);
    curve->startDir->setCoords(0, 50); // pixels below the text
    curve->end->setCoords(7.1, 0.53);
    curve->endDir->setCoords(7.9, 0.53);
    curve->setHead(QCPLineEnding::esSpikeArrow);
    customPlot->replot();
    // [/items_annotations]
}

DOC_SNIPPET(items_tracer_bracket, 600, 360)
{
    // [items_tracer_bracket]
    QVector<double> x(800), y(800);
    for (int i = 0; i < 800; ++i)
    {
        x[i] = i / 40.0;
        y[i] = std::exp(-(x[i] - 10) * (x[i] - 10) / 8) * std::cos(3 * x[i]);
    }
    QCPGraph* graph = customPlot->addGraph();
    graph->setData(x, y);
    customPlot->xAxis->setRange(0, 20);
    customPlot->yAxis->setRange(-1.3, 1.9);

    auto* tracer = new QCPItemTracer(customPlot);
    tracer->setGraph(graph);
    tracer->setGraphKey(12.5);
    tracer->setInterpolating(true);
    tracer->setStyle(QCPItemTracer::tsCircle);
    tracer->setPen(QPen(QColor(214, 39, 40), 2));
    tracer->setBrush(Qt::white);
    tracer->setSize(8);
    auto* value = new QCPItemText(customPlot);
    value->position->setParentAnchor(tracer->position);
    value->position->setCoords(10, -6); // 10 px right, 6 px up
    value->setPositionAlignment(Qt::AlignLeft | Qt::AlignBottom);
    value->setText("tracer at x = 12.5");

    auto* bracket = new QCPItemBracket(customPlot);
    bracket->left->setCoords(5, 1.2);
    bracket->right->setCoords(15, 1.2);
    bracket->setLength(12);
    auto* bracketLabel = new QCPItemText(customPlot);
    bracketLabel->position->setParentAnchor(bracket->center);
    bracketLabel->position->setCoords(0, -6);
    bracketLabel->setPositionAlignment(Qt::AlignBottom | Qt::AlignHCenter);
    bracketLabel->setText("wave packet");
    customPlot->replot();
    // [/items_tracer_bracket]
}

DOC_SNIPPET(items_shapes, 600, 360)
{
    // [items_shapes]
    QVector<double> x(60), y(60);
    for (int i = 0; i < 60; ++i)
    {
        x[i] = i / 6.0;
        y[i] = 0.5 * x[i] + 1 + 0.8 * std::sin(i * 2.7) * std::cos(i * 1.3);
    }
    y[45] += 4; // an outlier at x = 7.5
    QCPGraph* points = customPlot->addGraph();
    points->setData(x, y);
    points->setLineStyle(QCPGraph::lsNone);
    points->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 6));
    customPlot->xAxis->setRange(-0.5, 10.5);
    customPlot->yAxis->setRange(-1, 11);

    auto* trend = new QCPItemStraightLine(customPlot);
    trend->point1->setCoords(0, 1);
    trend->point2->setCoords(10, 6);
    trend->setPen(QPen(QColor(214, 39, 40), 1.5));

    auto* outlier = new QCPItemEllipse(customPlot);
    outlier->topLeft->setCoords(x[45] - 0.4, y[45] + 0.7);
    outlier->bottomRight->setCoords(x[45] + 0.4, y[45] - 0.7);
    outlier->setPen(QPen(QColor(255, 127, 14), 2));

    auto* region = new QCPItemRect(customPlot);
    region->topLeft->setCoords(1, 4);
    region->bottomRight->setCoords(3, 0);
    region->setPen(QPen(QColor(44, 160, 44)));
    region->setBrush(QColor(44, 160, 44, 40));
    customPlot->replot();
    // [/items_shapes]
}

DOC_SNIPPET(items_rich_text_pixmap, 600, 360)
{
    // [items_rich_text_pixmap]
    QVector<double> x(200), y(200);
    for (int i = 0; i < 200; ++i)
    {
        x[i] = i / 20.0;
        y[i] = 0.5 * x[i] + 1 + 0.3 * std::sin(3 * x[i]);
    }
    customPlot->addGraph()->setData(x, y);
    customPlot->xAxis->setRange(0, 10);
    customPlot->yAxis->setRange(0, 7.5);

    auto* summary = new QCPItemRichText(customPlot);
    summary->position->setType(QCPItemPosition::ptAxisRectRatio);
    summary->position->setCoords(0.97, 0.94);
    summary->setPositionAlignment(Qt::AlignRight | Qt::AlignBottom);
    summary->setHtml("<b>Linear fit</b><br>slope = <span style='color:#d62728'>0.50</span>"
                     "<br>R<sup>2</sup> = 0.97");
    summary->setPadding(QMargins(8, 6, 8, 6));
    summary->setPen(QPen(Qt::gray));
    summary->setBrush(QColor(255, 255, 255, 230));
    summary->setCornerRadius(4);

    QImage image(48, 48, QImage::Format_ARGB32);
    for (int px = 0; px < 48; ++px)
        for (int py = 0; py < 48; ++py)
            image.setPixelColor(px, py, QColor::fromHsv((px * 5 + py * 2) % 360, 150, 230));
    auto* logo = new QCPItemPixmap(customPlot);
    logo->setPixmap(QPixmap::fromImage(image));
    logo->topLeft->setType(QCPItemPosition::ptAxisRectAbsolute);
    logo->topLeft->setCoords(10, 10); // pixels from the axis rect's top-left corner
    customPlot->replot();
    // [/items_rich_text_pixmap]
}

DOC_SNIPPET(items_positions, 600, 380)
{
    // [items_positions]
    QVector<double> x(200), y(200);
    for (int i = 0; i < 200; ++i)
    {
        x[i] = i / 20.0;
        y[i] = std::sin(x[i]);
    }
    customPlot->addGraph()->setData(x, y);
    customPlot->xAxis->setRange(0, 10);
    customPlot->yAxis->setRange(-1.5, 1.5);
    customPlot->xAxis->setLabel("time (s)");

    auto* event = new QCPItemStraightLine(customPlot);
    event->point1->setCoords(6, 0);
    event->point2->setCoords(6, 1);
    event->setPen(QPen(QColor(214, 39, 40), 1.5));
    auto* eventLabel = new QCPItemText(customPlot);
    eventLabel->position->setTypeX(QCPItemPosition::ptPlotCoords);
    eventLabel->position->setTypeY(QCPItemPosition::ptAxisRectRatio);
    eventLabel->position->setCoords(6.1, 0.03); // x = 6 in data, y = 3 % below the top
    eventLabel->setPositionAlignment(Qt::AlignLeft | Qt::AlignTop);
    eventLabel->setText("event at t = 6 s");

    auto* corner = new QCPItemText(customPlot);
    corner->position->setType(QCPItemPosition::ptAxisRectRatio);
    corner->position->setCoords(0.02, 0.97);
    corner->setPositionAlignment(Qt::AlignLeft | Qt::AlignBottom);
    corner->setText("axis rect ratio (0.02, 0.97)");

    auto* stamp = new QCPItemText(customPlot);
    stamp->position->setType(QCPItemPosition::ptViewportRatio);
    stamp->position->setCoords(0.99, 0.99);
    stamp->setPositionAlignment(Qt::AlignRight | Qt::AlignBottom);
    stamp->setClipToAxisRect(false); // it sits in the margin, outside the axis rect
    stamp->setColor(Qt::gray);
    stamp->setText("viewport ratio (0.99, 0.99)");
    customPlot->replot();
    // [/items_positions]
}

DOC_SNIPPET(items_spans, 600, 360)
{
    // [items_spans]
    QVector<double> t(1000), v(1000);
    for (int i = 0; i < 1000; ++i)
    {
        t[i] = i / 10.0;
        v[i] = std::sin(t[i] / 8) + 0.3 * std::sin(0.9 * t[i]);
    }
    customPlot->addGraph()->setData(t, v);
    customPlot->xAxis->setRange(0, 100);
    customPlot->yAxis->setRange(-2, 2);

    auto* event = new QCPItemVSpan(customPlot);
    event->setRange(QCPRange(20, 35));
    event->setPen(Qt::NoPen);
    event->setBrush(QColor(255, 127, 14, 50));
    event->setBorderPen(QPen(QColor(255, 127, 14), 2));

    auto* band = new QCPItemHSpan(customPlot);
    band->setRange(QCPRange(-0.5, 0.5));
    band->setPen(Qt::NoPen);
    band->setBrush(QColor(44, 160, 44, 40));
    band->setBorderPen(QPen(QColor(44, 160, 44), 1));
    band->setMovable(false);

    auto* box = new QCPItemRSpan(customPlot);
    box->setKeyRange(QCPRange(60, 80));
    box->setValueRange(QCPRange(0.8, 1.6));
    box->setPen(Qt::NoPen);
    box->setBrush(QColor(148, 103, 189, 50));
    box->setBorderPen(QPen(QColor(148, 103, 189), 2));

    QObject::connect(event, &QCPItemVSpan::rangeChanged, customPlot,
                     [](const QCPRange& range) { qDebug() << "event:" << range.lower << range.upper; });
    customPlot->replot();
    // [/items_spans]
}

DOC_SNIPPET(items_creation, 600, 360)
{
    // [items_creation]
    customPlot->xAxis->setRange(0, 100);
    customPlot->yAxis->setRange(-2, 2);

    customPlot->setItemCreator([](QCustomPlot* plot, QCPAxis*, QCPAxis*) -> QCPAbstractItem*
    {
        auto* span = new QCPItemVSpan(plot);
        span->setPen(Qt::NoPen);
        span->setBrush(QColor(255, 127, 14, 50));
        span->setBorderPen(QPen(QColor(255, 127, 14), 2));
        return span;
    });
    customPlot->setItemPositioner([](QCPAbstractItem* item, double anchorKey, double,
                                     double currentKey, double)
    {
        static_cast<QCPItemVSpan*>(item)->setRange(QCPRange(anchorKey, currentKey));
    });
    customPlot->setCreationModeEnabled(true); // or hold Shift on the first click

    QObject::connect(customPlot, &QCustomPlot::itemCreated, customPlot,
                     [](QCPAbstractItem* item)
                     { qDebug() << "new span:" << static_cast<QCPItemVSpan*>(item)->range().lower; });
    customPlot->replot();
    // [/items_creation]
}

DOC_CODE(items_overlay)
{
    // [items_overlay]
    customPlot->xAxis->setRange(0, 100);
    customPlot->yAxis->setRange(-2, 2);

    QCPOverlay* overlay = customPlot->overlay(); // created on first use
    overlay->setCollapsible(true);
    overlay->setOpacity(0.85);
    overlay->showMessage("Loading data from the archive...", QCPOverlay::Info);
    customPlot->replot();
    // [/items_overlay]
}
