#include "test-item-events.h"

#include "../../../src/qcp.h"

#include <QCoreApplication>
#include <QMouseEvent>

#include <cmath>
#include <functional>

using namespace QCP::Literals;

namespace
{
void sendMouse(QCustomPlot* plot, QEvent::Type type, QPoint pos, Qt::MouseButtons buttons)
{
    QMouseEvent event(type, pos, plot->mapToGlobal(pos), Qt::LeftButton, buttons,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(plot, &event);
}

/*!
  An item the tests drive: it records ratio boxes, so a click lands on a part the test knows, and it
  keeps what every mouse handler saw.
*/
class Probe : public QCPItem
{
public:
    enum Shape
    {
        oneBox,
        twoBoxes
    };

    Probe()
        : mFrame(createPosition(QStringLiteral("frame")))
    {
    }

    void parentPlotInitialized(QCustomPlot* plot) override
    {
        QCPItem::parentPlotInitialized(plot);
        mFrame->setAxes(plot->xAxis, plot->yAxis);
        mFrame->setCoords(1.0, 1.0);
    }

    void draw(QCPDrawContext& context) const override
    {
        const QPen outline(Qt::black, 3);
        if (shape == twoBoxes)
        {
            context.setPart(1);
            context.rect(QCP::AxisRatio { 0.0, 0.0 }, QCP::AxisRatio { 0.5, 1.0 }, outline,
                         QBrush(QColor(200, 40, 40)));
            context.setPart(2);
            context.rect(QCP::AxisRatio { 0.5, 0.0 }, QCP::AxisRatio { 1.0, 1.0 }, outline,
                         QBrush(QColor(40, 160, 40)));
            return;
        }
        context.setPart(0);
        context.rect(QCP::AxisRatio { 0.25, 0.25 }, QCP::AxisRatio { 0.75, 0.75 }, outline,
                     QBrush(QColor(220, 200, 40)));
    }

    void mousePressEvent(QCPMouseEvent* event) override
    {
        ++pressCount;
        pressPart = event->part();
        pressData = event->data();
        pressPixels = event->pixels();
        if (onPress)
        {
            onPress(*this);
            return; // the handler may delete the item, and its plot with it
        }
        if (acceptPress)
            event->accept();
        else
            event->ignore();
    }

    void mouseMoveEvent(QCPMouseEvent* event) override
    {
        ++moveCount;
        moveParts.append(event->part());
        moveDiffs.append(QPointF(event->pixels().x - event->lastPixels().x,
                                 event->pixels().y - event->lastPixels().y));
        moveData.append(event->data());
        if (onMove)
        {
            onMove(*this);
            return; // the handler may delete the item
        }
    }

    void mouseReleaseEvent(QCPMouseEvent* event) override
    {
        ++releaseCount;
        releasePart = event->part();
    }

    void mouseDoubleClickEvent(QCPMouseEvent* event) override
    {
        ++doubleClickCount;
        Q_UNUSED(event)
    }

    QCPItemPosition* const mFrame;
    Shape shape = oneBox;
    bool acceptPress = true;
    std::function<void(Probe&)> onPress;
    std::function<void(Probe&)> onMove;
    int pressCount = 0;
    int moveCount = 0;
    int releaseCount = 0;
    int doubleClickCount = 0;
    int pressPart = -99;
    int releasePart = -99;
    QList<int> moveParts;
    QList<QPointF> moveDiffs;
    QList<QCP::Data> moveData;
    QCP::Data pressData {};
    QCP::Pixels pressPixels {};
};
} // namespace

void TestItemEvents::init()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
    mPlot->xAxis->setRange(0, 10);
    mPlot->yAxis->setRange(0, 10);
    mPlot->replot();
}

void TestItemEvents::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
}

