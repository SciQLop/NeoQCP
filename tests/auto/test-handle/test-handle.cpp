#include "test-handle.h"
#include <QDebug>
#include <QSet>
#include <functional>
#include <memory>
#include <type_traits>
#include <unordered_set>
#include <utility>

class TestHandleFwd;
QCPHandle<TestHandleFwd> forwardDeclaredHandle();
class TestHandleFwd : public QObject
{
};
QCPHandle<TestHandleFwd> forwardDeclaredHandle() { return {}; }

namespace
{
int gMisuses = 0;
void record(const char*, const char*) { ++gMisuses; }

// A handle is not implicitly built from a raw pointer: making one stays a deliberate act. The
// gcc/clang-friendly way to ask is a requires-expression on a by-value parameter.
template <typename T>
void takesAHandle(QCPHandle<T>);

template <typename T>
concept RawPointerMakesAHandle = requires(T* raw) { takesAHandle<T>(raw); };

// The other direction: a handle does not convert to a QPointer either (`QPointer<T> p = h;`).
template <typename P>
void takesAValue(P);

template <typename P, typename H>
concept CopyInitialisesFromHandle = requires(H h) { takesAValue<P>(h); };

enum class OverloadPicked
{
    none,
    pointer,
    boolean,
    object
};
OverloadPicked gPicked = OverloadPicked::none;
void pickOverload(QCPAxis*) { gPicked = OverloadPicked::pointer; }
void pickOverload(bool) { gPicked = OverloadPicked::boolean; }
void pickOverload(QObject*) { gPicked = OverloadPicked::object; }

template <typename H>
concept CallableWithTheOverloadSet = requires(H h) { pickOverload(h); };
} // namespace

static_assert(!RawPointerMakesAHandle<QCPAxis>);
static_assert(!RawPointerMakesAHandle<QCPGraph>);
static_assert(!CopyInitialisesFromHandle<QPointer<QCPAxis>, QCPHandle<QCPAxis>>);
static_assert(std::is_convertible_v<QCPHandle<QCPAxis>, QCPAxis*>);
static_assert(!std::is_convertible_v<QCPHandle<QCPAxis>, QPointer<QCPAxis>>);
static_assert(std::is_constructible_v<QPointer<QCPAxis>, QCPHandle<QCPAxis>>);
static_assert(std::is_constructible_v<bool, QCPHandle<QCPAxis>>);
static_assert(CallableWithTheOverloadSet<QCPHandle<QCPAxis>>);
static_assert(std::is_convertible_v<QCPHandle<QCPGraph>, QCPHandle<QCPAbstractPlottable>>);
// Assignments: a derived handle assigns to a base one, and nullptr copy-initialises a handle
// (Opus I1, opencode I-2, Opus M-1, M-2). `h = 0` and `h = {}` are pinned in the body below.
static_assert(std::is_assignable_v<QCPHandle<QCPAbstractPlottable>&, QCPHandle<QCPGraph>>);
static_assert(std::is_convertible_v<std::nullptr_t, QCPHandle<QCPAxis>>);
static_assert(!std::is_assignable_v<QCPHandle<QCPGraph>&, QCPHandle<QCPAbstractPlottable>>);

void TestHandle::init()
{
    gMisuses = 0;
    gPicked = OverloadPicked::none;
    qcp::setMisuseHandler(record);
}

void TestHandle::cleanup() { qcp::setMisuseHandler(nullptr); }

void TestHandle::handleTestsFalseAfterDelete()
{
    QCustomPlot plot;
    QCPHandle<QCPGraph> h(plot.addGraph());
    auto copy = h;
    QVERIFY(h);
    QVERIFY(copy);
    delete h.get();
    QVERIFY(!h);
    QVERIFY(!copy);
    QVERIFY(h.get() == nullptr);
}

void TestHandle::handleOutlivesPlot()
{
    auto plot = std::make_unique<QCustomPlot>();
    QCPHandle<QCPGraph> h(plot->addGraph());
    std::function<bool()> alive = [h] { return static_cast<bool>(h); };
    QVERIFY(alive());
    plot.reset();
    QVERIFY(!alive());
}

void TestHandle::aHandleConnectsLikeTheObject()
{
    QCustomPlot plot;
    QCPHandle<QCPAxis> axis(plot.axisRect()->axis(QCPAxis::atBottom));
    int fired = 0;
    QCPRange seen;
    const auto connection = QObject::connect(axis, &QCPAxis::rangeChanged, &plot,
                                             [&fired, &seen](const QCPRange& range) {
                                                 ++fired;
                                                 seen = range;
                                             });
    QVERIFY(connection);
    axis->setRange(1.0, 5.0);
    QCOMPARE(fired, 1);
    QCOMPARE(seen.lower, 1.0);
    QCOMPARE(seen.upper, 5.0);
}

void TestHandle::connectingADeadHandleWarns()
{
    // QObject::connect on a null sender: Qt's warning, and an invalid connection (K10).
    QCustomPlot plot;
    QCPHandle<QCPAxis> dead;
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("invalid nullptr parameter"));
    QVERIFY(!QObject::connect(dead, &QCPAxis::rangeChanged, &plot, [](const QCPRange&) {}));

    auto extra = plot.axisRect()->add<QCPAxis>(QCPAxis::atRight);
    QVERIFY(extra);
    extra.remove();
    QVERIFY(!extra);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("invalid nullptr parameter"));
    QVERIFY(!QObject::connect(extra, &QCPAxis::rangeChanged, &plot, [](const QCPRange&) {}));
}

void TestHandle::aHandleConvertsToItsPointer()
{
    QCustomPlot plot;
    QCPHandle<QCPGraph> handle(plot.addGraph());
    QCPGraph* raw = handle; // implicit, like QPointer
    QCOMPARE(raw, handle.get());
    QVERIFY(static_cast<QCPGraph*>(handle) == raw);

    delete raw;
    QVERIFY(static_cast<QCPGraph*>(handle) == nullptr);
    QVERIFY(!handle);

    // `delete h` compiles through the conversion (and bypasses remove()).
    QCPHandle<QCPTextElement> loose(new QCPTextElement(&plot, "loose"));
    QVERIFY(loose);
    delete loose;
    QVERIFY(!loose);
    QCOMPARE(gMisuses, 0);
}

void TestHandle::aDerivedHandleConvertsToABaseHandle()
{
    QCustomPlot plot;
    QCPHandle<QCPGraph> graph(plot.addGraph());
    QCPHandle<QCPAbstractPlottable> base = graph; // implicit converting constructor
    QVERIFY(base);
    QCOMPARE(base.get(), static_cast<QCPAbstractPlottable*>(graph.get()));

    const QCPHandle<QCPGraph>& cref = graph;
    QCPHandle<QCPAbstractPlottable> direct(cref);
    QVERIFY(direct);

    base.remove();
    QVERIFY(!base);
    QVERIFY(!graph);
    QVERIFY(!direct);
    QCOMPARE(gMisuses, 0);
}

void TestHandle::handlesHashAndCompare()
{
    QCustomPlot plot;
    auto first = plot.add<QCPAxis>(QCPAxis::atLeft);
    auto second = plot.add<QCPAxis>(QCPAxis::atLeft);
    QVERIFY(first);
    QVERIFY(second);

    QCPHandle<QCPAxis> a(first.get());
    QCPHandle<QCPAxis> aCopy(first.get());
    QCPHandle<QCPAxis> b(second.get());
    QVERIFY(a == aCopy);
    QVERIFY(a != b);
    QCOMPARE(qHash(a), qHash(aCopy));

    QSet<QCPHandle<QCPAxis>> set;
    set.insert(a);
    set.insert(aCopy);
    set.insert(b);
    QCOMPARE(set.size(), 2);
    QVERIFY(set.contains(a));
    QVERIFY(!set.contains(QCPHandle<QCPAxis>()));

    std::unordered_set<QCPHandle<QCPAxis>> unordered;
    unordered.insert(a);
    unordered.insert(aCopy);
    unordered.insert(b);
    QCOMPARE(unordered.size(), std::size_t(2));
    QCOMPARE(unordered.count(a), std::size_t(1));
}

