#include "snippets.h"

#include <QTimeZone>

#include <cmath>
#include <numbers>
#include <tuple>

DOC_SNIPPET(axes_basics, 600, 360)
{
    // [axes_basics]
    QVector<double> temperature(60), depth(60);
    for (int i = 0; i < 60; ++i)
    {
        depth[i] = 10.0 * i;                                            // 0 to 590 m
        temperature[i] = 4 + 18 / (1 + std::exp((depth[i] - 150) / 40));
    }
    QCPGraph* profile = customPlot->addGraph();
    profile->setData(temperature, depth);
    profile->setPen(QPen(QColor(31, 119, 180), 2));

    customPlot->xAxis->setLabel("temperature [°C]");
    customPlot->xAxis->setRange(0, 25);
    customPlot->yAxis->setLabel("depth [m]");
    customPlot->yAxis->setRange(0, 600);
    customPlot->yAxis->setRangeReversed(true);   // depth grows downward
    customPlot->yAxis->setPadding(15);           // extra blank space left of the label
    customPlot->replot();
    // [/axes_basics]
}

DOC_SNIPPET(axes_log, 600, 360)
{
    // [axes_log]
    QVector<double> frequency(200), power(200);
    for (int i = 0; i < 200; ++i)
    {
        frequency[i] = std::pow(10.0, -1 + 4.0 * i / 199);              // 0.1 Hz to 1 kHz
        power[i] = 1e3 / (1 + std::pow(frequency[i] / 2, 5.0 / 3.0));
    }
    QCPGraph* spectrum = customPlot->addGraph();
    spectrum->setData(frequency, power);
    spectrum->setPen(QPen(QColor(214, 39, 40), 2));

    for (QCPAxis* axis : { customPlot->xAxis, customPlot->yAxis })
    {
        axis->setScaleType(QCPAxis::stLogarithmic);
        axis->setTicker(QSharedPointer<QCPAxisTickerLog>::create());
        axis->setNumberFormat("eb");      // 10^n instead of 1e+03
        axis->setNumberPrecision(0);
        axis->grid()->setSubGridVisible(true);
    }
    customPlot->xAxis->setLabel("frequency [Hz]");
    customPlot->yAxis->setLabel("power [nT²/Hz]");
    customPlot->rescaleAxes();
    customPlot->replot();
    // [/axes_log]
}

DOC_SNIPPET(axes_extra_axis, 600, 360)
{
    // [axes_extra_axis]
    QVector<double> hour(97), temperature(97), pressure(97), humidity(97);
    for (int i = 0; i < 97; ++i)
    {
        hour[i] = i / 4.0;                                              // one day, every 15 min
        const double phase = (hour[i] - 9) / 24 * 2 * std::numbers::pi;
        temperature[i] = 15 + 7 * std::sin(phase);
        pressure[i] = 1013 + 4 * std::cos(hour[i] / 24 * 2 * std::numbers::pi);
        humidity[i] = 60 - 20 * std::sin(phase);
    }

    customPlot->yAxis2->setVisible(true);                                   // right axis
    QCPAxis* humidityAxis = customPlot->axisRect()->addAxis(QCPAxis::atLeft);   // second left axis
    humidityAxis->grid()->setVisible(false);

    const QList<std::tuple<QCPAxis*, QVector<double>, QColor, QString>> series {
        { customPlot->yAxis, temperature, QColor(214, 39, 40), "temperature [°C]" },
        { customPlot->yAxis2, pressure, QColor(31, 119, 180), "pressure [hPa]" },
        { humidityAxis, humidity, QColor(44, 160, 44), "humidity [%]" },
    };
    for (const auto& [axis, values, color, label] : series)
    {
        QCPGraph* graph = customPlot->addGraph(customPlot->xAxis, axis);
        graph->setData(hour, values);
        graph->setPen(QPen(color, 2));
        axis->setLabel(label);
        axis->setLabelColor(color);
        axis->setTickLabelColor(color);
    }
    customPlot->xAxis->setLabel("hour");
    customPlot->rescaleAxes();
    customPlot->replot();
    // [/axes_extra_axis]
}

