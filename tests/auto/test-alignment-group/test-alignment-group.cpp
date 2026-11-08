#include "test-alignment-group.h"

#include <QHBoxLayout>
#include <QSignalSpy>
#include <QVBoxLayout>
#include <QWidget>

#include "qcustomplot.h"

namespace
{
int gMisuses = 0;

void recordMisuse(const char*, const char*) { ++gMisuses; }

QPoint plotOrigin(const QCustomPlot* plot)
{
    return plot->mapTo(plot->window(), QPoint(0, 0));
}

//! The pixel of one inner edge of a layout element, in the coordinates of the window it sits in.
int innerEdge(const QCPLayoutElement* element, const QCustomPlot* plot, QCP::MarginSide side)
{
    const QPoint origin = plotOrigin(plot);
    const QRect outer = element->outerRect();
    const QMargins margins = element->margins();
    switch (side)
    {
    case QCP::msLeft:
        return origin.x() + outer.left() + margins.left();
    case QCP::msRight:
        return origin.x() + outer.right() - margins.right();
    case QCP::msTop:
        return origin.y() + outer.top() + margins.top();
    case QCP::msBottom:
        return origin.y() + outer.bottom() - margins.bottom();
    default:
        return 0;
    }
}

int innerLeft(QCustomPlot* plot)
{
    return innerEdge(plot->axisRect(), plot, QCP::msLeft);
}

int innerRight(QCustomPlot* plot)
{
    return innerEdge(plot->axisRect(), plot, QCP::msRight);
}

//! A plot with one graph and, unless told otherwise, y labels wide enough to widen its left margin.
QCustomPlot* makePlot(bool wideLeftLabels = true)
{
    auto* plot = new QCustomPlot;
    plot->setMinimumSize(200, 80);
    plot->addGraph();
    plot->graph(0)->setData({ 0, 1 }, { 0, 1 });
    plot->yAxis->setRange(0, wideLeftLabels ? 1e9 : 10);
    plot->replot();
    return plot;
}

//! A window stacking the plots vertically, each shifted right by spacer pixels.
QWidget* makeStack(const QList<QCustomPlot*>& plots, int spacer = 0)
{
    auto* window = new QWidget;
    auto* column = new QVBoxLayout(window);
    for (auto* plot : plots)
    {
        auto* row = new QHBoxLayout;
        if (spacer > 0)
            row->addSpacing(spacer);
        row->addWidget(plot);
        column->addLayout(row);
    }
    return window;
}

//! A window that places the plots by geometry only: one plot's move or resize leaves the others.
QWidget* makeFreeStack(const QList<QCustomPlot*>& plots, int left = 0)
{
    auto* window = new QWidget;
    int top = 0;
    for (auto* plot : plots)
    {
        plot->setParent(window);
        plot->setGeometry(left, top, 300, 100);
        plot->show();
        top += 120;
    }
    window->resize(400, top);
    return window;
}

/*! Replots the plots until a round causes no extra repaint, so a group that marks its peers has
  settled. Returns false when that did not happen within maxRounds; roundsUsed gets the rounds. */
bool settle(const QList<QCustomPlot*>& plots, int maxRounds = 4, int* roundsUsed = nullptr)
{
    QList<int> counts(plots.size(), 0);
    QList<QMetaObject::Connection> connections;
    for (int i = 0; i < plots.size(); ++i)
        connections << QObject::connect(plots[i], &QCustomPlot::afterReplot, plots[i],
                                        [&counts, i] { ++counts[i]; });
    bool settled = false;
    int rounds = 0;
    while (rounds < maxRounds && !settled)
    {
        ++rounds;
        for (int& count : counts)
            count = 0;
        for (auto* plot : plots)
            plot->replot();
        QTest::qWait(20); // lets a peer's singleShot repaint run
        settled = true;
        for (int count : counts)
            if (count > 1)
                settled = false;
    }
    for (const QMetaObject::Connection& connection : connections)
        QObject::disconnect(connection);
    if (roundsUsed)
        *roundsUsed = rounds;
    return settled;
}
} // namespace

