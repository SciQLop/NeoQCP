#include "test-qcpfinancial.h"
#include "qcustomplot.h"
#include "reference-image.h"
#include <QImage>
#include <QMainWindow>
#include <functional>

void TestQCPFinancial::init()
{
  mPlot = new QCustomPlot(0);
  mFinancial = new QCPFinancial(mPlot->xAxis, mPlot->yAxis);
}

void TestQCPFinancial::cleanup()
{
  delete mPlot;
}

void TestQCPFinancial::dataManipulation()
{
  QVERIFY(mFinancial->data()->isEmpty());
  
  QVector<double> key, open, high, low, close;
  key << -1 << 1 << -2 << 2;
  open << 11 << 13 << 15 << 17;
  high << 20 << 22 << 24 << 26;
  low << 10 << 12 << 14 << 16;
  close << 12 << 14 << 16 << 18;
  
  mFinancial->setData(key, open, high, low, close);
  QCOMPARE(mFinancial->data()->size(), 4);
  // data should be sorted by key:
  QCOMPARE((mFinancial->data()->constBegin()+0)->key, -2.0);
  QCOMPARE((mFinancial->data()->constBegin()+1)->key, -1.0);
  QCOMPARE((mFinancial->data()->constBegin()+2)->key, 1.0);
  QCOMPARE((mFinancial->data()->constBegin()+3)->key, 2.0);
  QCOMPARE((mFinancial->data()->constBegin()+0)->open, 15.0);
  QCOMPARE((mFinancial->data()->constBegin()+1)->open, 11.0);
  QCOMPARE((mFinancial->data()->constBegin()+2)->open, 13.0);
  QCOMPARE((mFinancial->data()->constBegin()+3)->open, 17.0);
  QCOMPARE((mFinancial->data()->constBegin()+0)->high, 24.0);
  QCOMPARE((mFinancial->data()->constBegin()+1)->high, 20.0);
  QCOMPARE((mFinancial->data()->constBegin()+2)->high, 22.0);
  QCOMPARE((mFinancial->data()->constBegin()+3)->high, 26.0);
  QCOMPARE((mFinancial->data()->constBegin()+0)->low, 14.0);
  QCOMPARE((mFinancial->data()->constBegin()+1)->low, 10.0);
  QCOMPARE((mFinancial->data()->constBegin()+2)->low, 12.0);
  QCOMPARE((mFinancial->data()->constBegin()+3)->low, 16.0);
  QCOMPARE((mFinancial->data()->constBegin()+0)->close, 16.0);
  QCOMPARE((mFinancial->data()->constBegin()+1)->close, 12.0);
  QCOMPARE((mFinancial->data()->constBegin()+2)->close, 14.0);
  QCOMPARE((mFinancial->data()->constBegin()+3)->close, 18.0);

  // map key should be identical to data key:
  for (int i=0; i<4; ++i)
    QCOMPARE((mFinancial->data()->constBegin()+i)->key, (mFinancial->data()->constBegin()+i)->key);
  
  // data removal:
  mFinancial->data()->removeBefore(0);
  QCOMPARE(mFinancial->data()->size(), 2);
  QCOMPARE((mFinancial->data()->constBegin()+0)->key, 1.0);
  QCOMPARE((mFinancial->data()->constBegin()+1)->key, 2.0);
  
  mFinancial->setData(key, open, high, low, close);
  mFinancial->data()->removeAfter(0);
  QCOMPARE(mFinancial->data()->size(), 2);
  QCOMPARE((mFinancial->data()->constBegin()+0)->key, -2.0);
  QCOMPARE((mFinancial->data()->constBegin()+1)->key, -1.0);
  
  mFinancial->setData(key, open, high, low, close);
  mFinancial->data()->remove(-1.1, -0.9);
  QCOMPARE(mFinancial->data()->size(), 3);
  QCOMPARE((mFinancial->data()->constBegin()+0)->key, -2.0);
  QCOMPARE((mFinancial->data()->constBegin()+1)->key, 1.0);
  QCOMPARE((mFinancial->data()->constBegin()+2)->key, 2.0);
  
  mFinancial->setData(key, open, high, low, close);
  mFinancial->data()->remove(-2.1, -1.9);
  QCOMPARE(mFinancial->data()->size(), 3);
  QCOMPARE((mFinancial->data()->constBegin()+0)->key, -1.0);
  QCOMPARE((mFinancial->data()->constBegin()+1)->key, 1.0);
  QCOMPARE((mFinancial->data()->constBegin()+2)->key, 2.0);
  
  mFinancial->setData(key, open, high, low, close);
  mFinancial->data()->remove(1.9, 2.1);
  QCOMPARE(mFinancial->data()->size(), 3);
  QCOMPARE((mFinancial->data()->constBegin()+0)->key, -2.0);
  QCOMPARE((mFinancial->data()->constBegin()+1)->key, -1.0);
  QCOMPARE((mFinancial->data()->constBegin()+2)->key, 1.0);
  
  mFinancial->setData(key, open, high, low, close);
  mFinancial->data()->remove(-1.1, 1.1);
  QCOMPARE(mFinancial->data()->size(), 2);
  QCOMPARE((mFinancial->data()->constBegin()+0)->key, -2.0);
  QCOMPARE((mFinancial->data()->constBegin()+1)->key, 2.0);
  
  mFinancial->setData(key, open, high, low, close);
  mFinancial->data()->clear();
  QCOMPARE(mFinancial->data()->size(), 0);
  
  // add single data points:
  mFinancial->addData(5, 19, 28, 18, 20);
  QCOMPARE(mFinancial->data()->size(), 1);
  mFinancial->addData(4, 21, 30, 20, 22);
  QCOMPARE(mFinancial->data()->size(), 2);
  QCOMPARE((mFinancial->data()->constBegin()+0)->open, 21.0);
  QCOMPARE((mFinancial->data()->constBegin()+1)->open, 19.0);
  
  mFinancial->setData(key, open, high, low, close);
  mFinancial->addData(3, 19, 28, 18, 20);
  QCOMPARE(mFinancial->data()->size(), 5);
  QCOMPARE((mFinancial->data()->constBegin()+0)->open, 15.0);
  QCOMPARE((mFinancial->data()->constBegin()+1)->open, 11.0);
  QCOMPARE((mFinancial->data()->constBegin()+2)->open, 13.0);
  QCOMPARE((mFinancial->data()->constBegin()+3)->open, 17.0);
  QCOMPARE((mFinancial->data()->constBegin()+4)->open, 19.0);
  
  mFinancial->setData(key, open, high, low, close);
  mFinancial->addData(QVector<double>() << 3 << 4 << 5,
                      QVector<double>() << 19 << 21 << 23,
                      QVector<double>() << 28 << 30 << 32,
                      QVector<double>() << 18 << 20 << 22,
                      QVector<double>() << 20 << 22 << 24);
  QCOMPARE(mFinancial->data()->size(), 7);
  QCOMPARE((mFinancial->data()->constBegin()+0)->open, 15.0);
  QCOMPARE((mFinancial->data()->constBegin()+1)->open, 11.0);
  QCOMPARE((mFinancial->data()->constBegin()+2)->open, 13.0);
  QCOMPARE((mFinancial->data()->constBegin()+3)->open, 17.0);
  QCOMPARE((mFinancial->data()->constBegin()+4)->open, 19.0);
  QCOMPARE((mFinancial->data()->constBegin()+5)->open, 21.0);
  QCOMPARE((mFinancial->data()->constBegin()+6)->open, 23.0);
}