void TestHandle::aHandlePrintsToQDebug()
{
    QCustomPlot plot;
    QCPHandle<QCPAxis> axis(plot.axisRect()->axis(QCPAxis::atBottom));
    QString alive;
    QDebug(&alive) << axis;
    QVERIFY(alive.contains(QStringLiteral("QCPHandle")));
    QVERIFY(alive.contains(QString::number(reinterpret_cast<quintptr>(axis.get()), 16)));

    QCPHandle<QCPAxis> dead;
    QString text;
    QDebug(&text) << dead;
    QVERIFY(text.contains(QStringLiteral("QCPHandle")));
}

void TestHandle::ifOnAHandleIsNotAmbiguous()
{
    QCustomPlot plot;
    QCPHandle<QCPAxis> alive(plot.axisRect()->axis(QCPAxis::atBottom));
    if (alive)
    {
    }
    else
    {
        QFAIL("an alive handle does not test true in an if");
    }
    bool direct {alive};
    QVERIFY(direct);
    QVERIFY(static_cast<bool>(alive));

    QCPHandle<QCPAxis> dead;
    if (dead)
        QFAIL("a dead handle tests true in an if");
    bool deadDirect {dead};
    QVERIFY(!deadDirect);
}

void TestHandle::aRawPointerDoesNotMakeAHandleImplicitly()
{
    // The compile checks above are the test; the explicit constructor is deliberate.
    static_assert(std::is_constructible_v<QCPHandle<QCPAxis>, QCPAxis*>);
    static_assert(!std::is_convertible_v<QCPAxis*, QCPHandle<QCPAxis>>);
    QVERIFY(true);
}

void TestHandle::aHandleTakesPartInTheAmbiguityMatrix()
{
    QCustomPlot plot;
    QCPAxis* raw = plot.axisRect()->axis(QCPAxis::atBottom);
    QCPHandle<QCPAxis> handle(raw);

    QVERIFY(handle == raw);
    QVERIFY(raw == handle);
    QVERIFY(!(handle == nullptr));
    QVERIFY(handle != nullptr);
    QVERIFY(!(handle != raw));

    // A base handle against a const subclass pointer (`area() == this` in a const method): MSVC
    // found no operator for it before the handle had its own.
    const QCPHandle<QCPArea> area = plot.axisRect();
    const QCPAxisRect* rect = plot.axisRect();
    QVERIFY(area == rect);
    QVERIFY(rect == area);
    QVERIFY(!(area != rect));
    QVERIFY(!(area == static_cast<const QCPAxisRect*>(nullptr)));

    QCPHandle<QCPAxis> dead;
    QCOMPARE(handle ? 1 : 2, 1);
    QCOMPARE(dead ? 1 : 2, 2);

    gPicked = OverloadPicked::none;
    pickOverload(handle);
    QCOMPARE(gPicked, OverloadPicked::pointer);

    QCPHandle<QCPAbstractPlottable> plottable(plot.addGraph());
    QVERIFY(qobject_cast<QCPGraph*>(plottable));
    QVERIFY(!qobject_cast<QCPCurve*>(plottable));
    QVERIFY(!qobject_cast<QCPGraph*>(QCPHandle<QCPAbstractPlottable>()));

    {
        QSignalBlocker blocker(handle);
        QVERIFY(handle);
    }

    // Direct initialisation goes through QPointer(T*); copy-initialisation would need two
    // user-defined conversions and does not compile (checked above).
    QPointer<QCPAxis> tracked(handle);
    QCOMPARE(tracked.data(), raw);

    QCPHandle<QCPAxis> assigned;
    assigned = raw;
    QCOMPARE(assigned.get(), raw);
    assigned = nullptr;
    QVERIFY(!assigned);

    QCPHandle<QCPAbstractPlottable> baseAssigned;
    QCPGraph* graph = plot.addGraph();
    baseAssigned = graph;
    QCOMPARE(baseAssigned.get(), static_cast<QCPAbstractPlottable*>(graph));

    // A derived handle assigns to a base one, and a creator's handle assigns where a base handle is
    // expected (Opus I1).
    QCPHandle<QCPGraph> derived(graph);
    baseAssigned = derived;
    QCOMPARE(baseAssigned.get(), static_cast<QCPAbstractPlottable*>(graph));
    baseAssigned = plot.add<QCPBars>();
    QVERIFY(baseAssigned);

    // The zero, braced-empty and nullptr spellings all clear a handle (opencode I-2, Opus M-1, M-2).
    assigned = 0;
    QVERIFY(!assigned);
    assigned = raw;
    QVERIFY(assigned);
    assigned = {};
    QVERIFY(!assigned);
    QCPHandle<QCPAxis> nullInitialised = nullptr;
    QVERIFY(!nullInitialised);
    QCOMPARE(gMisuses, 0);
}

void TestHandle::pointerListHelper()
{
    QCustomPlot plot;
    auto first = plot.add<QCPAxis>(QCPAxis::atLeft);
    auto second = plot.add<QCPAxis>(QCPAxis::atLeft);
    QVERIFY(first);
    QVERIFY(second);

    const QList<QCPHandle<QCPAxis>> handles {first, second};
    const QList<QCPAxis*> raw = qcp::pointers(handles);
    QCOMPARE(raw.size(), 2);
    QCOMPARE(raw.at(0), first.get());
    QCOMPARE(raw.at(1), second.get());

    QCPHandle<QCPAxis> dead;
    const QList<QCPAxis*> withDead = qcp::pointers(QList<QCPHandle<QCPAxis>> {dead});
    QCOMPARE(withDead.size(), 1);
    QCOMPARE(withDead.at(0), nullptr);
}

