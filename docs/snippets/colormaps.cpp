#include "snippets.h"

#include <QMetaEnum>

#include <cmath>
#include <random>
#include <vector>

DOC_SNIPPET(colormaps_classic, 600, 360)
{
    // [colormaps_classic]
    auto* map = new QCPColorMap(customPlot->xAxis, customPlot->yAxis);
    map->data()->setSize(200, 150);                             // 200 x 150 cells
    map->data()->setRange(QCPRange(-4, 4), QCPRange(-3, 3));    // centres of the corner cells
    for (int xIndex = 0; xIndex < 200; ++xIndex)
        for (int yIndex = 0; yIndex < 150; ++yIndex)
        {
            double x, y;
            map->data()->cellToCoord(xIndex, yIndex, &x, &y);
            map->data()->setCell(xIndex, yIndex,
                                 std::sin(2 * x) * std::cos(2 * y) * std::exp(-(x * x + y * y) / 8));
        }

    auto* scale = new QCPColorScale(customPlot);
    customPlot->plotLayout()->addElement(0, 1, scale);   // right of the axis rect
    scale->setLabel("amplitude");
    map->setColorScale(scale);
    map->setGradient(QCPColorGradient::gpViridis);
    map->rescaleDataRange();

    // Keep the colour bar exactly as tall as the axis rect.
    auto* margins = new QCPMarginGroup(customPlot);
    customPlot->axisRect()->setMarginGroup(QCP::msTop | QCP::msBottom, margins);
    scale->setMarginGroup(QCP::msTop | QCP::msBottom, margins);

    customPlot->rescaleAxes();
    customPlot->replot();
    // [/colormaps_classic]
}

DOC_SNIPPET(colormaps_colormap2, 600, 360)
{
    // [colormaps_colormap2]
    const int channels = 80;
    std::vector<double> time, frequency(channels), power;
    for (int j = 0; j < channels; ++j)
        frequency[j] = 0.5 * j;                                  // 0 to 39.5 Hz
    auto addColumns = [&](int first, int last) {                 // one column every 0.5 s
        for (int i = first; i < last; ++i)
        {
            const double t = 0.5 * i;
            time.push_back(t);
            for (double f : frequency)                           // z is column after column
                power.push_back(std::exp(-std::pow(f - 20 - 10 * std::sin(t / 8), 2) / 8)
                                + 0.2 * std::exp(-f / 10));
        }
    };
    addColumns(0, 80);      // 0 to 39.5 s
    addColumns(120, 200);   // 60 to 99.5 s: nothing was recorded in between

    auto* map = new QCPColorMap2(customPlot->xAxis, customPlot->yAxis);
    map->setData(std::move(time), std::move(frequency), std::move(power));
    map->setGradient(QCPColorGradient::gpTurbo);
    map->rescaleDataRange(true);

    customPlot->xAxis->setLabel("time [s]");
    customPlot->yAxis->setLabel("frequency [Hz]");
    customPlot->rescaleAxes();
    customPlot->replot();
    // [/colormaps_colormap2]
}

DOC_SNIPPET(colormaps_log, 600, 360)
{
    // [colormaps_log]
    const int columns = 300, channels = 40;
    std::vector<double> time(columns), energy(channels), flux;
    for (int j = 0; j < channels; ++j)
        energy[j] = 10 * std::pow(10.0, 3.0 * j / (channels - 1));   // 10 eV to 10 keV
    for (int i = 0; i < columns; ++i)
    {
        time[i] = i;
        const double peak = 2.5 + 0.7 * std::sin(i / 30.0);         // log10 of the peak energy
        for (double e : energy)
            flux.push_back(1e6 * std::exp(-std::pow(std::log10(e) - peak, 2) / 0.1) + 1e2);
    }

    customPlot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    customPlot->yAxis->setTicker(QSharedPointer<QCPAxisTickerLog>::create());

    auto* scale = new QCPColorScale(customPlot);
    customPlot->plotLayout()->addElement(0, 1, scale);
    scale->setDataScaleType(QCPAxis::stLogarithmic);
    scale->axis()->setTicker(QSharedPointer<QCPAxisTickerLog>::create());
    scale->axis()->setNumberFormat("eb");   // 10^n labels
    scale->axis()->setNumberPrecision(0);
    scale->setLabel("flux");

    auto* map = new QCPColorMap2(customPlot->xAxis, customPlot->yAxis);
    map->setColorScale(scale);   // the map takes the scale's log z
    map->setGradient(QCPColorGradient::gpInferno);
    map->setData(std::move(time), std::move(energy), std::move(flux));
    map->rescaleDataRange(true);

    customPlot->xAxis->setLabel("time [s]");
    customPlot->yAxis->setLabel("energy [eV]");
    customPlot->rescaleAxes();
    customPlot->replot();
    // [/colormaps_log]
}

DOC_SNIPPET(colormaps_histogram2d, 600, 360)
{
    // [colormaps_histogram2d]
    std::mt19937 rng(42);
    std::normal_distribution<double> normal(0.0, 1.0);
    std::vector<double> x(200000), y(200000);
    for (std::size_t i = 0; i < x.size(); ++i)
    {
        x[i] = normal(rng);
        y[i] = 0.6 * x[i] + 0.8 * normal(rng);   // correlated with x
    }

    auto* histogram = new QCPHistogram2D(customPlot->xAxis, customPlot->yAxis);
    histogram->setBins(120, 80);
    histogram->setGradient(QCPColorGradient::gpInferno);
    histogram->setData(std::move(x), std::move(y));
    // Counts exist only once the background binning is done.
    QObject::connect(&histogram->pipeline(), &QCPHistogramPipeline::finished, histogram,
                     [histogram] { histogram->rescaleDataRange(); });

    customPlot->xAxis->setLabel("x");
    customPlot->yAxis->setLabel("y");
    customPlot->rescaleAxes();
    customPlot->replot();
    // [/colormaps_histogram2d]
}

