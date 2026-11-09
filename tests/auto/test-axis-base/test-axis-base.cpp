#include "test-axis-base.h"
#include "misuse-recorder.h"
#include "polar/angularaxis.h"
#include "polar/polar-area.h"
#include "polar/radialaxis.h"
#include <memory>

namespace
{
void settle(QCustomPlot& plot)
{
    plot.resize(400, 300);
    plot.replot();
    for (int i = 0; i < 10 && plot.isRepaintQueued(); ++i)
        QCoreApplication::processEvents();
}

void sendMouse(QCustomPlot* plot, QEvent::Type type, QPoint pos, Qt::MouseButtons buttons)
{
    QMouseEvent e(type, pos, plot->mapToGlobal(pos), Qt::LeftButton, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(plot, &e);
}

void dragFromTo(QCustomPlot* plot, QPoint from, QPoint to)
{
    sendMouse(plot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(plot, QEvent::MouseMove, to, Qt::LeftButton);
    sendMouse(plot, QEvent::MouseButtonRelease, to, Qt::NoButton);
}

//! The area alone in the layout, with its one radial axis spanning [0, 1].
QCPPolarArea* polarArea(QCustomPlot& plot)
{
    plot.plotLayout()->clear();
    auto* area = new QCPPolarArea(&plot);
    plot.plotLayout()->addElement(0, 0, area);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    settle(plot);
    return area;
}
} // namespace

void TestAxisBase::init()
{
    test_misuse::calls.clear();
    qcp::setMisuseHandler(test_misuse::record);
}

void TestAxisBase::cleanup()
{
    qcp::setMisuseHandler(nullptr);
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
}

void TestAxisBase::everyAxisSharesTheRangeModel_data()
{
    QTest::addColumn<QString>("kind");
    QTest::newRow("cartesian") << QStringLiteral("cartesian");
    QTest::newRow("angular") << QStringLiteral("angular");
    QTest::newRow("radial") << QStringLiteral("radial");
}

void TestAxisBase::everyAxisSharesTheRangeModel()
{
    QFETCH(QString, kind);

    QCustomPlot plot;
    QCPAxisBase* axis = nullptr;
    QCPPolarArea* area = nullptr;
    QCPPolarAxisRadial* radial = nullptr;
    if (kind == QLatin1String("cartesian"))
        axis = plot.xAxis;
    else
    {
        area = polarArea(plot);
        radial = area->radialAxis();
        axis = kind == QLatin1String("angular") ? static_cast<QCPAxisBase*>(area->angularAxis())
                                                : static_cast<QCPAxisBase*>(radial);
    }

    const QCPRange initial = axis->range();
    int changes = 0;
    QCPRange lastNew, lastOld;
    connect(axis, &QCPAxisBase::rangeChanged, &plot,
            [&changes, &lastNew, &lastOld](const QCPRange& newRange, const QCPRange& oldRange)
            {
                ++changes;
                lastNew = newRange;
                lastOld = oldRange;
            });

    axis->setRange(0, 10);
    QCOMPARE(axis->range(), QCPRange(0, 10));
    QCOMPARE(changes, 1);
    QCOMPARE(lastNew, QCPRange(0, 10));
    QCOMPARE(lastOld, initial);

    axis->moveRange(2);
    QCOMPARE(axis->range(), QCPRange(2, 12));
    QCOMPARE(changes, 2);

    axis->scaleRange(2);
    QCOMPARE(axis->range(), QCPRange(-3, 17));
    QCOMPARE(changes, 3);

    // The area moves an axis only while that axis allows it.
    QCPRange beforeDrag = axis->range();
    QPoint from, to;
    if (kind == QLatin1String("cartesian"))
    {
        from = QPoint(qRound(plot.xAxis->coordToPixel(5)), 150);
        to = from + QPoint(60, 0);
    }
    else
    {
        area->angularAxis()->setRange(0, 360);
        radial->setRange(0, 1);
        settle(plot);
        if (kind == QLatin1String("angular"))
        {
            from = radial->coordToPixel(20, 0.5).toPoint();
            to = radial->coordToPixel(120, 0.5).toPoint();
        }
        else
        {
            from = radial->coordToPixel(0, 0.3).toPoint();
            to = radial->coordToPixel(0, 0.6).toPoint();
        }
    }
    plot.setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    settle(plot);
    beforeDrag = axis->range();
    axis->setRangeDraggable(false);
    dragFromTo(&plot, from, to);
    QCOMPARE(axis->range(), beforeDrag);
    axis->setRangeDraggable(true);
    dragFromTo(&plot, from, to);
    QVERIFY2(axis->range() != beforeDrag, qPrintable(kind));
    axis->setRange(1, 2);

    axis->setRangeReversed(true);
    QVERIFY(axis->rangeReversed());

    const int before = changes;
    axis->setRange(4, 5);
    QCOMPARE(changes, before + 1);

    // Queued emissions report the latest range once, and a superseded one is dropped.
    const int queuedBefore = changes;
    {
        const QCPEmissionScope scope(&plot);
        axis->setRange(3, 4);
        axis->setRange(5, 6);
    }
    QCOMPARE(axis->range(), QCPRange(5, 6));
    QCOMPARE(changes, queuedBefore + 1);
    QCOMPARE(lastNew, QCPRange(5, 6));
    QCOMPARE(lastOld, QCPRange(3, 4));

    // A log scale sanitises a range that reaches through zero.
    axis->setScaleType(QCPAxisBase::stLogarithmic);
    QCOMPARE(axis->scaleType(), QCPAxisBase::stLogarithmic);
    axis->setRange(-1, 10);
    QVERIFY2(
        axis->range().lower > 0.0 && axis->range().upper > 0.0,
        qPrintable(QStringLiteral("%1..%2").arg(axis->range().lower).arg(axis->range().upper)));
    QVERIFY(QCPRange::validRange(axis->range()));
}

void TestAxisBase::linkingAPolarAxisIsRefused()
{
    QCustomPlot plot;
    auto* area = new QCPPolarArea(&plot);
    plot.plotLayout()->addElement(0, 1, area);

    std::size_t before = test_misuse::calls.size();
    area->angularAxis()->linkTo(plot.xAxis);
    QCOMPARE(test_misuse::calls.size(), before + 1);
    QVERIFY(!area->angularAxis()->link());
    QVERIFY(!plot.xAxis->link());

    // And a cartesian axis cannot be linked to a polar one.
    before = test_misuse::calls.size();
    plot.xAxis->linkTo(area->radialAxis());
    QCOMPARE(test_misuse::calls.size(), before + 1);
    QVERIFY(!plot.xAxis->link());
    QVERIFY(!area->radialAxis()->link());
    test_misuse::calls.clear(); // the two refusals above are the point of this test
}

void TestAxisBase::cartesianAxisApiStillCompiles()
{
    QCustomPlot plot;
    QCPAxis* axis = plot.xAxis;

    QCOMPARE(axis->scaleType(), QCPAxis::stLinear);
    axis->setScaleType(QCPAxis::stLogarithmic);
    QCOMPARE(axis->scaleType(), QCPAxis::stLogarithmic);

    const QCPAxis::SelectableParts parts = QCPAxis::spAxis | QCPAxis::spTickLabels;
    QVERIFY(parts.testFlag(QCPAxis::spTickLabels));
    QVERIFY(parts.testFlag(QCPAxis::spAxisLabel) == false);

    int fired = 0;
    connect(axis, &QCPAxis::scaleTypeChanged, &plot, [&fired](QCPAxis::ScaleType) { ++fired; });
    axis->setScaleType(QCPAxis::stLinear);
    QCOMPARE(fired, 1);

    // getPartAt is 2.x public API: hover cursors and context menus call it from outside.
    QCOMPARE(axis->getPartAt(QPointF(-100, -100)), QCPAxis::spNone);
}

void TestAxisBase::axisPropertiesStillNotify()
{
    QCustomPlot plot;
    QCPAxis* axis = plot.xAxis;
    const QMetaObject* meta = axis->metaObject();

    const QMetaProperty scaleType = meta->property(meta->indexOfProperty("scaleType"));
    QVERIFY(scaleType.isValid());
    QVERIFY(scaleType.hasNotifySignal());
    QVERIFY(scaleType.notifySignal() == QMetaMethod::fromSignal(&QCPAxis::scaleTypeChanged));

    const QMetaProperty selectedParts = meta->property(meta->indexOfProperty("selectedParts"));
    QVERIFY(selectedParts.isValid());
    QVERIFY(selectedParts.hasNotifySignal());
    QVERIFY(selectedParts.notifySignal() == QMetaMethod::fromSignal(&QCPAxis::selectionChanged));

    int scaleTypes = 0, selections = 0;
    connect(axis, &QCPAxis::scaleTypeChanged, &plot, [&scaleTypes] { ++scaleTypes; });
    connect(axis, &QCPAxis::selectionChanged, &plot, [&selections] { ++selections; });
    axis->setScaleType(QCPAxis::stLogarithmic);
    QCOMPARE(scaleTypes, 1);
    axis->setSelectedParts(QCPAxis::spAxis);
    QVERIFY(axis->selectedParts().testFlag(QCPAxis::spAxis));
    QCOMPARE(selections, 1);
}