void TestHandle::thePlotHandsOutHandles()
{
    using Plot = QCustomPlot;
    // QCustomPlot's getters (inventory §2.1): plain QCPHandle<T>, lists of handles (K1, K5).
    static_assert(std::is_same_v<decltype(std::declval<Plot&>().plotLayout()),
                                 QCPHandle<QCPLayoutGrid>>);
    static_assert(std::is_same_v<decltype(std::declval<const Plot&>().theme()),
                                 QCPHandle<QCPTheme>>);
    static_assert(std::is_same_v<decltype(std::declval<Plot&>().overlay()),
                                 QCPHandle<QCPOverlay>>);
    static_assert(std::is_same_v<decltype(std::declval<const Plot&>().pipelineScheduler()),
                                 QCPHandle<QCPPipelineScheduler>>);
    static_assert(std::is_same_v<decltype(std::declval<Plot&>().plottable(0)),
                                 QCPHandle<QCPAbstractPlottable>>);
    static_assert(std::is_same_v<decltype(std::declval<Plot&>().plottable()),
                                 QCPHandle<QCPAbstractPlottable>>);
    static_assert(std::is_same_v<decltype(std::declval<Plot&>().plottableGroups()),
                                 QList<QCPHandle<QCPPlottableGroup>>>);
    static_assert(std::is_same_v<decltype(std::declval<const Plot&>().selectedPlottables()),
                                 QList<QCPHandle<QCPAbstractPlottable>>>);
    static_assert(std::is_same_v<decltype(std::declval<const Plot&>().plottableAt(QPointF())),
                                 QCPHandle<QCPAbstractPlottable>>);
    static_assert(
        std::is_same_v<decltype(std::declval<const Plot&>().plottableAt<QCPGraph>(QPointF())),
                       QCPHandle<QCPGraph>>);
    static_assert(std::is_same_v<decltype(std::declval<const Plot&>().item(0)),
                                 QCPHandle<QCPAbstractItem>>);
    static_assert(std::is_same_v<decltype(std::declval<const Plot&>().item()),
                                 QCPHandle<QCPAbstractItem>>);
    static_assert(std::is_same_v<decltype(std::declval<const Plot&>().selectedItems()),
                                 QList<QCPHandle<QCPAbstractItem>>>);
    static_assert(std::is_same_v<decltype(std::declval<const Plot&>().itemAt(QPointF())),
                                 QCPHandle<QCPAbstractItem>>);
    static_assert(
        std::is_same_v<decltype(std::declval<const Plot&>().itemAt<QCPItemLine>(QPointF())),
                       QCPHandle<QCPItemLine>>);
    static_assert(std::is_same_v<decltype(std::declval<const Plot&>().layer(QString())),
                                 QCPHandle<QCPLayer>>);
    static_assert(std::is_same_v<decltype(std::declval<const Plot&>().layer(0)),
                                 QCPHandle<QCPLayer>>);
    static_assert(std::is_same_v<decltype(std::declval<const Plot&>().currentLayer()),
                                 QCPHandle<QCPLayer>>);
    static_assert(std::is_same_v<decltype(std::declval<const Plot&>().axisRect(0)),
                                 QCPHandle<QCPAxisRect>>);
    static_assert(std::is_same_v<decltype(std::declval<const Plot&>().axisRects()),
                                 QList<QCPHandle<QCPAxisRect>>>);
    static_assert(std::is_same_v<decltype(std::declval<const Plot&>().areas()),
                                 QList<QCPHandle<QCPArea>>>);
    static_assert(std::is_same_v<decltype(std::declval<const Plot&>().layoutElementAt(QPointF())),
                                 QCPHandle<QCPLayoutElement>>);
    static_assert(std::is_same_v<decltype(std::declval<const Plot&>().areaAt(QPointF())),
                                 QCPHandle<QCPArea>>);
    static_assert(std::is_same_v<decltype(std::declval<const Plot&>().axisRectAt(QPointF())),
                                 QCPHandle<QCPAxisRect>>);
    static_assert(std::is_same_v<decltype(std::declval<const Plot&>().selectedAxes()),
                                 QList<QCPHandle<QCPAxis>>>);
    static_assert(std::is_same_v<decltype(std::declval<const Plot&>().selectedLegends()),
                                 QList<QCPHandle<QCPLegend>>>);
    static_assert(std::is_same_v<decltype(std::declval<const Plot&>().selectionRect()),
                                 QCPHandle<QCPSelectionRect>>);
    static_assert(std::is_same_v<decltype(std::declval<Plot&>().addPlottableGroup(QString())),
                                 QCPHandle<QCPPlottableGroup>>);

    // The shortcut members (H-5, K1), same names.
    static_assert(std::is_same_v<decltype(std::declval<Plot&>().xAxis), QCPHandle<QCPAxis>>);
    static_assert(std::is_same_v<decltype(std::declval<Plot&>().yAxis), QCPHandle<QCPAxis>>);
    static_assert(std::is_same_v<decltype(std::declval<Plot&>().xAxis2), QCPHandle<QCPAxis>>);
    static_assert(std::is_same_v<decltype(std::declval<Plot&>().yAxis2), QCPHandle<QCPAxis>>);
    static_assert(std::is_same_v<decltype(std::declval<Plot&>().legend), QCPHandle<QCPLegend>>);

    // G-3 (5c): the graph list holds handles of the multigraph; addGraph returns add<>'s handle.
    static_assert(std::is_same_v<decltype(std::declval<const Plot&>().graph(0)),
                                 QCPHandle<QCPGraph>>);
    static_assert(std::is_same_v<decltype(std::declval<const Plot&>().graph()),
                                 QCPHandle<QCPGraph>>);
    static_assert(std::is_same_v<decltype(std::declval<Plot&>().addGraph()),
                                 decltype(std::declval<Plot&>().add<QCPGraph>())>);
    static_assert(std::is_same_v<decltype(std::declval<Plot&>().addGraph()),
                                 QCPGraphOps<QCPGraph>>);
    static_assert(std::is_same_v<decltype(std::declval<const Plot&>().selectedGraphs()),
                                 QList<QCPHandle<QCPGraph>>>);
    static_assert(std::is_same_v<decltype(std::declval<Plot&>().removeGraph(
                                     std::declval<QCPGraph*>())),
                                 bool>);

    // H-3: the returns that are not QObjects.
    static_assert(std::is_same_v<decltype(std::declval<Plot&>().gridRhiLayer()),
                                 QCPGridRhiLayer*>);
    static_assert(std::is_same_v<decltype(std::declval<const Plot&>().gridRhiLayerIfExists()),
                                 QCPGridRhiLayer*>);
    static_assert(std::is_same_v<decltype(std::declval<Plot&>().plottableRhiLayer(nullptr)),
                                 QCPPlottableRhiLayer*>);
    static_assert(std::is_same_v<decltype(std::declval<Plot&>().scatterRhiLayer(nullptr)),
                                 QCPScatterRhiLayer*>);

    QCustomPlot plot;
    QVERIFY(plot.plottableAt(QPointF()) == nullptr);
    QVERIFY(plot.itemAt(QPointF()) == nullptr);
    QVERIFY(plot.layoutElementAt(QPointF()) != nullptr);
    QCOMPARE(plot.axisRects().first().get(), plot.axisRect(0).get());
    // The object-typed property keeps its pointer type: moc assigns the getter's handle to it (H-7).
    QCOMPARE(plot.property("plotLayout").value<QCPLayoutGrid*>(), plot.plotLayout().get());
}

void TestHandle::aDeletedDefaultAxisTestsFalse()
{
    QCustomPlot plot;
    QCPHandle<QCPAxis> x = plot.xAxis;
    QVERIFY(x);
    QCOMPARE(x.get(), plot.xAxis.get());

    x.remove();
    QVERIFY(!x);
    QVERIFY(!plot.xAxis); // the member observes the deletion by itself (H-5)
    QVERIFY(plot.yAxis);
    QVERIFY(plot.xAxis2);
    QVERIFY(plot.yAxis2);
    QCOMPARE(plot.xAxis.get(), nullptr);
    plot.replot();
    QCOMPARE(gMisuses, 0);
}

void TestHandle::aDeletedLegendTestsFalse()
{
    QCustomPlot plot;
    QCPHandle<QCPLegend> legend = plot.legend;
    QVERIFY(legend);

    legend.remove();
    QVERIFY(!legend);
    QVERIFY(!plot.legend); // the member observes the deletion by itself (H-5)
    plot.replot();
    QCOMPARE(gMisuses, 0);
}

void TestHandle::addGraphReturnsTheSameHandleAsAdd()
{
    QCustomPlot plot;
    QCPGraph* graph = plot.addGraph();
    QVERIFY(graph);
    QCOMPARE(plot.graph(0), graph);

    auto added = plot.add<QCPGraph>();
    QVERIFY(added);
    auto chained = plot.addGraph().setName("chained");
    QCOMPARE(chained->name(), QString("chained"));
    QCOMPARE(plot.graph().get(), chained.get());
    QVERIFY(plot.removeGraph(chained));
    QCOMPARE(plot.graph(1), added.get());
    QCOMPARE(plot.plottable(0).get(), static_cast<QCPAbstractPlottable*>(graph));
    QCOMPARE(plot.plottable(1).get(), static_cast<QCPAbstractPlottable*>(added.get()));
    QCOMPARE(plot.plottableCount(), 2);
    QCOMPARE(gMisuses, 0);
}

void TestHandle::aConstructedGraphIsInTheGraphList()
{
    QCustomPlot plot;
    auto* graph = new QCPGraph(plot.xAxis, plot.yAxis);
    auto* waterfall = new QCPWaterfallGraph(plot.xAxis, plot.yAxis);
    QCOMPARE(plot.graphCount(), 2);
    QCOMPARE(plot.graph(0).get(), graph);
    QCOMPARE(plot.graph().get(), static_cast<QCPGraph*>(waterfall));
    QCOMPARE(plot.xAxis->graphs().size(), 2);

    // Taken out and handed back: it leaves the list and joins it again, at the end.
    std::unique_ptr<QCPGraph> taken = plot.take(plot.graph(0));
    QVERIFY(taken);
    QCOMPARE(plot.graphCount(), 1);
    auto back = plot.add(std::move(taken));
    QVERIFY(back);
    QCOMPARE(plot.graphCount(), 2);
    QCOMPARE(plot.graph(1).get(), back.get());

    delete waterfall; // a direct delete unlists it
    QCOMPARE(plot.graphCount(), 1);
    QCOMPARE(plot.clearGraphs(), 1);
    QCOMPARE(plot.plottableCount(), 0);
    QCOMPARE(gMisuses, 0);
}

