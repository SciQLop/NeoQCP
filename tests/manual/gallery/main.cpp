#include <QApplication>
#include <QMainWindow>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QTimer>
#include <cmath>
#include <numbers>
#include <random>
#include "../../../src/qcp.h"

static QCustomPlot* makePlot(QWidget* parent = nullptr)
{
    auto* plot = new QCustomPlot(parent);
    plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables
                          | QCP::iSelectItems | QCP::iSelectLegend);
    return plot;
}

static QWidget* wrapPlot(QCustomPlot* plot)
{
    auto* w = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(plot);
    return w;
}

// ── Tab 1: Spans ─────────────────────────────────────────────────────────────

static QWidget* createSpansTab()
{
    auto* plot = makePlot();
    plot->xAxis->setRange(0, 100);
    plot->yAxis->setRange(0, 100);

    // some data behind the spans
    auto* graph = plot->addGraph();
    QVector<double> x(200), y(200);
    for (int i = 0; i < 200; ++i) {
        x[i] = i * 0.5;
        y[i] = 50 + 30 * qSin(x[i] * 0.15) + 10 * qCos(x[i] * 0.4);
    }
    graph->setData(x, y);
    graph->setPen(QPen(Qt::darkGray, 1.5));

    auto* vspan = new QCPItemVSpan(plot);
    vspan->setRange(QCPRange(15, 35));
    vspan->setBrush(QBrush(QColor(0, 100, 255, 50)));
    vspan->setBorderPen(QPen(QColor(0, 100, 255), 2));

    auto* hspan = new QCPItemHSpan(plot);
    hspan->setRange(QCPRange(60, 80));
    hspan->setBrush(QBrush(QColor(255, 100, 0, 50)));
    hspan->setBorderPen(QPen(QColor(255, 100, 0), 2));

    auto* rspan = new QCPItemRSpan(plot);
    rspan->setKeyRange(QCPRange(55, 85));
    rspan->setValueRange(QCPRange(15, 45));
    rspan->setBrush(QBrush(QColor(0, 180, 0, 50)));
    rspan->setBorderPen(QPen(QColor(0, 180, 0), 2));

    // delete support for all spans
    for (int i = 0; i < plot->itemCount(); ++i) {
        auto* item = plot->item(i);
        QObject::connect(item, &QCPAbstractItem::deleteRequested, plot, [plot, item]() {
            plot->removeItem(item);
            plot->replot();
        });
    }

    // range label
    auto* label = new QCPItemText(plot);
    label->setPositionAlignment(Qt::AlignTop | Qt::AlignLeft);
    label->position->setType(QCPItemPosition::ptAxisRectRatio);
    label->position->setCoords(0.02, 0.02);
    label->setText("Drag edges/fill. Select + Delete to remove.");
    label->setSelectable(false);

    plot->replot();
    return wrapPlot(plot);
}

// ── Tab 2: Graph + Graph2 ────────────────────────────────────────────────────

static QWidget* createGraphTab()
{
    auto* plot = makePlot();
    const int n = 10000;

    // QCPGraph (legacy)
    auto* g1 = plot->addGraph();
    QVector<double> x1(n), y1(n);
    for (int i = 0; i < n; ++i) {
        x1[i] = i / double(n) * 10 - 5;
        y1[i] = qSin(x1[i] * 3) * qExp(-x1[i] * x1[i] / 8.0);
    }
    g1->setData(x1, y1);
    g1->setPen(QPen(Qt::blue, 1.5));
    g1->setName("QCPGraph");

    // QCPGraph2 (zero-copy SoA)
    auto* g2 = new QCPGraph2(plot->xAxis, plot->yAxis);
    std::vector<double> x2(n), y2(n);
    for (int i = 0; i < n; ++i) {
        x2[i] = i / double(n) * 10 - 5;
        y2[i] = qCos(x2[i] * 5) * 0.5 / (1.0 + x2[i] * x2[i]);
    }
    g2->setData(std::move(x2), std::move(y2));
    g2->setPen(QPen(Qt::red, 1.5));
    g2->setName("QCPGraph2");

    // QCPCurve (parametric)
    auto* curve = new QCPCurve(plot->xAxis, plot->yAxis);
    QVector<double> ct(500), cx(500), cy(500);
    for (int i = 0; i < 500; ++i) {
        ct[i] = i;
        double t = i / 500.0 * 2 * std::numbers::pi;
        cx[i] = 2 * qCos(t) + qCos(3 * t);
        cy[i] = 2 * qSin(t) - qSin(3 * t);
    }
    curve->setData(ct, cx, cy);
    curve->setPen(QPen(QColor(0, 150, 0), 1.5));
    curve->setName("QCPCurve (Lissajous)");
    curve->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssNone));

    plot->legend->setVisible(true);
    plot->rescaleAxes();
    plot->replot();
    return wrapPlot(plot);
}

