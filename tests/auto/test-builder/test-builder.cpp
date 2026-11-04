#include "test-builder.h"
#include <functional>
#include <memory>
#include <type_traits>

namespace
{
int gMisuses = 0;
void record(const char*, const char*) { ++gMisuses; }

const QVector<double> kKeys{1, 2, 3};
const QVector<double> kValues{4, 5, 6};

void settle(QCustomPlot& plot)
{
    plot.resize(400, 300);
    plot.replot();
    for (int i = 0; i < 10 && plot.isRepaintQueued(); ++i)
        QCoreApplication::processEvents();
}

// A leaf subclass: its Handle adds one setter.
class LeafGraph : public QCPGraph
{
public:
    using QCPGraph::QCPGraph;
    int tag = 0;

    class Handle : public QCPGraphOps<LeafGraph, Handle>
    {
    public:
        Handle setTag(int t)
        {
            return apply([t](LeafGraph& g) { g.tag = t; });
        }
    };
};

// A subclass meant to be subclassed again: its setters are written once, for any final type.
class MyGraph : public QCPGraph
{
public:
    using QCPGraph::QCPGraph;
    int tag = 0;

    template <typename T, typename Self = void>
    class Ops : public QCPGraphOps<T, qcp::OpsRet<Self, Ops<T>>>
    {
    public:
        auto setTag(int t)
        {
            return this->apply([t](T& g) { g.tag = t; });
        }
    };
    template <typename T>
    using OpsFor = Ops<T>;
};

class Deeper : public MyGraph
{
public:
    using MyGraph::MyGraph;
    int depth = 0;

    class Handle : public MyGraph::Ops<Deeper, Handle>
    {
    public:
        Handle setDepth(int d)
        {
            return apply([d](Deeper& g) { g.depth = d; });
        }
    };
};

class PlainSub : public QCPGraph
{
public:
    using QCPGraph::QCPGraph;
};

class LeafSub : public LeafGraph
{
public:
    using LeafGraph::LeafGraph;
};
} // namespace

void TestBuilder::init()
{
    gMisuses = 0;
    qcp::setMisuseHandler(record);
}

void TestBuilder::cleanup() { qcp::setMisuseHandler(nullptr); }

void TestBuilder::addGraphWithData()
{
    QCustomPlot plot;
    auto g = plot.add<QCPGraph>(QVector<double>{1, 2, 3}, QVector<double>{4, 5, 6})
                 .setName("Bx")
                 .setPen(QPen(Qt::red));
    QVERIFY(g);
    QCOMPARE(g->name(), QString("Bx"));
    QCOMPARE(g->pen().color(), QColor(Qt::red));
    QCOMPARE(g->dataCount(), 3);
    QCOMPARE(plot.graphCount(), 1);
    QCOMPARE(g->keyAxis(), plot.xAxis);
    QCOMPARE(g->valueAxis(), plot.yAxis);
    QCOMPARE(g->layer(), plot.currentLayer());
    QCOMPARE(gMisuses, 0);
}

void TestBuilder::addWithoutData()
{
    QCustomPlot plot;
    auto g = plot.add<QCPGraph>();
    QVERIFY(g);
    QCOMPARE(g->dataCount(), 0);
    QCOMPARE(plot.graphCount(), 1);
}

void TestBuilder::handleIsTheHandle()
{
    QCustomPlot plot;
    static_assert(std::is_base_of_v<QCPHandle<QCPGraph>, decltype(plot.add<QCPGraph>())>);
    auto g = plot.add<QCPGraph>();
    static_assert(!std::is_reference_v<decltype(g.setName(""))>);
    static_assert(std::is_same_v<decltype(g.setName("")), QCPGraphOps<QCPGraph>>);
    static_assert(!std::is_convertible_v<decltype(g), QCPGraph*>);
    QVERIFY(g.setName("a") == g);
}

void TestBuilder::oneChainOneRepaint()
{
    QCustomPlot plot;
    settle(plot);
    QSignalSpy after(&plot, &QCustomPlot::afterReplot);
    auto g = plot.add<QCPGraph>(kKeys, kValues)
                 .setName("a")
                 .setPen(QPen(Qt::red))
                 .setBrush(QBrush(Qt::blue))
                 .setLineStyle(QCPGraph::lsStepLeft)
                 .setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle));
    QVERIFY(plot.isRepaintQueued());
    QCOMPARE(after.count(), 0);
    QTRY_COMPARE(after.count(), 1);
    for (int i = 0; i < 3; ++i)
        QCoreApplication::processEvents();
    QCOMPARE(after.count(), 1);
}

