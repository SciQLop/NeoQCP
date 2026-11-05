#include "test-qcpbars.h"
#include "painting/line-extruder.h"
#include <QElapsedTimer>
#include <QMainWindow>
#include <QtWidgets/qtestsupport_widgets.h>
#include <functional>

namespace
{
//! A bar chart that says how many times it recorded.
class CountingBars : public QCPBars
{
public:
    using QCPBars::QCPBars;

    int recordings() const { return mCore.recordingCount(); }
};

//! A plot with a real QRhi under it, or none: only the GPU replay extrudes bar outlines.
QCustomPlot* shownPlot(QWidget& host)
{
    host.resize(400, 300);
    auto* plot = new QCustomPlot(&host);
    plot->setGeometry(0, 0, 400, 300);
    host.show();
    if (!QTest::qWaitForWindowExposed(&host))
        return nullptr;
    QElapsedTimer waited;
    waited.start();
    while (!plot->rhi() && waited.elapsed() < 5000)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QTest::qWait(10);
    }
    return plot->rhi() ? plot : nullptr;
}

QVector<double> flatValues(int count)
{
    return QVector<double>(count, 0.5);
}

QVector<double> spreadKeys(int count)
{
    QVector<double> keys;
    keys.reserve(count);
    for (int i = 0; i < count; ++i)
        keys << 10.0 * i / count;
    return keys;
}

//! The plot as the user sees it, without the grid lines a probe pixel could land on.
QImage frameOf(QCustomPlot* plot)
{
    plot->xAxis->grid()->setVisible(false);
    plot->yAxis->grid()->setVisible(false);
    plot->resize(300, 200);
    return plot->toPixmap(300, 200).toImage();
}

QColor pixelAt(const QImage& frame, QCustomPlot* plot, double key, double value)
{
    const double dpr = frame.devicePixelRatio();
    return frame.pixelColor(qRound(plot->xAxis->coordToPixel(key) * dpr),
                            qRound(plot->yAxis->coordToPixel(value) * dpr));
}

//! The left-most inked column inside \a plot's axis rect, on the row of \a value, in device pixels.
int barLeftEdge(const QImage& frame, QCustomPlot* plot, double value)
{
    const double dpr = frame.devicePixelRatio();
    const QRect rect = plot->axisRect()->rect();
    const int row = qRound(plot->yAxis->coordToPixel(value) * dpr);
    for (int x = qRound(rect.left() * dpr) + 4; x <= qRound(rect.right() * dpr) - 4; ++x)
        if (frame.pixelColor(x, row) != QColor(Qt::white))
            return x;
    return -1;
}

//! Pans the key axis by \a keys and reports by how much more than that the bar's left edge moved.
//! -1 when an edge was not found.
int leftEdgeShift(QCustomPlot* plot, const std::function<QImage()>& frame, double rowValue,
                  double keys)
{
    const QImage before = frame();
    const int left = barLeftEdge(before, plot, rowValue);
    const double moved
        = (plot->xAxis->coordToPixel(0) - plot->xAxis->coordToPixel(keys)) * before.devicePixelRatio();
    const QCPRange range = plot->xAxis->range();
    plot->xAxis->setRange(range.lower + keys, range.upper + keys);
    plot->replot();
    for (int i = 0; i < 100 && plot->isRepaintQueued(); ++i)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    const int movedLeft = barLeftEdge(frame(), plot, rowValue);
    if (left < 0 || movedLeft < 0)
        return -1;
    return qRound((movedLeft - left) - moved);
}

//! Two bars side by side in one group, both with a width in plot coordinates.
void groupedPair(QCustomPlot* plot, QCPBars*& first, QCPBars*& second)
{
    first = new QCPBars(plot->xAxis, plot->yAxis);
    second = new QCPBars(plot->xAxis, plot->yAxis);
    auto* group = new QCPBarsGroup(plot); // default: an absolute spacing in pixels
    group->append(first);
    group->append(second);
}

