#include "test-meta-surface.h"
#include "../misuse-recorder.h"
#include "datasource/async-pipeline.h"
#include "datasource/graph-resampler.h"
#include "datasource/pipeline-scheduler.h"
#include "item-creation-state.h"
#include <QBindable>
#include <QProperty>
#include <memory>

// The spike (milestone 5e, Task 1): three plain properties, an axis label, a plottable pen and an
// axis scale type. Each is a Q_PROPERTY with a NOTIFY emitted through qcpEmit; bindings go through
// QBindable(obj, "name"), whose adaptor hangs off that signal.

namespace
{
std::unique_ptr<QCustomPlot> newPlot()
{
    auto plot = std::make_unique<QCustomPlot>();
    plot->resize(400, 300);
    plot->xAxis->setRange(0, 10);
    plot->yAxis->setRange(0, 10);
    return plot;
}

void settle(QCustomPlot& plot)
{
    plot.replot();
    QTRY_VERIFY(!plot.isRepaintQueued());
}

QCPGraph* graphOn(QCustomPlot& plot)
{
    QCPGraph* graph = plot.addGraph();
    graph->setData(QVector<double> { 0, 5, 10 }, QVector<double> { 1, 6, 2 });
    return graph;
}

QCPGraph* twoColumnGraphOn(QCustomPlot& plot)
{
    QCPGraph* graph = plot.addGraph();
    graph->setData(QVector<double> { 0, 5, 10 },
                   std::vector<QVector<double>> { { 1, 6, 2 }, { 3, 2, 5 } });
    return graph;
}

// A value column and its two error bounds.
QCPGraph* threeColumnGraphOn(QCustomPlot& plot)
{
    QCPGraph* graph = plot.addGraph();
    graph->setData(QVector<double> { 0, 5, 10 },
                   std::vector<QVector<double>> { { 1, 6, 2 }, { 0, 5, 1 }, { 2, 7, 3 } });
    return graph;
}

QCPIntervals* intervalsOn(QCustomPlot& plot)
{
    auto* layout = new QCPLaneLayout(&plot);
    auto* intervals = plot.add<QCPIntervals>(layout).get();
    qcp::intervals::Columns columns;
    columns.start = { 10, 60 };
    columns.stop = { 40, 90 };
    columns.lane
        = { layout->laneIndex(QStringLiteral("A")), layout->laneIndex(QStringLiteral("B")) };
    columns.category = { 0, 1 };
    columns.ids = { 1, 2 };
    intervals->setData(std::move(columns));
    intervals->setSelectable(QCP::stMultipleDataRanges);
    return intervals;
}

// A histogram whose binning has settled: the next rebinning makes it busy again.
QCPHistogram* settledHistogramOn(QCustomPlot& plot)
{
    using Source = QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>;
    auto* histogram = plot.add<QCPHistogram>().get();
    histogram->setDataSource(std::make_shared<Source>(
        std::vector<double> { 0, 1, 2 }, std::vector<std::vector<double>> { { 1, 2, 3 } }));
    QTest::qWaitFor([histogram] { return !histogram->busy(); }, 5000);
    return histogram;
}

QCPWaterfallGraph* settledWaterfallOn(QCustomPlot& plot)
{
    // Enough points for the resampling pipeline: a new source makes the graph busy again.
    const int count = qcp::algo::kResampleThreshold;
    QVector<double> keys(count), first(count), second(count);
    for (int i = 0; i < count; ++i)
    {
        keys[i] = i;
        first[i] = i % 7;
        second[i] = i % 5;
    }
    auto* waterfall = plot.add<QCPWaterfallGraph>().get();
    waterfall->setData(keys, std::vector<QVector<double>> { first, second });
    QTest::qWaitFor([waterfall] { return !waterfall->busy(); }, 5000);
    return waterfall;
}

QCPColorMap2* settledColorMap2On(QCustomPlot& plot)
{
    auto* map = plot.add<QCPColorMap2>().get();
    map->setData(std::vector<double> { 0, 1, 2 }, std::vector<double> { 0, 1, 2 },
                 std::vector<double>(9, 1.0));
    QTest::qWaitFor([map] { return !map->busy(); }, 5000);
    return map;
}

// The row's plottable, armed on its busy signal: a pipeline restart makes it busy again.
template <typename T>
std::pair<QObject*, const char*> armedOnBusy(T* plottable)
{
    return { plottable, "busyChanged(bool)" };
}

template <typename T>
T* firstPlottable(QCustomPlot& plot)
{
    return qobject_cast<T*>(plot.plottable(0).get());
}

QCPPlottableGroup* groupOfOneOn(QCustomPlot& plot)
{
    QCPPlottableGroup* group = plot.addPlottableGroup(QStringLiteral("group")).get();
    group->addMember(graphOn(plot));
    return group;
}

// Two legend rows: removing one collapses a row, which the legend grid signals.
std::pair<QObject*, const char*> armedOnTheLegendOfTwo(QCustomPlot& plot)
{
    graphOn(plot);
    graphOn(plot);
    return { plot.legend.get(), "rowStretchFactors" };
}

QCPCurve* curveOn(QCustomPlot& plot)
{
    auto* curve = plot.add<QCPCurve>().get();
    curve->setData(QVector<double> { 0, 5, 10 }, QVector<double> { 1, 6, 2 });
    return curve;
}

QCPBars* barsOn(QCustomPlot& plot)
{
    auto* bars = plot.add<QCPBars>().get();
    bars->setData(QVector<double> { 1, 5, 9 }, QVector<double> { 3, 6, 2 });
    return bars;
}

QCPTextElement* textOn(QCustomPlot& plot)
{
    auto* text = new QCPTextElement(&plot, QStringLiteral("title"));
    plot.plotLayout()->addElement(1, 0, text);
    return text;
}

QCPColorScale* colorScaleOn(QCustomPlot& plot)
{
    auto* scale = new QCPColorScale(&plot);
    plot.plotLayout()->addElement(0, 1, scale);
    return scale;
}

template <typename Item>
QObject* itemOn(QCustomPlot& plot)
{
    return plot.add<Item>().get();
}

QCPPolarArea* polarAreaOn(QCustomPlot& plot)
{
    auto* area = new QCPPolarArea(&plot);
    plot.plotLayout()->addElement(3, 0, area);
    return area;
}

// A polar graph on the area's second radial axis.
QCPPolarGraph* polarGraphOnASecondRadialAxis(QCustomPlot& plot)
{
    QCPPolarArea* area = polarAreaOn(plot);
    QCPPolarAxisRadial* radial = area->addRadialAxis().get();
    auto* graph = area->add<QCPPolarGraph>().get();
    // add<> binds the area's first radial axis; this graph is about the second one.
    graph->setSystem(area->system(area->angularAxis(), radial));
    return graph;
}

QCPAxisRect* axisRectOn(QCustomPlot& plot)
{
    auto* rect = new QCPAxisRect(&plot);
    plot.plotLayout()->addElement(2, 0, rect);
    return rect;
}

void removeAxis(QObject* object)
{
    auto* axis = qobject_cast<QCPAxis*>(object);
    QVERIFY(axis->axisRect()->removeAxis(axis));
}

void removePlottable(QObject* object)
{
    auto* plottable = qobject_cast<QCPPlottable*>(object);
    QVERIFY(plottable->parentPlot()->removePlottable(plottable));
}

template <typename T>
QUntypedBindable bindableOf(QObject* object, const char* property)
{
    return QBindable<T>(object, property);
}

// The safe form of a binding that reads another object (review I10): the source is held by a
// QPointer and read through a fresh QBindable each time, so a dead source is never touched.
template <typename T>
void bindGuarded(QObject* target, QObject* source, const char* property, QProperty<int>& tick)
{
    const T fallback = QBindable<T>(target, property).value();
    const QPointer<QObject> alive(source);
    QBindable<T>(target, property).setBinding([alive, property, &tick, fallback] {
        tick.value();
        return alive ? QBindable<T>(alive.data(), property).value() : fallback;
    });
}

struct SpikeCase
{
    const char* name;
    const char* property;
    // The object under test and a second one of its kind.
    std::function<std::pair<QObject*, QObject*>(QCustomPlot&)> make;
    std::function<void(QObject*)> remove;
    QVariant other; // accepted by the setter, different from the default
    QUntypedBindable (*bindable)(QObject*, const char*);
    void (*bindToSource)(QObject*, QObject*, const char*, QProperty<int>&);
};

const QVector<SpikeCase>& spikeCases()
{
    static const QVector<SpikeCase> cases {
        { "axis label", "label",
          [](QCustomPlot& plot) { return std::pair<QObject*, QObject*>(plot.xAxis, plot.yAxis); },
          removeAxis, QVariant(QStringLiteral("time (s)")), bindableOf<QString>,
          bindGuarded<QString> },
        { "graph pen", "pen",
          [](QCustomPlot& plot) {
              return std::pair<QObject*, QObject*>(graphOn(plot), graphOn(plot));
          },
          removePlottable, QVariant(QPen(Qt::blue, 3)), bindableOf<QPen>, bindGuarded<QPen> },
        { "curve pen", "pen",
          [](QCustomPlot& plot) {
              return std::pair<QObject*, QObject*>(curveOn(plot), curveOn(plot));
          },
          removePlottable, QVariant(QPen(Qt::blue, 3)), bindableOf<QPen>, bindGuarded<QPen> },
        { "bars pen", "pen",
          [](QCustomPlot& plot) {
              return std::pair<QObject*, QObject*>(barsOn(plot), barsOn(plot));
          },
          removePlottable, QVariant(QPen(Qt::blue, 3)), bindableOf<QPen>, bindGuarded<QPen> },
        { "axis scale", "scale",
          [](QCustomPlot& plot) { return std::pair<QObject*, QObject*>(plot.xAxis, plot.yAxis); },
          removeAxis, QVariant::fromValue(QCPScale::log()),
          bindableOf<QCPScale>, bindGuarded<QCPScale> },
    };
    return cases;
}

void addSpikeRows()
{
    QTest::addColumn<int>("index");
    for (int i = 0; i < spikeCases().size(); ++i)
        QTest::newRow(spikeCases()[i].name) << i;
}

void addSpikeRowsWithTiming()
{
    QTest::addColumn<int>("index");
    QTest::addColumn<bool>("deferred");
    for (int i = 0; i < spikeCases().size(); ++i)
        for (bool deferred : { false, true })
            QTest::addRow("%s, %s", spikeCases()[i].name, deferred ? "deferred" : "at once")
                << i << deferred;
}

QMetaProperty metaProperty(const QObject* object, const char* name)
{
    return object->metaObject()->property(object->metaObject()->indexOfProperty(name));
}

// Writes \a value as a binding, a slot or the user's code would: through the WRITE setter.
void write(QObject* object, const char* name, const QVariant& value)
{
    QVERIFY2(metaProperty(object, name).write(object, value), name);
}

// The write, inside an open delivery scope of the plot when \a deferred.
void writeMaybeDeferred(QCustomPlot* plot, QObject* object, const char* name,
                        const QVariant& value, bool deferred)
{
    if (!deferred)
        return write(object, name, value);
    QCPEmissionScope scope(plot);
    write(object, name, value);
}

// \a property names a property, whose NOTIFY fires the trigger, or a signal ("changed()").
void connectTrigger(QObject* sender, const char* property, MetaTrigger* trigger)
{
    const QMetaObject* meta = sender->metaObject();
    const QMetaMethod notify = meta->indexOfProperty(property) >= 0
        ? metaProperty(sender, property).notifySignal()
        : meta->method(meta->indexOfSignal(QMetaObject::normalizedSignature(property)));
    const QMetaMethod fire =
        trigger->metaObject()->method(trigger->metaObject()->indexOfSlot("fire()"));
    QVERIFY(QObject::connect(sender, notify, trigger, fire));
}
} // namespace

void TestMetaSurface::init()
{
    qcp::setMisuseHandler(test_misuse::record);
    test_misuse::calls.clear();
}

void TestMetaSurface::cleanup()
{
    for (const test_misuse::Call& call : test_misuse::calls)
        qWarning("unexpected misuse: %s: %s", call.where.c_str(), call.what.c_str());
    QVERIFY(test_misuse::calls.empty());
    qcp::setMisuseHandler(nullptr);
}

void TestMetaSurface::spikeSignalIsDeferredAndFiresOnce_data() { addSpikeRows(); }

void TestMetaSurface::spikeSignalIsDeferredAndFiresOnce()
{
    QFETCH(int, index);
    const SpikeCase& c = spikeCases()[index];
    auto plot = newPlot();
    QObject* target = c.make(*plot).first;
    QSignalSpy spy(target, metaProperty(target, c.property).notifySignal());
    QVERIFY(spy.isValid());
    {
        QCPEmissionScope scope(plot.get());
        write(target, c.property, c.other);
        QCOMPARE(spy.count(), 0);
    }
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first(), c.other);
    QCOMPARE(target->property(c.property), c.other);

    write(target, c.property, c.other);
    QCOMPARE(spy.count(), 1);
}

void TestMetaSurface::spikeSubscriberSeesTheChangeAfterDelivery_data() { addSpikeRows(); }

void TestMetaSurface::spikeSubscriberSeesTheChangeAfterDelivery()
{
    QFETCH(int, index);
    const SpikeCase& c = spikeCases()[index];
    auto plot = newPlot();
    QObject* target = c.make(*plot).first;
    QUntypedBindable bindable = c.bindable(target, c.property);
    QVERIFY(bindable.isValid());
    int notified = 0;
    QVariant seen;
    const QPropertyNotifier notifier = bindable.addNotifier([&] {
        ++notified;
        seen = target->property(c.property);
    });
    {
        QCPEmissionScope scope(plot.get());
        write(target, c.property, c.other);
        QCOMPARE(notified, 0);
    }
    QCOMPARE(notified, 1);
    QCOMPARE(seen, c.other);

    write(target, c.property, c.other);
    QCOMPARE(notified, 1);
}

void TestMetaSurface::spikeBindingDrivesTheSetterAndRepaints_data() { addSpikeRows(); }

// A binding writes through the setter, so the result is the plot a direct call draws.
void TestMetaSurface::spikeBindingDrivesTheSetterAndRepaints()
{
    QFETCH(int, index);
    const SpikeCase& c = spikeCases()[index];
    auto bound = newPlot();
    auto [target, source] = c.make(*bound);
    QVERIFY(c.bindable(target, c.property).setBinding(c.bindable(source, c.property).makeBinding()));
    settle(*bound);
    QSignalSpy spy(target, metaProperty(target, c.property).notifySignal());

    write(source, c.property, c.other);
    QCOMPARE(target->property(c.property), c.other);
    QCOMPARE(spy.count(), 1);
    QVERIFY2(bound->isRepaintQueued(), "the binding changed the value and scheduled no repaint");
    QVERIFY(c.bindable(target, c.property).hasBinding());

    auto called = newPlot();
    auto [calledTarget, calledSource] = c.make(*called);
    write(calledSource, c.property, c.other);
    write(calledTarget, c.property, c.other);
    QCOMPARE(bound->toPixmap(400, 300).toImage(), called->toPixmap(400, 300).toImage());
}

void TestMetaSurface::spikeSlotDeletingTheObject_data() { addSpikeRowsWithTiming(); }

void TestMetaSurface::spikeSlotDeletingTheObject()
{
    QFETCH(int, index);
    QFETCH(bool, deferred);
    const SpikeCase& c = spikeCases()[index];
    auto plot = newPlot();
    QObject* target = c.make(*plot).first;
    const QPointer<QObject> alive(target);
    MetaTrigger deleter, later;
    int laterRuns = 0;
    deleter.action = [&] { c.remove(target); };
    later.action = [&] { ++laterRuns; };
    connectTrigger(target, c.property, &deleter);
    connectTrigger(target, c.property, &later);

    writeMaybeDeferred(plot.get(), target, c.property, c.other, deferred);

    QVERIFY(!alive);
    QCOMPARE(laterRuns, 0); // no further signal from a deleted sender
    settle(*plot);
}

void TestMetaSurface::spikeObserverDeletingTheObject_data() { addSpikeRowsWithTiming(); }

void TestMetaSurface::spikeObserverDeletingTheObject()
{
    QFETCH(int, index);
    QFETCH(bool, deferred);
    const SpikeCase& c = spikeCases()[index];
    auto plot = newPlot();
    QObject* target = c.make(*plot).first;
    const QPointer<QObject> alive(target);
    QUntypedBindable bindable = c.bindable(target, c.property);
    const QPropertyNotifier notifier = bindable.addNotifier([&] { c.remove(target); });

    writeMaybeDeferred(plot.get(), target, c.property, c.other, deferred);

    QVERIFY(!alive);
    settle(*plot);
}

void TestMetaSurface::spikeSlotDeletingThePlot_data() { addSpikeRowsWithTiming(); }

void TestMetaSurface::spikeSlotDeletingThePlot()
{
    QFETCH(int, index);
    QFETCH(bool, deferred);
    const SpikeCase& c = spikeCases()[index];
    QCustomPlot* plot = newPlot().release();
    const QPointer<QCustomPlot> alive(plot);
    QObject* target = c.make(*plot).first;
    MetaTrigger deleter;
    deleter.action = [plot] { delete plot; };
    connectTrigger(target, c.property, &deleter);

    writeMaybeDeferred(plot, target, c.property, c.other, deferred);

    QVERIFY(!alive);
}

void TestMetaSurface::spikeObserverDeletingThePlot_data() { addSpikeRowsWithTiming(); }