// ── Tab 3: ColorMap2 ─────────────────────────────────────────────────────────

static QWidget* createColorMapTab()
{
    auto* plot = makePlot();

    const int ny = 50;
    std::vector<double> x, y(ny), z;
    for (int j = 0; j < ny; ++j)
        y[j] = j * 0.5;

    auto addSegment = [&](double x0, double x1, double dx, auto zFunc) {
        for (double xv = x0; xv <= x1; xv += dx) {
            x.push_back(xv);
            for (int j = 0; j < ny; ++j)
                z.push_back(zFunc(xv, y[j]));
        }
    };

    addSegment(0.0, 5.0, 0.05, [](double xv, double yv) {
        return qSin(xv * 1.2) * qCos(yv * 0.3);
    });
    addSegment(15.0, 25.0, 0.1, [](double xv, double yv) {
        return qSin(xv * 0.4) * qSin(yv * 0.2);
    });
    addSegment(40.0, 50.0, 0.2, [](double xv, double yv) {
        return qExp(-(xv - 45.0) * (xv - 45.0) / 20.0) * qSin(yv * 0.5);
    });

    auto* cm = new QCPColorMap2(plot->xAxis, plot->yAxis);
    cm->setData(std::move(x), std::move(y), std::move(z));
    cm->setGapThreshold(3.0);

    auto* scale = new QCPColorScale(plot);
    plot->plotLayout()->addElement(0, 1, scale);
    cm->setColorScale(scale);

    QCPColorGradient gradient(QCPColorGradient::gpJet);
    gradient.setNanHandling(QCPColorGradient::nhTransparent);
    cm->setGradient(gradient);
    cm->rescaleDataRange(true);

    plot->rescaleAxes();
    plot->replot();
    return wrapPlot(plot);
}

// ── Tab 4: ColorMap2 Log-Y + Log-Z + NaN + Gaps ─────────────────────────

static QWidget* createColorMapLogTab()
{
    auto* plot = makePlot();

    // Energy spectrogram: log-spaced Y channels, two data segments with a gap
    const int nChannels = 32;
    std::vector<double> y(nChannels);
    for (int j = 0; j < nChannels; ++j)
        y[j] = std::pow(10.0, 0.5 + j * 3.5 / (nChannels - 1)); // ~3 to ~31623

    std::vector<double> x, z;
    auto addSegment = [&](double t0, double t1, double dt) {
        for (double t = t0; t <= t1; t += dt) {
            x.push_back(t);
            for (int j = 0; j < nChannels; ++j) {
                double freq = y[j];
                double logFreq = std::log10(freq);
                // Two spectral peaks drifting in time, plus noise
                double peak1 = 2.0 + 0.5 * std::sin(t * 0.3);
                double peak2 = 3.5 + 0.3 * std::cos(t * 0.2);
                double v = std::exp(-(logFreq - peak1) * (logFreq - peak1) / 0.15)
                         + 0.6 * std::exp(-(logFreq - peak2) * (logFreq - peak2) / 0.08)
                         + 0.02 * std::sin(logFreq * t);
                // Sprinkle some NaN to simulate missing data
                if (std::sin(t * 7.1 + j * 1.3) > 0.97)
                    v = std::nan("");
                z.push_back(v);
            }
        }
    };

    addSegment(0.0, 30.0, 0.5);   // segment 1
    // gap: 30 to 50
    addSegment(50.0, 100.0, 0.5);  // segment 2

    auto* cm = new QCPColorMap2(plot->xAxis, plot->yAxis);
    cm->setData(std::move(x), std::move(y), std::move(z));
    cm->setGapThreshold(3.0);

    // Log-Y axis
    plot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    plot->yAxis->setTicker(QSharedPointer<QCPAxisTickerLog>::create());
    plot->yAxis->setLabel("Energy (eV)");
    plot->xAxis->setLabel("Time (s)");

    // Color scale with log-Z
    auto* scale = new QCPColorScale(plot);
    plot->plotLayout()->addElement(0, 1, scale);
    scale->setDataScaleType(QCPAxis::stLogarithmic);
    scale->axis()->setTicker(QSharedPointer<QCPAxisTickerLog>::create());
    cm->setColorScale(scale);

    QCPColorGradient gradient(QCPColorGradient::gpJet);
    gradient.setNanHandling(QCPColorGradient::nhTransparent);
    cm->setGradient(gradient);
    cm->rescaleDataRange(true);

    plot->rescaleAxes();
    plot->replot();
    return wrapPlot(plot);
}