void TestHandle::theLayoutHandsOutHandles()
{
    using Element = QCPLayoutElement;
    static_assert(std::is_same_v<decltype(std::declval<Element&>().layout()),
                                 QCPHandle<QCPLayout>>);
    static_assert(std::is_same_v<decltype(std::declval<Element&>().elements(false)),
                                 QList<QCPHandle<QCPLayoutElement>>>);
    static_assert(std::is_same_v<decltype(std::declval<QCPLayout&>().elements(false)),
                                 QList<QCPHandle<QCPLayoutElement>>>);
    static_assert(std::is_same_v<decltype(std::declval<QCPLayout&>().elementAt(0)),
                                 QCPHandle<QCPLayoutElement>>);
    static_assert(std::is_same_v<decltype(std::declval<QCPLayoutGrid&>().elementAt(0)),
                                 QCPHandle<QCPLayoutElement>>);
    static_assert(std::is_same_v<decltype(std::declval<QCPLayoutGrid&>().element(0, 0)),
                                 QCPHandle<QCPLayoutElement>>);
    static_assert(std::is_same_v<decltype(std::declval<QCPLayoutInset&>().elementAt(0)),
                                 QCPHandle<QCPLayoutElement>>);
    static_assert(std::is_same_v<decltype(std::declval<QCPAlignmentGroup&>().elements()),
                                 QList<QCPHandle<QCPLayoutElement>>>);
    // K4: takeAt hands the ownership out.
    static_assert(std::is_same_v<decltype(std::declval<QCPLayout&>().takeAt(0)),
                                 QCPLayoutElement*>);
    // K5: the margin group class goes in 5d; the alignment group already answers a QPointer.
    static_assert(std::is_same_v<decltype(std::declval<Element&>().marginGroup(QCP::msLeft)),
                                 QCPMarginGroup*>);
    static_assert(std::is_same_v<decltype(std::declval<Element&>().alignmentGroup(QCP::msLeft)),
                                 QPointer<QCPAlignmentGroup>>);

    using Area = QCPArea;
    static_assert(std::is_same_v<decltype(std::declval<Area&>().insetLayout()),
                                 QCPHandle<QCPLayoutInset>>);
    static_assert(std::is_same_v<decltype(std::declval<Area&>().system(nullptr, nullptr)),
                                 QCPHandle<QCPCoordinateSystem>>);
    static_assert(std::is_same_v<decltype(std::declval<Area&>().systems()),
                                 QList<QCPHandle<QCPCoordinateSystem>>>);
    static_assert(std::is_same_v<decltype(std::declval<Area&>().allAxes()),
                                 QList<QCPHandle<QCPAxisBase>>>);

    using Rect = QCPAxisRect;
    static_assert(std::is_same_v<decltype(std::declval<Rect&>().rangeDragAxis(Qt::Horizontal)),
                                 QCPHandle<QCPAxis>>);
    static_assert(std::is_same_v<decltype(std::declval<Rect&>().rangeZoomAxis(Qt::Horizontal)),
                                 QCPHandle<QCPAxis>>);
    static_assert(std::is_same_v<decltype(std::declval<Rect&>().rangeDragAxes(Qt::Horizontal)),
                                 QList<QCPHandle<QCPAxis>>>);
    static_assert(std::is_same_v<decltype(std::declval<Rect&>().rangeZoomAxes(Qt::Horizontal)),
                                 QList<QCPHandle<QCPAxis>>>);
    static_assert(std::is_same_v<decltype(std::declval<Rect&>().axis(QCPAxis::atLeft)),
                                 QCPHandle<QCPAxis>>);
    static_assert(std::is_same_v<decltype(std::declval<Rect&>().axes()),
                                 QList<QCPHandle<QCPAxis>>>);
    static_assert(std::is_same_v<decltype(std::declval<Rect&>().axes(QCPAxis::atLeft)),
                                 QList<QCPHandle<QCPAxis>>>);
    static_assert(std::is_same_v<decltype(std::declval<Rect&>().allAxes()),
                                 QList<QCPHandle<QCPAxisBase>>>);
    static_assert(std::is_same_v<decltype(std::declval<Rect&>().addAxis(QCPAxis::atLeft)),
                                 QCPHandle<QCPAxis>>);
    static_assert(std::is_same_v<decltype(std::declval<Rect&>().addAxes(QCPAxis::atLeft)),
                                 QList<QCPHandle<QCPAxis>>>);
    static_assert(std::is_same_v<decltype(std::declval<Rect&>().colorScale()),
                                 QCPColorScaleOps<QCPColorScale>>);
    static_assert(std::is_same_v<decltype(std::declval<Rect&>().plottables()),
                                 QList<QCPHandle<QCPAbstractPlottable>>>);
    static_assert(std::is_same_v<decltype(std::declval<Rect&>().graphs()),
                                 QList<QCPHandle<QCPGraph>>>);
    static_assert(std::is_same_v<decltype(std::declval<Rect&>().items()),
                                 QList<QCPHandle<QCPAbstractItem>>>);

    using Scale = QCPColorScale;
    static_assert(std::is_same_v<decltype(std::declval<Scale&>().axis()),
                                 QCPHandle<QCPAxis>>);
    static_assert(std::is_same_v<decltype(std::declval<Scale&>().colorMaps()),
                                 QList<QCPHandle<QCPColorMap>>>);
    static_assert(std::is_same_v<decltype(std::declval<Scale&>().users()),
                                 QList<QCPHandle<QCPAbstractColorMapped>>>);

    QCustomPlot plot;
    QVERIFY(plot.axisRect()->addAxis(QCPAxis::atBottom));
    QVERIFY(plot.axisRect()->addAxes(QCPAxis::atLeft).size() == 1);
    plot.replot();
    QCOMPARE(gMisuses, 0);
}

void TestHandle::theAxesHandOutHandles()
{
    using Base = QCPAxisBase;
    static_assert(std::is_same_v<decltype(std::declval<Base&>().plottables()),
                                 QList<QCPHandle<QCPAbstractPlottable>>>);
    // K4: the virtual getters whose overrides return a derived pointer stay raw.
    static_assert(std::is_same_v<decltype(std::declval<Base&>().linkSource()), QCPAxisBase*>);
    static_assert(std::is_same_v<decltype(std::declval<Base&>().moveTarget()), QCPAxisBase*>);
    static_assert(std::is_same_v<decltype(std::declval<Base&>().area()), QCPArea*>);

    using Axis = QCPAxis;
    static_assert(std::is_same_v<decltype(std::declval<Axis&>().axisRect()),
                                 QCPHandle<QCPAxisRect>>);
    static_assert(std::is_same_v<decltype(std::declval<Axis&>().grid()),
                                 QCPHandle<QCPGrid>>);
    static_assert(std::is_same_v<decltype(std::declval<Axis&>().gridIfExists()),
                                 QCPHandle<QCPGrid>>);
    // H-3: not owned by the axis.
    static_assert(std::is_same_v<decltype(std::declval<Axis&>().labelRenderer()),
                                 QCPLabelRenderer*>);
    static_assert(std::is_same_v<decltype(std::declval<Axis&>().linkSource()), QCPAxis*>);
    static_assert(std::is_same_v<decltype(std::declval<Axis&>().moveTarget()), QCPAxis*>);
    static_assert(std::is_same_v<decltype(std::declval<Axis&>().area()), QCPArea*>);
    static_assert(std::is_same_v<decltype(std::declval<Axis&>().plottables()),
                                 QList<QCPHandle<QCPAbstractPlottable>>>);
    // G-3 (5c).
    static_assert(std::is_same_v<decltype(std::declval<Axis&>().graphs()),
                                 QList<QCPHandle<QCPGraph>>>);
    static_assert(std::is_same_v<decltype(std::declval<Axis&>().items()),
                                 QList<QCPHandle<QCPAbstractItem>>>);

    QCustomPlot plot;
    QVERIFY(plot.xAxis->axisRect());
    QVERIFY(plot.xAxis->grid());
    QVERIFY(plot.xAxis->gridIfExists());
    QCOMPARE(plot.xAxis->linkSource(), nullptr);
    QVERIFY(plot.xAxis->plottables().isEmpty());
}

