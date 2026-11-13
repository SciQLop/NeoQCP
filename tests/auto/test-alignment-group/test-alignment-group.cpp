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

bool holds(const QPointer<QCPAlignmentGroup>& group, const QCPLayoutElement* element)
{
    return group && qcp::pointers(group->elements()).contains(element);
}

enum class Grouping
{
    margin,
    alignment
};

//! One layout pass at \a size on a plot that was never shown, as the doc pictures render.
void renderOnce(QCustomPlot& plot, QSize size)
{
    plot.resize(size);
    plot.setViewport(plot.rect());
    plot.replot();
}

QList<QRect> layoutRects(const QList<QCPLayoutElement*>& elements)
{
    QList<QRect> rects;
    for (const QCPLayoutElement* element : elements)
        rects << element->outerRect() << element->rect();
    return rects;
}

//! The left and right edges of the outer and inner rects: what a left/right group decides.
QList<int> horizontalEdges(const QList<QCPLayoutElement*>& elements)
{
    QList<int> edges;
    for (const QCPLayoutElement* element : elements)
        edges << element->outerRect().left() << element->outerRect().right()
              << element->rect().left() << element->rect().right();
    return edges;
}

// docs/snippets/gallery.cpp, gallery_joint_density: a main rect with marginals above and right,
// the density's colour scale right of main. Returns main, top, right.
QList<QCPLayoutElement*> jointDensity(QCustomPlot& plot, Grouping grouping)
{
    QCPAxisRect* main = plot.axisRect();
    auto* density = new QCPColorMap(plot.xAxis, plot.yAxis); // its scale widens main's right
    density->data()->setSize(2, 2);
    main->colorScale()->setDataRange(QCPRange(0, 250));
    plot.xAxis->setRange(-4, 4);
    plot.yAxis->setRange(-3, 5);
    QCPLayoutGrid* layout = plot.plotLayout();
    layout->insertRow(0);
    auto* top = new QCPAxisRect(&plot);
    auto* right = new QCPAxisRect(&plot);
    layout->addElement(0, 0, top);
    layout->addElement(1, 1, right);
    layout->setRowStretchFactors({ 1, 4 });
    layout->setColumnStretchFactors({ 4, 1 });
    for (QCPAxis* axis : top->axes() + right->axes())
        axis->setTickLabels(false);
    top->setAutoMargins(QCP::msLeft | QCP::msRight | QCP::msTop);
    top->setMargins(QMargins(0, 0, 0, 6));
    right->setAutoMargins(QCP::msTop | QCP::msBottom | QCP::msRight);
    right->setMargins(QMargins(6, 0, 0, 0));
    plot.xAxis->setLabel("x");
    plot.yAxis->setLabel("y");
    if (grouping == Grouping::margin)
    {
        auto* columns = new QCPMarginGroup(&plot);
        main->setMarginGroup(QCP::msLeft | QCP::msRight, columns);
        top->setMarginGroup(QCP::msLeft | QCP::msRight, columns);
        auto* rows = new QCPMarginGroup(&plot);
        main->setMarginGroup(QCP::msTop | QCP::msBottom, rows);
        right->setMarginGroup(QCP::msTop | QCP::msBottom, rows);
    }
    else
    {
        QCPAlignmentGroup::create({ main, top }, QCP::msLeft | QCP::msRight);
        QCPAlignmentGroup::create({ main, right }, QCP::msTop | QCP::msBottom);
    }
    return { main, top, right };
}

