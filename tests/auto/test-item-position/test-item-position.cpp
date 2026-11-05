#include "test-item-position.h"
#include "qcustomplot.h"
#include <functional>

namespace
{
// The axis rect only has a meaningful geometry once the layout has run.
QCPItemText* laidOutTextItem(QCustomPlot* plot)
{
    plot->replot();
    return new QCPItemText(plot);
}
}

void TestItemPosition::init()
{
    mPlot = new QCustomPlot(nullptr);
    mPlot->resize(640, 480);
    mPlot->show();
}

void TestItemPosition::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
}

void TestItemPosition::axisRectAbsoluteIsOffsetByAxisRectOrigin()
{
    QCPItemText* item = laidOutTextItem(mPlot);
    const QPoint origin = mPlot->axisRect()->topLeft();
    // A non-trivial left/top margin is what makes this distinct from ptAbsolute.
    QVERIFY(origin.x() > 0);
    QVERIFY(origin.y() > 0);

    item->position->setType(QCPItemPosition::ptAxisRectAbsolute);
    item->position->setCoords(10, 20);

    QCOMPARE(item->position->pixelPosition(), QPointF(origin.x() + 10, origin.y() + 20));
}

void TestItemPosition::axisRectAbsoluteFollowsAxisRectOnResize()
{
    QCPItemText* item = laidOutTextItem(mPlot);
    item->position->setType(QCPItemPosition::ptAxisRectAbsolute);
    item->position->setCoords(10, 20);

    mPlot->resize(900, 700);
    mPlot->replot();
    const QPoint origin = mPlot->axisRect()->topLeft();

    QCOMPARE(item->position->pixelPosition(), QPointF(origin.x() + 10, origin.y() + 20));
    // The stored coords are the offset; only the resolved pixel position moves.
    QCOMPARE(item->position->coords(), QPointF(10, 20));
}

void TestItemPosition::axisRectAbsoluteSetPixelPositionRoundTrips()
{
    QCPItemText* item = laidOutTextItem(mPlot);
    item->position->setType(QCPItemPosition::ptAxisRectAbsolute);
    const QPoint origin = mPlot->axisRect()->topLeft();

    item->position->setPixelPosition(QPointF(origin.x() + 33, origin.y() + 44));

    QCOMPARE(item->position->coords(), QPointF(33, 44));
    QCOMPARE(item->position->pixelPosition(), QPointF(origin.x() + 33, origin.y() + 44));
}

void TestItemPosition::axisRectAbsoluteRespectsParentAnchor()
{
    QCPItemText* anchorItem = laidOutTextItem(mPlot);
    anchorItem->position->setType(QCPItemPosition::ptAbsolute);
    anchorItem->position->setCoords(100, 50);

    QCPItemText* item = new QCPItemText(mPlot);
    item->position->setType(QCPItemPosition::ptAxisRectAbsolute);
    QVERIFY(item->position->setParentAnchor(anchorItem->position));
    item->position->setCoords(10, 20);

    // A parent anchor takes precedence over the axis rect origin, mirroring
    // ptAxisRectRatio's behaviour.
    QCOMPARE(item->position->pixelPosition(), QPointF(110, 70));
}

void TestItemPosition::absoluteRemainsWidgetRelative()
{
    QCPItemText* item = laidOutTextItem(mPlot);
    item->position->setType(QCPItemPosition::ptAbsolute);
    item->position->setCoords(10, 20);

    // Guards against a fix that makes every static type axis-rect-relative.
    QCOMPARE(item->position->pixelPosition(), QPointF(10, 20));
}

void TestItemPosition::textRotatedHitTest()
{
    // The hit test is the rotated box, not the box the text was measured in.
    QCPItemText* item = laidOutTextItem(mPlot);
    item->setText(QStringLiteral("abcdefgh")); // wide, so a rotation moves the corners far
    item->position->setType(QCPItemPosition::ptAxisRectRatio);
    item->position->setCoords(0.5, 0.5);
    item->setPositionAlignment(Qt::AlignCenter);
    mPlot->replot();

    const QPointF centre = item->position->pixelPosition();
    const QPointF flatCorner = item->topRight->pixelPosition();

    item->setRotation(45);
    mPlot->replot();
    const QPointF rotatedCorner = item->topRight->pixelPosition();
    QVERIFY2(QLineF(flatCorner, rotatedCorner).length() > 10,
             "the rotation did not move the corner: the case proves nothing");

    const auto hits = [item](const QPointF& at)
    {
        const double distance = item->selectTest(at, false);
        return distance >= 0 && distance <= item->parentPlot()->selectionTolerance();
    };
    QVERIFY2(hits(centre + 0.9 * (rotatedCorner - centre)),
             "a point inside the rotated box is not hit");
    QVERIFY2(!hits(centre + 0.95 * (flatCorner - centre)),
             "a point outside the rotated box is hit");
}

