#include "test-axisrect-grid.h"

#include <vector>

#include "qcustomplot.h"

namespace
{
int gMisuses = 0;

void recordMisuse(const char*, const char*)
{
    ++gMisuses;
}
} // namespace

void TestAxisRectGrid::init()
{
    gMisuses = 0;
    qcp::setMisuseHandler(recordMisuse);
    mPlot = new QCustomPlot;
    mPlot->resize(400, 400);
    mPlot->show();
}

void TestAxisRectGrid::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
    qcp::setMisuseHandler(nullptr);
}

//! One layout pass at \a size: a shown plot's viewport follows its widget.
void TestAxisRectGrid::renderOnce(QSize size)
{
    mPlot->resize(size);
    mPlot->setViewport(mPlot->rect());
    mPlot->replot();
}

//! Replots until two rounds leave every rect's inner edges where they were: alignment has settled.
void TestAxisRectGrid::settle()
{
    const auto edges = [this]
    {
        QList<int> result;
        for (const QCPHandle<QCPAxisRect>& rect : mPlot->axisRects())
            result << rect->left() << rect->top() << rect->right() << rect->bottom();
        return result;
    };
    QList<int> previous = edges();
    for (int round = 0; round < 4; ++round)
    {
        mPlot->replot();
        const QList<int> current = edges();
        if (current == previous)
            return;
        previous = current;
    }
}

void TestAxisRectGrid::aGridKeepsTheFirstRect()
{
    QCPAxisRect* const first = mPlot->axisRect().get();
    QCPGraph* const graph = mPlot->addGraph();
    graph->setData({ 0.0, 1.0 }, { 0.0, 1.0 });
    mPlot->setTheme(QCPTheme::dark(mPlot));

    mPlot->setAxisRectGrid(2, 3);

    QCOMPARE(mPlot->axisRectCount(), 6);
    QCOMPARE(mPlot->axisRect(0, 0).get(), first);
    QCOMPARE(first->graphs().size(), 1);
    QCOMPARE(first->graphs().first().get(), graph);

    const QList<QCPHandle<QCPAxisRect>> rects = mPlot->axisRects();
    QCOMPARE(rects.size(), 6);
    QVERIFY(rects.at(4) == mPlot->axisRect(1, 1));

    const QColor foreground = mPlot->theme()->foreground();
    for (const QCPHandle<QCPAxisRect>& rect : rects)
    {
        for (QCPAxis::AxisType type :
             { QCPAxis::atBottom, QCPAxis::atLeft, QCPAxis::atTop, QCPAxis::atRight })
        {
            QCOMPARE(rect->axisCount(type), first->axisCount(type));
            QCOMPARE(rect->axis(type)->visible(), first->axis(type)->visible());
            QCOMPARE(rect->axis(type)->layer()->name(), QStringLiteral("axes"));
            QCOMPARE(rect->axis(type)->grid()->layer()->name(), QStringLiteral("grid"));
        }
        QVERIFY(rect->xAxis());
        QVERIFY(rect->yAxis());
        QCOMPARE(rect->xAxis()->tickLabelColor(), foreground);
        QCOMPARE(rect->yAxis()->tickLabelColor(), foreground);
        QCOMPARE(rect->layer()->name(), QStringLiteral("background"));
    }

    auto* const inset = new QCPAxisRect(mPlot, true);
    first->insetLayout()->addElement(inset, QRectF(0.1, 0.1, 0.3, 0.3));
    QCOMPARE(first->insetLayout()->elementCount(), 2); // the legend and the added rect
    QCOMPARE(mPlot->axisRectCount(), 7);
    QCOMPARE(mPlot->axisRects().size(), 7);
    QVERIFY(mPlot->axisRects().at(4) == mPlot->axisRect(1, 1));
    QVERIFY(mPlot->axisRects().last() == QCPHandle<QCPAxisRect>(inset));
}