void TestQCPFinancial::dataSharing()
{
  QCPFinancial *financial2 = new QCPFinancial(mPlot->xAxis, mPlot->yAxis);
  
  QSharedPointer<QCPFinancialDataContainer> data = financial2->data();
  mFinancial->setData(data); // now shares data with financial2
  
  QVector<double> key, open, high, low, close;
  key << -1 << 1 << -2 << 2;
  open << 11 << 13 << 15 << 17;
  high << 20 << 22 << 24 << 26;
  low << 10 << 12 << 14 << 16;
  close << 12 << 14 << 16 << 18;
  financial2->setData(key, open, high, low, close);
  
  QCOMPARE(mFinancial->data()->size(), 4);
  QCOMPARE(financial2->data()->size(), 4);
  data->add(QCPFinancialData(4, 19, 28, 18, 20)); // modify from outside
  QCOMPARE(mFinancial->data()->size(), 5);
  QCOMPARE(financial2->data()->size(), 5);
  data.clear(); // clear external reference, internals should live on
  QVERIFY(!data);
  QCOMPARE(mFinancial->data()->size(), 5);
  QCOMPARE(financial2->data()->size(), 5);
  
  // decouple data again:
  mFinancial->setData(QSharedPointer<QCPFinancialDataContainer>(new QCPFinancialDataContainer));
  mFinancial->addData(1, 1, 6, 0, 3);
  financial2->addData(4, 1, 6, 0, 3);
  QCOMPARE(mFinancial->data()->size(), 1);
  QCOMPARE(financial2->data()->size(), 6);
}