//! Two bars on one key range, the second stacked on the first.
void stackedPair(QCustomPlot* plot, QCPBars*& below, QCPBars*& above)
{
    below = new QCPBars(plot->xAxis, plot->yAxis);
    above = new QCPBars(plot->xAxis, plot->yAxis);
    above->moveAbove(below);
}

//! A bar on \a plot at keys {2, 3}, painted \a colour without an outline.
QCPBars* colouredBars(QCustomPlot* plot, const QColor& colour, double value = 1.0)
{
    auto* bars = new QCPBars(plot->xAxis, plot->yAxis);
    bars->setWidth(0.6);
    bars->setPen(Qt::NoPen);
    bars->setBrush(colour);
    bars->setData(QVector<double> { 2, 3 }, QVector<double> { value, value });
    return bars;
}

//! The left-most pixel of \a colour inside \a plot's axis rect on the row of \a value.
int leftEdgeOf(const QImage& frame, QCustomPlot* plot, double value, const QColor& colour)
{
    const double dpr = frame.devicePixelRatio();
    const QRect rect = plot->axisRect()->rect();
    const int row = qRound(plot->yAxis->coordToPixel(value) * dpr);
    for (int x = qRound(rect.left() * dpr) + 4; x <= qRound(rect.right() * dpr) - 4; ++x)
        if (frame.pixelColor(x, row) == colour)
            return x;
    return -1;
}

//! The bottom-most pixel of \a colour down the column of \a key, in device pixels.
int lowestEdgeOf(const QImage& frame, QCustomPlot* plot, double key, const QColor& colour)
{
    const double dpr = frame.devicePixelRatio();
    const QRect rect = plot->axisRect()->rect();
    const int x = qRound(plot->xAxis->coordToPixel(key) * dpr);
    for (int y = qRound(rect.bottom() * dpr) - 4; y >= qRound(rect.top() * dpr) + 4; --y)
        if (frame.pixelColor(x, y) == colour)
            return y;
    return -1;
}
} // namespace

void TestQCPBars::init()
{
  mPlot = new QCustomPlot(0);
  mBars = new QCPBars(mPlot->xAxis, mPlot->yAxis);
}

void TestQCPBars::cleanup()
{
  delete mPlot;
}