void TestBuilder::deadHandleSettersAreNoOps()
{
    QCustomPlot plot;
    auto g = plot.add<QCPGraph>(kKeys, kValues);
    g.remove();
    g.setName("x").setPen(QPen()).setLineStyle(QCPGraph::lsNone).setData(kKeys, kValues);
    QVERIFY(!g);
    QCOMPARE(plot.graphCount(), 0);
    QCOMPARE(gMisuses, 0);
}

// QCPGraph has no data signal; visibleChanged is the setter signal a slot can react to mid-chain.
void TestBuilder::slotDeletesMidChain()
{
    QCustomPlot plot;
    settle(plot);
    auto g = plot.add<QCPGraph>(kKeys, kValues);
    connect(g.get(), &QCPLayerable::visibleChanged, &plot, [g]() mutable { g.remove(); });
    auto after = g.setVisible(false).setName("x").setPen(QPen());
    QVERIFY(!g);
    QVERIFY(!after);
    QCOMPARE(plot.graphCount(), 0);
    QCOMPARE(gMisuses, 0);
    QSignalSpy replots(&plot, &QCustomPlot::afterReplot);
    plot.replot();
    QCOMPARE(replots.count(), 1);
}

void TestBuilder::setAxesByRole()
{
    QCustomPlot plot;
    auto g = plot.add<QCPGraph>().setAxes(QCPAxis::atTop, QCPAxis::atRight);
    QCOMPARE(g->keyAxis(), plot.xAxis2);
    QCOMPARE(g->valueAxis(), plot.yAxis2);
    QCOMPARE(gMisuses, 0);

    QCustomPlot other;
    g.setAxes(other.xAxis, other.yAxis);
    QCOMPARE(gMisuses, 1);
    QCOMPARE(g->keyAxis(), plot.xAxis2);

    g.setAxes(plot.xAxis, plot.yAxis);
    QCOMPARE(g->keyAxis(), plot.xAxis);
    QCOMPARE(g->valueAxis(), plot.yAxis);
    g.setAxes(plot.yAxis, plot.yAxis2); // parallel axes
    QCOMPARE(gMisuses, 2);
    QCOMPARE(g->keyAxis(), plot.xAxis);

    QVERIFY(plot.axisRect()->removeAxis(plot.xAxis2));
    g.setAxes(QCPAxis::atTop, QCPAxis::atLeft);
    QCOMPARE(gMisuses, 3);
    QCOMPARE(g->keyAxis(), plot.xAxis);
    QCOMPARE(g->valueAxis(), plot.yAxis);
}

void TestBuilder::setAxisRectAndLayerAreChecked()
{
    QCustomPlot plot;
    auto* rect = new QCPAxisRect(&plot, false);
    rect->addAxes(QCPAxis::atBottom | QCPAxis::atLeft);
    plot.plotLayout()->addElement(0, 1, rect);

    auto g = plot.add<QCPGraph>().setAxisRect(rect);
    QCOMPARE(g->keyAxis(), rect->axis(QCPAxis::atBottom));
    QCOMPARE(g->valueAxis(), rect->axis(QCPAxis::atLeft));
    QCOMPARE(gMisuses, 0);

    g.setAxes(QCPAxis::atTop, QCPAxis::atRight);
    QCOMPARE(gMisuses, 1);
    QCOMPARE(g->keyAxis(), rect->axis(QCPAxis::atBottom));

    QCustomPlot other;
    g.setAxisRect(other.axisRect());
    QCOMPARE(gMisuses, 2);
    QCOMPARE(g->keyAxis(), rect->axis(QCPAxis::atBottom));

    QCPLayer* before = g->layer();
    g.setLayer("no such layer");
    QCOMPARE(gMisuses, 3);
    QCOMPARE(g->layer(), before);
    g.setLayer("grid");
    QCOMPARE(g->layer(), plot.layer("grid"));
    QCOMPARE(gMisuses, 3);
}