// ── Tab 5: ColorMap2 Variable-Y ──────────────────────────────────────────────

static QWidget* createColorMapVariableYTab()
{
    auto* plot = makePlot();

    // Simulate a spectrogram where energy channels drift over time.
    // Each column has its own Y values (yIs2D), showcasing per-column Y support.
    const int nCols = 200;
    const int nChannels = 24;

    std::vector<double> x(nCols), y(nCols * nChannels), z(nCols * nChannels);

    for (int i = 0; i < nCols; ++i)
    {
        double t = i * 0.5;
        x[i] = t;

        // Channel centers drift: base range 1..1e4 with a time-dependent shift
        double shift = 1.0 + 0.3 * std::sin(t * 0.08);
        for (int j = 0; j < nChannels; ++j)
        {
            double logCenter = shift * (0.0 + j * 4.0 / (nChannels - 1));
            y[i * nChannels + j] = std::pow(10.0, logCenter);

            double logFreq = logCenter;
            double peak1 = shift * 1.5;
            double peak2 = shift * 3.0;
            z[i * nChannels + j] =
                std::exp(-(logFreq - peak1) * (logFreq - peak1) / 0.3)
                + 0.5 * std::exp(-(logFreq - peak2) * (logFreq - peak2) / 0.15)
                + 0.02 * std::sin(logFreq * t * 0.1);
        }
    }

    auto* cm = new QCPColorMap2(plot->xAxis, plot->yAxis);
    cm->setData(std::move(x), std::move(y), std::move(z));

    plot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    plot->yAxis->setTicker(QSharedPointer<QCPAxisTickerLog>::create());
    plot->yAxis->setLabel("Energy (eV) — drifting channels");
    plot->xAxis->setLabel("Time (s)");

    auto* scale = new QCPColorScale(plot);
    plot->plotLayout()->addElement(0, 1, scale);
    cm->setColorScale(scale);

    QCPColorGradient gradient(QCPColorGradient::gpJet);
    gradient.setNanHandling(QCPColorGradient::nhTransparent);
    cm->setGradient(gradient);
    cm->rescaleDataRange(true);

    plot->rescaleAxes();
    plot->replot();
    return wrapPlot(plot);
}

// ── Tab 6: MultiGraph ────────────────────────────────────────────────────────

static QWidget* createMultiGraphTab()
{
    auto* plot = makePlot();
    const int nComp = 6, nPts = 50000;

    std::vector<double> keys(nPts);
    std::vector<std::vector<double>> vals(nComp, std::vector<double>(nPts));
    for (int i = 0; i < nPts; ++i) {
        keys[i] = i * 0.002;
        for (int c = 0; c < nComp; ++c)
            vals[c][i] = (1.0 + c * 0.3) * std::sin(keys[i] * (1.0 + c * 0.2) + c * 0.7);
    }

    QList<QColor> colors = {
        QColor(31, 119, 180), QColor(255, 127, 14), QColor(44, 160, 44),
        QColor(214, 39, 40), QColor(148, 103, 189), QColor(140, 86, 75)
    };

    auto* mg = new QCPMultiGraph(plot->xAxis, plot->yAxis);
    mg->setData(std::move(keys), std::move(vals));
    mg->setComponentColors(colors);
    QStringList names;
    for (int c = 0; c < nComp; ++c)
        names << QString("Component %1").arg(c);
    mg->setComponentNames(names);
    mg->setName("MultiGraph");

    plot->legend->setVisible(true);
    plot->rescaleAxes();
    plot->replot();
    return wrapPlot(plot);
}

// ── Tab 5: Waterfall / Seismograph ───────────────────────────────────────────

