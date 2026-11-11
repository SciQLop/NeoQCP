#include "test-item-system.h"
#include "../../../src/qcp.h"
#include "../expect-debug.h"
#include <QApplication>
#include <QMouseEvent>

namespace
{
int gMisuses = 0;

void countMisuse(const char*, const char*)
{
    ++gMisuses;
}

void sendMouseClick(QWidget* w, const QPoint& pos)
{
    QMouseEvent press(QEvent::MouseButtonPress, pos, w->mapToGlobal(pos), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, pos, w->mapToGlobal(pos), Qt::LeftButton,
                        Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(w, &press);
    QApplication::sendEvent(w, &release);
}

void sendMouseMove(QWidget* w, const QPoint& pos)
{
    QMouseEvent move(QEvent::MouseMove, pos, w->mapToGlobal(pos), Qt::NoButton, Qt::NoButton,
                     Qt::NoModifier);
    QApplication::sendEvent(w, &move);
}

// A second area under the main one: it has its own axes, and so its own systems.
QCPAxisRect* addSecondRect(QCustomPlot* plot)
{
    auto* rect = new QCPAxisRect(plot);
    plot->plotLayout()->addElement(1, 0, rect);
    plot->replot();
    return rect;
}

bool samePixel(QPointF a, QPointF b)
{
    return qAbs(a.x() - b.x()) < 1e-6 && qAbs(a.y() - b.y()) < 1e-6;
}
} // namespace

void TestItemSystem::init()
{
    qRegisterMetaType<QCPAbstractItem*>();
    mPlot = new QCustomPlot(nullptr);
    mPlot->resize(640, 480);
    mPlot->show();
    mPlot->xAxis->setRange(0, 10);
    mPlot->yAxis->setRange(0, 10);
    mPlot->replot();
}

void TestItemSystem::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
}

void TestItemSystem::aPositionHasTheSystemOfItsAxes()
{
    auto* item = new QCPItemLine(mPlot);
    QCPItemPosition* position = item->positions().first();

    // The item constructor binds the plot's default pair: the position now names the same system
    // the rect makes for those two axes, and measures ratios in that rect.
    QCOMPARE(position->keyAxis(), mPlot->xAxis);
    QCOMPARE(position->valueAxis(), mPlot->yAxis);
    QVERIFY(position->system());
    QCOMPARE(position->system(), mPlot->axisRect()->system(mPlot->xAxis, mPlot->yAxis));
    QCOMPARE(position->area(), static_cast<QCPArea*>(mPlot->axisRect()));

    // Binding another pair of the same rect renames the system, not the area.
    QCPAxis* top = mPlot->axisRect()->axis(QCPAxis::atTop);
    position->setAxes(top, mPlot->yAxis);
    QCOMPARE(position->keyAxis(), top);
    QCOMPARE(position->valueAxis(), mPlot->yAxis);
    QCOMPARE(position->system(), mPlot->axisRect()->system(top, mPlot->yAxis));
    QCOMPARE(position->area(), static_cast<QCPArea*>(mPlot->axisRect()));
}

void TestItemSystem::aOneAxisPositionKeepsTwoXPixels()
{
    // A6: one axis given, no invented axis. The position has no system, and each slot keeps 2.x's
    // resolution: the plot slot through the axis it has, the other one through its own type.
    const QRect rect = mPlot->axisRect()->rect();

    {
        auto* item = new QCPItemLine(mPlot);
        QCPItemPosition* position = item->positions().first();
        position->setAxes(nullptr, mPlot->yAxis);
        QCOMPARE(position->keyAxis(), nullptr);
        QCOMPARE(position->valueAxis(), mPlot->yAxis);
        QCOMPARE(position->system(), nullptr);
        position->setTypeX(QCPItemPosition::ptAxisRectRatio);
        position->setTypeY(QCPItemPosition::ptPlotCoords);
        position->setCoords(0.25, 3.0);
        const QPointF expected(rect.left() + 0.25 * rect.width(), mPlot->yAxis->coordToPixel(3.0));
        QVERIFY2(samePixel(position->pixelPosition(), expected),
                 qPrintable(QStringLiteral("vertical axis: %1,%2, expected %3,%4")
                                .arg(position->pixelPosition().x())
                                .arg(position->pixelPosition().y())
                                .arg(expected.x())
                                .arg(expected.y())));
    }

    {
        auto* item = new QCPItemLine(mPlot);
        QCPItemPosition* position = item->positions().first();
        position->setAxes(mPlot->xAxis, nullptr);
        QCOMPARE(position->keyAxis(), mPlot->xAxis);
        QCOMPARE(position->valueAxis(), nullptr);
        QCOMPARE(position->system(), nullptr);
        position->setTypeX(QCPItemPosition::ptPlotCoords);
        position->setTypeY(QCPItemPosition::ptAxisRectRatio);
        position->setCoords(3.0, 0.25);
        const QPointF expected(mPlot->xAxis->coordToPixel(3.0), rect.top() + 0.25 * rect.height());
        QVERIFY2(samePixel(position->pixelPosition(), expected),
                 qPrintable(QStringLiteral("horizontal axis: %1,%2, expected %3,%4")
                                .arg(position->pixelPosition().x())
                                .arg(position->pixelPosition().y())
                                .arg(expected.x())
                                .arg(expected.y())));
    }
}