void TestQCPBars::dataManipulation()
{
  QVERIFY(mBars->data()->isEmpty());
  
  QVector<double> x, y;
  x << -1 << 1 << -2 << 2;
  y <<  1 << 2 <<  0 << 3;
  
  mBars->setData(x, y);
  QCOMPARE(mBars->data()->size(), 4);
  // data should be sorted by x:
  QCOMPARE((mBars->data()->constBegin()+0)->value, 0.0);
  QCOMPARE((mBars->data()->constBegin()+1)->value, 1.0);
  QCOMPARE((mBars->data()->constBegin()+2)->value, 2.0);
  QCOMPARE((mBars->data()->constBegin()+3)->value, 3.0);
  QCOMPARE((mBars->data()->constBegin()+0)->key, -2.0);
  QCOMPARE((mBars->data()->constBegin()+1)->key, -1.0);
  QCOMPARE((mBars->data()->constBegin()+2)->key, 1.0);
  QCOMPARE((mBars->data()->constBegin()+3)->key, 2.0);
  // map key should be identical to data key:
  for (int i=0; i<4; ++i)
    QCOMPARE((mBars->data()->constBegin()+i)->key, (mBars->data()->constBegin()+i)->key);
  
  // data removal:
  mBars->data()->removeBefore(0);
  QCOMPARE(mBars->data()->size(), 2);
  QCOMPARE((mBars->data()->constBegin()+0)->key, 1.0);
  QCOMPARE((mBars->data()->constBegin()+1)->key, 2.0);
  
  mBars->setData(x, y);
  mBars->data()->removeAfter(0);
  QCOMPARE(mBars->data()->size(), 2);
  QCOMPARE((mBars->data()->constBegin()+0)->key, -2.0);
  QCOMPARE((mBars->data()->constBegin()+1)->key, -1.0);
  
  mBars->setData(x, y);
  mBars->data()->remove(-1.1, -0.9);
  QCOMPARE(mBars->data()->size(), 3);
  QCOMPARE((mBars->data()->constBegin()+0)->key, -2.0);
  QCOMPARE((mBars->data()->constBegin()+1)->key, 1.0);
  QCOMPARE((mBars->data()->constBegin()+2)->key, 2.0);
  
  mBars->setData(x, y);
  mBars->data()->remove(-2.1, -1.9);
  QCOMPARE(mBars->data()->size(), 3);
  QCOMPARE((mBars->data()->constBegin()+0)->key, -1.0);
  QCOMPARE((mBars->data()->constBegin()+1)->key, 1.0);
  QCOMPARE((mBars->data()->constBegin()+2)->key, 2.0);
  
  mBars->setData(x, y);
  mBars->data()->remove(1.9, 2.1);
  QCOMPARE(mBars->data()->size(), 3);
  QCOMPARE((mBars->data()->constBegin()+0)->key, -2.0);
  QCOMPARE((mBars->data()->constBegin()+1)->key, -1.0);
  QCOMPARE((mBars->data()->constBegin()+2)->key, 1.0);
  
  mBars->setData(x, y);
  mBars->data()->remove(-1.1, 1.1);
  QCOMPARE(mBars->data()->size(), 2);
  QCOMPARE((mBars->data()->constBegin()+0)->key, -2.0);
  QCOMPARE((mBars->data()->constBegin()+1)->key, 2.0);
  
  mBars->setData(x, y);
  mBars->data()->clear();
  QCOMPARE(mBars->data()->size(), 0);
  
  // add single data points:
  mBars->addData(5, 6);
  QCOMPARE(mBars->data()->size(), 1);
  mBars->addData(4, 5);
  QCOMPARE(mBars->data()->size(), 2);
  QCOMPARE((mBars->data()->constBegin()+0)->value, 5.0);
  QCOMPARE((mBars->data()->constBegin()+1)->value, 6.0);
  
  mBars->setData(x, y);
  mBars->addData(3, 4);
  QCOMPARE(mBars->data()->size(), 5);
  QCOMPARE((mBars->data()->constBegin()+0)->value, 0.0);
  QCOMPARE((mBars->data()->constBegin()+1)->value, 1.0);
  QCOMPARE((mBars->data()->constBegin()+2)->value, 2.0);
  QCOMPARE((mBars->data()->constBegin()+3)->value, 3.0);
  QCOMPARE((mBars->data()->constBegin()+4)->value, 4.0);
  
  mBars->setData(x, y);
  mBars->addData(QVector<double>() << 3 << 4 << 5, QVector<double>() << 4 << 5 << 6);
  QCOMPARE(mBars->data()->size(), 7);
  QCOMPARE((mBars->data()->constBegin()+0)->value, 0.0);
  QCOMPARE((mBars->data()->constBegin()+1)->value, 1.0);
  QCOMPARE((mBars->data()->constBegin()+2)->value, 2.0);
  QCOMPARE((mBars->data()->constBegin()+3)->value, 3.0);
  QCOMPARE((mBars->data()->constBegin()+4)->value, 4.0);
  QCOMPARE((mBars->data()->constBegin()+5)->value, 5.0);
  QCOMPARE((mBars->data()->constBegin()+6)->value, 6.0);
}