void TestAlignmentGroup::init()
{
    gMisuses = 0;
    qcp::setMisuseHandler(recordMisuse);
}

void TestAlignmentGroup::cleanup() { qcp::setMisuseHandler(nullptr); }

void TestAlignmentGroup::leftEdgesAlignAcrossPlots()
{
    auto* a = makePlot();
    auto* b = makePlot(false);
    QScopedPointer<QWidget> window(makeStack({ a, b }, 24));
    window->resize(420, 260);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    QVERIFY(settle({ a, b }));
    const int beforeA = innerLeft(a);
    const int beforeB = innerLeft(b);
    QVERIFY2(beforeA != beforeB, "the two plots already start at the same inner left");

    const auto group = QCPAlignmentGroup::create({ a->axisRect(), b->axisRect() });
    QVERIFY(group);
    QCOMPARE(group->elements().size(), 2);
    QVERIFY(settle({ a, b }));
    QCOMPARE(innerLeft(a), innerLeft(b));
    QCOMPARE(innerLeft(a), beforeA); // the widest requirement keeps its edge
    QVERIFY2(innerLeft(b) > beforeB, "the narrower plot took the wider plot's margin");
}

void TestAlignmentGroup::rightEdgesAlignWithAColourScale()
{
    auto* a = makePlot(false);
    auto* b = makePlot(false);
    auto* map = new QCPColorMap(b->xAxis, b->yAxis);
    map->data()->setSize(2, 2);
    b->axisRect()->colorScale()->setDataRange(QCPRange(0, 1));
    QScopedPointer<QWidget> window(makeStack({ a, b }));
    window->resize(420, 260);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    QVERIFY(settle({ a, b }));
    QVERIFY(innerRight(a) != innerRight(b));

    const auto group = QCPAlignmentGroup::create({ a->axisRect(), b->axisRect() });
    QVERIFY(group);
    QVERIFY(settle({ a, b }));
    QCOMPARE(innerRight(a), innerRight(b));
}

void TestAlignmentGroup::userColourScaleCellAligns()
{
    auto* plot = new QCustomPlot;
    plot->setMinimumSize(320, 200);
    plot->addGraph();
    plot->graph(0)->setData({ 0, 1 }, { 0, 1 });
    plot->yAxis->setRange(0, 1e9);
    auto* scale = new QCPColorScale(plot);
    plot->plotLayout()->addElement(0, 1, scale);
    scale->setDataRange(QCPRange(0, 1));
    plot->replot();
    QScopedPointer<QWidget> window(makeStack({ plot }));
    window->resize(360, 240);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    QVERIFY(settle({ plot }));
    QVERIFY(innerEdge(scale, plot, QCP::msTop) != innerEdge(plot->axisRect(), plot, QCP::msTop));

    const auto group = QCPAlignmentGroup::create({ plot->axisRect(), scale },
                                                 QCP::msTop | QCP::msBottom);
    QVERIFY(group);
    QVERIFY(settle({ plot }));
    QCOMPARE(innerEdge(scale, plot, QCP::msTop), innerEdge(plot->axisRect(), plot, QCP::msTop));
    QCOMPARE(innerEdge(scale, plot, QCP::msBottom),
             innerEdge(plot->axisRect(), plot, QCP::msBottom));
}