// documentation/doc-image-generator, genQCPColorScale: a vertical scale right of the rect, a
// horizontal one below it. Returns the rect, the vertical scale, the horizontal scale.
QList<QCPLayoutElement*> colourScaleLayout(QCustomPlot& plot, Grouping grouping)
{
    QCPAxisRect* rect = plot.axisRect();
    rect->setupFullAxesBox(true);
    plot.xAxis->setTickLabels(false);
    plot.yAxis->setTickLabels(false);
    auto* vertical = new QCPColorScale(&plot);
    plot.plotLayout()->addElement(0, 1, vertical);
    vertical->setDataScaleType(QCPAxis::stLogarithmic);
    vertical->setDataRange(QCPRange(1, 1000));
    vertical->axis()->setTicker(QSharedPointer<QCPAxisTickerLog>(new QCPAxisTickerLog));
    vertical->axis()->setNumberFormat("eb");
    vertical->axis()->setNumberPrecision(0);
    auto* horizontal = new QCPColorScale(&plot);
    plot.plotLayout()->addElement(1, 0, horizontal);
    horizontal->setType(QCPAxis::atBottom);
    horizontal->setMinimumMargins(QMargins());
    horizontal->setDataRange(QCPRange(-5, 5));
    if (grouping == Grouping::margin)
    {
        auto* group = new QCPMarginGroup(&plot);
        rect->setMarginGroup(QCP::msAll, group);
        vertical->setMarginGroup(QCP::msTop | QCP::msBottom, group);
        horizontal->setMarginGroup(QCP::msLeft | QCP::msRight, group);
    }
    else
    {
        QCPAlignmentGroup::create({ rect, vertical }, QCP::msTop | QCP::msBottom);
        QCPAlignmentGroup::create({ rect, horizontal }, QCP::msLeft | QCP::msRight);
    }
    return { rect, vertical, horizontal };
}

// examples/plots, setupAdvancedAxesDemo: a wide rect above two rects in a sub layout, the right
// one fixed at 100x100. Returns the wide rect, the left and the right sub rect.
QList<QCPLayoutElement*> advancedAxes(QCustomPlot& plot, Grouping grouping)
{
    plot.plotLayout()->clear();
    auto* wide = new QCPAxisRect(&plot);
    wide->setupFullAxesBox(true);
    wide->axis(QCPAxis::atRight, 0)->setTickLabels(true);
    wide->addAxis(QCPAxis::atLeft);
    auto* subLayout = new QCPLayoutGrid;
    plot.plotLayout()->addElement(0, 0, wide);
    plot.plotLayout()->addElement(1, 0, subLayout);
    auto* subLeft = new QCPAxisRect(&plot, false);
    auto* subRight = new QCPAxisRect(&plot, false);
    subLayout->addElement(0, 0, subLeft);
    subLayout->addElement(0, 1, subRight);
    subRight->setMaximumSize(100, 100);
    subRight->setMinimumSize(100, 100);
    subLeft->addAxes(QCPAxis::atBottom | QCPAxis::atLeft);
    subRight->addAxes(QCPAxis::atBottom | QCPAxis::atRight);
    subLeft->axis(QCPAxis::atLeft)->ticker()->setTickCount(2);
    subRight->axis(QCPAxis::atRight)->ticker()->setTickCount(2);
    subRight->axis(QCPAxis::atBottom)->ticker()->setTickCount(2);
    wide->axis(QCPAxis::atLeft, 1)->setRange(0, 1e6); // the extra left axis: a wide left margin
    if (grouping == Grouping::margin)
    {
        auto* group = new QCPMarginGroup(&plot);
        subLeft->setMarginGroup(QCP::msLeft, group);
        subRight->setMarginGroup(QCP::msRight, group);
        wide->setMarginGroup(QCP::msLeft | QCP::msRight, group);
    }
    else
    {
        QCPAlignmentGroup::create({ subLeft, wide }, QCP::msLeft);
        QCPAlignmentGroup::create({ subRight, wide }, QCP::msRight);
    }
    return { wide, subLeft, subRight };
}

// tests/manual, setupMarginGroupTest: the default rect above one with two axes on every side.
// Returns the two rects.
QList<QCPLayoutElement*> stackedPair(QCustomPlot& plot, Grouping grouping)
{
    QCPLayoutGrid* layout = plot.plotLayout();
    auto* lower = new QCPAxisRect(&plot);
    layout->addElement(1, 0, lower);
    const auto everySide = QCPAxis::atLeft | QCPAxis::atRight | QCPAxis::atBottom | QCPAxis::atTop;
    lower->addAxes(everySide);
    lower->addAxes(everySide);
    QCPLayoutElement* upper = layout->element(0, 0);
    if (grouping == Grouping::margin)
    {
        auto* group = new QCPMarginGroup(&plot);
        upper->setMarginGroup(QCP::msAll, group);
        lower->setMarginGroup(QCP::msAll, group);
    }
    else
        QCPAlignmentGroup::create({ upper, lower }, QCP::msLeft | QCP::msRight);
    return { upper, lower };
}

