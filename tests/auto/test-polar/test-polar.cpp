#include "test-polar.h"
#include "misuse-recorder.h"
#include "polar/angularaxis.h"
#include "polar/polar-area.h"
#include "polar/polargraph.h"
#include "polar/radialaxis.h"
#include <QPointer>

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

// The area alone in the layout, so its rect is the whole plot minus its margins.
QCPPolarArea* polarArea(QCustomPlot& plot)
{
    plot.plotLayout()->clear();
    auto* area = new QCPPolarArea(&plot);
    plot.plotLayout()->addElement(0, 0, area);
    settle(plot);
    return area;
}

QColor pixelAt(QCustomPlot& plot, QPoint pos)
{
    return QColor(plot.toPixmap(400, 300).toImage().pixel(pos));
}
} // namespace

void TestPolar::init()
{
    test_misuse::calls.clear();
    qcp::setMisuseHandler(test_misuse::record);
}

void TestPolar::cleanup()
{
    qcp::setMisuseHandler(nullptr);
    QCOMPARE(test_misuse::calls.size(), std::size_t(0));
}

void TestPolar::areaOwnsItsAxes()
{
    QCustomPlot plot;
    plot.plotLayout()->clear();
    auto area = plot.add<QCPPolarArea>(0, 0);
    QVERIFY(area);
    QCOMPARE(plot.plotLayout()->element(0, 0), area.get());

    QVERIFY(area->angularAxis());
    QCOMPARE(area->angularAxis()->area(), area.get());
    QCOMPARE(area->radialAxisCount(), 1);
    QVERIFY(area->radialAxis(0));
    QCOMPARE(area->grid() != nullptr, true);
    QVERIFY(area->layer());
}

void TestPolar::areaLaysOutCentreAndRadius()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    plot.resize(400, 300);
    settle(plot);

    QCOMPARE(area->rect().topLeft(), QPoint(30, 30));
    QCOMPARE(area->rect().size(), QSize(340, 240));
    QCOMPARE(area->center(), QPointF(area->rect().center()));
    // Half the smaller side, as the angular axis laid itself out before the area took over.
    QCOMPARE(area->radius(), 120.0);
}

void TestPolar::deletingTheAreaMidDragIsSafe()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    plot.setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    area->angularAxis()->setRangeDrag(true);
    area->angularAxis()->setRange(0, 360);

    QPointer<QCPPolarArea> alive(area);
    connect(
        area->angularAxis(), &QCPPolarAxisAngular::rangeChanged, area,
        [&alive] { delete alive.data(); }, Qt::DirectConnection);

    // A quarter turn across the centre: a radial move would leave the angle unchanged.
    const QPoint from = area->center().toPoint() + QPoint(int(0.6 * area->radius()), 0);
    const QPoint to = area->center().toPoint() + QPoint(0, int(0.6 * area->radius()));
    sendMouse(&plot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(&plot, QEvent::MouseMove, to, Qt::LeftButton);
    sendMouse(&plot, QEvent::MouseButtonRelease, to, Qt::NoButton);

    QVERIFY(!alive);
}

void TestPolar::aPolarGraphAddsToTheArea()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    auto graph
        = area->add<QCPPolarGraph>(QVector<double> { 0, 90, 180 }, QVector<double> { 1, 2, 1 });
    QVERIFY(graph);
    QCOMPARE(area->graphs(), QList<QCPPolarGraph*> { graph.get() });
    QCOMPARE(graph->keyAxis(), area->angularAxis());

    QPointer<QCPPolarGraph> alive(graph.get());
    QVERIFY(area->removeGraph(graph.get()));
    QVERIFY(!alive);
    QCOMPARE(area->graphs().size(), 0);
    QVERIFY(!area->removeGraph(nullptr));
}

void TestPolar::insetAndBackgroundAreAreaFeatures()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    settle(plot);

    auto* legend = new QCPLegend;
    area->insetLayout()->addElement(legend, Qt::AlignRight | Qt::AlignTop);
    area->setBackground(QBrush(Qt::red));
    area->grid()->setVisible(false);
    settle(plot);

    // The inset layout aligns the legend's outer rect to the area's: QRect::right() is inclusive.
    QVERIFY(qAbs(legend->outerRect().right() - area->rect().right()) <= 1);
    QVERIFY(qAbs(legend->outerRect().top() - area->rect().top()) <= 1);
    // The area fills its whole rect, not just the circle the angular axis used to fill.
    const QColor corner = pixelAt(plot, area->rect().topLeft() + QPoint(5, 5));
    QVERIFY2(corner.red() > 200 && corner.green() < 60 && corner.blue() < 60,
             qPrintable(corner.name()));
    const QColor middle = pixelAt(plot, area->center().toPoint() + QPoint(40, 40));
    QVERIFY2(middle.red() > 200 && middle.green() < 60 && middle.blue() < 60,
             qPrintable(middle.name()));
}
