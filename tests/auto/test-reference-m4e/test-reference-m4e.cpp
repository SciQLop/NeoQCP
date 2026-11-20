#include "test-reference-m4e.h"

#include <QImage>
#include <QPixmap>
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <functional>
#include <numbers>
#include <numeric>
#include <random>
#include <set>
#include <utility>
#include <vector>

#include "../test-user-system/skew-system.h"
#include "qcustomplot.h"
#include "reference-image.h"

namespace
{
const QString kDir = QStringLiteral("test-reference-m4e");

QImage render(const std::function<void(QCustomPlot&)>& setup,
              const QSize& size = QSize(400, 300))
{
    QCustomPlot plot;
    plot.resize(size);
    QFont font = qcp::test::referenceFont();
    font.setPixelSize(10);
    plot.setFont(font);
    for (QCPAxis* axis : plot.axisRect()->axes())
    {
        axis->setTickLabelFont(font);
        axis->setLabelFont(font);
    }
    setup(plot);
    plot.replot();
    return plot.toPixmap(size.width(), size.height())
        .toImage()
        .convertToFormat(QImage::Format_ARGB32);
}

void check(const QImage& image, const char* name)
{
    QString why;
    QVERIFY2(qcp::test::matchesReference(image, kDir, QLatin1String(name), &why),
             qPrintable(QStringLiteral("%1: %2").arg(QLatin1String(name), why)));
}

//! The inset-zoom target program's sample: matplotlib's axes_grid/bivariate_normal.npy, pasted at
//! row 30, column 30 of a 150 x 150 zero image.
double zoomSampleImage(int column, int row)
{
    const int i = column - 30, j = row - 30;
    if (i < 0 || i >= 15 || j < 0 || j >= 15)
        return 0;
    const double x = -3 + 0.5 * i, y = -4 + 0.5 * j;
    const double u = x, v = y, u1 = (x - 1) / 1.5, v1 = (y - 1) / 0.5;
    const double first = std::exp(-0.5 * (u * u + v * v)) / (2 * std::numbers::pi);
    const double second = std::exp(-0.5 * (u1 * u1 + v1 * v1)) / (2 * std::numbers::pi * 1.5 * 0.5);
    return 10 * (first - second);
}

QVector<double> sampleKeys()
{
    return QVector<double> { 0.5, 1.5, 2.5, 3.5, 4.5, 5.5 };
}

QImage errorBarsScene(bool verticalKey, bool logValue, bool selected, bool legend,
                      QCPErrorBars::ErrorType type = QCPErrorBars::etValueError)
{
    return render(
        [=](QCustomPlot& plot)
        {
            QCPAxis* key = verticalKey ? plot.yAxis : plot.xAxis;
            QCPAxis* value = verticalKey ? plot.xAxis : plot.yAxis;
            key->setRange(0, 6);
            value->setRange(logValue ? 0.5 : -1, logValue ? 10 : 1.5);
            auto* graph = key->axisRect()->add<QCPGraph>(key, value).get();
            graph->setLineStyle(QCPGraph::lsNone);
            graph->setPen(QPen(Qt::blue, 0));
            graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 6));
            graph->setData(sampleKeys(),
                           logValue ? QVector<double> { 0.7, 1.2, 2.4, 1.6, 3.4, 5.5 }
                                    : QVector<double> { 0.2, -0.4, 0.6, 0.1, -0.3, 0.5 });
            auto* bars = key->axisRect()->add<QCPErrorBars>(key, value).get();
            bars->setDataPlottable(graph);
            bars->setErrorType(type);
            bars->setData(QVector<double> { 0.2, 0.4, 0.3, 0.5, 0.2, 0.4 });
            bars->setWhiskerWidth(6);
            bars->setPen(QPen(QColor(200, 30, 30)));
            if (selected)
            {
                bars->setSelectable(QCP::stDataRange);
                bars->setSelection(QCPDataSelection(QCPDataRange(2, 4)));
            }
            if (legend)
            {
                plot.legend->setVisible(true);
                bars->addToLegend();
            }
            if (logValue)
                value->setScale(QCPScale::log());
        });
}

QImage financialScene(bool ohlc, bool verticalKey, bool logValue, bool selected, bool legend,
                      bool twoColored = true)
{
    return render(
        [=](QCustomPlot& plot)
        {
            QCPAxis* key = verticalKey ? plot.yAxis : plot.xAxis;
            QCPAxis* value = verticalKey ? plot.xAxis : plot.yAxis;
            key->setRange(0, 7);
            value->setRange(logValue ? 1 : 0, 10);
            auto* financial = key->axisRect()->add<QCPFinancial>(key, value).get();
            const QVector<double> keys { 0.6, 1.4, 2.2, 3.0, 3.8, 4.6, 5.4, 6.2 };
            QVector<double> open, high, low, close;
            for (int i = 0; i < keys.size(); ++i)
            {
                const double base = 3.0 + qSin(0.9 * i) * 1.5 + i * 0.4;
                open << base;
                close << base + (i % 3 == 0 ? 0.8 : -0.6);
                high << qMax(open.last(), close.last()) + 0.7;
                low << qMin(open.last(), close.last()) - 0.6;
            }
            financial->setData(keys, open, high, low, close);
            financial->setChartStyle(ohlc ? QCPFinancial::csOhlc : QCPFinancial::csCandlestick);
            financial->setWidth(0.7);
            if (!twoColored)
            {
                financial->setTwoColored(false);
                financial->setPen(QPen(QColor(40, 60, 160)));
                financial->setBrush(QColor(120, 160, 230));
            }
            if (selected)
            {
                financial->setSelectable(QCP::stDataRange);
                financial->setSelection(QCPDataSelection(QCPDataRange(2, 5)));
            }
            if (legend)
            {
                plot.legend->setVisible(true);
                financial->addToLegend();
            }
            if (logValue)
                value->setScale(QCPScale::log());
        });
}

QImage statisticalBoxScene(bool verticalKey, bool selected, bool legend, bool logValue = false)
{
    return render(
        [=](QCustomPlot& plot)
        {
            QCPAxis* key = verticalKey ? plot.yAxis : plot.xAxis;
            QCPAxis* value = verticalKey ? plot.xAxis : plot.yAxis;
            key->setRange(0, 4);
            value->setRange(logValue ? 0.1 : 0, 10);
            auto* box = key->axisRect()->add<QCPStatisticalBox>(key, value).get();
            box->addData(0.8, 1.0, 2.5, 4.0, 6.0, 8.5);
            box->addData(1.8, 0.5, 3.0, 5.2, 7.0, 9.5);
            box->addData(2.8, 2.0, 3.5, 5.5, 7.5, 9.0);
            box->setWidth(0.5);
            box->setWhiskerWidth(0.3);
            if (selected)
            {
                box->setSelectable(QCP::stDataRange);
                box->setSelection(QCPDataSelection(QCPDataRange(1, 2)));
            }
            if (legend)
            {
                plot.legend->setVisible(true);
                box->addToLegend();
            }
            if (logValue)
                value->setScale(QCPScale::log());
        });
}

qcp::intervals::Columns intervalColumns(QCPLaneLayout& layout)
{
    qcp::intervals::Columns columns;
    const int laneA = layout.laneIndex(QStringLiteral("A"));
    const int laneB = layout.laneIndex(QStringLiteral("B"));
    columns.start = { 0.2, 1.0, 2.1, 3.0, 4.2, 5.0 };
    columns.stop = { 0.8, 1.7, 2.6, 3.9, 4.9, 5.8 };
    columns.lane = { laneA, laneB, laneA, laneB, laneA, laneB };
    columns.category.assign(columns.start.size(), 0);
    columns.ids.resize(columns.start.size());
    std::iota(columns.ids.begin(), columns.ids.end(), qint64(101));
    return columns;
}

QImage intervalsScene(bool lanes, bool selected, bool legend)
{
    QCPLaneLayout layout;
    if (lanes)
        layout.setPlacement(QCPLaneLayout::plLanes);
    return render(
        [&](QCustomPlot& plot)
        {
            plot.xAxis->setRange(0, 6);
            plot.yAxis->setRange(-0.5, 2.5);
            auto* intervals = plot.add<QCPIntervals>(&layout).get();
            intervals->setData(intervalColumns(layout));
            intervals->setCategoryNames(QStringList { QStringLiteral("A") });
            intervals->setCategoryColors(QVector<QColor> { QColor(70, 130, 200) });
            if (selected)
                intervals->setSelectedRows(QVector<int> { 1, 2 });
            if (legend)
            {
                plot.legend->setVisible(true);
                intervals->addToLegend();
            }
        });
}

//! The intervals' own legend icon: a plain plottable row, not the category list it adds itself.
QImage intervalsIconScene()
{
    QCPLaneLayout layout;
    return render(
        [&](QCustomPlot& plot)
        {
            plot.xAxis->setRange(0, 6);
            plot.yAxis->setRange(-0.5, 2.5);
            auto* intervals = plot.add<QCPIntervals>(&layout).get();
            intervals->setData(intervalColumns(layout));
            intervals->setCategoryColors(QVector<QColor> { QColor(70, 130, 200) });
            intervals->setName(QStringLiteral("intervals"));
            plot.legend->setVisible(true);
            plot.legend->clearItems();
            plot.legend->addItem(new QCPPlottableLegendItem(plot.legend, intervals));
        });
}