void TestQCPBars::dataSharing()
{
  QCPBars *bars2 = new QCPBars(mPlot->xAxis, mPlot->yAxis);
  
  QSharedPointer<QCPBarsDataContainer> data = bars2->data();
  mBars->setData(data); // now shares data with bars2
  
  QVector<double> x, y;
  x << -1 << 1 << -2 << 2;
  y <<  1 << 2 <<  0 << 3;
  bars2->setData(x, y);
  
  QCOMPARE(mBars->data()->size(), 4);
  QCOMPARE(bars2->data()->size(), 4);
  data->add(QCPBarsData(4, 5)); // modify from outside
  QCOMPARE(mBars->data()->size(), 5);
  QCOMPARE(bars2->data()->size(), 5);
  data.clear(); // clear external reference, internals should live on
  QVERIFY(!data);
  QCOMPARE(mBars->data()->size(), 5);
  QCOMPARE(bars2->data()->size(), 5);
  
  // decouple data again:
  mBars->setData(QSharedPointer<QCPBarsDataContainer>(new QCPBarsDataContainer));
  mBars->addData(1, 1);
  bars2->addData(4, 5);
  QCOMPARE(mBars->data()->size(), 1);
  QCOMPARE(bars2->data()->size(), 6);
}

void TestQCPBars::setDataTwiceRedrawsTheNewBars()
{
    // setData(keys, values) rewrites the container without marking the bar dirty itself: the
    // screen must still show the new heights after the next replot.
    mBars->setWidth(0.6);
    mPlot->xAxis->setRange(0, 4);
    mPlot->yAxis->setRange(0, 4);
    mBars->setData(QVector<double> { 1, 2, 3 }, QVector<double> { 0.5, 0.5, 0.5 });
    mPlot->replot();
    QCOMPARE(pixelAt(frameOf(mPlot), mPlot, 2.0, 2.0), QColor(Qt::white));

    mBars->setData(QVector<double> { 1, 2, 3 }, QVector<double> { 3, 3, 3 });
    mPlot->replot();
    const QColor raised = pixelAt(frameOf(mPlot), mPlot, 2.0, 2.0);
    QVERIFY2(raised != QColor(Qt::white),
             qPrintable(QStringLiteral("the raised bar is still painted as %1").arg(raised.name())));
}

void TestQCPBars::addDataShowsTheNewBar()
{
    mBars->setWidth(0.6);
    mPlot->xAxis->setRange(0, 5);
    mPlot->yAxis->setRange(0, 4);
    mBars->setData(QVector<double> { 1, 2 }, QVector<double> { 1, 1 });
    mPlot->replot();
    QCOMPARE(pixelAt(frameOf(mPlot), mPlot, 4.0, 1.5), QColor(Qt::white));

    mBars->addData(4, 3);
    mPlot->replot();
    const QColor added = pixelAt(frameOf(mPlot), mPlot, 4.0, 1.5);
    QVERIFY2(added != QColor(Qt::white),
             qPrintable(QStringLiteral("the added bar is painted as %1").arg(added.name())));
}

void TestQCPBars::containerEditRedraws()
{
    // The container API is public: bars->data()->add() changes the data behind the recording.
    mBars->setWidth(0.6);
    mPlot->xAxis->setRange(0, 5);
    mPlot->yAxis->setRange(0, 4);
    mBars->setData(QVector<double> { 1 }, QVector<double> { 1 });
    mPlot->replot();
    QCOMPARE(pixelAt(frameOf(mPlot), mPlot, 3.0, 1.5), QColor(Qt::white));

    mBars->data()->add(QCPBarsData(3, 3));
    mPlot->replot();
    const QColor edited = pixelAt(frameOf(mPlot), mPlot, 3.0, 1.5);
    QVERIFY2(edited != QColor(Qt::white),
             qPrintable(QStringLiteral("the container edit is painted as %1").arg(edited.name())));
}

void TestQCPBars::groupedBarsFollowAPan()
{
    // A grouped bar's position is in pixels: a pan moves it, so the recording must be remade.
    QCPBars* first = nullptr;
    QCPBars* second = nullptr;
    groupedPair(mPlot, first, second);
    for (QCPBars* bars : { first, second })
    {
        bars->setWidth(0.6);
        bars->setData(QVector<double> { 2, 3 }, QVector<double> { 1, 1 });
    }
    mPlot->xAxis->setRange(0, 4);
    mPlot->yAxis->setRange(0, 4);
    mPlot->replot();
    QVERIFY2(barLeftEdge(frameOf(mPlot), mPlot, 0.5) > 0, "the grouped bar must be painted");

    const int shift = leftEdgeShift(mPlot, [this] { return frameOf(mPlot); }, 0.5, 1.0);
    QVERIFY2(qAbs(shift) <= 1,
             qPrintable(QStringLiteral("the grouped bar moved %1 px off the axis").arg(shift)));
}

