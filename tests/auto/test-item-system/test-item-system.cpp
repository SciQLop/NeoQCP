#include "test-item-system.h"
#include "../../../src/polar/angularaxis.h"
#include "../../../src/polar/polar-area.h"
#include "../../../src/polar/radialaxis.h"
#include "../../../src/qcp.h"
#include "../expect-debug.h"
#include <QApplication>
#include <QImage>
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

//! A polar area alone on the plot: the reference scenes and the pixel assertions have no axis rect.
QCPPolarArea* polarOnly(QCustomPlot* plot)
{
    plot->plotLayout()->clear();
    auto* area = new QCPPolarArea(plot);
    plot->plotLayout()->addElement(0, 0, area);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    plot->replot();
    return area;
}

//! A polar area under the plot's axis rect: the take/adopt test needs the default rect to return to.
QCPPolarArea* secondPolar(QCustomPlot* plot)
{
    auto* area = new QCPPolarArea(plot);
    plot->plotLayout()->addElement(1, 0, area);
    area->angularAxis()->setRange(0, 360);
    area->radialAxis()->setRange(0, 1);
    plot->replot();
    return area;
}

void bindToPolar(QCPAbstractItem* item, QCPPolarArea* area)
{
    QCPCoordinateSystem* system = area->system(area->angularAxis(), area->radialAxis());
    for (QCPItemPosition* position : item->positions())
    {
        position->setSystem(system);
        position->setType(QCPItemPosition::ptPlotCoords);
    }
    item->setClipArea(area);
    item->setClipToAxisRect(true);
}

//! How many red pixels sit within \a half of \a at: a text small enough to fit its box.
int redInkNear(QCustomPlot* plot, QPointF at, int half = 12)
{
    const QImage image = plot->toPixmap().toImage().convertToFormat(QImage::Format_ARGB32);
    int ink = 0;
    for (int dy = -half; dy <= half; ++dy)
        for (int dx = -half; dx <= half; ++dx)
        {
            const QPoint p(qRound(at.x()) + dx, qRound(at.y()) + dy);
            if (!image.rect().contains(p))
                continue;
            const QColor c = image.pixelColor(p);
            if (c.red() > 150 && c.green() < 100 && c.blue() < 100)
                ++ink;
        }
    return ink;
}

//! A text that says how many times it recorded.
class CountingText : public QCPItemText
{
public:
    using QCPItemText::QCPItemText;
    int recordings() const { return mCore.recordingCount(); }
};
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
    mPlot->layer(QStringLiteral("main"))->markDirty(); // the item's layer tries to draw once more
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
    auto* graph = new QCPGraph(mPlot->xAxis, mPlot->yAxis);
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

void TestItemSystem::aTextFollowsAPolarDataPoint()
{
    QCPPolarArea* area = polarOnly(mPlot);
    auto* text = new CountingText(mPlot);
    text->setText(QStringLiteral("polar"));
    text->setColor(Qt::red);
    text->position->setCoords(90.0, 0.6);
    bindToPolar(text, area);
    QVERIFY(mPlot->addLayer(QStringLiteral("polar-item")));
    text->setLayer(mPlot->layer(QStringLiteral("polar-item")));
    mPlot->replot();
    // toPixmap() replaces the viewport and so bumps the view epoch, which remakes a theme-reading
    // recording: paint once before the count, then settle the recording for that epoch.
    QVERIFY2(redInkNear(mPlot, text->position->pixelPosition()) > 0,
             "the text is not where it says it is");
    mPlot->replot();
    const int recorded = text->recordings();
    const QPointF before = text->position->pixelPosition();
    QVERIFY2(recorded > 0, "the item never recorded");

    // A rotation moves the frame, not the recording: the same commands replay at new pixels.
    area->angularAxis()->setAngle(-45);
    mPlot->replot();
    const QPointF afterAngle = text->position->pixelPosition();
    QVERIFY(!samePixel(afterAngle, before));
    QCOMPARE(text->recordings(), recorded);
    QVERIFY2(redInkNear(mPlot, afterAngle) > 0, "the rotated text was not painted");
    QCOMPARE(redInkNear(mPlot, before), 0); // the old place is empty again

    // A radial zoom moves it along the radius, no re-record on a frame change either.
    area->radialAxis()->setRange(0, 2);
    mPlot->replot();
    QVERIFY(!samePixel(text->position->pixelPosition(), afterAngle));
}