QPixmap testPixmap()
{
    QImage image(48, 36, QImage::Format_ARGB32);
    for (int y = 0; y < 36; ++y)
        for (int x = 0; x < 48; ++x)
            image.setPixel(x, y,
                           x < 24 ? (y < 18 ? qRgb(40, 120, 200) : qRgb(230, 200, 40))
                                  : (y < 18 ? qRgb(220, 60, 60) : qRgb(60, 170, 90)));
    return QPixmap::fromImage(image);
}

QImage itemScene(const char* kind)
{
    const QString name = QLatin1String(kind);
    return render(
        [=](QCustomPlot& plot)
        {
            plot.xAxis->setRange(0, 6);
            plot.yAxis->setRange(0, 1);
            if (name == QLatin1String("line-arrows"))
            {
                auto line = plot.add<QCPItemLine>();
                line->start->setCoords(0.5, 0.2);
                line->end->setCoords(5.5, 0.8);
                line->setHead(QCPLineEnding::esSpikeArrow);
                line->setTail(QCPLineEnding::esDisc);
                line->setPen(QPen(QColor(30, 80, 160), 2));
            }
            else if (name == QLatin1String("straightline"))
            {
                auto line = plot.add<QCPItemStraightLine>();
                line->point1->setCoords(1.0, 0.1);
                line->point2->setCoords(4.0, 0.9);
                line->setPen(QPen(QColor(160, 40, 120), 2));
            }
            else if (name == QLatin1String("curve"))
            {
                auto curve = plot.add<QCPItemCurve>();
                curve->start->setCoords(0.5, 0.2);
                curve->startDir->setCoords(2.0, 0.9);
                curve->endDir->setCoords(4.0, 0.1);
                curve->end->setCoords(5.5, 0.8);
                curve->setPen(QPen(QColor(20, 140, 90), 2));
            }
            else if (name == QLatin1String("bracket"))
            {
                auto bracket = plot.add<QCPItemBracket>();
                bracket->left->setCoords(1.0, 0.75);
                bracket->right->setCoords(5.0, 0.25);
                bracket->setLength(30);
                bracket->setPen(QPen(Qt::black, 2));
            }
            else if (name == QLatin1String("rect"))
            {
                auto rect = plot.add<QCPItemRect>();
                rect->topLeft->setCoords(1.0, 0.8);
                rect->bottomRight->setCoords(4.5, 0.2);
                rect->setPen(QPen(QColor(200, 120, 20), 2));
                rect->setBrush(QColor(200, 120, 20, 60));
            }
            else if (name == QLatin1String("ellipse"))
            {
                auto ellipse = plot.add<QCPItemEllipse>();
                ellipse->topLeft->setCoords(1.5, 0.85);
                ellipse->bottomRight->setCoords(4.5, 0.15);
                ellipse->setPen(QPen(QColor(120, 60, 180), 2));
                ellipse->setBrush(QColor(120, 60, 180, 60));
            }
            else if (name == QLatin1String("pixmap") || name == QLatin1String("pixmap-scaled"))
            {
                auto pixmap = plot.add<QCPItemPixmap>();
                pixmap->setPixmap(testPixmap());
                pixmap->topLeft->setCoords(1.0, 0.8);
                pixmap->bottomRight->setCoords(4.5, 0.2);
                pixmap->setScaled(name.endsWith(QLatin1String("scaled")), Qt::KeepAspectRatio,
                                  Qt::SmoothTransformation);
            }
            else if (name == QLatin1String("tracer"))
            {
                auto* graph = plot.add<QCPGraph>().get();
                QVector<double> keys, values;
                for (int i = 0; i <= 60; ++i)
                {
                    keys << i * 0.1;
                    values << 0.5 + 0.35 * qSin(i * 0.1);
                }
                graph->setPen(QPen(Qt::blue, 0));
                graph->setData(keys, values);
                auto tracer = plot.add<QCPItemTracer>();
                tracer->setGraph(graph);
                tracer->setGraphKey(3.2);
                tracer->setStyle(QCPItemTracer::tsCircle);
                tracer->setSize(9);
                tracer->setPen(QPen(Qt::red, 2));
            }
        });
}

QImage spanScene(const char* kind)
{
    const QString name = QLatin1String(kind);
    return render(
        [=](QCustomPlot& plot)
        {
            plot.xAxis->setRange(0, 6);
            plot.yAxis->setRange(0, 1);
            if (name == QLatin1String("h"))
            {
                auto span = plot.add<QCPItemSpan>(Qt::Horizontal);
                span->setRange(QCPRange(0.3, 0.6));
                span->setBrush(QColor(40, 90, 200, 50));
                span->setBorderPen(QPen(QColor(40, 90, 200), 2));
            }
            else if (name == QLatin1String("v"))
            {
                auto span = plot.add<QCPItemSpan>(Qt::Vertical);
                span->setRange(QCPRange(2.0, 4.0));
                span->setBrush(QColor(200, 60, 40, 50));
                span->setBorderPen(QPen(QColor(200, 60, 40), 2));
            }
            else if (name == QLatin1String("r"))
            {
                auto span = plot.add<QCPItemRSpan>();
                span->leftEdge->setCoords(1.5, 0);
                span->rightEdge->setCoords(4.5, 1);
                span->topEdge->setCoords(0, 0.8);
                span->bottomEdge->setCoords(6, 0.3);
                span->setBrush(QColor(30, 150, 90, 50));
                span->setBorderPen(QPen(QColor(30, 150, 90), 2));
            }
            else if (name == QLatin1String("selected"))
            {
                auto span = plot.add<QCPItemSpan>(Qt::Vertical);
                span->setRange(QCPRange(1.5, 3.5));
                span->setBrush(QColor(200, 60, 40, 50));
                span->setBorderPen(QPen(QColor(200, 60, 40), 2));
                span->setSelected(true);
            }
        });
}

QCPPolarArea* polarArea(QCustomPlot& plot)
{
    plot.plotLayout()->clear();
    auto* area = new QCPPolarArea(&plot);
    plot.plotLayout()->addElement(0, 0, area);
    return area;
}

