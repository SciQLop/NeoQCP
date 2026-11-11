#include "test-reference-m4e.h"

#include <QImage>
#include <QPixmap>
#include <algorithm>
#include <cmath>
#include <complex>
#include <functional>
#include <numbers>
#include <numeric>
#include <utility>
#include <vector>

#include "qcustomplot.h"
#include "reference-image.h"

namespace
{
const QString kDir = QStringLiteral("test-reference-m4e");

QImage render(const std::function<void(QCustomPlot&)>& setup)
{
    QCustomPlot plot;
    plot.resize(400, 300);
    QFont font(QStringLiteral("Helvetica"));
    font.setPixelSize(10);
    plot.setFont(font);
    for (QCPAxis* axis : plot.axisRect()->axes())
    {
        axis->setTickLabelFont(font);
        axis->setLabelFont(font);
    }
    setup(plot);
    plot.replot();
    return plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
}

void check(const QImage& image, const char* name)
{
    QString why;
    QVERIFY2(qcp::test::matchesReference(image, kDir, QLatin1String(name), &why),
             qPrintable(QStringLiteral("%1: %2").arg(QLatin1String(name), why)));
}

QVector<double> sampleKeys()
{
    return QVector<double> { 0.5, 1.5, 2.5, 3.5, 4.5, 5.5 };
}

QImage errorBarsScene(bool verticalKey, bool logValue, bool selected, bool legend)
{
    return render(
        [=](QCustomPlot& plot)
        {
            QCPAxis* key = verticalKey ? plot.yAxis : plot.xAxis;
            QCPAxis* value = verticalKey ? plot.xAxis : plot.yAxis;
            key->setRange(0, 6);
            value->setRange(logValue ? 0.5 : -1, logValue ? 10 : 1.5);
            auto* graph = new QCPGraph(key, value);
            graph->setLineStyle(QCPGraph::lsNone);
            graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 6));
            graph->setData(sampleKeys(),
                           logValue ? QVector<double> { 0.7, 1.2, 2.4, 1.6, 3.4, 5.5 }
                                    : QVector<double> { 0.2, -0.4, 0.6, 0.1, -0.3, 0.5 });
            auto* bars = new QCPErrorBars(key, value);
            bars->setDataPlottable(graph);
            bars->setErrorType(QCPErrorBars::etValueError);
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
                value->setScaleType(QCPAxis::stLogarithmic);
        });
}