void TestHandle::theLegendHandsOutHandles()
{
    static_assert(
        std::is_same_v<decltype(std::declval<QCPAbstractLegendItem&>().parentLegend()),
                       QCPHandle<QCPLegend>>);
    static_assert(std::is_same_v<decltype(std::declval<QCPLegend&>().item(0)),
                                 QCPHandle<QCPAbstractLegendItem>>);
    static_assert(
        std::is_same_v<decltype(std::declval<QCPLegend&>().itemWithPlottable(nullptr)),
                       QCPHandle<QCPPlottableLegendItem>>);
    static_assert(std::is_same_v<decltype(std::declval<QCPLegend&>().selectedItems()),
                                 QList<QCPHandle<QCPAbstractLegendItem>>>);
    static_assert(
        std::is_same_v<decltype(std::declval<QCPPlottableLegendItem&>().plottable()),
                       QCPHandle<QCPAbstractPlottable>>);
    // G-1/G-3 (5c).
    static_assert(std::is_same_v<decltype(std::declval<QCPGroupLegendItem&>().graph()),
                                 QCPHandle<QCPGraph>>);
    static_assert(std::is_same_v<decltype(std::declval<QCPIntervalsLegendItem&>().intervals()),
                                 QCPHandle<QCPIntervals>>);

    QCustomPlot plot;
    QCPGraph* graph = plot.addGraph();
    graph->setName("a");
    QVERIFY(plot.legend->item(0));
    QCOMPARE(plot.legend->item(0)->parentLegend().get(), plot.legend.get());
    QCOMPARE(plot.legend->itemWithPlottable(graph).get(),
             static_cast<QCPPlottableLegendItem*>(plot.legend->item(0).get()));
    QVERIFY(plot.legend->selectedItems().isEmpty());
}

void TestHandle::theLayersHandOutHandles()
{
    static_assert(std::is_same_v<decltype(std::declval<QCPLayer&>().parentPlot()),
                                 QCPHandle<QCustomPlot>>);
    static_assert(std::is_same_v<decltype(std::declval<QCPLayer&>().children()),
                                 QList<QCPHandle<QCPLayerable>>>);
    static_assert(std::is_same_v<decltype(std::declval<QCPLayerable&>().parentPlot()),
                                 QCPHandle<QCustomPlot>>);
    static_assert(std::is_same_v<decltype(std::declval<QCPLayerable&>().parentLayerable()),
                                 QCPHandle<QCPLayerable>>);
    static_assert(std::is_same_v<decltype(std::declval<QCPLayerable&>().layer()),
                                 QCPHandle<QCPLayer>>);
    // The intervals' label layer is an overlay of its own: a layer like any other (Opus M2).
    static_assert(std::is_same_v<decltype(std::declval<const QCPIntervals&>().labelLayer()),
                                 QCPHandle<QCPLayer>>);

    QCustomPlot plot;
    QCPLayer* mainLayer = plot.layer("main");
    QCOMPARE(mainLayer->parentPlot().get(), &plot);
    auto graph = plot.add<QCPGraph>();
    QVERIFY(graph);
    QCOMPARE(graph->layer().get(), mainLayer);
    QCOMPARE(graph->parentPlot().get(), &plot);
    QVERIFY(mainLayer->children().size() >= 1);
}

void TestHandle::theSystemsAndPolarHandOutHandles()
{
    using System = QCPCoordinateSystem;
    static_assert(std::is_same_v<decltype(std::declval<System&>().area()),
                                 QCPHandle<QCPArea>>);
    static_assert(std::is_same_v<decltype(std::declval<System&>().keyAxis()),
                                 QCPHandle<QCPAxisBase>>);
    static_assert(std::is_same_v<decltype(std::declval<System&>().valueAxis()),
                                 QCPHandle<QCPAxisBase>>);
    static_assert(std::is_same_v<decltype(std::declval<System&>().plottables()),
                                 QList<QCPHandle<QCPAbstractPlottable>>>);

    using Polar = QCPPolarArea;
    static_assert(std::is_same_v<decltype(std::declval<Polar&>().angularAxis()),
                                 QCPHandle<QCPPolarAxisAngular>>);
    static_assert(std::is_same_v<decltype(std::declval<Polar&>().radialAxis(0)),
                                 QCPHandle<QCPPolarAxisRadial>>);
    static_assert(std::is_same_v<decltype(std::declval<Polar&>().radialAxes()),
                                 QList<QCPHandle<QCPPolarAxisRadial>>>);
    static_assert(std::is_same_v<decltype(std::declval<Polar&>().grid()),
                                 QCPHandle<QCPPolarGrid>>);
    // K3.
    static_assert(std::is_same_v<decltype(std::declval<Polar&>().graphs()),
                                 QList<QCPPolarGraph*>>);
    static_assert(std::is_same_v<decltype(std::declval<Polar&>().allAxes()),
                                 QList<QCPHandle<QCPAxisBase>>>);
    static_assert(std::is_same_v<decltype(std::declval<Polar&>().addRadialAxis()),
                                 QCPHandle<QCPPolarAxisRadial>>);
    static_assert(std::is_same_v<decltype(std::declval<QCPPolarGraph&>().angularAxis()),
                                 QCPHandle<QCPPolarAxisAngular>>);
    static_assert(std::is_same_v<decltype(std::declval<QCPPolarGraph&>().radialAxis()),
                                 QCPHandle<QCPPolarAxisRadial>>);
    static_assert(std::is_same_v<decltype(std::declval<QCPPolarGrid&>().radialAxis()),
                                 QCPHandle<QCPPolarAxisRadial>>);
    // K4: the axis area() overrides and the typed polarArea() stay raw (covariance is
    // pointer-only).
    static_assert(std::is_same_v<decltype(std::declval<QCPPolarAxisAngular&>().area()),
                                 QCPPolarArea*>);
    static_assert(std::is_same_v<decltype(std::declval<QCPPolarAxisRadial&>().area()),
                                 QCPPolarArea*>);
    static_assert(
        std::is_same_v<decltype(std::declval<QCPPolarAxisRadial&>().angularAxis()),
                       QCPHandle<QCPPolarAxisAngular>>);

    QCustomPlot plot;
    QCPPolarArea* area = plot.add<QCPPolarArea>(1, 0).get();
    QVERIFY(area);
    QVERIFY(area->angularAxis());
    QVERIFY(area->radialAxis(0));
    QVERIFY(area->grid());
    QCOMPARE(area->radialAxes().size(), 1);
    QCOMPARE(area->angularAxis()->area(), area);
    QCOMPARE(area->radialAxis(0)->area(), area);
    QCOMPARE(area->radialAxis(0)->angularAxis().get(), area->angularAxis().get());
    QCOMPARE(area->grid()->radialAxis().get(), area->radialAxis(0).get());
    QCOMPARE(area->allAxes().size(), 2);
}

