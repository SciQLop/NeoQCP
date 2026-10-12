#include "snippets.h"

#include <QTimeZone>

#include <algorithm>
#include <cmath>
#include <complex>
#include <memory>
#include <numbers>
#include <random>
#include <vector>

DOC_SNIPPET(gallery_space_weather, 800, 560)
{
    // [gallery_space_weather]
    const double start = 1709251200;    // 2024-03-01 00:00 UTC
    const double shock = start + 2.5 * 3600;
    std::mt19937 rng(11);
    std::normal_distribution<double> noise(0.0, 1.0);

    // Magnetic field at 1 s: quiet solar wind, then a compressed, turbulent sheath.
    const int samples = 6 * 3600;
    std::vector<double> time(samples);
    std::vector<std::vector<float>> b(4, std::vector<float>(samples));
    double wave[3] = { 0, 0, 0 };
    for (int i = 0; i < samples; ++i)
    {
        time[i] = start + i;
        const bool after = time[i] > shock;
        const double gain = after ? 3.2 : 1.0;
        const double base[3] = { 3.0, -4.0, 1.0 };
        double magnitude = 0;
        for (int c = 0; c < 3; ++c)
        {
            wave[c] = 0.995 * wave[c] + (after ? 0.35 : 0.08) * noise(rng);   // red noise
            b[c][i] = static_cast<float>(gain * base[c] + wave[c]);
            magnitude += b[c][i] * b[c][i];
        }
        b[3][i] = static_cast<float>(std::sqrt(magnitude));
    }
    auto* field = new QCPMultiGraph(customPlot->xAxis, customPlot->yAxis);
    field->setData(std::move(time), std::move(b));
    field->setComponentNames({ "Bx", "By", "Bz", "|B|" });
    field->setComponentPens({ QPen(QColor(77, 166, 255), 1), QPen(QColor(102, 204, 102), 1),
                              QPen(QColor(255, 107, 107), 1), QPen(QColor(230, 230, 230), 1.5) });

    // Ion energy spectrogram at 1 min: a cold 1 keV beam, then a hot, broad sheath.
    auto* spectraRect = new QCPAxisRect(customPlot);
    customPlot->plotLayout()->addElement(1, 0, spectraRect);
    QCPAxis* energyAxis = spectraRect->axis(QCPAxis::atLeft);
    energyAxis->setScaleType(QCPAxis::stLogarithmic);
    energyAxis->setTicker(QSharedPointer<QCPAxisTickerLog>::create());
    const int channels = 40;
    std::vector<double> minutes, energy(channels), flux;
    for (int j = 0; j < channels; ++j)
        energy[j] = 10 * std::pow(10.0, 3.5 * j / (channels - 1));   // 10 eV to 30 keV
    for (int m = 0; m < 6 * 60; ++m)
    {
        const double t = start + 60 * m;
        const bool after = t > shock;
        minutes.push_back(t);
        for (double e : energy)
        {
            const double x = std::log10(e) - (after ? 3.3 : 3.0);
            const double width = after ? 0.3 : 0.07;
            flux.push_back((after ? 4e6 : 2e7) * std::exp(-x * x / (2 * width * width))
                           * std::exp(0.15 * noise(rng)) + 3e3 / e);
        }
    }
    auto* scale = new QCPColorScale(customPlot);
    customPlot->plotLayout()->addElement(1, 1, scale);
    scale->setDataScaleType(QCPAxis::stLogarithmic);
    scale->axis()->setTicker(QSharedPointer<QCPAxisTickerLog>::create());
    scale->axis()->setNumberFormat("eb");
    scale->axis()->setNumberPrecision(0);
    scale->setLabel("flux");
    auto* spectra = new QCPColorMap2(spectraRect->axis(QCPAxis::atBottom), energyAxis);
    spectra->setColorScale(scale);
    spectra->setGradient(QCPColorGradient::gpTurbo);
    spectra->setData(std::move(minutes), std::move(energy), std::move(flux));
    spectra->rescaleDataRange(true);
    spectra->removeFromLegend();

    // One time axis for both panels: dates below, linked ranges, aligned margins.
    QCPAxis* fieldTime = customPlot->xAxis;
    QCPAxis* spectraTime = spectraRect->axis(QCPAxis::atBottom);
    auto dates = QSharedPointer<QCPAxisTickerDateTime>::create();
    dates->setDateTimeFormat("HH:mm");
    dates->setTimeZone(QTimeZone(QTimeZone::UTC));
    spectraTime->setTicker(dates);
    fieldTime->setTicker(dates);
    fieldTime->setTickLabels(false);
    QObject::connect(fieldTime, qOverload<const QCPRange&>(&QCPAxis::rangeChanged), spectraTime,
                     qOverload<const QCPRange&>(&QCPAxis::setRange));
    QObject::connect(spectraTime, qOverload<const QCPRange&>(&QCPAxis::rangeChanged), fieldTime,
                     qOverload<const QCPRange&>(&QCPAxis::setRange));
    auto* margins = new QCPMarginGroup(customPlot);
    customPlot->axisRect()->setMarginGroup(QCP::msLeft, margins);
    spectraRect->setMarginGroup(QCP::msLeft, margins);

    // The shock, marked on both panels.
    for (QCPAxisRect* rect : { customPlot->axisRect(), spectraRect })
    {
        auto* span = new QCPItemVSpan(customPlot);
        span->setClipAxisRect(rect);
        for (QCPItemPosition* edge : { span->lowerEdge, span->upperEdge })
            edge->setAxes(rect->axis(QCPAxis::atBottom), rect->axis(QCPAxis::atLeft));
        span->setRange(QCPRange(shock - 300, shock + 300));
        span->setPen(Qt::NoPen);
        span->setBrush(QColor(255, 200, 80, 60));
        span->setBorderPen(QPen(QColor(255, 200, 80), 1));
        span->setMovable(false);
    }

    customPlot->yAxis->setLabel("B [nT]");
    energyAxis->setLabel("ions [eV]");
    spectraTime->setLabel("2024-03-01 [UTC]");
    customPlot->legend->setVisible(true);
    qobject_cast<QCPGroupLegendItem*>(customPlot->legend->item(0))->setExpanded(true);
    customPlot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop | Qt::AlignLeft);
    customPlot->setTheme(QCPTheme::dark(customPlot));
    customPlot->rescaleAxes();
    customPlot->yAxis->setRange(-25, 40);
    customPlot->replot();
    // [/gallery_space_weather]
}