DOC_SNIPPET(colormaps_gradients, 600, 560)
{
    // [colormaps_gradients]
    const QMetaEnum presets = QCPColorGradient::staticMetaObject.enumerator(
        QCPColorGradient::staticMetaObject.indexOfEnumerator("GradientPreset"));
    auto names = QSharedPointer<QCPAxisTickerText>::create();
    for (int i = 0; i < presets.keyCount(); ++i)
    {
        auto* stripe = new QCPColorMap(customPlot->xAxis, customPlot->yAxis);
        stripe->data()->setSize(256, 2);
        stripe->data()->setRange(QCPRange(0, 1), QCPRange(i - 0.2, i + 0.2));
        for (int k = 0; k < 256; ++k)
        {
            stripe->data()->setCell(k, 0, k);
            stripe->data()->setCell(k, 1, k);
        }
        stripe->setGradient(QCPColorGradient::GradientPreset(presets.value(i)));
        stripe->setDataRange(QCPRange(0, 255));
        names->addTick(i, presets.key(i));
    }
    customPlot->yAxis->setTicker(names);
    customPlot->yAxis->setRange(-0.6, presets.keyCount() - 0.4);
    customPlot->yAxis->setRangeReversed(true);   // first preset on top
    customPlot->yAxis->setTickLength(0, 0);   // setTicks(false) would hide the names too
    customPlot->yAxis->setSubTicks(false);
    customPlot->yAxis->grid()->setVisible(false);
    customPlot->xAxis->setVisible(false);
    customPlot->xAxis->grid()->setVisible(false);
    customPlot->xAxis->setRange(-0.01, 1.01);
    customPlot->replot();
    // [/colormaps_gradients]
}

DOC_SNIPPET(colormaps_interpolation, 600, 300)
{
    // [colormaps_interpolation]
    auto* rightRect = new QCPAxisRect(customPlot);
    customPlot->plotLayout()->addElement(0, 1, rightRect);
    const QList<QCPAxisRect*> rects { customPlot->axisRect(0), rightRect };
    for (int r = 0; r < 2; ++r)
    {
        QCPAxis* xAxis = rects[r]->axis(QCPAxis::atBottom);
        auto* map = new QCPColorMap(xAxis, rects[r]->axis(QCPAxis::atLeft));
        map->data()->setSize(8, 8);
        map->data()->setRange(QCPRange(0, 7), QCPRange(0, 7));
        for (int x = 0; x < 8; ++x)
            for (int y = 0; y < 8; ++y)
                map->data()->setCell(x, y, std::sin(0.8 * x) + std::cos(0.6 * y));
        map->setGradient(QCPColorGradient::gpViridis);
        map->setInterpolate(r == 1);
        map->rescaleDataRange();
        map->rescaleAxes();
        xAxis->setLabel(r == 1 ? "setInterpolate(true)" : "setInterpolate(false)");
    }
    customPlot->replot();
    // [/colormaps_interpolation]
}

DOC_SNIPPET(colormaps_nan, 600, 360)
{
    // [colormaps_nan]
    auto* map = new QCPColorMap(customPlot->xAxis, customPlot->yAxis);
    map->data()->setSize(120, 90);
    map->data()->setRange(QCPRange(0, 12), QCPRange(0, 9));
    for (int xIndex = 0; xIndex < 120; ++xIndex)
        for (int yIndex = 0; yIndex < 90; ++yIndex)
        {
            double x, y;
            map->data()->cellToCoord(xIndex, yIndex, &x, &y);
            const bool missing = std::hypot(x - 7, y - 4.5) < 2;   // a hole in the data
            map->data()->setCell(xIndex, yIndex,
                                 missing ? std::nan("") : std::sin(x / 2) * std::cos(y / 3));
        }

    QCPColorGradient gradient(QCPColorGradient::gpCoolwarm);
    gradient.setNanHandling(QCPColorGradient::nhNanColor);
    gradient.setNanColor(QColor(160, 160, 160));
    map->setGradient(gradient);
    map->rescaleDataRange(true);
    customPlot->rescaleAxes();
    customPlot->replot();
    // [/colormaps_nan]
}

DOC_CODE(colormaps_preset)
{
    auto* map = new QCPColorMap(customPlot->xAxis, customPlot->yAxis);
    // [colormaps_preset]
    map->setGradient(QCPColorGradient::gpViridis);
    // [/colormaps_preset]
}

DOC_CODE(colormaps_custom_gradient)
{
    auto* map = new QCPColorMap(customPlot->xAxis, customPlot->yAxis);
    // [colormaps_custom_gradient]
    QCPColorGradient gradient;
    gradient.setColorStopAt(0.0, QColor(10, 10, 60));
    gradient.setColorStopAt(0.5, QColor(40, 160, 160));
    gradient.setColorStopAt(1.0, QColor(250, 250, 200));
    map->setGradient(gradient);
    // [/colormaps_custom_gradient]
}