void TestMetaSurface::spikeObserverDeletingThePlot()
{
    QFETCH(int, index);
    QFETCH(bool, deferred);
    const SpikeCase& c = spikeCases()[index];
    QCustomPlot* plot = newPlot().release();
    const QPointer<QCustomPlot> alive(plot);
    QObject* target = c.make(*plot).first;
    QUntypedBindable bindable = c.bindable(target, c.property);
    const QPropertyNotifier notifier = bindable.addNotifier([plot] { delete plot; });

    writeMaybeDeferred(plot, target, c.property, c.other, deferred);

    QVERIFY(!alive);
}

void TestMetaSurface::spikeBindingWhoseSourceDiesFirst_data() { addSpikeRows(); }

// Review I10: a binding that captured QBindable(source, "x") by value would read freed memory on
// its next evaluation once the source is gone. The guarded form (QPointer, a fresh QBindable per
// evaluation) falls back instead; docs/migrating.md states the rule.
void TestMetaSurface::spikeBindingWhoseSourceDiesFirst()
{
    QFETCH(int, index);
    const SpikeCase& c = spikeCases()[index];
    auto plot = newPlot();
    auto [target, source] = c.make(*plot);
    const QVariant before = target->property(c.property);
    QProperty<int> tick(0);
    c.bindToSource(target, source, c.property, tick);

    write(source, c.property, c.other);
    QCOMPARE(target->property(c.property), c.other);

    c.remove(source);
    tick = 1; // another dependency changes: the binding evaluates with its source gone
    QCOMPARE(target->property(c.property), before);
    QVERIFY(c.bindable(target, c.property).hasBinding());
    settle(*plot);
}

// Review M6: the polar angular axis refuses a logarithmic scale. A binding that asks for one
// leaves the value, sends no signal, and stays installed.
void TestMetaSurface::bindingToARefusedScaleTypeKeepsTheValue()
{
    auto plot = newPlot();
    auto* area = new QCPPolarArea(plot.get());
    plot->plotLayout()->addElement(0, 1, area);
    QCPPolarAxisAngular* angular = area->angularAxis();
    QBindable<QCPScale> scale(angular, "scale");
    QProperty<QCPScale> wanted(QCPScale::linear());
    scale.setBinding([&wanted] { return wanted.value(); });
    QSignalSpy spy(angular, &QCPAxisBase::scaleChanged);

    wanted = QCPScale::log();

    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    test_misuse::calls.clear(); // the refusal is the point of this test
    QCOMPARE(angular->scale(), QCPScale::linear());
    QCOMPARE(scale.value(), QCPScale::linear());
    QCOMPARE(spy.count(), 0);
    QVERIFY(scale.hasBinding());

    wanted = QCPScale::linear();
    QCOMPARE(spy.count(), 0);
    QVERIFY(test_misuse::calls.empty());
}

// Review M7: Qt's adaptor keeps its binding through a direct setter call; the next change of
// the source overwrites the direct value. (A storage property would drop the binding instead.)
void TestMetaSurface::plainWriteDoesNotBreakTheBinding()
{
    auto plot = newPlot();
    QBindable<QString> label(plot->xAxis, "label");
    QProperty<QString> wanted(QStringLiteral("bound"));
    label.setBinding([&wanted] { return wanted.value(); });
    QCOMPARE(plot->xAxis->label(), QStringLiteral("bound"));

    plot->xAxis->setLabel(QStringLiteral("direct"));
    QCOMPARE(plot->xAxis->label(), QStringLiteral("direct"));
    QVERIFY(label.hasBinding());

    wanted = QStringLiteral("bound again");
    QCOMPARE(plot->xAxis->label(), QStringLiteral("bound again"));
}

// Review M5: the curve's setPen forwards to the line family's: one penChanged, not two.
void TestMetaSurface::curvePenNotifiesOnce()
{
    auto plot = newPlot();
    QCPCurve* curve = curveOn(*plot);
    QSignalSpy spy(curve, &QCPPlottable::penChanged);
    curve->setPen(QPen(Qt::green, 2));
    QCOMPARE(spy.count(), 1);
    curve->setPen(QPen(Qt::green, 2));
    QCOMPARE(spy.count(), 1);
}

// QCPLine::setPen repaints when only a component's pen differs (plottable-line.cpp), but
// pen() does not move: no penChanged.
void TestMetaSurface::componentRepenRepaintsWithoutPenChanged()
{
    auto plot = newPlot();
    QCPGraph* graph = graphOn(*plot);
    graph->setPen(QPen(Qt::red));
    graph->setComponentPens({ QPen(Qt::green) });
    settle(*plot);
    QSignalSpy spy(graph, &QCPPlottable::penChanged);

    graph->setPen(QPen(Qt::red));

    QCOMPARE(spy.count(), 0);
    QVERIFY(plot->isRepaintQueued());
    QCOMPARE(graph->component(0).pen, QPen(Qt::red));
}

// ---------------------------------------------------------------------------------------------
// The generic test (E-16). Three tables, all data: the fixtures (a class and its factory on a
// real plot), the "different value" per property type, and the setters that refuse, clamp, trim
// or take ownership, each checked by its own rule. Every NOTIFY property of every fixture gets
// one row; Tasks 2 and 3 extend the fixtures, the tables grow with the types they bring.
// ---------------------------------------------------------------------------------------------
namespace
{
struct Fixture
{
    const char* name;
    std::function<QObject*(QCustomPlot&)> make;
    bool deferred = true; // the theme emits at once (E-5): it has no plot to defer through
};

const QVector<Fixture>& fixtures()
{
    static const QVector<Fixture> list {
        { "QCPAxis", [](QCustomPlot& plot) -> QObject* { return plot.xAxis; } },
        { "QCustomPlot", [](QCustomPlot& plot) -> QObject* { return &plot; } },
        { "QCPLayer", [](QCustomPlot& plot) -> QObject* { return plot.layer("main").get(); } },
        { "QCPTheme", [](QCustomPlot& plot) -> QObject* { return plot.theme().get(); }, false },
        { "QCPGrid", [](QCustomPlot& plot) -> QObject* { return plot.xAxis->grid().get(); } },
        { "QCPLayoutGrid", [](QCustomPlot& plot) -> QObject* { return plot.plotLayout().get(); } },
        { "QCPAxisRect", [](QCustomPlot& plot) -> QObject* { return plot.axisRect().get(); } },
        // With a row: an empty legend has no stretch factor to change.
        { "QCPLegend",
          [](QCustomPlot& plot) -> QObject*
          {
              graphOn(plot);
              return plot.legend.get();
          } },
        { "QCPPlottableLegendItem",
          [](QCustomPlot& plot) -> QObject*
          {
              graphOn(plot);
              return plot.legend->item(0).get();
          } },
        { "QCPTextElement", [](QCustomPlot& plot) -> QObject* { return textOn(plot); } },
        { "QCPColorScale", [](QCustomPlot& plot) -> QObject* { return colorScaleOn(plot); } },
        { "QCPGraph", [](QCustomPlot& plot) -> QObject* { return graphOn(plot); } },
        { "QCPCurve", [](QCustomPlot& plot) -> QObject* { return curveOn(plot); } },
        { "QCPBars", [](QCustomPlot& plot) -> QObject* { return barsOn(plot); } },
        { "QCPBarsGroup", [](QCustomPlot& plot) -> QObject* { return new QCPBarsGroup(&plot); } },
        { "QCPPolarArea", [](QCustomPlot& plot) -> QObject* { return polarAreaOn(plot); } },
        { "QCPPolarAxisAngular",
          [](QCustomPlot& plot) -> QObject* { return polarAreaOn(plot)->angularAxis().get(); } },
        { "QCPPolarAxisRadial",
          [](QCustomPlot& plot) -> QObject* { return polarAreaOn(plot)->radialAxis().get(); } },
        { "QCPPolarGrid",
          [](QCustomPlot& plot) -> QObject* { return polarAreaOn(plot)->grid().get(); } },
        { "QCPPolarGraph",
          [](QCustomPlot& plot) -> QObject*
          {
              QCPPolarArea* area = polarAreaOn(plot);
              return area->add<QCPPolarGraph>().get();
          } },
        { "QCPOverlay", [](QCustomPlot& plot) -> QObject* { return plot.overlay().get(); } },
        { "QCPSelectionRect",
          [](QCustomPlot& plot) -> QObject* { return plot.selectionRect().get(); } },
        { "QCPItemBracket", itemOn<QCPItemBracket> },
        { "QCPItemCurve", itemOn<QCPItemCurve> },
        { "QCPItemEllipse", itemOn<QCPItemEllipse> },
        { "QCPItemLine", itemOn<QCPItemLine> },
        { "QCPItemPixmap", itemOn<QCPItemPixmap> },
        { "QCPItemRect", itemOn<QCPItemRect> },
        { "QCPItemRichText", itemOn<QCPItemRichText> },
        { "QCPItemRSpan", itemOn<QCPItemRSpan> },
        { "QCPItemSpan",
          [](QCustomPlot& plot) -> QObject* { return plot.add<QCPItemSpan>(Qt::Vertical).get(); } },
        { "QCPItemStraightLine", itemOn<QCPItemStraightLine> },
        { "QCPItemText", itemOn<QCPItemText> },
        { "QCPItemTracer", itemOn<QCPItemTracer> },
        { "QCPColorMap",
          [](QCustomPlot& plot) -> QObject* { return plot.add<QCPColorMap>().get(); } },
        { "QCPErrorBars",
          [](QCustomPlot& plot) -> QObject*
          {
              auto* errors = plot.add<QCPErrorBars>().get();
              errors->setDataPlottable(graphOn(plot));
              return errors;
          } },
        { "QCPFinancial",
          [](QCustomPlot& plot) -> QObject* { return plot.add<QCPFinancial>().get(); } },
        { "QCPStatisticalBox", [](QCustomPlot& plot) -> QObject*
          { return plot.add<QCPStatisticalBox>().get(); } },
        { "QCPColorMap2",
          [](QCustomPlot& plot) -> QObject* { return plot.add<QCPColorMap2>().get(); } },
        { "QCPHistogram",
          [](QCustomPlot& plot) -> QObject* { return plot.add<QCPHistogram>().get(); } },
        { "QCPHistogram2D", [](QCustomPlot& plot) -> QObject*
          { return plot.add<QCPHistogram2D>().get(); } },
        { "QCPIntervals", [](QCustomPlot& plot) -> QObject* { return intervalsOn(plot); } },
        { "QCPWaterfallGraph", [](QCustomPlot& plot) -> QObject*
          { return plot.add<QCPWaterfallGraph>().get(); } },
        { "QCPCartesianSystem", [](QCustomPlot& plot) -> QObject*
          { return plot.axisRect()->system(plot.xAxis, plot.yAxis).get(); } },
        { "QCPPolarSystem",
          [](QCustomPlot& plot) -> QObject*
          {
              QCPPolarArea* area = polarAreaOn(plot);
              return area->system(area->angularAxis().get(), area->radialAxis().get()).get();
          } },
        { "QCPAlignmentGroup",
          [](QCustomPlot& plot) -> QObject*
          {
              return QCPAlignmentGroup::create({ plot.axisRect().get(), axisRectOn(plot) },
                                               QCP::msLeft)
                  .data();
          } },
        { "QCPPlottableGroup",
          [](QCustomPlot& plot) -> QObject*
          {
              QCPPlottableGroup* group = plot.addPlottableGroup(QStringLiteral("group")).get();
              group->addMember(graphOn(plot));
              return group;
          } },
    };
    return list;
}

// A value other than \a current, for a property of that type. Object pointers name a sibling on
// the same plot.
using Different = std::function<QVariant(const QVariant& current, QObject* object)>;

template <typename T>
std::pair<QMetaType, Different> differentFor(std::function<T(const T&, QObject*)> make)
{
    return { QMetaType::fromType<T>(),
             [make](const QVariant& current, QObject* object) {
                 return QVariant::fromValue(make(current.value<T>(), object));
             } };
}

QCustomPlot* plotOf(QObject* object)
{
    if (auto* layerable = qobject_cast<QCPLayerable*>(object))
        return layerable->parentPlot();
    if (auto* layer = qobject_cast<QCPLayer*>(object))
        return layer->parentPlot();
    return qobject_cast<QCustomPlot*>(object);
}

// A typesetter other than the default one, which is null without LaTeX.
class PlainRenderer : public QCPLabelRenderer
{
public:
    QSize measure(const QFont& font, const QString& text) const override
    {
        return measureWith(nullptr, font, text);
    }