void TestBuilder::addUserSubclass()
{
    QCustomPlot plot;

    auto leaf = plot.add<LeafGraph>().setName("m").setTag(7);
    static_assert(std::is_same_v<decltype(leaf), LeafGraph::Handle>);
    QCOMPARE(leaf->name(), QString("m"));
    QCOMPARE(leaf->tag, 7);

    auto my = plot.add<MyGraph>(kKeys, kValues).setTag(3).setName("my");
    static_assert(std::is_base_of_v<QCPHandle<MyGraph>, decltype(my)>);
    QCOMPARE(my->tag, 3);
    QCOMPARE(my->dataCount(), 3);

    auto d = plot.add<Deeper>().setName("d").setTag(1).setDepth(2).setLineStyle(QCPGraph::lsNone);
    static_assert(std::is_same_v<decltype(d), Deeper::Handle>);
    QCOMPARE(d->name(), QString("d"));
    QCOMPARE(d->tag, 1);
    QCOMPARE(d->depth, 2);
    QCOMPARE(d->lineStyle(), QCPGraph::lsNone);

    auto plain = plot.add<PlainSub>().setLineStyle(QCPGraph::lsImpulse);
    static_assert(std::is_base_of_v<QCPHandle<PlainSub>, decltype(plain)>);
    QCOMPARE(plain->lineStyle(), QCPGraph::lsImpulse);

    // An inherited Handle wraps the base type, so it is not this type's handle.
    auto leafSub = plot.add<LeafSub>();
    static_assert(std::is_same_v<decltype(leafSub), QCPGraphOps<LeafSub>>);
    QVERIFY(leafSub);

    QCOMPARE(plot.graphCount(), 5);
    QCOMPARE(gMisuses, 0);
}

void TestBuilder::addRefusedDuringTeardown()
{
    auto* plot = new QCustomPlot;
    auto* graph = plot->addGraph();
    bool refused = false;
    connect(graph, &QObject::destroyed, [plot, &refused] { refused = !plot->add<QCPGraph>(); });

    delete plot;

    QVERIFY(refused);
    QCOMPARE(gMisuses, 1);
}

void TestBuilder::addRefusedWithoutAxisRect()
{
    QCustomPlot plot;
    plot.plotLayout()->clear();
    QVERIFY(!plot.add<QCPGraph>());
    QCOMPARE(gMisuses, 1);
    QCOMPARE(plot.graphCount(), 0);
}

void TestBuilder::addPutsOneLegendRow()
{
    QCustomPlot plot;
    const int rows = plot.legend->itemCount();
    auto g = plot.add<QCPGraph>(kKeys, kValues).setName("Bx");
    QCOMPARE(plot.legend->itemCount(), rows + 1);
    QVERIFY(plot.legend->itemWithPlottable(g.get()));
}

void TestBuilder::addGraphWithContainer()
{
    QCustomPlot plot;
    auto data = QSharedPointer<QCPGraphDataContainer>::create();
    data->add(QCPGraphData(1, 2));
    auto g = plot.add<QCPGraph>(data);
    QCOMPARE(g->dataCount(), 1);
    QCOMPARE(g->data(), data);
}

void TestBuilder::withReachesAnySetter()
{
    QCustomPlot plot;
    auto g = plot.add<QCPGraph>().with([](QCPGraph& graph) { graph.setScatterSkip(3); }).setName("w");
    static_assert(std::is_same_v<decltype(g), QCPGraphOps<QCPGraph>>);
    QCOMPARE(g->scatterSkip(), 3);
    QCOMPARE(g->name(), QString("w"));

    int calls = 0;
    g.remove();
    g.with([&calls](QCPGraph&) { ++calls; });
    QCOMPARE(calls, 0);
    QCOMPARE(gMisuses, 0);
}

// The setter's own body must survive its signal's slot deleting the object (it emits last).
void TestBuilder::setterSlotDeletesInsideTheSetter()
{
    QCustomPlot plot;
    auto g = plot.add<QCPGraph>(kKeys, kValues);
    connect(g.get(), qOverload<bool>(&QCPAbstractPlottable::selectionChanged), &plot,
            [g](bool) mutable { g.remove(); });
    auto after = g.with([](QCPGraph& graph) { graph.setSelection(QCPDataSelection(QCPDataRange(0, 1))); })
                     .setName("gone");
    QVERIFY(!g);
    QVERIFY(!after);
    QCOMPARE(plot.graphCount(), 0);
    QCOMPARE(gMisuses, 0);
}

