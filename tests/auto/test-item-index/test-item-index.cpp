#include "test-item-index.h"

#include <items/item-bracket.h>
#include <items/item-curve.h>
#include <items/item-ellipse.h>
#include <items/item-line.h>
#include <items/item-rect.h>

#include <algorithm>
#include <random>
#include <vector>

namespace
{
QCPAbstractItem* makeItem(QCustomPlot* plot, int kind)
{
    switch (kind % 5)
    {
        case 0:
            return new QCPItemLine(plot);
        case 1:
            return new QCPItemRect(plot);
        case 2:
            return new QCPItemEllipse(plot);
        case 3:
            return new QCPItemCurve(plot);
        default:
            return new QCPItemBracket(plot);
    }
}

void setPen(QCPAbstractItem* item, const QPen& pen)
{
    if (auto* line = qobject_cast<QCPItemLine*>(item))
        line->setPen(pen);
    else if (auto* rect = qobject_cast<QCPItemRect*>(item))
        rect->setPen(pen);
    else if (auto* ellipse = qobject_cast<QCPItemEllipse*>(item))
        ellipse->setPen(pen);
    else if (auto* curve = qobject_cast<QCPItemCurve*>(item))
        curve->setPen(pen);
    else if (auto* bracket = qobject_cast<QCPItemBracket*>(item))
        bracket->setPen(pen);
}

void placeRandomly(QCPAbstractItem* item, std::mt19937& rng, QCPAxis* keyAxis, QCPAxis* valueAxis)
{
    const auto key = [&] { return std::uniform_real_distribution<double>(-200.0, 200.0)(rng); };
    const auto value = [&] { return std::uniform_real_distribution<double>(-200.0, 200.0)(rng); };
    const auto ratio = [&] { return std::uniform_real_distribution<double>(-0.2, 1.2)(rng); };
    const auto pixels = [&] { return std::uniform_real_distribution<double>(-50.0, 450.0)(rng); };
    for (QCPItemPosition* position : item->positions())
    {
        switch (std::uniform_int_distribution<int>(0, 2)(rng))
        {
            case 0: // data coordinates, possibly on the secondary axes
                position->setType(QCPItemPosition::ptPlotCoords);
                position->setAxes(keyAxis, valueAxis);
                position->setCoords(key(), value());
                break;
            case 1: // a ratio of the axis rect: indexable on no axis
                position->setType(QCPItemPosition::ptAxisRectRatio);
                position->setCoords(ratio(), ratio());
                break;
            case 2: // absolute pixels: indexable on no axis
                position->setType(QCPItemPosition::ptAbsolute);
                position->setCoords(pixels(), pixels());
                break;
            default: // data coordinates on the default axes
                position->setType(QCPItemPosition::ptPlotCoords);
                position->setAxes(nullptr, nullptr);
                position->setCoords(key(), value());
                break;
        }
    }
}

bool sameList(const QList<QCPLayerable*>& a, const QList<QCPLayerable*>& b)
{
    return a == b;
}
} // namespace

