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
