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
