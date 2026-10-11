#include "snippets.h"

#include <QTemporaryDir>

#include <cmath>

DOC_SNIPPET(export_png, 600, 360)
{
    // [export_png]
    QVector<double> x(401), y(401);
    for (int i = 0; i < 401; ++i)
    {
        x[i] = (i - 200) / 10.0;
        y[i] = x[i] == 0 ? 1 : std::sin(x[i]) / x[i];
    }
    QCPGraph* graph = customPlot->addGraph();
    graph->setData(x, y);
    graph->setPen(QPen(QColor(31, 119, 180), 2));
    customPlot->xAxis->setLabel("x");
    customPlot->yAxis->setLabel("sin(x) / x");
    customPlot->rescaleAxes();

    QTemporaryDir dir;
    const QString file = dir.filePath("sinc.png");
    if (!customPlot->savePng(file, 600, 360, 2.0)) // 1200 x 720 pixels
        qWarning() << "could not write" << file;
    // [/export_png]
}

DOC_SNIPPET(export_pdf, 600, 360)
{
    // [export_pdf]
    QVector<double> f(300), power(300);
    for (int i = 0; i < 300; ++i)
    {
        f[i] = 0.1 * std::pow(10.0, i / 100.0);
        power[i] = 1 / (1 + std::pow(f[i] / 3, 5.0 / 3.0 * 2));
    }
    customPlot->addGraph()->setData(f, power);
    customPlot->xAxis->setScaleType(QCPAxis::stLogarithmic);
    customPlot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    customPlot->xAxis->setTicker(QSharedPointer<QCPAxisTickerLog>(new QCPAxisTickerLog));
    customPlot->yAxis->setTicker(QSharedPointer<QCPAxisTickerLog>(new QCPAxisTickerLog));
    customPlot->xAxis->setLabel("frequency (Hz)");
    customPlot->yAxis->setLabel("power");
    customPlot->rescaleAxes();

    QTemporaryDir dir;
    const QString file = dir.filePath("spectrum.pdf");
    if (!customPlot->savePdf(file, 600, 360, QCP::epNoCosmetic, "my-app", "Power spectrum"))
        qWarning() << "could not write" << file;
    // [/export_pdf]
}

DOC_SNIPPET(export_formats, 600, 360)
{
    // [export_formats]
    QVector<double> x(100), y(100);
    for (int i = 0; i < 100; ++i)
    {
        x[i] = i;
        y[i] = std::sqrt(i) * std::sin(i / 8.0);
    }
    QCPGraph* graph = customPlot->addGraph();
    graph->setData(x, y);
    graph->setLineStyle(QCPGraph::lsStepCenter);
    graph->setBrush(QColor(31, 119, 180, 50));
    customPlot->rescaleAxes();

    QTemporaryDir dir;
    const bool ok = customPlot->saveJpg(dir.filePath("steps.jpg"), 600, 360, 1.0, 90)
        && customPlot->saveBmp(dir.filePath("steps.bmp"), 600, 360)
        && customPlot->saveRastered(dir.filePath("steps.ppm"), 600, 360, 1.0, "PPM")
        && customPlot->savePng(dir.filePath("steps-300dpi.png"), 600, 360, 1.0, -1, 300);
    if (!ok)
        qWarning() << "an image format is missing; see QImageWriter::supportedImageFormats()";
    // [/export_formats]
}

DOC_SNIPPET(export_pixmap, 600, 360)
{
    // [export_pixmap]
    QVector<double> t(300), v(300);
    for (int i = 0; i < 300; ++i)
    {
        t[i] = i / 30.0;
        v[i] = std::cos(t[i]) * std::exp(-t[i] / 5);
    }
    customPlot->addGraph()->setData(t, v);
    customPlot->rescaleAxes();

    const QPixmap thumbnail = customPlot->toPixmap(300, 180);    // 300 x 180 pixels
    QPixmap sharp = customPlot->toPixmap(300, 180, 2.0);         // same layout, 600 x 360 pixels
    sharp.setDevicePixelRatio(2.0);                              // shows at 300 x 180 on HiDPI
    QTemporaryDir dir;
    if (!thumbnail.save(dir.filePath("thumbnail.png")))
        qWarning() << "could not write the thumbnail";
    // [/export_pixmap]
}

DOC_SNIPPET(export_to_painter, 600, 360)
{
    // [export_to_painter]
    QVector<double> t(2000), v(2000);
    for (int i = 0; i < 2000; ++i)
    {
        t[i] = i / 100.0;
        v[i] = std::sin(t[i]) + 0.2 * std::sin(25 * t[i]);
    }
    customPlot->addGraph()->setData(t, v);
    customPlot->rescaleAxes();

    QImage figure(1200, 360, QImage::Format_ARGB32_Premultiplied);
    figure.fill(Qt::white);
    QCPPainter painter(&figure);
    customPlot->toPainter(&painter, 600, 360); // left panel: everything
    painter.translate(600, 0);
    customPlot->xAxis->setRange(4, 6);
    customPlot->toPainter(&painter, 600, 360); // right panel: a zoom
    painter.end();

    QTemporaryDir dir;
    if (!figure.save(dir.filePath("overview-and-zoom.png")))
        qWarning() << "could not write the figure";
    // [/export_to_painter]
}