void TestAlignmentGroup::stackedRectsInOnePlotAlign()
{
    auto* plot = new QCustomPlot;
    plot->setMinimumSize(300, 240);
    plot->addGraph();
    plot->yAxis->setRange(0, 1e9);
    auto* second = new QCPAxisRect(plot);
    plot->plotLayout()->addElement(1, 0, second);
    second->axis(QCPAxis::atLeft)->setRange(0, 10);
    plot->replot();
    QScopedPointer<QWidget> window(makeStack({ plot }));
    window->resize(340, 300);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    QVERIFY(settle({ plot }));
    QVERIFY(innerEdge(second, plot, QCP::msLeft) != innerEdge(plot->axisRect(), plot, QCP::msLeft));

    const auto group = QCPAlignmentGroup::create({ plot->axisRect(), second });
    QVERIFY(group);
    QVERIFY(settle({ plot }));
    QCOMPARE(innerEdge(second, plot, QCP::msLeft), innerEdge(plot->axisRect(), plot, QCP::msLeft));
}

void TestAlignmentGroup::firstShowSettlesInTwoRepaints()
{
    auto* a = makePlot();
    auto* b = makePlot(false);
    const auto group = QCPAlignmentGroup::create({ a->axisRect(), b->axisRect() });
    QVERIFY(group);
    QScopedPointer<QWidget> window(makeStack({ a, b }, 24));
    window->resize(420, 260);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    int rounds = 0;
    QVERIFY(settle({ a, b }, 4, &rounds));
    QVERIFY2(rounds <= 2, qPrintable(QStringLiteral("%1 rounds").arg(rounds)));
    QCOMPARE(innerLeft(a), innerLeft(b));
}

void TestAlignmentGroup::aMemberResizedRealigns()
{
    auto* a = makePlot();
    auto* b = makePlot(false);
    QScopedPointer<QWidget> window(makeFreeStack({ a, b }, 24));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    const auto group = QCPAlignmentGroup::create({ a->axisRect(), b->axisRect() }, QCP::msRight);
    QVERIFY(group);
    QVERIFY(settle({ a, b }));
    QCOMPARE(innerRight(a), innerRight(b));

    b->resize(b->width() - 60, b->height());
    QTest::qWait(200); // only the group's own follow-up pass may relay the peer out
    QCOMPARE(innerRight(a), innerRight(b));
}

void TestAlignmentGroup::hiddenMemberIsLeftOut()
{
    auto* a = makePlot();
    auto* b = makePlot(false);
    auto* reference = makePlot(false); // the same plot, never grouped
    QScopedPointer<QWidget> window(makeFreeStack({ a, b, reference }, 24));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    const auto group = QCPAlignmentGroup::create({ a->axisRect(), b->axisRect() });
    QVERIFY(group);
    QVERIFY(settle({ a, b, reference }));
    QCOMPARE(innerLeft(a), innerLeft(b));
    QVERIFY(innerLeft(b) > innerLeft(reference)); // the wide labels moved B inwards
    const int own = innerLeft(reference);        // B's edge without the group

    a->hide();
    QTest::qWait(150); // no replot of B: hiding A must schedule it
    QCOMPARE(innerLeft(b), own);

    a->show();
    QTest::qWait(150);
    QCOMPARE(innerLeft(a), innerLeft(b));
    QVERIFY(innerLeft(b) > own);
}

void TestAlignmentGroup::destroyedPlotLeavesItsGroup()
{
    auto* a = makePlot();
    auto* b = makePlot();
    auto* c = makePlot();
    const auto group = QCPAlignmentGroup::create({ a->axisRect(), b->axisRect(), c->axisRect() });
    QVERIFY(group);
    QCOMPARE(group->elements().size(), 3);
    delete a;
    QCOMPARE(group->elements().size(), 2);
    QCOMPARE(gMisuses, 0);

    auto* holder = makePlot();
    auto* second = new QCPAxisRect(holder);
    holder->plotLayout()->addElement(1, 0, second);
    group->add(holder->axisRect());
    group->add(second);
    QCOMPARE(group->elements().size(), 4);
    delete holder;
    QCOMPARE(group->elements().size(), 2);
    QCOMPARE(gMisuses, 0);
}

