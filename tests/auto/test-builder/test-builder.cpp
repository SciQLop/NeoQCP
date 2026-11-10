#include "test-builder.h"
#include "polar/angularaxis.h"
#include "polar/polar-area.h"
#include "polar/polargraph.h"
#include "polar/radialaxis.h"
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

int pixelsOfColor(QCustomPlot& plot, const QColor& color)
{
    const QImage image = plot.toPixmap(400, 300).toImage();
    int count = 0;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            count += image.pixelColor(x, y) == color;
    return count;
}

void showRedLine(QCustomPlot& plot, QCPGraph& graph)
{
    graph.setPen(QPen(Qt::red, 3));
    plot.xAxis->setRange(0, 4);
    plot.yAxis->setRange(0, 7);
    settle(plot);
}

class UserGraph : public QCPGraph
{
public:
    using QCPGraph::QCPGraph;
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
    connect(g.get(), &QCPAbstractPlottable::selectionChanged, &plot,
            [g]() mutable { g.remove(); });
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
            auto f = p.add<QCPFinancial>(kKeys, kValues, kValues, kValues, kValues).setTwoColored(false);
            return f->data()->size() == 3 && !f->twoColored();
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
    row("errorbar",
        [](QCustomPlot& p)
        {
            auto e = p.add<QCPErrorBars>().setWhiskerWidth(5);
            return e->whiskerWidth() == 5;
        });
    row("statisticalbox",
        [](QCustomPlot& p)
        {
            auto s = p.add<QCPStatisticalBox>().setWidth(0.4);
            s->setData(kKeys, kValues, kValues, kValues, kValues, kValues);
            return s->dataCount() == 3 && s->width() == 0.4;
        });
    row("multigraph",
        [](QCustomPlot& p)
        {
            auto m = p.add<QCPMultiGraph>(std::vector<double>{1, 2, 3},
                                          std::vector<std::vector<double>>{{4, 5, 6}})
                         .setComponentNames({"a"})
                         .setColorValues(std::vector<double>{1, 2, 3})
                         .setGradient(QCPColorGradient(QCPColorGradient::gpJet))
                         .setDataRange(QCPRange(1, 3))
                         .setDataScaleType(QCPAxis::stLogarithmic)
                         .setScatterSkip(3)
                         .setLineStyle(QCPMultiGraph::lsLine);
            return m->dataSource() && m->hasColorValues() && m->scatterSkip() == 3
                && m->dataScaleType() == QCPAxis::stLogarithmic && m->dataRange() == QCPRange(1, 3)
                && m->gradient() == QCPColorGradient(QCPColorGradient::gpJet);
        });
    row("waterfall",
        [](QCustomPlot& p)
        {
            auto w = p.add<QCPWaterfallGraph>(std::vector<double>{1, 2, 3},
                                              std::vector<std::vector<double>>{{4, 5, 6}})
                         .setGain(2.0)
                         .setScatterSkip(2);
            return w->dataSource() && w->gain() == 2.0 && w->scatterSkip() == 2;
        });
    row("histogram",
        [](QCustomPlot& p)
        {
            return p.add<QCPHistogram>().setBins(7)->bins() == 7;
        });
    row("graph2",
        [](QCustomPlot& p) { return p.add<QCPGraph2>(kKeys, kValues).get()->dataCount() == 3; });

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
    row("itemcurve",
        [](QCustomPlot& p)
        {
            auto i = p.add<QCPItemCurve>()
                         .setStart(1, 2);
            return i->start->coords() == QPointF(1, 2);
        });
    row("itembracket",
        [](QCustomPlot& p)
        {
            auto i = p.add<QCPItemBracket>()
                         .setLeft(1, 2);
            return i->left->coords() == QPointF(1, 2);
        });
    row("itemellipse",
        [](QCustomPlot& p)
        {
            auto i = p.add<QCPItemEllipse>()
                         .setTopLeft(1, 2);
            return i->topLeft->coords() == QPointF(1, 2);
        });
    row("itempixmap",
        [](QCustomPlot& p)
        {
            auto i = p.add<QCPItemPixmap>()
                         .setTopLeft(1, 2);
            return i->topLeft->coords() == QPointF(1, 2);
        });
    row("itemrect",
        [](QCustomPlot& p)
        {
            auto i = p.add<QCPItemRect>()
                         .setTopLeft(1, 2);
            return i->topLeft->coords() == QPointF(1, 2);
        });
    row("itemtext",
        [](QCustomPlot& p)
        {
            auto i = p.add<QCPItemText>()
                         .setPosition(1, 2);
            return i->position->coords() == QPointF(1, 2);
        });
    row("itemtracer",
        [](QCustomPlot& p)
        {
            auto i = p.add<QCPItemTracer>()
                         .setPosition(1, 2);
            return i->position->coords() == QPointF(1, 2);
        });
    row("itemstraightline",
        [](QCustomPlot& p)
        {
            auto i = p.add<QCPItemStraightLine>()
                         .setPoint1(1, 2);
            return i->point1->coords() == QPointF(1, 2);
        });
    row("itemvspan",
        [](QCustomPlot& p)
        {
            auto i = p.add<QCPItemSpan>(Qt::Vertical)
                         .setLowerEdge(1, 2);
            return i->lowerEdge->coords() == QPointF(1, 2);
        });
    row("itemhspan",
        [](QCustomPlot& p)
        {
            auto i = p.add<QCPItemSpan>(Qt::Horizontal)
                         .setLowerEdge(1, 2);
            return i->lowerEdge->coords() == QPointF(1, 2);
        });
    row("itemrspan",
        [](QCustomPlot& p)
        {
            auto i = p.add<QCPItemRSpan>()
                         .setLeftEdge(1, 2);
            return i->leftEdge->coords() == QPointF(1, 2);
        });
}