void TestItemSystem::aPolarRatioIsAngleAndRadiusFractions()
{
    QCPPolarArea* area = polarOnly(mPlot);
    auto* item = new QCPItemLine(mPlot);
    QCPItemPosition* position = item->positions().first();
    position->setSystem(area->system(area->angularAxis(), area->radialAxis()));
    position->setType(QCPItemPosition::ptAxisRectRatio);
    // x is a fraction of the angular span, y a fraction of the radius. The default offset is -90
    // degrees, so a quarter of 0..360 lands on the 90 degree data angle, i.e. screen +x.
    position->setCoords(0.25, 0.5);
    const QPointF expected(area->center().x() + 0.5 * area->radius(), area->center().y());
    QVERIFY2(samePixel(position->pixelPosition(), expected),
             qPrintable(QStringLiteral("ratio pixel %1,%2, expected %3,%4")
                            .arg(position->pixelPosition().x())
                            .arg(position->pixelPosition().y())
                            .arg(expected.x())
                            .arg(expected.y())));
}

void TestItemSystem::aPolarItemClipsToTheDisc()
{
    QCPPolarArea* area = polarOnly(mPlot);
    auto* item = new QCPItemEllipse(mPlot);
    item->setBrush(QBrush(Qt::red));
    item->setPen(Qt::NoPen);
    for (QCPItemPosition* position : item->positions())
        position->setSystem(area->system(area->angularAxis(), area->radialAxis()));
    // Pixels cover the whole area rect: the corners are outside the disc, the centre inside it.
    const QRect rect = area->rect();
    item->topLeft->setType(QCPItemPosition::ptAbsolute);
    item->topLeft->setCoords(rect.topLeft());
    item->bottomRight->setType(QCPItemPosition::ptAbsolute);
    item->bottomRight->setCoords(rect.bottomRight());
    item->setClipArea(area);
    item->setClipToAxisRect(true);
    mPlot->replot();

    const QImage image = mPlot->toPixmap().toImage().convertToFormat(QImage::Format_ARGB32);
    const QPoint centre(qRound(area->center().x()), qRound(area->center().y()));
    QCOMPARE(image.pixelColor(centre), QColor(Qt::red));
    const QPoint corner = area->rect().topLeft() + QPoint(4, 4);
    QVERIFY2(image.pixelColor(corner) != QColor(Qt::red),
             "the ellipse inked a corner outside the disc");
}

void TestItemSystem::anUnclippedPolarItemIsNotClipped()
{
    QCPPolarArea* area = polarOnly(mPlot);
    auto* item = new QCPItemEllipse(mPlot);
    item->setBrush(QBrush(Qt::red));
    item->setPen(Qt::NoPen);
    for (QCPItemPosition* position : item->positions())
        position->setSystem(area->system(area->angularAxis(), area->radialAxis()));
    // The ellipse runs from the top of the disc to a point at 1.2 radii on the +x ray: the
    // rightmost pixel is ink outside the disc.
    item->topLeft->setType(QCPItemPosition::ptAxisRectRatio);
    item->topLeft->setCoords(0.0, 1.0);
    item->bottomRight->setType(QCPItemPosition::ptAxisRectRatio);
    item->bottomRight->setCoords(0.25, 1.2);
    // The clip area is set, but the clip flag is off: the item is not clipped, as on an axis rect.
    item->setClipArea(area);
    item->setClipToAxisRect(false);
    mPlot->replot();

    const QPointF outside(area->center().x() + 1.1 * area->radius(),
                          area->center().y() - 0.4 * area->radius());
    QVERIFY2(QLineF(area->center(), outside).length() > area->radius(),
             "the probe is not outside the disc");
    const QImage image = mPlot->toPixmap().toImage().convertToFormat(QImage::Format_ARGB32);
    QCOMPARE(image.pixelColor(qRound(outside.x()), qRound(outside.y())), QColor(Qt::red));
    QVERIFY2(item->selectTest(outside, false) >= 0, "the ink outside the disc does not hit");
}