using Check = std::function<bool(QCustomPlot&)>;
Q_DECLARE_METATYPE(Check)

void TestBuilder::addEveryPlottable_data()
{
    QTest::addColumn<Check>("add");
    const auto row = [](const char* name, Check check) { QTest::newRow(name) << check; };
    row("graph", [](QCustomPlot& p) { return p.add<QCPGraph>(kKeys, kValues).get()->dataCount() == 3; });
    row("curve",
        [](QCustomPlot& p)
        {
            auto c = p.add<QCPCurve>(kKeys, kValues).setLineStyle(QCPCurve::lsNone).setScatterSkip(2);
            return c->dataCount() == 3 && c->lineStyle() == QCPCurve::lsNone && c->scatterSkip() == 2;
        });
    row("curveWithParameter",
        [](QCustomPlot& p) { return p.add<QCPCurve>(kKeys, kKeys, kValues).get()->dataCount() == 3; });
    row("bars",
        [](QCustomPlot& p)
        {
            auto b = p.add<QCPBars>(kKeys, kValues).setWidth(0.5).setBaseValue(1);
            return b->dataCount() == 3 && b->width() == 0.5 && b->baseValue() == 1;
        });
    row("financial",
        [](QCustomPlot& p)
        {
            auto f = p.add<QCPFinancial>(kKeys, kValues, kValues, kValues, kValues).setTwoColored(true);
            return f->data()->size() == 3 && f->twoColored();
        });
    row("histogram2d",
        [](QCustomPlot& p)
        {
            auto h = p.add<QCPHistogram2D>(std::vector<double>{1, 2, 3}, std::vector<double>{4, 5, 6})
                         .setBins(10, 20);
            return h->keyBins() == 10 && h->valueBins() == 20 && h->dataSource();
        });
    row("colormap",
        [](QCustomPlot& p)
        {
            auto data = std::make_unique<QCPColorMapData>(2, 2, QCPRange(0, 1), QCPRange(0, 1));
            auto m = p.add<QCPColorMap>().setData(std::move(data)).setInterpolate(false);
            return m->data() && m->data()->keySize() == 2 && !m->interpolate();
        });
    row("colormap2",
        [](QCustomPlot& p)
        {
            auto m = p.add<QCPColorMap2>(std::vector<double>{1, 2}, std::vector<double>{1, 2},
                                         std::vector<double>{1, 2, 3, 4})
                         .setGapThreshold(0.5);
            return m->dataSource() && m->gapThreshold() == 0.5;
        });
    row("intervals",
        [](QCustomPlot& p)
        {
            qcp::intervals::Columns columns;
            columns.start = {0.0};
            columns.stop = {1.0};
            columns.lane = {0};
            columns.category = {0};
            columns.ids = {1};
            auto i = p.add<QCPIntervals>(columns);
            return i->rowCount() == 1;
        });
}

void TestBuilder::addEveryPlottable()
{
    QFETCH(Check, add);
    QCustomPlot plot;
    QVERIFY(add(plot));
    QCOMPARE(plot.plottableCount(), 1);
    QCOMPARE(plot.plottable()->keyAxis(), plot.xAxis);
    QCOMPARE(plot.plottable()->layer(), plot.currentLayer());
    QCOMPARE(gMisuses, 0);
}

void TestBuilder::addEveryItem_data()
{
    QTest::addColumn<Check>("add");
    const auto row = [](const char* name, Check check) { QTest::newRow(name) << check; };
    row("line",
        [](QCustomPlot& p)
        {
            auto line = p.add<QCPItemLine>()
                            .setPositionType(QCPItemPosition::ptPlotCoords)
                            .setStart(1, 2)
                            .setEnd(3, 4)
                            .setHead(QCPLineEnding(QCPLineEnding::esSpikeArrow))
                            .setPen(QPen(Qt::green))
                            .setClipToAxisRect(false)
                            .setVisible(false);
            return line->start->coords() == QPointF(1, 2) && line->end->coords() == QPointF(3, 4)
                && line->start->type() == QCPItemPosition::ptPlotCoords
                && line->head().style() == QCPLineEnding::esSpikeArrow
                && line->pen().color() == QColor(Qt::green) && !line->clipToAxisRect()
                && !line->visible();
        });
}