void TestBuilder::addEveryItem()
{
    QFETCH(Check, add);
    QCustomPlot plot;
    QVERIFY(add(plot));
    QCOMPARE(plot.itemCount(), 1);
    QCOMPARE(plot.item()->layer(), plot.layer(QStringLiteral("annotations")));
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

void TestBuilder::addOnAnArea()
{
    QCustomPlot plot;
    QCPAxisRect* rect = plot.add<QCPAxisRect>(1, 0).get();
    QCPGraph* graph = rect->add<QCPGraph>().setData(kKeys, kValues).get();
    QCOMPARE(graph->keyAxis(), rect->axis(QCPAxis::atBottom));
    QCOMPARE(graph->valueAxis(), rect->axis(QCPAxis::atLeft));
    QCOMPARE(graph->system(),
             rect->system(rect->axis(QCPAxis::atBottom), rect->axis(QCPAxis::atLeft)));
    QCOMPARE(plot.plottableCount(), 1);

    // An explicit pair of this rect (another value axis) is another system.
    QCPAxis* right = rect->add<QCPAxis>(QCPAxis::atRight).get();
    QCPGraph* second = rect->add<QCPGraph>(rect->axis(QCPAxis::atBottom), right).get();
    QCOMPARE(second->valueAxis(), right);
    QVERIFY(second->system() != graph->system());
    QCOMPARE(rect->plottables(), (QList<QCPAbstractPlottable*> { graph, second }));
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
    QVERIFY(!group->visible());
    QVERIFY(!b->visible());
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

void TestBuilder::takeAndReAdd()
{
    QCustomPlot a;
    QCustomPlot b;
    auto g = a.add<QCPGraph>(kKeys, kValues).setName("moved");
    auto group = a.add<QCPPlottableGroup>("pair").addMember(g);
    showRedLine(a, *g.get());
    QVERIFY(pixelsOfColor(a, Qt::red) > 20);

    std::unique_ptr<QCPGraph> taken = a.take(g);
    QVERIFY(taken);
    QVERIFY(!g || g.get() == taken.get());
    QCOMPARE(a.graphCount(), 0);
    QCOMPARE(a.plottableCount(), 0);
    QVERIFY(!a.legend->hasItemWithPlottable(taken.get()));
    QCOMPARE(group->members().size(), 0);
    QCOMPARE(taken->parent(), static_cast<QObject*>(nullptr));
    QCOMPARE(taken->keyAxis(), static_cast<QCPAxis*>(nullptr));
    QCOMPARE(taken->layer(), static_cast<QCPLayer*>(nullptr));
    settle(a);
    QCOMPARE(pixelsOfColor(a, Qt::red), 0);

    auto moved = b.add(std::move(taken));
    QCOMPARE(b.graphCount(), 1);
    QCOMPARE(moved->keyAxis(), b.xAxis);
    QCOMPARE(moved->layer(), b.currentLayer());
    QVERIFY(b.legend->hasItemWithPlottable(moved.get()));
    b.xAxis->setRange(0, 4);
    b.yAxis->setRange(0, 7);
    settle(b);
    QVERIFY(pixelsOfColor(b, Qt::red) > 20);
    QCOMPARE(gMisuses, 0);
}

void TestBuilder::takeAndReAddItem()
{
    QCustomPlot a;
    QCustomPlot b;
    auto line = a.add<QCPItemLine>().setStart(1, 1).setEnd(2, 2);
    auto taken = a.take(line);
    QVERIFY(taken);
    QCOMPARE(a.itemCount(), 0);
    QCOMPARE(taken->parentPlot(), static_cast<QCustomPlot*>(nullptr));
    QCOMPARE(taken->start->keyAxis(), static_cast<QCPAxis*>(nullptr));
    settle(a);

    auto moved = b.add(std::move(taken));
    QCOMPARE(b.itemCount(), 1);
    QCOMPARE(moved->parentPlot(), &b);
    QCOMPARE(moved->start->keyAxis(), b.xAxis);
    QCOMPARE(moved->end->coords(), QPointF(2, 2));
    settle(b);
    QCOMPARE(gMisuses, 0);
}

void TestBuilder::takeColorMapLeavesScale()
{
    QCustomPlot plot;
    auto scale = plot.add<QCPColorScale>(1, 0);
    auto map = plot.add<QCPColorMap>().setColorScale(scale);
    QCOMPARE(map->colorScale(), scale.get());
    QCOMPARE(scale->colorMaps().size(), 1);

    auto taken = plot.take(map);
    QVERIFY(taken);
    QCOMPARE(taken->colorScale(), static_cast<QCPColorScale*>(nullptr));
    QCOMPARE(scale->colorMaps().size(), 0);
    scale->setDataRange(QCPRange(5, 9));
    QVERIFY(taken->dataRange().lower != 5);
    taken.reset();
    settle(plot);
    QCOMPARE(gMisuses, 0);
}

void TestBuilder::takeBarsLeavesGroupAndStack()
{
    QCustomPlot plot;
    auto group = plot.add<QCPBarsGroup>();
    auto lower = plot.add<QCPBars>(kKeys, kValues).setBarsGroup(group);
    auto upper = plot.add<QCPBars>(kKeys, kValues).setBarsGroup(group);
    upper->moveAbove(lower.get());
    QCOMPARE(upper->barBelow(), lower.get());

    auto taken = plot.take(lower);
    QVERIFY(taken);
    QCOMPARE(taken->barsGroup(), static_cast<QCPBarsGroup*>(nullptr));
    QCOMPARE(group->bars().size(), 1);
    QCOMPARE(upper->barBelow(), static_cast<QCPBars*>(nullptr));
    taken.reset();
    settle(plot);
    QCOMPARE(gMisuses, 0);
}

void TestBuilder::takeDeadHandle()
{
    QCustomPlot plot;
    auto g = plot.add<QCPGraph>(kKeys, kValues);
    g.remove();
    QVERIFY(!plot.take(g));
    QCOMPARE(gMisuses, 1);
}

void TestBuilder::takeForeignObject()
{
    QCustomPlot owner;
    QCustomPlot other;
    auto g = owner.add<QCPGraph>(kKeys, kValues);
    QVERIFY(!other.take(g));
    QCOMPARE(gMisuses, 1);
    QCOMPARE(owner.graphCount(), 1);
    QVERIFY(g);
}

void TestBuilder::takeDuringTeardown()
{
    auto* plot = new QCustomPlot;
    auto first = plot->add<QCPGraph>(kKeys, kValues);
    auto second = plot->add<QCPGraph>(kKeys, kValues);
    bool refused = false;
    connect(first.get(), &QObject::destroyed,
            [plot, second, &refused] { refused = plot->take(second) == nullptr; });
    delete plot;
    QVERIFY(refused);
    QCOMPARE(gMisuses, 1);
}

void TestBuilder::addUserBuilt()
{
    QCustomPlot plot;
    auto built = std::make_unique<UserGraph>(nullptr, nullptr); // detached: valid, no misuse
    QCOMPARE(gMisuses, 0);
    auto handle = plot.add(std::move(built));
    static_assert(std::is_base_of_v<QCPHandle<UserGraph>, decltype(handle)>);
    QVERIFY(handle);
    QCOMPARE(handle->keyAxis(), plot.xAxis);
    QCOMPARE(handle->valueAxis(), plot.yAxis);
    QCOMPARE(handle->layer(), plot.currentLayer());
    QCOMPARE(plot.graphCount(), 1);
    QVERIFY(plot.legend->hasItemWithPlottable(handle.get()));
    handle.setName("user").setPen(QPen(Qt::red));
    QCOMPARE(handle->name(), QString("user"));
    QCOMPARE(gMisuses, 0);
}

void TestBuilder::addRefusesObjectOfAPlot()
{
    QCustomPlot plot;
    QCustomPlot other;
    auto handle = plot.add(std::make_unique<QCPGraph>(other.xAxis, other.yAxis));
    QVERIFY(!handle);
    QCOMPARE(gMisuses, 1);
    QCOMPARE(plot.graphCount(), 0);
    QCOMPARE(other.graphCount(), 0);
    QVERIFY(!plot.add(std::unique_ptr<QCPGraph>()));
    QCOMPARE(gMisuses, 2);

    plot.plotLayout()->clear();
    QVERIFY(!plot.add(std::make_unique<QCPGraph>(other.xAxis, other.yAxis)));
    QCOMPARE(gMisuses, 3); // the object is the cause; the missing axis rect is not reported too
}

void TestBuilder::axesOfTwoPlotsAreRefused()
{
    QCustomPlot plot;
    QCustomPlot other;
    auto built = std::make_unique<QCPGraph>(plot.xAxis, other.yAxis);
    QCOMPARE(gMisuses, 1);
    QCOMPARE(plot.plottableCount(), 0);
    QCOMPARE(other.plottableCount(), 0);
    QVERIFY(!built->layer()); // inert: neither plot draws it
    settle(plot);
    settle(other);
}

void TestBuilder::tracerGraphIsClearedOnTake()
{
    QCustomPlot plot;
    auto g = plot.add<QCPGraph>(kKeys, kValues);
    auto tracer = plot.add<QCPItemTracer>();
    tracer->setGraph(g.get());
    QCOMPARE(tracer->graph(), g.get());

    auto taken = plot.take(tracer);
    QVERIFY(taken);
    QCOMPARE(tracer->graph(), nullptr); // the graph belongs to the plot it left

    QCustomPlot other;
    auto moved = other.add(std::move(taken));
    QVERIFY(moved);
    settle(other);
    QCOMPARE(gMisuses, 0);
}

void TestBuilder::takenPointerDeletedIsClean()
{
    QCustomPlot plot;
    auto g = plot.add<QCPGraph>(kKeys, kValues);
    auto line = plot.add<QCPItemLine>();
    auto bars = plot.add<QCPBars>(kKeys, kValues);
    auto map = plot.add<QCPColorMap>();
    auto takenGraph = plot.take(g);
    auto takenLine = plot.take(line);
    auto takenBars = plot.take(bars);
    auto takenMap = plot.take(map);
    takenGraph.reset();
    takenLine.reset();
    takenBars.reset();
    takenMap.reset();
    QVERIFY(!g);
    QCOMPARE(plot.plottableCount(), 0);
    QCOMPARE(plot.itemCount(), 0);
    settle(plot);
    QCOMPARE(gMisuses, 0);
}

void TestBuilder::addIntervalsWithBadColumnsLeavesThePlotUnchanged()
{
    QCustomPlot plot;
    const int rows = plot.legend->itemCount();
    qcp::intervals::Columns bad;
    bad.start = {0.0, 1.0};
    bad.stop = {1.0};
    QVERIFY_EXCEPTION_THROWN(plot.add<QCPIntervals>(bad), std::invalid_argument);
    QCOMPARE(plot.plottableCount(), 0);
    QCOMPARE(plot.legend->itemCount(), rows);
    QCOMPARE(gMisuses, 0);
    settle(plot);
}

void TestBuilder::takeCutsAnchorsBothWays()
{
    QCustomPlot plot;
    settle(plot);
    auto rect = plot.add<QCPItemRect>();
    rect->topLeft->setCoords(1, 1);
    rect->bottomRight->setCoords(2, 2);
    auto follower = plot.add<QCPItemLine>();
    QVERIFY(follower->start->setParentAnchor(rect->topLeft));
    const QPointF before = follower->start->pixelPosition();

    auto taken = plot.take(rect);
    QVERIFY(taken);
    QCOMPARE(follower->start->parentAnchor(), static_cast<QCPItemAnchor*>(nullptr));
    QCOMPARE(follower->start->pixelPosition(), before);
    taken.reset();
    settle(plot);
    QCOMPARE(gMisuses, 0);
}

void TestBuilder::takeCutsOutboundAnchorLinks()
{
    QCustomPlot a;
    QCustomPlot b;
    for (QCustomPlot* p : {&a, &b})
    {
        p->xAxis->setRange(0, 4);
        p->yAxis->setRange(0, 4);
        settle(*p);
    }
    auto base = a.add<QCPItemRect>();
    base->topLeft->setCoords(1, 1);
    base->bottomRight->setCoords(2, 2);
    auto line = a.add<QCPItemLine>().setStart(0, 0).setEnd(0, 0);
    QVERIFY(line->end->setParentAnchor(base->bottomRight));
    const QPointF pixel = line->end->pixelPosition();

    auto taken = a.take(line);
    QCOMPARE(taken->end->parentAnchor(), static_cast<QCPItemAnchor*>(nullptr));
    QCOMPARE(base->bottomRight->pixelPosition().isNull(), false);
    auto moved = b.add(std::move(taken));
    QCOMPARE(moved->end->pixelPosition(), pixel);
    QCOMPARE(gMisuses, 0);
}

void TestBuilder::pixelPositionOfATakenItemIsNull()
{
    QCustomPlot plot;
    auto line = plot.add<QCPItemLine>();
    line->start->setType(QCPItemPosition::ptViewportRatio);
    line->start->setCoords(0.5, 0.5);
    auto taken = plot.take(line);
    QCOMPARE(taken->start->pixelPosition(), QPointF());
    QCOMPARE(taken->end->pixelPosition(), QPointF());
}

void TestBuilder::addPlottableDuringTeardownIsRefused()
{
    auto* plot = new QCustomPlot;
    auto first = plot->add<QCPGraph>(kKeys, kValues);
    bool refused = false;
    connect(first.get(), &QObject::destroyed,
            [plot, &refused]
            { refused = !plot->add(std::make_unique<QCPGraph>(nullptr, nullptr)); });
    delete plot;
    QVERIFY(refused);
    QCOMPARE(gMisuses, 1);
}

void TestBuilder::itemsWithoutAxisRectAreRefusedLikePlottables()
{
    QCustomPlot plot;
    QCustomPlot source;
    auto taken = source.take(source.add<QCPItemLine>());
    plot.plotLayout()->clear();
    QVERIFY(!plot.add<QCPItemLine>());
    QCOMPARE(gMisuses, 1);
    QCOMPARE(plot.itemCount(), 0);
    QVERIFY(!plot.add(std::move(taken)));
    QCOMPARE(gMisuses, 2);
    QCOMPARE(plot.itemCount(), 0);
}

void TestBuilder::layoutCellsFarBeyondTheGridAreRefused()
{
    QCustomPlot plot;
    QVERIFY(!plot.add<QCPTextElement>(1 << 30, 0, "far"));
    QVERIFY(!plot.add<QCPTextElement>(0, 5, "far"));
    QVERIFY(!plot.add<QCPTextElement>(-1, 0, "negative"));
    QCOMPARE(gMisuses, 3);
    QVERIFY(plot.add<QCPTextElement>(1, 0, "next row"));
    QVERIFY(plot.add<QCPTextElement>(0, 1, "next column"));
    QCOMPARE(gMisuses, 3);
}

namespace
{
using Src = QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>;

// Data in, in flight, source plot gone, data in again on the detached object, then onto plot B.
template <typename T, typename Feed>
void roundTripThroughADeadPlot(Feed feed)
{
    QCustomPlot b;
    settle(b);
    const QImage empty = b.toPixmap(400, 300).toImage();

    auto* a = new QCustomPlot;
    auto handle = a->add<T>();
    handle.with(feed);
    std::unique_ptr<T> taken = a->take(handle);
    QVERIFY(taken);
    QVERIFY(taken->pipeline().scheduler() == nullptr);
    delete a;
    feed(*taken);
    for (int i = 0; i < 20; ++i)
        QCoreApplication::processEvents();

    auto moved = b.add(std::move(taken));
    QVERIFY(moved);
    QVERIFY(moved->pipeline().scheduler() == b.pipelineScheduler());
    QTRY_VERIFY_WITH_TIMEOUT(!moved->busy(), 5000);
    b.rescaleAxes();
    settle(b);
    QVERIFY(b.toPixmap(400, 300).toImage() != empty);
}
} // namespace

void TestBuilder::takenPipelinePlottablesOutliveTheirPlot()
{
    const std::vector<double> x{0, 1, 2, 3}, y{1, 3, 2, 4};
    roundTripThroughADeadPlot<QCPGraph2>([&](QCPGraph2& p) { p.setData(x, y); });
    roundTripThroughADeadPlot<QCPMultiGraph>(
        [&](QCPMultiGraph& p) { p.setData(x, std::vector<std::vector<double>>{y, x}); });
    roundTripThroughADeadPlot<QCPWaterfallGraph>(
        [&](QCPWaterfallGraph& p) { p.setData(x, std::vector<std::vector<double>>{y, x}); });
    roundTripThroughADeadPlot<QCPHistogram>(
        [&](QCPHistogram& p)
        {
            p.setDataSource(std::make_shared<Src>(x, std::vector<std::vector<double>>{y}));
        });
    roundTripThroughADeadPlot<QCPHistogram2D>([&](QCPHistogram2D& p) { p.setData(x, y); });
    roundTripThroughADeadPlot<QCPColorMap2>(
        [&](QCPColorMap2& p)
        { p.setData(std::vector<double>{0, 1}, std::vector<double>{0, 1}, std::vector<double>{1, 2, 3, 4}); });
    QCOMPARE(gMisuses, 0);
}

void TestBuilder::errorBarsTakeTheirPlottableByHandle()
{
    QCustomPlot plot;
    auto graph = plot.add<QCPGraph>(kKeys, kValues);
    auto bars = plot.add<QCPErrorBars>().setDataPlottable(graph).setWhiskerWidth(5);
    QCOMPARE(bars->dataPlottable(), graph.get());
    QCOMPARE(gMisuses, 0);

    graph.remove();
    bars.setDataPlottable(graph);
    QCOMPARE(gMisuses, 1);
}

void TestBuilder::tracerTakesItsGraphByHandle()
{
    QCustomPlot plot;
    auto graph = plot.add<QCPGraph>(kKeys, kValues);
    auto tracer = plot.add<QCPItemTracer>().setGraph(graph).setGraphKey(2);
    QCOMPARE(tracer->graph(), graph.get());
    QCOMPARE(gMisuses, 0);

    graph.remove();
    tracer.setGraph(graph);
    QCOMPARE(gMisuses, 1);
}

void TestBuilder::richTextHandleKeepsTheTextSetters()
{
    QCustomPlot plot;
    auto text = plot.add<QCPItemRichText>();
    auto chained = text.setPosition(1, 2).setColor(Qt::red).setHtml("<b>bold</b>").setVisible(false);
    static_assert(std::is_same_v<decltype(chained), QCPItemRichTextOps<QCPItemRichText>>);
    QCOMPARE(text->position->coords(), QPointF(1, 2));
    QCOMPARE(text->color(), QColor(Qt::red));
    QCOMPARE(text->html(), QString("<b>bold</b>"));
    QVERIFY(!text->visible());
    QCOMPARE(gMisuses, 0);
}

void TestBuilder::graph2ScatterColoursChangeTheDrawing()
{
    QCustomPlot plot;
    plot.xAxis->setRange(0, 4);
    plot.yAxis->setRange(0, 7);
    auto g = plot.add<QCPGraph2>(kKeys, kValues)
                 .setLineStyle(QCPGraph2::lsNone)
                 .setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 9));
    QTRY_VERIFY_WITH_TIMEOUT(!g->busy(), 5000);
    settle(plot);
    const QImage plain = plot.toPixmap(400, 300).toImage();
    g.setScatterColorGradient(QCPColorGradient(QCPColorGradient::gpHot))
        .setScatterColorValues(std::vector<float>{0, 5, 10});
    settle(plot);
    QVERIFY(plot.toPixmap(400, 300).toImage() != plain);
    QCOMPARE(gMisuses, 0);
}