void TestItemSystem::aPolarItemIsHittable()
{
    QCPPolarArea* area = polarOnly(mPlot);
    auto* item = new QCPItemRect(mPlot);
    item->setBrush(QBrush(Qt::red));
    item->setPen(Qt::NoPen);
    for (QCPItemPosition* position : item->positions())
    {
        position->setSystem(area->system(area->angularAxis(), area->radialAxis()));
        position->setType(QCPItemPosition::ptAxisRectRatio);
    }
    item->topLeft->setCoords(0, 0);
    item->bottomRight->setCoords(1, 1);
    item->setClipArea(area);
    item->setClipToAxisRect(true);
    mPlot->replot();

    QVERIFY2(item->selectTest(area->center(), false) >= 0, "a click in the disc misses the item");
    const QPointF corner(area->rect().topLeft() + QPoint(4, 4));
    QVERIFY2(item->selectTest(corner, false) < 0, "a click outside the disc still hits the item");
}

void TestItemSystem::deletingThePolarAreaLeavesItsItemsInert()
{
    QCPPolarArea* area = secondPolar(mPlot);
    auto* text = new CountingText(mPlot);
    text->setText(QStringLiteral("polar"));
    text->setColor(Qt::red);
    text->position->setCoords(90.0, 0.6);
    bindToPolar(text, area);
    mPlot->replot();
    QVERIFY(text->position->system());
    QCOMPARE(text->position->area(), static_cast<QCPArea*>(area));
    QVERIFY2(redInkNear(mPlot, text->position->pixelPosition()) > 0, "the item was never painted");

    QCPLayoutElement* taken = area;
    QVERIFY(mPlot->plotLayout()->takeAt(1) == taken);
    delete taken;
    QVERIFY(!text->position->system());
    QVERIFY(!text->position->area());
    QCOMPARE(text->position->pixelPosition(), QPointF(0, 0));
    text->layer()->markDirty(); // the item's layer tries to draw once more
    expectMissingAxis();
    mPlot->replot();
}

void TestItemSystem::takeAndAdoptAPolarItem()
{
    QCPPolarArea* area = secondPolar(mPlot);
    auto text = mPlot->add<QCPItemText>();
    auto* item = text.get();
    QCPItemPosition* position = item->positions().first();
    position->setSystem(area->system(area->angularAxis(), area->radialAxis()));
    position->setType(QCPItemPosition::ptPlotCoords);
    position->setCoords(90.0, 0.6);
    item->setClipArea(area);
    item->setClipToAxisRect(true);
    mPlot->replot();
    QVERIFY(position->system());
    QCOMPARE(position->area(), static_cast<QCPArea*>(area));

    std::unique_ptr<QCPItemText> taken = mPlot->take(text);
    QVERIFY(taken);
    QVERIFY(!position->system());
    QVERIFY(!position->area());
    QVERIFY(!item->clipArea());

    auto readded = mPlot->add(std::move(taken));
    QVERIFY(readded);
    QVERIFY(position->system());
    QCOMPARE(position->system(),
             mPlot->axisRect()->system(mPlot->xAxis, mPlot->yAxis));
    QCOMPARE(position->area(), static_cast<QCPArea*>(mPlot->axisRect()));
    QCOMPARE(item->clipArea(), static_cast<QCPArea*>(mPlot->axisRect()));
}

void TestItemSystem::aPolarSetTypeKeepsThePixel()
{
    auto* area = polarOnly(mPlot);
    mPlot->replot();
    auto* item = new QCPItemEllipse(mPlot);
    QCPItemPosition* position = item->positions().first();
    position->setSystem(area->system(area->angularAxis(), area->radialAxis()));
    position->setType(QCPItemPosition::ptPlotCoords);
    position->setCoords(90.0, 0.6);
    mPlot->replot();
    const QPointF pixel = position->pixelPosition();
    QVERIFY2(!pixel.isNull(), "the polar position has no pixel to keep");

    for (QCPItemPosition::PositionType type :
         { QCPItemPosition::ptAbsolute, QCPItemPosition::ptAxisRectRatio,
           QCPItemPosition::ptPlotCoords })
    {
        position->setType(type);
        QVERIFY2(samePixel(position->pixelPosition(), pixel),
                 qPrintable(QStringLiteral("type %1 moved the pixel").arg(int(type))));
    }
}