static QWidget* createWaterfallTab()
{
    auto* plot = makePlot();

    const QVector<double> distances = {12, 25, 42, 63, 89, 114, 152, 208, 310, 420};
    const double sampleRate = 100.0, duration = 60.0;
    const int nSamples = int(duration * sampleRate);

    std::vector<double> keys(nSamples);
    for (int i = 0; i < nSamples; ++i)
        keys[i] = i / sampleRate;

    std::vector<std::vector<double>> vals(distances.size());
    for (int s = 0; s < distances.size(); ++s) {
        vals[s].resize(nSamples);
        double dist = distances[s];
        double pArr = dist / 6.0, sArr = dist / 3.5;
        for (int i = 0; i < nSamples; ++i) {
            double t = keys[i];
            double noise = 0.03 * std::sin(17.1 * t + s * 2.7);
            double pSig = 0, sSig = 0;
            if (double dt = t - pArr; dt > 0)
                pSig = 0.7 * std::exp(-1.2 * dt)
                       * std::sin(2 * std::numbers::pi * 3.0 * dt);
            if (double dt = t - sArr; dt > 0)
                sSig = 1.0 * std::exp(-0.6 * dt)
                       * std::sin(2 * std::numbers::pi * 1.5 * dt);
            vals[s][i] = pSig + sSig + noise;
        }
    }

    auto source = std::make_shared<QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>>(
        std::move(keys), std::move(vals));

    auto* wf = new QCPWaterfallGraph(plot->xAxis, plot->yAxis);
    wf->setDataSource(source);
    wf->setNormalize(true);
    wf->setUniformSpacing(1.5);

    QStringList names;
    for (int s = 0; s < distances.size(); ++s)
        names << QString("STA%1 (%2 km)").arg(s + 1, 2, 10, QChar('0')).arg(distances[s]);
    wf->setComponentNames(names);

    plot->xAxis->setLabel("Time (s)");
    plot->yAxis->setLabel("Station");
    plot->legend->setVisible(true);
    plot->rescaleAxes();
    plot->replot();
    return wrapPlot(plot);
}

// ── Tab 6: Items Showcase ────────────────────────────────────────────────────

static QWidget* createItemsTab()
{
    auto* plot = makePlot();
    plot->xAxis->setRange(0, 10);
    plot->yAxis->setRange(0, 10);

    // text item
    auto* text = new QCPItemText(plot);
    text->position->setCoords(2, 9);
    text->setText("Plain Text");
    text->setFont(QFont("Sans", 12, QFont::Bold));
    text->setColor(Qt::darkBlue);

    // rich text item
    auto* rich = new QCPItemRichText(plot);
    rich->position->setCoords(6, 9);
    rich->setHtml("<b>Rich</b> <i style='color:red'>HTML</i> Text");
    rich->setFont(QFont("Sans", 12));

    // line item
    auto* line = new QCPItemLine(plot);
    line->start->setCoords(1, 7);
    line->end->setCoords(4, 5);
    line->setPen(QPen(Qt::red, 2));
    line->setHead(QCPLineEnding::esSpikeArrow);

    // straight line (infinite)
    auto* sline = new QCPItemStraightLine(plot);
    sline->point1->setCoords(0, 2);
    sline->point2->setCoords(10, 5);
    sline->setPen(QPen(Qt::gray, 1, Qt::DashLine));

    // rect item
    auto* rect = new QCPItemRect(plot);
    rect->topLeft->setCoords(5, 7);
    rect->bottomRight->setCoords(8, 5);
    rect->setPen(QPen(Qt::darkGreen, 2));
    rect->setBrush(QBrush(QColor(0, 200, 0, 40)));

    // ellipse item
    auto* ellipse = new QCPItemEllipse(plot);
    ellipse->topLeft->setCoords(1, 4);
    ellipse->bottomRight->setCoords(4, 2);
    ellipse->setPen(QPen(Qt::darkMagenta, 2));
    ellipse->setBrush(QBrush(QColor(200, 0, 200, 40)));

    // bracket item
    auto* bracket = new QCPItemBracket(plot);
    bracket->left->setCoords(6, 3);
    bracket->right->setCoords(9, 3);
    bracket->setPen(QPen(Qt::darkCyan, 2));
    bracket->setLength(15);

    // curve item (Bezier)
    auto* curveItem = new QCPItemCurve(plot);
    curveItem->start->setCoords(6, 2);
    curveItem->startDir->setCoords(7, 4);
    curveItem->endDir->setCoords(8, 0);
    curveItem->end->setCoords(9, 2);
    curveItem->setPen(QPen(QColor(200, 100, 0), 2));
    curveItem->setTail(QCPLineEnding::esBar);
    curveItem->setHead(QCPLineEnding::esSpikeArrow);

    // tracer on a graph
    auto* graph = plot->addGraph();
    QVector<double> x(100), y(100);
    for (int i = 0; i < 100; ++i) {
        x[i] = i * 0.1;
        y[i] = 1 + qSin(x[i] * 2);
    }
    graph->setData(x, y);
    graph->setPen(QPen(Qt::darkGray, 1));

    auto* tracer = new QCPItemTracer(plot);
    tracer->setGraph(graph);
    tracer->setGraphKey(3.0);
    tracer->setStyle(QCPItemTracer::tsCircle);
    tracer->setPen(QPen(Qt::red, 2));
    tracer->setBrush(Qt::red);
    tracer->setSize(8);

    plot->replot();
    return wrapPlot(plot);
}