DOC_SNIPPET(gallery_ten_million_points, 800, 520)
{
    // [gallery_ten_million_points]
    const int n = 10'000'000;   // 10 000 s at 1 kHz
    std::vector<double> t(n);
    std::vector<float> v(n);
    std::minstd_rand rng(3);
    for (int i = 0; i < n; ++i)
    {
        t[i] = i * 1e-3;
        const double chirp = std::sin(2 * std::numbers::pi * t[i] * (0.5 + t[i] / 4000));
        const double envelope = 1 + 0.5 * std::sin(2 * std::numbers::pi * t[i] / 2500);
        const double jitter = rng() / double(rng.max()) - 0.5;
        const double burst = (i / 1000) % 1777 == 0 ? 2.5 : 0.0;   // rare one-second bursts
        v[i] = static_cast<float>(envelope * chirp + 0.3 * jitter + burst);
    }
    // One copy of the data, two views of it.
    auto source = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<float>>>(
        std::move(t), std::move(v));

    auto* overview = new QCPGraph2(customPlot->xAxis, customPlot->yAxis);
    overview->setDataSource(source);
    overview->setPen(QPen(QColor(31, 119, 180), 1));
    customPlot->xAxis->setRange(0, 10000);
    customPlot->yAxis->setRange(-2, 4.5);

    auto* detailRect = new QCPAxisRect(customPlot);
    customPlot->plotLayout()->addElement(1, 0, detailRect);
    auto* detail = new QCPGraph2(detailRect->axis(QCPAxis::atBottom), detailRect->axis(QCPAxis::atLeft));
    detail->setDataSource(source);
    detail->setPen(QPen(QColor(214, 39, 40), 1.5));
    const QCPRange window(7108, 7112);   // four seconds around a burst
    detailRect->axis(QCPAxis::atBottom)->setRange(window);
    detailRect->axis(QCPAxis::atLeft)->setRange(-2, 4.5);
    detailRect->axis(QCPAxis::atBottom)->setLabel("time [s]");

    auto* marker = new QCPItemRect(customPlot);   // where the detail view looks
    marker->topLeft->setCoords(window.lower - 40, 4.3);
    marker->bottomRight->setCoords(window.upper + 40, -1.8);
    marker->setPen(QPen(QColor(214, 39, 40), 1.5));

    auto* note = new QCPItemText(customPlot);
    note->position->setType(QCPItemPosition::ptAxisRectRatio);
    note->position->setCoords(0.01, 0.03);
    note->setPositionAlignment(Qt::AlignLeft | Qt::AlignTop);
    note->setText("10 000 000 samples, plotted from the caller's vectors without a copy");

    auto* margins = new QCPMarginGroup(customPlot);
    customPlot->axisRect()->setMarginGroup(QCP::msLeft | QCP::msRight, margins);
    detailRect->setMarginGroup(QCP::msLeft | QCP::msRight, margins);
    customPlot->replot();
    // [/gallery_ten_million_points]
}