void TestHandle::thePlottablesHandOutHandles()
{
    using Plottable = QCPAbstractPlottable;
    // Inventory §2.4: every plot-owned QObject getter returns a handle (K1), lists of handles (K5).
    static_assert(std::is_same_v<decltype(std::declval<Plottable&>().system()),
                                 QCPHandle<QCPCoordinateSystem>>);
    static_assert(std::is_same_v<decltype(std::declval<Plottable&>().area()),
                                 QCPHandle<QCPArea>>);
    static_assert(std::is_same_v<decltype(std::declval<Plottable&>().keyAxis()),
                                 QCPHandle<QCPAxis>>);
    static_assert(std::is_same_v<decltype(std::declval<Plottable&>().valueAxis()),
                                 QCPHandle<QCPAxis>>);
    static_assert(std::is_same_v<decltype(std::declval<Plottable&>().keyAxisBase()),
                                 QCPHandle<QCPAxisBase>>);
    static_assert(std::is_same_v<decltype(std::declval<Plottable&>().valueAxisBase()),
                                 QCPHandle<QCPAxisBase>>);
    static_assert(std::is_same_v<decltype(std::declval<Plottable&>().group()),
                                 QCPHandle<QCPPlottableGroup>>);
    static_assert(std::is_same_v<decltype(std::declval<Plottable&>().dependentLayers()),
                                 QList<QCPHandle<QCPLayer>>>);
    // H-3: the Q_GADGET and the plain interface cannot be handles.
    static_assert(std::is_same_v<decltype(std::declval<Plottable&>().selectionDecorator()),
                                 QCPSelectionDecorator*>);
    static_assert(std::is_same_v<decltype(std::declval<Plottable&>().interface1D()),
                                 QCPPlottableInterface1D*>);

    static_assert(std::is_same_v<decltype(std::declval<QCPErrorBars&>().dataPlottable()),
                                 QCPHandle<QCPAbstractPlottable>>);
    static_assert(std::is_same_v<decltype(std::declval<QCPBars&>().barsGroup()),
                                 QCPHandle<QCPBarsGroup>>);
    static_assert(std::is_same_v<decltype(std::declval<QCPBars&>().barBelow()),
                                 QCPHandle<QCPBars>>);
    static_assert(std::is_same_v<decltype(std::declval<QCPBars&>().barAbove()),
                                 QCPHandle<QCPBars>>);
    static_assert(std::is_same_v<decltype(std::declval<QCPBarsGroup&>().bars(0)),
                                 QCPHandle<QCPBars>>);
    static_assert(std::is_same_v<decltype(std::declval<QCPBarsGroup&>().bars()),
                                 QList<QCPHandle<QCPBars>>>);
    static_assert(std::is_same_v<decltype(std::declval<QCPAbstractColorMapped&>().colorScale()),
                                 QCPHandle<QCPColorScale>>);
    static_assert(std::is_same_v<decltype(std::declval<QCPColorMap&>().data()),
                                 QCPColorMapData*>); // H-3
    static_assert(std::is_same_v<decltype(std::declval<QCPIntervals&>().laneLayout()),
                                 QCPHandle<QCPLaneLayout>>);
    // G-4 (5c): the tracer follows a multigraph component.
    static_assert(std::is_same_v<decltype(std::declval<QCPItemTracer&>().graph()),
                                 QCPHandle<QCPGraph>>);
    static_assert(std::is_same_v<decltype(std::declval<QCPItemTracer&>().setGraph(
                                     std::declval<QCPGraph*>(), 1)),
                                 void>);
    static_assert(std::is_same_v<decltype(std::declval<QCPItemTracerOps<QCPItemTracer>&>().setGraph(
                                     std::declval<QCPHandle<QCPGraph>>(), 1)),
                                 QCPItemTracerOps<QCPItemTracer>>);
    static_assert(std::is_same_v<decltype(std::declval<QCPPlottableGroup&>().parentPlot()),
                                 QCPHandle<QCustomPlot>>);
    static_assert(std::is_same_v<decltype(std::declval<QCPPlottableGroup&>().members()),
                                 QList<QCPHandle<QCPAbstractPlottable>>>);
    static_assert(
        std::is_same_v<decltype(std::declval<QCPPlottableGroupLegendItem&>().group()),
                       QCPHandle<QCPPlottableGroup>>);
    static_assert(std::is_same_v<decltype(std::declval<QCPDataLocator&>().plottable()),
                                 QCPHandle<QCPAbstractPlottable>>);
    static_assert(std::is_same_v<decltype(std::declval<QCPDataLocator&>().hitPlottable()),
                                 QCPHandle<QCPAbstractPlottable>>);

    QCustomPlot plot;
    QCPGraph* graph = plot.addGraph();
    graph->setName("a");
    QCOMPARE(graph->keyAxis().get(), plot.xAxis.get());
    QCOMPARE(graph->valueAxis().get(), plot.yAxis.get());
    QVERIFY(graph->system());
    QCOMPARE(graph->area().get(), static_cast<QCPArea*>(plot.axisRect().get()));
    QCOMPARE(graph->keyAxisBase().get(), static_cast<QCPAxisBase*>(plot.xAxis.get()));

    auto group = plot.add<QCPPlottableGroup>("g");
    QVERIFY(group);
    QVERIFY(group.addMember(QCPHandle<QCPGraph>(graph)));
    auto grouped = plot.add<QCPBars>();
    QVERIFY(group.addMember(grouped));
    QCOMPARE(graph->group().get(), group.get());
    QCOMPARE(group->members().size(), 2);
    QCOMPARE(group->members().first().get(), static_cast<QCPAbstractPlottable*>(graph));
    QCOMPARE(group->parentPlot().get(), &plot);

    QCPErrorBars* errors = new QCPErrorBars(plot.xAxis, plot.yAxis);
    errors->setDataPlottable(graph);
    QCOMPARE(errors->dataPlottable().get(), static_cast<QCPAbstractPlottable*>(graph));

    QCPBarsGroup* barsGroup = new QCPBarsGroup(&plot);
    auto bars = plot.add<QCPBars>();
    bars->setBarsGroup(barsGroup);
    QCOMPARE(bars->barsGroup().get(), barsGroup);
    QCOMPARE(barsGroup->bars(0).get(), bars.get());
    QCOMPARE(barsGroup->bars().size(), 1);
    QCOMPARE(barsGroup->bars().first().get(), bars.get());
    QCOMPARE(barsGroup->bars(-1).get(), nullptr);

    QCPColorScale* scale = new QCPColorScale(&plot);
    QCPColorMap* map = new QCPColorMap(plot.xAxis, plot.yAxis);
    map->setColorScale(scale);
    QCOMPARE(map->colorScale().get(), scale);

    QCPDataLocator locator;
    locator.setPlottable(graph);
    QCOMPARE(locator.plottable().get(), static_cast<QCPAbstractPlottable*>(graph));
    QVERIFY(!locator.hitPlottable());
    QVERIFY(graph->dependentLayers().isEmpty());
    plot.replot();
    QCOMPARE(gMisuses, 0);
}

