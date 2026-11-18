#include "test-qcplegend.h"

namespace {
using namespace QCP::Literals;

//! An icon that names positions in the icon rect only: no data position, so nothing to fit.
class RatioIconPlottable : public QCPPlottable
{
public:
    using QCPPlottable::drawLegendIcon;
    void draw(QCPDrawContext&) const override { }
    void drawLegendIcon(QCPDrawContext& icon) const override
    {
        icon.line(QCP::AxisRatio { 0.5, 1.0 }, QCP::AxisRatio { 0.5, 0.0 }, pen());
    }
};

//! An icon whose only position is a pixel radius around a ratio centre.
class PixelIconPlottable : public QCPPlottable
{
public:
    using QCPPlottable::drawLegendIcon;
    void draw(QCPDrawContext&) const override { }
    void drawLegendIcon(QCPDrawContext& icon) const override
    {
        icon.circle(QCP::AxisRatio { 0.5, 0.5 }, 3_px, pen(), brush());
    }
};

bool isRed(const QColor& colour) { return colour.red() > 150 && colour.green() < 100 && colour.blue() < 100; }

//! The icon rect of the legend's only row, in the plot's pixels.
QRect iconRectOf(QCustomPlot& plot)
{
    auto* item = qobject_cast<QCPPlottableLegendItem*>(plot.legend->item(0));
    return item ? QRect(item->rect().topLeft(), plot.legend->iconSize()) : QRect();
}

int redInColumn(const QImage& frame, const QRect& rect, int x)
{
    int count = 0;
    for (int y = rect.top(); y <= rect.bottom(); ++y)
        if (rect.contains(x, y) && isRed(frame.pixelColor(x, y)))
            ++count;
    return count;
}
} // namespace

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
  mPlot->add<QCPBars>().get();
  QCOMPARE(mPlot->legend->itemCount(), 0);
  
  // legend items should be added if auto-add is on:
  mPlot->setAutoAddPlottableToLegend(true);
  mPlot->addGraph();
  QCOMPARE(mPlot->legend->itemCount(), 1);
  mPlot->addGraph();
  QCOMPARE(mPlot->legend->itemCount(), 2);
  mPlot->add<QCPBars>().get();
  QCOMPARE(mPlot->legend->itemCount(), 3);
}