    void draw(QCPPainter* painter, const QRect& rect, const QFont& font, const QColor& color,
              const QString& text, int flags) const override
    {
        drawWith(nullptr, painter, rect, font, color, text, flags);
    }
} plainRenderer;

QColor otherColor(const QColor& color)
{
    return color == QColor(Qt::magenta) ? QColor(Qt::cyan) : QColor(Qt::magenta);
}

const QVector<std::pair<QMetaType, Different>>& differentValues()
{
    static const QVector<std::pair<QMetaType, Different>> table {
        differentFor<QString>([](const QString& v, QObject*) { return v + QStringLiteral("x"); }),
        differentFor<bool>([](const bool& v, QObject*) { return !v; }),
        differentFor<int>([](const int& v, QObject*) { return v + 1; }),
        differentFor<double>([](const double& v, QObject*) { return v + 0.5; }),
        differentFor<QColor>([](const QColor& v, QObject*) { return otherColor(v); }),
        differentFor<QPen>(
            [](const QPen& v, QObject*)
            {
                QPen pen = v;
                pen.setColor(otherColor(v.color()));
                pen.setWidthF(v.widthF() + 1);
                return pen;
            }),
        differentFor<QBrush>([](const QBrush& v, QObject*)
                             { return QBrush(otherColor(v.color()), Qt::SolidPattern); }),
        differentFor<QFont>(
            [](const QFont& v, QObject*)
            {
                QFont font = v;
                font.setPointSizeF(v.pointSizeF() + 1);
                return font;
            }),
        differentFor<QMargins>([](const QMargins& v, QObject*) { return v + 1; }),
        differentFor<QRect>([](const QRect& v, QObject*) { return v.adjusted(1, 1, 2, 2); }),
        differentFor<QSize>([](const QSize& v, QObject*) { return v + QSize(1, 1); }),
        differentFor<QPointF>([](const QPointF& v, QObject*) { return v + QPointF(1, 1); }),
        differentFor<QPixmap>(
            [](const QPixmap& v, QObject*)
            {
                QPixmap pixmap(2, 2);
                pixmap.fill(v.isNull() ? Qt::red : Qt::green);
                return pixmap;
            }),
        differentFor<QCPRange>([](const QCPRange& v, QObject*)
                               { return QCPRange(v.lower + 1, v.upper + 2); }),
        differentFor<QCPLineEnding>(
            [](const QCPLineEnding& v, QObject*)
            {
                return QCPLineEnding(v.style() == QCPLineEnding::esSpikeArrow
                                         ? QCPLineEnding::esDisc
                                         : QCPLineEnding::esSpikeArrow);
            }),
        differentFor<QCPScatterStyle>(
            [](const QCPScatterStyle& v, QObject*)
            {
                return QCPScatterStyle(v.shape() == QCPScatterStyle::ssCircle
                                           ? QCPScatterStyle::ssDisc
                                           : QCPScatterStyle::ssCircle);
            }),
        differentFor<QCPScale>(
            [](const QCPScale& v, QObject*) { return v.isLinear() ? QCPScale::log() : QCPScale::linear(); }),
        differentFor<QCPColorGradient>(
            [](const QCPColorGradient& v, QObject*)
            {
                const QCPColorGradient hot(QCPColorGradient::gpHot);
                return v == hot ? QCPColorGradient(QCPColorGradient::gpJet) : hot;
            }),
        differentFor<QCPDataSelection>(
            [](const QCPDataSelection& v, QObject*)
            {
                const QCPDataSelection first(QCPDataRange(0, 1));
                return v == first ? QCPDataSelection(QCPDataRange(1, 2)) : first;
            }),
        differentFor<QSharedPointer<QCPAxisTicker>>(
            [](const QSharedPointer<QCPAxisTicker>&, QObject*)
            { return QSharedPointer<QCPAxisTicker>(new QCPAxisTickerFixed); }),
        // A list of stretch factors keeps its length (one per row or column): a value moves.
        differentFor<QList<double>>(
            [](const QList<double>& v, QObject*)
            {
                QList<double> list = v;
                if (list.isEmpty())
                    list.append(1);
                else
                    list.last() += 1;
                return list;
            }),
        differentFor<QList<QColor>>(
            [](const QList<QColor>& v, QObject*)
            {
                QList<QColor> list = v;
                list.append(Qt::red);
                return list;
            }),
        differentFor<QCPLayer*>(
            [](QCPLayer* const& v, QObject* object) -> QCPLayer*
            {
                QCustomPlot* plot = plotOf(object);
                QCPLayer* grid = plot->layer(QStringLiteral("grid"));
                return v == grid ? plot->layer(QStringLiteral("overlay")).get() : grid;
            }),
        differentFor<QCPPlottable*>(
            [](QCPPlottable* const&, QObject* object) -> QCPPlottable*
            { return graphOn(*plotOf(object)); }),
        differentFor<QCPGraph*>([](QCPGraph* const&, QObject* object) -> QCPGraph*
                                { return graphOn(*plotOf(object)); }),
        differentFor<QCPAxisRect*>([](QCPAxisRect* const&, QObject* object) -> QCPAxisRect*
                                   { return axisRectOn(*plotOf(object)); }),
        differentFor<QCPBarsGroup*>([](QCPBarsGroup* const&, QObject* object) -> QCPBarsGroup*
                                    { return new QCPBarsGroup(plotOf(object)); }),
        differentFor<QCPColorScale*>([](QCPColorScale* const&, QObject* object) -> QCPColorScale*
                                     { return colorScaleOn(*plotOf(object)); }),
        differentFor<QCPLabelRenderer*>(
            [](QCPLabelRenderer* const& v, QObject*) -> QCPLabelRenderer*
            { return v == &plainRenderer ? nullptr : &plainRenderer; }),
        differentFor<QCPPolarAxisRadial*>(
            [](QCPPolarAxisRadial* const& v, QObject* object) -> QCPPolarAxisRadial*
            {
                auto* area = plotOf(object)->findChild<QCPPolarArea*>();
                QCPPolarAxisRadial* first = area->radialAxis().get();
                return v == first ? area->addRadialAxis().get() : first;
            }),
        differentFor<QMap<double, QColor>>(
            [](const QMap<double, QColor>& v, QObject*)
            {
                QMap<double, QColor> stops = v;
                stops.insert(0.25, otherColor(v.value(0.25)));
                return stops;
            }),
        differentFor<QPainterPath>(
            [](const QPainterPath& v, QObject*)
            {
                QPainterPath path = v;
                path.addEllipse(QRectF(-2, -2, 4, 4));
                return path;
            }),
        differentFor<QList<int>>([](const QList<int>& v, QObject*)
                                 { return v.isEmpty() ? QList<int> { 0 } : QList<int> { }; }),
        differentFor<QCPCoordinateSystem*>(
            [](QCPCoordinateSystem* const& v, QObject* object) -> QCPCoordinateSystem*
            {
                QCustomPlot* plot = plotOf(object);
                QCPAxisRect* rect = plot->axisRect().get();
                QCPCoordinateSystem* other = rect->system(plot->xAxis2, plot->yAxis2).get();
                return v == other ? rect->system(plot->xAxis, plot->yAxis).get() : other;
            }),
        differentFor<QCPArea*>([](QCPArea* const&, QObject* object) -> QCPArea*
                               { return axisRectOn(*plotOf(object)); }),
        differentFor<QCPSelectionRect*>(
            [](QCPSelectionRect* const&, QObject* object) -> QCPSelectionRect*
            { return new QCPSelectionRect(plotOf(object)); }),
        differentFor<QCPTheme*>(
            [](QCPTheme* const& v, QObject*) -> QCPTheme*
            {
                // Outlive every plot of the run: a plot reading a theme does not own it.
                static QCPTheme* const first = new QCPTheme;
                static QCPTheme* const second = new QCPTheme;
                return v == first ? second : first;
            }),
        differentFor<QCPAxis*>(
            [](QCPAxis* const& v, QObject* object) -> QCPAxis*
            {
                QCustomPlot* plot = plotOf(object);
                return v == plot->xAxis2.get() ? plot->yAxis2.get() : plot->xAxis2.get();
            }),
    };
    return table;
}

// Enums and flags are read from the meta-enum: the first other key, or one bit toggled.
QVariant differentEnum(const QMetaProperty& property, const QVariant& current)
{
    const QMetaEnum e = property.enumerator();
    const int now = current.toInt();
    int next = now;
    for (int i = 0; i < e.keyCount() && next == now; ++i)
    {
        const int key = e.value(i);
        if (!e.isFlag())
            next = key;
        else if (key != 0 && (key & (key - 1)) == 0)
            next = now ^ key;
    }
    QVariant value(next);
    if (!value.convert(property.metaType()))
        return {};
    return value;
}

// QVariant's own ==, except for the types without an operator== (QPixmap: the same image).
bool same(const QVariant& a, const QVariant& b)
{
    if (a.metaType() == QMetaType::fromType<QPixmap>() && b.metaType() == a.metaType())
        return a.value<QPixmap>().cacheKey() == b.value<QPixmap>().cacheKey();
    return a == b;
}

QVariant differentValue(const QMetaProperty& property, const QVariant& current, QObject* object)
{
    if (property.isEnumType() || property.isFlagType())
        return differentEnum(property, current);
    for (const auto& [type, make] : differentValues())
        if (type == property.metaType())
            return make(current, object);
    return {};
}

// The setters that do not simply store what they get. Each has a value the generic check uses,
// and a rule of its own, run by exceptionRulesHold once the property has its NOTIFY.
enum class Rule
{
    Refuses, // an invalid value (the probe) leaves the property, with no signal
    Clamps, // a value out of range (the probe) is stored clamped, one signal
    TrimmedBySelectable, // the selection is cut to what the selectable type allows
    TakesOwnership, // the object is adopted; writing the same pointer again is refused
    ComponentPen, // a line's setPen repaints components, pen() may not move
};

using MakeValue = std::function<QVariant(QObject*)>;

struct Exception
{
    const QMetaObject* meta;
    const char* property;
    Rule rule;
    // A value the setter stores as given. None: the setter accepts only the current value, so the
    // generic check has no row and the rule covers the property.
    MakeValue accepted;
    MakeValue probe; // the value the rule writes, when it needs one
    const char* why;
    bool reportsMisuse = false; // the refusal goes through the misuse channel
};

QCPSelectionDecorator* freshDecorator() { return new QCPSelectionDecorator; }

const QVector<Exception>& exceptions()
{
    static const QVector<Exception> list {
        { &QCPAxis::staticMetaObject, "numberFormat", Rule::Refuses,
          [](QObject*) { return QVariant(QStringLiteral("eb")); },
          [](QObject*) { return QVariant(QStringLiteral("xb")); },
          "parses the format code and refuses an invalid one" },
        { &QCPAxis::staticMetaObject, "tickLabelRotation", Rule::Clamps,
          [](QObject*) { return QVariant(45.0); }, [](QObject*) { return QVariant(120.0); },
          "clamps to [-90, 90]" },
        { &QCPPlottable::staticMetaObject, "selection", Rule::TrimmedBySelectable,
          [](QObject* object)
          {
              auto* plottable = qobject_cast<QCPPlottable*>(object);
              plottable->setSelectable(QCP::stDataRange);
              return QVariant::fromValue(QCPDataSelection(QCPDataRange(0, 1)));
          },
          nullptr, "enforceType trims it by selectable (plottable.cpp)" },
        { &QCPPlottable::staticMetaObject, "selectionDecorator", Rule::TakesOwnership,
          [](QObject*) { return QVariant::fromValue(freshDecorator()); }, nullptr,
          "adopts the decorator and refuses the same one again (plottable.cpp:966-976)" },
        { &QCPLine::staticMetaObject, "pen", Rule::ComponentPen,
          [](QObject*) { return QVariant(QPen(Qt::blue, 3)); }, nullptr,
          "repaints when only a component differs (plottable-line.cpp:84)" },
        { &QCPPolarAxisAngular::staticMetaObject, "numberFormat", Rule::Refuses,
          [](QObject*) { return QVariant(QStringLiteral("eb")); },
          [](QObject*) { return QVariant(QStringLiteral("xb")); },
          "parses the format code and refuses an invalid one (angularaxis.cpp)" },
        { &QCPPolarAxisRadial::staticMetaObject, "numberFormat", Rule::Refuses,
          [](QObject*) { return QVariant(QStringLiteral("eb")); },
          [](QObject*) { return QVariant(QStringLiteral("xb")); },
          "parses the format code and refuses an invalid one (radialaxis.cpp)" },
        { &QCPPolarAxisAngular::staticMetaObject, "tickLabelRotation", Rule::Clamps,
          [](QObject*) { return QVariant(45.0); }, [](QObject*) { return QVariant(120.0); },
          "the label painter clamps to [-90, 90]" },
        { &QCPPolarAxisRadial::staticMetaObject, "tickLabelRotation", Rule::Clamps,
          [](QObject*) { return QVariant(45.0); }, [](QObject*) { return QVariant(120.0); },
          "the label painter clamps to [-90, 90]" },
        { &QCustomPlot::staticMetaObject, "dataSwapDebounceMs", Rule::Clamps,
          [](QObject*) { return QVariant(250); }, [](QObject*) { return QVariant(-5); },
          "a negative window is none (core.cpp)" },
        { &QCustomPlot::staticMetaObject, "dataSwapMaxWaitMs", Rule::Clamps,
          [](QObject*) { return QVariant(1500); }, [](QObject*) { return QVariant(-5); },
          "a negative wait is none (core.cpp)" },
        { &QCustomPlot::staticMetaObject, "currentLayer", Rule::Refuses,
          [](QObject* object)
          {
              return QVariant::fromValue(
                  qobject_cast<QCustomPlot*>(object)->layer(QStringLiteral("grid")).get());
          },
          [](QObject*) { return QVariant::fromValue(static_cast<QCPLayer*>(nullptr)); },
          "takes only a layer of this plot (core.cpp)" },
        { &QCPItemTracer::staticMetaObject, "component", Rule::Refuses,
          [](QObject* object)
          {
              auto* tracer = qobject_cast<QCPItemTracer*>(object);
              tracer->setGraph(twoColumnGraphOn(*tracer->parentPlot()));
              return QVariant(1);
          },
          [](QObject*) { return QVariant(1); },
          "selects a column of its graph: none without a graph (item-tracer.cpp)" },
        { &QCPOverlay::staticMetaObject, "opacity", Rule::Clamps,
          [](QObject*) { return QVariant(qreal(0.5)); },
          [](QObject*) { return QVariant(qreal(-1.0)); }, "clamps to [0, 1] (overlay.cpp)" },
        { &QCPHistogram::staticMetaObject, "bins", Rule::Refuses,
          [](QObject*) { return QVariant(17); }, [](QObject*) { return QVariant(0); },
          "needs at least one bin (plottable-histogram.cpp)", true },
        { &QCPPolarGraph::staticMetaObject, "system", Rule::Refuses,
          [](QObject* object)
          {
              QCPPolarArea* area = plotOf(object)->findChild<QCPPolarArea*>();
              return QVariant::fromValue(
                  area->system(area->angularAxis().get(), area->addRadialAxis().get()).get());
          },
          [](QObject* object)
          {
              QCustomPlot* plot = plotOf(object);
              return QVariant::fromValue(plot->axisRect()->system(plot->xAxis, plot->yAxis).get());
          },
          "draws in a polar system only (polargraph.cpp)", true },
        { &QCPPolarAxisAngular::staticMetaObject, "scale", Rule::Refuses, nullptr,
          [](QObject*) { return QVariant::fromValue(QCPScale::log()); },
          "the ring has no non-linear scale (angularaxis.cpp)", true },
    };
    return list;
}

const Exception* exceptionFor(const QObject* object, const char* property)
{
    for (const Exception& e : exceptions())
        if (object->metaObject()->inherits(e.meta) && qstrcmp(e.property, property) == 0)
            return &e;
    return nullptr;
}

bool isNotifyProperty(const QMetaProperty& property)
{
    return property.hasNotifySignal() && property.isWritable();
}

// The library's classes, not Qt's (QWidget, QRhiWidget): the surface under test is ours.
bool isOurs(const QMetaObject* meta)
{
    const QByteArray name = meta->className();
    return name.startsWith("QCP") || name == "QCustomPlot";
}

// The class whose Q_PROPERTY declares property \a index of \a meta.
const QMetaObject* declaringClass(const QMetaObject* meta, int index)
{
    while (index < meta->propertyOffset())
        meta = meta->superClass();
    return meta;
}

// One row per fixture and per class it inherits: a class shows its properties through every
// fixture, so the first fixture of each class is enough.
struct Declared
{
    int fixture;
    const QMetaObject* meta;
};

QVector<Declared> declaredClasses()
{
    QVector<Declared> out;
    QSet<const QMetaObject*> seen;
    for (int f = 0; f < fixtures().size(); ++f)
    {
        auto plot = newPlot();
        for (const QMetaObject* meta = fixtures()[f].make(*plot)->metaObject(); isOurs(meta);
             meta = meta->superClass())
            if (!seen.contains(meta))
            {
                seen.insert(meta);
                out.append({ f, meta });
            }
    }
    return out;
}

bool hasGenericRow(const QMetaObject* meta, const QMetaProperty& property)
{
    for (const Exception& e : exceptions())
        if (meta->inherits(e.meta) && qstrcmp(e.property, property.name()) == 0)
            return bool(e.accepted);
    return true;
}

// True when the notify signal's first argument carries the property's value.
bool signalCarriesValue(const QMetaProperty& property)
{
    const QMetaMethod notify = property.notifySignal();
    return notify.parameterCount() > 0 && notify.parameterMetaType(0) == property.metaType();
}
} // namespace

void TestMetaSurface::everyPropertyIsConstantOrNotifies_data()
{
    QTest::addColumn<int>("fixture");
    QTest::addColumn<QByteArray>("className");
    for (const Declared& d : declaredClasses())
        QTest::newRow(d.meta->className()) << d.fixture << QByteArray(d.meta->className());
}

// E-11: a property is constant, or it has a change signal; a read-only one without a signal is
// off the surface.
void TestMetaSurface::everyPropertyIsConstantOrNotifies()
{
    QFETCH(int, fixture);
    QFETCH(QByteArray, className);
    auto plot = newPlot();
    const QMetaObject* meta = fixtures()[fixture].make(*plot)->metaObject();
    for (int i = QObject::staticMetaObject.propertyCount(); i < meta->propertyCount(); ++i)
    {
        const QMetaProperty property = meta->property(i);
        if (declaringClass(meta, i)->className() != className)
            continue;
        QVERIFY2(property.isConstant() || property.hasNotifySignal(),
                 qPrintable(QStringLiteral("%1::%2 neither constant nor notifying")
                                .arg(QLatin1String(className), QLatin1String(property.name()))));
        QVERIFY2(!property.isConstant() || !property.isWritable(),
                 qPrintable(QStringLiteral("%1::%2 is constant and writable")
                                .arg(QLatin1String(className), QLatin1String(property.name()))));
    }
}

