#include "test-builder.h"
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