DOC_SNIPPET(gallery_orbit, 640, 560)
{
    // [gallery_orbit]
    // A Keplerian orbit, 1.5 x 12 Earth radii, sampled every minute: the dots bunch
    // up where the spacecraft is slow.
    const double earthRadius = 6371, mu = 398600;   // km, km^3/s^2
    const double perigee = 1.5, apogee = 12;          // Earth radii
    const double a = (perigee + apogee) / 2, e = (apogee - perigee) / (apogee + perigee);
    const double period = 2 * std::numbers::pi * std::sqrt(std::pow(a * earthRadius, 3) / mu);
    QVector<double> index, x, y, speed;
    for (double t = 0; t <= period; t += 60)
    {
        double anomaly = 2 * std::numbers::pi * t / period;   // solve Kepler's equation
        for (int k = 0; k < 20; ++k)
            anomaly -= (anomaly - e * std::sin(anomaly) - 2 * std::numbers::pi * t / period)
                       / (1 - e * std::cos(anomaly));
        index.append(index.size());
        x.append(a * (std::cos(anomaly) - e));
        y.append(a * std::sqrt(1 - e * e) * std::sin(anomaly));
        const double r = std::hypot(x.last(), y.last()) * earthRadius;
        speed.append(std::sqrt(mu * (2 / r - 1 / (a * earthRadius))));
    }

    QCPColorGradient gradient(QCPColorGradient::gpPlasma);
    const QCPRange speedRange(0, 10);
    for (int i = 0; i + 1 < x.size(); ++i)   // one short segment per minute, coloured by speed
    {
        auto* segment = new QCPCurve(customPlot->xAxis, customPlot->yAxis);
        segment->setData({ index[i], index[i + 1] }, { x[i], x[i + 1] }, { y[i], y[i + 1] });
        segment->setPen(QPen(QColor::fromRgba(gradient.color(speed[i], speedRange)), 4));
    }
    QVector<double> hourIndex, hourX, hourY;   // a dot every hour
    for (int i = 0; i < x.size(); i += 60)
    {
        hourIndex.append(i);
        hourX.append(x[i]);
        hourY.append(y[i]);
    }
    auto* hours = new QCPCurve(customPlot->xAxis, customPlot->yAxis);
    hours->setData(hourIndex, hourX, hourY);
    hours->setLineStyle(QCPCurve::lsNone);
    hours->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, Qt::black, 6));

    auto* earth = new QCPItemEllipse(customPlot);
    earth->topLeft->setCoords(-1, 1);
    earth->bottomRight->setCoords(1, -1);
    earth->setPen(QPen(QColor(31, 119, 180)));
    earth->setBrush(QColor(31, 119, 180));

    auto* scale = new QCPColorScale(customPlot);   // a scale with no colormap: set it by hand
    customPlot->plotLayout()->addElement(0, 1, scale);
    scale->setGradient(gradient);
    scale->setDataRange(speedRange);
    scale->setLabel("speed [km/s]");

    customPlot->xAxis->setLabel("x [Earth radii]");
    customPlot->yAxis->setLabel("y [Earth radii]");
    customPlot->xAxis->setRange(-13, 3);
    customPlot->yAxis->setRange(-6, 6);
    customPlot->replot();   // lays out the axis rect, so the ratio below uses its real size
    customPlot->yAxis->setScaleRatio(customPlot->xAxis, 1.0);
    customPlot->replot();
    // [/gallery_orbit]
}

