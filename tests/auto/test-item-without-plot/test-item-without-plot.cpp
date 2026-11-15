#include "test-item-without-plot.h"
#include "../../../src/qcp.h"
#include <memory>

namespace
{
int gMisuses = 0;

void countMisuse(const char*, const char*)
{
    ++gMisuses;
}

//! A second area under the main one: it has its own axes, and so its own systems.
QCPAxisRect* addSecondRect(QCustomPlot* plot)
{
    auto* rect = new QCPAxisRect(plot);
    plot->plotLayout()->addElement(1, 0, rect);
    plot->replot();
    return rect;
}

//! Deletes itself the moment it lands: the add must survive an item whose signals run mid-add.
class DoomedLine : public QCPItemLine
{
public:
    DoomedLine()
    {
        connect(this, &QCPLayerable::layerChanged, this,
                [this](QCPLayer*) {
                    if (QCustomPlot* plot = parentPlot())
                        plot->removeItem(this);
                });
    }
};

//! A user item whose position is made and placed in the constructor, with no plot in sight.
class MarkedItem : public QCPItem
{
public:
    explicit MarkedItem(double at)
            : mPosition(createPosition(QStringLiteral("position")))
            , mTag(at)
    {
        mPosition->setCoords(at, at);
    }

    void draw(QCPDrawContext&) const override {}

    QCPItemPosition* const mPosition;
    const double mTag;
};

//! Two positions, one of them bound to an explicit system before the item is placed.
class BoundItem : public QCPItem
{
public:
    explicit BoundItem(QCPAxisRect* rect)
            : mBound(createPosition(QStringLiteral("bound")))
            , mFree(createPosition(QStringLiteral("free")))
    {
        mBound->setAxes(rect->xAxis(), rect->yAxis());
    }

    void draw(QCPDrawContext&) const override {}

    QCPItemPosition* const mBound;
    QCPItemPosition* const mFree;
};

//! Counts its instances, so a refused add can prove the object is gone.
class CountedItem : public QCPItem
{
public:
    CountedItem()
            : mPosition(createPosition(QStringLiteral("position")))
    {
        ++live;
    }

    ~CountedItem() override { --live; }

    void draw(QCPDrawContext&) const override {}

    static int live;
    QCPItemPosition* const mPosition;
};

int CountedItem::live = 0;

//! Every built-in item, built with no plot.
QList<QCPItem*> builtInItems()
{
    return {
        new QCPItemBracket(),
        new QCPItemCurve(),
        new QCPItemEllipse(),
        new QCPItemLine(),
        new QCPItemPixmap(),
        new QCPItemRect(),
        new QCPItemRichText(),
        new QCPItemRSpan(),
        new QCPItemSpan(Qt::Vertical),
        new QCPItemStraightLine(),
        new QCPItemText(),
        new QCPItemTracer(),
    };
}
} // namespace

void TestItemWithoutPlot::init()
{
    qRegisterMetaType<QCPItem*>();
    gMisuses = 0;
    mMisuseHandler = qcp::setMisuseHandler(countMisuse);
    mPlot = new QCustomPlot(nullptr);
    mPlot->resize(640, 480);
    mPlot->show();
    mPlot->xAxis->setRange(0, 10);
    mPlot->yAxis->setRange(0, 10);
    mPlot->replot();
}

void TestItemWithoutPlot::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
    qcp::setMisuseHandler(mMisuseHandler);
}