void TestAxisRectGrid::stretchFactorsApply()
{
    mPlot->setAxisRectGrid(2, 1, { .rowStretch = { 1.0, 0.35 } });
    renderOnce({ 400, 600 });

    QCPAxisRect* const top = mPlot->axisRect(0, 0).get();
    QCPAxisRect* const bottom = mPlot->axisRect(1, 0).get();
    QVERIFY(qAbs(top->outerRect().height() - bottom->outerRect().height() / 0.35) <= 2);

    // A missing entry is 1 (M9), and a kept row keeps its rect.
    mPlot->setAxisRectGrid(2, 1, { .rowStretch = { 2.0 } });
    renderOnce({ 400, 600 });
    QCOMPARE(mPlot->axisRect(0, 0).get(), top);
    QVERIFY(qAbs(top->outerRect().height() - 2.0 * bottom->outerRect().height()) <= 2);

    // A list longer than the grid is misuse, and nothing changed.
    const int before = gMisuses;
    mPlot->setAxisRectGrid(1, 1, { .rowStretch = { 1.0, 2.0 } });
    QCOMPARE(gMisuses, before + 1);
    QCOMPARE(mPlot->axisRectCount(), 2);
}

void TestAxisRectGrid::aSecondCallGrowsAndShrinks()
{
    mPlot->setInteractions(QCP::iRangeDrag);
    mPlot->setAxisRectGrid(3, 1);
    const QCPHandle<QCPAxisRect> rect = mPlot->axisRect(2, 0);
    const QCPHandle<QCPGraph> graph
        = rect->add<QCPGraph>(std::vector<double> { 0.0, 1.0 }, std::vector<double> { 0.0, 1.0 });
    const QCPHandle<QCPItemText> item = rect->add<QCPItemText>();
    const QCPHandle<QCPColorMap2> map
        = rect->colorMap(std::vector<double> { 0.0, 1.0 }, std::vector<double> { 0.0, 1.0 },
                         std::vector<double> { 0.0, 1.0, 2.0, 3.0 });
    const QCPHandle<QCPColorScale> scale = rect->colorScale();
    auto* const insetRect = new QCPAxisRect(mPlot, true);
    rect->insetLayout()->addElement(insetRect, QRectF(0.1, 0.1, 0.4, 0.4));
    const QCPHandle<QCPAxisRect> inset(insetRect);
    const QCPHandle<QCPGraph> insetGraph
        = inset->add<QCPGraph>(std::vector<double> { 0.0, 1.0 }, std::vector<double> { 0.0, 1.0 });
    QVERIFY(rect);
    QVERIFY(graph);
    QVERIFY(item);
    QVERIFY(map);
    QVERIFY(scale);
    QVERIFY(inset);
    QVERIFY(insetGraph);
    QCOMPARE(mPlot->legend->itemCount(), 3); // the graph, the colour map and the inset's graph

    // The legend ends up in the rect the next call removes.
    QVERIFY(mPlot->axisRect(0, 0)->insetLayout()->take(mPlot->legend.get()));
    rect->insetLayout()->addElement(mPlot->legend.get(), Qt::AlignRight | Qt::AlignTop);

    // A drag in progress on that rect when it goes away.
    renderOnce({ 400, 600 });
    const QPoint pressed((rect->left() + rect->right()) / 2, (rect->top() + rect->bottom()) / 2);
    QTest::mousePress(mPlot, Qt::LeftButton, Qt::NoModifier, pressed);
    QTest::mouseMove(mPlot, pressed + QPoint(12, 8));

    mPlot->setAxisRectGrid(2, 1);

    QVERIFY(!rect);
    QVERIFY(!graph);
    QVERIFY(!item);
    QVERIFY(!map);
    QVERIFY(!scale);
    QVERIFY(!inset);
    QVERIFY(!insetGraph);
    QCOMPARE(mPlot->axisRectCount(), 2);
    QVERIFY(mPlot->legend);
    QCOMPARE(mPlot->legend->layout().get(), mPlot->axisRect(0, 0)->insetLayout().get());
    QCOMPARE(mPlot->legend->itemCount(), 0);

    renderOnce({ 400, 400 });
    const QPixmap exported = mPlot->toPixmap(200, 150);
    QVERIFY(!exported.isNull());
    QVERIFY(!exported.toImage().isNull());

    mPlot->setAxisRectGrid(2, 2);
    QCOMPARE(mPlot->axisRectCount(), 4);
}