void TestQCPBars::stackedBarsFollowAPan()
{
    // A stacked bar's own rect is in pixels too (stacking gap, outline): a pan moves it.
    QCPBars* below = nullptr;
    QCPBars* above = nullptr;
    stackedPair(mPlot, below, above);
    for (QCPBars* bars : { below, above })
    {
        bars->setWidth(0.6);
        bars->setData(QVector<double> { 2, 3 }, QVector<double> { 1, 1 });
    }
    mPlot->xAxis->setRange(0, 4);
    mPlot->yAxis->setRange(0, 4);
    mPlot->replot();
    QVERIFY2(barLeftEdge(frameOf(mPlot), mPlot, 1.5) > 0, "the stacked bar must be painted");

    const int shift = leftEdgeShift(mPlot, [this] { return frameOf(mPlot); }, 1.5, 1.0);
    QVERIFY2(qAbs(shift) <= 1,
             qPrintable(QStringLiteral("the stacked bar moved %1 px off the axis").arg(shift)));
}

void TestQCPBars::groupedBarsFollowAPanOnGpu()
{
    QWidget host;
    QCustomPlot* plot = shownPlot(host);
    if (!plot)
        QSKIP("no QRhi on this platform");
    QCPBars* first = nullptr;
    QCPBars* second = nullptr;
    groupedPair(plot, first, second);
    for (QCPBars* bars : { first, second })
    {
        bars->setWidth(0.6);
        bars->setData(QVector<double> { 2, 3 }, QVector<double> { 1, 1 });
    }
    plot->xAxis->grid()->setVisible(false);
    plot->yAxis->grid()->setVisible(false);
    plot->xAxis->setRange(0, 4);
    plot->yAxis->setRange(0, 4);
    plot->replot();
    QTRY_VERIFY(!plot->isRepaintQueued());
    const QImage before = plot->grabFramebuffer();
    QVERIFY2(barLeftEdge(before, plot, 0.5) > 0, "the grouped bar must be painted");

    const int shift = leftEdgeShift(plot, [plot] { return plot->grabFramebuffer(); }, 0.5, 1.0);
    QVERIFY2(qAbs(shift) <= 1,
             qPrintable(QStringLiteral("the grouped bar moved %1 px off the axis").arg(shift)));
}

void TestQCPBars::stackedBarsFollowAPanOnGpu()
{
    QWidget host;
    QCustomPlot* plot = shownPlot(host);
    if (!plot)
        QSKIP("no QRhi on this platform");
    QCPBars* below = nullptr;
    QCPBars* above = nullptr;
    stackedPair(plot, below, above);
    for (QCPBars* bars : { below, above })
    {
        bars->setWidth(0.6);
        bars->setData(QVector<double> { 2, 3 }, QVector<double> { 1, 1 });
    }
    plot->xAxis->grid()->setVisible(false);
    plot->yAxis->grid()->setVisible(false);
    plot->xAxis->setRange(0, 4);
    plot->yAxis->setRange(0, 4);
    plot->replot();
    QTRY_VERIFY(!plot->isRepaintQueued());
    const QImage before = plot->grabFramebuffer();
    QVERIFY2(barLeftEdge(before, plot, 1.5) > 0, "the stacked bar must be painted");

    const int shift = leftEdgeShift(plot, [plot] { return plot->grabFramebuffer(); }, 1.5, 1.0);
    QVERIFY2(qAbs(shift) <= 1,
             qPrintable(QStringLiteral("the stacked bar moved %1 px off the axis").arg(shift)));
}

