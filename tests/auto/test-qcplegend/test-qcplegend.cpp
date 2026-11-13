#include "test-qcplegend.h"

void TestQCPLegend::init()
{
  mPlot = new QCustomPlot(0);
  mPlot->legend->setVisible(true);
  mPlot->show();
  //QTest::qWait(150);
}


void TestQCPLegend::cleanup()
{
  delete mPlot;
}

void TestQCPLegend::autoAddPlottables()
{
  // legend items shouldn't be added if auto-add is off:
  QCOMPARE(mPlot->legend->itemCount(), 0);
  mPlot->setAutoAddPlottableToLegend(false);
  mPlot->addGraph();
  QCOMPARE(mPlot->legend->itemCount(), 0);
  new QCPBars(mPlot->xAxis, mPlot->yAxis);
  QCOMPARE(mPlot->legend->itemCount(), 0);
  
  // legend items should be added if auto-add is on:
  mPlot->setAutoAddPlottableToLegend(true);
  mPlot->addGraph();
  QCOMPARE(mPlot->legend->itemCount(), 1);
  mPlot->addGraph();
  QCOMPARE(mPlot->legend->itemCount(), 2);
  new QCPBars(mPlot->xAxis, mPlot->yAxis);
  QCOMPARE(mPlot->legend->itemCount(), 3);
}

void TestQCPLegend::addAndRemove()
{
  mPlot->setAutoAddPlottableToLegend(false);
  
  QCPMultiGraph *g1 = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
  QCPMultiGraph *g2 = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
  QCPBars *b1 = new QCPBars(mPlot->xAxis, mPlot->yAxis);
  QCOMPARE(mPlot->legend->itemCount(), 0);
  
  g1->addToLegend();
  QCOMPARE(mPlot->legend->itemCount(), 1);
  mPlot->legend->addItem(new QCPPlottableLegendItem(mPlot->legend, g2)); // explicit way of adding to legend
  QCOMPARE(mPlot->legend->itemCount(), 2);
  b1->addToLegend();
  QCOMPARE(mPlot->legend->itemCount(), 3);
  
  g1->removeFromLegend();
  QCOMPARE(mPlot->legend->itemCount(), 2);
  
  mPlot->legend->clearItems();
  QCOMPARE(mPlot->legend->itemCount(), 0);
}





void TestQCPLegend::itemTextColorRepaintsTheLegend()
{
  // Dimming an entry (a hidden graph) only changes its text colour. The legend lives on a
  // layer of its own, so the setter must dirty it or the entry keeps its old colour until
  // something else repaints the legend.
  auto *graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
  graph->setName("dimmed");
  graph->addToLegend();
  mPlot->replot(QCustomPlot::rpImmediateRefresh);

  const auto buffer = mPlot->layer("legend")->mPaintBuffer.toStrongRef();
  QVERIFY(buffer);
  QVERIFY(!buffer->contentDirty());

  QCPAbstractLegendItem *item = mPlot->legend->item(0);
  QVERIFY(item);
  item->setTextColor(Qt::red);
  QVERIFY2(buffer->contentDirty(), "a new text colour must repaint the legend");

  mPlot->replot(QCustomPlot::rpImmediateRefresh);
  QVERIFY(!buffer->contentDirty());
  item->setTextColor(Qt::red);
  QVERIFY2(!buffer->contentDirty(), "an unchanged colour must not repaint it");
}

void TestQCPLegend::groupRowFollowsComponentVisibility()
{
  auto *mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
  mg->setData(std::vector<double>{1.0, 2.0, 3.0},
              std::vector<std::vector<double>>{{10.0, 20.0, 30.0}, {-1.0, -2.0, -3.0}});
  mg->addToLegend();
  mPlot->replot(QCustomPlot::rpImmediateRefresh);

  const auto buffer = mPlot->layer("legend")->mPaintBuffer.toStrongRef();
  QVERIFY(buffer);
  QVERIFY(!buffer->contentDirty());

  mg->setComponentVisible(0, false);
  QVERIFY2(buffer->contentDirty(), "the group row draws one segment per visible component");
}