void TestItemSystem::aSystemMisuseKeepsTheOldSystem()
{
    QCPAxisRect* second = addSecondRect(mPlot);
    QCPAxis* foreign = second->axis(QCPAxis::atBottom);
    QVERIFY(foreign);

    auto* item = new QCPItemLine(mPlot);
    QCPItemPosition* position = item->positions().first();
    QCPCoordinateSystem* before = position->system();
    QVERIFY(before);

    gMisuses = 0;
    const qcp::MisuseHandler previous = qcp::setMisuseHandler(countMisuse);
    // Two axes of two areas cannot be one frame: refused, and the position stays where it was.
    position->setAxes(mPlot->xAxis, foreign);
    qcp::setMisuseHandler(previous);

    QCOMPARE(gMisuses, 1);
    QCOMPARE(position->system(), before);
    QCOMPARE(position->keyAxis(), mPlot->xAxis);
    QCOMPARE(position->valueAxis(), mPlot->yAxis);
}

void TestItemSystem::aDeletedAxisLeavesThePositionInert()
{
    auto* item = new QCPItemLine(mPlot);
    QCPItemPosition* position = item->positions().first();
    QVERIFY(position->system());

    // Deleting an axis deletes the systems that use it; the position loses its frame.
    QVERIFY(mPlot->axisRect()->removeAxis(mPlot->xAxis));
    QVERIFY(!position->system());
    QVERIFY(!position->keyAxis());
    QVERIFY(mPlot->axisRect()->removeAxis(mPlot->yAxis));
    QVERIFY(!position->valueAxis());

    // An inert position answers pixels quietly (0, 0), and the item says once that it is not drawn.
    QCOMPARE(position->pixelPosition(), QPointF(0, 0));
    expectMissingAxis();
    mPlot->replot();
}

void TestItemSystem::takeClearsAndAdoptRebindsItemSystems()
{
    auto line = mPlot->add<QCPItemLine>();
    QCPAbstractItem* item = line.get();
    QCPItemPosition* position = item->positions().first();
    QVERIFY(position->system());
    QCOMPARE(item->clipArea(), static_cast<QCPArea*>(mPlot->axisRect()));

    std::unique_ptr<QCPItemLine> taken = mPlot->take(line);
    QVERIFY(taken);
    QVERIFY(!position->system());
    QVERIFY(!position->area());
    QVERIFY(!item->clipArea());

    auto readded = mPlot->add(std::move(taken));
    QVERIFY(position->system());
    QCOMPARE(position->system(), mPlot->axisRect()->system(mPlot->xAxis, mPlot->yAxis));
    QCOMPARE(position->area(), static_cast<QCPArea*>(mPlot->axisRect()));
    QCOMPARE(item->clipArea(), static_cast<QCPArea*>(mPlot->axisRect()));
    QVERIFY(readded);
}

void TestItemSystem::theTracerFollowsItsGraphSystem()
{
    QCPGraph* graph = mPlot->addGraph();
    QVERIFY(graph);
    QVERIFY(graph->system());

    auto* tracer = new QCPItemTracer(mPlot);
    tracer->setGraph(graph);
    QCOMPARE(tracer->position->system(), graph->system());
    QCOMPARE(tracer->position->area(), static_cast<QCPArea*>(mPlot->axisRect()));
}

void TestItemSystem::itemClipAreaIsTheCartesianClip()
{
    QCPAxisRect* second = addSecondRect(mPlot);
    auto* item = new QCPItemLine(mPlot);
    QCOMPARE(item->clipArea(), static_cast<QCPArea*>(mPlot->axisRect()));
    QCOMPARE(item->clipAxisRect(), mPlot->axisRect());

    item->setClipArea(second);
    QCOMPARE(item->clipArea(), static_cast<QCPArea*>(second));
    QCOMPARE(item->clipAxisRect(), second);

    item->setClipAxisRect(nullptr);
    QVERIFY(!item->clipArea());
    QVERIFY(!item->clipAxisRect());
}

void TestItemSystem::aCreatedItemBindsTheNewRectsSystem()
{
    QCPAxisRect* second = addSecondRect(mPlot);
    QVERIFY(second->rect().isValid());

    mPlot->setItemCreator([](QCustomPlot* plot, QCPAxis*, QCPAxis*)
                          { return new QCPItemLine(plot); });
    mPlot->setCreationModeEnabled(true);

    QSignalSpy created(mPlot, &QCustomPlot::itemCreated);
    const QPoint p1 = second->rect().center() - QPoint(20, 20);
    const QPoint p2 = second->rect().center() + QPoint(20, 20);
    sendMouseClick(mPlot, p1);
    sendMouseMove(mPlot, p2);
    QApplication::processEvents();
    sendMouseClick(mPlot, p2);

    QCOMPARE(created.count(), 1);
    auto* item = created.at(0).at(0).value<QCPAbstractItem*>();
    QVERIFY(item);
    QCPItemPosition* position = item->positions().first();
    // The item constructor bound the main rect's default pair; creation rebinds the clicked one.
    QVERIFY(position->system());
    QCOMPARE(position->system()->area(), static_cast<QCPArea*>(second));
    QCOMPARE(position->area(), static_cast<QCPArea*>(second));
}