DOC_SNIPPET(gallery_joint_density, 640, 640)
{
    // [gallery_joint_density]
    // Two populations, 400 000 points: the 2D histogram and both marginals.
    std::mt19937 rng(8);
    std::normal_distribution<double> normal(0.0, 1.0);
    std::vector<double> x, y;
    for (int i = 0; i < 400'000; ++i)
    {
        const double u = normal(rng), w = normal(rng);
        if (i % 5 < 3)
        {
            x.push_back(u);
            y.push_back(0.7 * u + 0.7 * w);
        }
        else
        {
            x.push_back(2.8 + 0.5 * u);
            y.push_back(2.2 + 0.4 * w);
        }
    }
    // The 2D bins span the data; the marginals use the same span and bin count.
    const int bins = 90;
    auto marginal = [&](const std::vector<double>& values) {
        const auto [low, high] = std::minmax_element(values.begin(), values.end());
        const QCPRange range(*low, *high);
        const double width = range.size() / bins;
        QVector<double> centres(bins), counts(bins, 0);
        for (int k = 0; k < bins; ++k)
            centres[k] = range.lower + (k + 0.5) * width;
        for (double v : values)
            counts[std::min(bins - 1, int((v - range.lower) / width))] += 1;
        return std::tuple { range, centres, counts };
    };
    const auto [xRange, xCentres, xCounts] = marginal(x);
    const auto [yRange, yCentres, yCounts] = marginal(y);

    auto* density = new QCPHistogram2D(customPlot->xAxis, customPlot->yAxis);
    density->setBins(bins, bins);
    density->setGradient(QCPColorGradient::gpInferno);
    density->setData(std::move(x), std::move(y));
    QObject::connect(&density->pipeline(), &QCPHistogramPipeline::finished, density,
                     [density] { density->rescaleDataRange(); });
    customPlot->xAxis->setRange(xRange);
    customPlot->yAxis->setRange(yRange);

    // Marginals above and to the right, sharing the main plot's ranges.
    QCPAxisRect* main = customPlot->axisRect();
    QCPLayoutGrid* layout = customPlot->plotLayout();
    layout->insertRow(0);
    auto* top = new QCPAxisRect(customPlot);
    auto* right = new QCPAxisRect(customPlot);
    layout->addElement(0, 0, top);
    layout->addElement(1, 1, right);
    layout->setRowStretchFactors({ 1, 4 });
    layout->setColumnStretchFactors({ 4, 1 });

    auto* xBars = new QCPBars(top->axis(QCPAxis::atBottom), top->axis(QCPAxis::atLeft));
    xBars->setData(xCentres, xCounts);
    auto* yBars = new QCPBars(right->axis(QCPAxis::atLeft), right->axis(QCPAxis::atBottom));
    yBars->setData(yCentres, yCounts);
    xBars->setWidth(xRange.size() / bins);
    yBars->setWidth(yRange.size() / bins);
    for (QCPBars* bars : { xBars, yBars })
    {
        bars->setPen(Qt::NoPen);
        bars->setBrush(QColor(187, 55, 84));
        bars->rescaleValueAxis();
    }
    top->axis(QCPAxis::atBottom)->setRange(xRange);
    right->axis(QCPAxis::atLeft)->setRange(yRange);
    for (QCPAxis* axis : top->axes() + right->axes())
        axis->setTickLabels(false);   // the main plot carries the numbers
    top->setAutoMargins(QCP::msLeft | QCP::msRight | QCP::msTop);
    top->setMargins(QMargins(0, 0, 0, 6));
    right->setAutoMargins(QCP::msTop | QCP::msBottom | QCP::msRight);
    right->setMargins(QMargins(6, 0, 0, 0));

    auto* columns = new QCPMarginGroup(customPlot);
    main->setMarginGroup(QCP::msLeft | QCP::msRight, columns);
    top->setMarginGroup(QCP::msLeft | QCP::msRight, columns);
    auto* rows = new QCPMarginGroup(customPlot);
    main->setMarginGroup(QCP::msTop | QCP::msBottom, rows);
    right->setMarginGroup(QCP::msTop | QCP::msBottom, rows);

    customPlot->xAxis->setLabel("x");
    customPlot->yAxis->setLabel("y");
    customPlot->replot();
    // [/gallery_joint_density]
}

DOC_SNIPPET(gallery_contours, 640, 500)
{
    // [gallery_contours]
    // The potential of two charges and a dipole, with 16 contour lines on top.
    const int size = 300;
    std::vector<double> x(size), y(size), potential;
    for (int i = 0; i < size; ++i)
        x[i] = y[i] = -3 + 6.0 * i / (size - 1);
    const struct { double x, y, q; } charges[] = {
        { -1.2, 0.8, 1.0 }, { 1.4, -0.6, -1.0 }, { 0.4, 1.6, 0.6 }, { 0.4, 1.3, -0.6 } };
    for (double cx : x)   // z is column after column
        for (double cy : y)
        {
            double v = 0;
            for (const auto& c : charges)
                v += c.q / std::hypot(cx - c.x, cy - c.y, 0.25);   // softened near the charge
            potential.push_back(v);
        }

    auto* map = new QCPColorMap2(customPlot->xAxis, customPlot->yAxis);
    map->setData(std::move(x), std::move(y), std::move(potential));
    map->setAutoContourLevels(16);
    map->setContourPen(QPen(QColor(255, 255, 255, 170), 1));

    auto* scale = new QCPColorScale(customPlot);
    customPlot->plotLayout()->addElement(0, 1, scale);
    scale->setLabel("potential");
    map->setColorScale(scale);   // the map takes the scale's gradient: set it afterwards
    map->setGradient(QCPColorGradient::gpCoolwarm);
    map->setDataRange(QCPRange(-3, 3));   // symmetric, so zero is the neutral colour
    auto* margins = new QCPMarginGroup(customPlot);
    customPlot->axisRect()->setMarginGroup(QCP::msTop | QCP::msBottom, margins);
    scale->setMarginGroup(QCP::msTop | QCP::msBottom, margins);

    customPlot->xAxis->setLabel("x");
    customPlot->yAxis->setLabel("y");
    customPlot->rescaleAxes();
    customPlot->replot();
    // [/gallery_contours]
}

DOC_SNIPPET(gallery_stock_chart, 800, 520)
{
    // [gallery_stock_chart]
    // 120 days of hourly prices: daily candles, a 20-day average, and the volume.
    const double day = 86400, start = 1704067200;   // 2024-01-01 UTC
    std::mt19937 rng(21);
    std::normal_distribution<double> step(0.02, 0.9);
    QVector<double> time, price;
    double p = 100;
    for (int h = 0; h < 120 * 24; ++h)
    {
        p = std::max(5.0, p + step(rng));
        time.append(start + h * 3600);
        price.append(p);
    }
    auto* candles = new QCPFinancial(customPlot->xAxis, customPlot->yAxis);
    candles->data()->set(QCPFinancial::timeSeriesToOhlc(time, price, day, start));
    candles->setChartStyle(QCPFinancial::csCandlestick);
    candles->setWidth(0.7 * day);
    candles->setTwoColored(true);
    candles->setBrushPositive(QColor(38, 166, 154));
    candles->setBrushNegative(QColor(239, 83, 80));
    candles->setPenPositive(QPen(QColor(38, 166, 154)));
    candles->setPenNegative(QPen(QColor(239, 83, 80)));
    candles->setName("price");

    QVector<double> days, average, volumeUp, volumeDown, upDays, downDays;
    std::lognormal_distribution<double> volume(0.0, 0.4);
    double sum = 0;
    int i = 0;
    for (auto it = candles->data()->constBegin(); it != candles->data()->constEnd(); ++it, ++i)
    {
        sum += it->close;
        if (i >= 20)
            sum -= (it - 20)->close;
        if (i >= 19)
        {
            days.append(it->key);
            average.append(sum / 20);
        }
        const double traded = volume(rng) * (1 + std::abs(it->close - it->open) / 3);
        (it->close >= it->open ? upDays : downDays).append(it->key);
        (it->close >= it->open ? volumeUp : volumeDown).append(traded);
    }
    QCPGraph* trend = customPlot->addGraph();
    trend->setData(days, average);
    trend->setPen(QPen(QColor(255, 152, 0), 2));
    trend->setName("20-day average");

    auto* volumeRect = new QCPAxisRect(customPlot);
    customPlot->plotLayout()->addElement(1, 0, volumeRect);
    customPlot->plotLayout()->setRowStretchFactors({ 3, 1 });
    QCPAxis* volumeTime = volumeRect->axis(QCPAxis::atBottom);
    for (auto [keys, values, color] : { std::tuple { upDays, volumeUp, QColor(38, 166, 154, 160) },
                                        std::tuple { downDays, volumeDown, QColor(239, 83, 80, 160) } })
    {
        auto* bars = new QCPBars(volumeTime, volumeRect->axis(QCPAxis::atLeft));
        bars->setData(keys, values);
        bars->setWidth(0.7 * day);
        bars->setPen(Qt::NoPen);
        bars->setBrush(color);
        bars->removeFromLegend();
    }
    volumeRect->axis(QCPAxis::atLeft)->setLabel("volume [M]");

    auto dates = QSharedPointer<QCPAxisTickerDateTime>::create();
    dates->setDateTimeFormat("MMM d");
    dates->setTimeZone(QTimeZone(QTimeZone::UTC));
    customPlot->xAxis->setTicker(dates);
    volumeTime->setTicker(dates);
    customPlot->xAxis->setTickLabels(false);
    QObject::connect(customPlot->xAxis, qOverload<const QCPRange&>(&QCPAxis::rangeChanged), volumeTime,
                     qOverload<const QCPRange&>(&QCPAxis::setRange));
    auto* margins = new QCPMarginGroup(customPlot);
    customPlot->axisRect()->setMarginGroup(QCP::msLeft | QCP::msRight, margins);
    volumeRect->setMarginGroup(QCP::msLeft | QCP::msRight, margins);

    customPlot->yAxis->setLabel("price");
    customPlot->legend->setVisible(true);
    customPlot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop | Qt::AlignLeft);
    customPlot->rescaleAxes();
    customPlot->xAxis->scaleRange(1.03);
    customPlot->replot();
    // [/gallery_stock_chart]
}