void TestAlignmentGroup::lastMemberLeavingDeletesTheGroup()
{
    auto* a = makePlot();
    auto* b = makePlot();
    QPointer<QCPAlignmentGroup> group = QCPAlignmentGroup::create({ a->axisRect(), b->axisRect() });
    QVERIFY(group);
    group->remove(a->axisRect());
    QVERIFY(group);
    group->remove(b->axisRect());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(!group);
    delete a;
    delete b;
}

void TestAlignmentGroup::joiningAnotherGroupLeavesTheFirst()
{
    auto* a = makePlot();
    auto* b = makePlot();
    auto* c = makePlot();
    const auto first = QCPAlignmentGroup::create({ a->axisRect(), b->axisRect() });
    const auto second = QCPAlignmentGroup::create({ c->axisRect() });
    QVERIFY(first);
    QVERIFY(second);
    second->add(a->axisRect());
    QCOMPARE(first->elements().size(), 1);
    QCOMPARE(second->elements().size(), 2);
    QVERIFY(!first->elements().contains(a->axisRect()));
    delete a;
    delete b;
    delete c;
}

void TestAlignmentGroup::refusedCreate()
{
    QVERIFY(!QCPAlignmentGroup::create({}));
    QCOMPARE(gMisuses, 1);
    QVERIFY(!QCPAlignmentGroup::create({ static_cast<QCPLayoutElement*>(nullptr) }));
    QCOMPARE(gMisuses, 2);
    QCPLayoutElement orphan;
    QVERIFY(!QCPAlignmentGroup::create({ &orphan }));
    QCOMPARE(gMisuses, 3);
}

void TestAlignmentGroup::settledStackRepaintsNoPeer()
{
    auto* a = makePlot();
    auto* b = makePlot(false);
    const auto group = QCPAlignmentGroup::create({ a->axisRect(), b->axisRect() });
    QVERIFY(group);
    QScopedPointer<QWidget> window(makeStack({ a, b }, 24));
    window->resize(420, 260);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    QVERIFY(settle({ a, b }));

    int peerRepaints = 0;
    const auto connection = connect(a, &QCustomPlot::afterReplot, a,
                                    [&peerRepaints] { ++peerRepaints; });
    b->graph(0)->setData({ 0, 1 }, { 1, 0 });
    b->replot();
    QTest::qWait(200);
    QCOMPARE(peerRepaints, 0);

    b->yAxis->setRange(0, 1e12); // wider labels: the shared edge moves
    b->replot();
    QTest::qWait(200);
    QVERIFY2(peerRepaints >= 1, "the peer did not repaint after the shared edge moved");
    QObject::disconnect(connection);
    QVERIFY(settle({ a, b }));
    QCOMPARE(innerLeft(a), innerLeft(b));
}

void TestAlignmentGroup::aPeerMoveRepaintsNoOtherPlot()
{
    auto* a = makePlot();
    auto* b = makePlot(false);
    QScopedPointer<QWidget> window(makeFreeStack({ a, b }, 24));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    const auto group = QCPAlignmentGroup::create({ a->axisRect(), b->axisRect() });
    QVERIFY(group);
    QVERIFY(settle({ a, b }));

    int repaints = 0;
    const auto counted
        = connect(a, &QCustomPlot::afterReplot, a, [&repaints] { ++repaints; });
    b->move(b->x(), b->y() + 10);
    QTest::qWait(200);
    disconnect(counted);
    QCOMPARE(repaints, 0);
}

void TestAlignmentGroup::aPeerHeightChangeRepaintsNoOtherPlot()
{
    auto* a = makePlot();
    auto* b = makePlot(false);
    QScopedPointer<QWidget> window(makeFreeStack({ a, b }, 24));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    const auto group = QCPAlignmentGroup::create({ a->axisRect(), b->axisRect() });
    QVERIFY(group);
    QVERIFY(settle({ a, b }));

    int repaints = 0;
    const auto counted
        = connect(a, &QCustomPlot::afterReplot, a, [&repaints] { ++repaints; });
    b->resize(b->width(), b->height() + 20);
    QTest::qWait(200);
    disconnect(counted);
    QCOMPARE(repaints, 0);
}