void TestHandle::theItemsHandOutHandles()
{
    using Item = QCPAbstractItem;
    // Inventory §2.6: the item's clip frame is handles; the Q_GADGET positions and anchors stay raw
    // (H-3), and so do the lists of them.
    static_assert(std::is_same_v<decltype(std::declval<Item&>().clipAxisRect()),
                                 QCPHandle<QCPAxisRect>>);
    static_assert(std::is_same_v<decltype(std::declval<Item&>().clipArea()),
                                 QCPHandle<QCPArea>>);
    static_assert(std::is_same_v<decltype(std::declval<Item&>().positions()),
                                 QList<QCPItemPosition*>>);
    static_assert(std::is_same_v<decltype(std::declval<Item&>().anchors()),
                                 QList<QCPItemAnchor*>>);
    static_assert(std::is_same_v<decltype(std::declval<Item&>().position(QString())),
                                 QCPItemPosition*>);
    static_assert(std::is_same_v<decltype(std::declval<Item&>().anchor(QString())),
                                 QCPItemAnchor*>);
    static_assert(std::is_same_v<decltype(std::declval<Item&>().firstPosition()),
                                 const QCPItemPosition*>);

    using Position = QCPItemPosition;
    static_assert(std::is_same_v<decltype(std::declval<Position&>().keyAxis()),
                                 QCPHandle<QCPAxis>>);
    static_assert(std::is_same_v<decltype(std::declval<Position&>().valueAxis()),
                                 QCPHandle<QCPAxis>>);
    static_assert(std::is_same_v<decltype(std::declval<Position&>().axisRect()),
                                 QCPHandle<QCPAxisRect>>);
    static_assert(std::is_same_v<decltype(std::declval<Position&>().system()),
                                 QCPHandle<QCPCoordinateSystem>>);
    static_assert(std::is_same_v<decltype(std::declval<Position&>().area()),
                                 QCPHandle<QCPArea>>);
    static_assert(std::is_same_v<decltype(std::declval<Position&>().parentAnchor()),
                                 QCPItemAnchor*>);
    static_assert(std::is_same_v<decltype(std::declval<Position&>().parentAnchorX()),
                                 QCPItemAnchor*>);
    static_assert(std::is_same_v<decltype(std::declval<Position&>().parentAnchorY()),
                                 QCPItemAnchor*>);

    QCustomPlot plot;
    auto line = plot.add<QCPItemLine>();
    QVERIFY(line);
    line->setClipAxisRect(plot.axisRect());
    QCOMPARE(line->clipAxisRect().get(), plot.axisRect().get());
    QCOMPARE(line->clipArea().get(), static_cast<QCPArea*>(plot.axisRect().get()));
    QCOMPARE(line->positions().size(), 2);
    QCOMPARE(line->anchors().size(), 2); // every position is also an anchor
    QCPItemPosition* start = line->position("start");
    QVERIFY(start);
    QCOMPARE(start->keyAxis().get(), plot.xAxis.get());
    QCOMPARE(start->valueAxis().get(), plot.yAxis.get());
    QCOMPARE(start->axisRect().get(), plot.axisRect().get());
    QVERIFY(start->system());
    QCOMPARE(start->area().get(), static_cast<QCPArea*>(plot.axisRect().get()));
    QVERIFY(!start->parentAnchor());
    QCOMPARE(line->firstPosition(), start);
    plot.replot();
    QCOMPARE(gMisuses, 0);
}

void TestHandle::everyObjectPropertyReadsThroughQObjectProperty()
{
    QCustomPlot plot;
    QCPAxis* axis = plot.xAxis.get();
    QCPLayer* layer = plot.layer("main").get();
    QCPAxisRect* rect = plot.axisRect().get();
    QCPLegend* legend = plot.legend.get();
    QCPGraph* graph = plot.addGraph();
    graph->setName("a");
    QCPItemTracer* tracer = plot.add<QCPItemTracer>().get();
    tracer->setGraph(graph);
    QCPAbstractItem* item = plot.add<QCPItemLine>().get();
    item->setClipAxisRect(rect);
    QCPColorScale* scale = new QCPColorScale(&plot);
    QCPColorMap* map = new QCPColorMap(plot.xAxis, plot.yAxis);
    map->setColorScale(scale);
    QCPErrorBars* errors = new QCPErrorBars(plot.xAxis, plot.yAxis);
    errors->setDataPlottable(graph);
    QCPBars* bars = new QCPBars(plot.xAxis, plot.yAxis);
    QCPBarsGroup* group = new QCPBarsGroup(&plot);
    bars->setBarsGroup(group);
    QCPBars* below = new QCPBars(plot.xAxis, plot.yAxis);
    QCPBars* above = new QCPBars(plot.xAxis, plot.yAxis);
    above->moveAbove(below);
    QCPAbstractLegendItem* row = legend->item(0).get();
    QVERIFY(row);

    QCOMPARE(plot.property("plotLayout").value<QCPLayoutGrid*>(), plot.plotLayout().get());
    QCOMPARE(axis->property("axisRect").value<QCPAxisRect*>(), axis->axisRect().get());
    QCOMPARE(axis->property("grid").value<QCPGrid*>(), axis->grid().get());
    QCOMPARE(rect->property("layout").value<QCPLayout*>(), rect->layout().get());
    QCOMPARE(layer->property("parentPlot").value<QCustomPlot*>(), layer->parentPlot().get());
    QCOMPARE(layer->property("children").value<QList<QCPLayerable*>>(),
             qcp::pointers(layer->children()));
    QCOMPARE(graph->property("parentPlot").value<QCustomPlot*>(), graph->parentPlot().get());
    QCOMPARE(graph->property("parentLayerable").value<QCPLayerable*>(),
             graph->parentLayerable().get());
    QCOMPARE(graph->property("layer").value<QCPLayer*>(), graph->layer().get());
    QCOMPARE(row->property("parentLegend").value<QCPLegend*>(), row->parentLegend().get());
    QCOMPARE(tracer->property("graph").value<QCPGraph*>(), tracer->graph().get());
    QCOMPARE(item->property("clipAxisRect").value<QCPAxisRect*>(), item->clipAxisRect().get());
    QCOMPARE(map->property("colorScale").value<QCPColorScale*>(), map->colorScale().get());
    QCOMPARE(errors->property("dataPlottable").value<QCPAbstractPlottable*>(),
             errors->dataPlottable().get());
    QCOMPARE(bars->property("barsGroup").value<QCPBarsGroup*>(), bars->barsGroup().get());
    QCOMPARE(below->property("barAbove").value<QCPBars*>(), below->barAbove().get());
    QCOMPARE(above->property("barBelow").value<QCPBars*>(), above->barBelow().get());
    QCOMPARE(graph->property("keyAxis").value<QCPAxis*>(), graph->keyAxis().get());
    QCOMPARE(graph->property("valueAxis").value<QCPAxis*>(), graph->valueAxis().get());
    QCOMPARE(graph->property("selectionDecorator").value<QCPSelectionDecorator*>(),
             graph->selectionDecorator());

    // Writing: the property keeps the pointer type (H-7), so moc's WRITE still takes a raw pointer
    // and setProperty takes one too (opencode M-3). Every writable object property is written here.
    auto writeObject = [](QObject* object, const char* name, const QVariant& value) {
        const int index = object->metaObject()->indexOfProperty(name);
        QVERIFY2(index >= 0, name);
        QVERIFY2(object->metaObject()->property(index).isWritable(), name);
        QVERIFY2(object->setProperty(name, value), name);
        QCOMPARE(object->property(name), value);
    };

    QVERIFY(plot.addLayer("write-test"));
    QCPLayer* overlay = plot.layer("write-test").get();
    writeObject(graph, "layer", QVariant::fromValue(overlay));
    QCOMPARE(graph->layer().get(), overlay);
    writeObject(graph, "layer", QVariant::fromValue(layer));
    QCOMPARE(graph->layer().get(), layer);

    QCPGraph* other = plot.addGraph();
    writeObject(tracer, "graph", QVariant::fromValue(other));
    QCOMPARE(tracer->graph().get(), other);

    auto* otherRect = new QCPAxisRect(&plot);
    plot.plotLayout()->addElement(1, 0, otherRect);
    writeObject(item, "clipAxisRect", QVariant::fromValue(otherRect));
    QCOMPARE(item->clipAxisRect().get(), otherRect);


    auto* otherScale = new QCPColorScale(&plot);
    writeObject(map, "colorScale", QVariant::fromValue(otherScale));
    QCOMPARE(map->colorScale().get(), otherScale);

    writeObject(errors, "dataPlottable", QVariant::fromValue(other));
    QCOMPARE(errors->dataPlottable().get(), other);

    auto* otherGroup = new QCPBarsGroup(&plot);
    writeObject(bars, "barsGroup", QVariant::fromValue(otherGroup));
    QCOMPARE(bars->barsGroup().get(), otherGroup);

    writeObject(graph, "selectionDecorator", QVariant::fromValue(new QCPSelectionDecorator));
    QVERIFY(graph->selectionDecorator());

    // The axes stay where they are: a pair of another area would be refused (and reported), and
    // these two properties are the same write path as the others.
    writeObject(graph, "keyAxis", QVariant::fromValue(graph->keyAxis().get()));
    writeObject(graph, "valueAxis", QVariant::fromValue(graph->valueAxis().get()));

    // What has no setter stays read-only, so no write path can reach it.
    const QMetaObject* layerMeta = layer->metaObject();
    QVERIFY(!layerMeta->property(layerMeta->indexOfProperty("children")).isWritable());
    const QMetaObject* plotMeta = plot.metaObject();
    QVERIFY(!plotMeta->property(plotMeta->indexOfProperty("plotLayout")).isWritable());
    QCOMPARE(gMisuses, 0);
}