void TestQCPBars::stackedBarFollowsTheBarBelow()
{
    // A stacked bar's base is the bar below it at record time: when that bar grows, the base moves.
    auto* below = colouredBars(mPlot, Qt::red, 1.0);
    auto* above = colouredBars(mPlot, Qt::green, 1.0);
    above->moveAbove(below);
    mPlot->xAxis->setRange(0, 4);
    mPlot->yAxis->setRange(0, 4);
    mPlot->replot();
    const int base = lowestEdgeOf(frameOf(mPlot), mPlot, 2.0, QColor(Qt::green));
    QVERIFY2(base > 0, "the upper bar must be painted");

    below->setData(QVector<double> { 2, 3 }, QVector<double> { 2, 2 }); // it grew under the upper bar
    mPlot->replot();
    const int raised = lowestEdgeOf(frameOf(mPlot), mPlot, 2.0, QColor(Qt::green));
    QVERIFY2(raised >= 0, "the upper bar must still be painted");
    QVERIFY2(raised < base - 1,
             qPrintable(QStringLiteral("the upper bar's base stayed at %1 px").arg(base)));
}

void TestQCPBars::moveAboveRestacks()
{
    // Moving a bar above its neighbour restacks both: the one that lost its base must redraw too.
    auto* first = colouredBars(mPlot, Qt::red, 3.0);
    auto* second = colouredBars(mPlot, Qt::green, 1.0);
    second->moveAbove(first);
    mPlot->xAxis->setRange(0, 4);
    mPlot->yAxis->setRange(0, 4);
    mPlot->replot();
    QCOMPARE(pixelAt(frameOf(mPlot), mPlot, 2.0, 3.5), QColor(Qt::green));

    first->moveAbove(second);
    mPlot->replot();
    QCOMPARE(pixelAt(frameOf(mPlot), mPlot, 2.0, 3.5), QColor(Qt::red));
}

void TestQCPBars::groupJoinShiftsTheOthers()
{
    // A bar joining a group re-centres the others: the plot that gained it must render like one
    // that had the member from the start.
    QCustomPlot joined(0);
    QCustomPlot reference(0);
    for (QCustomPlot* plot : { &joined, &reference })
    {
        plot->resize(300, 200); // the frame of the assertion: a resize would re-record everything
        plot->xAxis->setRange(0, 4);
        plot->yAxis->setRange(0, 4);
    }
    auto* group = new QCPBarsGroup(&joined);
    group->append(colouredBars(&joined, Qt::red));
    group->append(colouredBars(&joined, Qt::green));
    auto* referenceGroup = new QCPBarsGroup(&reference);
    referenceGroup->append(colouredBars(&reference, Qt::red));
    referenceGroup->append(colouredBars(&reference, Qt::green));
    referenceGroup->append(colouredBars(&reference, Qt::blue));
    joined.replot(); // the two-bar group is on screen, and recorded
    reference.replot();

    group->append(colouredBars(&joined, Qt::blue)); // a third member re-centres red and green
    joined.replot();
    QCOMPARE(leftEdgeOf(frameOf(&joined), &joined, 0.5, QColor(Qt::red)),
             leftEdgeOf(frameOf(&reference), &reference, 0.5, QColor(Qt::red)));
}