void TestBuilder::addPolar()
{
    QCustomPlot plot;
    auto area = plot.add<QCPPolarArea>(1, 0);
    QVERIFY(area);
    QCOMPARE(plot.plotLayout()->element(1, 0), area.get());
    QCOMPARE(plot.legend->itemCount(), 0);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis(0)->setRange(0, 10);
    settle(plot);
    const QImage withoutGraph = plot.toPixmap(400, 300).toImage();

    auto graph = area.add<QCPPolarGraph>(kKeys, kValues).setName("p");
    QVERIFY(graph);
    QCOMPARE(graph->name(), QString("p"));
    QCOMPARE(graph->data()->size(), 3);
    QCOMPARE(plot.legend->itemCount(), 1);
    settle(plot);
    QVERIFY(plot.toPixmap(400, 300).toImage() != withoutGraph);
    QCOMPARE(gMisuses, 0);
}

void TestBuilder::takeAndAdoptAPolarGraph()
{
    QCustomPlot plot;
    auto area = plot.add<QCPPolarArea>(1, 0);
    QVERIFY(area);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis(0)->setRange(0, 10);

    auto graph = area.add<QCPPolarGraph>(kKeys, kValues).setName("p");
    QVERIFY(graph);
    QCOMPARE(plot.plottableCount(), 1);

    // A taken graph belongs to nobody: the plot's default axes still refuse it.
    std::unique_ptr<QCPPolarGraph> orphan = plot.take(area.add<QCPPolarGraph>(kKeys, kValues));
    QVERIFY(orphan);
    QCOMPARE(orphan->system(), nullptr);

    // The way back in is the area that carries its frame.
    auto readded = area.add(std::move(orphan));
    QVERIFY(readded);
    QCOMPARE(plot.plottableCount(), 2); // the first graph and the one that came back
    QVERIFY(readded->system() != nullptr);
    QCOMPARE(readded->system()->area(), static_cast<QCPArea*>(area.get()));
    settle(plot);
    QCOMPARE(gMisuses, 0);
}