//! An axis rect that counts its layout passes.
class PassCountingRect : public QCPAxisRect
{
public:
    using QCPAxisRect::QCPAxisRect;
    int passes = 0;

    void update(UpdatePhase phase) override
    {
        QCPAxisRect::update(phase);
        if (phase == upLayout)
            ++passes;
    }
};

//! A fixed-width rect whose left margin grows on every call: its left edge never settles.
class RestlessRect : public PassCountingRect
{
public:
    using PassCountingRect::PassCountingRect;
    int calls = 0;

protected:
    int calculateAutoMargin(QCP::MarginSide side) override
    {
        return side == QCP::msLeft ? 15 + 5 * ++calls : QCPAxisRect::calculateAutoMargin(side);
    }
};

int innerSide(const QCPLayoutElement* element, QCP::MarginSide side)
{
    const QRect rect = element->rect();
    switch (side)
    {
    case QCP::msLeft:
        return rect.left();
    case QCP::msRight:
        return rect.right();
    case QCP::msTop:
        return rect.top();
    default:
        return rect.bottom();
    }
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
    QScopedPointer<QCustomPlot> a(makePlot());
    QScopedPointer<QCustomPlot> b(makePlot());
    QScopedPointer<QCustomPlot> c(makePlot());
    const auto group = QCPAlignmentGroup::create({ a->axisRect(), b->axisRect(), c->axisRect() });
    QVERIFY(group);
    QCOMPARE(group->elements().size(), 3);
    a.reset();
    QCOMPARE(group->elements().size(), 2);
    QCOMPARE(gMisuses, 0);

    QScopedPointer<QCustomPlot> holder(makePlot());
    auto* second = new QCPAxisRect(holder.data());
    holder->plotLayout()->addElement(1, 0, second);
    group->add(holder->axisRect());
    group->add(second);
    QCOMPARE(group->elements().size(), 4);
    holder.reset();
    QCOMPARE(group->elements().size(), 2);
    QCOMPARE(gMisuses, 0);
}

void TestAlignmentGroup::lastMemberLeavingDeletesTheGroup()
{
    QScopedPointer<QCustomPlot> a(makePlot());
    QScopedPointer<QCustomPlot> b(makePlot());
    QPointer<QCPAlignmentGroup> group = QCPAlignmentGroup::create({ a->axisRect(), b->axisRect() });
    QVERIFY(group);
    group->remove(a->axisRect());
    QVERIFY(group);
    group->remove(b->axisRect());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(!group);
    a.reset();
    b.reset();
}