void TestItemSystem::aPolarAreaAbsolutePositionIsFromTheAreaCorner()
{
    auto* area = polarOnly(mPlot);
    mPlot->replot();
    auto* item = new QCPItemEllipse(mPlot);
    QCPItemPosition* position = item->positions().first();
    position->setSystem(area->system(area->angularAxis(), area->radialAxis()));
    position->setType(QCPItemPosition::ptAxisRectAbsolute);
    position->setCoords(10.0, 10.0);
    const QPointF corner = area->rect().topLeft();
    QVERIFY2(samePixel(position->pixelPosition(), corner + QPointF(10, 10)),
             qPrintable(QStringLiteral("pixel %1,%2, expected %3,%4")
                            .arg(position->pixelPosition().x())
                            .arg(position->pixelPosition().y())
                            .arg(corner.x() + 10)
                            .arg(corner.y() + 10)));

    const QPointF asked = corner + QPointF(40, 30);
    position->setPixelPosition(asked);
    QVERIFY2(samePixel(position->pixelPosition(), asked), "the pixel did not round trip");
    QVERIFY2(samePixel(position->coords(), QPointF(40, 30)), "the coords did not round trip");
}

void TestItemSystem::aCartesianSystemWithAPolarAreaPlacesRatiosByPixel()
{
    auto* area = polarOnly(mPlot);
    mPlot->replot();
    auto* item = new QCPItemEllipse(mPlot);
    QCPItemPosition* position = item->positions().first();
    position->setArea(area);
    position->setType(QCPItemPosition::ptAxisRectRatio);
    position->setCoords(0.1, 0.1);
    const QCP::Pixels mapped = area->ratioTransform().map(QCP::AxisRatio { 0.6, 0.4 });
    const QPointF pixel(mapped.x, mapped.y);

    gMisuses = 0;
    const qcp::MisuseHandler previous = qcp::setMisuseHandler(countMisuse);
    position->setPixelPosition(pixel);
    qcp::setMisuseHandler(previous);

    QCOMPARE(gMisuses, 0);
    QVERIFY2(samePixel(position->pixelPosition(), pixel),
             qPrintable(QStringLiteral("pixel %1,%2, expected %3,%4")
                            .arg(position->pixelPosition().x())
                            .arg(position->pixelPosition().y())
                            .arg(pixel.x())
                            .arg(pixel.y())));
}

void TestItemSystem::aSetAxesAfterAPolarSystemTakesTheRectsArea()
{
    auto* area = polarOnly(mPlot);
    mPlot->replot();
    auto* item = new QCPItemEllipse(mPlot);
    QCPItemPosition* position = item->positions().first();
    position->setSystem(area->system(area->angularAxis(), area->radialAxis()));
    QCOMPARE(position->area(), static_cast<QCPArea*>(area));

    auto* rect = new QCPAxisRect(mPlot);
    mPlot->plotLayout()->addElement(1, 0, rect);
    mPlot->replot();
    QCPAxis* key = rect->axis(QCPAxis::atBottom);
    QCPAxis* value = rect->axis(QCPAxis::atLeft);
    QVERIFY(key && value);
    position->setAxes(key, value);
    QCOMPARE(position->system(), rect->system(key, value));
    QCOMPARE(position->area(), static_cast<QCPArea*>(rect));
    QCOMPARE(position->axisRect(), rect);
}