void TestItemWithoutPlot::aUserItemIsBuiltWithoutAPlot()
{
    auto* item = new MarkedItem(3.0);
    QVERIFY(!item->parentPlot());
    QVERIFY(!item->mPosition->system());
    QVERIFY(!item->mPosition->area());
    QCOMPARE(item->mPosition->coords(), QPointF(3, 3));
    // No plot: the position has no pixels, and asks for none.
    QCOMPARE(item->mPosition->pixelPosition(), QPointF(0, 0));

    item->mPosition->setCoords(4, 5);
    item->setClipToAxisRect(true);
    item->setSelected(true);
    QVERIFY(item->selected());
    for (QCPItemAnchor* anchor : item->anchors())
        anchor->pixelPosition();
    item->selectTest(QPointF(10, 10), false);
    item->update();
    QCOMPARE(gMisuses, 0);
    QCOMPARE(item->mPosition->coords(), QPointF(4, 5));

    QCPAxisRect* mainRect = mPlot->axisRect();
    auto placed = mPlot->add(std::unique_ptr<MarkedItem>(item));
    QVERIFY(placed);
    QCOMPARE(item->parentPlot(), mPlot);
    QCOMPARE(item->layer()->name(), QStringLiteral("annotations"));
    QCOMPARE(item->clipAxisRect(), mainRect);
    QVERIFY(item->clipToAxisRect());
    QCOMPARE(item->mPosition->system(), mainRect->system(mainRect->xAxis(), mainRect->yAxis()));
    QCOMPARE(item->mPosition->coords(), QPointF(4, 5));
    QCOMPARE(gMisuses, 0);
}

void TestItemWithoutPlot::aRectAddPlacesAnItemBuiltFromItsArguments()
{
    QCPAxisRect* second = addSecondRect(mPlot);
    auto placed = second->add<MarkedItem>(3.0);
    QVERIFY(placed);
    QCOMPARE(placed->mTag, 3.0);
    QCOMPARE(placed->positions().size(), 1);
    QCOMPARE(placed->mPosition->coords(), QPointF(3, 3));
    QCOMPARE(placed->parentPlot(), mPlot);
    QCOMPARE(placed->layer()->name(), QStringLiteral("annotations"));
    QCOMPARE(placed->clipAxisRect(), second);
    QVERIFY(placed->clipToAxisRect());
    QCOMPARE(placed->mPosition->area(), static_cast<QCPArea*>(second));
    QCOMPARE(placed->mPosition->system(), second->system(second->xAxis(), second->yAxis()));
    QCOMPARE(gMisuses, 0);
}

void TestItemWithoutPlot::everyBuiltInItemIsBuiltWithoutAPlot()
{
    const QList<QCPItem*> items = builtInItems();
    for (QCPItem* item : items)
    {
        QVERIFY2(!item->parentPlot(), qPrintable(item->metaObject()->className()));
        for (QCPItemPosition* position : item->positions())
        {
            position->setCoords(2, 3);
            QCOMPARE(position->pixelPosition(), QPointF(0, 0));
            // A pixel write lands on a viewport the item does not have yet: it is inert, not a
            // crash, whatever the slot type reads beside it.
            position->setType(QCPItemPosition::ptViewportRatio);
            position->setPixelPosition(QPointF(320, 240));
            position->setType(QCPItemPosition::ptAxisRectAbsolute);
            position->setPixelPosition(QPointF(12, 34));
            position->setType(QCPItemPosition::ptPlotCoords);
            position->setCoords(2, 3);
        }
        for (QCPItemAnchor* anchor : item->anchors())
            anchor->pixelPosition();
        item->setSelected(true);
        item->update();
        item->selectTest(QPointF(5, 5), false);

        const int before = mPlot->itemCount();
        auto placed = mPlot->add(std::unique_ptr<QCPItem>(item));
        QVERIFY2(placed, qPrintable(item->metaObject()->className()));
        QCOMPARE(mPlot->itemCount(), before + 1);
        QCOMPARE(item->parentPlot(), mPlot);
        QCOMPARE(item->layer()->name(), QStringLiteral("annotations"));
        QCOMPARE(item->clipAxisRect(), mPlot->axisRect());
        QVERIFY(item->clipToAxisRect());
        QVERIFY(item->selected());
        for (QCPItemPosition* position : item->positions())
        {
            QCOMPARE(position->system(),
                     mPlot->axisRect()->system(mPlot->axisRect()->xAxis(),
                                               mPlot->axisRect()->yAxis()));
            QCOMPARE(position->coords(), QPointF(2, 3));
        }
    }
    QCOMPARE(gMisuses, 0);
}

