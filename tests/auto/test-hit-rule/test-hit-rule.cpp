#include "test-hit-rule.h"
#include "qcustomplot.h"
#include <vector>

namespace
{
// A non-selectable label that records the press it takes, to show it still wins the press while
// the graph under it wins the selection.
class CountingText : public QCPItemText
{
public:
    using QCPItemText::QCPItemText;
    int presses = 0;

protected:
    void mousePressEvent(QMouseEvent* event, const QVariant&) override
    {
        ++presses;
        event->accept();
    }
};

bool hitsContain(const QList<QCPHit>& hits, const QCPLayerable* layerable)
{
    for (const QCPHit& hit : hits)
        if (hit.layerable == layerable)
            return true;
    return false;
}

QCPGraph2* addDiagonalGraph2(QCustomPlot* plot)
{
    auto* graph = new QCPGraph2(plot->xAxis, plot->yAxis);
    graph->setData(std::vector<double> { 0, 10 }, std::vector<double> { 0, 10 });
    return graph;
}

QCPMultiGraph* addDiagonalMultiGraph(QCustomPlot* plot)
{
    auto* graph = new QCPMultiGraph(plot->xAxis, plot->yAxis);
    graph->setData(std::vector<double> { 0, 10 }, std::vector<std::vector<double>> { { 0, 10 } });
    return graph;
}
} // namespace

void TestHitRule::init()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
    mPlot->xAxis->setRange(0, 10);
    mPlot->yAxis->setRange(0, 10);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
}

void TestHitRule::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
}

QPointF TestHitRule::pixelOf(double key, double value) const
{
    return { mPlot->xAxis->coordToPixel(key), mPlot->yAxis->coordToPixel(value) };
}

void TestHitRule::sendMouse(QEvent::Type type, const QPoint& pos, Qt::MouseButton button)
{
    const Qt::MouseButton buttons = type == QEvent::MouseButtonPress ? button : Qt::NoButton;
    QMouseEvent event(type, pos, mPlot->mapToGlobal(pos), button, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(mPlot, &event);
}

void TestHitRule::wheelAt(QPointF pos)
{
    QWheelEvent event(pos, mPlot->mapToGlobal(pos), QPoint(), QPoint(0, 120), Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(mPlot, &event);
}

// The wheel used to take a graph2 anywhere in its axis rect: a point 150 px from the curve must
// miss it, a point on the curve must hit it.
void TestHitRule::wheelNeedsDistanceToo()
{
    auto* graph = addDiagonalGraph2(mPlot);
    auto* multi = addDiagonalMultiGraph(mPlot);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    const QPointF far = pixelOf(1, 9);
    QVERIFY(mPlot->axisRect()->rect().contains(far.toPoint()));
    QVERIFY(!hitsContain(mPlot->hitsAt(far, QCPHitAction::Press), graph));
    QVERIFY(!hitsContain(mPlot->hitsAt(far, QCPHitAction::Press), multi));

    const QPointF onCurve = pixelOf(5, 5);
    QVERIFY(hitsContain(mPlot->hitsAt(onCurve, QCPHitAction::Press), graph));
    QVERIFY(hitsContain(mPlot->hitsAt(onCurve, QCPHitAction::Press), multi));
}

void TestHitRule::pressAndSelectionAgreeWhenSelectable()
{
    auto* graph = addDiagonalGraph2(mPlot);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    const QPointF pos = pixelOf(5, 5);
    const QList<QCPHit> press = mPlot->hitsAt(pos, QCPHitAction::Press);
    const QList<QCPHit> select = mPlot->hitsAt(pos, QCPHitAction::Select);
    QVERIFY(!press.isEmpty());
    QVERIFY(!select.isEmpty());
    QCOMPARE(press.first().layerable, static_cast<QCPLayerable*>(graph));
    QCOMPARE(select.first().layerable, press.first().layerable);
}

void TestHitRule::nonSelectableLabelTakesPressGraphTakesSelection()
{
    mPlot->setInteraction(QCP::iSelectPlottables);
    auto* graph = addDiagonalGraph2(mPlot);
    auto* text = new CountingText(mPlot);
    text->setText(QStringLiteral("label"));
    text->position->setCoords(5, 5);
    text->setSelectable(QCP::stNone);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    const QPointF pos = pixelOf(5, 5);
    QCOMPARE(mPlot->hitsAt(pos, QCPHitAction::Press).first().layerable,
             static_cast<QCPLayerable*>(text));
    QCOMPARE(mPlot->hitsAt(pos, QCPHitAction::Select).first().layerable,
             static_cast<QCPLayerable*>(graph));

    QTest::mouseClick(mPlot, Qt::LeftButton, Qt::NoModifier, pos.toPoint());
    QVERIFY(graph->selected());
    QCOMPARE(text->presses, 1);
}

void TestHitRule::axisRectWheelZoomStillWins()
{
    mPlot->setInteraction(QCP::iRangeZoom);
    addDiagonalGraph2(mPlot);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    const QCPRange before = mPlot->xAxis->range();
    wheelAt(pixelOf(5, 5));
    QVERIFY2(mPlot->xAxis->range().size() < before.size(),
             qPrintable(QString::number(mPlot->xAxis->range().size())));
}

void TestHitRule::topmostWithinToleranceWins()
{
    auto* lower = new QCPItemText(mPlot);
    lower->setText(QStringLiteral("hit"));
    lower->position->setCoords(5, 5);
    auto* upper = new QCPItemText(mPlot);
    upper->setText(QStringLiteral("hit"));
    upper->position->setCoords(5, 5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    const QPointF pos = pixelOf(5, 5);
    for (const QCPHitAction action : { QCPHitAction::Press, QCPHitAction::Select })
    {
        const QList<QCPHit> hits = mPlot->hitsAt(pos, action);
        QVERIFY(hits.size() >= 2);
        QCOMPARE(hits.first().layerable, static_cast<QCPLayerable*>(upper));
        QVERIFY(hitsContain(hits, lower));
    }
}

void TestHitRule::spanDragStillBeatsRangeDrag()
{
    mPlot->setInteraction(QCP::iRangeDrag);
    auto* span = new QCPItemVSpan(mPlot);
    span->setRange(QCPRange(3, 7));
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    const QCPRange before = mPlot->xAxis->range();
    const QPoint from(qRound(mPlot->xAxis->coordToPixel(5)),
                      mPlot->axisRect()->rect().center().y());
    sendMouse(QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(QEvent::MouseMove, from + QPoint(40, 0), Qt::NoButton);
    sendMouse(QEvent::MouseButtonRelease, from + QPoint(40, 0), Qt::NoButton);

    QCOMPARE(mPlot->xAxis->range().lower, before.lower);
    QCOMPARE(mPlot->xAxis->range().upper, before.upper);
    QVERIFY(span->range().lower > 3.0);
    QVERIFY(span->range().upper > 7.0);
}