void TestAlignmentGroup::aPeerSidewaysMoveRealigns()
{
    auto* a = makePlot();
    auto* b = makePlot(false);
    QScopedPointer<QWidget> window(makeFreeStack({ a, b }, 24));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    const auto group = QCPAlignmentGroup::create({ a->axisRect(), b->axisRect() });
    QVERIFY(group);
    QVERIFY(settle({ a, b }));
    QCOMPARE(innerLeft(a), innerLeft(b));

    b->move(b->x() + 30, b->y());
    QVERIFY(settle({ a, b }));
    QCOMPARE(innerLeft(a), innerLeft(b));
}

void TestAlignmentGroup::sideBySideRectsAlignAndSettle()
{
    QCustomPlot plot;
    plot.resize(600, 300);
    QCPAxisRect* left = plot.axisRect();
    QCPAxisRect* right = new QCPAxisRect(&plot);
    plot.plotLayout()->addElement(0, 1, right);
    // Only the left rect carries a tall top axis: the two want different top margins.
    left->axis(QCPAxis::atTop)->setVisible(true);
    left->axis(QCPAxis::atTop)->setTickLabels(true);
    left->axis(QCPAxis::atTop)->setLabel("top label");
    right->axis(QCPAxis::atTop)->setVisible(false);
    plot.show();
    QVERIFY(QTest::qWaitForWindowExposed(&plot));

    QCPAlignmentGroup::create({left, right}, QCP::msTop | QCP::msBottom);
    int repaints = 0;
    const auto connection = QObject::connect(&plot, &QCustomPlot::afterReplot,
                                            [&repaints] { ++repaints; });
    plot.replot();
    // Each round waits for the group's own follow-up pass. Once a round is quiet, aligned
    // elements have stopped asking each other to lay out again.
    int round = 0;
    for (; round < 4; ++round)
    {
        repaints = 0;
        QTest::qWait(60);
        if (repaints == 0)
            break;
    }
    QObject::disconnect(connection);
    QVERIFY2(round < 4, "the side by side rects never settled");
    QCOMPARE(innerEdge(left, &plot, QCP::msTop), innerEdge(right, &plot, QCP::msTop));
    QCOMPARE(innerEdge(left, &plot, QCP::msBottom), innerEdge(right, &plot, QCP::msBottom));
}

void TestAlignmentGroup::alignmentWinsOverMarginGroup()
{
    auto* a = makePlot();
    auto* b = makePlot(false);
    auto* window = new QWidget;
    auto* rows = new QVBoxLayout(window);
    auto* firstRow = new QHBoxLayout;
    firstRow->addWidget(a);
    auto* secondRow = new QHBoxLayout;
    secondRow->addSpacing(20); // B starts 20 px right of A: equal margins do not align the edges
    secondRow->addWidget(b);
    rows->addLayout(firstRow);
    rows->addLayout(secondRow);
    QScopedPointer<QWidget> windowGuard(window);
    window->resize(420, 260);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    auto* marginGroup = new QCPMarginGroup(a);
    a->axisRect()->setMarginGroup(QCP::msLeft, marginGroup);
    b->axisRect()->setMarginGroup(QCP::msLeft, marginGroup);
    QVERIFY(settle({ a, b }));
    QVERIFY2(innerLeft(b) - innerLeft(a) == 20,
             "the margin group alone already aligned the edges, so this test proves nothing");

    const auto group = QCPAlignmentGroup::create({ a->axisRect(), b->axisRect() }, QCP::msLeft);
    QVERIFY(group);
    QVERIFY(settle({ a, b }));
    QCOMPARE(innerLeft(a), innerLeft(b));
}
