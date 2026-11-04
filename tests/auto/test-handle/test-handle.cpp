#include "test-handle.h"
#include <functional>
#include <memory>
#include <type_traits>

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
} // namespace

void TestHandle::init()
{
    gMisuses = 0;
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

void TestHandle::handleConnects()
{
    QCustomPlot plot;
    QCPHandle<QCPGraph> h(plot.addGraph());
    int fired = 0;
    auto c = qcpConnect(h, qOverload<bool>(&QCPAbstractPlottable::selectionChanged), &plot,
                        [&fired](bool) { ++fired; });
    QVERIFY(c);
    h->setSelectable(QCP::stWhole);
    h->setSelection(QCPDataSelection(QCPDataRange(0, 1)));
    QVERIFY(fired > 0);

    QCPHandle<QCPGraph> dead;
    QVERIFY(!qcpConnect(dead, qOverload<bool>(&QCPAbstractPlottable::selectionChanged), &plot, [](bool) {}));

    QCPHandle<QCPGraph> deleted(plot.addGraph());
    delete deleted.get();
    QVERIFY(!qcpConnect(deleted, qOverload<bool>(&QCPAbstractPlottable::selectionChanged), &plot,
                        [](bool) {}));
}

void TestHandle::handleIsNotARawPointer()
{
    static_assert(!std::is_convertible_v<QCPHandle<QCPGraph>, QCPGraph*>);
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