void TestHandle::handleAsCast()
{
    QCustomPlot plot;
    QCPHandle<QCPAbstractPlottable> h(plot.addGraph());
    QVERIFY(h.as<QCPGraph>());
    QVERIFY(!h.as<QCPCurve>());
    QCPHandle<QCPAbstractPlottable> dead;
    QVERIFY(!dead.as<QCPGraph>());
}

void TestHandle::handleRemove()
{
    QCustomPlot plot;
    QCPHandle<QCPGraph> h(plot.addGraph());
    QCOMPARE(plot.plottableCount(), 1);
    h.remove();
    QVERIFY(!h);
    QCOMPARE(plot.plottableCount(), 0);
    h.remove();
    QCOMPARE(plot.plottableCount(), 0);
}

void TestHandle::deadHandleReportsMisuse()
{
    QCustomPlot plot;
    QCPHandle<QCPGraph> h(plot.addGraph());
    QVERIFY(h.checkAlive());
    QCOMPARE(gMisuses, 0);
    delete h.get();
    QVERIFY(!h.checkAlive());
    QCOMPARE(gMisuses, 1);
}

void TestHandle::removeAxisThroughHandle()
{
    QCustomPlot plot;
    QCPAxisRect* rect = plot.axisRect();
    const int before = rect->axisCount(QCPAxis::atRight);
    auto axis = rect->add<QCPAxis>(QCPAxis::atRight);
    QPointer<QCPAxis> tracked = axis.get();
    QCOMPARE(rect->axisCount(QCPAxis::atRight), before + 1);
    axis.remove();
    QVERIFY(!axis);
    QVERIFY(!tracked);
    QCOMPARE(rect->axisCount(QCPAxis::atRight), before);
    plot.replot();
    QCOMPARE(gMisuses, 0);
}

void TestHandle::removeLayerThroughHandle()
{
    QCustomPlot plot;
    plot.addLayer("extra", plot.layer("main"));
    auto graph = plot.add<QCPGraph>();
    QCPLayer* extra = plot.layer("extra");
    QVERIFY(extra);
    graph->setLayer(extra);
    QCOMPARE(graph->layer(), extra);

    QCPHandle<QCPLayer> layer(extra);
    layer.remove();
    QVERIFY(!layer);
    QVERIFY(!plot.layer("extra"));
    QVERIFY(graph);
    QVERIFY(graph->layer() != extra);
    plot.replot();
    QCOMPARE(gMisuses, 0);

    // the plot keeps one layer: removing it is refused and the layer stays
    while (plot.layerCount() > 1)
        QVERIFY(plot.removeLayer(plot.layer(plot.layerCount() - 1)));
    QCOMPARE(plot.layerCount(), 1);
    QCPLayer* last = plot.layer(0);
    QCPHandle<QCPLayer> lastHandle(last);
    lastHandle.remove();
    QCOMPARE(gMisuses, 1);
    QVERIFY(lastHandle);
    QCOMPARE(plot.layerCount(), 1);
    QVERIFY(plot.layer(0) == last);

    // the plot API refuses it the same way: the rule lives in removeLayer, not in the handle
    QVERIFY(!plot.removeLayer(last));
    QCOMPARE(gMisuses, 2);
    QCOMPARE(plot.layerCount(), 1);
    plot.replot();
}

void TestHandle::removeAxisRectThroughHandle()
{
    QCustomPlot plot;
    auto rect = plot.add<QCPAxisRect>(1, 0);
    QVERIFY(rect);
    QPointer<QCPAxis> axis(rect->axis(QCPAxis::atBottom));
    auto graph = plot.add<QCPGraph>();
    graph->setKeyAxis(rect->axis(QCPAxis::atBottom));
    graph->setValueAxis(rect->axis(QCPAxis::atLeft));
    QCOMPARE(graph->keyAxis(), rect->axis(QCPAxis::atBottom));

    rect.remove();
    QVERIFY(!rect);
    QVERIFY(!axis);
    QVERIFY(!plot.plotLayout()->element(1, 0));
    QVERIFY(graph);
    QVERIFY(!graph->keyAxis());
    QCOMPARE(gMisuses, 0);

    graph.remove();
    plot.replot();
}

void TestHandle::noRemoveOnWidgets()
{
    // A requires-expression on the call itself (`requires(QCPHandle<QCustomPlot> h) { h.remove(); }`)
    // is a hard error on GCC and Clang, not false: a failed constraint is not SFINAE-friendly inside
    // a requires-expression. The concept behind remove() answers the same question.
    static_assert(!qcp::Removable<QCustomPlot>);
    static_assert(!qcp::Removable<QCPLayerable>);
    static_assert(qcp::Removable<QCPGraph>);
    static_assert(qcp::Removable<QCPAxis>);
    static_assert(qcp::Removable<QCPLayer>);
    static_assert(qcp::Removable<QCPAxisRect>);
    static_assert(qcp::Removable<QCPTextElement>);
    static_assert(qcp::Removable<QCPItemLine>);
}

void TestHandle::removeLooseLayoutElement()
{
    QCustomPlot plot;
    QCPHandle<QCPTextElement> loose(new QCPTextElement(&plot, "loose"));
    QVERIFY(loose);
    loose.remove();
    QVERIFY(!loose);
    QCOMPARE(gMisuses, 0);
}

void TestHandle::removeLastAxisRectThenAddIsRefused()
{
    QCustomPlot plot;
    QCPHandle<QCPAxisRect> rect(plot.axisRect());
    rect.remove();
    QVERIFY(!rect);
    QCOMPARE(gMisuses, 0);
    QVERIFY(!plot.axisRect());
    QVERIFY(!plot.add<QCPGraph>());
    QCOMPARE(gMisuses, 1);
}

void TestHandle::axisAddedOnThePlotStacksOnItsSide()
{
    QCustomPlot plot;
    QCPAxisRect* rect = plot.axisRect();
    QCPAxis* first = rect->axis(QCPAxis::atLeft);
    auto a = plot.add<QCPAxis>(QCPAxis::atLeft).setLabel("B").setColor(Qt::red);
    QVERIFY(a);
    QCOMPARE(rect->axisCount(QCPAxis::atLeft), 2);
    QCOMPARE(a->label(), QStringLiteral("B"));
    QCOMPARE(a->labelColor(), QColor(Qt::red));
    QCOMPARE(a->basePen().color(), QColor(Qt::red));
    QCOMPARE(a->tickLabelColor(), QColor(Qt::red));
    plot.replot();
    QVERIFY2(a->offset() > first->offset(), "the second left axis stacks outside the first");
    QCOMPARE(gMisuses, 0);
}

void TestHandle::axisAddedWithoutARectIsRefused()
{
    QCustomPlot plot;
    plot.plotLayout()->clear();
    QVERIFY(!plot.axisRect());
    QVERIFY(!plot.add<QCPAxis>(QCPAxis::atLeft));
    QCOMPARE(gMisuses, 1);
}

void TestHandle::colorKeepsPenWidthAndStyle()
{
    QCustomPlot plot;
    auto handle = plot.add<QCPAxis>(QCPAxis::atLeft);
    QVERIFY(handle);
    handle->setBasePen(QPen(Qt::black, 3, Qt::DashLine));
    handle.setColor(Qt::blue);
    QCOMPARE(handle->basePen().color(), QColor(Qt::blue));
    QCOMPARE(handle->basePen().width(), 3);
    QCOMPARE(handle->basePen().style(), Qt::DashLine);
}

void TestHandle::aNullAxisInARangeListIsRefused()
{
    QCustomPlot plot;
    plot.axisRect()->setRangeDragAxes(QList<QCPAxis*> {nullptr});
    QCOMPARE(gMisuses, 1);
    plot.axisRect()->setRangeZoomAxes(QList<QCPAxis*> {nullptr});
    QCOMPARE(gMisuses, 2);
}