namespace
{
// E-4b: a function other than the setter that moves properties emits the signal of each.
struct Writer
{
    const char* name;
    std::function<QObject*(QCustomPlot&)> make;
    std::function<void(QObject*)> write;
    QVector<const char*> moved; // each signals once
    QVector<const char*> still; // each signals nothing
    bool idempotent = true; // a second call moves nothing
};

const QVector<Writer>& writers()
{
    static const QVector<Writer> list {
        // P-3: an identity link shares its source's scale.
        { "QCPAxisBase::linkTo takes the source's scale",
          [](QCustomPlot& plot) -> QObject*
          {
              plot.xAxis->setScale(QCPScale::log());
              return plot.xAxis2;
          },
          [](QObject* o)
          {
              auto* axis = qobject_cast<QCPAxis*>(o);
              axis->linkTo(axis->parentPlot()->xAxis);
          },
          { "scale" },
          { } },
        { "QCPAxisBase::setScale on a source reaches its identity follower",
          [](QCustomPlot& plot) -> QObject*
          {
              plot.xAxis2->linkTo(plot.xAxis);
              return plot.xAxis2;
          },
          [](QObject* o) { qobject_cast<QCPAxis*>(o)->parentPlot()->xAxis->setScale(QCPScale::log()); },
          { "scale" },
          { } },
        { "QCPAxis::setColor",
          [](QCustomPlot& plot) -> QObject* { return plot.xAxis; },
          [](QObject* o) { qobject_cast<QCPAxis*>(o)->setColor(Qt::red); },
          { "labelColor", "tickLabelColor", "basePen", "tickPen", "subTickPen" },
          { "selectedLabelColor" } },
        { "QCPAxis::setTickLength",
          [](QCustomPlot& plot) -> QObject* { return plot.xAxis; },
          [](QObject* o) { qobject_cast<QCPAxis*>(o)->setTickLength(9, 8); },
          { "tickLengthIn", "tickLengthOut" },
          { "subTickLengthIn" } },
        { "QCPAxis::setSubTickLength",
          [](QCustomPlot& plot) -> QObject* { return plot.xAxis; },
          [](QObject* o) { qobject_cast<QCPAxis*>(o)->setSubTickLength(9, 8); },
          { "subTickLengthIn", "subTickLengthOut" },
          { "tickLengthIn" } },
        { "QCPPolarAxisAngular::setTickLength",
          [](QCustomPlot& plot) -> QObject* { return polarAreaOn(plot)->angularAxis().get(); },
          [](QObject* o) { qobject_cast<QCPPolarAxisAngular*>(o)->setTickLength(9, 8); },
          { "tickLengthIn", "tickLengthOut" },
          { "subTickLengthIn" } },
        { "QCPPolarAxisAngular::setSubTickLength",
          [](QCustomPlot& plot) -> QObject* { return polarAreaOn(plot)->angularAxis().get(); },
          [](QObject* o) { qobject_cast<QCPPolarAxisAngular*>(o)->setSubTickLength(9, 8); },
          { "subTickLengthIn", "subTickLengthOut" },
          { "tickLengthIn" } },
        { "QCPPolarAxisRadial::setTickLength",
          [](QCustomPlot& plot) -> QObject* { return polarAreaOn(plot)->radialAxis().get(); },
          [](QObject* o) { qobject_cast<QCPPolarAxisRadial*>(o)->setTickLength(9, 8); },
          { "tickLengthIn", "tickLengthOut" },
          { "subTickLengthIn" } },
        { "QCPPolarAxisRadial::setSubTickLength",
          [](QCustomPlot& plot) -> QObject* { return polarAreaOn(plot)->radialAxis().get(); },
          [](QObject* o) { qobject_cast<QCPPolarAxisRadial*>(o)->setSubTickLength(9, 8); },
          { "subTickLengthIn", "subTickLengthOut" },
          { "tickLengthIn" } },
        { "QCustomPlot::setAntialiasedElements moving both sets",
          [](QCustomPlot& plot) -> QObject*
          {
              plot.setAntialiasedElements(QCP::aeNone);
              plot.setNotAntialiasedElements(QCP::aeAxes);
              return &plot;
          },
          [](QObject* o) { qobject_cast<QCustomPlot*>(o)->setAntialiasedElements(QCP::aeAxes); },
          { "antialiasedElements", "notAntialiasedElements" },
          { } },
        { "QCustomPlot::setAntialiasedElement",
          [](QCustomPlot& plot) -> QObject* { return &plot; },
          [](QObject* o)
          { qobject_cast<QCustomPlot*>(o)->setAntialiasedElement(QCP::aeAxes, false); },
          { "antialiasedElements" },
          { "notAntialiasedElements" } },
        { "QCustomPlot::setNotAntialiasedElement",
          [](QCustomPlot& plot) -> QObject* { return &plot; },
          [](QObject* o)
          { qobject_cast<QCustomPlot*>(o)->setNotAntialiasedElement(QCP::aeGrid, true); },
          { "notAntialiasedElements" },
          { } },
        { "QCustomPlot::setInteraction",
          [](QCustomPlot& plot) -> QObject* { return &plot; },
          [](QObject* o) { qobject_cast<QCustomPlot*>(o)->setInteraction(QCP::iRangeDrag, true); },
          { "interactions" },
          { "plottingHints" } },
        { "QCustomPlot::setPlottingHint",
          [](QCustomPlot& plot) -> QObject* { return &plot; },
          [](QObject* o)
          { qobject_cast<QCustomPlot*>(o)->setPlottingHint(QCP::phFastPolylines, true); },
          { "plottingHints" },
          { "interactions" } },
        { "QCustomPlot::setCurrentLayer(name)",
          [](QCustomPlot& plot) -> QObject* { return &plot; },
          [](QObject* o)
          { qobject_cast<QCustomPlot*>(o)->setCurrentLayer(QStringLiteral("grid")); },
          { "currentLayer" },
          { } },
        { "QCustomPlot::removeLayer of the current layer",
          [](QCustomPlot& plot) -> QObject*
          {
              (void)plot.addLayer(QStringLiteral("doomed")); // the next line fails without it
              plot.setCurrentLayer(QStringLiteral("doomed"));
              return &plot;
          },
          [](QObject* o)
          {
              auto* plot = qobject_cast<QCustomPlot*>(o);
              if (QCPLayer* doomed = plot->layer(QStringLiteral("doomed")))
                  (void)plot->removeLayer(doomed); // the moved layer shows the removal
          },
          { "currentLayer" },
          { } },
        { "QCPItemTracer::setGraph with a component",
          [](QCustomPlot& plot) -> QObject* { return plot.add<QCPItemTracer>().get(); },
          [](QObject* o)
          {
              auto* tracer = qobject_cast<QCPItemTracer*>(o);
              if (!tracer->graph())
                  tracer->setGraph(twoColumnGraphOn(*tracer->parentPlot()), 1);
          },
          { "graph", "component" },
          { "graphKey" } },
        { "QCPOverlay::setCollapsible uncollapsing",
          [](QCustomPlot& plot) -> QObject*
          {
              plot.overlay()->setCollapsible(true);
              plot.overlay()->setCollapsed(true);
              return plot.overlay().get();
          },
          [](QObject* o) { qobject_cast<QCPOverlay*>(o)->setCollapsible(false); },
          { "collapsible", "collapsed" },
          { "opacity" } },
        { "QCPPlottable::setKeyAxis dropping the system",
          [](QCustomPlot& plot) -> QObject* { return graphOn(plot); },
          [](QObject* o) { qobject_cast<QCPGraph*>(o)->setKeyAxis(nullptr); },
          { "system", "keyAxis" },
          { "valueAxis" } },
        { "QCPPlottableGroup::removeMember of the last member",
          [](QCustomPlot& plot) -> QObject*
          {
              QCPPlottableGroup* group = plot.addPlottableGroup(QStringLiteral("group")).get();
              group->addMember(graphOn(plot));
              return group;
          },
          [](QObject* o)
          {
              auto* group = qobject_cast<QCPPlottableGroup*>(o);
              group->removeMember(group->parentPlot()->plottable(0).get());
          },
          { "visible" },
          { "name" } },
        { "a group member hidden",
          [](QCustomPlot& plot) -> QObject*
          {
              QCPPlottableGroup* group = plot.addPlottableGroup(QStringLiteral("group")).get();
              group->addMember(graphOn(plot));
              return group;
          },
          [](QObject* o)
          {
              auto* group = qobject_cast<QCPPlottableGroup*>(o);
              group->parentPlot()->plottable(0)->setVisible(false);
          },
          { "visible" },
          { "name" } },
        { "QCPColorMap2::setAutoContourLevels",
          [](QCustomPlot& plot) -> QObject*
          {
              auto* map = plot.add<QCPColorMap2>().get();
              map->setContourLevels({ 1, 2 });
              return map;
          },
          [](QObject* o) { qobject_cast<QCPColorMap2*>(o)->setAutoContourLevels(3); },
          { "contourLevels" },
          { "contourPen" } },
        { "QCPHistogram2D::setAutoContourLevels",
          [](QCustomPlot& plot) -> QObject*
          {
              auto* histogram = plot.add<QCPHistogram2D>().get();
              histogram->setContourLevels({ 1, 2 });
              return histogram;
          },
          [](QObject* o) { qobject_cast<QCPHistogram2D*>(o)->setAutoContourLevels(3); },
          { "contourLevels" },
          { "contourPen" } },
        { "QCPIntervals::setSelection",
          [](QCustomPlot& plot) -> QObject* { return intervalsOn(plot); },
          [](QObject* o)
          { qobject_cast<QCPIntervals*>(o)->setSelection(QCPDataSelection(QCPDataRange(1, 2))); },
          { "selectedRows", "selection" },
          { "style" } },
        { "removePlottable of a group's last member",
          [](QCustomPlot& plot) -> QObject* { return groupOfOneOn(plot); },
          [](QObject* o)
          {
              QCustomPlot* plot = qobject_cast<QCPPlottableGroup*>(o)->parentPlot().get();
              if (plot->plottableCount() > 0)
                  (void)plot->removePlottable(plot->plottable(0).get());
          },
          { "visible" },
          { "name" } },
        { "take() of a group's last member",
          [](QCustomPlot& plot) -> QObject* { return groupOfOneOn(plot); },
          [](QObject* o)
          {
              static std::unique_ptr<QCPGraph> taken; // outlives the plot of the row
              QCustomPlot* plot = qobject_cast<QCPPlottableGroup*>(o)->parentPlot().get();
              if (plot->graphCount() > 0)
                  taken = plot->take(plot->graph(0));
          },
          { "visible" },
          { "name" } },
        { "deleting a group's last member",
          [](QCustomPlot& plot) -> QObject* { return groupOfOneOn(plot); },
          [](QObject* o)
          {
              QCustomPlot* plot = qobject_cast<QCPPlottableGroup*>(o)->parentPlot().get();
              if (plot->plottableCount() > 0)
                  delete plot->plottable(0).get();
          },
          { "visible" },
          { "name" } },
        { "take() of a plottable",
          [](QCustomPlot& plot) -> QObject* { return graphOn(plot); },
          [](QObject* o)
          {
              static std::unique_ptr<QCPGraph> taken; // outlives the plot of the row
              auto* graph = qobject_cast<QCPGraph*>(o);
              if (QCustomPlot* plot = graph->parentPlot())
                  taken = plot->take(QCPHandle<QCPGraph>(graph));
          },
          { "system", "keyAxis", "valueAxis" },
          { "name" } },
        { "QCPItemRSpanOps::setLeftEdge",
          [](QCustomPlot& plot) -> QObject* { return plot.add<QCPItemRSpan>().get(); },
          [](QObject* o)
          {
              qcp::makeHandle<qcp::HandleFor<QCPItemRSpan>>(qobject_cast<QCPItemRSpan*>(o))
                  .setLeftEdge(-4, 0);
          },
          { "keyRange" },
          { "valueRange" } },
        { "QCPItemRSpanOps::setTopEdge",
          [](QCustomPlot& plot) -> QObject* { return plot.add<QCPItemRSpan>().get(); },
          [](QObject* o)
          {
              qcp::makeHandle<qcp::HandleFor<QCPItemRSpan>>(qobject_cast<QCPItemRSpan*>(o))
                  .setTopEdge(0, 9);
          },
          { "valueRange" },
          { "keyRange" } },
        { "QCPItemRSpan's raw right edge",
          [](QCustomPlot& plot) -> QObject* { return plot.add<QCPItemRSpan>().get(); },
          [](QObject* o) { qobject_cast<QCPItemRSpan*>(o)->rightEdge->setCoords(8, 0); },
          { "keyRange" },
          { "valueRange" } },
        { "QCPItemSpanOps::setUpperEdge",
          [](QCustomPlot& plot) -> QObject* { return plot.add<QCPItemSpan>(Qt::Vertical).get(); },
          [](QObject* o)
          {
              qcp::makeHandle<qcp::HandleFor<QCPItemSpan>>(qobject_cast<QCPItemSpan*>(o))
                  .setUpperEdge(7, 1);
          },
          { "range" },
          { "brush" } },
        { "QCPItemSpan's raw lower edge",
          [](QCustomPlot& plot) -> QObject* { return plot.add<QCPItemSpan>(Qt::Vertical).get(); },
          [](QObject* o) { qobject_cast<QCPItemSpan*>(o)->lowerEdge->setCoords(-2, 0); },
          { "range" },
          { "brush" } },
        { "a legend item selected",
          [](QCustomPlot& plot) -> QObject*
          {
              graphOn(plot);
              return plot.legend.get();
          },
          [](QObject* o) { qobject_cast<QCPLegend*>(o)->item(0)->setSelected(true); },
          { "selectedParts" },
          { "font" } },
        { "a legend item deselected",
          [](QCustomPlot& plot) -> QObject*
          {
              graphOn(plot);
              plot.legend->item(0)->setSelected(true);
              return plot.legend.get();
          },
          [](QObject* o) { qobject_cast<QCPLegend*>(o)->item(0)->setSelected(false); },
          { "selectedParts" },
          { "font" } },
        { "a selected legend item removed",
          [](QCustomPlot& plot) -> QObject*
          {
              graphOn(plot);
              graphOn(plot);
              plot.legend->item(0)->setSelected(true);
              return plot.legend.get();
          },
          [](QObject* o)
          {
              auto* legend = qobject_cast<QCPLegend*>(o);
              if (legend->item(0) && legend->item(0)->selected())
                  legend->removeItem(0);
          },
          { "selectedParts" },
          { "font" } },
        { "QCPAxisBase::setRangeLower",
          [](QCustomPlot& plot) -> QObject* { return plot.xAxis; },
          [](QObject* o) { qobject_cast<QCPAxis*>(o)->setRangeLower(-3); },
          { "range" },
          { } },
        { "QCPAxisBase::setRangeUpper",
          [](QCustomPlot& plot) -> QObject* { return plot.xAxis; },
          [](QObject* o) { qobject_cast<QCPAxis*>(o)->setRangeUpper(30); },
          { "range" },
          { } },
        { "QCPLayoutGrid::insertRow",
          [](QCustomPlot& plot) -> QObject* { return plot.plotLayout().get(); },
          [](QObject* o) { qobject_cast<QCPLayoutGrid*>(o)->insertRow(0); },
          { "rowStretchFactors" },
          { "columnStretchFactors" },
          false },
        { "QCPLayoutGrid::insertColumn",
          [](QCustomPlot& plot) -> QObject* { return plot.plotLayout().get(); },
          [](QObject* o) { qobject_cast<QCPLayoutGrid*>(o)->insertColumn(0); },
          { "columnStretchFactors" },
          { "rowStretchFactors" },
          false },
        { "QCPLayoutGrid::addElement",
          [](QCustomPlot& plot) -> QObject* { return plot.plotLayout().get(); },
          [](QObject* o)
          {
              auto* grid = qobject_cast<QCPLayoutGrid*>(o);
              if (!grid->hasElement(1, 1))
                  grid->addElement(1, 1, new QCPTextElement(grid->parentPlot()));
          },
          { "columnStretchFactors", "rowStretchFactors" },
          { } },
        { "QCPLayoutGrid::simplify",
          [](QCustomPlot& plot) -> QObject*
          {
              plot.plotLayout()->insertRow(1);
              return plot.plotLayout().get();
          },
          [](QObject* o) { qobject_cast<QCPLayoutGrid*>(o)->simplify(); },
          { "rowStretchFactors" },
          { "columnStretchFactors" } },
        { "QCPColorScale::setType",
          [](QCustomPlot& plot) -> QObject*
          {
              QCPColorScale* scale = colorScaleOn(plot);
              scale->setLabel(QStringLiteral("z"));
              return scale;
          },
          [](QObject* o) { qobject_cast<QCPColorScale*>(o)->setType(QCPAxis::atBottom); },
          { "type" },
          { "label", "rangeDrag", "rangeZoom" } },
        { "QCPLine::setComponentScatterStyle on component 0",
          [](QCustomPlot& plot) -> QObject* { return graphOn(plot); },
          [](QObject* o)
          {
              qobject_cast<QCPGraph*>(o)->setComponentScatterStyle(
                  0, QCPScatterStyle(QCPScatterStyle::ssDisc));
          },
          { "scatterStyle" },
          { "lineStyle" } },
        { "QCPErrorBars refusing a data plottable",
          [](QCustomPlot& plot) -> QObject*
          {
              auto* errors = plot.add<QCPErrorBars>().get();
              errors->setDataPlottable(graphOn(plot));
              return errors;
          },
          [](QObject* o)
          {
              auto* errors = qobject_cast<QCPErrorBars*>(o);
              errors->setDataPlottable(errors); // error bars cannot carry error bars: detached
          },
          { "dataPlottable" },
          { } },
        { "QCPBarsGroup::clear",
          [](QCustomPlot& plot) -> QObject*
          {
              QCPBars* bars = barsOn(plot);
              bars->setBarsGroup(new QCPBarsGroup(&plot));
              return bars;
          },
          [](QObject* o)
          {
              if (QCPBarsGroup* group = qobject_cast<QCPBars*>(o)->barsGroup().get())
                  group->clear();
          },
          { "barsGroup" },
          { } },
        { "QCustomPlot::setBackground(pixmap, scaled, mode)",
          [](QCustomPlot& plot) -> QObject* { return &plot; },
          [](QObject* o)
          {
              static const QPixmap shared = []
              {
                  QPixmap pixmap(2, 2);
                  pixmap.fill(Qt::red);
                  return pixmap;
              }(); // one image: the second call moves nothing
              qobject_cast<QCustomPlot*>(o)->setBackground(shared, false, Qt::IgnoreAspectRatio);
          },
          { "background", "backgroundScaled", "backgroundScaledMode" },
          { } },
        { "a theme setter",
          [](QCustomPlot& plot) -> QObject* { return &plot; },
          [](QObject* o) { qobject_cast<QCustomPlot*>(o)->theme()->setGrid(Qt::red); },
          { "themeGrid" },
          { "viewport" } },
        { "QCustomPlot::setTheme",
          [](QCustomPlot& plot) -> QObject* { return &plot; },
          [](QObject* o)
          {
              static QCPTheme dark; // outlives every plot of the test run
              dark.setBackground(Qt::black);
              qobject_cast<QCustomPlot*>(o)->setTheme(&dark);
          },
          { "themeBackground" },
          { } },
        { "QCPArea::setBackground(pixmap, scaled, mode)",
          [](QCustomPlot& plot) -> QObject* { return plot.axisRect().get(); },
          [](QObject* o)
          {
              static const QPixmap shared = []
              {
                  QPixmap pixmap(2, 2);
                  pixmap.fill(Qt::red);
                  return pixmap;
              }(); // one image: the second call moves nothing
              qobject_cast<QCPArea*>(o)->setBackground(shared, false, Qt::IgnoreAspectRatio);
          },
          { "background", "backgroundScaled", "backgroundScaledMode" },
          { } },
        { "QCPItem::setClipArea",
          [](QCustomPlot& plot) -> QObject* { return plot.add<QCPItemLine>().get(); },
          [](QObject* o)
          {
              auto* item = qobject_cast<QCPItem*>(o);
              QCustomPlot* plot = item->parentPlot();
              if (item->clipAxisRect().get() == plot->axisRect().get())
                  item->setClipArea(axisRectOn(*plot));
          },
          { "clipAxisRect" },
          { "clipToAxisRect" } },
        { "QCPLegend::setIconSize(int, int)",
          [](QCustomPlot& plot) -> QObject* { return plot.legend.get(); },
          [](QObject* o) { qobject_cast<QCPLegend*>(o)->setIconSize(41, 17); },
          { "iconSize" },
          { "iconTextPadding" } },
        { "QCPColorScale::setType moving rangeDrag",
          [](QCustomPlot& plot) -> QObject*
          {
              QCPColorScale* scale = colorScaleOn(plot);
              scale->setRangeDrag(false);
              scale->setRangeDrag(true); // the vertical orientation only
              return scale;
          },
          [](QObject* o) { qobject_cast<QCPColorScale*>(o)->setType(QCPAxis::atBottom); },
          { "type", "rangeDrag" },
          { } },
        { "QCPColorScale's axis label",
          [](QCustomPlot& plot) -> QObject* { return colorScaleOn(plot); },
          [](QObject* o)
          { qobject_cast<QCPColorScale*>(o)->axis()->setLabel(QStringLiteral("z")); },
          { "label" },
          { } },
        { "a link's source reverses",
          [](QCustomPlot& plot) -> QObject*
          {
              plot.xAxis2->linkTo(plot.xAxis);
              return plot.xAxis2;
          },
          [](QObject* o)
          { qobject_cast<QCPAxis*>(o)->parentPlot()->xAxis->setRangeReversed(true); },
          { "rangeReversed" },
          { } },
    };
    return list;
}
} // namespace