void TestItemEvents::anAcceptedPressGrabsTheDragAndStopsThePan()
{
    mPlot->setInteractions(QCP::iRangeDrag);
    Probe* probe = mPlot->add<Probe>().get();
    probe->acceptPress = true;
    mPlot->replot();

    const QPoint from = mPlot->viewport().center();
    sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseMove, from + QPoint(40, 30), Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseButtonRelease, from + QPoint(40, 30), Qt::NoButton);

    QCOMPARE(probe->pressCount, 1);
    QCOMPARE(probe->moveCount, 1);
    QCOMPARE(probe->moveDiffs.at(0), QPointF(40, 30));
    QCOMPARE(mPlot->xAxis->range().lower, 0.0);
    QCOMPARE(mPlot->xAxis->range().upper, 10.0);
    QCOMPARE(mPlot->yAxis->range().lower, 0.0);
    QCOMPARE(mPlot->yAxis->range().upper, 10.0);
}

void TestItemEvents::anIgnoredPressPans()
{
    mPlot->setInteractions(QCP::iRangeDrag);
    Probe* probe = mPlot->add<Probe>().get();
    probe->acceptPress = false;
    mPlot->replot();

    const QPoint from = mPlot->viewport().center();
    sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseMove, from + QPoint(40, 0), Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseButtonRelease, from + QPoint(40, 0), Qt::NoButton);

    QCOMPARE(probe->pressCount, 1);
    QCOMPARE(probe->moveCount, 0);
    QVERIFY2(mPlot->xAxis->range().lower < 0.0, "the drag panned the range under the item");
    QCOMPARE(mPlot->yAxis->range().lower, 0.0);
}

void TestItemEvents::theDragKeepsThePressPart()
{
    Probe* probe = mPlot->add<Probe>().get();
    probe->shape = Probe::twoBoxes;
    mPlot->replot();

    const QRect rect = mPlot->viewport();
    const QPoint left(rect.center().x() - rect.width() / 8, rect.center().y());
    const QPoint right(rect.center().x() + rect.width() / 8, rect.center().y());

    sendMouse(mPlot, QEvent::MouseButtonPress, left, Qt::LeftButton);
    QCOMPARE(probe->pressPart, 1);
    sendMouse(mPlot, QEvent::MouseButtonRelease, left, Qt::NoButton);

    sendMouse(mPlot, QEvent::MouseButtonPress, right, Qt::LeftButton);
    QCOMPARE(probe->pressPart, 2);
    sendMouse(mPlot, QEvent::MouseMove, left, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseMove, left + QPoint(5, 0), Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseButtonRelease, left + QPoint(5, 0), Qt::NoButton);

    QCOMPARE(probe->moveParts.size(), 2);
    QCOMPARE(probe->moveParts.at(0), 2);
    QCOMPARE(probe->moveParts.at(1), 2);
}

void TestItemEvents::lastIsThePreviousEvent()
{
    Probe* probe = mPlot->add<Probe>().get();
    mPlot->replot();

    const QPoint from = mPlot->viewport().center();
    sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseMove, from + QPoint(10, 0), Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseMove, from + QPoint(25, 0), Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseButtonRelease, from + QPoint(25, 0), Qt::NoButton);

    QCOMPARE(probe->moveDiffs.size(), 2);
    QCOMPARE(probe->moveDiffs.at(0), QPointF(10, 0));
    QCOMPARE(probe->moveDiffs.at(1), QPointF(15, 0));
}

void TestItemEvents::dataFollowsTheItemsOwnAxes_data()
{
    QTest::addColumn<bool>("secondRect");
    QTest::newRow("a reversed log key axis") << false;
    QTest::newRow("an item measuring in the second rect") << true;
}