DOC_SNIPPET(gallery_antenna_pattern, 600, 600)
{
    // [gallery_antenna_pattern]
    // 16 antennas half a wavelength apart, steered 20 degrees off broadside.
    // A Hann taper trades a wider main lobe for much lower side lobes.
    customPlot->plotLayout()->clear();
    auto* angular = new QCPPolarAxisAngular(customPlot);
    customPlot->plotLayout()->addElement(0, 0, angular);
    QCPPolarAxisRadial* radial = angular->radialAxis();
    angular->setRange(0, 360);
    radial->setRange(-50, 0);
    radial->setLabel("gain [dB]");

    const int elements = 16;
    const double steer = 20 * std::numbers::pi / 180;
    auto pattern = [&](auto weight) {
        QVector<double> angle, gain;
        for (double d = 0; d <= 360; d += 0.25)
        {
            const double theta = d * std::numbers::pi / 180;
            std::complex<double> sum = 0, peak = 0;
            for (int n = 0; n < elements; ++n)
            {
                const double phase = std::numbers::pi * n * (std::sin(theta) - std::sin(steer));
                sum += weight(n) * std::polar(1.0, phase);
                peak += weight(n);
            }
            angle.append(d);
            gain.append(std::max(-50.0, 20 * std::log10(std::abs(sum) / std::abs(peak))));
        }
        return std::pair { angle, gain };
    };
    const auto [angle, uniform] = pattern([](int) { return 1.0; });
    const auto [_, hann] = pattern([&](int n) {
        return std::pow(std::sin(std::numbers::pi * (n + 0.5) / elements), 2);
    });

    auto* plain = new QCPPolarGraph(angular, radial);
    plain->setData(angle, uniform);
    plain->setPen(QPen(QColor(31, 119, 180), 1.5));
    auto* tapered = new QCPPolarGraph(angular, radial);
    tapered->setData(angle, hann);
    tapered->setPen(QPen(QColor(214, 39, 40), 2));

    customPlot->plotLayout()->insertRow(0);
    customPlot->plotLayout()->addElement(0, 0, new QCPTextElement(
        customPlot, "16-element array: uniform (blue), Hann taper (red)", QFont("sans", 11)));
    customPlot->replot();
    // [/gallery_antenna_pattern]
}

