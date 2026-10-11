#include "snippets.h"

#include <cmath>

DOC_SNIPPET(styling_dark_theme, 600, 360)
{
    // [styling_dark_theme]
    QVector<double> t(300), a(300), b(300);
    for (int i = 0; i < 300; ++i)
    {
        t[i] = i / 30.0;
        a[i] = std::sin(t[i]) * std::exp(-t[i] / 8);
        b[i] = std::cos(t[i]) * std::exp(-t[i] / 8);
    }
    QCPGraph* sine = customPlot->addGraph();
    sine->setData(t, a);
    sine->setName("sine");
    sine->setPen(QPen(QColor(77, 166, 255), 2));
    QCPGraph* cosine = customPlot->addGraph();
    cosine->setData(t, b);
    cosine->setName("cosine");
    cosine->setPen(QPen(QColor(255, 159, 67), 2));
    customPlot->xAxis->setLabel("time (s)");
    customPlot->yAxis->setLabel("amplitude");
    customPlot->rescaleAxes();
    customPlot->legend->setVisible(true);

    customPlot->setTheme(QCPTheme::dark(customPlot)); // the plot owns the theme
    customPlot->replot();
    // [/styling_dark_theme]
}

DOC_SNIPPET(styling_custom_theme, 600, 360)
{
    // [styling_custom_theme]
    QVector<double> x(200), y(200);
    for (int i = 0; i < 200; ++i)
    {
        x[i] = i / 20.0;
        y[i] = std::sin(x[i]) + 0.5 * std::sin(3 * x[i]);
    }
    QCPGraph* graph = customPlot->addGraph();
    graph->setData(x, y);
    graph->setName("signal");
    graph->setPen(QPen(QColor("#268bd2"), 2));
    customPlot->rescaleAxes();
    customPlot->legend->setVisible(true);

    auto* theme = new QCPTheme(customPlot);
    theme->setBackground(QColor("#fdf6e3"));
    theme->setForeground(QColor("#586e75"));
    theme->setGrid(QColor("#e4dcc4"));
    theme->setSubGrid(QColor("#eee8d5"));
    theme->setSelection(QColor("#d33682"));
    theme->setLegendBackground(QColor("#eee8d5"));
    theme->setLegendBorder(QColor("#93a1a1"));
    customPlot->setTheme(theme);
    customPlot->replot();
    // [/styling_custom_theme]
}

DOC_SNIPPET(styling_pens_brushes, 600, 360)
{
    // [styling_pens_brushes]
    QVector<double> x(200), mid(200), upper(200), lower(200), limit(200, 1.2);
    for (int i = 0; i < 200; ++i)
    {
        x[i] = i / 20.0;
        mid[i] = std::sin(x[i]) + 0.1 * x[i];
        upper[i] = mid[i] + 0.4;
        lower[i] = mid[i] - 0.4;
    }
    QCPGraph* low = customPlot->addGraph();
    low->setData(x, lower);
    low->setPen(Qt::NoPen);
    QCPGraph* high = customPlot->addGraph();
    high->setData(x, upper);
    high->setPen(Qt::NoPen);
    high->setBrush(QColor(31, 119, 180, 60));
    high->setChannelFillGraph(low); // fill between high and low
    QCPGraph* centre = customPlot->addGraph();
    centre->setData(x, mid);
    centre->setPen(QPen(QColor(31, 119, 180), 2));
    QCPGraph* threshold = customPlot->addGraph();
    threshold->setData(x, limit);
    threshold->setPen(QPen(QColor(214, 39, 40), 1.5, Qt::DashLine));
    customPlot->rescaleAxes();
    customPlot->replot();
    // [/styling_pens_brushes]
}

DOC_SNIPPET(styling_scatter_gallery, 600, 360)
{
    // [styling_scatter_gallery]
    using SS = QCPScatterStyle;
    const QList<SS::ScatterShape> shapes { SS::ssDot,          SS::ssCross,        SS::ssPlus,
                                           SS::ssCircle,       SS::ssDisc,         SS::ssSquare,
                                           SS::ssDiamond,      SS::ssStar,         SS::ssTriangle,
                                           SS::ssTriangleInverted, SS::ssCrossSquare, SS::ssPlusSquare,
                                           SS::ssCrossCircle,  SS::ssPlusCircle,   SS::ssPeace };
    const QStringList names { "ssDot",      "ssCross",       "ssPlus",       "ssCircle",
                              "ssDisc",     "ssSquare",      "ssDiamond",    "ssStar",
                              "ssTriangle", "ssTriangleInverted", "ssCrossSquare", "ssPlusSquare",
                              "ssCrossCircle", "ssPlusCircle", "ssPeace" };
    customPlot->xAxis->setRange(0, 5);
    customPlot->yAxis->setRange(0, 3);
    for (QCPAxis* axis : { customPlot->xAxis, customPlot->yAxis })
    {
        axis->setVisible(false);
        axis->grid()->setVisible(false);
    }
    for (int i = 0; i < shapes.size(); ++i)
    {
        const double x = i % 5 + 0.5;
        const double y = 2.6 - i / 5;
        QCPGraph* graph = customPlot->addGraph();
        graph->setData({ x }, { y });
        graph->setLineStyle(QCPGraph::lsNone);
        graph->setScatterStyle(QCPScatterStyle(shapes[i], QPen(QColor(31, 119, 180), 1.5),
                                               QBrush(QColor(31, 119, 180, 70)), 18));
        auto* label = new QCPItemText(customPlot);
        label->position->setCoords(x, y - 0.3);
        label->setText(names[i]);
    }
    customPlot->replot();
    // [/styling_scatter_gallery]
}