QImage polarScene(const char* kind)
{
    const QString name = QLatin1String(kind);
    return render(
        [=](QCustomPlot& plot)
        {
            if (name == QLatin1String("polar-basic"))
            {
                QCPPolarArea* area = polarArea(plot);
                QCPPolarAxisAngular* angular = area->angularAxis();
                QCPPolarAxisRadial* radial = area->radialAxis();
                angular->setRange(0, 360);
                radial->setRange(0, 1.05);
                const QList<std::pair<double, QColor>> patterns {
                    { 1.0, QColor(127, 127, 127) },
                    { 0.5, QColor(31, 119, 180) },
                    { 0.0, QColor(214, 39, 40) },
                };
                for (const auto& [a, color] : patterns)
                {
                    QVector<double> angle(361), gain(361);
                    for (int i = 0; i <= 360; ++i)
                    {
                        angle[i] = i;
                        gain[i] = std::abs(a + (1 - a) * std::cos(i * std::numbers::pi / 180));
                    }
                    auto* graph = area->add<QCPPolarGraph>().get();
                    graph->setData(angle, gain);
                    graph->setPen(QPen(color, 2));
                }
            }
            else if (name == QLatin1String("polar-radians"))
            {
                QCPPolarArea* area = polarArea(plot);
                QCPPolarAxisAngular* angular = area->angularAxis();
                QCPPolarAxisRadial* radial = area->radialAxis();
                angular->setAngle(0);
                angular->setRangeReversed(true);
                angular->setRange(0, 2 * std::numbers::pi);
                auto ticker = QSharedPointer<QCPAxisTickerPi>::create();
                ticker->setTickCount(8);
                angular->setTicker(ticker);
                area->grid()->setSubGridType(QCPPolarGrid::gtAll);
                radial->setRange(0, 1);
                QVector<double> theta(721), r(721);
                for (int i = 0; i <= 720; ++i)
                {
                    theta[i] = i * std::numbers::pi / 360;
                    r[i] = std::abs(std::cos(3 * theta[i]));
                }
                auto* rose = area->add<QCPPolarGraph>().get();
                rose->setData(theta, r);
                rose->setPen(QPen(QColor(148, 103, 189), 2));
                rose->setBrush(QColor(148, 103, 189, 60));
            }
            else if (name == QLatin1String("polar-gallery"))
            {
                QCPPolarArea* area = polarArea(plot);
                QCPPolarAxisAngular* angular = area->angularAxis();
                QCPPolarAxisRadial* radial = area->radialAxis();
                angular->setRange(0, 360);
                radial->setRange(-50, 0);
                radial->setLabel(QStringLiteral("gain [dB]"));
                const int elements = 16;
                const double steer = 20 * std::numbers::pi / 180;
                auto pattern = [&](auto weight)
                {
                    QVector<double> angle, gain;
                    for (double d = 0; d <= 360; d += 0.25)
                    {
                        const double theta = d * std::numbers::pi / 180;
                        std::complex<double> sum = 0, peak = 0;
                        for (int n = 0; n < elements; ++n)
                        {
                            const double phase
                                = std::numbers::pi * n * (std::sin(theta) - std::sin(steer));
                            sum += weight(n) * std::polar(1.0, phase);
                            peak += weight(n);
                        }
                        angle.append(d);
                        gain.append(
                            std::max(-50.0, 20 * std::log10(std::abs(sum) / std::abs(peak))));
                    }
                    return std::pair { angle, gain };
                };
                const auto [angle, uniform] = pattern([](int) { return 1.0; });
                const auto [_, hann] = pattern(
                    [&](int n)
                    { return std::pow(std::sin(std::numbers::pi * (n + 0.5) / elements), 2); });
                auto* plain = area->add<QCPPolarGraph>().get();
                plain->setData(angle, uniform);
                plain->setPen(QPen(QColor(31, 119, 180), 1.5));
                auto* tapered = area->add<QCPPolarGraph>().get();
                tapered->setData(angle, hann);
                tapered->setPen(QPen(QColor(214, 39, 40), 2));
                plot.plotLayout()->insertRow(0);
                plot.plotLayout()->addElement(
                    0, 0,
                    new QCPTextElement(
                        &plot, QStringLiteral("16-element array: uniform (blue), Hann taper (red)"),
                        qcp::test::referenceFont(11)));
            }
            else if (name == QLatin1String("polar-log"))
            {
                QCPPolarArea* area = polarArea(plot);
                QCPPolarAxisAngular* angular = area->angularAxis();
                QCPPolarAxisRadial* radial = area->radialAxis();
                angular->setRange(0, 360);
                radial->setRange(1, 1000);
                radial->setScale(QCPScale::log());
                auto ticker = QSharedPointer<QCPAxisTickerLog>::create();
                ticker->setLogBase(10);
                radial->setTicker(ticker);
                QVector<double> angle(361), r(361);
                for (int i = 0; i <= 360; ++i)
                {
                    angle[i] = i;
                    r[i] = std::pow(10.0, 1.5 + 1.4 * std::sin(i * std::numbers::pi / 90));
                }
                auto* graph = area->add<QCPPolarGraph>().get();
                graph->setData(angle, r);
                graph->setPen(QPen(QColor(20, 120, 140), 2));
            }
            else if (name == QLatin1String("polar-rotated"))
            {
                QCPPolarArea* area = polarArea(plot);
                QCPPolarAxisAngular* angular = area->angularAxis();
                QCPPolarAxisRadial* radial = area->radialAxis();
                angular->setAngle(30);
                angular->setRangeReversed(true);
                angular->setRange(0, 360);
                radial->setRange(0, 2);
                QVector<double> angle(361), r(361);
                for (int i = 0; i <= 360; ++i)
                {
                    angle[i] = i;
                    r[i] = 1 + 0.8 * std::cos(2 * i * std::numbers::pi / 180);
                }
                auto* graph = area->add<QCPPolarGraph>().get();
                graph->setData(angle, r);
                graph->setPen(QPen(QColor(230, 140, 20), 2));
            }
            else if (name == QLatin1String("polar-axis-selected"))
            {
                QCPPolarArea* area = polarArea(plot);
                QCPPolarAxisAngular* angular = area->angularAxis();
                QCPPolarAxisRadial* radial = area->radialAxis();
                angular->setRange(0, 360);
                radial->setRange(0, 1);
                QVector<double> angle(361), r(361);
                for (int i = 0; i <= 360; ++i)
                {
                    angle[i] = i;
                    r[i] = 0.5 + 0.4 * std::sin(3 * i * std::numbers::pi / 180);
                }
                auto* graph = area->add<QCPPolarGraph>().get();
                graph->setData(angle, r);
                graph->setPen(QPen(QColor(90, 90, 90), 2));
                angular->setSelectableParts(QCPPolarAxisAngular::spAxis
                                            | QCPPolarAxisAngular::spTickLabels);
                angular->setSelectedParts(QCPPolarAxisAngular::spAxis);
                radial->setSelectableParts(QCPPolarAxisRadial::spAxis
                                           | QCPPolarAxisRadial::spTickLabels);
                radial->setSelectedParts(QCPPolarAxisRadial::spTickLabels);
            }
            else if (name == QLatin1String("polar-clamped"))
            {
                QCPPolarArea* area = polarArea(plot);
                QCPPolarAxisAngular* angular = area->angularAxis();
                QCPPolarAxisRadial* radial = area->radialAxis();
                angular->setRange(0, 360);
                radial->setRange(0, 1);
                const QVector<double> angle { 0,   30,  60,  90,  120, 150, 170,
                                              190, 210, 240, 270, 300, 330, 360 };
                const QVector<double> r { 0.2, 1.6, 0.9, 1.8, 1.3, 0.5, 1.1,
                                          0.7, 1.9, 1.2, 0.3, 1.5, 0.8, 0.4 };
                auto* graph = area->add<QCPPolarGraph>().get();
                graph->setData(angle, r);
                graph->setPen(QPen(QColor(31, 119, 180), 2));
            }
            else if (name == QLatin1String("polar-sparse"))
            {
                QCPPolarArea* area = polarArea(plot);
                QCPPolarAxisAngular* angular = area->angularAxis();
                QCPPolarAxisRadial* radial = area->radialAxis();
                angular->setRange(0, 360);
                radial->setRange(0, 1);
                const QVector<double> angle { 0, 60, 120, 180, 240, 300 };
                const QVector<double> r { 0.9, 0.35, 0.8, 0.45, 0.95, 0.5 };
                auto* graph = area->add<QCPPolarGraph>().get();
                graph->setData(angle, r);
                graph->setPen(QPen(QColor(214, 39, 40), 2));
            }
        });
}
QImage userSystemMultiGraphScene()
{
    return render(
        [](QCustomPlot& plot)
        {
            plot.plotLayout()->clear();
            auto* rect = new SkewRect(&plot);
            plot.plotLayout()->addElement(0, 0, rect);
            rect->setShear(0.5);
            QFont font = qcp::test::referenceFont();
            font.setPixelSize(10);
            QCPAxis* key = rect->axis(QCPAxis::atBottom);
            QCPAxis* value = rect->axis(QCPAxis::atLeft);
            key->setRange(0, 10);
            value->setRange(0, 10);
            for (QCPAxis* axis : rect->axes())
            {
                axis->setTickLabelFont(font);
                axis->setLabelFont(font);
            }

            const QVector<double> keys { 0.5, 2, 3.5, 5, 6.5, 8 };
            auto* graph = key->axisRect()->add<QCPGraph>(key, value).get();
            graph->setData(
                keys, std::vector<QVector<double>> {
                          QVector<double> { 7, 6, 8, 7, 9, 8.5 },     // the step column
                          QVector<double> { 2, 4, 3, 5, 3.5, 6 },      // the line column
                          QVector<double> { 1, 1.5, 1, 2, 1.5, 2.5 },  // lower error bound
                          QVector<double> { 3, 3.5, 3, 4, 3.5, 4.5 },  // upper error bound
                          QVector<double> { 8, 9, 6, 8, 7, 9 }         // scatter-only column
                      });
            graph->setComponentLineStyle(0, QCPGraph::lsStepLeft);
            graph->setComponentLineStyle(1, QCPGraph::lsLine);
            graph->setComponentLineStyle(4, QCPGraph::lsNone);
            graph->setComponentScatterStyle(4, QCPScatterStyle(QCPScatterStyle::ssDisc, 6));
            graph->setComponentVisible(2, false);
            graph->setComponentVisible(3, false);
            graph->setComponentFill(0, 0.0, QColor(40, 110, 200, 70));
            rect->fillBetween(QCPHandle<QCPGraph>(graph), QCPHandle<QCPGraph>(graph),
                              { .brush = QColor(220, 160, 60, 70), .firstComponent = 1,
                                .secondComponent = 0 });
            graph->setComponentErrorBars(1, 2, 3, 6.0);
            graph->setColorValues(std::vector<double> { 0.0, 0.2, 0.4, 0.6, 0.8, 1.0 });
            graph->setName(QStringLiteral("multi"));
        });
}
QImage userSystemSkewScene()
{
    return render(
        [](QCustomPlot& plot)
        {
            plot.plotLayout()->clear();
            auto* rect = new SkewRect(&plot);
            plot.plotLayout()->addElement(0, 0, rect);
            rect->setShear(0.5);
            QFont font = qcp::test::referenceFont();
            font.setPixelSize(10);
            QCPAxis* key = rect->axis(QCPAxis::atBottom);
            QCPAxis* value = rect->axis(QCPAxis::atLeft);
            key->setRange(0, 10);
            value->setRange(0, 10);
            for (QCPAxis* axis : rect->axes())
            {
                axis->setTickLabelFont(font);
                axis->setLabelFont(font);
            }

            const QVector<double> keys { 0.5, 2, 3.5, 5, 6.5, 8 };
            auto* filled = key->axisRect()->add<QCPGraph>(key, value).get();
            filled->setData(keys, QVector<double> { 2, 4, 3, 5, 3.5, 6 });
            filled->setBrush(QColor(40, 110, 200, 70));
            filled->setPen(QPen(QColor(40, 110, 200), 2));
            filled->setName(QStringLiteral("filled"));

            auto* step = key->axisRect()->add<QCPGraph>(key, value).get();
            step->setLineStyle(QCPGraph::lsStepLeft);
            step->setData(QVector<double> { 1, 3, 5, 7, 9 },
                          QVector<double> { 7, 6, 8, 7, 9 });
            step->setPen(QPen(QColor(200, 60, 40), 2));
            step->setName(QStringLiteral("step"));

            auto* impulse = key->axisRect()->add<QCPGraph>(key, value).get();
            impulse->setLineStyle(QCPGraph::lsImpulse);
            impulse->setData(QVector<double> { 0.5, 1.5, 2.5, 3.5 },
                             QVector<double> { 1, 3, 2, 4 });
            impulse->setPen(QPen(QColor(60, 150, 70), 2));
            impulse->setName(QStringLiteral("impulse"));

            auto* dots = key->axisRect()->add<QCPGraph>(key, value).get();
            dots->setLineStyle(QCPGraph::lsNone);
            dots->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 6));
            dots->setData(QVector<double> { 2, 4, 6, 8 }, QVector<double> { 8, 9, 6, 8 });
            dots->setPen(QPen(QColor(120, 60, 160), 2));
            dots->setName(QStringLiteral("dots"));

            auto* lower = key->axisRect()->add<QCPGraph>(key, value).get();
            lower->setData(keys, QVector<double> { 1, 1.5, 1, 2, 1.5, 2.5 });
            lower->setPen(QPen(QColor(180, 120, 40), 2));
            auto* upper = key->axisRect()->add<QCPGraph>(key, value).get();
            upper->setData(keys, QVector<double> { 3, 3.5, 3, 4, 3.5, 4.5 });
            upper->setPen(QPen(QColor(180, 120, 40), 2));
            key->axisRect()->fillBetween(QCPHandle<QCPGraph>(upper), QCPHandle<QCPGraph>(lower),
                                         { .brush = QColor(220, 160, 60, 70) });

            std::vector<double> curveKeys, curveValues;
            for (int i = 0; i <= 24; ++i)
            {
                const double angle = 2.0 * std::numbers::pi * double(i) / 24.0;
                curveKeys.push_back(6.0 + 2.0 * std::cos(angle));
                curveValues.push_back(3.0 + 1.5 * std::sin(angle));
            }
            auto* curve = key->axisRect()->add<QCPCurve>(key, value).get();
            curve->setData(QVector<double>(curveKeys.begin(), curveKeys.end()),
                           QVector<double>(curveValues.begin(), curveValues.end()));
            curve->setPen(QPen(QColor(30, 30, 30), 2));
            curve->setName(QStringLiteral("curve"));
        });
}