// ── Tab 7: Bars + Error Bars ─────────────────────────────────────────────────

static QWidget* createBarsTab()
{
    auto* plot = makePlot();

    auto* bars1 = new QCPBars(plot->xAxis, plot->yAxis);
    bars1->setName("Sales");
    bars1->setPen(QPen(QColor(0, 100, 200)));
    bars1->setBrush(QColor(0, 100, 200, 80));
    bars1->setData({1, 2, 3, 4, 5, 6}, {42, 58, 35, 72, 50, 63});

    auto* bars2 = new QCPBars(plot->xAxis, plot->yAxis);
    bars2->setName("Returns");
    bars2->setPen(QPen(QColor(200, 50, 50)));
    bars2->setBrush(QColor(200, 50, 50, 80));
    bars2->setData({1, 2, 3, 4, 5, 6}, {5, 8, 3, 12, 7, 4});
    bars2->moveAbove(bars1);

    // error bars on bars1
    auto* errBars = new QCPErrorBars(plot->xAxis, plot->yAxis);
    errBars->setDataPlottable(bars1);
    errBars->setData({5, 8, 4, 10, 6, 7});
    errBars->setPen(QPen(Qt::black));

    auto ticker = QSharedPointer<QCPAxisTickerText>::create();
    ticker->addTicks({1, 2, 3, 4, 5, 6}, {"Jan", "Feb", "Mar", "Apr", "May", "Jun"});
    plot->xAxis->setTicker(ticker);
    plot->xAxis->setSubTicks(false);

    plot->legend->setVisible(true);
    plot->yAxis->setRange(0, 100);
    plot->xAxis->setRange(0.5, 6.5);
    plot->replot();
    return wrapPlot(plot);
}

// ── Tab 8: Scatter Styles ────────────────────────────────────────────────────

static QWidget* createScatterTab()
{
    auto* plot = makePlot();

    const QCPScatterStyle::ScatterShape shapes[] = {
        QCPScatterStyle::ssCircle, QCPScatterStyle::ssSquare,
        QCPScatterStyle::ssDiamond, QCPScatterStyle::ssTriangle,
        QCPScatterStyle::ssTriangleInverted, QCPScatterStyle::ssCrossSquare,
        QCPScatterStyle::ssPlusSquare, QCPScatterStyle::ssCross,
        QCPScatterStyle::ssPlus, QCPScatterStyle::ssStar,
    };
    const QStringList shapeNames = {
        "Circle", "Square", "Diamond", "Triangle", "TriangleInv",
        "CrossSquare", "PlusSquare", "Cross", "Plus", "Star"
    };
    const QList<QColor> colors = {
        Qt::blue, Qt::red, Qt::darkGreen, Qt::magenta, Qt::darkCyan,
        QColor(200, 100, 0), Qt::darkBlue, Qt::darkRed, QColor(0, 150, 0), Qt::darkYellow
    };

    for (int s = 0; s < 10; ++s) {
        auto* g = plot->addGraph();
        QVector<double> x(15), y(15);
        for (int i = 0; i < 15; ++i) {
            x[i] = i;
            y[i] = s + qSin(i * 0.5 + s) * 0.3;
        }
        g->setData(x, y);
        g->setPen(QPen(colors[s], 1));
        g->setScatterStyle(QCPScatterStyle(shapes[s], colors[s], colors[s], 8));
        g->setLineStyle(QCPGraph::lsLine);
        g->setName(shapeNames[s]);
    }

    plot->legend->setVisible(true);
    plot->rescaleAxes();
    plot->yAxis->scaleRange(1.2, plot->yAxis->range().center());
    plot->replot();
    return wrapPlot(plot);
}

// ── Tab 9: Dark Theme ────────────────────────────────────────────────────────