void TestAxisRectGrid::aSlotDeletingDroppedContentsIsSafe()
{
    mPlot->setAxisRectGrid(2, 1);
    const QCPHandle<QCPAxisRect> dropped = mPlot->axisRect(1, 0);
    const QCPHandle<QCPAxisRect> kept = mPlot->axisRect(0, 0);
    const QCPHandle<QCPGraph> first = dropped->add<QCPGraph>(std::vector<double> { 0.0, 1.0 },
                                                             std::vector<double> { 0.0, 1.0 });
    const QCPHandle<QCPGraph> second = dropped->add<QCPGraph>(std::vector<double> { 0.0, 1.0 },
                                                              std::vector<double> { 0.0, 1.0 });
    const QCPHandle<QCPGraph> survivor
        = kept->add<QCPGraph>(std::vector<double> { 0.0, 1.0 }, std::vector<double> { 0.0, 1.0 });
    QVERIFY(first);
    QVERIFY(second);
    QVERIFY(survivor);

    // A destroyed slot runs while the dropped rect is emptied, not when the call returns: it
    // removes the next plottable of that rect and one of a kept rect.
    connect(first.get(), &QObject::destroyed, mPlot,
            [this, &second, &survivor]
            {
                if (second)
                    mPlot->removePlottable(second.get());
                if (survivor)
                    mPlot->removePlottable(survivor.get());
            });

    mPlot->setAxisRectGrid(1, 1);

    QVERIFY(!first);
    QVERIFY(!second);
    QVERIFY(!survivor);
    QCOMPARE(mPlot->axisRectCount(), 1);
    QVERIFY(mPlot->axisRect(0, 0));
}

void TestAxisRectGrid::aDeferredSlotThatDeletesThePlotIsSafe()
{
    mPlot->setAxisRectGrid(2, 1);
    int delivered = 0;
    connect(mPlot->plotLayout().get(), &QCPLayoutGrid::rowStretchFactorsChanged, mPlot,
            [this, &delivered](const QList<double>&)
            {
                if (!mPlot) // the layout's own teardown emits once more
                    return;
                ++delivered;
                QCustomPlot* const doomed = mPlot;
                mPlot = nullptr;
                delete doomed;
            });

    mPlot->setAxisRectGrid(2, 2, { .rowStretch = { 1.0, 2.0 } });

    QCOMPARE(delivered, 1);
    QVERIFY(!mPlot);
}

void TestAxisRectGrid::aDeferredSlotReenteringIsDeferred()
{
    mPlot->setAxisRectGrid(2, 1);
    int calls = 0;
    connect(mPlot->plotLayout().get(), &QCPLayoutGrid::rowStretchFactorsChanged, mPlot,
            [this, &calls](const QList<double>&)
            {
                ++calls;
                mPlot->setAxisRectGrid(1, 1);
            });

    mPlot->setAxisRectGrid(2, 2, { .rowStretch = { 1.0, 2.0 } });

    // The signal came after the walk: the grid it built is the one the slot then shrinks. A
    // delivery in the middle of the walk would be refilled by the walk's own remaining steps.
    disconnect(mPlot->plotLayout().get(), &QCPLayoutGrid::rowStretchFactorsChanged, mPlot,
               nullptr);
    QVERIFY(calls >= 1);
    QCOMPARE(mPlot->axisRectCount(), 1);
    QVERIFY(mPlot->axisRect(0, 0));
}