// The symlog gallery's third panel (examples/user-code/symlog): a wave, both axes symlog, the
// band of the value axis visible around zero and the sub grids on.
QImage symlogScene()
{
    return render(
        [](QCustomPlot& plot)
        {
            plot.xAxis->setScale(QCPScale::symlog());
            plot.yAxis->setScale(QCPScale::symlog(0.015));
            plot.xAxis->setRange(-50, 50);
            plot.yAxis->setRange(-1, 1);
            for (QCPAxis* axis : plot.axisRect()->axes())
                axis->grid()->setSubGridVisible(true);
            QVector<double> x, wave;
            for (int i = 0; i < 1000; ++i)
            {
                x.append(-50.0 + 0.1 * i);
                wave.append(std::sin(x.back() / 3.0));
            }
            auto* graph = plot.axisRect()->add<QCPGraph>(plot.xAxis, plot.yAxis).get();
            graph->setData(x, wave);
            graph->setPen(QPen(QColor(31, 119, 180), 1.5));
        });
}

using namespace QCP::Literals;

// Copied from examples/user-code/lollipop/main.cpp: the picture is the target's own scene, at the
// target's own size, with the fifth lollipop picked the way a click picks it.
class QCPLollipop : public QCPPlottable1D<QCPGraphData>
{
public:
    using QCPPlottable1D::QCPPlottable1D;

    void draw(QCPDrawContext& context) const override
    {
        for (int i = 0; i < dataCount(); ++i)
        {
            const QCPGraphData& point = at(i);
            const QCP::Data tip { point.key, point.value };
            const bool picked = selection().contains(i);
            const QPen stem(pen().color(), picked ? 3.5 : 1.5);
            context.setDataIndex(i);
            context.line(QCP::Data { point.key, 0 }, tip, stem);
            context.circle(tip, picked ? 8_px : 5_px, stem, brush());
        }
    }

    void drawLegendIcon(QCPDrawContext& icon) const override
    {
        icon.line(QCP::AxisRatio { 0.5, 1.0 }, QCP::AxisRatio { 0.5, 0.4 }, pen());
        icon.circle(QCP::AxisRatio { 0.5, 0.35 }, 3_px, pen(), brush());
    }
};

QImage lollipopScene()
{
    QCustomPlot plot;
    plot.resize(640, 400);
    QFont font = qcp::test::referenceFont();
    font.setPixelSize(10);
    plot.setFont(font);
    for (QCPAxis* axis : plot.axisRect()->axes())
    {
        axis->setTickLabelFont(font);
        axis->setLabelFont(font);
    }

    const std::vector<double> month { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
    const std::vector<double> rainfall { 78, 61, 55, 52, 63, 49, 62, 53, 47, 64, 79, 76 };
    const QColor blue(31, 119, 180);
    auto ax = plot.axisRect();
    auto lollipops = ax->add<QCPLollipop>(month, rainfall)
                         .setName("rainfall (mm)")
                         .setPen(QPen(blue))
                         .setBrush(blue);
    lollipops->setSelectable(QCP::stSingleData);
    plot.setInteractions(QCP::iSelectPlottables);
    plot.legend->setVisible(true);
    ax->xAxis()->setLabel("month");
    plot.rescaleAxes();

    plot.show();
    QTest::qWaitForWindowExposed(&plot);
    plot.replot();
    const QCPGraphData& fifth = lollipops->at(4);
    const QPoint click(qRound(plot.xAxis->coordToPixel(fifth.key)),
                       qRound(plot.yAxis->coordToPixel(fifth.value)));
    QTest::mouseClick(&plot, Qt::LeftButton, Qt::NoModifier, click);
    plot.replot();
    return plot.toPixmap(640, 400).toImage().convertToFormat(QImage::Format_ARGB32);
}

} // namespace

QImage polarItemsScene()
{
    return render(
        [](QCustomPlot& plot)
        {
            QCPPolarArea* area = polarArea(plot);
            area->angularAxis()->setRange(0, 360);
            area->radialAxis()->setRange(0, 1.05);
            QCPCoordinateSystem* system = area->system(area->angularAxis(), area->radialAxis());
            const auto place = [&](QCPItemPosition* position, double angle, double radius)
            {
                position->setSystem(system);
                position->setType(QCPItemPosition::ptPlotCoords);
                position->setCoords(angle, radius);
            };
            const auto ratio = [&](QCPItemPosition* position, double x, double y)
            {
                position->setSystem(system);
                position->setType(QCPItemPosition::ptAxisRectRatio);
                position->setCoords(x, y);
            };
            const auto clip = [&](QCPItem* item)
            {
                item->setClipArea(area);
                item->setClipToAxisRect(true);
            };

            auto line = plot.add<QCPItemLine>();
            place(line->start, 150.0, 0.25);
            place(line->end, 300.0, 0.9);
            line->setPen(QPen(QColor(31, 119, 180), 2));
            clip(line);

            auto ellipse = plot.add<QCPItemEllipse>();
            ratio(ellipse->topLeft, 0.10, 0.10);
            ratio(ellipse->bottomRight, 0.55, 1.25); // past the rim: the disc cuts the bottom
            ellipse->setPen(QPen(QColor(44, 160, 44), 2));
            ellipse->setBrush(QBrush(QColor(44, 160, 44, 60)));
            clip(ellipse);

            // A span-like band across the lower half, in area ratios: its corners land outside the
            // disc, so the clip cuts it with the arc instead of the area rect.
            auto band = plot.add<QCPItemRect>();
            ratio(band->topLeft, -0.10, 0.85);
            ratio(band->bottomRight, 1.10, 1.25); // a corner past the rim: cut by the arc
            band->setPen(Qt::NoPen);
            band->setBrush(QBrush(QColor(255, 127, 14, 120)));
            clip(band);

            auto text = plot.add<QCPItemText>();
            text->setText(QStringLiteral("text"));
            text->setColor(QColor(214, 39, 40));
            place(text->position, 60.0, 0.75);
            clip(text);
        });
}


// Copied from examples/user-code/callout/main.cpp: the target's own class and scene, with the drags
// the picture is taken after.
class Callout : public QCPItem
{
public:
    Callout(QCP::Data tip, QString text) : mTip(tip), mText(std::move(text)) { }

    QCPItemAnchor* const tip = createAnchor("tip", [this] { return mTip; });
    QCPItemAnchor* const box = createAnchor("box", [this] { return mTip + offset(mBox.center()); });

