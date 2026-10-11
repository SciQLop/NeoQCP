#include "snippets.h"

#include <cmath>
#include <numbers>
#include <random>
#include <vector>

DOC_SNIPPET(plottables_curve, 480, 420)
{
    // [plottables_curve]
    const int n = 600;
    QVector<double> t(n), x(n), y(n);
    for (int i = 0; i < n; ++i)
    {
        t[i] = 2 * std::numbers::pi * i / (n - 1);
        x[i] = std::sin(3 * t[i]);
        y[i] = std::sin(4 * t[i]);
    }
    auto* lissajous = new QCPCurve(customPlot->xAxis, customPlot->yAxis);
    lissajous->setData(t, x, y);   // drawn in the order of t, not of x
    lissajous->setPen(QPen(QColor(148, 103, 189), 2));
    customPlot->xAxis->setRange(-1.2, 1.2);
    customPlot->yAxis->setRange(-1.2, 1.2);
    customPlot->replot();
    // [/plottables_curve]
}

DOC_SNIPPET(plottables_bars_grouped, 600, 360)
{
    // [plottables_bars_grouped]
    const QVector<double> keys { 1, 2, 3, 4, 5 };
    const QList<QVector<double>> values { { 12, 15, 9, 14, 11 },
                                          { 8, 11, 13, 10, 7 },
                                          { 5, 6, 8, 12, 9 } };
    const QStringList names { "2023", "2024", "2025" };
    const QList<QColor> colors { QColor(31, 119, 180), QColor(255, 127, 14), QColor(44, 160, 44) };
    auto* group = new QCPBarsGroup(customPlot);
    for (int s = 0; s < values.size(); ++s)
    {
        auto* bars = new QCPBars(customPlot->xAxis, customPlot->yAxis);
        bars->setData(keys, values[s]);
        bars->setWidth(0.25);   // in key units: three bars fill 0.75 of each slot
        bars->setBarsGroup(group);
        bars->setPen(Qt::NoPen);
        bars->setBrush(colors[s]);
        bars->setName(names[s]);
    }
    auto ticker = QSharedPointer<QCPAxisTickerText>::create();
    ticker->setTicks(keys, { "Mon", "Tue", "Wed", "Thu", "Fri" });
    customPlot->xAxis->setTicker(ticker);
    customPlot->xAxis->setRange(0.4, 5.6);
    customPlot->yAxis->setRange(0, 17);
    customPlot->legend->setVisible(true);
    customPlot->replot();
    // [/plottables_bars_grouped]
}

DOC_SNIPPET(plottables_bars_stacked, 600, 360)
{
    // [plottables_bars_stacked]
    const QVector<double> years { 2019, 2020, 2021, 2022, 2023, 2024 };
    auto* fossil = new QCPBars(customPlot->xAxis, customPlot->yAxis);
    fossil->setData(years, { 62, 58, 57, 54, 50, 46 });
    fossil->setBrush(QColor(127, 127, 127));
    fossil->setName("fossil");
    auto* nuclear = new QCPBars(customPlot->xAxis, customPlot->yAxis);
    nuclear->setData(years, { 18, 19, 18, 17, 18, 18 });
    nuclear->setBrush(QColor(255, 127, 14));
    nuclear->setName("nuclear");
    nuclear->moveAbove(fossil);
    auto* renewable = new QCPBars(customPlot->xAxis, customPlot->yAxis);
    renewable->setData(years, { 20, 23, 25, 29, 32, 36 });
    renewable->setBrush(QColor(44, 160, 44));
    renewable->setName("renewable");
    renewable->moveAbove(nuclear);
    for (QCPBars* bars : { fossil, nuclear, renewable })
        bars->setPen(Qt::NoPen);
    customPlot->xAxis->setRange(2018.4, 2024.6);
    customPlot->yAxis->setRange(0, 135);   // headroom for the legend
    customPlot->yAxis->setLabel("share (%)");
    customPlot->legend->setVisible(true);
    customPlot->replot();
    // [/plottables_bars_stacked]
}

DOC_SNIPPET(plottables_financial, 600, 360)
{
    // [plottables_financial]
    std::mt19937 rng(7);
    std::normal_distribution<double> step(0.0, 0.6);
    QVector<double> time(30 * 24), price(30 * 24);
    double p = 100;
    for (int i = 0; i < time.size(); ++i)
    {
        time[i] = i / 24.0;   // hourly samples over 30 days
        p += step(rng);
        price[i] = p;
    }
    auto* candles = new QCPFinancial(customPlot->xAxis, customPlot->yAxis);
    candles->data()->set(QCPFinancial::timeSeriesToOhlc(time, price, 1.0));   // one bin per day
    candles->setChartStyle(QCPFinancial::csCandlestick);
    candles->setWidth(0.6);
    customPlot->xAxis->setLabel("day");
    customPlot->rescaleAxes();
    customPlot->xAxis->scaleRange(1.05);
    customPlot->replot();
    // [/plottables_financial]
}

