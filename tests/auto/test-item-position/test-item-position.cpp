#include "test-item-position.h"
#include "qcustomplot.h"
#include <functional>

namespace
{
//! A text that says how many times it recorded.
class CountingText : public QCPItemText
{
public:
    using QCPItemText::QCPItemText;
    int recordings() const { return mCore.recordingCount(); }
};

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

void TestItemPosition::oneAxisAnchoredTextFollowsItsAnchor()
{
    // A position QCP::Position cannot express (here: x on an item's anchor, y a ratio) is recorded
    // as the pixels it had at that instant. Nothing in the recording moves them, so the recording
    // must be remade; the count is the observable, because a text always re-records on an export.
    mPlot->xAxis->setRange(0, 10);
    mPlot->yAxis->setRange(0, 10);
    auto* anchorItem = new QCPItemText(mPlot);
    anchorItem->setText(QStringLiteral("anchor"));
    anchorItem->position->setCoords(2.0, 5.0); // plot coordinates: a pan moves it
    anchorItem->setPositionAlignment(Qt::AlignLeft | Qt::AlignTop);

    auto* text = new CountingText(mPlot);
    text->setText(QStringLiteral("label"));
    text->position->setType(QCPItemPosition::ptAxisRectRatio);
    text->position->setCoords(0, 0.5);
    QVERIFY(text->position->setParentAnchorX(anchorItem->bottomRight));
    mPlot->replot();

    const int recorded = text->recordings();
    const double before = text->position->pixelPosition().x();
    QVERIFY2(recorded > 0, "the first frame must record");

    mPlot->xAxis->setRange(4, 14); // a pan: the anchor moves, nothing marks the text
    mPlot->replot();

    QVERIFY2(text->position->pixelPosition().x() < before - 50,
             "the anchor did not move: the case proves nothing");
    QVERIFY2(text->recordings() > recorded, "the text did not follow its anchor");
}

void TestItemPosition::axisRectAbsoluteTextFollowsTheAxisRect()
{
    // The same snapshot problem in its other form: an axis-rect pixel position follows a margin
    // change, so the recording that holds its pixels must be remade when the frame is redrawn.
    auto* text = new CountingText(mPlot);
    text->setText(QStringLiteral("label"));
    text->position->setType(QCPItemPosition::ptAxisRectAbsolute);
    text->position->setCoords(20, 30);
    mPlot->replot();

    const int recorded = text->recordings();
    const double before = text->position->pixelPosition().x();
    QVERIFY2(recorded > 0, "the first frame must record");

    QCPAxisRect* axisRect = mPlot->axisRect();
    axisRect->setAutoMargins(axisRect->autoMargins() & ~QCP::msLeft);
    axisRect->setMargins(axisRect->margins() + QMargins(40, 0, 0, 0));
    text->layer()->invalidatePaintBuffer(); // a layout change alone leaves this layer's buffer
    mPlot->replot();

    QVERIFY2(text->position->pixelPosition().x() > before + 20,
             "the axis rect did not move: the case proves nothing");
    QVERIFY2(text->recordings() > recorded, "the text did not follow the axis rect");
}

void TestItemPosition::parentDeletedBeforeAnyFrameKeepsPlace()
{
    // A child of a live parent reports where it is; the one whose parent is deleted before any frame
    // or hit test asked for a pixel must keep exactly that pixel, not zero. Each way of attaching
    // holds its own side, so all three are checked.
    const auto attach = [](QCPItemText* parent, QCPItemPosition* child, int axes)
    {
        if (axes & 1)
            child->setParentAnchorX(parent->bottomRight);
        if (axes & 2)
            child->setParentAnchorY(parent->bottomRight);
    };
    const auto newText = [this]()
    {
        auto* text = new QCPItemText(mPlot);
        text->position->setType(QCPItemPosition::ptAbsolute);
        text->position->setCoords(400, 300);
        text->setText(QStringLiteral("reference"));
        return text;
    };
    const auto newChild = [this]()
    {
        auto* child = new QCPItemText(mPlot);
        child->position->setType(QCPItemPosition::ptAbsolute);
        child->position->setCoords(5, 7);
        return child;
    };

    for (const int axes : { 3, 1, 2 }) // both, x only, y only
    {
        auto* reference = newText();
        QCPItemText* live = newChild();
        attach(reference, live->position, axes);
        mPlot->replot(); // a frame: the anchor reports its pixel
        const QPointF expected = live->position->pixelPosition();

        auto* doomed = newText(); // the same box, so the same anchor pixel
        QCPItemText* orphan = newChild();
        attach(doomed, orphan->position, axes);
        QVERIFY(mPlot->removeItem(doomed)); // no frame and no hit test between attach and delete

        const QPointF after = orphan->position->pixelPosition();
        QVERIFY2(qAbs(after.x() - expected.x()) <= 1 && qAbs(after.y() - expected.y()) <= 1,
                 qPrintable(QStringLiteral("axes %1: the child moved from %2,%3 to %4,%5")
                                .arg(axes)
                                .arg(expected.x())
                                .arg(expected.y())
                                .arg(after.x())
                                .arg(after.y())));
    }
}

// SciQLop#151: an item anchored far outside the view has a pixel position beyond int range (or NaN).
// QRect arithmetic on it overflowed, which Qt 6.11's checked QRect turns into an abort on mouse move
// (selectTest) or on draw.
using ItemFactory = std::function<QCPAbstractItem*(QCustomPlot*)>;
Q_DECLARE_METATYPE(ItemFactory)

void TestItemPosition::setRefusedAnchorIsReported()
{
    mPlot->replot();
    auto* parent = new QCPItemText(mPlot);
    parent->position->setCoords(100, 100);
    auto* child = new QCPItemText(mPlot);
    child->position->setCoords(200, 200);

    // The child follows the parent: an anchor the plot can resolve.
    child->position->set(QCP::AnchorOffset { parent->position, QCP::PixelOffset { 5, 6 } });
    QCOMPARE(child->position->parentAnchor(), parent->position);

    // The reverse link closes a cycle: set() must leave the position alone and say why.
    QTest::ignoreMessage(QtDebugMsg,
                         QRegularExpression(QStringLiteral("recursive parent-child")));
    QTest::ignoreMessage(QtDebugMsg, QRegularExpression(QStringLiteral("the anchor was refused")));
    const QPointF before = parent->position->pixelPosition();
    parent->position->set(QCP::AnchorOffset { child->position, QCP::PixelOffset { 5, 6 } });
    QVERIFY2(!parent->position->parentAnchor(), "a refused anchor was attached");
    QCOMPARE(parent->position->pixelPosition(), before);
}

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