void TestItemEvents::dataFollowsTheItemsOwnAxes()
{
    QFETCH(bool, secondRect);
    Probe* probe = mPlot->add<Probe>().get();
    QCPAxis* keyAxis = mPlot->xAxis;
    QCPAxis* valueAxis = mPlot->yAxis;
    QPoint from;
    if (secondRect)
    {
        QCPLayoutGrid* layout = mPlot->plotLayout();
        layout->insertRow(1);
        auto* second = new QCPAxisRect(mPlot);
        layout->addElement(1, 0, second);
        keyAxis = second->axis(QCPAxis::atBottom);
        valueAxis = second->axis(QCPAxis::atLeft);
        keyAxis->setRange(100, 200);
        valueAxis->setRange(-5, 5);
        probe->mFrame->setAxes(keyAxis, valueAxis);
        // The probe measures ratios against its own rect, so clipping it there puts its boxes
        // where the click lands as well as naming the axes it reports in.
        probe->setClipToAxisRect(true);
        probe->setClipAxisRect(second);
        mPlot->replot();
        from = second->rect().center();
    }
    else
    {
        mPlot->xAxis->setScale(QCPScale::log());
        mPlot->xAxis->setRange(1, 1000);
        mPlot->xAxis->setRangeReversed(true);
        mPlot->replot();
        from = mPlot->viewport().center();
    }

    sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);

    QCOMPARE(probe->pressCount, 1);
    const double key = keyAxis->pixelToCoord(from.x());
    const double value = valueAxis->pixelToCoord(from.y());
    QVERIFY2(std::abs(probe->pressData.x - key) <= 1e-9 * std::abs(key),
             "the event's key is the one the item's own key axis gives");
    QVERIFY2(std::abs(probe->pressData.y - value) <= 1e-9 * std::abs(value),
             "the event's value is the one the item's own value axis gives");
}

void TestItemEvents::aDragOutsideThePlotStillEndsOnRelease()
{
    Probe* probe = mPlot->add<Probe>().get();
    mPlot->replot();

    const QPoint from = mPlot->viewport().center();
    const QPoint outside(-40, -30);
    sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseMove, outside, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseButtonRelease, outside, Qt::NoButton);

    QCOMPARE(probe->moveCount, 1);
    QCOMPARE(probe->releaseCount, 1);
    QCOMPARE(probe->releasePart, 0);
    QCOMPARE(probe->moveDiffs.at(0), QPointF(outside - from));
    // The press was the whole drag: a move without a button reaches the item no more.
    sendMouse(mPlot, QEvent::MouseMove, outside + QPoint(5, 5), Qt::NoButton);
    QCOMPARE(probe->moveCount, 1);
}

void TestItemEvents::anItemDeletedInItsMoveHandler()
{
    Probe* probe = mPlot->add<Probe>().get();
    probe->onMove = [](Probe& self) { delete &self; };
    mPlot->replot();

    const QPoint from = mPlot->viewport().center();
    sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseMove, from + QPoint(10, 0), Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseMove, from + QPoint(20, 0), Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseButtonRelease, from + QPoint(20, 0), Qt::NoButton);

    QCOMPARE(mPlot->itemCount(), 0);
    mPlot->replot();
}

void TestItemEvents::aPlotDeletedInAPressHandler()
{
    Probe* probe = mPlot->add<Probe>().get();
    probe->onPress = [](Probe& self) { delete self.parentPlot().get(); };
    mPlot->replot();

    const QPoint from = mPlot->viewport().center();
    QMouseEvent event(QEvent::MouseButtonPress, from, mPlot->mapToGlobal(from), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(mPlot, &event);
    mPlot = nullptr; // the handler deleted it, and the dispatch returned on a dead plot
}

void TestItemEvents::spansStillDrag()
{
    auto span = mPlot->add<QCPItemSpan>(Qt::Vertical);
    span->setRange(QCPRange(3, 7));
    mPlot->replot();

    const double lower = mPlot->xAxis->pixelToCoord(mPlot->xAxis->coordToPixel(3) + 40);
    const double upper = mPlot->xAxis->pixelToCoord(mPlot->xAxis->coordToPixel(7) + 40);
    const QPoint from(qRound(mPlot->xAxis->coordToPixel(5)),
                      mPlot->axisRect()->rect().center().y());
    sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseMove, from + QPoint(40, 0), Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseButtonRelease, from + QPoint(40, 0), Qt::NoButton);

    QCOMPARE(span->range().lower, lower);
    QCOMPARE(span->range().upper, upper);
}
