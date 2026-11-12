#include "test-handle.h"
#include <QDebug>
#include <QSet>
#include <functional>
#include <memory>
#include <type_traits>
#include <unordered_set>

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
    QPointer<QCPAxis> axis = rect->axis(QCPAxis::atBottom);
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