DOC_SNIPPET(plottables_error_bars, 600, 360)
{
    // [plottables_error_bars]
    std::mt19937 rng(3);
    std::normal_distribution<double> noise(0.0, 1.0);
    QVector<double> x(12), y(12), error(12);
    for (int i = 0; i < 12; ++i)
    {
        x[i] = i + 1;
        error[i] = 0.3 + 0.08 * i;
        y[i] = 0.5 * x[i] + noise(rng) * error[i];
    }
    QCPGraph* points = customPlot->addGraph();
    points->setData(x, y);
    points->setLineStyle(QCPGraph::lsNone);
    points->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, QColor(31, 119, 180), 7));
    auto* errors = new QCPErrorBars(customPlot->xAxis, customPlot->yAxis);
    errors->removeFromLegend();
    errors->setDataPlottable(points);
    errors->setData(error);   // error[i] belongs to point i, the same above and below
    errors->setPen(QPen(QColor(31, 119, 180)));
    customPlot->rescaleAxes();   // includes the error bars
    customPlot->xAxis->setRange(0, 13);
    customPlot->replot();
    // [/plottables_error_bars]
}

DOC_SNIPPET(plottables_statistical_box, 600, 360)
{
    // [plottables_statistical_box]
    auto* boxes = new QCPStatisticalBox(customPlot->xAxis, customPlot->yAxis);
    boxes->setBrush(QColor(31, 119, 180, 70));
    //             key  min  Q1   median  Q3   max  outliers
    boxes->addData(1, 1.1, 1.9, 2.25, 2.7, 3.4);
    boxes->addData(2, 0.8, 1.5, 1.9, 2.3, 3.0, { 4.1, 4.4 });
    boxes->addData(3, 1.6, 2.5, 3.1, 3.6, 4.6, { 0.6 });
    auto ticker = QSharedPointer<QCPAxisTickerText>::create();
    ticker->setTicks({ 1, 2, 3 }, { "sensor A", "sensor B", "sensor C" });
    customPlot->xAxis->setTicker(ticker);
    customPlot->xAxis->setRange(0.4, 3.6);
    customPlot->yAxis->setRange(0, 5);
    customPlot->yAxis->setLabel("response time (ms)");
    customPlot->replot();
    // [/plottables_statistical_box]
}

DOC_SNIPPET(plottables_waterfall, 600, 420)
{
    // [plottables_waterfall]
    const int n = 400, traces = 10;
    std::vector<double> frequency(n);
    std::vector<std::vector<double>> spectra(traces, std::vector<double>(n));
    for (int i = 0; i < n; ++i)
    {
        frequency[i] = i * 0.25;
        for (int k = 0; k < traces; ++k)
        {
            const double peak = 25 + 4 * k;   // the main line drifts from trace to trace
            spectra[k][i] = std::exp(-std::pow((frequency[i] - peak) / 3, 2))
                            + 0.3 * std::exp(-std::pow((frequency[i] - 75) / 6, 2));
        }
    }
    auto* waterfall = new QCPWaterfallGraph(customPlot->xAxis, customPlot->yAxis);
    waterfall->setData(std::move(frequency), std::move(spectra));
    waterfall->setUniformSpacing(1.0);   // trace k sits at value k
    waterfall->setGain(0.8);             // each normalised trace spans 0.8 of a step
    QList<QColor> colors;
    for (int k = 0; k < traces; ++k)
        colors << QColor::fromHsvF(0.6 - 0.6 * k / (traces - 1), 0.8, 0.75);
    waterfall->setComponentColors(colors);
    customPlot->xAxis->setLabel("frequency (Hz)");
    customPlot->yAxis->setLabel("trace");
    customPlot->rescaleAxes();
    customPlot->replot();
    // [/plottables_waterfall]
}

DOC_SNIPPET(plottables_intervals, 600, 240)
{
    // [plottables_intervals]
    auto* lanes = new QCPLaneLayout(customPlot);
    lanes->setLaneHeight(24);
    const QVector<int> lane = lanes->laneIndices({ "Probe A", "Probe B", "Ground" });

    qcp::intervals::Columns c;
    c.start = { 1, 5.5, 12, 2, 8, 9, 15, 3, 11 };
    c.stop = { 4.5, 9, 16, 6, 10.5, 13, 18, 7, 17 };
    c.lane = { lane[0], lane[0], lane[0], lane[1], lane[1], lane[1], lane[1], lane[2], lane[2] };
    c.category = { 0, 1, 0, 1, 0, 2, 0, 2, 1 };
    c.ids = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    c.labels = { "", "orbit 12", "", "", "", "cal", "", "pass 1", "pass 2" };

    auto* timeline = new QCPIntervals(customPlot->xAxis, customPlot->yAxis, lanes);
    timeline->setCategoryNames({ "burst", "survey", "calibration" });
    timeline->setStackMode(QCPIntervals::skTime);   // overlaps go to a second row
    timeline->setData(std::move(c));

    customPlot->yAxis->setVisible(false);
    customPlot->xAxis->setRange(-4, 20);
    customPlot->xAxis->setLabel("time (h)");
    customPlot->legend->setVisible(true);
    customPlot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignBottom | Qt::AlignRight);
    customPlot->replot();
    // [/plottables_intervals]
}