DOC_SNIPPET(axes_tickers, 600, 640)
{
    // [axes_tickers]
    customPlot->plotLayout()->clear();   // one axis rect per ticker, stacked
    auto addRow = [customPlot](QSharedPointer<QCPAxisTicker> ticker, const QString& name,
                               double lower, double upper) {
        auto* rect = new QCPAxisRect(customPlot, false);    // no default axes
        rect->setMinimumMargins(QMargins(40, 0, 40, 0));
        QCPAxis* axis = rect->addAxis(QCPAxis::atBottom);
        axis->setTicker(ticker);
        axis->setRange(lower, upper);
        axis->setLabel(name);
        customPlot->plotLayout()->addElement(customPlot->plotLayout()->rowCount(), 0, rect);
        return axis;
    };

    addRow(QSharedPointer<QCPAxisTicker>::create(), "QCPAxisTicker (the default)", -0.5, 12.5);

    auto dates = QSharedPointer<QCPAxisTickerDateTime>::create();
    dates->setDateTimeFormat("yyyy-MM-dd");
    dates->setTimeZone(QTimeZone(QTimeZone::UTC));
    addRow(dates, "QCPAxisTickerDateTime", 1709251200, 1709251200 + 10 * 86400);   // 1 to 11 March 2024

    addRow(QSharedPointer<QCPAxisTickerTime>::create(), "QCPAxisTickerTime", 0, 3 * 3600);

    QCPAxis* logAxis = addRow(QSharedPointer<QCPAxisTickerLog>::create(), "QCPAxisTickerLog", 1e-2, 1e4);
    logAxis->setScaleType(QCPAxis::stLogarithmic);
    logAxis->setNumberFormat("eb");
    logAxis->setNumberPrecision(0);

    auto days = QSharedPointer<QCPAxisTickerText>::create();
    days->setTicks({ 1, 2, 3, 4, 5, 6, 7 }, { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" });
    addRow(days, "QCPAxisTickerText", 0.5, 7.5);

    auto fixed = QSharedPointer<QCPAxisTickerFixed>::create();
    fixed->setTickStep(15);
    addRow(fixed, "QCPAxisTickerFixed (step 15)", 0, 90);

    addRow(QSharedPointer<QCPAxisTickerPi>::create(), "QCPAxisTickerPi", 0, 2 * std::numbers::pi);
    customPlot->replot();
    // [/axes_tickers]
}

DOC_SNIPPET(axes_latex, 600, 360)
{
    // [axes_latex]
    QVector<double> f(300), response(300);
    for (int i = 0; i < 300; ++i)
    {
        f[i] = 0.05 * i;                                                // 0 to 15 Hz
        response[i] = 1 / std::sqrt(std::pow(1 - f[i] * f[i] / 25, 2) + std::pow(0.2 * f[i] / 5, 2));
    }
    customPlot->addGraph()->setData(f, response);
    customPlot->graph()->setPen(QPen(QColor(148, 103, 189), 2));

    customPlot->xAxis->setLabel("frequency $f$ [Hz]");
    customPlot->yAxis->setLabel(R"($\frac{\partial B_x}{\partial t}$ [nT$\cdot$s$^{-1}$])");
    customPlot->rescaleAxes();
    customPlot->replot();
    // [/axes_latex]
}

DOC_SNIPPET(axes_long_label, 600, 300)
{
    // [axes_long_label]
    QVector<double> t(400), b(400);
    for (int i = 0; i < 400; ++i)
    {
        t[i] = 0.05 * i;
        b[i] = 12 + 3 * std::sin(t[i]) + std::sin(7.3 * t[i]);
    }
    customPlot->addGraph()->setData(t, b);
    customPlot->yAxis->setLabel(
        "magnetic field magnitude, fluxgate magnetometer, GSE frame, burst mode [nT]");
    customPlot->xAxis->setLabel("time [s]");
    customPlot->rescaleAxes();
    customPlot->replot();
    // [/axes_long_label]
}

DOC_SNIPPET(axes_linked, 600, 400)
{
    // [axes_linked]
    auto* lowerRect = new QCPAxisRect(customPlot);
    customPlot->plotLayout()->addElement(1, 0, lowerRect);
    QCPAxis* upperX = customPlot->xAxis;
    QCPAxis* lowerX = lowerRect->axis(QCPAxis::atBottom);

    // Whichever x axis moves, the other one follows.
    QObject::connect(upperX, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
                     lowerX, QOverload<const QCPRange&>::of(&QCPAxis::setRange));
    QObject::connect(lowerX, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
                     upperX, QOverload<const QCPRange&>::of(&QCPAxis::setRange));

    // Line the two axis rects up on the left and right.
    auto* margins = new QCPMarginGroup(customPlot);
    customPlot->axisRect(0)->setMarginGroup(QCP::msLeft | QCP::msRight, margins);
    lowerRect->setMarginGroup(QCP::msLeft | QCP::msRight, margins);

    QVector<double> t(500), position(500), speed(500);
    for (int i = 0; i < 500; ++i)
    {
        t[i] = 0.02 * i;
        position[i] = std::sin(t[i]) * std::exp(-t[i] / 6);
        speed[i] = (std::cos(t[i]) - std::sin(t[i]) / 6) * std::exp(-t[i] / 6);
    }
    customPlot->addGraph(upperX, customPlot->yAxis)->setData(t, position);
    customPlot->addGraph(lowerX, lowerRect->axis(QCPAxis::atLeft))->setData(t, speed);
    customPlot->yAxis->setLabel("position");
    lowerRect->axis(QCPAxis::atLeft)->setLabel("speed");
    lowerRect->axis(QCPAxis::atLeft)->setRange(-1.1, 1.1);
    customPlot->yAxis->setRange(-1.1, 1.1);
    lowerX->setLabel("time [s]");

    upperX->setRange(0, 10);   // the lower x axis follows
    customPlot->replot();
    // [/axes_linked]
}