    //! Where the drags land: the box's centre and its bottom-right corner, in plot pixels.
    QPointF boxCentrePixels() const
    {
        return QPointF(tip->pixelPosition().x() + mBox.center().x(),
                       tip->pixelPosition().y() + mBox.center().y());
    }
    QPointF cornerPixels() const
    {
        return QPointF(tip->pixelPosition().x() + mBox.right(),
                       tip->pixelPosition().y() + mBox.bottom());
    }

protected:
    enum Part { Pointer, Box, Corner };

    void draw(QCPDrawContext& context) const override
    {
        const auto topLeft = mTip + offset(mBox.topLeft());
        const auto bottomRight = mTip + offset(mBox.bottomRight());
        const QPen outline(QColor(60, 60, 60), 1.2);
        const QColor paper(255, 250, 225);

        context.setPart(Pointer);
        context.polygon({ mTip, mTip + offset(mBox.center() + QPointF(-12, 0)),
                          mTip + offset(mBox.center() + QPointF(12, 0)) },
                        outline, paper);
        context.setPart(Box);
        context.roundedRect(topLeft, bottomRight, 6_px, outline, paper);
        context.text(topLeft, bottomRight, mText, { .padding = QMarginsF(8, 6, 8, 6) });
        context.setPart(Corner);
        context.rect(bottomRight + QCP::PixelOffset { -8, -8 }, bottomRight, Qt::NoPen,
                     QColor(60, 60, 60));
    }

    void mousePressEvent(QCPMouseEvent* event) override
    {
        if (event->part() == Pointer)
            event->ignore(); // a press on the pointer pans the plot
    }

    void mouseMoveEvent(QCPMouseEvent* event) override
    {
        const QCP::PixelOffset moved = event->pixels() - event->lastPixels();
        const QPointF delta(moved.dx, moved.dy);
        if (event->part() == Box)
            mBox.translate(delta);
        else
            mBox.setBottomRight(mBox.bottomRight() + delta);
        mBox.setSize(mBox.size().expandedTo(QSizeF(80, 36)));
        update();
    }

private:
    static QCP::PixelOffset offset(QPointF p) { return { p.x(), p.y() }; }

    QCP::Data mTip;
    QString mText;
    QRectF mBox { 30, -110, 170, 60 }; // pixels from the tip; y grows downwards
};

//! The target's font and size, ready for a scene that shows its own picture.
void targetPlot(QCustomPlot& plot, int width, int height)
{
    plot.resize(width, height);
    QFont font = qcp::test::referenceFont();
    font.setPixelSize(10);
    plot.setFont(font);
    for (const QCPHandle<QCPAxisRect>& rect : plot.axisRects())
        for (QCPAxis* axis : rect->axes())
        {
            axis->setTickLabelFont(font);
            axis->setLabelFont(font);
        }
}