namespace
{
const QString kDir = QStringLiteral("test-reference-m4e");

//! The financial chart of the reference scenes, with what it recorded: the recording is what these
//! tests hold to the reference images.
class RecordingFinancial : public QCPFinancial
{
public:
  using QCPFinancial::QCPFinancial;

  [[nodiscard]] int recordings() const { return mCore.recordingCount(); }
  [[nodiscard]] int recordedCommands() const { return mCore.recordedCommandCount(); }
};

struct Scene
{
  QImage image;
  int commands = 0;
};

QImage imageOf(QCustomPlot& plot)
{
  return plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
}

//! The scene the reference suite renders: the same data, width and axes, through a plottable that
//! says what it recorded.
Scene financialScene(bool ohlc, bool verticalKey, bool logValue, bool selected)
{
  QCustomPlot plot;
  plot.resize(400, 300);
  QFont font(QStringLiteral("Helvetica"));
  font.setPixelSize(10);
  plot.setFont(font);
  for (QCPAxis* axis : plot.axisRect()->axes())
  {
    axis->setTickLabelFont(font);
    axis->setLabelFont(font);
  }
  QCPAxis* key = verticalKey ? plot.yAxis : plot.xAxis;
  QCPAxis* value = verticalKey ? plot.xAxis : plot.yAxis;
  key->setRange(0, 7);
  value->setRange(logValue ? 1 : 0, 10);
  auto* financial = new RecordingFinancial(key, value);
  const QVector<double> keys { 0.6, 1.4, 2.2, 3.0, 3.8, 4.6, 5.4, 6.2 };
  QVector<double> open, high, low, close;
  for (int i = 0; i < keys.size(); ++i)
  {
    const double base = 3.0 + qSin(0.9 * i) * 1.5 + i * 0.4;
    open << base;
    close << base + (i % 3 == 0 ? 0.8 : -0.6);
    high << qMax(open.last(), close.last()) + 0.7;
    low << qMin(open.last(), close.last()) - 0.6;
  }
  financial->setData(keys, open, high, low, close);
  financial->setChartStyle(ohlc ? QCPFinancial::csOhlc : QCPFinancial::csCandlestick);
  financial->setWidth(0.7);
  if (selected)
  {
    financial->setSelectable(QCP::stDataRange);
    financial->setSelection(QCPDataSelection(QCPDataRange(2, 5)));
  }
  if (logValue)
    value->setScaleType(QCPAxis::stLogarithmic);
  plot.replot();
  return { imageOf(plot), financial->recordedCommands() };
}

//! A candlestick chart to poke at: the reference scene's axes and width, unselected.
RecordingFinancial* makeFinancialPlot(QCustomPlot& plot)
{
  plot.resize(400, 300);
  plot.xAxis->setRange(0, 7);
  plot.yAxis->setRange(0, 10);
  auto* financial = new RecordingFinancial(plot.xAxis, plot.yAxis);
  financial->setData(QVector<double> { 0.6, 1.4, 2.2, 3.0, 3.8, 4.6, 5.4, 6.2 },
                     QVector<double> { 3.0, 3.6, 4.2, 4.8, 5.4, 6.0, 6.6, 7.2 },
                     QVector<double> { 4.0, 4.6, 5.2, 5.8, 6.4, 7.0, 7.6, 8.2 },
                     QVector<double> { 2.0, 2.6, 3.2, 3.8, 4.4, 5.0, 5.6, 6.2 },
                     QVector<double> { 3.4, 4.0, 4.6, 5.2, 5.8, 6.4, 7.0, 7.6 });
  financial->setChartStyle(QCPFinancial::csCandlestick);
  financial->setWidth(0.7);
  plot.replot();
  return financial;
}
} // namespace