void TestQCPLegend::addAndRemove()
{
  mPlot->setAutoAddPlottableToLegend(false);
  
  QCPGraph *g1 = mPlot->add<QCPGraph>().get();
  QCPGraph *g2 = mPlot->add<QCPGraph>().get();
  QCPBars *b1 = mPlot->add<QCPBars>().get();
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
  auto *graph = mPlot->add<QCPGraph>().get();
  graph->setName("dimmed");
  graph->addToLegend();
  mPlot->replot(QCustomPlot::rpImmediateRefresh);

  const auto buffer = mPlot->layer("legend")->mPaintBuffer.toStrongRef();
  QVERIFY(buffer);
  QVERIFY(!buffer->contentDirty());

  QCPLegendItem *item = mPlot->legend->item(0);
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
  auto *mg = mPlot->add<QCPGraph>().get();
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
  QCPLegendItem* first = mPlot->legend->item(0);
  connect(first, &QCPLegendItem::selectionChanged, mPlot,
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
  connect(mPlot->legend->item(0), &QCPLegendItem::selectionChanged, mPlot,
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
  connect(mPlot->legend->item(0), &QCPLegendItem::selectionChanged, mPlot, [this, &ran] {
    ran = true;
    delete mPlot->legend;
  }, Qt::SingleShotConnection);
  clickAway(mPlot);
  QVERIFY(ran);
  QCOMPARE(mPlot->legend, nullptr);
}

// A row made for one legend and put into another as a plain element belongs to the one it sits in:
// deleting the first must not leave it drawing through a null legend.
void TestQCPLegend::aRowAddedAsAnElementBelongsToItsNewLegend()
{
  mPlot->setAutoAddPlottableToLegend(false);
  auto *graph = mPlot->add<QCPGraph>().get();
  auto *other = new QCPLegend;
  mPlot->axisRect()->insetLayout()->addElement(other, Qt::AlignTop | Qt::AlignLeft);
  auto *row = new QCPPlottableLegendItem(mPlot->legend, graph);
  QVERIFY(other->addElement(row));
  QCOMPARE(row->parentLegend().get(), other);

  delete mPlot->legend;
  mPlot->replot(QCustomPlot::rpImmediateRefresh);
  QCOMPARE(row->parentLegend().get(), other);
}

// A row taken out of its legend and put into a layout that is not a legend has no legend: it draws
// and lays out as text only, and the legend it left can go.
void TestQCPLegend::aRowTakenOutOfItsLegendOutlivesIt()
{
  mPlot->setAutoAddPlottableToLegend(false);
  auto *graph = mPlot->add<QCPGraph>().get();
  graph->setName("orphan");
  QPointer<QCPLegend> other = new QCPLegend;
  mPlot->axisRect()->insetLayout()->addElement(other, Qt::AlignTop | Qt::AlignLeft);
  auto *row = new QCPPlottableLegendItem(other, graph);
  QVERIFY(other->addItem(row));
  QVERIFY(other->take(row));
  mPlot->axisRect()->insetLayout()->addElement(row, Qt::AlignBottom | Qt::AlignRight);

  delete other.data();
  mPlot->setInteractions(QCP::iSelectLegend);
  mPlot->replot(QCustomPlot::rpImmediateRefresh);
  QVERIFY(!row->parentLegend());
  QVERIFY(static_cast<QCPLayoutElement*>(row)->minimumOuterSizeHint().width() > 0);
  QCOMPARE(row->selectTest(row->rect().center(), true), -1.0);
  row->setSelected(true); // no legend to tell: nothing to dereference either
  mPlot->replot(QCustomPlot::rpImmediateRefresh);
  delete graph;            // the row leaves no legend
  mPlot->replot(QCustomPlot::rpImmediateRefresh);
}

void TestQCPLegend::aSlotDeletingTheLegendARowJoinsIsSafe()
{
  mPlot->setAutoAddPlottableToLegend(false);
  auto *graph = mPlot->add<QCPGraph>().get();
  QPointer<QCPLegend> other = new QCPLegend;
  mPlot->axisRect()->insetLayout()->addElement(other, Qt::AlignTop | Qt::AlignLeft);
  QPointer<QCPLegendItem> row = new QCPPlottableLegendItem(mPlot->legend, graph);
  bool ran = false;
  connect(other, &QCPLayoutGrid::rowStretchFactorsChanged, mPlot, [&] {
    ran = true;
    delete other.data();
  }, Qt::SingleShotConnection);
  other->addElement(row);
  QVERIFY(ran);
  QVERIFY(!other);
  QVERIFY(!row);
  mPlot->replot(QCustomPlot::rpImmediateRefresh);
}

// Removing a row re-wraps the legend: the grid's stretch factors signal once, with the final list,
// and a list that ends where it started does not signal at all. A new graph swaps the base's plain
// row for its group row: that is one row added, so one signal too.
void TestQCPLegend::removingAnItemSignalsTheStretchFactorsOnce()
{
  for (int i = 0; i < 3; ++i)
    mPlot->addGraph();
  QCOMPARE(mPlot->legend->itemCount(), 3);
  QSignalSpy rows(mPlot->legend, &QCPLayoutGrid::rowStretchFactorsChanged);
  QSignalSpy columns(mPlot->legend, &QCPLayoutGrid::columnStretchFactorsChanged);

  QVERIFY(mPlot->legend->removeItem(0));
  QCOMPARE(rows.count(), 1);
  QCOMPARE(rows.at(0).at(0).value<QList<double>>(), (QList<double> { 1, 1 }));
  QCOMPARE(columns.count(), 0);

  rows.clear();
  mPlot->addGraph();
  QCOMPARE(rows.count(), 1);
  QCOMPARE(rows.at(0).at(0).value<QList<double>>(), (QList<double> { 1, 1, 1 }));
  QCOMPARE(columns.count(), 0);
}

void TestQCPLegend::aStretchSlotDeletingTheLegendOnRemovalIsSafe()
{
  for (int i = 0; i < 3; ++i)
    mPlot->addGraph();
  bool ran = false;
  connect(mPlot->legend, &QCPLayoutGrid::rowStretchFactorsChanged, mPlot, [this, &ran] {
    ran = true;
    delete mPlot->legend;
  }, Qt::SingleShotConnection);
  QVERIFY(mPlot->legend->removeItem(0));
  QVERIFY(ran);
  QCOMPARE(mPlot->legend, nullptr);
  mPlot->replot(QCustomPlot::rpImmediateRefresh);
}

void TestQCPLegend::aRatioOnlyIconIsDrawn()
{
    mPlot->resize(400, 300);
    auto* plottable = mPlot->add<RatioIconPlottable>().get();
    plottable->setPen(QPen(Qt::red, 2));
    mPlot->replot();
    const QRect icon = iconRectOf(*mPlot);
    QVERIFY(!icon.isEmpty());
    const QImage frame = mPlot->toPixmap(400, 300).toImage();
    // The half way point of the rect, which is where a ratio of 0.5 lands: not QRect::center(),
    // which rounds the middle of an even width to the left.
    const int middle = icon.left() + icon.width() / 2;
    QVERIFY2(redInColumn(frame, icon, middle) >= icon.height() / 2,
             "the icon's middle column carries the line");
    QCOMPARE(redInColumn(frame, icon, icon.left()), 0);
    QCOMPARE(redInColumn(frame, icon, icon.right()), 0);
}

void TestQCPLegend::aPixelIconSitsInTheIconRect()
{
    mPlot->resize(400, 300);
    auto* plottable = mPlot->add<PixelIconPlottable>().get();
    plottable->setPen(QPen(Qt::red));
    plottable->setBrush(Qt::red);
    mPlot->replot();
    const QRect icon = iconRectOf(*mPlot);
    QVERIFY(!icon.isEmpty());
    const QImage frame = mPlot->toPixmap(400, 300).toImage();
    QRect disc;
    int count = 0;
    for (int x = icon.left(); x <= icon.right(); ++x)
        for (int y = icon.top(); y <= icon.bottom(); ++y)
            if (isRed(frame.pixelColor(x, y)))
            {
                disc = disc.isNull() ? QRect(x, y, 1, 1) : disc.united(QRect(x, y, 1, 1));
                ++count;
            }
    QVERIFY2(count > 10, "the pixel icon is drawn");
    QVERIFY2(qAbs(disc.center().x() - icon.center().x()) <= 1, "the disc sits at the icon's centre");
    QVERIFY2(qAbs(disc.center().y() - icon.center().y()) <= 1, "the disc sits at the icon's centre");
    QVERIFY2(disc.width() >= 5 && disc.width() <= 8, "the disc keeps its 3 px radius");
}