void TestBuilder::takePolarGraph()
{
    QCustomPlot plot;
    settle(plot);
    auto area = plot.add<QCPPolarArea>(1, 0);
    QVERIFY(area);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis(0)->setRange(0, 10);
    settle(plot);
    const QImage withoutGraph = plot.toPixmap(400, 300).toImage();

    auto graph = area.add<QCPPolarGraph>(kKeys, kValues);
    QVERIFY(graph);
    QCOMPARE(plot.legend->itemCount(), 1);
    settle(plot);
    QVERIFY(plot.toPixmap(400, 300).toImage() != withoutGraph);

    auto taken = plot.take(graph);
    QVERIFY(taken);
    QCOMPARE(taken.get(), graph.get()); // same object, now owned by the caller
    QCOMPARE(plot.legend->itemCount(), 0);
    QCOMPARE(taken->keyAxis(), nullptr);
    // Nothing of the graph is left on the axis: the plot draws what it drew before it.
    settle(plot);
    QCOMPARE(plot.toPixmap(400, 300).toImage(), withoutGraph);
    QCOMPARE(gMisuses, 0);

    // A polar graph is owned by a polar area, so the plot cannot take one back: the refused add
    // destroys the pointer it was given.
    QVERIFY(!plot.add(std::move(taken)));
    QCOMPARE(gMisuses, 1);
    QVERIFY(!graph);
}

