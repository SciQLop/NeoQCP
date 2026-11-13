#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent),
  ui(new Ui::MainWindow)
{
  ui->setupUi(this);
  setGeometry(300, 300, 500, 500);
  
  // invoke all methods of MainWindow that start with "snippet" or "website":
  for (int i=this->metaObject()->methodOffset(); i<this->metaObject()->methodCount(); ++i)
  {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    if (QString::fromLatin1(this->metaObject()->method(i).signature()).startsWith("snippet") ||
        QString::fromLatin1(this->metaObject()->method(i).signature()).startsWith("website"))
#else
    if (this->metaObject()->method(i).methodSignature().startsWith("snippet") ||
        this->metaObject()->method(i).methodSignature().startsWith("website"))
#endif
    {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
      qDebug() << "executing" << this->metaObject()->method(i).signature() << "...";
#else
      qDebug() << "executing" << this->metaObject()->method(i).methodSignature() << "...";
#endif
      resetPlot();
      if (!this->metaObject()->method(i).invoke(this))
        qDebug() << "Failed to invoke doc-code-snippet method" << i;
    }
  }
  QTimer::singleShot(0, qApp, SLOT(quit()));
}

MainWindow::~MainWindow()
{
  delete ui;
}

void MainWindow::snippetQCPColorGradient()
{
  QCPColorMap *colorMap = new QCPColorMap(customPlot->xAxis, customPlot->yAxis);
  
  //! [qcpcolorgradient-setgradient]
  colorMap->setGradient(QCPColorGradient::gpHot);
  //! [qcpcolorgradient-setgradient]
}

void MainWindow::snippetQCPColorScale()
{
  //! [qcpcolorscale-creation]
  QCPColorScale *colorScale = new QCPColorScale(customPlot);
  customPlot->plotLayout()->addElement(0, 1, colorScale);
  colorScale->setLabel("Some Label Text");
  //! [qcpcolorscale-creation]
  
  //! [qcpcolorscale-tickcount]
  colorScale->axis()->ticker()->setTickCount(3);
  //! [qcpcolorscale-tickcount]
  
  //! [qcpcolorscale-margingroup]
  QCPMarginGroup *group = new QCPMarginGroup(customPlot);
  colorScale->setMarginGroup(QCP::msTop|QCP::msBottom, group);
  customPlot->axisRect()->setMarginGroup(QCP::msTop|QCP::msBottom, group);
  //! [qcpcolorscale-margingroup]
}

void MainWindow::snippetQCPColorMap()
{
  //! [qcpcolormap-creation-1]
  QCPColorMap *colorMap = new QCPColorMap(customPlot->xAxis, customPlot->yAxis);
  //! [qcpcolormap-creation-1]

  //! [qcpcolormap-creation-2]
  colorMap->data()->setSize(50, 50);
  colorMap->data()->setRange(QCPRange(0, 2), QCPRange(0, 2));
  for (int x=0; x<50; ++x)
    for (int y=0; y<50; ++y)
      colorMap->data()->setCell(x, y, qCos(x/10.0)+qSin(y/10.0));
  colorMap->setGradient(QCPColorGradient::gpPolar);
  colorMap->rescaleDataRange(true);
  customPlot->rescaleAxes();
  customPlot->replot();
  //! [qcpcolormap-creation-2]
}

void MainWindow::snippetQCPCurve()
{
  QVector<double> tData, xData, yData;
  
  //! [qcpcurve-creation-1]
  QCPCurve *newCurve = new QCPCurve(customPlot->xAxis, customPlot->yAxis);
  //! [qcpcurve-creation-1]
  
  //! [qcpcurve-creation-2]
  newCurve->setName("Fermat's Spiral");
  newCurve->setData(tData, xData, yData);
  //! [qcpcurve-creation-2]
}

void MainWindow::snippetQCPItemLine()
{
  //! [qcpitemline-creation-1]
  QCPItemLine *line = new QCPItemLine(customPlot);
  //! [qcpitemline-creation-1]
  
  //! [qcpitemline-creation-2]
  line->start->setCoords(-0.1, 0.8);
  line->end->setCoords(1.1, 0.2);
  //! [qcpitemline-creation-2]
  
  //! [qcpitemline-creation-3]
  line->start->setType(QCPItemPosition::ptAbsolute);
  line->end->setType(QCPItemPosition::ptAbsolute);
  //! [qcpitemline-creation-3]
  
  //! [qcpitemline-creation-4]
  line->start->setCoords(100, 200);
  line->end->setCoords(450, 320);
  //! [qcpitemline-creation-4]
  
  //! [qcpitemline-creation-5]
  line->setClipToAxisRect(false);
  //! [qcpitemline-creation-5]
}

void MainWindow::snippetQCPMarginGroup()
{
  QCPAxisRect *ar = new QCPAxisRect(customPlot);
  customPlot->plotLayout()->addElement(0, 1, ar);
  
  //! [qcpmargingroup-creation-1]
  QCPMarginGroup *group = new QCPMarginGroup(customPlot);
  //! [qcpmargingroup-creation-1]
  
  //! [qcpmargingroup-creation-2]
  customPlot->axisRect(0)->setMarginGroup(QCP::msLeft|QCP::msRight, group);
  customPlot->axisRect(1)->setMarginGroup(QCP::msLeft|QCP::msRight, group);
  //! [qcpmargingroup-creation-2]
}