static QWidget* createThemeTab()
{
    auto* plot = makePlot();
    plot->setTheme(QCPTheme::dark(plot));

    auto* g1 = plot->addGraph();
    auto* g2 = plot->addGraph();
    QVector<double> x(500), y1(500), y2(500);
    for (int i = 0; i < 500; ++i) {
        x[i] = i * 0.02;
        y1[i] = qSin(x[i] * 3) + 0.1 * qSin(x[i] * 30);
        y2[i] = qCos(x[i] * 2) * qExp(-x[i] * 0.1);
    }
    g1->setData(x, y1);
    g1->setPen(QPen(QColor(100, 180, 255), 1.5));
    g1->setName("Signal A");

    g2->setData(x, y2);
    g2->setPen(QPen(QColor(255, 140, 60), 1.5));
    g2->setName("Signal B");

    // vspan on dark theme
    auto* vspan = new QCPItemVSpan(plot);
    vspan->setRange(QCPRange(3, 5));
    vspan->setBrush(QBrush(QColor(100, 180, 255, 40)));
    vspan->setBorderPen(QPen(QColor(100, 180, 255, 120), 1));

    plot->legend->setVisible(true);
    plot->rescaleAxes();
    plot->replot();
    return wrapPlot(plot);
}

// ── Tab 10: Financial ────────────────────────────────────────────────────────

static QWidget* createFinancialTab()
{
    auto* plot = makePlot();

    auto* candlestick = new QCPFinancial(plot->xAxis, plot->yAxis);
    candlestick->setName("ACME Corp");
    candlestick->setChartStyle(QCPFinancial::csCandlestick);
    candlestick->setBrushPositive(QColor(0, 150, 0, 120));
    candlestick->setBrushNegative(QColor(200, 0, 0, 120));
    candlestick->setPenPositive(QPen(QColor(0, 150, 0)));
    candlestick->setPenNegative(QPen(QColor(200, 0, 0)));

    QVector<double> keys(60), open(60), high(60), low(60), close(60);
    double price = 100;
    for (int i = 0; i < 60; ++i) {
        keys[i] = i;
        open[i] = price;
        double change = (std::sin(i * 0.3) + 0.5 * std::cos(i * 0.7)) * 3;
        double volatility = 2 + std::abs(std::sin(i * 0.2)) * 3;
        close[i] = open[i] + change;
        high[i] = qMax(open[i], close[i]) + volatility * 0.5;
        low[i] = qMin(open[i], close[i]) - volatility * 0.5;
        price = close[i];
    }
    candlestick->setData(keys, open, high, low, close);

    auto ticker = QSharedPointer<QCPAxisTickerText>::create();
    for (int i = 0; i < 60; i += 10)
        ticker->addTick(i, QString("Day %1").arg(i));
    plot->xAxis->setTicker(ticker);

    plot->legend->setVisible(true);
    plot->rescaleAxes();
    plot->yAxis->scaleRange(1.1, plot->yAxis->range().center());
    plot->replot();
    return wrapPlot(plot);
}

// ── Tab 11: Statistical Box ──────────────────────────────────────────────────

static QWidget* createStatBoxTab()
{
    auto* plot = makePlot();

    const QList<QColor> colors = {
        QColor(31, 119, 180), QColor(255, 127, 14), QColor(44, 160, 44),
        QColor(214, 39, 40), QColor(148, 103, 189)
    };

    for (int i = 0; i < 5; ++i) {
        auto* box = new QCPStatisticalBox(plot->xAxis, plot->yAxis);
        double center = 50 + i * 10 + std::sin(i) * 5;
        double spread = 8 + i * 2;
        box->addData(i + 1,
                     center - spread * 1.8,  // min
                     center - spread * 0.5,  // Q1
                     center,                  // median
                     center + spread * 0.6,  // Q3
                     center + spread * 2.0,  // max
                     QVector<double>{center - spread * 2.5, center + spread * 2.8}); // outliers
        box->setPen(QPen(colors[i]));
        box->setBrush(QColor(colors[i].red(), colors[i].green(), colors[i].blue(), 60));
        box->setMedianPen(QPen(colors[i], 2));
        box->setName(QString("Group %1").arg(QChar('A' + i)));
    }

    auto ticker = QSharedPointer<QCPAxisTickerText>::create();
    for (int i = 0; i < 5; ++i)
        ticker->addTick(i + 1, QString("Group %1").arg(QChar('A' + i)));
    plot->xAxis->setTicker(ticker);
    plot->xAxis->setSubTicks(false);

    plot->legend->setVisible(true);
    plot->rescaleAxes();
    plot->yAxis->scaleRange(1.2, plot->yAxis->range().center());
    plot->xAxis->setRange(0, 6);
    plot->replot();
    return wrapPlot(plot);
}

// ── Tab 12: Realtime ColorMap2 ───────────────────────────────────────────────