namespace {
void addSelectedItems(QCustomPlot* plot, int count)
{
  for (int i = 0; i < count; ++i)
    plot->addGraph();
  for (int i = 0; i < count; ++i)
    plot->legend->item(i)->setSelected(true);
}
} // namespace

// setSelectedParts deselects the items one by one: a slot removing the item it was told about
// must not make the loop skip the next one.
void TestQCPLegend::deselectSlotRemovingAnItemStillDeselectsTheOthers()
{
  addSelectedItems(mPlot, 3);
  QCPAbstractLegendItem* first = mPlot->legend->item(0);
  connect(first, &QCPAbstractLegendItem::selectionChanged, mPlot,
          [this, first] { QVERIFY(mPlot->legend->removeItem(first)); }, Qt::SingleShotConnection);
  mPlot->legend->setSelectedParts(QCPLegend::spNone);
  QCOMPARE(mPlot->legend->itemCount(), 2);
  QVERIFY(!mPlot->legend->item(0)->selected());
  QVERIFY(!mPlot->legend->item(1)->selected());
  QCOMPARE(mPlot->legend->selectedParts(), QCPLegend::SelectableParts(QCPLegend::spNone));
}

void TestQCPLegend::theDefaultLegendClearsBeforeItDies()
{
  bool sawClearedLegend = false;
  connect(mPlot->legend, &QObject::destroyed, mPlot,
          [this, &sawClearedLegend] { sawClearedLegend = !mPlot->legend; });
  QCPLegend* legend = mPlot->legend;
  delete legend;
  QVERIFY2(sawClearedLegend, "a slot on the legend's destroyed() sees the member cleared, as in 2.x");
  QVERIFY(!mPlot->legend);
}

void TestQCPLegend::deselectSlotDeletingTheLegendIsSafe()
{
  addSelectedItems(mPlot, 3);
  connect(mPlot->legend->item(0), &QCPAbstractLegendItem::selectionChanged, mPlot,
          [this] { delete mPlot->legend; }, Qt::SingleShotConnection);
  mPlot->legend->setSelectedParts(QCPLegend::spNone);
  QCOMPARE(mPlot->legend, nullptr);
}

namespace {
// A click on nothing deselects everything: QCustomPlot::deselectAllExcept, then each
// layerable's deselectEvent.
void clickAway(QCustomPlot* plot)
{
  plot->setInteractions(QCP::iSelectLegend | QCP::iSelectPlottables);
  plot->resize(400, 300);
  plot->replot();
  const QPoint empty = plot->axisRect()->rect().bottomLeft() + QPoint(5, -5);
  QMouseEvent press(QEvent::MouseButtonPress, empty, plot->mapToGlobal(empty), Qt::LeftButton,
                    Qt::LeftButton, Qt::NoModifier);
  QCoreApplication::sendEvent(plot, &press);
  QMouseEvent release(QEvent::MouseButtonRelease, empty, plot->mapToGlobal(empty), Qt::LeftButton,
                      Qt::NoButton, Qt::NoModifier);
  QCoreApplication::sendEvent(plot, &release);
}
} // namespace

void TestQCPLegend::clickAwayDeletingTheLegendFromItsDeselectionIsSafe()
{
  mPlot->addGraph();
  mPlot->legend->setSelectableParts(QCPLegend::spLegendBox | QCPLegend::spItems);
  mPlot->legend->setSelectedParts(QCPLegend::spLegendBox);
  bool ran = false;
  connect(mPlot->legend, &QCPLegend::selectionChanged, mPlot, [this, &ran] {
    ran = true;
    delete mPlot->legend;
  }, Qt::SingleShotConnection);
  clickAway(mPlot);
  QVERIFY(ran);
  QCOMPARE(mPlot->legend, nullptr);
}

void TestQCPLegend::clickAwayDeletingTheLegendFromAnItemDeselectionIsSafe()
{
  addSelectedItems(mPlot, 3);
  bool ran = false;
  connect(mPlot->legend->item(0), &QCPAbstractLegendItem::selectionChanged, mPlot, [this, &ran] {
    ran = true;
    delete mPlot->legend;
  }, Qt::SingleShotConnection);
  clickAway(mPlot);
  QVERIFY(ran);
  QCOMPARE(mPlot->legend, nullptr);
}