void TestQCPBars::groupedBarsOnALogAxis()
{
    // A group spacing in plot coordinates is a coordinate offset on a linear axis only: switching
    // the axis to logarithmic must re-record the bars on their new pixels.
    QCustomPlot changed(0);
    QCustomPlot reference(0);
    for (QCustomPlot* plot : { &changed, &reference })
    {
        plot->resize(300, 200); // the frame of the assertion: a resize would re-record everything
        plot->xAxis->setRange(0.5, 200);
        plot->yAxis->setRange(0, 4);
    }
    auto* group = new QCPBarsGroup(&changed);
    group->setSpacingType(QCPBarsGroup::stPlotCoords);
    group->setSpacing(0.1);
    group->append(colouredBars(&changed, Qt::red));
    group->append(colouredBars(&changed, Qt::green));
    auto* referenceGroup = new QCPBarsGroup(&reference);
    referenceGroup->setSpacingType(QCPBarsGroup::stPlotCoords);
    referenceGroup->setSpacing(0.1);
    referenceGroup->append(colouredBars(&reference, Qt::red));
    referenceGroup->append(colouredBars(&reference, Qt::green));
    changed.replot(); // first frame on a linear key axis
    reference.xAxis->setScaleType(QCPAxis::stLogarithmic);
    reference.replot();

    changed.xAxis->setScaleType(QCPAxis::stLogarithmic); // the axis the reference always had
    changed.replot();
    QCOMPARE(leftEdgeOf(frameOf(&changed), &changed, 0.5, QColor(Qt::red)),
             leftEdgeOf(frameOf(&reference), &reference, 0.5, QColor(Qt::red)));
}

void TestQCPBars::plotWidthBarsNeverReRecord()
{
    // A width in plot coordinates is a data position: a pan and a zoom both reuse the recording.
    auto* bars = new CountingBars(mPlot->xAxis, mPlot->yAxis);
    bars->setData(QVector<double> { 1, 2, 3 }, QVector<double> { 1, 2, 3 });
    mPlot->resize(300, 200);
    mPlot->xAxis->setRange(0, 4);
    mPlot->yAxis->setRange(0, 4);
    mPlot->replot();
    const int recorded = bars->recordings();
    QVERIFY2(recorded > 0, "the first frame must record");

    mPlot->xAxis->setRange(1, 5); // a pan
    mPlot->replot();
    QCOMPARE(bars->recordings(), recorded);

    mPlot->xAxis->setRange(0.5, 1.5); // a zoom: a new scale, and still no new recording
    mPlot->replot();
    QCOMPARE(bars->recordings(), recorded);
}

void TestQCPBars::pixelWidthBarsReRecordOnEveryPan()
{
    // A width in pixels is baked out of the transform: any new transform remakes the recording,
    // a pan included (the pixels it baked all moved).
    auto* bars = new CountingBars(mPlot->xAxis, mPlot->yAxis);
    bars->setData(QVector<double> { 1, 2, 3 }, QVector<double> { 1, 2, 3 });
    bars->setWidth(8);
    bars->setWidthType(QCPBars::wtAbsolute);
    mPlot->resize(300, 200);
    mPlot->xAxis->setRange(0, 4);
    mPlot->yAxis->setRange(0, 4);
    mPlot->replot();
    const int recorded = bars->recordings();
    QVERIFY2(recorded > 0, "the first frame must record");

    mPlot->xAxis->setRange(1, 5); // a pan: same width, but every baked pixel moved
    mPlot->replot();
    QCOMPARE(bars->recordings(), recorded + 1);

    mPlot->xAxis->setRange(0.5, 1.5); // a zoom: a new scale as well
    mPlot->replot();
    QCOMPARE(bars->recordings(), recorded + 2);
}

void TestQCPBars::barsSelectionReRecordsAndUsesTheDecorator()
{
    auto* bars = new CountingBars(mPlot->xAxis, mPlot->yAxis);
    bars->setData(QVector<double> { 1, 2, 3 }, QVector<double> { 2, 2, 2 });
    bars->setWidth(0.6);
    bars->setPen(Qt::NoPen); // fill only, so the pixel read is the brush
    mPlot->resize(300, 200);
    mPlot->xAxis->setRange(0, 4);
    mPlot->yAxis->setRange(0, 4);
    mPlot->replot();
    const int recorded = bars->recordings();

    const auto pixelOfBar = [this](double key)
    {
        const QImage image = mPlot->toPixmap(300, 200).toImage();
        const double dpr = image.devicePixelRatio();
        return image.pixelColor(qRound(mPlot->xAxis->coordToPixel(key) * dpr),
                                qRound(mPlot->yAxis->coordToPixel(1) * dpr));
    };
    const QColor selected = bars->selectionDecorator()->brush().color();
    const QColor unselected = pixelOfBar(2);
    QVERIFY2(unselected != selected, "the bar is already drawn selected");

    bars->setSelectable(QCP::stSingleData); // otherwise the whole plottable is one selection
    bars->setSelection(QCPDataSelection(QCPDataRange(1, 2)));
    mPlot->replot();
    QCOMPARE(bars->recordings(), recorded + 1); // once, for the new look
    QCOMPARE(pixelOfBar(2), selected);          // the selected bar wears the decorator's brush
    QCOMPARE(pixelOfBar(1), unselected);        // its neighbour did not change
}