void TestItemSystem::aBindingAPositionToASystemMovesTheClipArea()
{
    // The cartesian rect stays: a new item starts clipped to it, which is what binding moves.
    auto* area = new QCPPolarArea(mPlot);
    mPlot->plotLayout()->addElement(1, 0, area);
    mPlot->replot();
    auto* item = new QCPItemEllipse(mPlot);
    QCOMPARE(item->clipArea(), static_cast<QCPArea*>(mPlot->axisRect()));

    item->positions().first()->setSystem(area->system(area->angularAxis(), area->radialAxis()));
    QCOMPARE(item->clipArea(), static_cast<QCPArea*>(area));

    auto* explicitArea = new QCPPolarArea(mPlot);
    mPlot->plotLayout()->addElement(2, 0, explicitArea);
    mPlot->replot();
    item->setClipArea(explicitArea);
    item->positions().first()->setSystem(area->system(area->angularAxis(), area->radialAxis()));
    QCOMPARE(item->clipArea(), static_cast<QCPArea*>(explicitArea));
}

void TestItemSystem::setAxesNullUnbindsAPolarSystem()
{
    auto* area = polarOnly(mPlot);
    auto* item = new QCPItemEllipse(mPlot);
    QCPItemPosition* position = item->positions().first();
    position->setSystem(area->system(area->angularAxis(), area->radialAxis()));
    QVERIFY(position->system());

    position->setAxes(nullptr, nullptr);
    QVERIFY2(!position->system(), "the polar system survived setAxes(nullptr, nullptr)");
}

void TestItemSystem::setSystemTwiceRetakesTheArea()
{
    auto* area = polarOnly(mPlot);
    auto* other = secondPolar(mPlot);
    mPlot->replot();
    QCPCoordinateSystem* system = area->system(area->angularAxis(), area->radialAxis());
    auto* item = new QCPItemEllipse(mPlot);
    QCPItemPosition* position = item->positions().first();
    position->setSystem(system);
    position->setArea(other);
    QCOMPARE(position->area(), static_cast<QCPArea*>(other));

    position->setSystem(system);
    QVERIFY2(position->area() == area, "a repeated setSystem did not retake the system's area");
}

void TestItemSystem::aTakeClearsAOneAxisPosition()
{
    auto item = mPlot->add<QCPItemEllipse>();
    QCPItemPosition* position = item->positions().first();
    position->setAxes(nullptr, mPlot->yAxis);
    QCOMPARE(position->valueAxis(), mPlot->yAxis);

    std::unique_ptr<QCPItemEllipse> taken = mPlot->take(item);
    QVERIFY(taken);
    QVERIFY2(!position->keyAxis() && !position->valueAxis(),
             "take left an axis on a one-axis position");
}

void TestItemSystem::theProjectionsFollowEveryFrame()
{
    // The getters answer from the position's cached 2.x projections: the cartesian system's pair,
    // the one axis of a one-axis position (axistag.cpp), null on a polar frame.
    QCPAxisRect* rect = mPlot->axisRect();
    auto* polar = secondPolar(mPlot);
    auto* item = new QCPItemTracer(mPlot);
    QCPItemPosition* position = item->position;

    position->setAxes(mPlot->xAxis, mPlot->yAxis);
    QCOMPARE(position->keyAxis(), mPlot->xAxis);
    QCOMPARE(position->valueAxis(), mPlot->yAxis);
    QCOMPARE(position->axisRect(), rect);
    QCOMPARE(position->system(), rect->system(mPlot->xAxis, mPlot->yAxis));

    position->setAxisRect(rect);
    position->setAxes(nullptr, mPlot->yAxis);
    QCOMPARE(position->keyAxis(), nullptr);
    QCOMPARE(position->valueAxis(), mPlot->yAxis);
    QCOMPARE(position->axisRect(), rect);
    QCOMPARE(position->system(), nullptr);

    position->setArea(polar);
    QCOMPARE(position->axisRect(), nullptr);
    QCOMPARE(position->area(), static_cast<QCPArea*>(polar));
    QCOMPARE(position->valueAxis(), mPlot->yAxis);

    position->setSystem(polar->system(polar->angularAxis(), polar->radialAxis()));
    QCOMPARE(position->keyAxis(), nullptr);
    QCOMPARE(position->valueAxis(), nullptr);
    QCOMPARE(position->axisRect(), nullptr);

    position->setAxes(mPlot->xAxis, mPlot->yAxis);
    QCOMPARE(position->keyAxis(), mPlot->xAxis);
    QCOMPARE(position->valueAxis(), mPlot->yAxis);
    QCOMPARE(position->axisRect(), rect);
}