DOC_SNIPPET(styling_line_endings, 600, 360)
{
    // [styling_line_endings]
    using LE = QCPLineEnding;
    const QList<LE::EndingStyle> styles { LE::esNone,    LE::esFlatArrow, LE::esSpikeArrow,
                                          LE::esLineArrow, LE::esDisc,    LE::esSquare,
                                          LE::esDiamond, LE::esBar,       LE::esHalfBar,
                                          LE::esSkewedBar };
    const QStringList names { "esNone",    "esFlatArrow", "esSpikeArrow", "esLineArrow",
                              "esDisc",    "esSquare",    "esDiamond",    "esBar",
                              "esHalfBar", "esSkewedBar" };
    customPlot->xAxis->setRange(0, 10);
    customPlot->yAxis->setRange(-0.7, styles.size() - 0.3);
    for (QCPAxis* axis : { customPlot->xAxis, customPlot->yAxis })
    {
        axis->setVisible(false);
        axis->grid()->setVisible(false);
    }
    for (int i = 0; i < styles.size(); ++i)
    {
        const double y = styles.size() - 1 - i;
        auto* line = new QCPItemLine(customPlot);
        line->start->setCoords(1.5, y);
        line->end->setCoords(6, y);
        line->setPen(QPen(QColor(60, 60, 60), 1.5));
        line->setHead(QCPLineEnding(styles[i], 12, 14));
        auto* label = new QCPItemText(customPlot);
        label->position->setCoords(6.5, y);
        label->setPositionAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        label->setText(names[i]);
    }
    customPlot->replot();
    // [/styling_line_endings]
}

DOC_SNIPPET(styling_legend, 600, 360)
{
    // [styling_legend]
    const QList<QColor> colors { QColor(31, 119, 180), QColor(255, 127, 14), QColor(44, 160, 44) };
    for (int k = 0; k < 3; ++k)
    {
        QVector<double> x(200), y(200);
        for (int i = 0; i < 200; ++i)
        {
            x[i] = i / 20.0;
            y[i] = std::sin(x[i] + k * 2.1);
        }
        QCPGraph* graph = customPlot->addGraph();
        graph->setData(x, y);
        graph->setPen(QPen(colors[k], 2));
        graph->setName(QString("phase %1").arg(k));
    }
    customPlot->rescaleAxes();
    customPlot->yAxis->setRange(-1.6, 1.2);

    QCPLegend* legend = customPlot->legend;
    legend->setVisible(true);
    legend->setFillOrder(QCPLayoutGrid::foColumnsFirst); // entries side by side
    legend->setBrush(QColor(255, 255, 255, 220));
    legend->setBorderPen(QPen(QColor(160, 160, 160)));
    legend->setFont(QFont(customPlot->font().family(), 9));
    legend->setIconSize(24, 10);
    customPlot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignBottom | Qt::AlignHCenter);
    customPlot->replot();
    // [/styling_legend]
}

DOC_SNIPPET(styling_background, 600, 360)
{
    // [styling_background]
    QVector<double> x(200), y(200);
    for (int i = 0; i < 200; ++i)
    {
        x[i] = i / 20.0;
        y[i] = std::exp(-x[i] / 4) * std::cos(2 * x[i]);
    }
    QCPGraph* graph = customPlot->addGraph();
    graph->setData(x, y);
    graph->setPen(QPen(QColor(20, 60, 110), 2));
    customPlot->rescaleAxes();

    QLinearGradient gradient(0, 0, 0, 360);
    gradient.setColorAt(0, QColor(250, 252, 255));
    gradient.setColorAt(1, QColor(205, 222, 240));
    customPlot->setBackground(QBrush(QColor(245, 245, 245)));
    customPlot->axisRect()->setBackground(QBrush(gradient));
    customPlot->replot();
    // [/styling_background]
}

DOC_SNIPPET(styling_axes_grid, 600, 360)
{
    // [styling_axes_grid]
    QVector<double> x(200), y(200);
    for (int i = 0; i < 200; ++i)
    {
        x[i] = -5 + i / 20.0;
        y[i] = x[i] * std::exp(-x[i] * x[i] / 4);
    }
    customPlot->addGraph()->setData(x, y);
    customPlot->xAxis->setRange(-5, 5);
    customPlot->yAxis->setRange(-1, 1);
    customPlot->xAxis->setLabel("x");
    customPlot->yAxis->setLabel("x·exp(-x²/4)");

    const QColor ink(70, 70, 70);
    for (QCPAxis* axis : { customPlot->xAxis, customPlot->yAxis })
    {
        axis->setBasePen(QPen(ink, 1.5));
        axis->setTickPen(QPen(ink, 1.5));
        axis->setSubTickPen(QPen(ink, 1));
        axis->setTickLabelColor(ink);
        axis->setLabelColor(ink);
        axis->setLabelFont(QFont(customPlot->font().family(), 10, QFont::Bold));
        axis->setUpperEnding(QCPLineEnding::esSpikeArrow);
        axis->grid()->setPen(QPen(QColor(200, 200, 200), 1, Qt::DashLine));
        axis->grid()->setSubGridVisible(true);
        axis->grid()->setSubGridPen(QPen(QColor(232, 232, 232), 1, Qt::DotLine));
        axis->grid()->setZeroLinePen(QPen(QColor(120, 120, 120), 1));
    }
    customPlot->replot();
    // [/styling_axes_grid]
}