void TestQCPBars::bracketDecorationOverTheBars()
{
    // The bracket decorator marks the ends of a selection: it must stay visible over the bar it
    // marks, which is drawn by the GPU on screen.
    QWidget host;
    QCustomPlot* plot = shownPlot(host);
    if (!plot)
        QSKIP("no QRhi on this platform");
    auto* bars = new QCPBars(plot->xAxis, plot->yAxis);
    bars->setData(QVector<double> { 1 }, QVector<double> { 2 });
    bars->setWidth(0.6);
    bars->setPen(Qt::NoPen);
    bars->setBrush(Qt::blue);
    plot->xAxis->setRange(0, 2);
    plot->yAxis->setRange(0, 4);

    auto* bracket = new QCPSelectionDecoratorBracket;
    bracket->setBracketStyle(QCPSelectionDecoratorBracket::bsSquareBracket);
    bracket->setBracketPen(QPen(Qt::green, 3));
    bracket->setBrush(Qt::blue); // the selected bar's fill, under the brackets
    bracket->setBracketWidth(10);
    bracket->setBracketHeight(40);
    bars->setSelectionDecorator(bracket);
    bars->setSelectable(QCP::stSingleData);
    bars->setSelection(QCPDataSelection(QCPDataRange(0, 1)));

    plot->replot(QCustomPlot::rpImmediateRefresh);
    QTest::qWait(10);
    const QImage frame = plot->grabFramebuffer();
    const double dpr = frame.devicePixelRatio();
    // The brackets sit on the bar's top pixel and are 40 pixels tall: the vertical stroke runs
    // through the bar, six pixels under its top edge.
    const QColor over
        = frame.pixelColor(qRound(plot->xAxis->coordToPixel(1) * dpr),
                           qRound((plot->yAxis->coordToPixel(2) + 6) * dpr));
    QVERIFY2(over.green() > 150 && over.red() < 120 && over.blue() < 120,
             qPrintable(QStringLiteral("the bar covers its bracket: %1").arg(over.name())));
}

void TestQCPBars::barsPanDoesNotReRecord()
{
    // 10 000 bars, all positions in plot coordinates: a pan reuses both the recording and the
    // triangles extruded for its outlines (Review Focus 2).
    QWidget host;
    QCustomPlot* plot = shownPlot(host);
    if (!plot)
        QSKIP("no QRhi on this platform");
    auto* bars = new CountingBars(plot->xAxis, plot->yAxis);
    bars->setData(spreadKeys(10000), flatValues(10000));
    plot->xAxis->setRange(0, 10);
    plot->yAxis->setRange(0, 1);
    plot->replot();
    QTRY_VERIFY(!plot->isRepaintQueued());
    QCoreApplication::processEvents();
    const int recorded = bars->recordings();
    const quint64 extruded = QCPLineExtruder::extruderCallCount();
    QVERIFY2(recorded > 0, "the first frame must record");
    QVERIFY2(extruded > 0, "the first frame must extrude the bar outlines");

    plot->xAxis->setRange(0.002, 10.002); // a pan: same span, shifted
    plot->replot();
    QTRY_VERIFY(!plot->isRepaintQueued());
    QCoreApplication::processEvents();
    QCOMPARE(bars->recordings(), recorded);
    QCOMPARE(QCPLineExtruder::extruderCallCount(), extruded);
}