void TestBuilder::addEveryItem()
{
    QFETCH(Check, add);
    QCustomPlot plot;
    QVERIFY(add(plot));
    QCOMPARE(plot.itemCount(), 1);
    QCOMPARE(plot.item()->layer(), plot.currentLayer());
    QCOMPARE(plot.item()->parentPlot(), &plot);
    QCOMPARE(gMisuses, 0);
}

void TestBuilder::addAxisToRect()
{
    QCustomPlot plot;
    QCPAxisRect* rect = plot.axisRect();
    QCOMPARE(rect->axisCount(QCPAxis::atRight), 1);
    auto axis = rect->add<QCPAxis>(QCPAxis::atRight).setVisible(false);
    QCOMPARE(rect->axisCount(QCPAxis::atRight), 2);
    QVERIFY(axis);
    QCOMPARE(axis->axisType(), QCPAxis::atRight);
    QVERIFY(!axis->visible());
    QCOMPARE(gMisuses, 0);
}

void TestBuilder::addLayoutParts()
{
    QCustomPlot plot;
    auto title = plot.add<QCPTextElement>(1, 0, "Title").setText("Heading").setVisible(true);
    auto rect = plot.add<QCPAxisRect>(2, 0).setRangeZoomFactor(1.5);
    auto scale = plot.add<QCPColorScale>(2, 1).setLabel("B").setBarWidth(12);
    QCOMPARE(plot.plotLayout()->element(1, 0), title.get());
    QCOMPARE(plot.plotLayout()->element(2, 0), rect.get());
    QCOMPARE(plot.plotLayout()->element(2, 1), scale.get());
    QCOMPARE(title->text(), QString("Heading"));
    QCOMPARE(scale->label(), QString("B"));
    QCOMPARE(scale->barWidth(), 12);
    QCOMPARE(rect->parentPlot(), &plot);
    QCOMPARE(gMisuses, 0);
    settle(plot);
}

void TestBuilder::addToTakenCellIsRefused()
{
    QCustomPlot plot;
    QVERIFY(!plot.add<QCPTextElement>(0, 0, "on the axis rect"));
    QCOMPARE(gMisuses, 1);
    QCOMPARE(plot.plotLayout()->element(0, 0), plot.axisRect());
}

void TestBuilder::groupsOwnedByPlot()
{
    auto* plot = new QCustomPlot;
    auto bars = plot->add<QCPBarsGroup>();
    auto group = plot->add<QCPPlottableGroup>("g");
    QVERIFY(bars);
    QVERIFY(group);
    QCOMPARE(plot->plottableGroups().size(), 1);
    QCOMPARE(group->name(), QString("g"));
    delete plot;
    QVERIFY(!bars);
    QVERIFY(!group);
    QCOMPARE(gMisuses, 0);
}

void TestBuilder::groupHandlesSetAndAddMembers()
{
    QCustomPlot plot;
    auto a = plot.add<QCPGraph>(kKeys, kValues);
    auto b = plot.add<QCPGraph>(kKeys, kValues);
    auto group = plot.add<QCPPlottableGroup>("g").setName("renamed").addMember(a).addMember(b);
    QCOMPARE(group->name(), QString("renamed"));
    QCOMPARE(group->members().size(), 2);
    group.removeMember(a).setVisible(false);
    QCOMPARE(group->members().size(), 1);
    QVERIFY(!b->visible() || !group->visible());
    a.remove();
    group.addMember(a);
    QCOMPARE(gMisuses, 1);
}

void TestBuilder::barsHandleGroupsAndStacks()
{
    QCustomPlot plot;
    auto bars = plot.add<QCPBarsGroup>().setSpacing(7);
    QCOMPARE(bars->spacing(), 7.0);
    auto bar = plot.add<QCPBars>(kKeys, kValues).setBarsGroup(bars);
    QCOMPARE(bar->barsGroup(), bars.get());
    bars.remove();
    bar.setBarsGroup(bars);
    QCOMPARE(gMisuses, 1);
}