void TestMetaSurface::everyWriterBesidesTheSetterNotifies_data()
{
    QTest::addColumn<int>("writer");
    for (int i = 0; i < writers().size(); ++i)
        QTest::newRow(writers()[i].name) << i;
}

void TestMetaSurface::everyWriterBesidesTheSetterNotifies()
{
    QFETCH(int, writer);
    const Writer& w = writers()[writer];
    auto plot = newPlot();
    QObject* object = w.make(*plot);
    std::vector<std::unique_ptr<QSignalSpy>> moved, still;
    for (const char* name : w.moved)
        moved.push_back(
            std::make_unique<QSignalSpy>(object, metaProperty(object, name).notifySignal()));
    for (const char* name : w.still)
        still.push_back(
            std::make_unique<QSignalSpy>(object, metaProperty(object, name).notifySignal()));
    {
        QCPEmissionScope scope(plot.get());
        w.write(object);
        for (const auto& spy : moved)
            QCOMPARE(spy->count(), 0);
    }
    for (int i = 0; i < moved.size(); ++i)
        QVERIFY2(moved[i]->count() == 1, w.moved[i]);
    for (int i = 0; i < still.size(); ++i)
        QVERIFY2(still[i]->count() == 0, w.still[i]);
    if (!w.idempotent)
        return;
    w.write(object);
    for (int i = 0; i < moved.size(); ++i)
        QVERIFY2(moved[i]->count() == 1, w.moved[i]);
}

namespace
{
// A writer that moves several properties, or calls other setters, delivers no signal before it is
// done: a slot of the first signal sees every value the call writes (and may delete the object).
struct Nested
{
    const char* name;
    // Builds, connects a probe to the first signal and calls the writer, outside any scope.
    // Returns whether the probe ran and saw the call finished.
    std::function<bool(QCustomPlot&)> run;
};

template <typename Sender, typename Signal>
bool probeOnce(Sender* sender, Signal signal, std::function<void()> write,
               std::function<bool()> finished)
{
    bool ran = false, sawFinished = false;
    QObject::connect(sender, signal, sender,
                     [&]
                     {
                         if (!ran)
                             sawFinished = finished();
                         ran = true;
                     });
    write();
    return ran && sawFinished;
}

const QVector<Nested>& nestedWriters()
{
    static const QVector<Nested> list {
        { "QCustomPlot::setBackground(pixmap, scaled, mode)",
          [](QCustomPlot& plot)
          {
              QPixmap pixmap(2, 2);
              pixmap.fill(Qt::red);
              return probeOnce(
                  &plot, &QCustomPlot::backgroundChanged,
                  [&] { plot.setBackground(pixmap, true, Qt::IgnoreAspectRatio); },
                  [&] { return plot.backgroundScaledMode() == Qt::IgnoreAspectRatio; });
          } },
        { "QCPBarsGroup::clear",
          [](QCustomPlot& plot)
          {
              auto* group = new QCPBarsGroup(&plot);
              QCPBars* first = barsOn(plot);
              QCPBars* second = barsOn(plot);
              first->setBarsGroup(group);
              second->setBarsGroup(group);
              return probeOnce(
                  first, &QCPBars::barsGroupChanged, [&] { group->clear(); },
                  [&] { return second->barsGroup().get() == nullptr; });
          } },
        { "QCPLegend::setFont",
          [](QCustomPlot& plot)
          {
              graphOn(plot);
              graphOn(plot);
              const QFont font(QStringLiteral("Serif"), 17);
              QCPLegend* legend = plot.legend.get();
              return probeOnce(
                  legend->item(0).get(), &QCPLegendItem::fontChanged,
                  [&] { legend->setFont(font); },
                  [&] { return legend->item(1)->font() == font && legend->font() == font; });
          } },
        { "QCPAxis::setColor",
          [](QCustomPlot& plot)
          {
              QCPAxis* axis = plot.xAxis.get();
              return probeOnce(
                  axis, &QCPAxis::labelColorChanged, [&] { axis->setColor(Qt::red); },
                  [&] { return axis->subTickPen().color() == QColor(Qt::red); });
          } },
        { "QCPAxis::setTickLength",
          [](QCustomPlot& plot)
          {
              QCPAxis* axis = plot.xAxis.get();
              return probeOnce(
                  axis, &QCPAxis::tickLengthInChanged, [&] { axis->setTickLength(9, 8); },
                  [&] { return axis->tickLengthOut() == 8; });
          } },
        { "QCPPolarAxisAngular::setTickLength",
          [](QCustomPlot& plot)
          {
              QCPPolarAxisAngular* axis = polarAreaOn(plot)->angularAxis().get();
              return probeOnce(
                  axis, &QCPPolarAxisAngular::tickLengthInChanged,
                  [&] { axis->setTickLength(9, 8); }, [&] { return axis->tickLengthOut() == 8; });
          } },
        { "QCPPolarAxisRadial::setSubTickLength",
          [](QCustomPlot& plot)
          {
              QCPPolarAxisRadial* axis = polarAreaOn(plot)->radialAxis().get();
              return probeOnce(
                  axis, &QCPPolarAxisRadial::subTickLengthInChanged, [&]
                  { axis->setSubTickLength(9, 8); }, [&] { return axis->subTickLengthOut() == 8; });
          } },
        { "QCPColorScale::setType",
          [](QCustomPlot& plot)
          {
              QCPColorScale* scale = colorScaleOn(plot);
              scale->setLabel(QStringLiteral("z"));
              return probeOnce(
                  scale->axis().get(), &QCPAxis::ticksChanged,
                  [&] { scale->setType(QCPAxis::atBottom); },
                  [&]
                  {
                      return scale->axis()->label() == QStringLiteral("z")
                          && scale->type() == QCPAxis::atBottom;
                  });
          } },
    };
    return list;
}
} // namespace

void TestMetaSurface::nestedWriterSignalsWaitForTheWriter_data()
{
    QTest::addColumn<int>("writer");
    for (int i = 0; i < nestedWriters().size(); ++i)
        QTest::newRow(nestedWriters()[i].name) << i;
}

void TestMetaSurface::nestedWriterSignalsWaitForTheWriter()
{
    QFETCH(int, writer);
    auto plot = newPlot();
    QVERIFY(nestedWriters()[writer].run(*plot));
}

// E-4c's motive: a layout pass writes the outer rect of every element inside replot's scope. A
// listener hears the move once, after the replot, never in the middle of it.
void TestMetaSurface::layoutPassSignalsAfterTheReplot()
{
    auto plot = newPlot();
    settle(*plot);
    QCPAxisRect* rect = plot->axisRect().get();
    int heard = 0;
    bool duringReplot = false;
    QObject::connect(rect, &QCPLayoutElement::outerRectChanged, rect,
                     [&]
                     {
                         ++heard;
                         duringReplot = duringReplot || plot->isReplotting();
                     });
    plot->resize(500, 350);
    plot->replot();
    QCOMPARE(heard, 1);
    QVERIFY(!duringReplot);
    QCOMPARE(rect->outerRect(), QRect(0, 0, 500, 350));
}

void TestMetaSurface::themeSlotDeletingThePlot_data()
{
    QTest::addColumn<bool>("shared");
    QTest::newRow("the plot's own theme") << false;
    QTest::newRow("a theme two plots share") << true;
}

// E-5: the theme emits changed() at once, from a setter whose last action it is. A slot that
// deletes a plot reading the theme (its owner, when it is the plot's own) leaves nothing to run.
void TestMetaSurface::themeSlotDeletingThePlot()
{
    QFETCH(bool, shared);
    QCPTheme external;
    QPointer<QCustomPlot> doomed(newPlot().release());
    auto survivor = newPlot();
    if (shared)
    {
        doomed->setTheme(&external);
        survivor->setTheme(&external);
    }
    QCPTheme* theme = doomed->theme().get();
    QSignalSpy survivorTheme(survivor.get(), &QCustomPlot::themeChanged);
    int later = 0;
    QObject::connect(theme, &QCPTheme::changed, survivor.get(), [&] { delete doomed.data(); });
    QObject::connect(theme, &QCPTheme::changed, survivor.get(), [&] { ++later; });
    theme->setGrid(Qt::red);
    QVERIFY(doomed.isNull());
    QCOMPARE(later, shared ? 1 : 0); // the plot's own theme died with it, mid-emission
    QCOMPARE(survivorTheme.count(), shared ? 1 : 0);
}