void TestBuilder::deletedPolarGraphLeavesNoLegendRow()
{
    QCustomPlot plot;
    settle(plot);
    auto area = plot.add<QCPPolarArea>(1, 0);
    QVERIFY(area);
    QCPLegend second;
    plot.plotLayout()->addElement(0, 1, &second);

    auto graph = area.add<QCPPolarGraph>(kKeys, kValues);
    QVERIFY(graph);
    graph->addToLegend(&second);
    QCOMPARE(plot.legend->itemCount(), 1);
    QCOMPARE(second.itemCount(), 1);

    graph.remove(); // a plain delete, as a handle's remove() does
    QCOMPARE(plot.legend->itemCount(), 0);
    QCOMPARE(second.itemCount(), 0);

    auto other = area.add<QCPPolarGraph>(kKeys, kValues);
    QVERIFY(other);
    QCOMPARE(plot.legend->itemCount(), 1);
    auto taken = plot.take(other);
    QVERIFY(taken);
    QCOMPARE(plot.legend->itemCount(), 0);
    taken.reset();
    QCOMPARE(plot.legend->itemCount(), 0);
    QCOMPARE(gMisuses, 0);
}

void TestBuilder::takePolarGraphRefusals()
{
    QCustomPlot plot;
    QCustomPlot other;
    settle(plot);
    auto area = plot.add<QCPPolarArea>(1, 0);
    QVERIFY(area);
    auto graph = area.add<QCPPolarGraph>(kKeys, kValues);
    QVERIFY(graph);

    // the graph of another plot
    auto foreignArea = other.add<QCPPolarArea>(1, 0);
    QVERIFY(foreignArea);
    auto foreign = foreignArea.add<QCPPolarGraph>(kKeys, kValues);
    QVERIFY(foreign);
    QVERIFY(!plot.take(foreign));
    QCOMPARE(gMisuses, 1);
    QVERIFY(foreign);

    // the same handle twice
    auto taken = plot.take(graph);
    QVERIFY(taken);
    QVERIFY(!plot.take(graph));
    QCOMPARE(gMisuses, 2);

    // a dead handle: the caller deleted the object it was given
    auto dead = area.add<QCPPolarGraph>(kKeys, kValues);
    QVERIFY(dead);
    std::unique_ptr<QCPPolarGraph> owned = plot.take(dead);
    QVERIFY(owned);
    owned.reset();
    QVERIFY(!plot.take(dead));
    QCOMPARE(gMisuses, 3);
    QVERIFY(taken);
}