void MainWindow::snippetQCPTextElement()
{
  //! [qcptextelement-creation]
  customPlot->plotLayout()->insertRow(0); // inserts an empty row above the default axis rect
  customPlot->plotLayout()->addElement(0, 0, new QCPTextElement(customPlot, "Your Text"));
  //! [qcptextelement-creation]
}

void MainWindow::snippetQCPLineEnding()
{
  QCPItemLine *myItemLine = new QCPItemLine(customPlot);
  //! [qcplineending-sethead]
  myItemLine->setHead(QCPLineEnding::esSpikeArrow);
  //! [qcplineending-sethead]
}

void MainWindow::snippetQCPScatterStyle()
{
  customPlot->addGraph();
  
  //! [qcpscatterstyle-creation-1]
  QCPScatterStyle myScatter;
  myScatter.setShape(QCPScatterStyle::ssCircle);
  myScatter.setPen(QPen(Qt::blue));
  myScatter.setBrush(Qt::white);
  myScatter.setSize(5);
  customPlot->graph(0)->setScatterStyle(myScatter);
  //! [qcpscatterstyle-creation-1]
  
  //! [qcpscatterstyle-creation-2]
  customPlot->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, Qt::blue, Qt::white, 5));
  //! [qcpscatterstyle-creation-2]
  
  //! [qcpscatterstyle-shortcreation]
  customPlot->graph(0)->setScatterStyle(QCPScatterStyle::ssPlus);
  //! [qcpscatterstyle-shortcreation]
}

void MainWindow::snippetQCPBarsGroup()
{
  QCPBars *bars1 = new QCPBars(customPlot->xAxis, customPlot->yAxis);
  QCPBars *bars2 = new QCPBars(customPlot->xAxis, customPlot->yAxis);
  
  //! [qcpbarsgroup-creation]
  QCPBarsGroup *group = new QCPBarsGroup(customPlot);
  group->append(bars1);
  group->append(bars2);
  //! [qcpbarsgroup-creation]
}

void MainWindow::snippetQCPBars()
{
  QVector<double> xData, yData;
  
  //! [qcpbars-creation-1]
  QCPBars *newBars = new QCPBars(customPlot->xAxis, customPlot->yAxis);
  //! [qcpbars-creation-1]
  
  //! [qcpbars-creation-2]
  newBars->setName("Country population");
  newBars->setData(xData, yData);
  //! [qcpbars-creation-2]
}

void MainWindow::snippetQCPStatisticalBox()
{
  //! [qcpstatisticalbox-creation-1]
  QCPStatisticalBox *newStatistical = new QCPStatisticalBox(customPlot->xAxis, customPlot->yAxis);
  //! [qcpstatisticalbox-creation-1]
  
  //! [qcpstatisticalbox-creation-2]
  newStatistical->setName("Measurement Series 1");
  newStatistical->addData(1000, 1, 3, 4, 5, 7);
  //! [qcpstatisticalbox-creation-2]
}

void MainWindow::snippetQCPFinancial()
{
  QVector<double> time, price;
  //! [qcpfinancial-creation-1]
  QCPFinancial *newFinancial = new QCPFinancial(customPlot->xAxis, customPlot->yAxis);
  //! [qcpfinancial-creation-1]
  
  //! [qcpfinancial-creation-2]
  newFinancial->setName("Stock prices");
  newFinancial->data()->set(QCPFinancial::timeSeriesToOhlc(time, price, 3600*24));
  newFinancial->setChartStyle(QCPFinancial::csCandlestick);
  //! [qcpfinancial-creation-2]
}

void MainWindow::snippetQCPAxisTickers()
{
  //! [qcpaxisticker-log-creation]
  customPlot->xAxis->setScaleType(QCPAxis::stLogarithmic);
  customPlot->xAxis->setTicker(QSharedPointer<QCPAxisTickerLog>(new QCPAxisTickerLog));
  //! [qcpaxisticker-log-creation]
  
  QCPColorScale *colorScale = new QCPColorScale(customPlot);
  customPlot->plotLayout()->addElement(colorScale);
  //! [qcpaxisticker-log-colorscale]
  colorScale->setDataScaleType(QCPAxis::stLogarithmic);
  colorScale->axis()->setTicker(QSharedPointer<QCPAxisTickerLog>(new QCPAxisTickerLog));
  //! [qcpaxisticker-log-colorscale]
}

void MainWindow::snippetQCPCurveDataSharing()
{
  QCPCurve *curve1 = new QCPCurve(customPlot->xAxis, customPlot->yAxis);
  QCPCurve *curve2 = new QCPCurve(customPlot->xAxis, customPlot->yAxis);
  //! [qcpcurve-datasharing-1]
  curve2->setData(curve1->data()); // curve1 and curve2 now share data container
  //! [qcpcurve-datasharing-1]
  
  //! [qcpcurve-datasharing-2]
  curve2->data()->set(*curve1->data()); // curve2 now has copy of curve1's data in its container
  //! [qcpcurve-datasharing-2]
}