DOC_SNIPPET(gallery_fit_residuals, 680, 560)
{
    // [gallery_fit_residuals]
    // Measurements of a damped oscillator, the model, and the residuals in units of sigma.
    auto model = [](double t) { return 4 * std::exp(-t / 6) * std::cos(1.7 * t + 0.3); };
    std::mt19937 rng(13);
    std::normal_distribution<double> noise(0.0, 1.0);
    QVector<double> t, x, sigma, pull;
    for (int i = 0; i < 40; ++i)
    {
        t.append(0.25 + 0.4 * i);
        sigma.append(0.12 + 0.006 * i);   // the sensor gets noisier
        x.append(model(t.last()) + sigma.last() * noise(rng));
        pull.append((x.last() - model(t.last())) / sigma.last());
    }
    QVector<double> fineT, fineX;
    for (int i = 0; i <= 800; ++i)
    {
        fineT.append(i * 0.02);
        fineX.append(model(fineT.last()));
    }

    QCPGraph* curve = customPlot->addGraph();
    curve->setData(fineT, fineX);
    curve->setPen(QPen(QColor(214, 39, 40), 2));
    curve->setName("model");
    QCPGraph* points = customPlot->addGraph();
    points->setData(t, x);
    points->setLineStyle(QCPGraph::lsNone);
    points->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, QColor(31, 119, 180), 6));
    points->setName("measurements");
    auto* errors = new QCPErrorBars(customPlot->xAxis, customPlot->yAxis);
    errors->setDataPlottable(points);
    errors->setData(sigma);
    errors->setPen(QPen(QColor(31, 119, 180)));
    errors->removeFromLegend();

    auto* residualRect = new QCPAxisRect(customPlot);
    customPlot->plotLayout()->addElement(1, 0, residualRect);
    customPlot->plotLayout()->setRowStretchFactors({ 3, 1.2 });
    QCPAxis* residualT = residualRect->axis(QCPAxis::atBottom);
    QCPAxis* residualAxis = residualRect->axis(QCPAxis::atLeft);
    auto* band = new QCPItemHSpan(customPlot);   // the 1 sigma band
    band->setClipAxisRect(residualRect);
    for (QCPItemPosition* edge : { band->lowerEdge, band->upperEdge })
        edge->setAxes(residualT, residualAxis);
    band->setRange(QCPRange(-1, 1));
    band->setPen(Qt::NoPen);
    band->setBrush(QColor(127, 127, 127, 45));
    band->setBorderPen(Qt::NoPen);
    band->setMovable(false);
    QCPGraph* residuals = customPlot->addGraph(residualT, residualAxis);
    residuals->setData(t, pull);
    residuals->setLineStyle(QCPGraph::lsImpulse);
    residuals->setPen(QPen(QColor(31, 119, 180)));
    residuals->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, QColor(31, 119, 180), 5));
    residuals->removeFromLegend();

    customPlot->xAxis->setTickLabels(false);
    customPlot->yAxis->setLabel(R"($x(t)$ [mm])");
    residualAxis->setLabel(R"($\Delta x / \sigma$)");
    residualT->setLabel(R"($t$ [s])");
    customPlot->xAxis->setRange(0, 16);
    residualT->setRange(0, 16);
    customPlot->yAxis->setRange(-4, 4.5);
    residualAxis->setRange(-3.2, 3.2);
    auto* margins = new QCPMarginGroup(customPlot);
    customPlot->axisRect()->setMarginGroup(QCP::msLeft | QCP::msRight, margins);
    residualRect->setMarginGroup(QCP::msLeft | QCP::msRight, margins);
    customPlot->legend->setVisible(true);
    customPlot->replot();
    // [/gallery_fit_residuals]
}