void TestAlignmentGroup::joiningAnotherGroupLeavesTheFirst()
{
    QScopedPointer<QCustomPlot> a(makePlot());
    QScopedPointer<QCustomPlot> b(makePlot());
    QScopedPointer<QCustomPlot> c(makePlot());
    const auto first = QCPAlignmentGroup::create({ a->axisRect(), b->axisRect() });
    const auto second = QCPAlignmentGroup::create({ c->axisRect() });
    QVERIFY(first);
    QVERIFY(second);
    second->add(a->axisRect());
    QCOMPARE(first->elements().size(), 1);
    QCOMPARE(second->elements().size(), 2);
    QVERIFY(!qcp::pointers(first->elements()).contains(a->axisRect()));
    a.reset();
    b.reset();
    c.reset();
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

void TestAlignmentGroup::anElementJoinsOneGroupPerSide()
{
    QScopedPointer<QCustomPlot> a(makePlot());
    QScopedPointer<QCustomPlot> b(makePlot());
    QScopedPointer<QCustomPlot> c(makePlot());
    const auto left = QCPAlignmentGroup::create({ a->axisRect(), b->axisRect() }, QCP::msLeft);
    const auto right = QCPAlignmentGroup::create({ a->axisRect(), c->axisRect() }, QCP::msRight);
    QVERIFY(holds(left, a->axisRect()));
    QVERIFY(holds(right, a->axisRect()));
    QCOMPARE(left->elements().size(), 2);
    QCOMPARE(right->elements().size(), 2);
    QCOMPARE(gMisuses, 0);
}

void TestAlignmentGroup::joiningForSomeSidesKeepsTheOthers()
{
    QScopedPointer<QCustomPlot> a(makePlot());
    QScopedPointer<QCustomPlot> b(makePlot());
    QScopedPointer<QCustomPlot> c(makePlot());
    const auto both = QCPAlignmentGroup::create({ a->axisRect(), b->axisRect() },
                                                QCP::msLeft | QCP::msRight);
    const auto right = QCPAlignmentGroup::create({ a->axisRect(), c->axisRect() }, QCP::msRight);
    QVERIFY(holds(both, a->axisRect())); // still there for its left side
    QVERIFY(holds(right, a->axisRect()));
    QCOMPARE(a->axisRect()->alignmentGroup(QCP::msLeft), both);
    QCOMPARE(a->axisRect()->alignmentGroup(QCP::msRight), right);
    QCOMPARE(b->axisRect()->alignmentGroup(QCP::msRight), both);

    // Joining for every side it holds leaves the group altogether.
    const auto other = QCPAlignmentGroup::create({ a->axisRect() }, QCP::msLeft);
    QVERIFY(!holds(both, a->axisRect()));
    QCOMPARE(both->elements().size(), 1);
    QCOMPARE(a->axisRect()->alignmentGroup(QCP::msLeft), other);
    QCOMPARE(a->axisRect()->alignmentGroup(QCP::msRight), right);

    // Joining again takes every side back, from both groups that held them meanwhile.
    both->add(a->axisRect());
    QCOMPARE(a->axisRect()->alignmentGroup(QCP::msLeft), both);
    QCOMPARE(a->axisRect()->alignmentGroup(QCP::msRight), both);
    QCOMPARE(qcp::pointers(right->elements()), QList<QCPLayoutElement*>({ c->axisRect() }));
    QVERIFY(!holds(other, a->axisRect()));
    QSignalSpy changes(both.data(), &QCPAlignmentGroup::membersChanged);
    both->add(a->axisRect()); // already holds every side: nothing changes
    QCOMPARE(changes.count(), 0);
    QCOMPARE(gMisuses, 0);
}

void TestAlignmentGroup::leavingOneGroupKeepsTheOther()
{
    QScopedPointer<QCustomPlot> a(makePlot());
    QScopedPointer<QCustomPlot> b(makePlot());
    QScopedPointer<QCustomPlot> c(makePlot());
    const auto left = QCPAlignmentGroup::create({ a->axisRect(), b->axisRect() }, QCP::msLeft);
    const auto right = QCPAlignmentGroup::create({ a->axisRect(), c->axisRect() }, QCP::msRight);
    left->remove(a->axisRect());
    QVERIFY(!holds(left, a->axisRect()));
    QVERIFY(holds(right, a->axisRect()));
    QVERIFY(!a->axisRect()->alignmentGroup(QCP::msLeft));
    QCOMPARE(a->axisRect()->alignmentGroup(QCP::msRight), right);
}

void TestAlignmentGroup::aDeletedGroupLeavesTheOtherSides()
{
    QScopedPointer<QCustomPlot> a(makePlot());
    QScopedPointer<QCustomPlot> b(makePlot());
    QScopedPointer<QCustomPlot> c(makePlot());
    QPointer<QCPAlignmentGroup> left
        = QCPAlignmentGroup::create({ a->axisRect(), b->axisRect() }, QCP::msLeft);
    const auto right = QCPAlignmentGroup::create({ a->axisRect(), c->axisRect() }, QCP::msRight);
    delete left.data();
    QVERIFY(!a->axisRect()->alignmentGroup(QCP::msLeft));
    QCOMPARE(a->axisRect()->alignmentGroup(QCP::msRight), right);
    a->replot(); // the deleted group's side lays out on its own
    b->replot();
    QVERIFY(holds(right, a->axisRect()));
    a.reset();
    QCOMPARE(right->elements().size(), 1);
}

void TestAlignmentGroup::aDeletedElementLeavesEveryGroup()
{
    QScopedPointer<QCustomPlot> a(makePlot());
    QScopedPointer<QCustomPlot> b(makePlot());
    auto* lower = new QCPAxisRect(a.data());
    a->plotLayout()->addElement(1, 0, lower);
    QPointer<QCPAlignmentGroup> left
        = QCPAlignmentGroup::create({ lower, b->axisRect() }, QCP::msLeft);
    QPointer<QCPAlignmentGroup> right = QCPAlignmentGroup::create({ lower }, QCP::msRight);
    delete lower;
    QCOMPARE(left->elements().size(), 1);
    QCOMPARE(right->elements().size(), 0);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(left);
    QVERIFY(!right); // its last member left
    QCOMPARE(gMisuses, 0);
}

void TestAlignmentGroup::aBadSideHasNoGroup()
{
    QScopedPointer<QCustomPlot> a(makePlot());
    const auto group = QCPAlignmentGroup::create({ a->axisRect() }, QCP::msAll);
    QCOMPARE(a->axisRect()->alignmentGroup(QCP::msBottom), group);
    QVERIFY(!a->axisRect()->alignmentGroup(QCP::msAll));
    QCOMPARE(gMisuses, 1);
    QVERIFY(!a->axisRect()->alignmentGroup(QCP::msNone));
    QCOMPARE(gMisuses, 2);
    // A group with no side would hold members no side points to: they could never leave it.
    QVERIFY(!QCPAlignmentGroup::create({ a->axisRect() }, QCP::msNone));
    QCOMPARE(gMisuses, 3);
    QCOMPARE(a->axisRect()->alignmentGroup(QCP::msLeft), group);
}

void TestAlignmentGroup::jointDensityAlignsAsItsMarginGroupsDid()
{
    const QSize size(640, 500);
    QCustomPlot withMargins;
    const auto marginElements = jointDensity(withMargins, Grouping::margin);
    renderOnce(withMargins, size);
    // The margin group needs a second render here: the top marginal reads main's right margin
    // before main has sized its colour scale, so one render leaves top wider by the scale.
    withMargins.replot();
    QCustomPlot aligned;
    const auto elements = jointDensity(aligned, Grouping::alignment);
    renderOnce(aligned, size);

    const QCPLayoutElement* main = elements[0];
    QCOMPARE(innerSide(elements[1], QCP::msLeft), innerSide(main, QCP::msLeft));
    QCOMPARE(innerSide(elements[1], QCP::msRight), innerSide(main, QCP::msRight));
    QCOMPARE(innerSide(elements[2], QCP::msTop), innerSide(main, QCP::msTop));
    QCOMPARE(innerSide(elements[2], QCP::msBottom), innerSide(main, QCP::msBottom));
    QCOMPARE(layoutRects(elements), layoutRects(marginElements));
}

void TestAlignmentGroup::colourScaleLayoutAlignsAsItsMarginGroupDid()
{
    const QSize size(450, 200);
    QCustomPlot withMargins;
    const auto marginElements = colourScaleLayout(withMargins, Grouping::margin);
    renderOnce(withMargins, size);
    QCustomPlot aligned;
    const auto elements = colourScaleLayout(aligned, Grouping::alignment);
    renderOnce(aligned, size);

    const QCPLayoutElement* rect = elements[0];
    QCOMPARE(innerSide(elements[1], QCP::msTop), innerSide(rect, QCP::msTop));
    QCOMPARE(innerSide(elements[1], QCP::msBottom), innerSide(rect, QCP::msBottom));
    QCOMPARE(innerSide(elements[2], QCP::msLeft), innerSide(rect, QCP::msLeft));
    QCOMPARE(innerSide(elements[2], QCP::msRight), innerSide(rect, QCP::msRight));
    QCOMPARE(layoutRects(elements), layoutRects(marginElements));
}

void TestAlignmentGroup::advancedAxesAlignAsTheirMarginGroupDid()
{
    const QSize size(700, 500);
    QCustomPlot withMargins;
    const auto marginElements = advancedAxes(withMargins, Grouping::margin);
    renderOnce(withMargins, size);
    QCustomPlot aligned;
    const auto elements = advancedAxes(aligned, Grouping::alignment);
    renderOnce(aligned, size);

    const QCPLayoutElement* wide = elements[0];
    QCOMPARE(innerSide(elements[1], QCP::msLeft), innerSide(wide, QCP::msLeft));
    QCOMPARE(innerSide(elements[2], QCP::msRight), innerSide(wide, QCP::msRight));
    QVERIFY2(innerSide(elements[1], QCP::msRight) < innerSide(elements[2], QCP::msLeft),
             "the left sub rect's right edge was pulled to the shared right edge");
    QCOMPARE(layoutRects(elements), layoutRects(marginElements));
}

/* A margin group on msAll also made the two rows' top and bottom margins equal. That is not an
   edge alignment (the rows' top edges are a row apart) and has no alignment-group form: the port
   aligns the left and right sides, and only those are compared. */
void TestAlignmentGroup::stackedPairAlignsSidewaysAsItsMarginGroupDid()
{
    const QSize size(500, 400);
    QCustomPlot withMargins;
    const auto marginElements = stackedPair(withMargins, Grouping::margin);
    renderOnce(withMargins, size);
    QCustomPlot aligned;
    const auto elements = stackedPair(aligned, Grouping::alignment);
    renderOnce(aligned, size);

    QCOMPARE(innerSide(elements[0], QCP::msLeft), innerSide(elements[1], QCP::msLeft));
    QCOMPARE(innerSide(elements[0], QCP::msRight), innerSide(elements[1], QCP::msRight));
    QCOMPARE(horizontalEdges(elements), horizontalEdges(marginElements));
}

void TestAlignmentGroup::aHiddenMemberRepaintsNoVisiblePeer()
{
    auto* a = makePlot();
    auto* b = makePlot(false);
    auto* c = makePlot(false);
    QScopedPointer<QWidget> window(makeFreeStack({ a, b, c }, 24));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    const auto group = QCPAlignmentGroup::create({ a->axisRect(), b->axisRect(), c->axisRect() });
    QVERIFY(group);
    QVERIFY(settle({ a, b, c }));
    c->hide();
    QTest::qWait(150);
    QVERIFY(settle({ a, b }));

    int peerRepaints = 0;
    const auto counted
        = connect(b, &QCustomPlot::afterReplot, b, [&peerRepaints] { ++peerRepaints; });
    c->replot(); // a hidden plot still replots unless told to skip
    a->replot();
    QTest::qWait(200);
    disconnect(counted);
    QCOMPARE(peerRepaints, 0);
    QCOMPARE(innerLeft(a), innerLeft(b));
}

void TestAlignmentGroup::oneRenderTakesOneExtraPassThenNone()
{
    QCustomPlot plot;
    auto* lower = new PassCountingRect(&plot);
    plot.plotLayout()->addElement(1, 0, lower);
    lower->axis(QCPAxis::atLeft)->setRange(0, 1e9);
    QVERIFY(QCPAlignmentGroup::create({ plot.axisRect(), lower }));
    renderOnce(plot, QSize(400, 300));
    QCOMPARE(lower->passes, 2); // the first pass has no edges to align to
    QCOMPARE(innerSide(plot.axisRect(), QCP::msLeft), innerSide(lower, QCP::msLeft));
    lower->passes = 0;
    plot.replot();
    QCOMPARE(lower->passes, 1);
}

void TestAlignmentGroup::anUnsettledLayoutStopsAndRepaintsLater()
{
    QCustomPlot plot;
    auto* restless = new RestlessRect(&plot);
    plot.plotLayout()->addElement(0, 1, restless);
    restless->setMinimumSize(100, 50); // a fixed inner width: its margin moves its outer left edge
    restless->setMaximumSize(100, QWIDGETSIZE_MAX);
    QVERIFY(QCPAlignmentGroup::create({ restless }, QCP::msLeft));
    renderOnce(plot, QSize(400, 300));
    QCOMPARE(restless->passes, 4); // the pass, then the three extra ones
    QVERIFY(plot.isRepaintQueued()); // the next replot carries on
}
