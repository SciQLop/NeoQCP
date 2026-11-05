#include "test-qcpbars.h"
#include "painting/line-extruder.h"
#include <QElapsedTimer>
#include <QMainWindow>
#include <QtWidgets/qtestsupport_widgets.h>

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

void TestQCPBars::pixelWidthBarsReRecordOnZoomOnly()
{
    // A width in pixels is baked out of the transform: a pan keeps it, a zoom does not.
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

    mPlot->xAxis->setRange(1, 5); // a pan: the bar is still 8 pixels wide
    mPlot->replot();
    QCOMPARE(bars->recordings(), recorded);

    mPlot->xAxis->setRange(0.5, 1.5); // a zoom: the recording would put the bar where it no longer is
    mPlot->replot();
    QCOMPARE(bars->recordings(), recorded + 1);
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