void TestBuilder::takenPolarGraphOutlivesItsPlot()
{
    auto* plot = new QCustomPlot;
    auto area = plot->add<QCPPolarArea>(1, 0);
    QVERIFY(area);
    auto graph = area.add<QCPPolarGraph>(kKeys, kValues);
    QVERIFY(graph);
    auto taken = plot->take(graph);
    QVERIFY(taken);
    delete plot;
    QVERIFY(taken);
    QCOMPARE(taken->keyAxis(), nullptr);
    QCOMPARE(taken->parentPlot(), nullptr);
    taken.reset();
    QCOMPARE(gMisuses, 0);
}

void TestBuilder::orphanedPolarGraphCanBeTaken()
{
    QCustomPlot plot;
    settle(plot);
    auto area = plot.add<QCPPolarArea>(1, 0);
    QVERIFY(area);
    auto graph = area.add<QCPPolarGraph>(kKeys, kValues);
    QVERIFY(graph);

    delete area.get(); // the area outlives its graphs by contract: it nulls their key axis
    QCOMPARE(graph->keyAxis(), nullptr);
    auto taken = plot.take(graph);
    QVERIFY(taken);
    QCOMPARE(taken.get(), graph.get());
    QCOMPARE(plot.legend->itemCount(), 0);
    QCOMPARE(gMisuses, 0);
}