static QWidget* createRealtimeColorMapTab()
{
    auto* plot = makePlot();

    const int ny = 64;
    auto x = std::make_shared<std::vector<double>>();
    auto y = std::make_shared<std::vector<double>>(ny);
    auto z = std::make_shared<std::vector<double>>();
    for (int j = 0; j < ny; ++j)
        (*y)[j] = j * (20.0 / ny); // frequency axis 0..20 Hz

    auto* cm = new QCPColorMap2(plot->xAxis, plot->yAxis);
    cm->setGapThreshold(0);

    auto* scale = new QCPColorScale(plot);
    plot->plotLayout()->addElement(0, 1, scale);
    cm->setColorScale(scale);
    cm->setDataRange(QCPRange(-1, 1));

    QCPColorGradient gradient(QCPColorGradient::gpPolar);
    gradient.setNanHandling(QCPColorGradient::nhTransparent);
    cm->setGradient(gradient);

    plot->xAxis->setLabel("Time (s)");
    plot->yAxis->setLabel("Frequency (Hz)");
    plot->xAxis->setRange(0, 10);
    plot->yAxis->setRange(0, 20);
    plot->replot();

    // Timer pushes a new column every 50ms (simulated spectrogram)
    auto* timer = new QTimer(plot);
    auto step = std::make_shared<int>(0);
    QObject::connect(timer, &QTimer::timeout, plot,
        [plot, cm, x, y, z, step]() {
            int ny = static_cast<int>(y->size());
            double t = (*step) * 0.05;
            x->push_back(t);
            for (int j = 0; j < ny; ++j) {
                double freq = (*y)[j];
                // Drifting peaks with harmonics
                double peak1 = 5.0 + 3.0 * std::sin(t * 0.3);
                double peak2 = 14.0 + 2.0 * std::cos(t * 0.5);
                double v = std::exp(-(freq - peak1) * (freq - peak1) / 2.0) * std::sin(t * 2.0)
                         + std::exp(-(freq - peak2) * (freq - peak2) / 1.0) * std::cos(t * 1.5)
                         + 0.1 * std::sin(freq * t * 0.4);
                z->push_back(v);
            }
            ++(*step);

            cm->viewData<double, double, double>(
                std::span<const double>{*x}, std::span<const double>{*y}, std::span<const double>{*z});
            cm->dataChanged();

            // Scroll the x axis to follow the data
            if (t > 10.0)
                plot->xAxis->setRange(t - 10.0, t);
        });
    timer->start(50);

    return wrapPlot(plot);
}

// ── Tab 13: Realtime Graph2 ──────────────────────────────────────────────────

static QWidget* createRealtimeGraphTab()
{
    auto* plot = makePlot();

    auto x = std::make_shared<std::vector<double>>();
    auto y1 = std::make_shared<std::vector<double>>();
    auto y2 = std::make_shared<std::vector<double>>();

    auto* g1 = new QCPGraph2(plot->xAxis, plot->yAxis);
    g1->setPen(QPen(QColor(31, 119, 180), 1.5));
    g1->setName("Signal");

    auto* g2 = new QCPGraph2(plot->xAxis, plot->yAxis);
    g2->setPen(QPen(QColor(214, 39, 40), 1.5));
    g2->setName("Envelope");

    plot->xAxis->setLabel("Time (s)");
    plot->yAxis->setLabel("Amplitude");
    plot->xAxis->setRange(0, 10);
    plot->yAxis->setRange(-2, 2);
    plot->legend->setVisible(true);
    plot->replot();

    auto* timer = new QTimer(plot);
    auto step = std::make_shared<int>(0);
    QObject::connect(timer, &QTimer::timeout, plot,
        [plot, g1, g2, x, y1, y2, step]() {
            double t = (*step) * 0.02;
            x->push_back(t);
            double envelope = std::sin(t * 0.4) * (1.0 + 0.5 * std::cos(t * 0.15));
            y1->push_back(envelope * std::sin(t * 5.0) + 0.1 * std::sin(t * 37.0));
            y2->push_back(envelope);
            ++(*step);

            g1->viewData<double, double>(
                std::span<const double>{*x}, std::span<const double>{*y1});
            g1->dataChanged();
            g2->viewData<double, double>(
                std::span<const double>{*x}, std::span<const double>{*y2});
            g2->dataChanged();

            if (t > 10.0)
                plot->xAxis->setRange(t - 10.0, t);
        });
    timer->start(20);

    return wrapPlot(plot);
}

// ── Tab: Histogram2D ────────────────────────────────────────────────────────