void MainWindow::snippetQCPBarsDataSharing()
{
  QCPBars *bars1 = new QCPBars(customPlot->xAxis, customPlot->yAxis);
  QCPBars *bars2 = new QCPBars(customPlot->xAxis, customPlot->yAxis);
  //! [qcpbars-datasharing-1]
  bars2->setData(bars1->data()); // bars1 and bars2 now share data container
  //! [qcpbars-datasharing-1]
  
  //! [qcpbars-datasharing-2]
  bars2->data()->set(*bars1->data()); // bars2 now has copy of bars1's data in its container
  //! [qcpbars-datasharing-2]
}

void MainWindow::snippetQCPStatisticalBoxDataSharing()
{
  QCPStatisticalBox *statBox1 = new QCPStatisticalBox(customPlot->xAxis, customPlot->yAxis);
  QCPStatisticalBox *statBox2 = new QCPStatisticalBox(customPlot->xAxis, customPlot->yAxis);
  //! [qcpstatisticalbox-datasharing-1]
  statBox2->setData(statBox1->data()); // statBox1 and statBox2 now share data container
  //! [qcpstatisticalbox-datasharing-1]
  
  //! [qcpstatisticalbox-datasharing-2]
  statBox2->data()->set(*statBox1->data()); // statBox2 now has copy of statBox1's data in its container
  //! [qcpstatisticalbox-datasharing-2]
}

void MainWindow::snippetQCPFinancialDataSharing()
{
  QCPFinancial *financial1 = new QCPFinancial(customPlot->xAxis, customPlot->yAxis);
  QCPFinancial *financial2 = new QCPFinancial(customPlot->xAxis, customPlot->yAxis);
  //! [qcpfinancial-datasharing-1]
  financial2->setData(financial1->data()); // financial1 and financial2 now share data container
  //! [qcpfinancial-datasharing-1]
  
  //! [qcpfinancial-datasharing-2]
  financial2->data()->set(*financial1->data()); // financial2 now has copy of financial1's data in its container
  //! [qcpfinancial-datasharing-2]
}

void MainWindow::snippetQCPErrorBarsDataSharing()
{
  QCPErrorBars *errorBars1 = new QCPErrorBars(customPlot->xAxis, customPlot->yAxis);
  QCPErrorBars *errorBars2 = new QCPErrorBars(customPlot->xAxis, customPlot->yAxis);
  //! [qcperrorbars-datasharing-1]
  errorBars2->setData(errorBars1->data()); // errorBars1 and errorBars2 now share data container
  //! [qcperrorbars-datasharing-1]
  
  //! [qcperrorbars-datasharing-2]
  *errorBars2->data() = *errorBars1->data(); // errorBars2 now has copy of errorBars1's data in its container
  //! [qcperrorbars-datasharing-2]
}

void MainWindow::snippetQCPDataSelectionIterating()
{
  QCPGraph *graph = customPlot->addGraph();
  for (int i=0; i<100; ++i)
    graph->addData(i, i);
  graph->setSelection(QCPDataRange(10, 15) + QCPDataRange(20, 40) + QCPDataRange(60, 80));
  
  //! [qcpdataselection-iterating-1]
  QCPDataSelection selection = graph->selection();
  double sum = 0;
  foreach (QCPDataRange dataRange, selection.dataRanges())
  {
    for (int i=dataRange.begin(); i<dataRange.end(); ++i)
    {
      // index "i" will go through all selected data points, as an example, we calculate the value average
      sum += graph->dataMainValue(i);
    }
  }
  double average = sum/selection.dataPointCount();
  //! [qcpdataselection-iterating-1]
  
  Q_UNUSED(average)
}

void MainWindow::snippetQCPDataSelectionPointAtPos()
{
  QCPGraph *graph = customPlot->addGraph();
  //! [qcpdataselection-pointatpos]
  int index = -1;
  QVariant details;
  if (graph->selectTest(QPoint(123, 456), false, &details) >= 0) // QPoint could be e.g. event->pos() of a mouse event
    index = qcp::hitDataIndex(details).value_or(-1);
  // "index" now is the data point at pixel coordinates (123, 456), or -1 if no data point was hit.
  //! [qcpdataselection-pointatpos]
  Q_UNUSED(index)
}

void MainWindow::websiteBasicPlottingBars()
{
  QCPBars *myBars = new QCPBars(customPlot->xAxis, customPlot->yAxis);
  // now we can modify properties of myBars:
  myBars->setName("Bars Series 1");
  QVector<double> keyData;
  QVector<double> valueData;
  keyData << 1 << 2 << 3;
  valueData << 2 << 4 << 8;
  myBars->setData(keyData, valueData);
  customPlot->rescaleAxes();
  customPlot->replot();
}




void MainWindow::resetPlot()
{
  customPlot = new QCustomPlot(this);
  setCentralWidget(customPlot);
}