void TestAxisRectGrid::aHoleDoesNotMoveKeptRects()
{
    mPlot->setAxisRectGrid(2, 2);
    const QCPHandle<QCPAxisRect> topRight = mPlot->axisRect(0, 1);
    const QCPHandle<QCPAxisRect> bottomRight = mPlot->axisRect(1, 1);
    mPlot->axisRect(0, 0).remove();
    mPlot->axisRect(1, 0).remove();
    QCOMPARE(mPlot->axisRectCount(), 2);

    mPlot->setAxisRectGrid(2, 2, { .columnStretch = { 2.0, 1.0 } });

    QCOMPARE(mPlot->axisRectCount(), 4);
    QCOMPARE(mPlot->axisRect(0, 1).get(), topRight.get());
    QCOMPARE(mPlot->axisRect(1, 1).get(), bottomRight.get());
    QCOMPARE(mPlot->plotLayout()->columnStretchFactors(), (QList<double> { 2.0, 1.0 }));
}

void TestAxisRectGrid::columnsAndRowsAlign()
{
    mPlot->setAxisRectGrid(2, 2);
    // Wide tick labels on one rect of the column widen only its own left margin.
    mPlot->axisRect(0, 0)->yAxis()->setRange(0, 1e9);
    // A visible top axis on one rect of the row asks for a top margin only there.
    QCPAxis* const topAxis = mPlot->axisRect(0, 0)->axis(QCPAxis::atTop);
    topAxis->setVisible(true);
    topAxis->setTickLabels(true);
    topAxis->setLabel(QStringLiteral("top axis label"));
    renderOnce({ 400, 400 });
    settle();

    QCPAxisRect* const topLeft = mPlot->axisRect(0, 0).get();
    QCPAxisRect* const bottomLeft = mPlot->axisRect(1, 0).get();
    QCPAxisRect* const topRight = mPlot->axisRect(0, 1).get();

    QCOMPARE(topLeft->left(), bottomLeft->left());
    QCOMPARE(topLeft->top(), topRight->top());

    // Without the row group each rect keeps its own top margin: a lone rect, with no label, has
    // its data area higher up.
    QCustomPlot baseline;
    baseline.resize(400, 400);
    baseline.setViewport(baseline.rect());
    baseline.replot();
    QVERIFY(baseline.axisRect()->top() < topLeft->top());
}

void TestAxisRectGrid::misuseIsRefused()
{
    const int before = gMisuses;

    mPlot->setAxisRectGrid(0, 1);
    QCOMPARE(gMisuses, before + 1);
    QCOMPARE(mPlot->axisRectCount(), 1);

    mPlot->setAxisRectGrid(1, -1);
    QCOMPARE(gMisuses, before + 2);
    QCOMPARE(mPlot->axisRectCount(), 1);

    mPlot->setAxisRectGrid(2, 1, { .rowStretch = { 1.0, 2.0 } });
    QCOMPARE(mPlot->axisRectCount(), 2);
    const QList<double> rows = mPlot->plotLayout()->rowStretchFactors();
    const QList<double> columns = mPlot->plotLayout()->columnStretchFactors();
    int reported = gMisuses;
    for (const double bad : { 0.0, -1.0, qQNaN() })
    {
        QCPAxisRectGridOptions options;
        options.rowStretch = { bad, 1.0 };
        mPlot->setAxisRectGrid(2, 1, options);
        QCOMPARE(gMisuses, reported + 1);
        QCOMPARE(mPlot->axisRectCount(), 2);
        QCOMPARE(mPlot->plotLayout()->rowStretchFactors(), rows);
        ++reported;

        options = { };
        options.columnStretch = { bad };
        mPlot->setAxisRectGrid(2, 1, options);
        QCOMPARE(gMisuses, reported + 1);
        QCOMPARE(mPlot->axisRectCount(), 2);
        QCOMPARE(mPlot->plotLayout()->columnStretchFactors(), columns);
        ++reported;
    }

    const QCPHandle<QCPTextElement> text = mPlot->add<QCPTextElement>(0, 1);
    QVERIFY(text);
    mPlot->setAxisRectGrid(2, 1);
    QCOMPARE(gMisuses, reported + 1);
    QCOMPARE(mPlot->axisRectCount(), 2);

    QVERIFY(!mPlot->axisRect(5, 0));
    QCOMPARE(gMisuses, reported + 2);
    QCOMPARE(mPlot->axisRectCount(), 2);
}
