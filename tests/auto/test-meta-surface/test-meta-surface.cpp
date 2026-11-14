#include "test-meta-surface.h"
#include "../misuse-recorder.h"
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

QCPCurve* curveOn(QCustomPlot& plot)
{
    auto* curve = new QCPCurve(plot.xAxis, plot.yAxis);
    curve->setData(QVector<double> { 0, 5, 10 }, QVector<double> { 1, 6, 2 });
    return curve;
}

QCPBars* barsOn(QCustomPlot& plot)
{
    auto* bars = new QCPBars(plot.xAxis, plot.yAxis);
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
    auto* plottable = qobject_cast<QCPAbstractPlottable*>(object);
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
        { "axis scale type", "scaleType",
          [](QCustomPlot& plot) { return std::pair<QObject*, QObject*>(plot.xAxis, plot.yAxis); },
          removeAxis, QVariant::fromValue(QCPAxisBase::stLogarithmic),
          bindableOf<QCPAxisBase::ScaleType>, bindGuarded<QCPAxisBase::ScaleType> },
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

void connectTrigger(QObject* sender, const char* property, MetaTrigger* trigger)
{
    const QMetaMethod notify = metaProperty(sender, property).notifySignal();
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
    QBindable<QCPAxisBase::ScaleType> scaleType(angular, "scaleType");
    QProperty<QCPAxisBase::ScaleType> wanted(QCPAxisBase::stLinear);
    scaleType.setBinding([&wanted] { return wanted.value(); });
    QSignalSpy spy(angular, &QCPAxisBase::scaleTypeChanged);

    wanted = QCPAxisBase::stLogarithmic;

    QCOMPARE(test_misuse::calls.size(), std::size_t(1));
    test_misuse::calls.clear(); // the refusal is the point of this test
    QCOMPARE(angular->scaleType(), QCPAxisBase::stLinear);
    QCOMPARE(scaleType.value(), QCPAxisBase::stLinear);
    QCOMPARE(spy.count(), 0);
    QVERIFY(scaleType.hasBinding());

    wanted = QCPAxisBase::stLinear;
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
    QSignalSpy spy(curve, &QCPAbstractPlottable::penChanged);
    curve->setPen(QPen(Qt::green, 2));
    QCOMPARE(spy.count(), 1);
    curve->setPen(QPen(Qt::green, 2));
    QCOMPARE(spy.count(), 1);
}

// QCPAbstractLine::setPen repaints when only a component's pen differs (plottable-line.cpp), but
// pen() does not move: no penChanged.
void TestMetaSurface::componentRepenRepaintsWithoutPenChanged()
{
    auto plot = newPlot();
    QCPGraph* graph = graphOn(*plot);
    graph->setPen(QPen(Qt::red));
    graph->setComponentPens({ QPen(Qt::green) });
    settle(*plot);
    QSignalSpy spy(graph, &QCPAbstractPlottable::penChanged);

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
              return new QCPPolarGraph(area->angularAxis(), area->radialAxis());
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
          [](QCustomPlot& plot) -> QObject* { return new QCPColorMap(plot.xAxis, plot.yAxis); } },
        { "QCPErrorBars",
          [](QCustomPlot& plot) -> QObject*
          {
              auto* errors = new QCPErrorBars(plot.xAxis, plot.yAxis);
              errors->setDataPlottable(graphOn(plot));
              return errors;
          } },
        { "QCPFinancial",
          [](QCustomPlot& plot) -> QObject* { return new QCPFinancial(plot.xAxis, plot.yAxis); } },
        { "QCPStatisticalBox", [](QCustomPlot& plot) -> QObject*
          { return new QCPStatisticalBox(plot.xAxis, plot.yAxis); } },
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
        differentFor<QCPAbstractPlottable*>(
            [](QCPAbstractPlottable* const&, QObject* object) -> QCPAbstractPlottable*
            { return graphOn(*plotOf(object)); }),
        differentFor<QCPGraph*>([](QCPGraph* const&, QObject* object) -> QCPGraph*
                                { return graphOn(*plotOf(object)); }),
        differentFor<QCPAxisRect*>([](QCPAxisRect* const&, QObject* object) -> QCPAxisRect*
                                   { return axisRectOn(*plotOf(object)); }),
        differentFor<QCPBarsGroup*>([](QCPBarsGroup* const&, QObject* object) -> QCPBarsGroup*
                                    { return new QCPBarsGroup(plotOf(object)); }),
        differentFor<QCPColorScale*>([](QCPColorScale* const&, QObject* object) -> QCPColorScale*
                                     { return colorScaleOn(*plotOf(object)); }),
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
        { &QCPAbstractPlottable::staticMetaObject, "selection", Rule::TrimmedBySelectable,
          [](QObject* object)
          {
              auto* plottable = qobject_cast<QCPAbstractPlottable*>(object);
              plottable->setSelectable(QCP::stDataRange);
              return QVariant::fromValue(QCPDataSelection(QCPDataRange(0, 1)));
          },
          nullptr, "enforceType trims it by selectable (plottable.cpp)" },
        { &QCPAbstractPlottable::staticMetaObject, "selectionDecorator", Rule::TakesOwnership,
          [](QObject*) { return QVariant::fromValue(freshDecorator()); }, nullptr,
          "adopts the decorator and refuses the same one again (plottable.cpp:966-976)" },
        { &QCPAbstractLine::staticMetaObject, "pen", Rule::ComponentPen,
          [](QObject*) { return QVariant(QPen(Qt::blue, 3)); }, nullptr,
          "repaints when only a component differs (plottable-line.cpp:84)" },
        { &QCPPolarAxisAngular::staticMetaObject, "scaleType", Rule::Refuses, nullptr,
          [](QObject*) { return QVariant::fromValue(QCPAxisBase::stLogarithmic); },
          "the ring has no logarithmic scale (angularaxis.cpp:938)", true },
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
        { "QCPAbstractLine::setComponentScatterStyle on component 0",
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
              auto* errors = new QCPErrorBars(plot.xAxis, plot.yAxis);
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
        { "QCPAbstractItem::setClipArea",
          [](QCustomPlot& plot) -> QObject* { return plot.add<QCPItemLine>().get(); },
          [](QObject* o)
          {
              auto* item = qobject_cast<QCPAbstractItem*>(o);
              QCustomPlot* plot = item->parentPlot();
              if (item->clipAxisRect().get() == plot->axisRect().get())
                  item->setClipArea(axisRectOn(*plot));
          },
          { "clipAxisRect" },
          { "clipToAxisRect" } },
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
                  legend->item(0).get(), &QCPAbstractLegendItem::fontChanged,
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

void TestMetaSurface::exceptionsNameRealProperties()
{
    for (const Exception& e : exceptions())
        QVERIFY2(e.meta->indexOfProperty(e.property) >= 0,
                 qPrintable(QStringLiteral("%1::%2 is no property")
                                .arg(QLatin1String(e.meta->className()),
                                     QLatin1String(e.property))));
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
            auto* plottable = qobject_cast<QCPAbstractPlottable*>(object);
            plottable->setSelectable(QCP::stNone);
            QVERIFY(meta.write(object, QVariant::fromValue(QCPDataSelection(QCPDataRange(0, 1)))));
            QVERIFY(plottable->selection().isEmpty());
            QCOMPARE(spy.count(), 0);
            break;
        }
        case Rule::ComponentPen:
        {
            auto* line = qobject_cast<QCPAbstractLine*>(object);
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
