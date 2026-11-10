#include "test-polar.h"
#include "misuse-recorder.h"
#include "polar/angularaxis.h"
#include "polar/polar-area.h"
#include "polar/polargraph.h"
#include "polar/polargrid.h"
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

// The selectable part the axis reports under pos, or spNone where it reports nothing.
QCPAxisBase::SelectablePart partAt(QCPAxisBase* axis, QPointF pos)
{
    QVariant details;
    if (axis->selectTest(pos, false, &details) <= 0)
        return QCPAxisBase::spNone;
    return details.value<QCPAxisBase::SelectablePart>();
}

// The first point of a coarse scan of rect where the axis reports that part.
QPointF findPart(QCPAxisBase* axis, const QRect& rect, QCPAxisBase::SelectablePart wanted)
{
    for (int y = rect.top(); y <= rect.bottom(); y += 2)
        for (int x = rect.left(); x <= rect.right(); x += 2)
            if (partAt(axis, QPointF(x, y)) == wanted)
                return QPointF(x, y);
    return QPointF(-1, -1);
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
    area->angularAxis()->setRangeDraggable(true);
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

void TestPolar::backgroundStaysInsideTheDisc()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    area->setBackground(QBrush(Qt::red));
    area->grid()->setVisible(false);
    area->radialAxis()->setVisible(false); // nothing drawn over the centre
    settle(plot);

    // The area's background is the disc, not the rect: the corner stays the plot's paper.
    const QColor corner = pixelAt(plot, area->rect().topLeft() + QPoint(5, 5));
    const bool cornerIsRed = corner.red() > 200 && corner.green() < 60 && corner.blue() < 60;
    QVERIFY2(!cornerIsRed, qPrintable(corner.name()));

    const QColor centre = pixelAt(plot, area->center().toPoint());
    const bool centreIsRed = centre.red() > 200 && centre.green() < 60 && centre.blue() < 60;
    QVERIFY2(centreIsRed, qPrintable(centre.name()));
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
    // The background follows the disc, so the corner outside it keeps the paper.
    const QColor corner = pixelAt(plot, area->rect().topLeft() + QPoint(5, 5));
    QVERIFY2(!(corner.red() > 200 && corner.green() < 60 && corner.blue() < 60),
             qPrintable(corner.name()));
    const QColor middle = pixelAt(plot, area->center().toPoint() + QPoint(40, 40));
    QVERIFY2(middle.red() > 200 && middle.green() < 60 && middle.blue() < 60,
             qPrintable(middle.name()));
}

void TestPolar::theAreaKeepsTheOldMinimumMargins()
{
    // The angular axis asked for at least 30 px of margin; the area must not hand out less, or the
    // tick labels that used to fit get clipped.
    QCustomPlot plot;
    auto* area = polarArea(plot);
    QCOMPARE(area->minimumMargins(), QMargins(30, 30, 30, 30));
}

void TestPolar::theAngularAxisRefusesALogScale()
{
    // The ring's geometry is linear in the angle: a logarithmic scale would change the tick vector
    // and nothing else, so it is refused.
    QCustomPlot plot;
    auto* area = polarArea(plot);
    auto* angular = area->angularAxis();
    angular->setScaleType(QCPAxisBase::stLogarithmic);
    QCOMPARE(angular->scaleType(), QCPAxisBase::stLinear);
    QCOMPARE(int(test_misuse::calls.size()), 1);
    test_misuse::calls.clear();
}