void TestItemPosition::textAnchorParentDeletedKeepsPlace()
{
    // The child is anchored to the parent's corner: deleting the parent must not move the child,
    // least of all to pixel zero.
    auto* parent = laidOutTextItem(mPlot);
    parent->position->setType(QCPItemPosition::ptAbsolute);
    parent->position->setCoords(120, 90);
    parent->setText(QStringLiteral("parent"));

    auto* child = new QCPItemText(mPlot);
    child->position->setType(QCPItemPosition::ptAbsolute);
    child->position->setParentAnchor(parent->bottomRight);
    child->position->setCoords(5, 7); // an offset from the anchor, in pixels
    mPlot->replot();
    const QPointF before = child->position->pixelPosition();
    QVERIFY2(before.x() > 100 && before.y() > 100, "the child is not where the anchor is");

    mPlot->removeItem(parent); // deletes it
    mPlot->replot();
    const QPointF after = child->position->pixelPosition();
    QVERIFY2(qAbs(after.x() - before.x()) <= 1.0,
             qPrintable(QStringLiteral("x moved from %1 to %2").arg(before.x()).arg(after.x())));
    QVERIFY2(qAbs(after.y() - before.y()) <= 1.0,
             qPrintable(QStringLiteral("y moved from %1 to %2").arg(before.y()).arg(after.y())));
}

// SciQLop#151: an item anchored far outside the view has a pixel position beyond int range (or NaN).
// QRect arithmetic on it overflowed, which Qt 6.11's checked QRect turns into an abort on mouse move
// (selectTest) or on draw.
using ItemFactory = std::function<QCPAbstractItem*(QCustomPlot*)>;
Q_DECLARE_METATYPE(ItemFactory)

void TestItemPosition::farOffscreenItemsDoNotOverflow_data()
{
    QTest::addColumn<ItemFactory>("make");
    QTest::addColumn<double>("coord");
    const QList<QPair<const char*, ItemFactory>> items = {
        {"text", [](QCustomPlot* p) { auto* i = new QCPItemText(p); i->setText("label"); return i; }},
        {"richtext", [](QCustomPlot* p) { auto* i = new QCPItemRichText(p); i->setHtml("<b>label</b>"); return i; }},
        {"pixmap", [](QCustomPlot* p) { auto* i = new QCPItemPixmap(p); i->setPixmap(QPixmap(16, 16)); return i; }},
        {"tracer", [](QCustomPlot* p) { return new QCPItemTracer(p); }},
        {"rect", [](QCustomPlot* p) { return new QCPItemRect(p); }},
        {"ellipse", [](QCustomPlot* p) { return new QCPItemEllipse(p); }},
        {"line", [](QCustomPlot* p) { return new QCPItemLine(p); }},
        {"curve", [](QCustomPlot* p) { return new QCPItemCurve(p); }},
        {"bracket", [](QCustomPlot* p) { return new QCPItemBracket(p); }},
        {"straightline", [](QCustomPlot* p) { return new QCPItemStraightLine(p); }},
        {"vspan", [](QCustomPlot* p) { return new QCPItemVSpan(p); }},
        {"hspan", [](QCustomPlot* p) { return new QCPItemHSpan(p); }},
        {"rspan", [](QCustomPlot* p) { return new QCPItemRSpan(p); }},
    };
    for (const auto& [name, make] : items)
    {
        QTest::newRow(qPrintable(QString("%1/huge").arg(name))) << make << 1e12;
        QTest::newRow(qPrintable(QString("%1/nan").arg(name))) << make << qQNaN();
    }
}

void TestItemPosition::farOffscreenItemsDoNotOverflow()
{
    QFETCH(ItemFactory, make);
    QFETCH(double, coord);
    mPlot->xAxis->setRange(0, 10);
    mPlot->yAxis->setRange(0, 10);
    mPlot->replot();
    QCPAbstractItem* item = make(mPlot);
    const auto positions = item->positions();
    for (int i = 0; i < positions.size(); ++i)
        positions[i]->setCoords(coord + i, coord + 2 * i);

    QVERIFY(item->selectTest(QPointF(100, 100), false) != 0);
    QVERIFY(!mPlot->toPixmap(400, 300).isNull());
}