void TestQCPFinancial::recordedChartsMatchTheirReferences_data()
{
  QTest::addColumn<bool>("ohlc");
  QTest::addColumn<bool>("verticalKey");
  QTest::addColumn<bool>("logValue");
  QTest::addColumn<bool>("selected");
  QTest::addColumn<QString>("name");
  QTest::newRow("candle")
      << false << false << false << false << QStringLiteral("financial-candle.png");
  QTest::newRow("ohlc") << true << false << false << false << QStringLiteral("financial-ohlc.png");
  QTest::newRow("vertical-key")
      << false << true << false << false << QStringLiteral("financial-vertical-key.png");
  QTest::newRow("log") << false << false << true << false << QStringLiteral("financial-log.png");
  QTest::newRow("selected")
      << false << false << false << true << QStringLiteral("financial-selected.png");
}

void TestQCPFinancial::recordedChartsMatchTheirReferences()
{
  QFETCH(bool, ohlc);
  QFETCH(bool, verticalKey);
  QFETCH(bool, logValue);
  QFETCH(bool, selected);
  QFETCH(QString, name);
  const Scene scene = financialScene(ohlc, verticalKey, logValue, selected);
  QVERIFY2(scene.commands > 0, "the financial chart recorded nothing");
  QString why;
  QVERIFY2(qcp::test::matchesReference(scene.image, kDir, name, &why),
           qPrintable(QStringLiteral("%1: %2").arg(name, why)));
}

void TestQCPFinancial::aPannedCandlestickIsNotReRecorded()
{
  QCustomPlot plot;
  RecordingFinancial* financial = makeFinancialPlot(plot);
  QCOMPARE(financial->recordings(), 1);
  plot.xAxis->setRange(1, 8); // a pan: the same width, one key further
  plot.replot();
  QCOMPARE(financial->recordings(), 1);
}

void TestQCPFinancial::everyStyleSetterReRecords()
{
  QCustomPlot plot;
  RecordingFinancial* financial = makeFinancialPlot(plot);
  const auto reRecords = [&](const std::function<void()>& change)
  {
    const int before = financial->recordings();
    change();
    plot.replot();
    return financial->recordings() == before + 1;
  };
  QVERIFY2(reRecords([&] { financial->setChartStyle(QCPFinancial::csOhlc); }),
           "setChartStyle did not remake the recording");
  QVERIFY2(reRecords([&] { financial->setWidth(0.3); }),
           "setWidth did not remake the recording");
  QVERIFY2(reRecords([&] { financial->setWidthType(QCPFinancial::wtAbsolute); }),
           "setWidthType did not remake the recording");
  QVERIFY2(reRecords([&] { financial->setTwoColored(false); }),
           "setTwoColored did not remake the recording");
  QVERIFY2(reRecords([&] { financial->setBrushPositive(QBrush(Qt::red)); }),
           "setBrushPositive did not remake the recording");
  QVERIFY2(reRecords([&] { financial->setBrushNegative(QBrush(Qt::green)); }),
           "setBrushNegative did not remake the recording");
  QVERIFY2(reRecords([&] { financial->setPenPositive(QPen(Qt::blue)); }),
           "setPenPositive did not remake the recording");
  QVERIFY2(reRecords([&] { financial->setPenNegative(QPen(Qt::darkRed)); }),
           "setPenNegative did not remake the recording");
  financial->setSelectable(QCP::stDataRange);
  QVERIFY2(reRecords([&] { financial->setSelection(QCPDataSelection(QCPDataRange(1, 3))); }),
           "a selection change did not remake the recording");
}