QImage financialScene(bool ohlc, bool verticalKey, bool logValue, bool selected, bool legend)
{
    return render(
        [=](QCustomPlot& plot)
        {
            QCPAxis* key = verticalKey ? plot.yAxis : plot.xAxis;
            QCPAxis* value = verticalKey ? plot.xAxis : plot.yAxis;
            key->setRange(0, 7);
            value->setRange(logValue ? 1 : 0, 10);
            auto* financial = new QCPFinancial(key, value);
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
                value->setScaleType(QCPAxis::stLogarithmic);
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
            auto* box = new QCPStatisticalBox(key, value);
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
                value->setScaleType(QCPAxis::stLogarithmic);
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
            auto* intervals = new QCPIntervals(plot.xAxis, plot.yAxis, &layout);
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
                auto* line = new QCPItemLine(&plot);
                line->start->setCoords(0.5, 0.2);
                line->end->setCoords(5.5, 0.8);
                line->setHead(QCPLineEnding::esSpikeArrow);
                line->setTail(QCPLineEnding::esDisc);
                line->setPen(QPen(QColor(30, 80, 160), 2));
            }
            else if (name == QLatin1String("straightline"))
            {
                auto* line = new QCPItemStraightLine(&plot);
                line->point1->setCoords(1.0, 0.1);
                line->point2->setCoords(4.0, 0.9);
                line->setPen(QPen(QColor(160, 40, 120), 2));
            }
            else if (name == QLatin1String("curve"))
            {
                auto* curve = new QCPItemCurve(&plot);
                curve->start->setCoords(0.5, 0.2);
                curve->startDir->setCoords(2.0, 0.9);
                curve->endDir->setCoords(4.0, 0.1);
                curve->end->setCoords(5.5, 0.8);
                curve->setPen(QPen(QColor(20, 140, 90), 2));
            }
            else if (name == QLatin1String("bracket"))
            {
                auto* bracket = new QCPItemBracket(&plot);
                bracket->left->setCoords(1.0, 0.75);
                bracket->right->setCoords(5.0, 0.25);
                bracket->setLength(30);
                bracket->setPen(QPen(Qt::black, 2));
            }
            else if (name == QLatin1String("rect"))
            {
                auto* rect = new QCPItemRect(&plot);
                rect->topLeft->setCoords(1.0, 0.8);
                rect->bottomRight->setCoords(4.5, 0.2);
                rect->setPen(QPen(QColor(200, 120, 20), 2));
                rect->setBrush(QColor(200, 120, 20, 60));
            }
            else if (name == QLatin1String("ellipse"))
            {
                auto* ellipse = new QCPItemEllipse(&plot);
                ellipse->topLeft->setCoords(1.5, 0.85);
                ellipse->bottomRight->setCoords(4.5, 0.15);
                ellipse->setPen(QPen(QColor(120, 60, 180), 2));
                ellipse->setBrush(QColor(120, 60, 180, 60));
            }
            else if (name == QLatin1String("pixmap") || name == QLatin1String("pixmap-scaled"))
            {
                auto* pixmap = new QCPItemPixmap(&plot);
                pixmap->setPixmap(testPixmap());
                pixmap->topLeft->setCoords(1.0, 0.8);
                pixmap->bottomRight->setCoords(4.5, 0.2);
                pixmap->setScaled(name.endsWith(QLatin1String("scaled")), Qt::KeepAspectRatio,
                                  Qt::SmoothTransformation);
            }
            else if (name == QLatin1String("tracer"))
            {
                auto* graph = new QCPGraph(plot.xAxis, plot.yAxis);
                QVector<double> keys, values;
                for (int i = 0; i <= 60; ++i)
                {
                    keys << i * 0.1;
                    values << 0.5 + 0.35 * qSin(i * 0.1);
                }
                graph->setData(keys, values);
                auto* tracer = new QCPItemTracer(&plot);
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
                auto* span = new QCPItemSpan(&plot, Qt::Horizontal);
                span->setRange(QCPRange(0.3, 0.6));
                span->setBrush(QColor(40, 90, 200, 50));
                span->setBorderPen(QPen(QColor(40, 90, 200), 2));
            }
            else if (name == QLatin1String("v"))
            {
                auto* span = new QCPItemSpan(&plot, Qt::Vertical);
                span->setRange(QCPRange(2.0, 4.0));
                span->setBrush(QColor(200, 60, 40, 50));
                span->setBorderPen(QPen(QColor(200, 60, 40), 2));
            }
            else if (name == QLatin1String("r"))
            {
                auto* span = new QCPItemRSpan(&plot);
                span->leftEdge->setCoords(1.5, 0);
                span->rightEdge->setCoords(4.5, 1);
                span->topEdge->setCoords(0, 0.8);
                span->bottomEdge->setCoords(6, 0.3);
                span->setBrush(QColor(30, 150, 90, 50));
                span->setBorderPen(QPen(QColor(30, 150, 90), 2));
            }
            else if (name == QLatin1String("selected"))
            {
                auto* span = new QCPItemSpan(&plot, Qt::Vertical);
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
                    auto* graph = new QCPPolarGraph(angular, radial);
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
                auto* rose = new QCPPolarGraph(angular, radial);
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
                auto* plain = new QCPPolarGraph(angular, radial);
                plain->setData(angle, uniform);
                plain->setPen(QPen(QColor(31, 119, 180), 1.5));
                auto* tapered = new QCPPolarGraph(angular, radial);
                tapered->setData(angle, hann);
                tapered->setPen(QPen(QColor(214, 39, 40), 2));
                plot.plotLayout()->insertRow(0);
                plot.plotLayout()->addElement(
                    0, 0,
                    new QCPTextElement(
                        &plot, QStringLiteral("16-element array: uniform (blue), Hann taper (red)"),
                        QFont(QStringLiteral("Helvetica"), 11)));
            }
            else if (name == QLatin1String("polar-log"))
            {
                QCPPolarArea* area = polarArea(plot);
                QCPPolarAxisAngular* angular = area->angularAxis();
                QCPPolarAxisRadial* radial = area->radialAxis();
                angular->setRange(0, 360);
                radial->setRange(1, 1000);
                radial->setScaleType(QCPPolarAxisRadial::stLogarithmic);
                auto ticker = QSharedPointer<QCPAxisTickerLog>::create();
                ticker->setLogBase(10);
                radial->setTicker(ticker);
                QVector<double> angle(361), r(361);
                for (int i = 0; i <= 360; ++i)
                {
                    angle[i] = i;
                    r[i] = std::pow(10.0, 1.5 + 1.4 * std::sin(i * std::numbers::pi / 90));
                }
                auto* graph = new QCPPolarGraph(angular, radial);
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
                auto* graph = new QCPPolarGraph(angular, radial);
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
                auto* graph = new QCPPolarGraph(angular, radial);
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
                auto* graph = new QCPPolarGraph(angular, radial);
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
                auto* graph = new QCPPolarGraph(angular, radial);
                graph->setData(angle, r);
                graph->setPen(QPen(QColor(214, 39, 40), 2));
            }
        });
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
            const auto clip = [&](QCPAbstractItem* item)
            {
                item->setClipArea(area);
                item->setClipToAxisRect(true);
            };

            auto* line = new QCPItemLine(&plot);
            place(line->start, 150.0, 0.25);
            place(line->end, 300.0, 0.9);
            line->setPen(QPen(QColor(31, 119, 180), 2));
            clip(line);

            auto* ellipse = new QCPItemEllipse(&plot);
            ratio(ellipse->topLeft, 0.10, 0.10);
            ratio(ellipse->bottomRight, 0.55, 1.25); // past the rim: the disc cuts the bottom
            ellipse->setPen(QPen(QColor(44, 160, 44), 2));
            ellipse->setBrush(QBrush(QColor(44, 160, 44, 60)));
            clip(ellipse);

            // A span-like band across the lower half, in area ratios: its corners land outside the
            // disc, so the clip cuts it with the arc instead of the area rect.
            auto* band = new QCPItemRect(&plot);
            ratio(band->topLeft, -0.10, 0.85);
            ratio(band->bottomRight, 1.10, 1.25); // a corner past the rim: cut by the arc
            band->setPen(Qt::NoPen);
            band->setBrush(QBrush(QColor(255, 127, 14, 120)));
            clip(band);

            auto* text = new QCPItemText(&plot);
            text->setText(QStringLiteral("text"));
            text->setColor(QColor(214, 39, 40));
            place(text->position, 60.0, 0.75);
            clip(text);
        });
}

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

void TestReferenceM4e::polarItems()
{
    check(polarItemsScene(), "polar-items.png");
}