namespace
{
// A function that calls emitting setters and then keeps using its objects holds their signals in
// one scope (review I1, I2). Each row's slot deletes the plot, from the first signal the function
// emits, outside any scope; under ASan a missing scope is a use-after-free.
struct Deleting
{
    const char* name;
    // Builds the scene and returns the object and the signal whose slot deletes the plot.
    std::function<std::pair<QObject*, const char*>(QCustomPlot&)> arm;
    std::function<void(QCustomPlot&)> write;
    bool deletesSender = false; // the slot deletes the object that signals, not the plot
};

QCPAxis* unlistedAxis = nullptr; // made by one row's arm, listed by its write
// Taken by one row's arm, given back by its write.
std::unique_ptr<QCPItemText> pendingItem;
std::unique_ptr<QCPGraph> pendingGraph;

std::pair<QObject*, const char*> armPendingItem(QCustomPlot& plot)
{
    QCPItemText* text = plot.add<QCPItemText>().get();
    pendingItem = plot.take(QCPHandle<QCPItemText>(text));
    return { pendingItem.get(), "layer" };
}

const QVector<Deleting>& deletingWriters()
{
    static const QVector<Deleting> list {
        // linkTo emits the follower's new scale: a slot deleting the follower must find linkTo
        // done with it.
        { "QCPAxisBase::linkTo", [](QCustomPlot& plot)
          {
              plot.xAxis->setScale(QCPScale::log());
              return std::pair<QObject*, const char*>(plot.xAxis2.get(), "scale");
          },
          [](QCustomPlot& plot) { plot.xAxis2->linkTo(plot.xAxis); } },
        { "QCustomPlot::setTheme", [](QCustomPlot& plot)
          { return std::pair<QObject*, const char*>(plot.xAxis.get(), "basePen"); },
          [](QCustomPlot& plot)
          {
              static QCPTheme* dark = QCPTheme::dark(); // outlives the plots of the run
              plot.setTheme(dark);
          } },
        { "QCustomPlot::setViewport", [](QCustomPlot& plot)
          { return std::pair<QObject*, const char*>(plot.plotLayout().get(), "outerRect"); },
          [](QCustomPlot& plot) { plot.setViewport(QRect(0, 0, 123, 45)); } },
        { "QCPBarsGroup::insert",
          [](QCustomPlot& plot)
          {
              QCPBars* bars = barsOn(plot);
              new QCPBarsGroup(&plot);
              return std::pair<QObject*, const char*>(bars, "barsGroup");
          },
          [](QCustomPlot& plot)
          {
              auto* group = plot.findChild<QCPBarsGroup*>();
              group->insert(0, qobject_cast<QCPBars*>(plot.plottable(0).get()));
          } },
        { "QCPLegend::addItem",
          [](QCustomPlot& plot)
          {
              plot.setAutoAddPlottableToLegend(false);
              graphOn(plot);
              return std::pair<QObject*, const char*>(plot.legend.get(), "rowStretchFactors");
          },
          [](QCustomPlot& plot) { plot.plottable(0)->addToLegend(); } },
        { "QCPLegend::removeItem",
          [](QCustomPlot& plot)
          {
              graphOn(plot);
              graphOn(plot);
              return std::pair<QObject*, const char*>(plot.legend.get(), "rowStretchFactors");
          },
          [](QCustomPlot& plot) { plot.legend->removeItem(0); } },
        { "QCustomPlot::detach(plottable)",
          [](QCustomPlot& plot)
          {
              QCPGraph* graph = graphOn(plot);
              auto* tracer = plot.add<QCPItemTracer>().get();
              tracer->setGraph(graph);
              return std::pair<QObject*, const char*>(tracer, "graph");
          },
          [](QCustomPlot& plot)
          {
              static std::unique_ptr<QCPGraph> taken; // the taken graph outlives the plot
              taken = plot.take(plot.graph(0));
          } },
        { "QCustomPlot::detach(item)",
          [](QCustomPlot& plot)
          {
              auto* line = plot.add<QCPItemLine>().get();
              return std::pair<QObject*, const char*>(line, "clipAxisRect");
          },
          [](QCustomPlot& plot)
          {
              static std::unique_ptr<QCPItemLine> taken;
              taken = plot.take(
                  QCPHandle<QCPItemLine>(qobject_cast<QCPItemLine*>(plot.item(0).get())));
          } },
        { "QCPAxisRect::addAxis(an existing axis)",
          [](QCustomPlot& plot)
          {
              unlistedAxis = new QCPAxis(plot.axisRect().get(), QCPAxis::atBottom);
              return std::pair<QObject*, const char*>(unlistedAxis, "lowerEnding");
          },
          [](QCustomPlot& plot) { plot.axisRect()->addAxis(QCPAxis::atBottom, unlistedAxis); } },
        { "QCPAxisRect::removeAxis",
          [](QCustomPlot& plot)
          {
              QCPAxis* second = plot.axisRect()->addAxis(QCPAxis::atBottom).get();
              plot.xAxis->setOffset(7);
              return std::pair<QObject*, const char*>(second, "offset");
          },
          [](QCustomPlot& plot) { plot.axisRect()->removeAxis(plot.xAxis.get()); } },
        { "QCustomPlot::add(item)", armPendingItem,
          [](QCustomPlot& plot) { plot.add(std::move(pendingItem)); } },
        { "QCustomPlot::add(item), deleting the item", armPendingItem,
          [](QCustomPlot& plot) { plot.add(std::move(pendingItem)); }, true },
        { "QCustomPlot::add(plottable)",
          [](QCustomPlot& plot)
          {
              pendingGraph = plot.take(QCPHandle<QCPGraph>(graphOn(plot)));
              return std::pair<QObject*, const char*>(plot.legend.get(), "rowStretchFactors");
          },
          [](QCustomPlot& plot) { plot.add(std::move(pendingGraph)); } },
        { "QCPPolarAxisAngular::setTickLength",
          [](QCustomPlot& plot)
          {
              return std::pair<QObject*, const char*>(polarAreaOn(plot)->angularAxis().get(),
                                                      "tickLengthIn");
          },
          [](QCustomPlot& plot)
          { plot.findChild<QCPPolarArea*>()->angularAxis()->setTickLength(9, 8); } },
        { "QCPPolarAxisRadial::setSubTickLength",
          [](QCustomPlot& plot)
          {
              return std::pair<QObject*, const char*>(polarAreaOn(plot)->radialAxis().get(),
                                                      "subTickLengthIn");
          },
          [](QCustomPlot& plot)
          { plot.findChild<QCPPolarArea*>()->radialAxis()->setSubTickLength(9, 8); } },
        { "QCustomPlot::setAntialiasedElements moving both sets",
          [](QCustomPlot& plot)
          {
              plot.setAntialiasedElements(QCP::aeNone);
              plot.setNotAntialiasedElements(QCP::aeAxes);
              return std::pair<QObject*, const char*>(&plot, "antialiasedElements");
          },
          [](QCustomPlot& plot) { plot.setAntialiasedElements(QCP::aeAxes); } },
        { "QCustomPlot::setCreationModeEnabled while drawing",
          [](QCustomPlot& plot)
          {
              plot.setItemCreator([](QCustomPlot* p, QCPAxis*, QCPAxis*)
                                  { return p->add<QCPItemSpan>(Qt::Vertical); });
              plot.setCreationModeEnabled(true);
              plot.replot();
              const QPoint center = plot.axisRect()->rect().center();
              QMouseEvent press(QEvent::MouseButtonPress, center, plot.mapToGlobal(center),
                                Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
              QMouseEvent release(QEvent::MouseButtonRelease, center, plot.mapToGlobal(center),
                                  Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
              QApplication::sendEvent(&plot, &press);
              QApplication::sendEvent(&plot, &release);
              return std::pair<QObject*, const char*>(&plot, "itemCanceled()");
          },
          [](QCustomPlot& plot) { plot.setCreationModeEnabled(false); } },
        { "QCPItemTracer::setGraph with a component", [](QCustomPlot& plot)
          { return std::pair<QObject*, const char*>(plot.add<QCPItemTracer>().get(), "graph"); },
          [](QCustomPlot& plot)
          {
              auto* tracer = plot.findChild<QCPItemTracer*>();
              tracer->setGraph(twoColumnGraphOn(plot), 1);
          } },
        { "QCPItem::setClipArea", [](QCustomPlot& plot)
          { return std::pair<QObject*, const char*>(plot.add<QCPItemLine>().get(), "clipArea"); },
          [](QCustomPlot& plot)
          {
              QCPAxisRect* rect = axisRectOn(plot);
              plot.findChild<QCPItemLine*>()->setClipArea(rect);
          } },
        { "QCPOverlay::setCollapsible uncollapsing",
          [](QCustomPlot& plot)
          {
              plot.overlay()->setCollapsible(true);
              plot.overlay()->setCollapsed(true);
              return std::pair<QObject*, const char*>(plot.overlay().get(), "collapsible");
          },
          [](QCustomPlot& plot) { plot.overlay()->setCollapsible(false); } },
        { "QCPPlottable::setKeyAxis dropping the system", [](QCustomPlot& plot)
          { return std::pair<QObject*, const char*>(graphOn(plot), "system"); },
          [](QCustomPlot& plot) { plot.graph(0)->setKeyAxis(nullptr); } },
        { "QCPPlottable::setWantsLegend",
          [](QCustomPlot& plot)
          {
              graphOn(plot);
              graphOn(plot);
              return std::pair<QObject*, const char*>(plot.legend.get(), "rowStretchFactors");
          },
          [](QCustomPlot& plot) { plot.graph(0)->setWantsLegend(false); } },
        { "QCPHistogram::setBins",
          [](QCustomPlot& plot)
          {
              return std::pair<QObject*, const char*>(settledHistogramOn(plot),
                                                      "busyChanged(bool)");
          },
          [](QCustomPlot& plot)
          { qobject_cast<QCPHistogram*>(plot.plottable(0).get())->setBins(23); } },
        { "QCPHistogram::setNormalization",
          [](QCustomPlot& plot) { return armedOnBusy(settledHistogramOn(plot)); },
          [](QCustomPlot& plot)
          {
              firstPlottable<QCPHistogram>(plot)->setNormalization(
                  QCP::HistogramNormalization::Density);
          } },
        { "QCPColorMap2::setGapThreshold",
          [](QCustomPlot& plot) { return armedOnBusy(settledColorMap2On(plot)); },
          [](QCustomPlot& plot) { firstPlottable<QCPColorMap2>(plot)->setGapThreshold(4.5); } },
        { "QCPWaterfallGraph::setOffsetMode",
          [](QCustomPlot& plot) { return armedOnBusy(settledWaterfallOn(plot)); },
          [](QCustomPlot& plot)
          {
              firstPlottable<QCPWaterfallGraph>(plot)->setOffsetMode(QCPWaterfallGraph::omCustom);
          } },
        { "QCPWaterfallGraph::setUniformSpacing", [](QCustomPlot& plot)
          { return armedOnBusy(settledWaterfallOn(plot)); }, [](QCustomPlot& plot)
          { firstPlottable<QCPWaterfallGraph>(plot)->setUniformSpacing(7.5); } },
        { "QCPWaterfallGraph::setOffsets", [](QCustomPlot& plot)
          { return armedOnBusy(settledWaterfallOn(plot)); }, [](QCustomPlot& plot)
          { firstPlottable<QCPWaterfallGraph>(plot)->setOffsets({ 1, 5 }); } },
        { "QCPWaterfallGraph::setNormalize",
          [](QCustomPlot& plot) { return armedOnBusy(settledWaterfallOn(plot)); },
          [](QCustomPlot& plot)
          {
              auto* waterfall = firstPlottable<QCPWaterfallGraph>(plot);
              waterfall->setNormalize(!waterfall->normalize());
          } },
        { "QCPWaterfallGraph::setGain",
          [](QCustomPlot& plot) { return armedOnBusy(settledWaterfallOn(plot)); },
          [](QCustomPlot& plot) { firstPlottable<QCPWaterfallGraph>(plot)->setGain(3.5); } },
        { "QCPIntervals::setStackMode", [](QCustomPlot& plot)
          { return std::pair<QObject*, const char*>(intervalsOn(plot), "stackMode"); },
          [](QCustomPlot& plot)
          { firstPlottable<QCPIntervals>(plot)->setStackMode(QCPIntervals::skTime); } },
        { "QCPIntervals::setSelection", [](QCustomPlot& plot)
          { return std::pair<QObject*, const char*>(intervalsOn(plot), "selection"); },
          [](QCustomPlot& plot)
          { plot.plottable(0)->setSelection(QCPDataSelection(QCPDataRange(1, 2))); } },
        { "QCPPlottableGroup::removeMember",
          [](QCustomPlot& plot)
          {
              QCPPlottableGroup* group = plot.addPlottableGroup(QStringLiteral("group")).get();
              group->addMember(graphOn(plot));
              return std::pair<QObject*, const char*>(group, "membersChanged()");
          },
          [](QCustomPlot& plot)
          { plot.plottableGroups().first()->removeMember(plot.plottable(0).get()); } },
        { "QCPPolarAxisAngular::setAngle",
          [](QCustomPlot& plot)
          {
              QCPPolarArea* area = polarAreaOn(plot);
              QCPCoordinateSystem* system
                  = area->system(area->angularAxis().get(), area->radialAxis().get()).get();
              return std::pair<QObject*, const char*>(system, "changed()");
          },
          [](QCustomPlot& plot) { plot.findChild<QCPPolarArea*>()->angularAxis()->setAngle(30); } },
        { "QCPPolarAxisRadial::setAngleReference",
          [](QCustomPlot& plot)
          {
              QCPPolarArea* area = polarAreaOn(plot);
              QCPCoordinateSystem* system
                  = area->system(area->angularAxis().get(), area->radialAxis().get()).get();
              return std::pair<QObject*, const char*>(system, "changed()");
          },
          [](QCustomPlot& plot)
          {
              plot.findChild<QCPPolarArea*>()->radialAxis()->setAngleReference(
                  QCPPolarAxisRadial::arAbsolute);
          } },
        { "QCPPlottableGroup::setVisible",
          [](QCustomPlot& plot)
          {
              QCPPlottableGroup* group = plot.addPlottableGroup(QStringLiteral("group")).get();
              QCPGraph* graph = twoColumnGraphOn(plot);
              group->addMember(graph);
              graph->setComponentVisible(0, false);
              graph->setComponentVisible(1, false);
              return std::pair<QObject*, const char*>(group, "visible");
          },
          [](QCustomPlot& plot) { plot.plottableGroups().first()->setVisible(true); } },
        { "QCPItemPosition::setType on a span edge",
          [](QCustomPlot& plot)
          {
              plot.replot(); // the edge keeps its pixel: the axis rect needs its size
              return std::pair<QObject*, const char*>(plot.add<QCPItemRSpan>().get(), "keyRange");
          },
          [](QCustomPlot& plot)
          { plot.findChild<QCPItemRSpan*>()->leftEdge->setType(QCPItemPosition::ptAbsolute); } },
        { "QCPItemPosition::setParentAnchor on a span edge",
          [](QCustomPlot& plot)
          {
              plot.replot();
              QCPItemText* text = plot.add<QCPItemText>().get();
              text->position->setCoords(3, 3);
              return std::pair<QObject*, const char*>(plot.add<QCPItemRSpan>().get(), "keyRange");
          },
          [](QCustomPlot& plot)
          {
              plot.findChild<QCPItemRSpan*>()->leftEdge->setParentAnchor(
                  plot.findChild<QCPItemText*>()->position, true);
          } },
        { "QCPItemPosition::set on a span edge",
          [](QCustomPlot& plot)
          {
              plot.replot();
              return std::pair<QObject*, const char*>(plot.add<QCPItemRSpan>().get(), "keyRange");
          },
          [](QCustomPlot& plot)
          { plot.findChild<QCPItemRSpan*>()->leftEdge->set(QCP::Pixels { 5, 5 }); } },
        { "QCustomPlot::removePlottable", armedOnTheLegendOfTwo,
          [](QCustomPlot& plot) { (void)plot.removePlottable(plot.plottable(0).get()); } },
        { "QCustomPlot::clearPlottables", armedOnTheLegendOfTwo,
          [](QCustomPlot& plot) { plot.clearPlottables(); } },
        { "QCustomPlot::removeGraph", armedOnTheLegendOfTwo,
          [](QCustomPlot& plot) { (void)plot.removeGraph(0); } },
        { "deleting a plottable", armedOnTheLegendOfTwo,
          [](QCustomPlot& plot) { delete plot.plottable(0).get(); } },
        { "QCPPolarArea::removeRadialAxis",
          [](QCustomPlot& plot)
          {
              return std::pair<QObject*, const char*>(polarGraphOnASecondRadialAxis(plot),
                                                      "system");
          },
          [](QCustomPlot& plot)
          {
              QCPPolarArea* area = plot.findChild<QCPPolarArea*>();
              (void)area->removeRadialAxis(area->radialAxes().last().get());
          },
          true },
        { "deleting a polar area",
          [](QCustomPlot& plot)
          {
              return std::pair<QObject*, const char*>(polarGraphOnASecondRadialAxis(plot),
                                                      "system");
          },
          [](QCustomPlot& plot) { delete plot.findChild<QCPPolarArea*>(); }, true },
        { "deleting an axis",
          [](QCustomPlot& plot)
          {
              auto* graph = plot.xAxis2->axisRect()->add<QCPGraph>(plot.xAxis2, plot.yAxis).get();
              return std::pair<QObject*, const char*>(graph, "system");
          },
          [](QCustomPlot& plot) { delete plot.xAxis2.get(); }, true },
        { "QCPGraph::setColorValues joining the rect's colour scale", [](QCustomPlot& plot)
          { return std::pair<QObject*, const char*>(graphOn(plot), "colorScaleChanged()"); },
          [](QCustomPlot& plot) { plot.graph(0)->setColorValues(std::vector<double> { 1, 2, 3 }); },
          true },
        { "QCustomPlot::overlay on an existing notification layer",
          [](QCustomPlot& plot)
          {
              (void)plot.addLayer(QStringLiteral("notification"));
              return std::pair<QObject*, const char*>(
                  plot.layer(QStringLiteral("notification")).get(), "mode");
          },
          [](QCustomPlot& plot) { (void)plot.overlay(); } },
        { "QCPAxisRect::setupFullAxesBox", [](QCustomPlot& plot)
          { return std::pair<QObject*, const char*>(plot.xAxis2.get(), "visible"); },
          [](QCustomPlot& plot) { plot.axisRect()->setupFullAxesBox(); } },
        { "QCPGraph::setComponentErrorBars hiding both bounds", [](QCustomPlot& plot)
          {
              return std::pair<QObject*, const char*>(threeColumnGraphOn(plot),
                                                      "componentVisibilityChanged()");
          },
          [](QCustomPlot& plot) { (void)plot.graph(0)->setComponentErrorBars(0, 1, 2); }, true },
        { "QCPGraph::clearComponentErrorBars showing both bounds",
          [](QCustomPlot& plot)
          {
              QCPGraph* graph = threeColumnGraphOn(plot);
              (void)graph->setComponentErrorBars(0, 1, 2);
              return std::pair<QObject*, const char*>(graph, "componentVisibilityChanged()");
          },
          [](QCustomPlot& plot) { plot.graph(0)->clearComponentErrorBars(0); }, true },
    };
    return list;
}
} // namespace

void TestMetaSurface::slotDeletingThePlotDuringAWriter_data()
{
    QTest::addColumn<int>("writer");
    for (int i = 0; i < deletingWriters().size(); ++i)
        QTest::newRow(deletingWriters()[i].name) << i;
}

void TestMetaSurface::slotDeletingThePlotDuringAWriter()
{
    QFETCH(int, writer);
    const Deleting& d = deletingWriters()[writer];
    QPointer<QCustomPlot> plot(newPlot().release());
    const auto [sender, property] = d.arm(*plot);
    QPointer<QObject> victim(d.deletesSender ? sender : static_cast<QObject*>(plot.data()));
    MetaTrigger trigger;
    // Once: the plot's teardown sends more signals, and a QPointer is cleared only by ~QObject.
    bool fired = false;
    trigger.action = [&victim, &fired]
    {
        if (std::exchange(fired, true))
            return;
        delete victim.data();
    };
    connectTrigger(sender, property, &trigger);
    d.write(*plot);
    QVERIFY2(victim.isNull(), "the signal never fired");
    delete plot.data();
}

void TestMetaSurface::exceptionsNameRealProperties()
{
    for (const Exception& e : exceptions())
        QVERIFY2(e.meta->indexOfProperty(e.property) >= 0,
                 qPrintable(QStringLiteral("%1::%2 is no property")
                                .arg(QLatin1String(e.meta->className()),
                                     QLatin1String(e.property))));
}

namespace
{
// The form table (docs/plans/neoqcp-3/m5e-forms.md), sections 3.1 and 3.3, row by row.
enum class Form
{
    Plain, // writable, NOTIFY
    Constant, // CONSTANT, read-only
    Derived, // read-only, NOTIFY
    Gadget, // writable, no NOTIFY, on a Q_GADGET
    Off, // dropped or out: no property of that name
};

struct FormRow
{
    const QMetaObject* meta;
    const char* property;
    Form form;
    const char* notify = nullptr; // the NOTIFY's name when it is not <property>Changed
};

QVector<FormRow> plainRows(const QMetaObject* meta, std::initializer_list<const char*> names)
{
    QVector<FormRow> rows;
    for (const char* name : names)
        rows.append({ meta, name, Form::Plain });
    return rows;
}

QVector<FormRow> polarAxisRows(const QMetaObject* meta)
{
    return plainRows(meta,
                     { "angle",
                       "ticks",
                       "tickLabels",
                       "tickLabelPadding",
                       "tickLabelFont",
                       "tickLabelColor",
                       "tickLabelRotation",
                       "tickLabelMode",
                       "numberFormat",
                       "numberPrecision",
                       "tickLengthIn",
                       "tickLengthOut",
                       "subTicks",
                       "subTickLengthIn",
                       "subTickLengthOut",
                       "basePen",
                       "tickPen",
                       "subTickPen",
                       "labelFont",
                       "labelColor",
                       "label",
                       "labelPadding",
                       "selectedTickLabelFont",
                       "selectedLabelFont",
                       "selectedTickLabelColor",
                       "selectedLabelColor",
                       "selectedBasePen",
                       "selectedTickPen",
                       "selectedSubTickPen" });
}

const QVector<FormRow>& formRows()
{
    static const QVector<FormRow> rows = []
    {
        QVector<FormRow> r;
        r += plainRows(&QCPAxisBase::staticMetaObject, { "rangeDraggable", "rangeZoomable" });
        r += plainRows(&QCPAxis::staticMetaObject, { "labelRenderer" });
        r += polarAxisRows(&QCPPolarAxisAngular::staticMetaObject);
        r += polarAxisRows(&QCPPolarAxisRadial::staticMetaObject);
        r += plainRows(&QCPPolarAxisRadial::staticMetaObject, { "angleReference" });
        r += plainRows(&QCPPolarArea::staticMetaObject, { "rangeZoomFactor" });
        r += plainRows(&QCPPolarGraph::staticMetaObject,
                       { "periodic", "lineStyle", "scatterStyle" });
        r += { { &QCPPolarGraph::staticMetaObject, "data", Form::Off } };
        r += plainRows(&QCustomPlot::staticMetaObject,
                       { "creationModeEnabled", "creationModifier", "bufferDevicePixelRatio",
                         "antialiasedElements", "notAntialiasedElements", "interactions",
                         "plottingHints", "selectionRectMode", "selectionRect",
                         "skipReplotsWhenHidden", "itemIndexing", "dataSwapDebounceMs",
                         "dataSwapMaxWaitMs", "currentLayer" });
        r += plainRows(&QCPItemRichText::staticMetaObject, { "cornerRadius" });
        r += plainRows(&QCPItemRSpan::staticMetaObject, { "keyRange", "valueRange" });
        r += plainRows(&QCPItemSpan::staticMetaObject, { "range" });
        r += plainRows(&QCPSpanItem::staticMetaObject,
                       { "brush", "selectedBrush", "borderPen", "selectedBorderPen", "movable" });
        r += plainRows(&QCPItemTracer::staticMetaObject, { "component" });
        r += plainRows(&QCPItem::staticMetaObject, { "clipArea" });
        for (const char* name :
             { "type", "typeX", "typeY", "coords", "axisRect", "system", "area", "pixelPosition" })
            r += { { &QCPItemPosition::staticMetaObject, name, Form::Off } };
        // A QVector, not a nested initializer_list: the inner lists' arrays would end with the
        // full expression that builds the outer one, before the loop reads them.
        for (const auto& [meta, names] :
             QVector<std::pair<const QMetaObject*, QVector<const char*>>> {
                 { &QCPColorGradient::staticMetaObject,
                   { "levelCount", "colorStops", "colorInterpolation", "nanHandling", "nanColor",
                     "periodic" } },
                 { &QCPLineEnding::staticMetaObject, { "style", "width", "length", "inverted" } },
                 { &QCPScatterStyle::staticMetaObject,
                   { "size", "shape", "pen", "brush", "pixmap", "customPath" } } })
            for (const char* name : names)
                r += { { meta, name, Form::Gadget } };
        for (const auto& [meta, names] :
             QVector<std::pair<const QMetaObject*, QVector<const char*>>> {
                 { &QCPSelectionDecorator::staticMetaObject,
                   { "pen", "brush", "usedScatterProperties" } },
                 { &QCPSelectionDecoratorBracket::staticMetaObject,
                   { "bracketPen", "bracketBrush", "bracketWidth", "bracketHeight", "bracketStyle",
                     "tangentToData", "tangentAverage" } },
                 { &QCPPainter::staticMetaObject, { "antialiasing", "modes" } },
                 { &QCPAsyncPipelineBase::staticMetaObject, { "scheduler" } },
                 { &QCPPipelineScheduler::staticMetaObject, { "maxThreads" } } })
            for (const char* name : names)
                r += { { meta, name, Form::Off } };
        for (const char* name : { "area", "keyAxis", "valueAxis", "capabilities" })
            r += { { &QCPCoordinateSystem::staticMetaObject, name, Form::Constant } };
        r += { { &QCPCoordinateSystem::staticMetaObject, "revision", Form::Derived, "changed" },
               { &QCPCoordinateSystem::staticMetaObject, "transform", Form::Off },
               { &QCPCoordinateSystem::staticMetaObject, "plottables", Form::Off },
               { &QCPArea::staticMetaObject, "insetLayout", Form::Constant },
               { &QCPArea::staticMetaObject, "systems", Form::Off },
               { &QCPArea::staticMetaObject, "systemsListen", Form::Off },
               { &QCPAlignmentGroup::staticMetaObject, "sides", Form::Constant },
               { &QCPAlignmentGroup::staticMetaObject, "elements", Form::Off },
               { &QCPAxisBase::staticMetaObject, "linkSource", Form::Off },
               { &QCPAxisBase::staticMetaObject, "link", Form::Off } };
        r += plainRows(&QCPPlottable::staticMetaObject, { "system", "wantsLegend" });
        r += { { &QCPPlottable::staticMetaObject, "busy", Form::Off } };
        r += plainRows(&QCPColorMap2::staticMetaObject,
                       { "gapThreshold", "contourLevels", "contourPen", "contourLabelEnabled" });
        r += plainRows(&QCPGraph::staticMetaObject, { "deferredDataSwap", "gapThreshold" });
        r += plainRows(&QCPPlottableGroup::staticMetaObject, { "name", "visible" });
        r += plainRows(&QCPHistogram::staticMetaObject,
                       { "bins", "normalization", "style", "columnColors" });
        r += plainRows(&QCPHistogram2D::staticMetaObject,
                       { "normalization", "columnNormalized", "contourLevels", "contourPen",
                         "contourLabelEnabled" });
        r += plainRows(
            &QCPIntervals::staticMetaObject,
            { "stackMode", "forbidOverlap", "style", "selectedRows", "editable", "editModes" });
        r += plainRows(&QCPWaterfallGraph::staticMetaObject,
                       { "offsetMode", "uniformSpacing", "offsets", "normalize", "gain" });
        for (const QMetaObject* meta :
             { &QCPBars::staticMetaObject, &QCPCurve::staticMetaObject,
               &QCPFinancial::staticMetaObject, &QCPStatisticalBox::staticMetaObject })
            r += { { meta, "data", Form::Off } };
        r += plainRows(&QCPLayoutElement::staticMetaObject, { "autoMargins" });
        r += plainRows(&QCPGroupLegendItem::staticMetaObject, { "expanded", "selectedComponent" });
        r += plainRows(&QCPTextElement::staticMetaObject, { "textFlags" });
        r += plainRows(&QCPOverlay::staticMetaObject,
                       { "collapsible", "collapsed", "opacity", "font" });
        r += plainRows(&QCPSelectionRect::staticMetaObject, { "pen", "brush" });
        r += { { &QCustomPlot::staticMetaObject, "theme", Form::Plain, "themeChanged" },
               { &QCustomPlot::staticMetaObject, "itemCreator", Form::Off },
               { &QCustomPlot::staticMetaObject, "itemPositioner", Form::Off } };
        r += plainRows(&QCPPolarGrid::staticMetaObject,
                       { "radialAxis", "type", "subGridType", "antialiasedSubGrid",
                         "antialiasedZeroLine", "angularPen", "angularSubGridPen", "radialPen",
                         "radialSubGridPen", "radialZeroLinePen" });
        for (const char* name : { "tickStepStrategy", "tickCount", "tickOrigin" })
            r += { { &QCPAxisTicker::staticMetaObject, name, Form::Off } };
        for (const char* name : { "tickStep", "scaleStrategy" })
            r += { { &QCPAxisTickerFixed::staticMetaObject, name, Form::Off } };
        for (const char* name : { "piSymbol", "piValue", "periodicity", "fractionStyle" })
            r += { { &QCPAxisTickerPi::staticMetaObject, name, Form::Off } };
        r += { { &QCPAxisTickerTime::staticMetaObject, "timeFormat", Form::Off } };
        for (const char* name :
             { "anchorSide", "anchorMode", "anchorReference", "anchorReferenceType", "font",
               "color", "padding", "rotation", "substituteExponent", "multiplicationSymbol",
               "abbreviateDecimalPowers", "cacheSize" })
            r += { { &QCPLabelPainterPrivate::staticMetaObject, name, Form::Off } };
        return r;
    }();
    return rows;
}
} // namespace

void TestMetaSurface::everyFormRowHolds_data()
{
    QTest::addColumn<int>("row");
    for (int i = 0; i < formRows().size(); ++i)
        QTest::addRow("%s::%s", formRows()[i].meta->className(), formRows()[i].property) << i;
}

void TestMetaSurface::everyFormRowHolds()
{
    QFETCH(int, row);
    const FormRow& r = formRows()[row];
    const int index = r.meta->indexOfProperty(r.property);
    if (r.form == Form::Off)
    {
        QVERIFY2(index < 0, "a property, against the table");
        return;
    }
    QVERIFY2(index >= 0, "no such property");
    QVERIFY2(declaringClass(r.meta, index) == r.meta, "declared by another class");
    const QMetaProperty property = r.meta->property(index);
    switch (r.form)
    {
        case Form::Plain:
            QVERIFY(property.isWritable());
            QVERIFY(property.hasNotifySignal());
            QCOMPARE(property.notifySignal().name(),
                     r.notify ? QByteArray(r.notify) : QByteArray(r.property) + "Changed");
            // A signal that carries a value carries the property's type (themeChanged() carries
            // none).
            if (property.notifySignal().parameterCount() > 0)
                QCOMPARE(property.notifySignal().parameterMetaType(0), property.metaType());
            break;
        case Form::Constant:
            QVERIFY(property.isConstant());
            QVERIFY(!property.isWritable());
            break;
        case Form::Derived:
            QVERIFY(!property.isWritable());
            QVERIFY(property.hasNotifySignal());
            QCOMPARE(property.notifySignal().name(), QByteArray(r.notify));
            break;
        case Form::Gadget:
            QVERIFY(property.isWritable());
            QVERIFY(!property.hasNotifySignal());
            break;
        case Form::Off:
            break;
    }
}

namespace
{
// E-7: the three value types whose properties are gadget ones, each from its default value.
const QVector<std::pair<const char*, QVariant>>& gadgets()
{
    static const QVector<std::pair<const char*, QVariant>> list {
        { "QCPScatterStyle", QVariant::fromValue(QCPScatterStyle()) },
        { "QCPLineEnding", QVariant::fromValue(QCPLineEnding()) },
        { "QCPColorGradient", QVariant::fromValue(QCPColorGradient()) },
    };
    return list;
}
} // namespace

void TestMetaSurface::everyGadgetPropertyReadsWhatItWrites_data()
{
    QTest::addColumn<int>("gadget");
    QTest::addColumn<int>("property");
    for (int g = 0; g < gadgets().size(); ++g)
    {
        const QMetaObject* meta = gadgets()[g].second.metaType().metaObject();
        for (int i = 0; i < meta->propertyCount(); ++i)
            QTest::addRow("%s::%s", gadgets()[g].first, meta->property(i).name()) << g << i;
    }
}

// The gadget properties carry no signal (E-7): a write through the meta-object stores the value
// a read gives back.
void TestMetaSurface::everyGadgetPropertyReadsWhatItWrites()
{
    QFETCH(int, gadget);
    QFETCH(int, property);
    QVariant value = gadgets()[gadget].second;
    const QMetaProperty meta = value.metaType().metaObject()->property(property);
    QVERIFY(meta.isWritable());
    QVERIFY(!meta.hasNotifySignal());
    const QVariant current = meta.readOnGadget(value.constData());
    const QVariant other = differentValue(meta, current, nullptr);
    QVERIFY2(other.isValid(), meta.typeName());
    QVERIFY(!same(other, current));
    QVERIFY(meta.writeOnGadget(value.data(), other));
    QVERIFY(same(meta.readOnGadget(value.constData()), other));
}

// A slot of a span's range signal that moves an edge in turn signals again: the writer's quiet
// guard covers its own edge writes only.
// A deferred signal's pointer argument names an object that may die before delivery: the slot gets
// null rather than a dangling pointer.
void TestMetaSurface::deferredPointerToADeadObjectArrivesNull()
{
    auto plot = newPlot();
    QCPItemTracer* tracer = plot->add<QCPItemTracer>().get();
    QCPGraph* graph = graphOn(*plot);
    QSignalSpy spy(tracer, &QCPItemTracer::graphChanged);
    {
        QCPEmissionScope scope(plot.get());
        tracer->setGraph(graph);
        delete graph;
    }
    QVERIFY(spy.count() >= 1);
    // QVERIFY, not QCOMPARE: a failing QCOMPARE would print the dangling object.
    QVERIFY(spy.first().first().value<QCPGraph*>() == nullptr);
}

void TestMetaSurface::spanSlotMovingAnEdgeIsHeard()
{
    auto plot = newPlot();
    QCPItemRSpan* rspan = plot->add<QCPItemRSpan>().get();
    QList<QCPRange> keyRanges;
    QObject::connect(rspan, &QCPItemRSpan::keyRangeChanged, rspan,
                     [&](const QCPRange& range)
                     {
                         keyRanges.append(range);
                         if (keyRanges.size() == 1)
                             rspan->rightEdge->setCoords(50, 0);
                     });
    rspan->setKeyRange(QCPRange(2, 3));
    QCOMPARE(keyRanges.size(), 2);
    QCOMPARE(keyRanges.last(), QCPRange(2, 50));

    QCPItemSpan* span = plot->add<QCPItemSpan>(Qt::Vertical).get();
    QList<QCPRange> ranges;
    QObject::connect(span, &QCPItemSpan::rangeChanged, span,
                     [&](const QCPRange& range)
                     {
                         ranges.append(range);
                         if (ranges.size() == 1)
                             span->upperEdge->setCoords(40, 1);
                     });
    span->setRange(QCPRange(2, 3));
    QCOMPARE(ranges.size(), 2);
    QCOMPARE(ranges.last(), QCPRange(2, 40));
}

// A gadget write goes through the setter, so its rules hold: a gradient keeps two levels at least.
void TestMetaSurface::gadgetSettersKeepTheirRules()
{
    QVariant gradient = QVariant::fromValue(QCPColorGradient());
    const QMetaObject* meta = gradient.metaType().metaObject();
    const QMetaProperty levelCount = meta->property(meta->indexOfProperty("levelCount"));
    QVERIFY(levelCount.writeOnGadget(gradient.data(), QVariant(1)));
    QCOMPARE(levelCount.readOnGadget(gradient.constData()).toInt(), 2);
}

// The two antialiasing sets never share an element: forcing one on takes it out of the other set,
// and only it.
void TestMetaSurface::antialiasingSetsStayApart()
{
    auto plot = newPlot();
    plot->setAntialiasedElements(QCP::aeNone);
    plot->setNotAntialiasedElements(QCP::aeAxes | QCP::aeGrid);
    plot->setAntialiasedElements(QCP::aeAxes);
    QCOMPARE(plot->antialiasedElements(), QCP::AntialiasedElements(QCP::aeAxes));
    QCOMPARE(plot->notAntialiasedElements(), QCP::AntialiasedElements(QCP::aeGrid));

    plot->setNotAntialiasedElements(QCP::aeAxes | QCP::aeGrid);
    QCOMPARE(plot->antialiasedElements(), QCP::AntialiasedElements(QCP::aeNone));

    plot->setAntialiasedElements(QCP::aeAxes | QCP::aeLegend);
    plot->setNotAntialiasedElement(QCP::aeAxes, true);
    QCOMPARE(plot->antialiasedElements(), QCP::AntialiasedElements(QCP::aeLegend));
    QCOMPARE(plot->notAntialiasedElements(), QCP::AntialiasedElements(QCP::aeAxes | QCP::aeGrid));

    plot->setAntialiasedElement(QCP::aeGrid, true);
    QCOMPARE(plot->antialiasedElements(), QCP::AntialiasedElements(QCP::aeLegend | QCP::aeGrid));
    QCOMPARE(plot->notAntialiasedElements(), QCP::AntialiasedElements(QCP::aeAxes));
}

void TestMetaSurface::polarNumberFormatIsWhatIsDrawn_data()
{
    QTest::addColumn<bool>("radial");
    QTest::newRow("angular") << false;
    QTest::newRow("radial") << true;
}

// The format a polar axis reports is the one it draws: writing it back draws the same picture.
// The labels need a decimal power to show the multiplication sign.
void TestMetaSurface::polarNumberFormatIsWhatIsDrawn()
{
    QFETCH(bool, radial);
    auto plot = newPlot();
    QCPPolarArea* area = polarAreaOn(*plot);
    QCPAxisBase* axis = radial ? static_cast<QCPAxisBase*>(area->radialAxis().get())
                               : static_cast<QCPAxisBase*>(area->angularAxis().get());
    axis->setRange(0, 4e7);
    // The angular axis ticks every 30 by default: a few ticks, not a million.
    if (auto fixed = axis->ticker().dynamicCast<QCPAxisTickerFixed>())
        fixed->setTickStep(1e7);
    const QImage before = plot->toPixmap(400, 300).toImage();
    const QVariant reported = axis->property("numberFormat");
    QVERIFY(axis->setProperty("numberFormat", QStringLiteral("e")));
    QVERIFY(axis->setProperty("numberFormat", reported));
    QCOMPARE(axis->property("numberFormat"), reported);
    QCOMPARE(plot->toPixmap(400, 300).toImage(), before);
}

void TestMetaSurface::revisionNotifiesThroughChanged_data()
{
    QTest::addColumn<bool>("polar");
    QTest::newRow("cartesian") << false;
    QTest::newRow("polar") << true;
}

// The derived revision (section 3.3): a binding on it hears the mapping move through changed(),
// whose listener count the binding's connection raises.
void TestMetaSurface::revisionNotifiesThroughChanged()
{
    QFETCH(bool, polar);
    auto plot = newPlot();
    QCPPolarArea* area = polar ? polarAreaOn(*plot) : nullptr;
    QCPCoordinateSystem* system = polar
        ? area->system(area->angularAxis().get(), area->radialAxis().get()).get()
        : plot->axisRect()->system(plot->xAxis, plot->yAxis).get();
    QBindable<quint64> revision(system, "revision");
    QVERIFY(revision.isValid());
    const quint64 before = revision.value();
    int notified = 0;
    const QPropertyNotifier notifier = revision.addNotifier([&] { ++notified; });
    if (polar)
        area->radialAxis()->setRange(0, 7);
    else
        plot->xAxis->setRange(3, 7);
    QVERIFY(notified >= 1);
    QVERIFY(revision.value() != before);
    QCOMPARE(revision.value(), system->revision());
}

void TestMetaSurface::everyNotifyPropertyNotifiesOncePerChange_data()
{
    QTest::addColumn<int>("fixture");
    QTest::addColumn<QByteArray>("property");
    for (int f = 0; f < fixtures().size(); ++f)
    {
        auto plot = newPlot();
        const QMetaObject* meta = fixtures()[f].make(*plot)->metaObject();
        // QObject's own objectName is Qt's, with its own BINDABLE storage: not ours to check.
        for (int i = QObject::staticMetaObject.propertyCount(); i < meta->propertyCount(); ++i)
            // A property a subclass declares again (the curve's) is reached by name only there.
            if (isNotifyProperty(meta->property(i)) && hasGenericRow(meta, meta->property(i))
                && isOurs(declaringClass(meta, i))
                && meta->indexOfProperty(meta->property(i).name()) == i)
                QTest::addRow("%s::%s", fixtures()[f].name, meta->property(i).name())
                    << f << QByteArray(meta->property(i).name());
    }
}

void TestMetaSurface::everyNotifyPropertyNotifiesOncePerChange()
{
    QFETCH(int, fixture);
    QFETCH(QByteArray, property);
    auto plot = newPlot();
    QObject* object = fixtures()[fixture].make(*plot);
    const QMetaProperty meta = metaProperty(object, property.constData());
    const Exception* exception = exceptionFor(object, property.constData());

    const QVariant current = meta.read(object);
    const QVariant other = exception ? exception->accepted(object)
                                     : differentValue(meta, current, object);
    QVERIFY2(other.isValid(), qPrintable(QStringLiteral("no different value for type %1")
                                             .arg(QLatin1String(meta.typeName()))));
    QVERIFY2(!same(other, current), "the different value is equal to the current one");

    QSignalSpy spy(object, meta.notifySignal());
    QVERIFY(spy.isValid());
    if (!exception || exception->rule != Rule::TakesOwnership)
    {
        QVERIFY(meta.write(object, meta.read(object)));
        QCOMPARE(spy.count(), 0);
    }

    {
        QCPEmissionScope scope(plot.get());
        QVERIFY(meta.write(object, other));
        QCOMPARE(spy.count(), fixtures()[fixture].deferred ? 0 : 1);
    }
    QCOMPARE(spy.count(), 1);
    QVERIFY(same(meta.read(object), other));
    if (signalCarriesValue(meta))
        QVERIFY(same(spy.first().first(), other));

    if (!exception || exception->rule != Rule::TakesOwnership)
    {
        QVERIFY(meta.write(object, other));
        QCOMPARE(spy.count(), 1);
    }
}

void TestMetaSurface::exceptionRulesHold_data()
{
    QTest::addColumn<int>("fixture");
    QTest::addColumn<int>("exception");
    for (int f = 0; f < fixtures().size(); ++f)
    {
        auto plot = newPlot();
        QObject* object = fixtures()[f].make(*plot);
        for (int e = 0; e < exceptions().size(); ++e)
        {
            const Exception& ex = exceptions()[e];
            if (!object->metaObject()->inherits(ex.meta))
                continue;
            if (isNotifyProperty(metaProperty(object, ex.property)))
                QTest::addRow("%s::%s", fixtures()[f].name, ex.property) << f << e;
        }
    }
}

void TestMetaSurface::exceptionRulesHold()
{
    QFETCH(int, fixture);
    QFETCH(int, exception);
    const Exception& ex = exceptions()[exception];
    auto plot = newPlot();
    QObject* object = fixtures()[fixture].make(*plot);
    const QMetaProperty meta = metaProperty(object, ex.property);
    QSignalSpy spy(object, meta.notifySignal());
    switch (ex.rule)
    {
        case Rule::TrimmedBySelectable:
        {
            auto* plottable = qobject_cast<QCPPlottable*>(object);
            plottable->setSelectable(QCP::stNone);
            QVERIFY(meta.write(object, QVariant::fromValue(QCPDataSelection(QCPDataRange(0, 1)))));
            QVERIFY(plottable->selection().isEmpty());
            QCOMPARE(spy.count(), 0);
            break;
        }
        case Rule::ComponentPen:
        {
            auto* line = qobject_cast<QCPLine*>(object);
            line->setComponentPens({ QPen(Qt::green) });
            settle(*plot);
            spy.clear();
            QVERIFY(meta.write(object, QVariant(line->pen())));
            QCOMPARE(spy.count(), 0);
            QVERIFY(plot->isRepaintQueued());
            break;
        }
        case Rule::Refuses:
        {
            const QVariant before = meta.read(object);
            {
                QCPEmissionScope scope(plot.get());
                meta.write(object, ex.probe(object));
            }
            QCOMPARE(meta.read(object), before);
            QCOMPARE(spy.count(), 0);
            QCOMPARE(int(test_misuse::calls.size()), ex.reportsMisuse ? 1 : 0);
            test_misuse::calls.clear();
            break;
        }
        case Rule::Clamps:
        {
            const QVariant probe = ex.probe(object);
            {
                QCPEmissionScope scope(plot.get());
                QVERIFY(meta.write(object, probe));
            }
            const QVariant clamped = meta.read(object);
            QVERIFY(clamped != probe);
            QCOMPARE(spy.count(), 1);
            QVERIFY(meta.write(object, probe));
            QCOMPARE(meta.read(object), clamped);
            QCOMPARE(spy.count(), 1);
            break;
        }
        case Rule::TakesOwnership:
        {
            const QVariant adopted = ex.accepted(object);
            QVERIFY(meta.write(object, adopted));
            QCOMPARE(spy.count(), 1);
            QVERIFY(meta.write(object, adopted)); // the same one again: refused, as it is ours
            QCOMPARE(meta.read(object), adopted);
            QCOMPARE(spy.count(), 1);
            break;
        }
    }
}

// ---------------------------------------------------------------------------------------------
// E-12..E-14: the enums, the flag and the invokables of the surface. A property whose type is an
// enumeration names a registered QMetaEnum; the public enums of section 6.1 are registered under
// their scoped names (E-13: dropping a Q_DECLARE_METATYPE must not rename them); each invokable
// is found by signature.
// ---------------------------------------------------------------------------------------------
namespace
{
// QCPCurve::LineStyle is an alias of the enum QCPLine registers (E-13 keeps its metatype line).
const QMetaObject* enumScope(const QMetaObject* meta, const QMetaProperty& property)
{
    return qstrcmp(property.typeName(), "QCPCurve::LineStyle") == 0 ? &QCPLine::staticMetaObject
                                                                     : meta;
}

struct EnumRow
{
    const QMetaObject* scope;
    const char* name;
    bool isFlag;
};

const QVector<EnumRow>& publicEnums()
{
    static const QVector<EnumRow> list {
        { &QCPIntervals::staticMetaObject, "HitPart", false },
        { &QCPIntervals::staticMetaObject, "EditMode", false },
        { &QCPIntervals::staticMetaObject, "EditModes", true },
        { &QCPIntervals::staticMetaObject, "SnapMode", false },
        { &QCPIntervals::staticMetaObject, "Style", false },
        { &QCPIntervals::staticMetaObject, "StackMode", false },
        { &QCPItemSpan::staticMetaObject, "HitPart", false },
        { &QCPItemRSpan::staticMetaObject, "HitPart", false },
        { &QCPPipelineScheduler::staticMetaObject, "Priority", false },
        { &QCPItemCreationState::staticMetaObject, "State", false },
        { &QCPAxis::staticMetaObject, "AxisType", false },
        { &QCPAxis::staticMetaObject, "AxisTypes", true },
        { &QCPLayer::staticMetaObject, "LayerMode", false },
        { &QCustomPlot::staticMetaObject, "LayerInsertMode", false },
        { &QCustomPlot::staticMetaObject, "RefreshPriority", false },
        { &QCPLayoutElement::staticMetaObject, "UpdatePhase", false },
        { &QCPLayoutGrid::staticMetaObject, "FillOrder", false },
        { &QCPLayoutInset::staticMetaObject, "InsetPlacement", false },
        { &QCPScatterStyle::staticMetaObject, "ScatterShape", false },
        { &QCPColorGradient::staticMetaObject, "NanHandling", false },
        { &QCPLine::staticMetaObject, "LineStyle", false },
        { &QCPBars::staticMetaObject, "WidthType", false },
        { &QCPFinancial::staticMetaObject, "ChartStyle", false },
    };
    return list;
}
} // namespace

void TestMetaSurface::everyEnumPropertyNamesARegisteredEnum_data()
{
    QTest::addColumn<int>("fixture");
    QTest::addColumn<QByteArray>("className");
    for (const Declared& d : declaredClasses())
        QTest::newRow(d.meta->className()) << d.fixture << QByteArray(d.meta->className());
}

void TestMetaSurface::everyEnumPropertyNamesARegisteredEnum()
{
    QFETCH(int, fixture);
    QFETCH(QByteArray, className);
    auto plot = newPlot();
    const QMetaObject* meta = fixtures()[fixture].make(*plot)->metaObject();
    for (int i = QObject::staticMetaObject.propertyCount(); i < meta->propertyCount(); ++i)
    {
        const QMetaProperty property = meta->property(i);
        if (declaringClass(meta, i)->className() != className
            || !property.metaType().flags().testFlag(QMetaType::IsEnumeration))
            continue;
        const QMetaObject* scope = enumScope(meta, property);
        const QByteArray bare = QByteArray(property.typeName()).split(':').last();
        const bool registered
            = property.isEnumType() || scope->indexOfEnumerator(bare.constData()) >= 0;
        QVERIFY2(registered,
                 qPrintable(QStringLiteral("%1::%2 is an enum type no Q_ENUM registers")
                                .arg(QLatin1String(className), QLatin1String(property.name()))));
    }
}

void TestMetaSurface::publicEnumsAreRegistered_data()
{
    QTest::addColumn<int>("row");
    for (int i = 0; i < publicEnums().size(); ++i)
        QTest::addRow("%s::%s", publicEnums()[i].scope->className(), publicEnums()[i].name) << i;
}

void TestMetaSurface::publicEnumsAreRegistered()
{
    QFETCH(int, row);
    const EnumRow& e = publicEnums()[row];
    const int index = e.scope->indexOfEnumerator(e.name);
    QVERIFY2(index >= 0, e.name);
    const QMetaEnum meta = e.scope->enumerator(index);
    QVERIFY(meta.isValid());
    QCOMPARE(meta.isFlag(), e.isFlag);
    QVERIFY(meta.keyCount() > 0);
    if (!e.isFlag)
    {
        QCOMPARE(QByteArray(meta.scope()), QByteArray(e.scope->className()));
        QCOMPARE(QByteArray(meta.enumName()), QByteArray(e.name));
    }
}

namespace
{
struct TypeRow
{
    const char* name;
    QMetaType type;
    const char* registeredAs; // the name moc gives it, when it differs from how it is written
};

#define TYPE_ROW(T) { #T, QMetaType::fromType<T>(), #T }

// E-13: the 31 enums whose Q_DECLARE_METATYPE line is gone, the 5 lines kept, and QCPScale, a
// Q_GADGET value rather than an enum, whose line is kept too. Each registers under the name it is
// written with: a deleted line that renamed its type, or an alias line removed, fails.
const QVector<TypeRow>& metatypeRows()
{
    static const QVector<TypeRow> list {
        TYPE_ROW(QCPAxisBase::SelectablePart),
        TYPE_ROW(QCPAxis::AxisType), TYPE_ROW(QCPAxis::LabelSide),
        TYPE_ROW(QCPAxisTicker::TickStepStrategy), TYPE_ROW(QCPAxisTickerFixed::ScaleStrategy),
        TYPE_ROW(QCPAxisTickerPi::FractionStyle), TYPE_ROW(QCPAxisTickerTime::TimeUnit),
        TYPE_ROW(QCPLabelPainterPrivate::AnchorMode), TYPE_ROW(QCPLabelPainterPrivate::AnchorSide),
        TYPE_ROW(QCPColorGradient::ColorInterpolation), TYPE_ROW(QCPColorGradient::NanHandling),
        TYPE_ROW(QCPColorGradient::GradientPreset), TYPE_ROW(QCustomPlot::LayerInsertMode),
        TYPE_ROW(QCustomPlot::RefreshPriority), TYPE_ROW(QCPItemBracket::BracketStyle),
        TYPE_ROW(QCPItemTracer::TracerStyle), TYPE_ROW(QCPItemPosition::PositionType),
        TYPE_ROW(QCPLayer::LayerMode), TYPE_ROW(QCPLayoutElement::UpdatePhase),
        TYPE_ROW(QCPLayoutGrid::FillOrder), TYPE_ROW(QCPLayoutInset::InsetPlacement),
        TYPE_ROW(QCPLegend::SelectablePart), TYPE_ROW(QCPLineEnding::EndingStyle),
        TYPE_ROW(QCPPainter::PainterMode), TYPE_ROW(QCPBarsGroup::SpacingType),
        TYPE_ROW(QCPBars::WidthType), TYPE_ROW(QCPFinancial::ChartStyle),
        TYPE_ROW(QCPPolarGrid::GridType), TYPE_ROW(QCPScatterStyle::ScatterProperty),
        TYPE_ROW(QCPScatterStyle::ScatterShape), TYPE_ROW(QCPSelectionDecoratorBracket::BracketStyle),
        { "QCPCurve::LineStyle", QMetaType::fromType<QCPCurve::LineStyle>(), "QCPLine::LineStyle" },
         TYPE_ROW(QCPDataSelection),
        TYPE_ROW(QSharedPointer<QCPAxisTicker>), TYPE_ROW(QCPSelectionDecorator*),
        TYPE_ROW(QCPIntervalEdit), TYPE_ROW(QCPScale),
    };
    return list;
}
} // namespace

void TestMetaSurface::metatypesKeepTheirNames_data()
{
    QTest::addColumn<int>("row");
    for (int i = 0; i < metatypeRows().size(); ++i)
        QTest::addRow("%s", metatypeRows()[i].name) << i;
}

void TestMetaSurface::metatypesKeepTheirNames()
{
    QFETCH(int, row);
    const TypeRow& r = metatypeRows()[row];
    QVERIFY(r.type.isValid());
    QCOMPARE(QByteArray(r.type.name()), QByteArray(r.registeredAs));
    QVERIFY(r.type.id() != 0);
    QCOMPARE(QMetaType::fromName(r.registeredAs), r.type);
}

// An invokable declared on a base is found through a derived class's meta-object, and runs the
// derived class' behaviour (the cartesian data range, through the virtual ownDataRange).
void TestMetaSurface::invokableOnABaseIsFoundThroughADerivedClass()
{
    auto plot = newPlot();
    graphOn(*plot);
    plot->xAxis->setRange(100, 200);
    QVERIFY(QCPAxis::staticMetaObject.indexOfMethod("rescale(bool)") >= 0);
    QVERIFY(QMetaObject::invokeMethod(plot->xAxis, "rescale", Q_ARG(bool, false)));
    QVERIFY(plot->xAxis->range().lower <= 0);
    QVERIFY(plot->xAxis->range().upper < 100);

    QCPLayoutGrid* grid = plot->plotLayout().get();
    QVERIFY(grid->metaObject()->indexOfMethod("takeAt(int)") >= 0);
    QCPLayoutElement* taken = nullptr;
    QVERIFY(QMetaObject::invokeMethod(grid, "takeAt", Q_RETURN_ARG(QCPLayoutElement*, taken),
                                      Q_ARG(int, 0)));
    QVERIFY(taken);
    QCOMPARE(taken->parent(), plot.get());
    QVERIFY(QMetaObject::invokeMethod(grid, "simplify"));
}

namespace
{
struct InvokableRow
{
    const QMetaObject* scope;
    const char* signature;
};

const QVector<InvokableRow>& invokables()
{
    static const QVector<InvokableRow> list {
        { &QCPAxisBase::staticMetaObject, "moveRange(double)" },
        { &QCPAxisBase::staticMetaObject, "scaleRange(double)" },
        { &QCPAxisBase::staticMetaObject, "rescale(bool)" },
        { &QCPAxisBase::staticMetaObject, "unlink()" },
        { &QCustomPlot::staticMetaObject, "requestRepaint()" },
        { &QCustomPlot::staticMetaObject, "settle()" },
        { &QCustomPlot::staticMetaObject, "requestDataSwap()" },
        { &QCustomPlot::staticMetaObject, "removePlottable(int)" },
        { &QCustomPlot::staticMetaObject, "clearPlottables()" },
        { &QCustomPlot::staticMetaObject, "removePlottableGroup(QCPPlottableGroup*)" },
        { &QCustomPlot::staticMetaObject, "removeGraph(int)" },
        { &QCustomPlot::staticMetaObject, "clearGraphs()" },
        { &QCustomPlot::staticMetaObject, "removeItem(int)" },
        { &QCustomPlot::staticMetaObject, "clearItems()" },
        { &QCustomPlot::staticMetaObject, "addLayer(QString,QCPLayer*,QCustomPlot::LayerInsertMode)" },
        { &QCustomPlot::staticMetaObject, "removeLayer(QCPLayer*)" },
        { &QCustomPlot::staticMetaObject,
          "moveLayer(QCPLayer*,QCPLayer*,QCustomPlot::LayerInsertMode)" },
        { &QCustomPlot::staticMetaObject,
          "savePdf(QString,int,int,QCP::ExportPen,QString,QString)" },
        { &QCustomPlot::staticMetaObject,
          "savePng(QString,int,int,double,int,int,QCP::ResolutionUnit)" },
        { &QCustomPlot::staticMetaObject,
          "saveJpg(QString,int,int,double,int,int,QCP::ResolutionUnit)" },
        { &QCustomPlot::staticMetaObject, "saveBmp(QString,int,int,double,int,QCP::ResolutionUnit)" },
        { &QCPLayout::staticMetaObject, "takeAt(int)" },
        { &QCPLayout::staticMetaObject, "take(QCPLayoutElement*)" },
        { &QCPLayout::staticMetaObject, "simplify()" },
        { &QCPLayout::staticMetaObject, "removeAt(int)" },
        { &QCPLayout::staticMetaObject, "remove(QCPLayoutElement*)" },
        { &QCPLayout::staticMetaObject, "clear()" },
        { &QCPLayoutGrid::staticMetaObject, "addElement(int,int,QCPLayoutElement*)" },
        { &QCPLayoutGrid::staticMetaObject, "expandTo(int,int)" },
        { &QCPLayoutGrid::staticMetaObject, "insertRow(int)" },
        { &QCPLayoutGrid::staticMetaObject, "insertColumn(int)" },
        { &QCPLayoutInset::staticMetaObject, "addElement(QCPLayoutElement*,Qt::Alignment)" },
        { &QCPLegend::staticMetaObject, "addItem(QCPLegendItem*)" },
        { &QCPLegend::staticMetaObject, "removeItem(int)" },
        { &QCPLegend::staticMetaObject, "clearItems()" },
    };
    return list;
}
} // namespace

void TestMetaSurface::invokablesAreFound_data()
{
    QTest::addColumn<int>("row");
    for (int i = 0; i < invokables().size(); ++i)
        QTest::addRow("%s::%s", invokables()[i].scope->className(), invokables()[i].signature) << i;
}

// E-14: found by signature, as a method of its own class (not a slot, not a signal).
void TestMetaSurface::invokablesAreFound()
{
    QFETCH(int, row);
    const InvokableRow& r = invokables()[row];
    const int index = r.scope->indexOfMethod(r.signature);
    QVERIFY2(index >= 0, r.signature);
    const QMetaMethod method = r.scope->method(index);
    QCOMPARE(method.methodType(), QMetaMethod::Method);
    QCOMPARE(method.access(), QMetaMethod::Public);
}