void TestItemWithoutPlot::plotAddStillUsesTheFirstRect()
{
    auto text = mPlot->add<QCPItemText>();
    QVERIFY(text);
    QCOMPARE(text->parentPlot(), mPlot);
    QCOMPARE(text->layer()->name(), QStringLiteral("annotations"));
    QCOMPARE(text->clipAxisRect(), mPlot->axisRect());
    QCOMPARE(text->position->coords(), QPointF(0, 0));
    QCOMPARE(text->position->system(),
             mPlot->axisRect()->system(mPlot->axisRect()->xAxis(), mPlot->axisRect()->yAxis()));
    QCOMPARE(gMisuses, 0);
}

void TestItemWithoutPlot::aRectAddBindsTheRectsAxes()
{
    QCPAxisRect* second = addSecondRect(mPlot);
    auto line = second->add<QCPItemLine>();
    QVERIFY(line);
    for (QCPItemPosition* position : line->positions())
    {
        QCOMPARE(position->area(), static_cast<QCPArea*>(second));
        QCOMPARE(position->system(), second->system(second->xAxis(), second->yAxis()));
    }
    QCOMPARE(line->clipAxisRect(), second);
    QCOMPARE(gMisuses, 0);
}

void TestItemWithoutPlot::aSystemGivenBeforeTheAddIsKept()
{
    QCPAxisRect* second = addSecondRect(mPlot);
    auto bound = mPlot->axisRect()->add<BoundItem>(second);
    QVERIFY(bound);
    // The position that named its system keeps it; the other one takes the rect it was placed in.
    QCOMPARE(bound->mBound->system(), second->system(second->xAxis(), second->yAxis()));
    QCOMPARE(bound->mBound->area(), static_cast<QCPArea*>(second));
    QCOMPARE(bound->mFree->system(),
             mPlot->axisRect()->system(mPlot->axisRect()->xAxis(), mPlot->axisRect()->yAxis()));
    QCOMPARE(bound->mFree->area(), static_cast<QCPArea*>(mPlot->axisRect()));
    QCOMPARE(gMisuses, 0);
}

void TestItemWithoutPlot::aClipFlagGivenBeforeTheAddIsKept()
{
    auto* item = new QCPItemText();
    item->setClipToAxisRect(false);
    auto text = mPlot->add(std::unique_ptr<QCPItemText>(item));
    QVERIFY(text);
    QVERIFY(!text->clipToAxisRect());
    QCOMPARE(text->clipAxisRect(), mPlot->axisRect());

    std::unique_ptr<QCPItemText> taken = mPlot->take(text);
    QVERIFY(taken);
    QVERIFY(!taken->clipToAxisRect());
    auto again = mPlot->add(std::move(taken));
    QVERIFY(again);
    QVERIFY(!again->clipToAxisRect()); // the take/add round trip keeps it off

    auto plain = mPlot->add<QCPItemLine>();
    QVERIFY(plain->clipToAxisRect()); // a new item clips to the area it is placed in
    QCOMPARE(gMisuses, 0);
}

void TestItemWithoutPlot::aRectAddWithoutBothAxesRefusesTheItem()
{
    QCPAxisRect* second = addSecondRect(mPlot);
    const int before = mPlot->itemCount();
    QVERIFY(second->removeAxis(second->xAxis()));
    gMisuses = 0;
    CountedItem::live = 0;

    auto counted = second->add<CountedItem>();
    QVERIFY(!counted);
    QCOMPARE(CountedItem::live, 0); // the refused item was destroyed, not leaked
    QCOMPARE(mPlot->itemCount(), before);
    QCOMPARE(gMisuses, 1);
}

void TestItemWithoutPlot::aSpanOrientationReachesTheRectAdd()
{
    QCPAxisRect* second = addSecondRect(mPlot);
    auto span = second->add<QCPItemSpan>(Qt::Horizontal);
    QVERIFY(span);
    QCOMPARE(span->orientation(), Qt::Horizontal);
    QCOMPARE(gMisuses, 0);
}

void TestItemWithoutPlot::anItemDeletedDuringItsOwnAddLeavesNoDanglingOne()
{
    const int before = mPlot->itemCount();
    const auto doomed = mPlot->axisRect()->add<DoomedLine>();
    Q_UNUSED(doomed); // gone before the add returned: the handle is dead, not followed
    QCOMPARE(mPlot->itemCount(), before);
    QCOMPARE(gMisuses, 0);
}
