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
        switch (std::uniform_int_distribution<int>(0, 3)(rng))
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
            default: // data coordinates on the plot's default axes
                position->setType(QCPItemPosition::ptPlotCoords);
                position->setAxes(item->parentPlot()->xAxis, item->parentPlot()->yAxis);
                position->setCoords(key(), value());
                break;
        }
    }
}

// Counts how often the index lets a hit test through to the item.
class CountingLine : public QCPItemLine
{
public:
    explicit CountingLine(QCustomPlot* plot) : QCPItemLine(plot) {}
    double selectTest(const QPointF& pos, bool onlySelectable, QVariant* details = nullptr)
        const override
    {
        ++mSelectCalls;
        return QCPItemLine::selectTest(pos, onlySelectable, details);
    }
    mutable int mSelectCalls = 0;
};

class DeselectCountingLine : public QCPItemLine
{
public:
    explicit DeselectCountingLine(QCustomPlot* plot) : QCPItemLine(plot) {}
    int mDeselectCalls = 0;

protected:
    void deselectEvent(bool* selectionStateChanged) override
    {
        ++mDeselectCalls;
        QCPItemLine::deselectEvent(selectionStateChanged);
    }
};

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

        QVERIFY(plot.addLayer(QStringLiteral("items2")));
        if (round % 4 == 2)
            plot.setCurrentLayer(QStringLiteral("items2"));

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
        plot.setCurrentLayer(QStringLiteral("main"));

        plot.replot();
        QCoreApplication::processEvents();

        // Random mutations through the public API, each followed by a frame: moves, pen changes
        // (they grow the pad), a bracket length, a line ending, a layer move, a delete, a take and
        // re-add, and a scale type flip every few steps.
        for (int step = 0; step < 60; ++step)
        {
            if (step % 10 == 5)
            {
                if (plot.xAxis->scaleType() == QCPAxis::stLinear)
                {
                    plot.xAxis->setRange(1, 100);
                    plot.xAxis->setScaleType(QCPAxis::stLogarithmic);
                }
                else
                {
                    plot.xAxis->setScaleType(QCPAxis::stLinear);
                }
            }
            if (items.empty())
                break;
            std::uniform_int_distribution<int> pick(0, int(items.size()) - 1);
            const auto index = std::size_t(pick(rng));
            QCPAbstractItem* item = items[index];
            const int kind = std::uniform_int_distribution<int>(0, 5)(rng);
            switch (kind)
            {
                case 0:
                    placeRandomly(item, rng, plot.xAxis, plot.yAxis);
                    break;
                case 1:
                    setPen(item, QPen(QColor(20 * (step % 12), 90, 160),
                                     1 + std::uniform_int_distribution<int>(0, 12)(rng)));
                    break;
                case 2:
                    if (auto* bracket = qobject_cast<QCPItemBracket*>(item))
                        bracket->setLength(
                            std::uniform_real_distribution<double>(10.0, 400.0)(rng));
                    if (auto* line = qobject_cast<QCPItemLine*>(item))
                    {
                        line->setHead(
                            QCPLineEnding(QCPLineEnding::esSpikeArrow, 20.0, 10.0));
                        line->setTail(QCPLineEnding(QCPLineEnding::esDisc, 30.0, 10.0));
                    }
                    break;
                case 3:
                    item->setLayer(plot.layer(step % 2 ? QStringLiteral("items2")
                                                      : QStringLiteral("main")));
                    break;
                case 4: // plain delete: the item unregisters itself (removeItem would delete it)
                    delete item;
                    items.erase(items.begin() + std::ptrdiff_t(index));
                    break;
                default: { // take through the owner and hand it back
                    auto handle = qcp::makeHandle<QCPHandle<QCPAbstractItem>>(item);
                    std::unique_ptr<QCPAbstractItem> taken = plot.take(handle);
                    QVERIFY(taken);
                    auto back = plot.add(std::move(taken));
                    QVERIFY(back);
                    QCOMPARE(back.get(), item);
                    break;
                }
            }
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

    QCPAbstractItem* dragged = plot.item(0);
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

void TestItemIndex::clickTestsOnlyItemsNearTheClick()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.xAxis->setRange(0, 10);
    plot.yAxis->setRange(0, 10);
    auto* nearItem = new CountingLine(&plot);
    nearItem->setPen(QPen(Qt::black, 4));
    nearItem->start->setCoords(2.0, 5.0);
    nearItem->end->setCoords(2.4, 5.0);
    auto* farItem = new CountingLine(&plot);
    farItem->setPen(QPen(Qt::black, 4));
    farItem->start->setCoords(8.0, 5.0);
    farItem->end->setCoords(8.4, 5.0);
    plot.replot();
    QCoreApplication::processEvents();

    const QPointF click(plot.xAxis->coordToPixel(2.2), plot.yAxis->coordToPixel(5.0));
    const QList<QCPLayerable*> hits = plot.layerableListAt(click, true);
    QVERIFY(hits.contains(nearItem));
    QVERIFY2(farItem->mSelectCalls == 0,
             qPrintable(QStringLiteral("an item 6 units away was hit-tested %1 times")
                            .arg(farItem->mSelectCalls)));
}