void TestPolar::angularWheelZoomRequeuesTheRepaint()
{
    // With only the angular axis zoomable, the wheel changes what the ring and the graphs draw:
    // the area must ask for that repaint itself.
    QCustomPlot plot;
    auto* area = polarArea(plot);
    auto* angular = area->angularAxis();
    angular->setRangeZoomable(true);
    area->radialAxis()->setRangeZoomable(false);
    plot.setInteractions(QCP::iRangeZoom);
    angular->setRange(0, 360);
    settle(plot);
    QVERIFY(!plot.isRepaintQueued());

    const QPointF cursor = area->center() + QPointF(0.5 * area->radius(), 0);
    QWheelEvent wheel(cursor, plot.mapToGlobal(cursor.toPoint()), QPoint(), QPoint(0, 120),
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(&plot, &wheel);

    QVERIFY2(angular->range().size() < 360,
             qPrintable(QString::number(angular->range().size())));
    QVERIFY2(plot.isRepaintQueued(), "an angular-only wheel zoom scheduled no repaint");
}

void TestPolar::polarAxisPartsAreClickable()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    auto* angular = area->angularAxis();
    auto* radial = area->radialAxis();
    angular->setRange(0, 360);
    radial->setRange(0, 1);
    settle(plot);

    // The angular axis is the circle: the point straight right of the centre is on its backbone.
    const QPointF onCircle = area->center() + QPointF(area->radius(), 0);
    QCOMPARE(partAt(angular, onCircle), QCPAxisBase::spAxis);
    QCOMPARE(angular->selectTest(onCircle, false), plot.selectionTolerance() * 0.99);

    const QPointF angularLabel = findPart(angular, area->rect(), QCPAxisBase::spTickLabels);
    QVERIFY2(angularLabel.x() >= 0, "no angular tick label is hittable");

    // The radial axis is a spoke from the centre, at its angle measured against the angular axis.
    const double spokeRad = (radial->angle() + angular->angle()) / 180.0 * M_PI;
    const QPointF onSpoke = radial->coordToPixel(angular->angleRadToCoord(spokeRad), 0.5);
    QCOMPARE(partAt(radial, onSpoke), QCPAxisBase::spAxis);

    const QPointF radialLabel = findPart(radial, area->rect(), QCPAxisBase::spTickLabels);
    QVERIFY2(radialLabel.x() >= 0, "no radial tick label is hittable");

    // The area's corner is far outside the circle and every label box.
    const QPointF corner = area->rect().topLeft() + QPoint(2, 2);
    QCOMPARE(partAt(angular, corner), QCPAxisBase::spNone);
    QCOMPARE(partAt(radial, corner), QCPAxisBase::spNone);

    // Unselectable parts are refused when only selectable axes are wanted.
    angular->setSelectableParts(QCPPolarAxisAngular::spNone);
    QCOMPARE(angular->selectTest(onCircle, true, nullptr), -1.0);
    QVERIFY(angular->selectTest(onCircle, false, nullptr) > 0.0);
    radial->setSelectableParts(QCPPolarAxisRadial::spNone);
    QCOMPARE(radial->selectTest(onSpoke, true, nullptr), -1.0);
    QVERIFY(radial->selectTest(onSpoke, false, nullptr) > 0.0);
}

void TestPolar::radialDragAndWheelWork()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    auto* angular = area->angularAxis();
    auto* radial = area->radialAxis();
    plot.setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    angular->setRange(0, 360);
    radial->setRange(0, 1);
    settle(plot);

    // A wheel zooms the radial range about the radius under the cursor.
    const QPointF cursor = area->center() + QPointF(0.5 * area->radius(), 0);
    double angleBefore = 0, radiusBefore = 0, angleAfter = 0, radiusAfter = 0;
    angular->pixelToCoord(cursor, angleBefore, radiusBefore);
    QWheelEvent wheel(cursor, plot.mapToGlobal(cursor.toPoint()), QPoint(), QPoint(0, 120),
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(&plot, &wheel);
    angular->pixelToCoord(cursor, angleAfter, radiusAfter);
    QVERIFY2(qAbs(radial->range().size() - 0.85) < 1e-9,
             qPrintable(QString::number(radial->range().size())));
    QVERIFY2(qAbs(radiusAfter - radiusBefore) < 1e-9,
             qPrintable(QString::number(radiusAfter - radiusBefore)));
    QCOMPARE(angleAfter, angleBefore);

    // A drag along the spoke pans it by the radius the cursor moved.
    radial->setRange(0, 1);
    settle(plot);
    const QPointF from = radial->coordToPixel(0.0, 0.3);
    const QPointF to = radial->coordToPixel(0.0, 0.6);
    const QCPRange angularBefore = angular->range();
    sendMouse(&plot, QEvent::MouseButtonPress, from.toPoint(), Qt::LeftButton);
    sendMouse(&plot, QEvent::MouseMove, to.toPoint(), Qt::LeftButton);
    sendMouse(&plot, QEvent::MouseButtonRelease, to.toPoint(), Qt::NoButton);
    QVERIFY2(qAbs(radial->range().size() - 1.0) < 1e-9,
             qPrintable(QString::number(radial->range().size())));
    QVERIFY2(qAbs(radial->range().lower + 0.3) < 1e-9,
             qPrintable(QString::number(radial->range().lower)));
    QCOMPARE(angular->range(), angularBefore);
}