void TestItemIndex::randomItemsMatchUnindexed()
{
    std::mt19937 rng(1234);
    for (int round = 0; round < 10; ++round)
    {
        QCustomPlot plot;
        plot.resize(400, 300);
        const bool logarithmic = round % 3 == 1;
        const bool reversed = round % 2 == 0;
        if (logarithmic)
        {
            plot.xAxis->setScaleType(QCPAxis::stLogarithmic);
            plot.yAxis->setScaleType(QCPAxis::stLogarithmic);
            plot.xAxis->setRange(0.5, 500);
            plot.yAxis->setRange(0.5, 500);
        }
        else
        {
            plot.xAxis->setRange(-180, 180);
            plot.yAxis->setRange(-180, 180);
        }
        plot.xAxis->setRangeReversed(reversed);
        plot.yAxis2->setRange(-5, 5);
        plot.xAxis2->setRange(0, 1);
        plot.xAxis2->setVisible(true);
        plot.yAxis2->setVisible(true);

        if (round % 4 == 2)
        {
            QVERIFY(plot.addLayer(QStringLiteral("items2")));
            plot.setCurrentLayer(QStringLiteral("items2"));
        }

        const int count = 120 + round * 20;
        std::vector<QCPAbstractItem*> items;
        items.reserve(std::size_t(count));
        for (int i = 0; i < count; ++i)
        {
            QCPAbstractItem* item = makeItem(&plot, i + round);
            QCPAxis* keyAxis = (i % 7 == 0) ? plot.xAxis2 : plot.xAxis;
            QCPAxis* valueAxis = (i % 7 == 0) ? plot.yAxis2 : plot.yAxis;
            placeRandomly(item, rng, keyAxis, valueAxis);
            if (i % 11 == 0)
                setPen(item, QPen(Qt::darkGreen, 1 + (i % 5)));
            if (i % 13 == 0)
                item->setSelected(true);
            items.push_back(item);
        }
        if (round % 4 == 2)
            plot.setCurrentLayer(QStringLiteral("main"));

        plot.replot();
        QCoreApplication::processEvents();

        // Random mutations through the public API, each followed by a frame.
        std::uniform_int_distribution<int> pick(0, count - 1);
        for (int step = 0; step < 40; ++step)
        {
            QCPAbstractItem* item = items[std::size_t(pick(rng))];
            placeRandomly(item, rng, plot.xAxis, plot.yAxis);
            if (step % 5 == 0)
            {
                plot.replot();
                QCoreApplication::processEvents();
            }
        }
        plot.replot();
        QCoreApplication::processEvents();

        const QImage indexed = plot.toPixmap().toImage();
        plot.setItemIndexing(false);
        const QImage unindexed = plot.toPixmap().toImage();
        QCOMPARE(indexed, unindexed);
        plot.setItemIndexing(true);

        // Hit testing must answer exactly what the unindexed path answers.
        for (int probe = 0; probe < 40; ++probe)
        {
            const QPointF pos(std::uniform_real_distribution<double>(-10.0, 410.0)(rng),
                              std::uniform_real_distribution<double>(-10.0, 310.0)(rng));
            const QList<QCPLayerable*> withIndex = plot.layerableListAt(pos, true);
            plot.setItemIndexing(false);
            const QList<QCPLayerable*> withoutIndex = plot.layerableListAt(pos, true);
            plot.setItemIndexing(true);
            QVERIFY2(sameList(withIndex, withoutIndex),
                     qPrintable(QStringLiteral("round %1 probe %2: %3 hits with the index, %4 "
                                               "without")
                                    .arg(round)
                                    .arg(probe)
                                    .arg(withIndex.size())
                                    .arg(withoutIndex.size())));
        }
    }
}

void TestItemIndex::dragOneItemKeepsTheTree()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.xAxis->setRange(0, 100);
    plot.yAxis->setRange(0, 100);
    for (int i = 0; i < 1000; ++i)
    {
        auto* line = new QCPItemLine(&plot);
        const double x = 0.1 * i;
        line->start->setCoords(x, x);
        line->end->setCoords(x + 0.5, x + 0.5);
    }
    plot.replot();
    QCoreApplication::processEvents();

    QCPAbstractItem* dragged = nullptr;
    for (QCPLayerable* child : plot.layer(QStringLiteral("main"))->children())
        if (auto* item = qobject_cast<QCPAbstractItem*>(child))
        {
            dragged = item;
            break;
        }
    QVERIFY(dragged);
    QCPLayer* layer = dragged->layer();
    QVERIFY(layer);
    const int builds = layer->mItemIndexBuilds;
    QVERIFY(builds >= 1);

    for (int step = 0; step < 50; ++step)
    {
        // Every move goes through the side list: a rebuild here would defeat the point.
        auto* line = qobject_cast<QCPItemLine*>(dragged);
        QVERIFY(line);
        line->start->setCoords(step * 2.0, 10);
        line->end->setCoords(step * 2.0 + 1, 20);
        plot.replot(QCustomPlot::rpImmediateRefresh);
    }
    QCOMPARE(layer->mItemIndexBuilds, builds);
}