void TestBuilder::addPolarGraphWithoutRadialAxisIsRefused()
{
    QCustomPlot plot;
    settle(plot);
    auto area = plot.add<QCPPolarArea>(1, 0);
    QVERIFY(area);
    while (area->radialAxisCount() > 0)
        area->removeRadialAxis(area->radialAxis(0));
    QCOMPARE(area->radialAxisCount(), 0);

    auto graph = area.add<QCPPolarGraph>(kKeys, kValues);
    QVERIFY(!graph);
    QCOMPARE(gMisuses, 1);
}

namespace
{
QStringList gLoggedMessages;
} // namespace

void TestBuilder::takeGraphClearsTracers()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    auto graph = plot.add<QCPGraph>(QVector<double> { 0, 1, 2 }, QVector<double> { 0, 1, 0 });
    auto tracer = plot.add<QCPItemTracer>();
    tracer->setGraph(graph.get());
    tracer->setGraphKey(1.0);
    plot.xAxis->setRange(0, 2);
    plot.yAxis->setRange(-1, 2);
    plot.replot();

    auto taken = plot.take(graph);
    QVERIFY(taken);
    QVERIFY2(!tracer->graph(), "the tracer still points at the taken graph");

    gLoggedMessages.clear();
    const QtMessageHandler previous = qInstallMessageHandler(
        [](QtMsgType, const QMessageLogContext&, const QString& message) {
            gLoggedMessages << message;
        });
    plot.replot();
    plot.replot();
    qInstallMessageHandler(previous);

    for (const QString& message : std::as_const(gLoggedMessages))
        QVERIFY2(!message.contains(QLatin1String("not contained")), qPrintable(message));
    QCOMPARE(gMisuses, 0);
}