static QWidget* createHistogram2DTab()
{
    auto* plot = makePlot();

    const int nPoints = 100000;
    std::vector<double> keys(nPoints), vals(nPoints);
    std::mt19937 rng(42);
    std::normal_distribution<double> dist(0.0, 1.0);
    const double rho = 0.6;
    for (int i = 0; i < nPoints; ++i)
    {
        double u = dist(rng);
        double v = dist(rng);
        keys[i] = u;
        vals[i] = rho * u + std::sqrt(1.0 - rho * rho) * v;
    }

    auto* hist = new QCPHistogram2D(plot->xAxis, plot->yAxis);
    hist->setData(std::move(keys), std::move(vals));
    hist->setBins(80, 80);
    hist->setNormalization(QCPHistogram2D::nColumn);

    auto* scale = new QCPColorScale(plot);
    plot->plotLayout()->addElement(0, 1, scale);
    hist->setColorScale(scale);

    QCPColorGradient grad(QCPColorGradient::gpJet);
    grad.setNanHandling(QCPColorGradient::nhTransparent);
    hist->setGradient(grad);

    QObject::connect(&hist->pipeline(), &QCPHistogramPipeline::finished,
        plot, [hist, plot](uint64_t) {
            hist->rescaleDataRange();
            plot->rescaleAxes();
        }, Qt::SingleShotConnection);

    plot->xAxis->setLabel("X");
    plot->yAxis->setLabel("Y");
    plot->replot();
    return wrapPlot(plot);
}

// ── Tab: Histogram2D (log) ──────────────────────────────────────────────────

static QWidget* createHistogram2DLogTab()
{
    auto* plot = makePlot();

    const int nPoints = 50000;
    std::vector<double> keys(nPoints), vals(nPoints);
    std::mt19937 rng(123);
    std::normal_distribution<double> keyDist(5.0, 2.0);
    std::lognormal_distribution<double> valDist(2.0, 0.8);
    for (int i = 0; i < nPoints; ++i)
    {
        keys[i] = keyDist(rng);
        vals[i] = valDist(rng);
    }

    auto* hist = new QCPHistogram2D(plot->xAxis, plot->yAxis);
    hist->setData(std::move(keys), std::move(vals));
    hist->setBins(60, 60);
    hist->setNormalization(QCPHistogram2D::nColumn);

    plot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    plot->yAxis->setTicker(QSharedPointer<QCPAxisTickerLog>::create());

    auto* scale = new QCPColorScale(plot);
    plot->plotLayout()->addElement(0, 1, scale);
    hist->setColorScale(scale);

    QCPColorGradient gradient(QCPColorGradient::gpHot);
    gradient.setNanHandling(QCPColorGradient::nhTransparent);
    hist->setGradient(gradient);

    QObject::connect(&hist->pipeline(), &QCPHistogramPipeline::finished,
        plot, [hist, plot](uint64_t) {
            hist->rescaleDataRange();
            plot->rescaleAxes();
        }, Qt::SingleShotConnection);

    plot->xAxis->setLabel("Wind Speed (km/s)");
    plot->yAxis->setLabel("He Abundance (log)");
    plot->replot();
    return wrapPlot(plot);
}

// ── Main ─────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle("NeoQCP Gallery");
    window.resize(1000, 700);

    auto* tabs = new QTabWidget;
    tabs->addTab(createSpansTab(),       "Spans");
    tabs->addTab(createGraphTab(),       "Graph / Curve");
    tabs->addTab(createColorMapTab(),    "ColorMap2");
    tabs->addTab(createColorMapLogTab(),       "ColorMap2 Log/NaN/Gap");
    tabs->addTab(createColorMapVariableYTab(), "ColorMap2 Variable-Y");
    tabs->addTab(createMultiGraphTab(),        "MultiGraph");
    tabs->addTab(createWaterfallTab(),   "Waterfall");
    tabs->addTab(createItemsTab(),       "Items");
    tabs->addTab(createBarsTab(),        "Bars + ErrorBars");
    tabs->addTab(createScatterTab(),     "Scatter Styles");
    tabs->addTab(createFinancialTab(),   "Financial");
    tabs->addTab(createStatBoxTab(),     "Statistical Box");
    tabs->addTab(createRealtimeColorMapTab(), "Realtime ColorMap2");
    tabs->addTab(createRealtimeGraphTab(),   "Realtime Graph2");
    tabs->addTab(createThemeTab(),       "Dark Theme");
    tabs->addTab(createHistogram2DTab(),    "Histogram2D");
    tabs->addTab(createHistogram2DLogTab(), "Histogram2D (log)");

    window.setCentralWidget(tabs);
    window.show();

    return app.exec();
}