void TestPolar::hidingTheAngularAxisKeepsTheGraphs()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 180 }, QVector<double> { 0.5, 0.5 });
    QVERIFY(graph);
    graph->setPen(QPen(Qt::blue, 4));
    area->angularAxis()->setVisible(false);
    area->radialAxis()->setVisible(false);
    area->grid()->setVisible(false);
    settle(plot);

    // The graphs belong to the area, not to the ring: hiding the ring keeps the data.
    QCOMPARE(graph->parentLayerable(), static_cast<QCPLayerable*>(area));
    QVERIFY(graph->realVisibility());
    const QColor pixel = pixelAt(plot, (area->center() + QPointF(0, 0.5 * area->radius())).toPoint());
    QVERIFY2(pixel.blue() > 150 && pixel.red() < 120, qPrintable(pixel.name()));
}

void TestPolar::deletingAPolarChildIsSafe()
{
    // The area holds its children by raw pointer: a direct delete must unlist, or the next replot
    // and the area's own teardown reach freed memory.
    {
        QCustomPlot plot;
        auto* area = polarArea(plot);
        auto graph = area->add<QCPPolarGraph>(QVector<double> { 0, 180 }, QVector<double> { 1, 1 });
        QVERIFY(graph);
        delete area->angularAxis();
        QCOMPARE(area->angularAxis(), nullptr);
        settle(plot);
        QVERIFY(plot.plotLayout()->remove(area));
        settle(plot);
    }
    {
        QCustomPlot plot;
        auto* area = polarArea(plot);
        delete area->radialAxis(0);
        QCOMPARE(area->radialAxisCount(), 0);
        settle(plot);
        QVERIFY(plot.plotLayout()->remove(area));
        settle(plot);
    }
    {
        QCustomPlot plot;
        auto* area = polarArea(plot);
        delete area->grid();
        QCOMPARE(area->grid(), nullptr);
        settle(plot);
        QVERIFY(plot.plotLayout()->remove(area));
        settle(plot);
    }
}

void TestPolar::deletingTheAngularAxisMidDragIsSafe()
{
    QCustomPlot plot;
    auto* area = polarArea(plot);
    plot.setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    auto* angular = area->angularAxis();
    angular->setRangeDraggable(true);
    angular->setRangeZoomable(true);
    angular->setRange(0, 360);

    QPointer<QCPPolarAxisAngular> alive(angular);
    connect(
        angular, &QCPPolarAxisAngular::rangeChanged, area, [&alive] { delete alive.data(); },
        Qt::DirectConnection);

    const QPoint from = area->center().toPoint() + QPoint(int(0.6 * area->radius()), 0);
    const QPoint to = area->center().toPoint() + QPoint(0, int(0.6 * area->radius()));
    sendMouse(&plot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(&plot, QEvent::MouseMove, to, Qt::LeftButton);
    sendMouse(&plot, QEvent::MouseButtonRelease, to, Qt::NoButton);

    QVERIFY(!alive);
    QCOMPARE(area->angularAxis(), nullptr);
    settle(plot);
}
