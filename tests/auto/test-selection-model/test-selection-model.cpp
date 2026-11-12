#include "test-selection-model.h"
#include "../misuse-recorder.h"
#include "polar/angularaxis.h"
#include "polar/polar-area.h"
#include "polar/polargraph.h"
#include "polar/radialaxis.h"
#include <QSignalSpy>
#include <functional>

namespace
{
// What one selectable object exposes to the data-driven tests, so every row is exercised by the
// same assertions instead of nine near-identical bodies.
struct Target
{
    QObject* object = nullptr;
    std::function<bool()> selected;
    std::function<void()> select;
    std::function<QCP::SelectionType()> selectable;
    std::function<void(QCP::SelectionType)> setSelectable;
};

QStringList targetNames()
{
    return { QStringLiteral("plottable"),      QStringLiteral("item"),
             QStringLiteral("axis"),           QStringLiteral("legend"),
             QStringLiteral("legendItem"),     QStringLiteral("textElement"),
             QStringLiteral("polarGraph"),     QStringLiteral("polarAxisAngular"),
             QStringLiteral("polarAxisRadial") };
}

void addTargetRows()
{
    QTest::addColumn<QString>("name");
    for (const QString& name : targetNames())
        QTest::newRow(qPrintable(name)) << name;
}

// The item, legend-row and text-element rows: bool state, stNone or stWhole.
template <typename T>
Target wholeObjectTarget(T* object)
{
    return { object, [object] { return object->selected(); },
             [object] { object->setSelected(true); }, [object] { return object->selectable(); },
             [object](QCP::SelectionType type) { object->setSelectable(type); } };
}

// The axis and legend rows: parts underneath, one whole above.
template <typename T>
Target partsTarget(T* object, typename T::SelectableParts part)
{
    return {
        object, [object] { return object->selected(); },
        [object, part] { object->setSelectedParts(part); },
        [object] { return object->selectable(); }, [object, part](QCP::SelectionType type)
        { object->setSelectableParts(type == QCP::stNone ? typename T::SelectableParts() : part); }
    };
}

QCPGraph* addGraph(QCustomPlot* plot)
{
    QCPGraph* graph = plot->addGraph();
    graph->setData(QVector<double> { 0, 5, 10 }, QVector<double> { 0, 5, 10 });
    return graph;
}

Target targetFor(QCustomPlot* plot, const QString& name)
{
    if (name == QLatin1String("plottable"))
    {
        QCPGraph* graph = addGraph(plot);
        return { graph, [graph] { return graph->selected(); },
                 [graph] { graph->setSelection(QCPDataSelection(QCPDataRange(0, 2))); },
                 [graph] { return graph->selectable(); },
                 [graph](QCP::SelectionType type) { graph->setSelectable(type); } };
    }
    if (name == QLatin1String("item"))
        return wholeObjectTarget(new QCPItemLine(plot));
    if (name == QLatin1String("axis"))
        return partsTarget(plot->xAxis.get(), QCPAxis::spAxis);
    if (name == QLatin1String("legend"))
        return partsTarget(plot->legend.get(), QCPLegend::spLegendBox);
    if (name == QLatin1String("legendItem"))
    {
        addGraph(plot);
        return wholeObjectTarget(plot->legend->item(0));
    }
    if (name == QLatin1String("textElement"))
        return wholeObjectTarget(new QCPTextElement(plot, QStringLiteral("title")));
    if (name == QLatin1String("polarGraph"))
    {
        QCPPolarArea* area = plot->add<QCPPolarArea>(1, 0).get();
        area->radialAxis(0)->setRange(0, 10);
        QCPPolarGraph* graph
            = area->add<QCPPolarGraph>(QVector<double> { 0, 90, 180 }, QVector<double> { 1, 2, 3 })
                  .get();
        return { graph, [graph] { return graph->selected(); },
                 [graph] { graph->setSelection(QCPDataSelection(QCPDataRange(0, 2))); },
                 [graph] { return graph->selectable(); },
                 [graph](QCP::SelectionType type) { graph->setSelectable(type); } };
    }
    if (name == QLatin1String("polarAxisAngular"))
    {
        QCPPolarArea* area = plot->add<QCPPolarArea>(1, 0).get();
        return partsTarget(area->angularAxis(), QCPPolarAxisAngular::spAxis);
    }
    Q_ASSERT(name == QLatin1String("polarAxisRadial"));
    QCPPolarArea* area = plot->add<QCPPolarArea>(1, 0).get();
    return partsTarget(area->radialAxis(0), QCPPolarAxisRadial::spAxis);
}

QPoint pixelOf(QCustomPlot* plot, double key, double value)
{
    return QPoint(qRound(plot->xAxis->coordToPixel(key)), qRound(plot->yAxis->coordToPixel(value)));
}

void clickAt(QCustomPlot* plot, const QPoint& pos)
{
    QMouseEvent press(QEvent::MouseButtonPress, pos, plot->mapToGlobal(pos), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(plot, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, pos, plot->mapToGlobal(pos), Qt::LeftButton,
                        Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(plot, &release);
}
} // namespace

void TestSelectionModel::init()
{
    test_misuse::calls.clear();
    qcp::setMisuseHandler(test_misuse::record);
    mPlot = new QCustomPlot;
    mPlot->resize(400, 300);
    mPlot->show();
    mPlot->replot();
}

void TestSelectionModel::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
    qcp::setMisuseHandler(nullptr);
}

void TestSelectionModel::everySelectableHasOneShape_data()
{
    addTargetRows();
}

void TestSelectionModel::everySelectableHasOneShape()
{
    QFETCH(QString, name);
    const Target target = targetFor(mPlot, name);
    QVERIFY(target.object);
    QVERIFY2(!target.selected(), qPrintable(name + QStringLiteral(" starts unselected")));

    QSignalSpy spy(target.object, SIGNAL(selectionChanged()));
    QVERIFY2(spy.isValid(), qPrintable(name + QStringLiteral(" has one selectionChanged()")));
    target.select();
    QVERIFY(target.selected());
    QCOMPARE(spy.count(), 1);

    target.select();
    QVERIFY(target.selected());
    QCOMPARE(spy.count(), 1);
}

void TestSelectionModel::selectableChangeIsOneSignal_data()
{
    addTargetRows();
}

void TestSelectionModel::selectableChangeIsOneSignal()
{
    QFETCH(QString, name);
    const Target target = targetFor(mPlot, name);
    QVERIFY(target.object);

    QSignalSpy spy(target.object, SIGNAL(selectableChanged()));
    QVERIFY2(spy.isValid(), qPrintable(name + QStringLiteral(" has one selectableChanged()")));
    const bool initiallyOn = target.selectable() != QCP::stNone;

    target.setSelectable(QCP::stNone);
    QCOMPARE(spy.count(), initiallyOn ? 1 : 0);
    QCOMPARE(target.selectable(), QCP::stNone);

    target.setSelectable(QCP::stNone);
    QCOMPARE(spy.count(), initiallyOn ? 1 : 0);

    target.setSelectable(QCP::stWhole);
    QCOMPARE(spy.count(), initiallyOn ? 2 : 1);
    QCOMPARE(target.selectable(), QCP::stWhole);
}

void TestSelectionModel::itemRefusesAPartialSelectionType()
{
    auto* item = new QCPItemLine(mPlot);
    QCOMPARE(item->selectable(), QCP::stWhole);
    QSignalSpy spy(item, SIGNAL(selectableChanged()));

    item->setSelectable(QCP::stDataRange);

    QCOMPARE(item->selectable(), QCP::stWhole);
    QCOMPARE(spy.count(), 0);
    QCOMPARE(test_misuse::calls.size(), size_t(1));
}

void TestSelectionModel::userSelectionStillAnnounced()
{
    QCPGraph* graph = addGraph(mPlot);
    graph->setData(QVector<double> { 0, 5, 10 }, QVector<double> { 5, 5, 5 });
    mPlot->xAxis->setRange(0, 10);
    mPlot->yAxis->setRange(0, 10);
    mPlot->setInteractions(QCP::iSelectPlottables);
    mPlot->replot();

    QStringList order;
    connect(graph, &QCPAbstractPlottable::selectionChanged, mPlot,
            [&order] { order << QStringLiteral("graph"); });
    connect(mPlot, &QCustomPlot::selectionChangedByUser, mPlot,
            [&order] { order << QStringLiteral("user"); });

    clickAt(mPlot, pixelOf(mPlot, 5, 5));

    QVERIFY2(graph->selected(), "the click selects the graph");
    QCOMPARE(order, (QStringList { QStringLiteral("graph"), QStringLiteral("user") }));
}