void sendMouse(QCustomPlot& plot, QEvent::Type type, QPoint pos, Qt::MouseButtons buttons)
{
    QMouseEvent event(type, pos, plot.mapToGlobal(pos), Qt::LeftButton, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(&plot, &event);
}

void drag(QCustomPlot& plot, QPoint from, QPoint by)
{
    sendMouse(plot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(plot, QEvent::MouseMove, from + by / 2, Qt::LeftButton);
    sendMouse(plot, QEvent::MouseMove, from + by, Qt::LeftButton);
    sendMouse(plot, QEvent::MouseButtonRelease, from + by, Qt::NoButton);
}

QImage calloutScene()
{
    QCustomPlot plot;
    targetPlot(plot, 720, 460);

    std::vector<double> t, signal;
    for (int i = 0; i <= 400; ++i)
    {
        t.push_back(i / 40.0);
        signal.push_back(std::sin(t.back()) * std::exp(-t.back() / 8));
    }
    auto ax = plot.axisRect();
    ax->line(t, signal, { .pen = QPen(QColor(31, 119, 180), 2), .name = "damped sine" });

    const double firstMax = std::numbers::pi / 2;
    auto peak = ax->add<Callout>(QCP::Data { firstMax, std::exp(-firstMax / 8) },
                                 "First maximum.\nDrag the box; resize it from its corner.");

    ax->ellipse(peak->tip + QCP::PixelOffset { -7, -7 }, peak->tip + QCP::PixelOffset { 7, 7 },
                { .pen = QPen(QColor(214, 39, 40), 2) });
    ax->text(peak->box + QCP::PixelOffset { 0, 50 }, "hangs below the box",
             { .alignment = Qt::AlignHCenter | Qt::AlignTop });

    plot.setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    plot.rescaleAxes();
    // The target's box hangs 110 px above the first maximum, which its own rescale puts at the top
    // edge: the picture gives the value axis the room the annotation needs, or there is nothing to
    // drag and nothing to see.
    plot.yAxis->setRange(plot.yAxis->range().lower, plot.yAxis->range().upper + 0.45);
    plot.replot();

    // The drags the picture is taken after: the box by (+40, +20), then its corner by (+30, +10).
    drag(plot, peak->boxCentrePixels().toPoint(), QPoint(40, 20));
    drag(plot, peak->cornerPixels().toPoint(), QPoint(30, 10));
    plot.replot();
    return plot.toPixmap(720, 460).toImage().convertToFormat(QImage::Format_ARGB32);
}

// Copied from examples/user-code/peak-fit/main.cpp: the fit and the peak bands, dragged like the
// target drags them.
struct Gaussian
{
    double centre;
    double width;
};

struct Fit
{
    std::vector<double> model;
    std::vector<double> residuals;
};

double basis(const std::vector<Gaussian>& peaks, std::size_t k, double x)
{
    if (k == 0)
        return 1.0;
    const double u = (x - peaks[k - 1].centre) / peaks[k - 1].width;
    return std::exp(-0.5 * u * u);
}

std::vector<double> solve(std::vector<std::vector<double>> a)
{
    const std::size_t n = a.size();
    for (std::size_t col = 0; col < n; ++col)
        for (std::size_t row = 0; row < n; ++row)
            if (row != col)
            {
                const double f = a[row][col] / a[col][col];
                for (std::size_t j = col; j <= n; ++j)
                    a[row][j] -= f * a[col][j];
            }
    std::vector<double> x(n);
    for (std::size_t i = 0; i < n; ++i)
        x[i] = a[i][n] / a[i][i];
    return x;
}

Fit fitAmplitudes(const std::vector<double>& x, const std::vector<double>& y,
                  const std::vector<Gaussian>& peaks)
{
    const std::size_t n = peaks.size() + 1;
    std::vector<std::vector<double>> normal(n, std::vector<double>(n + 1, 0.0));
    for (std::size_t i = 0; i < x.size(); ++i)
        for (std::size_t k = 0; k < n; ++k)
        {
            for (std::size_t j = 0; j < n; ++j)
                normal[k][j] += basis(peaks, k, x[i]) * basis(peaks, j, x[i]);
            normal[k][n] += basis(peaks, k, x[i]) * y[i];
        }
    const std::vector<double> amplitude = solve(normal);
    Fit fit;
    for (std::size_t i = 0; i < x.size(); ++i)
    {
        double model = 0;
        for (std::size_t k = 0; k < n; ++k)
            model += amplitude[k] * basis(peaks, k, x[i]);
        fit.model.push_back(model);
        fit.residuals.push_back(y[i] - model);
    }
    return fit;
}

class Peak : public QCPItem
{
public:
    Peak(double centre, double width) : mGaussian { centre, width } { }

    Gaussian gaussian() const { return mGaussian; }

protected:
    enum Part { Body, LeftEdge, RightEdge };

    void draw(QCPDrawContext& context) const override
    {
        const double left = mGaussian.centre - mGaussian.width;
        const double right = mGaussian.centre + mGaussian.width;
        const QPen edge(QColor(214, 39, 40), 2);

        context.setPart(Body);
        context.rect(QCP::DataXRatioY { left, 0 }, QCP::DataXRatioY { right, 1 }, Qt::NoPen,
                     QColor(214, 39, 40, 40));
        context.line(QCP::DataXRatioY { mGaussian.centre, 0 },
                     QCP::DataXRatioY { mGaussian.centre, 1 }, QPen(edge.color(), 1, Qt::DashLine));
        context.setPart(LeftEdge);
        context.line(QCP::DataXRatioY { left, 0 }, QCP::DataXRatioY { left, 1 }, edge);
        context.setPart(RightEdge);
        context.line(QCP::DataXRatioY { right, 0 }, QCP::DataXRatioY { right, 1 }, edge);
    }

    void mousePressEvent(QCPMouseEvent* event) override { event->accept(); }

    void mouseMoveEvent(QCPMouseEvent* event) override
    {
        const double dx = event->data().x - event->lastData().x;
        switch (event->part())
        {
            case Body: mGaussian.centre += dx; break;
            case LeftEdge: mGaussian.width -= dx; break;
            case RightEdge: mGaussian.width += dx; break;
        }
        mGaussian.width = std::max(mGaussian.width, 0.3);
        update();
    }

private:
    Gaussian mGaussian;
};

// std::normal_distribution is implementation-defined: libstdc++, libc++ and MSVC draw different
// numbers from the same engine, so the picture would differ per platform. Box-Muller on the
// engine's own output, which the standard specifies, gives the same noise everywhere.
double portableNormal(std::mt19937& rng, double sigma)
{
    const double u1 = (rng() + 1.0) / 4294967296.0;
    const double u2 = rng() / 4294967296.0;
    return sigma * std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * std::numbers::pi * u2);
}

QImage peakFitScene()
{
    QCustomPlot plot;
    targetPlot(plot, 800, 560);

    std::mt19937 rng(7);
    const std::vector<Gaussian> truth { { 25, 2.5 }, { 48, 4 }, { 70, 3 } };
    const double height[] = { 120, 80, 150 };
    std::vector<double> energy, counts;
    for (int i = 0; i <= 400; ++i)
    {
        energy.push_back(i * 0.25);
        double c = 12 + portableNormal(rng, 6.0);
        for (std::size_t k = 0; k < truth.size(); ++k)
            c += height[k] * basis(truth, k + 1, energy.back());
        counts.push_back(c);
    }

    plot.setAxisRectGrid(2, 1, { .shareX = true, .rowStretch = { 1, 0.35 } });
    auto spectrum = plot.axisRect(0, 0);
    auto residuals = plot.axisRect(1, 0);
    spectrum->yAxis()->setLabel("counts");
    residuals->yAxis()->setLabel("residual");
    residuals->xAxis()->setLabel("energy (keV)");

    QList<QCPHandle<Peak>> peaks;
    for (double centre : { 22.0, 52.0, 67.0 })
        peaks << spectrum->add<Peak>(centre, 4.0);
    const auto guesses = [&] {
        std::vector<Gaussian> g;
        for (const auto& peak : peaks)
            g.push_back(peak->gaussian());
        return g;
    };

    const Fit first = fitAmplitudes(energy, counts, guesses());
    spectrum->line(energy, counts, { .pen = QPen(Qt::gray), .name = "spectrum" });
    auto model = spectrum->line(energy, first.model, { .pen = QPen(Qt::black, 2), .name = "fit" });
    auto misfit = residuals->line(energy, first.residuals,
                                 { .pen = QPen(Qt::gray), .name = "residual" });

    const auto refit = [&] {
        const Fit fit = fitAmplitudes(energy, counts, guesses());
        model->setData(energy, fit.model);
        misfit->setData(energy, fit.residuals);
    };
    for (const auto& peak : peaks)
        QObject::connect(peak, &QCPItem::changed, &plot, refit);

    plot.legend->setVisible(true);
    plot.rescaleAxes();
    plot.replot();

    // The middle peak's body, dragged by 5 keV: the fit and the residual follow it.
    const QRect rect = spectrum->rect();
    const QPoint from(qRound(spectrum->xAxis()->coordToPixel(52.0)), rect.center().y());
    const int by = qRound(spectrum->xAxis()->coordToPixel(57.0)
                          - spectrum->xAxis()->coordToPixel(52.0));
    drag(plot, from, QPoint(by, 0));
    plot.replot();
    return plot.toPixmap(800, 560).toImage().convertToFormat(QImage::Format_ARGB32);
}

namespace
{
// The brick-breaker target's items, copied: the picture is the example's first frame.
class Brick : public QCPItem
{
public:
    Brick(QRectF box, QColor color) : mBox(box), mColor(color) { }

protected:
    void draw(QCPDrawContext& context) const override
    {
        context.rect(mBox, QPen(Qt::white), mColor);
    }

private:
    QRectF mBox;
    QColor mColor;
};

class Ball : public QCPItem
{
public:
    explicit Ball(double radius) : mRadius(radius) { }

    void setCentre(QPointF centre)
    {
        mCentre = centre;
        update();
    }

protected:
    void draw(QCPDrawContext& context) const override
    {
        context.circle(QCP::Data { mCentre.x(), mCentre.y() }, mRadius, Qt::NoPen, Qt::white);
    }

private:
    double mRadius;
    QPointF mCentre;
};

class Paddle : public QCPItem
{
protected:
    void draw(QCPDrawContext& context) const override
    {
        context.rect(QRectF(43, 2, 14, 1.5), Qt::NoPen, QColor(90, 200, 255));
    }
};

QImage brickBreakerScene()
{
    QCustomPlot plot;
    targetPlot(plot, 800, 520);
    plot.setBackground(QColor(20, 20, 30));

    auto ax = plot.axisRect();
    ax->xAxis()->setRange(0, 100);
    ax->yAxis()->setRange(0, 60);
    ax->xAxis()->setVisible(false);
    ax->yAxis()->setVisible(false);
    for (int row = 0; row < 6; ++row)
        for (int column = 0; column < 12; ++column)
            ax->add<Brick>(QRectF(column * 100.0 / 12, 36 + row * 3, 100.0 / 12, 3),
                           QColor::fromHsv(row * 50, 160, 230));
    ax->add<Paddle>();
    ax->add<Ball>(1.0)->setCentre(QPointF(50, 6));

    plot.replot();
    return plot.toPixmap(800, 520).toImage().convertToFormat(QImage::Format_ARGB32);
}

// The Asteroids target's own rules and items, copied from the example.
struct Body
{
    QPointF pos;
    QPointF vel;
    double angle = 0;
    double size = 1;
    double life = 0;
};

struct Game
{
    static constexpr double width = 100, height = 75;
    Body ship;
    std::vector<Body> rocks, bullets;
    std::set<int> held;
    int score = 0;
    bool over = false;

    Game() { restart(); }

    void restart()
    {
        ship = Body { { width / 2, height / 2 }, { }, 90, 2 };
        rocks.clear();
        bullets.clear();
        score = 0;
        over = false;
        std::mt19937 rng(3);
        std::uniform_real_distribution<double> unit(0, 1);
        for (int i = 0; i < 5; ++i)
            rocks.push_back({ { unit(rng) * width, 0 },
                              { 12 * unit(rng) - 6, 12 * unit(rng) - 6 },
                              360 * unit(rng),
                              6 });
    }

    void press(int key)
    {
        held.insert(key);
        if (key == Qt::Key_Space && !over)
            fire();
        if (key == Qt::Key_R && over)
            restart();
    }

    void release(int key) { held.erase(key); }

    void step(double dt)
    {
        if (over)
            return;
        steer(dt);
        move(ship, dt);
        for (Body& rock : rocks)
            move(rock, dt);
        for (Body& bullet : bullets)
            move(bullet, dt);
        std::erase_if(bullets, [](const Body& b) { return b.life <= 0; });
        collide();
    }

private:
    static QPointF heading(double degrees)
    {
        const double rad = degrees * std::numbers::pi / 180;
        return { std::cos(rad), std::sin(rad) };
    }

    static QPointF wrap(QPointF p)
    {
        return { std::fmod(p.x() + width, width), std::fmod(p.y() + height, height) };
    }

    static double distance(QPointF a, QPointF b)
    {
        return std::hypot(a.x() - b.x(), a.y() - b.y());
    }

    void steer(double dt)
    {
        if (held.contains(Qt::Key_Left))
            ship.angle += 200 * dt;
        if (held.contains(Qt::Key_Right))
            ship.angle -= 200 * dt;
        if (held.contains(Qt::Key_Up))
            ship.vel += 30 * dt * heading(ship.angle);
    }

    static void move(Body& body, double dt)
    {
        body.pos = wrap(body.pos + dt * body.vel);
        body.life -= dt;
    }

    void fire()
    {
        bullets.push_back({ ship.pos, ship.vel + 60 * heading(ship.angle), ship.angle, 0.4, 1.2 });
    }

    void collide()
    {
        std::vector<Body> fragments;
        std::erase_if(rocks,
                      [&](const Body& rock)
                      {
                          const auto hit = std::ranges::find_if(
                              bullets, [&](const Body& b)
                              { return b.life > 0 && distance(b.pos, rock.pos) < rock.size; });
                          if (hit == bullets.end())
                              return false;
                          hit->life = 0;
                          score += 10;
                          if (rock.size > 2)
                              for (double turn : { 90.0, -90.0 })
                                  fragments.push_back(
                                      { rock.pos, QTransform().rotate(turn).map(rock.vel) * 1.5,
                                        rock.angle + turn, rock.size / 2 });
                          return true;
                      });
        rocks.insert(rocks.end(), fragments.begin(), fragments.end());
        over
            = std::ranges::any_of(rocks, [&](const Body& r)
                                  { return distance(r.pos, ship.pos) < r.size + ship.size * 0.6; });
    }
};

class Sprites : public QCPItem
{
public:
    Sprites(QPolygonF outline, QColor color) : mOutline(std::move(outline)), mPen(color, 1.5) { }

    void setBodies(std::vector<Body> bodies)
    {
        mBodies = std::move(bodies);
        update();
    }

protected:
    void draw(QCPDrawContext& context) const override
    {
        for (const Body& b : mBodies)
        {
            const QTransform pose = QTransform()
                                        .translate(b.pos.x(), b.pos.y())
                                        .rotate(b.angle)
                                        .scale(b.size, b.size);
            context.polygon(pose.map(mOutline), mPen);
        }
    }

private:
    QPolygonF mOutline;
    QPen mPen;
    std::vector<Body> mBodies;
};

// The example picks "monospace"; the picture takes the reference font instead, so that it is the
// same picture on every platform.
class Hud : public QCPItem
{
public:
    explicit Hud(QFont font) : mFont(std::move(font)) { }

    void setScore(int score)
    {
        mScore = score;
        update();
    }

    void setGameOver(bool over)
    {
        mOver = over;
        update();
    }

protected:
    void draw(QCPDrawContext& context) const override
    {
        context.text(QCP::AxisRatio { 0.02, 0.02 }, QString("SCORE %1").arg(mScore),
                     { .font = mFont, .color = Qt::white });
        if (!mOver)
            return;
        context.rect(QCP::AxisRatio { 0.2, 0.4 }, QCP::AxisRatio { 0.8, 0.6 }, QPen(Qt::white),
                     QColor(40, 0, 0, 220));
        context.text(QCP::AxisRatio { 0.5, 0.5 }, "GAME OVER - press R",
                     { .font = mFont, .color = Qt::white, .alignment = Qt::AlignCenter });
    }

private:
    QFont mFont;
    int mScore = 0;
    bool mOver = false;
};

void sendKey(QCustomPlot& plot, QEvent::Type type, Qt::Key key)
{
    QKeyEvent event(type, key, Qt::NoModifier);
    QCoreApplication::sendEvent(&plot, &event);
}

//! Up and Left held and one bullet fired, thirty frames later; \a over also shows the banner.
QImage asteroidsScene(bool over)
{
    QCustomPlot plot;
    targetPlot(plot, 800, 600);
    plot.setBackground(Qt::black);

    const QPolygonF shipOutline { { 1, 0 }, { -0.7, 0.6 }, { -0.4, 0 }, { -0.7, -0.6 } };
    const QPolygonF rockOutline { { 1, 0 },    { 0.6, 0.7 },   { 0, 0.9 },  { -0.7, 0.8 },
                                  { -1, 0.1 }, { -0.6, -0.8 }, { 0.1, -1 }, { 0.8, -0.6 } };
    const QPolygonF bulletOutline { { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 } };

    auto ax = plot.axisRect();
    ax->xAxis()->setRange(0, Game::width);
    ax->yAxis()->setRange(0, Game::height);
    ax->xAxis()->setVisible(false);
    ax->yAxis()->setVisible(false);
    auto rocks = ax->add<Sprites>(rockOutline, QColor(200, 200, 200));
    auto bullets = ax->add<Sprites>(bulletOutline, QColor(255, 220, 90));
    auto ship = ax->add<Sprites>(shipOutline, QColor(120, 220, 255));
    QFont hudFont = qcp::test::referenceFont();
    hudFont.setPixelSize(16);
    hudFont.setBold(true);
    auto hud = ax->add<Hud>(hudFont);

    Game game;
    // The example's five rocks come from the standard library's distribution, which differs
    // between implementations: a fixed table keeps the picture the same everywhere.
    game.rocks = { { { 18, 12 }, { 3, 1.5 }, 20, 6 },
                   { { 45, 20 }, { -2, 2 }, 80, 6 },
                   { { 76, 8 }, { 1.5, 3 }, 140, 6 },
                   { { 30, 55 }, { 2.5, -1 }, 200, 6 },
                   { { 68, 48 }, { -1, -2.5 }, 300, 6 } };
    QObject::connect(&plot, &QCustomPlot::keyPress,
                     [&](QKeyEvent* event)
                     {
                         if (!event->isAutoRepeat())
                             game.press(event->key());
                     });
    QObject::connect(&plot, &QCustomPlot::keyRelease,
                     [&](QKeyEvent* event)
                     {
                         if (!event->isAutoRepeat())
                             game.release(event->key());
                     });

    sendKey(plot, QEvent::KeyPress, Qt::Key_Up);
    sendKey(plot, QEvent::KeyPress, Qt::Key_Left);
    sendKey(plot, QEvent::KeyPress, Qt::Key_Space);
    for (int frame = 0; frame < 30; ++frame)
        game.step(1.0 / 60);
    game.over = over;

    rocks->setBodies(game.rocks);
    bullets->setBodies(game.bullets);
    ship->setBodies({ game.ship });
    hud->setScore(game.score);
    hud->setGameOver(game.over);
    plot.replot();
    return plot.toPixmap(800, 600).toImage().convertToFormat(QImage::Format_ARGB32);
}
} // namespace

void TestReferenceM4e::errorBarsPlain()
{
    check(errorBarsScene(false, false, false, false), "errorbars-plain.png");
}

void TestReferenceM4e::errorBarsVerticalKey()
{
    check(errorBarsScene(true, false, false, false), "errorbars-vertical-key.png");
}

void TestReferenceM4e::errorBarsLog()
{
    check(errorBarsScene(false, true, false, false), "errorbars-log.png");
}

void TestReferenceM4e::errorBarsSelected()
{
    check(errorBarsScene(false, false, true, false), "errorbars-selected.png");
}

void TestReferenceM4e::errorBarsLegend()
{
    check(errorBarsScene(false, false, false, true), "errorbars-legend.png");
}

void TestReferenceM4e::errorBarsKeyErrorLegend()
{
    check(errorBarsScene(false, false, false, true, QCPErrorBars::etKeyError),
          "errorbars-key-error-legend.png");
}

void TestReferenceM4e::errorBarsHorizontalValueLegend()
{
    check(errorBarsScene(true, false, false, true), "errorbars-horizontal-value-legend.png");
}

void TestReferenceM4e::financialCandle()
{
    check(financialScene(false, false, false, false, false), "financial-candle.png");
}

void TestReferenceM4e::financialOhlc()
{
    check(financialScene(true, false, false, false, false), "financial-ohlc.png");
}

void TestReferenceM4e::financialVerticalKey()
{
    check(financialScene(false, true, false, false, false), "financial-vertical-key.png");
}

void TestReferenceM4e::financialLog()
{
    check(financialScene(false, false, true, false, false), "financial-log.png");
}

void TestReferenceM4e::financialSelected()
{
    check(financialScene(false, false, false, true, false), "financial-selected.png");
}

void TestReferenceM4e::financialLegend()
{
    check(financialScene(false, false, false, false, true), "financial-legend.png");
}

void TestReferenceM4e::financialOhlcLegend()
{
    check(financialScene(true, false, false, false, true), "financial-ohlc-legend.png");
}

void TestReferenceM4e::financialSingleColourLegend()
{
    check(financialScene(false, false, false, false, true, false),
          "financial-single-colour-legend.png");
}

void TestReferenceM4e::financialOhlcSingleColourLegend()
{
    check(financialScene(true, false, false, false, true, false),
          "financial-ohlc-single-colour-legend.png");
}

void TestReferenceM4e::statisticalBoxPlain()
{
    check(statisticalBoxScene(false, false, false), "statbox-plain.png");
}

void TestReferenceM4e::statisticalBoxVerticalKey()
{
    check(statisticalBoxScene(true, false, false), "statbox-vertical-key.png");
}

void TestReferenceM4e::statisticalBoxSelected()
{
    check(statisticalBoxScene(false, true, false), "statbox-selected.png");
}

void TestReferenceM4e::statisticalBoxLegend()
{
    check(statisticalBoxScene(false, false, true), "statbox-legend.png");
}

void TestReferenceM4e::statisticalBoxLog()
{
    check(statisticalBoxScene(false, false, false, true), "statbox-log.png");
}

void TestReferenceM4e::intervalsPlain()
{
    check(intervalsScene(false, false, false), "intervals-plain.png");
}

void TestReferenceM4e::intervalsLanes()
{
    check(intervalsScene(true, false, false), "intervals-lanes.png");
}

void TestReferenceM4e::intervalsSelected()
{
    check(intervalsScene(false, true, false), "intervals-selected.png");
}

void TestReferenceM4e::intervalsLegend()
{
    check(intervalsScene(false, false, true), "intervals-legend.png");
}

void TestReferenceM4e::intervalsIconLegend()
{
    check(intervalsIconScene(), "intervals-icon-legend.png");
}

void TestReferenceM4e::itemLineArrows()
{
    check(itemScene("line-arrows"), "item-line-arrows.png");
}

void TestReferenceM4e::itemStraightLine()
{
    check(itemScene("straightline"), "item-straightline.png");
}

void TestReferenceM4e::itemCurve()
{
    check(itemScene("curve"), "item-curve.png");
}

void TestReferenceM4e::itemBracket()
{
    check(itemScene("bracket"), "item-bracket.png");
}

void TestReferenceM4e::itemRect()
{
    check(itemScene("rect"), "item-rect.png");
}

void TestReferenceM4e::itemEllipse()
{
    check(itemScene("ellipse"), "item-ellipse.png");
}

void TestReferenceM4e::itemPixmap()
{
    check(itemScene("pixmap"), "item-pixmap.png");
}

void TestReferenceM4e::itemPixmapScaled()
{
    check(itemScene("pixmap-scaled"), "item-pixmap-scaled.png");
}

void TestReferenceM4e::itemTracer()
{
    check(itemScene("tracer"), "item-tracer.png");
}

void TestReferenceM4e::spanHorizontal()
{
    check(spanScene("h"), "span-h.png");
}

void TestReferenceM4e::spanVertical()
{
    check(spanScene("v"), "span-v.png");
}

void TestReferenceM4e::spanRect()
{
    check(spanScene("r"), "span-r.png");
}

void TestReferenceM4e::spanSelected()
{
    check(spanScene("selected"), "span-selected.png");
}

void TestReferenceM4e::polarBasic()
{
    check(polarScene("polar-basic"), "polar-basic.png");
}

void TestReferenceM4e::polarRadians()
{
    check(polarScene("polar-radians"), "polar-radians.png");
}

void TestReferenceM4e::polarGallery()
{
    check(polarScene("polar-gallery"), "polar-gallery.png");
}

void TestReferenceM4e::polarLog()
{
    check(polarScene("polar-log"), "polar-log.png");
}

void TestReferenceM4e::polarRotated()
{
    check(polarScene("polar-rotated"), "polar-rotated.png");
}

void TestReferenceM4e::polarAxisSelected()
{
    check(polarScene("polar-axis-selected"), "polar-axis-selected.png");
}

void TestReferenceM4e::polarClamped()
{
    check(polarScene("polar-clamped"), "polar-clamped.png");
}

void TestReferenceM4e::polarSparse()
{
    check(polarScene("polar-sparse"), "polar-sparse.png");
}

void TestReferenceM4e::userSystemSkew()
{
    check(userSystemSkewScene(), "user-system-skew.png");
}

void TestReferenceM4e::userSystemMultiGraph()
{
    check(userSystemMultiGraphScene(), "user-system-multigraph.png");
}

void TestReferenceM4e::polarItems()
{
    check(polarItemsScene(), "polar-items.png");
}

void TestReferenceM4e::symlogSingleRect()
{
    check(symlogScene(), "symlog-single-rect.png");
}

void TestReferenceM4e::lollipopTarget()
{
    check(lollipopScene(), "lollipop-target.png");
}

void TestReferenceM4e::calloutTarget()
{
    check(calloutScene(), "callout-target.png");
}

void TestReferenceM4e::peakFitTarget()
{
    check(peakFitScene(), "peak-fit-target.png");
}

void TestReferenceM4e::brickBreakerTarget()
{
    check(brickBreakerScene(), "brick-breaker-target.png");
}

void TestReferenceM4e::asteroidsTarget()
{
    check(asteroidsScene(false), "asteroids-target.png");
}

void TestReferenceM4e::asteroidsOverTarget()
{
    check(asteroidsScene(true), "asteroids-over-target.png");
}

void TestReferenceM4e::fillBetweenTarget()
{
    check(render(
              [](QCustomPlot& plot)
              {
                  const std::vector<double> keys { 0, 1, 2, 3 };
                  const std::vector<double> upper { 0.8, 0.8, 0.2, 0.2 };
                  const std::vector<double> lower { 0, 0, 1, 1 };
                  const QColor first(31, 119, 180), second(255, 127, 14);
                  const QCPHandle<QCPAxisRect> rect = plot.axisRect();
                  const auto upperLine = rect->line(
                      keys, upper,
                      { .pen = QPen(first, 1.5, Qt::DashLine),
                        .scatterStyle = QCPScatterStyle(QCPScatterStyle::ssDisc, first, 7) });
                  const auto lowerLine = rect->line(
                      keys, lower,
                      { .pen = QPen(second, 1.5, Qt::DashLine),
                        .scatterStyle = QCPScatterStyle(QCPScatterStyle::ssDisc, second, 7) });
                  rect->fillBetween(upperLine, lowerLine,
                                    { .brush = QColor(44, 160, 44, 76),
                                      .belowBrush = QColor(214, 39, 40, 76) });
                  rect->setTitle(QStringLiteral("interpolation=True"));
                  plot.rescaleAxes();
              },
              QSize(640, 400)),
          "fill-between-target.png");
}

void TestReferenceM4e::insetZoomTarget()
{
    check(render(
              [](QCustomPlot& plot)
              {
                  std::vector<double> x, y, z;
                  for (int i = 0; i < 150; ++i)
                  {
                      x.push_back(-3 + (i + 0.5) * 7.0 / 150);
                      y.push_back(-4 + (i + 0.5) * 7.0 / 150);
                  }
                  for (int column = 0; column < 150; ++column)
                      for (int row = 0; row < 150; ++row)
                          z.push_back(zoomSampleImage(column, row));
                  const QCPColorMapOptions look { .gradient = QCPColorGradient::gpViridis,
                                                  .colorBar = false };
                  const QCPHandle<QCPAxisRect> ax = plot.axisRect();
                  ax->colorMap(x, y, z, look);
                  ax->xAxis()->setRange(-3, 4);
                  ax->yAxis()->setRange(-4, 3);
                  const QCPHandle<QCPAxisRect> inset = ax->addInset(QRectF(0.5, 0.03, 0.47, 0.47));
                  inset->colorMap(x, y, z, look);
                  inset->xAxis()->setRange(-1.5, -0.9);
                  inset->yAxis()->setRange(-2.5, -1.9);
                  inset->xAxis()->setTickLabels(false);
                  inset->yAxis()->setTickLabels(false);
                  ax->zoomIndicator(inset, { .pen = QPen(Qt::black) });
              },
              QSize(560, 540)),
          "inset-zoom-target.png");
}

void TestReferenceM4e::annotationsTarget()
{
    // The annotations target: matplotlib's fourteen connector styles, one panel each, and the spare
    // cell with a text and its arrow as one annotation.
    check(render(
              [](QCustomPlot& plot)
              {
                  plot.setAxisRectGrid(3, 5);
                  const QCP::Data point { 0.3, 0.2 }, label { 0.8, 0.6 };
                  const QColor blue(31, 119, 180), grey(128, 128, 128);
                  const auto showPanel = [&](QCPAxisRect* ax)
                  {
                      ax->line(std::array { point.x, label.x }, std::array { point.y, label.y },
                               { .pen = Qt::NoPen,
                                 .scatterStyle
                                 = QCPScatterStyle(QCPScatterStyle::ssDisc, blue, 5) });
                      ax->xAxis()->setRange(0, 1);
                      ax->yAxis()->setRange(0, 1.25);
                      for (const auto& axis : ax->axes())
                      {
                          axis->setVisible(true);
                          axis->setTicks(false);
                          axis->grid()->setVisible(false);
                      }
                  };
                  const struct
                  {
                      int row, column;
                      QCPConnector connector;
                      QString name;
                  } demos[] = {
                      { 0, 0, QCPConnector::curve(90, 0), QStringLiteral("curve(90, 0)") },
                      { 1, 0, QCPConnector::curve(0, 90), QStringLiteral("curve(0, 90)") },
                      { 0, 1, QCPConnector::arc(0), QStringLiteral("arc(0)") },
                      { 1, 1, QCPConnector::arc(0.3), QStringLiteral("arc(0.3)") },
                      { 2, 1, QCPConnector::arc(-0.3), QStringLiteral("arc(-0.3)") },
                      { 0, 2, QCPConnector::elbow(-90, 180), QStringLiteral("elbow(-90, 180)") },
                      { 1, 2, QCPConnector::elbow(-90, 180, 5_px),
                        QStringLiteral("elbow(-90, 180,\n5 px)") },
                      { 2, 2, QCPConnector::elbow(-90, 10, 5_px),
                        QStringLiteral("elbow(-90, 10,\n5 px)") },
                      { 0, 3,
                        QCPConnector::arms(-90, 30_px, 0, 30_px),
                        QStringLiteral("arms(-90, 30 px,\n0, 30 px)") },
                      { 1, 3,
                        QCPConnector::arms(-90, 30_px, 0, 30_px, 5_px),
                        QStringLiteral("arms(-90, 30 px,\n0, 30 px, 5 px)") },
                      { 2, 3,
                        QCPConnector::arms(-90, 0_px, 0,
                                           40_px),
                        QStringLiteral("arms(-90, 0 px,\n0, 40 px)") },
                      { 0, 4, QCPConnector::bracket(0.3), QStringLiteral("bracket(0.3)") },
                      { 1, 4, QCPConnector::bracket(-0.3), QStringLiteral("bracket(-0.3)") },
                      { 2, 4, QCPConnector::bracket(-0.2, 180), QStringLiteral("bracket(-0.2, 180)") },
                  };
                  for (const auto& demo : demos)
                  {
                      QCPAxisRect* ax = plot.axisRect(demo.row, demo.column);
                      showPanel(ax);
                      ax->annotation(point, label, QString(),
                                     { .connector = demo.connector, .pen = QPen(grey),
                                       .head = QCPLineEnding::esLineArrow,
                                       .shrink = 5_px });
                      ax->text(QCP::AxisRatio { 0.05, 0.05 }, demo.name);
                  }

                  // matplotlib leaves this cell empty; it shows the text and its arrow as one
                  // annotation.
                  QCPAxisRect* spare = plot.axisRect(2, 0);
                  showPanel(spare);
                  spare->annotation(
                      point, label, QStringLiteral("text and\narrow"),
                      { .connector = QCPConnector::elbow(-90, 180, 5_px),
                        .pen = QPen(grey),
                        .head = QCPLineEnding::esLineArrow,
                        .shrink = 5_px,
                        .textStyle = { .pen = QPen(grey),
                                       .brush = Qt::white,
                                       .padding = QMarginsF(4, 2, 4, 2) } });
              },
              QSize(700, 630)),
          "annotations-target.png");
}