void TestItemIndex::scaleTypeChangeRebuildsTheIndex()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.xAxis->setRange(1, 100);
    plot.yAxis->setRange(0, 100);

    // A bracket left of a linear view whose 300 px arms reach into it. On a log axis the same
    // coordinate is drawn 200 px left of the axis rect, so its extent changes meaning: a tree
    // built for the linear axis must not answer after the switch.
    auto* bracket = new QCPItemBracket(&plot);
    bracket->setPen(QPen(Qt::darkGreen, 3));
    bracket->setLength(300);
    bracket->left->setCoords(-1, 20);
    bracket->right->setCoords(-1, 80);
    auto* inside = new QCPItemLine(&plot);
    inside->setPen(QPen(Qt::black, 3));
    inside->start->setCoords(10, 10);
    inside->end->setCoords(90, 90);

    plot.replot();
    QCoreApplication::processEvents();
    QCPLayer* layer = bracket->layer();
    QVERIFY(layer);
    const int builds = layer->mItemIndexBuilds;
    QVERIFY(builds >= 1);

    plot.xAxis->setScaleType(QCPAxis::stLogarithmic);
    plot.xAxis->setRange(1, 100);
    plot.replot();
    QCoreApplication::processEvents();

    // The extent of an item depends on the scale type (a non-positive coordinate is unbounded on a
    // log axis), so the tree has to be rebuilt, not reused.
    QVERIFY2(layer->mItemIndexBuilds > builds,
             "the index built for the linear axis survived the switch to log");

    // And the frame must be the one a plot built on a log axis from the start draws.
    QCustomPlot fresh;
    fresh.resize(400, 300);
    fresh.xAxis->setScaleType(QCPAxis::stLogarithmic);
    fresh.xAxis->setRange(1, 100);
    fresh.yAxis->setRange(0, 100);
    auto* freshBracket = new QCPItemBracket(&fresh);
    freshBracket->setPen(QPen(Qt::darkGreen, 3));
    freshBracket->setLength(300);
    freshBracket->left->setCoords(-1, 20);
    freshBracket->right->setCoords(-1, 80);
    auto* freshInside = new QCPItemLine(&fresh);
    freshInside->setPen(QPen(Qt::black, 3));
    freshInside->start->setCoords(10, 10);
    freshInside->end->setCoords(90, 90);
    fresh.replot();
    QCoreApplication::processEvents();

    QCOMPARE(plot.toPixmap().toImage(), fresh.toPixmap().toImage());
}

void TestItemIndex::emptyClickVisitsOnlySelectedItems()
{
    // A click deselects everything else; with thousands of items, visiting the ones that are not
    // selected was most of the click.
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.xAxis->setRange(0, 100);
    plot.yAxis->setRange(0, 100);
    plot.setInteractions(QCP::iSelectItems);
    std::vector<DeselectCountingLine*> lines;
    for (int i = 0; i < 50; ++i)
    {
        auto* line = new DeselectCountingLine(&plot);
        line->start->setCoords(i, 10);
        line->end->setCoords(i + 0.5, 20);
        line->setSelected(i % 10 == 0);
        lines.push_back(line);
    }
    plot.replot();
    const QPointF empty(plot.xAxis->coordToPixel(50), plot.yAxis->coordToPixel(80));
    const QPointF global = plot.mapToGlobal(empty);
    QMouseEvent press(QEvent::MouseButtonPress, empty, global, Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(&plot, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, empty, global, Qt::LeftButton, Qt::NoButton,
                        Qt::NoModifier);
    QCoreApplication::sendEvent(&plot, &release);

    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        QVERIFY(!lines[i]->selected());
        QCOMPARE(lines[i]->mDeselectCalls, i % 10 == 0 ? 1 : 0);
    }

    plot.deselectAll(); // nothing is selected any more: nobody is visited
    for (std::size_t i = 0; i < lines.size(); ++i)
        QCOMPARE(lines[i]->mDeselectCalls, i % 10 == 0 ? 1 : 0);
}
