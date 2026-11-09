#include <QSignalSpy>
#include <QtTest/qtestspontaneevent.h>
#include "test-qcustomplot.h"

void TestQCustomPlot::init()
{
  mPlot = new QCustomPlot(0);
  mPlot->show();
  //QTest::qWait(150);
}

void TestQCustomPlot::cleanup()
{
  delete mPlot;
}

void TestQCustomPlot::interface_plottables()
{
  mPlot->setGeometry(50, 50, 500, 500);
  mPlot->axisRect()->setAutoMargins(QCP::msNone);
  mPlot->axisRect()->setMargins(QMargins(0, 0, 0, 0));
  // set axes so plot coordinates equate pixel coordinates:
  mPlot->xAxis->setRange(mPlot->axisRect()->left(), mPlot->axisRect()->left()+mPlot->axisRect()->width());
  mPlot->yAxis->setRange(mPlot->axisRect()->top(), mPlot->axisRect()->top()+mPlot->axisRect()->height());
  mPlot->yAxis->setRangeReversed(true);
  
  QCPGraph *g1 = new QCPGraph(mPlot->xAxis, mPlot->yAxis);
  QCPGraph *g2 = new QCPGraph(mPlot->xAxis, mPlot->yAxis);
  g1->setData(QVector<double>() << 200 << 300, QVector<double>() << 100 << 120);
  g2->setData(QVector<double>() << 200 << 300, QVector<double>() << 106 << 126);

  QCPBars *b1 = new QCPBars(mPlot->xAxis, mPlot->yAxis);
  b1->setWidth(50);
  b1->setData(QVector<double>() << 250, QVector<double>() << 150);

  // test selectivity of template parameter:
  QCOMPARE(mPlot->plottableAt<QCPGraph>(QPointF(250, 110), false, nullptr), g1);
  QCOMPARE(mPlot->plottableAt<QCPGraph>(QPointF(250, 113), false, nullptr), g2);
  QCOMPARE(mPlot->plottableAt<QCPGraph>(QPointF(250, 130), false, nullptr), static_cast<QCPGraph*>(nullptr));

  QCOMPARE(mPlot->plottableAt<QCPBars>(QPointF(250, 110), false, nullptr), b1);
  QCOMPARE(mPlot->plottableAt<QCPBars>(QPointF(250, 170), false, nullptr), static_cast<QCPBars*>(nullptr));

  // test returned datapoint:
  int index = -1;
  QCOMPARE(mPlot->plottableAt<QCPGraph>(QPointF(180, 100), false, &index), static_cast<QCPGraph*>(nullptr));
  QCOMPARE(index, -1);
  QCOMPARE(mPlot->plottableAt<QCPGraph>(QPointF(200, 100), false, &index), g1);
  QCOMPARE(index, 0);
  QCOMPARE(mPlot->plottableAt<QCPGraph>(QPointF(249, 110), false, &index), g1);
  QCOMPARE(index, 0);
  QCOMPARE(mPlot->plottableAt<QCPGraph>(QPointF(251, 110), false, &index), g1);
  QCOMPARE(index, 1);
  QCOMPARE(mPlot->plottableAt<QCPGraph>(QPointF(300, 120), false, &index), g1);
  QCOMPARE(index, 1);

  /*
  mPlot->replot(QCustomPlot::rpImmediateRefresh);
  QApplication::processEvents();
  QTest::qWait(2000);
  */
}

void TestQCustomPlot::interface_items()
{
  mPlot->setGeometry(50, 50, 500, 500);
  mPlot->axisRect()->setAutoMargins(QCP::msNone);
  mPlot->axisRect()->setMargins(QMargins(0, 0, 0, 0));
  // set axes so plot coordinates equate pixel coordinates:
  mPlot->xAxis->setRange(mPlot->axisRect()->left(), mPlot->axisRect()->left()+mPlot->axisRect()->width());
  mPlot->yAxis->setRange(mPlot->axisRect()->top(), mPlot->axisRect()->top()+mPlot->axisRect()->height());
  mPlot->yAxis->setRangeReversed(true);
  
  QCPItemLine *l1 = new QCPItemLine(mPlot);
  l1->start->setCoords(200, 100);
  l1->end->setCoords(300, 120);
  QCPItemLine *l2 = new QCPItemLine(mPlot);
  l2->start->setCoords(200, 106);
  l2->end->setCoords(300, 126);

  QCPItemTracer *t1 = new QCPItemTracer(mPlot);
  t1->position->setCoords(250, 110);
  t1->setStyle(QCPItemTracer::tsPlus);
  t1->setSize(10);

  // test selectivity of template parameter:
  QCOMPARE(mPlot->itemAt<QCPItemLine>(QPointF(250, 110), false), l1);
  QCOMPARE(mPlot->itemAt<QCPItemLine>(QPointF(250, 113), false), l2);
  QCOMPARE(mPlot->itemAt<QCPItemLine>(QPointF(250, 130), false), static_cast<QCPItemLine*>(nullptr));
  
  QCOMPARE(mPlot->itemAt<QCPItemTracer>(QPointF(250, 110), false), t1);
  QCOMPARE(mPlot->itemAt<QCPItemTracer>(QPointF(250, 130), false), static_cast<QCPItemTracer*>(nullptr));

  /*
  mPlot->replot(QCustomPlot::rpImmediateRefresh);
  QApplication::processEvents();
  QTest::qWait(2000);
  */
}

void TestQCustomPlot::rescaleAxes_GraphVisibility()
{
  mPlot->setGeometry(50, 50, 500, 500);
  
  mPlot->addGraph();
  mPlot->addGraph();
  mPlot->graph(0)->setData(QVector<double>()<<4<<5<<6, QVector<double>()<<-2<<-1<<0);
  mPlot->graph(1)->setData(QVector<double>()<<3<<5<<7, QVector<double>()<<-3<<-1<<1);
  mPlot->graph(1)->setVisible(false);
  
  // test rescaling with respect to all graphs, even invisible ones:
  mPlot->rescaleAxes(false);
  QCOMPARE(mPlot->xAxis->range().lower, 3.0);
  QCOMPARE(mPlot->xAxis->range().upper, 7.0);
  QCOMPARE(mPlot->yAxis->range().lower, -3.0);
  QCOMPARE(mPlot->yAxis->range().upper, 1.0);
  
  // test rescaling with respect to all visible graphs:
  mPlot->rescaleAxes(true);
  QCOMPARE(mPlot->xAxis->range().lower, 4.0);
  QCOMPARE(mPlot->xAxis->range().upper, 6.0);
  QCOMPARE(mPlot->yAxis->range().lower, -2.0);
  QCOMPARE(mPlot->yAxis->range().upper, 0.0);
}

void TestQCustomPlot::rescaleAxes_FlatGraph()
{
  // test rescaling if only one graph is in plot and it's flat:
  mPlot->xAxis->setRange(10, 11);
  mPlot->yAxis->setRange(10, 11);
  mPlot->addGraph();
  mPlot->graph(0)->setData(QVector<double>()<<2<<2<<2, QVector<double>()<<0<<0<<0);
  mPlot->rescaleAxes();
  QCOMPARE(mPlot->xAxis->range().lower, 1.5);
  QCOMPARE(mPlot->xAxis->range().upper, 2.5);
  QCOMPARE(mPlot->yAxis->range().lower, -0.5);
  QCOMPARE(mPlot->yAxis->range().upper, 0.5);
  
  // test rescaling when there are multiple graphs with different values but first graph is flat:
  mPlot->xAxis->setRange(10, 11);
  mPlot->yAxis->setRange(10, 11);
  mPlot->addGraph();
  mPlot->graph(1)->setData(QVector<double>()<<-1<<0<<1, QVector<double>()<<-0.1<<0<<0.1);
  mPlot->rescaleAxes();
  QCOMPARE(mPlot->xAxis->range().lower, -1.0);
  QCOMPARE(mPlot->xAxis->range().upper, 2.0);
  QCOMPARE(mPlot->yAxis->range().lower, -0.1);
  QCOMPARE(mPlot->yAxis->range().upper, 0.1);
}

void TestQCustomPlot::rescaleAxes_MultipleFlatGraphs()
{
  // test rescaling when there are multiple flat graphs:
  mPlot->addGraph();
  mPlot->addGraph();
  mPlot->graph(0)->setData(QVector<double>()<<1<<2<<3, QVector<double>()<<0<<0<<0);
  mPlot->graph(1)->setData(QVector<double>()<<-1<<0<<1, QVector<double>()<<2<<2<<2);

  mPlot->rescaleAxes();

  QCOMPARE(mPlot->xAxis->range().lower, -1.0);
  QCOMPARE(mPlot->xAxis->range().upper, 3.0);
  QCOMPARE(mPlot->yAxis->range().lower, 0.0);
  QCOMPARE(mPlot->yAxis->range().upper, 2.0);
}

void TestQCustomPlot::calculateMargin_staleTickVectors()
{
  // Bug: calculateMargin() and draw() iterate mTickVector and access
  // mTickVectorLabels[i] without bounds checking. The vectors can desync
  // when setTickLabels(false) clears labels, a replot regenerates ticks
  // (but skips labels since mTickLabels is false), then setTickLabels(true)
  // re-enables label access without regenerating them.
  // Without the bounds check, this crashes with QVector::at() out of bounds.
  mPlot->setGeometry(50, 50, 500, 500);
  mPlot->yAxis->setRange(1, 100);

  // Replot populates mTickVector and mTickVectorLabels in sync
  mPlot->replot(QCustomPlot::rpImmediateRefresh);

  // Disable tick labels — this clears mTickVectorLabels (axis.cpp line 852)
  mPlot->yAxis->setTickLabels(false);

  // Replot regenerates mTickVector via setupTickVectors(), but does NOT
  // regenerate mTickVectorLabels (nullptr passed to generate() since
  // mTickLabels is false). Now mTickVector has entries, mTickVectorLabels is empty.
  mPlot->replot(QCustomPlot::rpImmediateRefresh);

  // Re-enable tick labels WITHOUT a replot. mTickLabels is now true,
  // but mTickVectorLabels is still empty from the clear above.
  mPlot->yAxis->setTickLabels(true);

  // calculateMargin iterates mTickVector.size() and accesses mTickVectorLabels[i].
  // Without bounds check: crash. With bounds check: safe.
  for (auto side : {QCP::msLeft, QCP::msRight, QCP::msTop, QCP::msBottom})
      mPlot->axisRect()->calculateAutoMargin(side);

  // If we reach here without crashing, the bounds check works
}

void TestQCustomPlot::dateTimeTicker_extremeZoomOutNoCrash()
{
    // Reproducer: on extreme zoom-out, tickStep exceeds int range and
    // qRound() (Qt 6.10+) asserts in qCheckedFPConversionToInteger.
    mPlot->setGeometry(50, 50, 500, 500);
    auto ticker = QSharedPointer<QCPAxisTickerDateTime>::create();
    mPlot->xAxis->setTicker(ticker);
    // Range spanning ~6000 years in seconds — triggers huge tickStep values
    mPlot->xAxis->setRange(0, 2e11);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    // If we reach here without asserting, the guard works
}

// QCPSelectionRect cancels on Escape, but QCustomPlot::keyPressEvent never passed the key on, so
// a rubber band being dragged could not be cancelled.
void TestQCustomPlot::escapeCancelsTheSelectionRect()
{
  mPlot->resize(400, 300);
  mPlot->setSelectionRectMode(QCP::srmZoom);
  mPlot->replot();
  QSignalSpy canceled(mPlot->selectionRect(), &QCPSelectionRect::canceled);
  const QPoint start = mPlot->axisRect()->rect().center();
  QMouseEvent press(QEvent::MouseButtonPress, start, mPlot->mapToGlobal(start), Qt::LeftButton,
                    Qt::LeftButton, Qt::NoModifier);
  QCoreApplication::sendEvent(mPlot, &press);
  QVERIFY(mPlot->selectionRect()->isActive());

  QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QCoreApplication::sendEvent(mPlot, &escape);
  QCOMPARE(canceled.count(), 1);
  QVERIFY(!mPlot->selectionRect()->isActive());
}

namespace {
void sendMouse(QCustomPlot* plot, QEvent::Type type, QPoint pos, Qt::MouseButtons buttons,
               Qt::KeyboardModifiers mods = Qt::NoModifier)
{
  QMouseEvent e(type, pos, plot->mapToGlobal(pos), Qt::LeftButton, buttons, mods);
  QCoreApplication::sendEvent(plot, &e);
}

void clickAt(QCustomPlot* plot, QPoint pos)
{
  sendMouse(plot, QEvent::MouseButtonPress, pos, Qt::LeftButton);
  sendMouse(plot, QEvent::MouseButtonRelease, pos, Qt::NoButton);
}

void dragFromTo(QCustomPlot* plot, QPoint from, QPoint to)
{
  sendMouse(plot, QEvent::MouseButtonPress, from, Qt::LeftButton);
  sendMouse(plot, QEvent::MouseMove, to, Qt::LeftButton);
  sendMouse(plot, QEvent::MouseButtonRelease, to, Qt::NoButton);
}

//! A flat graph at value 5 over keys [2, 8], axes [0, 10], laid out.
QCPGraph* flatGraph(QCustomPlot* plot, double value = 5)
{
  auto* g = new QCPGraph(plot->xAxis, plot->yAxis);
  g->setData(QVector<double>{ 2, 5, 8 }, QVector<double>{ value, value, value });
  return g;
}

void layOut(QCustomPlot* plot, QCP::Interactions interactions)
{
  plot->resize(400, 300);
  plot->xAxis->setRange(0, 10);
  plot->yAxis->setRange(0, 10);
  plot->setInteractions(interactions);
  plot->replot();
}

QPoint pixelOf(QCustomPlot* plot, double key, double value)
{
  return QPoint(qRound(plot->xAxis->coordToPixel(key)), qRound(plot->yAxis->coordToPixel(value)));
}
} // namespace

// A slot deleting the plottable from its own selection signal: QCP must not touch it afterwards.
void TestQCustomPlot::clickSlotDeletingTheGraphIsSafe()
{
  layOut(mPlot, QCP::iSelectPlottables);
  auto* g = flatGraph(mPlot);
  connect(g, &QCPAbstractPlottable::selectionChanged, mPlot,
          [this, g] { QVERIFY(mPlot->removePlottable(g)); });
  clickAt(mPlot, pixelOf(mPlot, 5, 5));
  QCOMPARE(mPlot->plottableCount(), 0);
}

void TestQCustomPlot::deselectSlotDeletingTheClickedGraphIsSafe()
{
  layOut(mPlot, QCP::iSelectPlottables);
  auto* selected = flatGraph(mPlot, 2);
  auto* clicked = flatGraph(mPlot, 8);
  selected->setSelection(QCPDataSelection(QCPDataRange(0, 3)));
  connect(selected, &QCPAbstractPlottable::selectionChanged, mPlot,
          [this, clicked] { QVERIFY(mPlot->removePlottable(clicked)); });
  clickAt(mPlot, pixelOf(mPlot, 5, 8));
  QCOMPARE(mPlot->plottableCount(), 1);
  QVERIFY(!selected->selected());
}

void TestQCustomPlot::rectDeselectSlotDeletingACandidateIsSafe()
{
  layOut(mPlot, QCP::iSelectPlottables);
  mPlot->setSelectionRectMode(QCP::srmSelect);
  auto* selected = flatGraph(mPlot, 2);
  auto* candidate = flatGraph(mPlot, 8);
  selected->setSelection(QCPDataSelection(QCPDataRange(0, 3)));
  connect(selected, &QCPAbstractPlottable::selectionChanged, mPlot,
          [this, candidate] { QVERIFY(mPlot->removePlottable(candidate)); });
  dragFromTo(mPlot, pixelOf(mPlot, 1, 9), pixelOf(mPlot, 9, 7));
  QCOMPARE(mPlot->plottableCount(), 1);
}

void TestQCustomPlot::clickSlotDeletingTheItemIsSafe()
{
  layOut(mPlot, QCP::iSelectItems);
  auto* line = new QCPItemLine(mPlot);
  line->start->setCoords(2, 5);
  line->end->setCoords(8, 5);
  connect(line, &QCPAbstractItem::selectionChanged, mPlot,
          [this, line] { QVERIFY(mPlot->removeItem(line)); });
  clickAt(mPlot, pixelOf(mPlot, 5, 5));
  QCOMPARE(mPlot->itemCount(), 0);
}

void TestQCustomPlot::deleteKeySlotDeletingEveryItemIsSafe()
{
  layOut(mPlot, QCP::iSelectItems);
  for (int i = 0; i < 3; ++i)
  {
    auto* line = new QCPItemLine(mPlot);
    line->setSelected(true);
    connect(line, &QCPAbstractItem::deleteRequested, mPlot, [this] { mPlot->clearItems(); });
  }
  QKeyEvent del(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
  QCoreApplication::sendEvent(mPlot, &del);
  QCOMPARE(mPlot->itemCount(), 0);
}

namespace {
void startRubberBand(QCustomPlot* plot)
{
  layOut(plot, QCP::Interactions());
  plot->setSelectionRectMode(QCP::srmZoom);
  sendMouse(plot, QEvent::MouseButtonPress, pixelOf(plot, 2, 2), Qt::LeftButton);
}

void pressEscape(QCustomPlot* plot)
{
  QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QCoreApplication::sendEvent(plot, &escape);
}

template <typename Signal>
void replaceRectOn(QCustomPlot* plot, Signal signal)
{
  QObject::connect(plot->selectionRect(), signal, plot,
                   [plot] { plot->setSelectionRect(new QCPSelectionRect(plot)); },
                   Qt::SingleShotConnection);
}

template <typename Signal>
void deleteRectOn(QCustomPlot* plot, Signal signal)
{
  QObject::connect(plot->selectionRect(), signal, plot, [plot] { delete plot->selectionRect(); },
                   Qt::SingleShotConnection);
}
} // namespace

// QCustomPlot::setSelectionRect deletes the rect a slot replaces: the rect must not repaint
// through itself after its own signal.
void TestQCustomPlot::moveSlotReplacingTheSelectionRectIsSafe()
{
  startRubberBand(mPlot);
  auto* old = mPlot->selectionRect();
  replaceRectOn(mPlot, &QCPSelectionRect::changed);
  sendMouse(mPlot, QEvent::MouseMove, pixelOf(mPlot, 8, 8), Qt::LeftButton);
  sendMouse(mPlot, QEvent::MouseMove, pixelOf(mPlot, 9, 9), Qt::LeftButton);
  sendMouse(mPlot, QEvent::MouseButtonRelease, pixelOf(mPlot, 9, 9), Qt::NoButton);
  QVERIFY(mPlot->selectionRect() != old);
  QVERIFY(!mPlot->selectionRect()->isActive());
}

// A click (no drag) cancels the band on release.
void TestQCustomPlot::cancelSlotReplacingTheSelectionRectIsSafe()
{
  startRubberBand(mPlot);
  auto* old = mPlot->selectionRect();
  replaceRectOn(mPlot, &QCPSelectionRect::canceled);
  sendMouse(mPlot, QEvent::MouseButtonRelease, pixelOf(mPlot, 2, 2), Qt::NoButton);
  QVERIFY(mPlot->selectionRect() != old);
  QVERIFY(!mPlot->selectionRect()->isActive());
}

void TestQCustomPlot::escapeSlotReplacingTheSelectionRectIsSafe()
{
  startRubberBand(mPlot);
  auto* old = mPlot->selectionRect();
  replaceRectOn(mPlot, &QCPSelectionRect::canceled);
  pressEscape(mPlot);
  QVERIFY(mPlot->selectionRect() != old);
  QVERIFY(!mPlot->selectionRect()->isActive());
}

// Deleting the rect outright leaves the plot without one, not with a dangling pointer.
void TestQCustomPlot::moveSlotDeletingTheSelectionRectIsSafe()
{
  startRubberBand(mPlot);
  deleteRectOn(mPlot, &QCPSelectionRect::changed);
  sendMouse(mPlot, QEvent::MouseMove, pixelOf(mPlot, 8, 8), Qt::LeftButton);
  QCOMPARE(mPlot->selectionRect(), nullptr);
  sendMouse(mPlot, QEvent::MouseMove, pixelOf(mPlot, 9, 9), Qt::LeftButton);
  sendMouse(mPlot, QEvent::MouseButtonRelease, pixelOf(mPlot, 9, 9), Qt::NoButton);
  pressEscape(mPlot);
}

void TestQCustomPlot::escapeSlotDeletingTheSelectionRectIsSafe()
{
  startRubberBand(mPlot);
  deleteRectOn(mPlot, &QCPSelectionRect::canceled);
  pressEscape(mPlot);
  QCOMPARE(mPlot->selectionRect(), nullptr);
}

// Replacing a rect mid-drag cancels it from its destructor; a canceled slot replacing it again
// must not delete it a second time.
void TestQCustomPlot::replacingADraggedSelectionRectFromItsCancelSlotIsSafe()
{
  startRubberBand(mPlot);
  auto* last = new QCPSelectionRect(mPlot);
  connect(mPlot->selectionRect(), &QCPSelectionRect::canceled, mPlot,
          [this, last] { mPlot->setSelectionRect(last); }, Qt::SingleShotConnection);
  mPlot->setSelectionRect(new QCPSelectionRect(mPlot));
  QCOMPARE(mPlot->selectionRect(), last);
}

// A rect being destroyed must not signal: a canceled slot would see it as the plot's current rect.
void TestQCustomPlot::deletingADraggedSelectionRectDoesNotSignal()
{
  startRubberBand(mPlot);
  int canceled = 0;
  connect(mPlot->selectionRect(), &QCPSelectionRect::canceled, mPlot, [&canceled] { ++canceled; });
  deleteRectOn(mPlot, &QCPSelectionRect::changed);
  sendMouse(mPlot, QEvent::MouseMove, pixelOf(mPlot, 8, 8), Qt::LeftButton);
  QCOMPARE(canceled, 0);
  QCOMPARE(mPlot->selectionRect(), nullptr);
  sendMouse(mPlot, QEvent::MouseButtonRelease, pixelOf(mPlot, 8, 8), Qt::NoButton);
}

// Before: the destructor's canceled slot installed a rect, which deleted the dying one again.
void TestQCustomPlot::cancelSlotInstallingARectWhileTheDraggedOneIsDeletedIsSafe()
{
  startRubberBand(mPlot);
  replaceRectOn(mPlot, &QCPSelectionRect::canceled);
  deleteRectOn(mPlot, &QCPSelectionRect::changed);
  sendMouse(mPlot, QEvent::MouseMove, pixelOf(mPlot, 8, 8), Qt::LeftButton);
  sendMouse(mPlot, QEvent::MouseButtonRelease, pixelOf(mPlot, 8, 8), Qt::NoButton);
  QCOMPARE(mPlot->selectionRect(), nullptr);
}

// Once: the rect was deleted (no zoom), or its accepted signal connected twice (two zooms).
void TestQCustomPlot::settingTheCurrentSelectionRectKeepsIt()
{
  layOut(mPlot, QCP::Interactions());
  mPlot->setSelectionRectMode(QCP::srmZoom);
  auto* rect = mPlot->selectionRect();
  mPlot->setSelectionRect(rect);
  QCOMPARE(mPlot->selectionRect(), rect);
  QSignalSpy zooms(mPlot->xAxis, &QCPAxis::rangeChanged);
  dragFromTo(mPlot, pixelOf(mPlot, 2, 2), pixelOf(mPlot, 6, 6));
  QCOMPARE(zooms.count(), 1);
}

// Before: the nested setSelectionRect connected the reinstalled rect's accepted signal again.
void TestQCustomPlot::cancelSlotReinstallingThePreviousRectZoomsOnce()
{
  startRubberBand(mPlot);
  auto* original = mPlot->selectionRect();
  connect(original, &QCPSelectionRect::canceled, mPlot,
          [this, original] { mPlot->setSelectionRect(original); }, Qt::SingleShotConnection);
  mPlot->setSelectionRect(new QCPSelectionRect(mPlot));
  QCOMPARE(mPlot->selectionRect(), original);
  sendMouse(mPlot, QEvent::MouseButtonRelease, pixelOf(mPlot, 2, 2), Qt::NoButton);
  QSignalSpy zooms(mPlot->xAxis, &QCPAxis::rangeChanged);
  dragFromTo(mPlot, pixelOf(mPlot, 2, 2), pixelOf(mPlot, 6, 6));
  QCOMPARE(zooms.count(), 1);
}

void TestQCustomPlot::cancelSlotDeletingThePlotWhileSettingARectIsSafe()
{
  startRubberBand(mPlot);
  connect(mPlot->selectionRect(), &QCPSelectionRect::canceled, this, [this] {
    delete mPlot;
    mPlot = nullptr;
  }, Qt::SingleShotConnection);
  mPlot->setSelectionRect(new QCPSelectionRect(mPlot));
  QCOMPARE(mPlot, nullptr);
}

void TestQCustomPlot::cancelSlotDeletingThePlotWhileClearingTheRectModeIsSafe()
{
  startRubberBand(mPlot);
  connect(mPlot->selectionRect(), &QCPSelectionRect::canceled, this, [this] {
    delete mPlot;
    mPlot = nullptr;
  }, Qt::SingleShotConnection);
  mPlot->setSelectionRectMode(QCP::srmNone);
  QCOMPARE(mPlot, nullptr);
}

namespace {
// Closing a panel from a plot signal deletes the plot while its event handler is on the stack.
template <typename Sender, typename Signal>
void deletePlotOn(QCustomPlot*& plot, Sender* sender, Signal signal, bool* ran)
{
  QObject::connect(sender, signal, sender, [&plot, ran] {
    *ran = true;
    delete plot;
    plot = nullptr;
  }, Qt::SingleShotConnection);
}
} // namespace

void TestQCustomPlot::clickSelectionSlotDeletingThePlotIsSafe()
{
  layOut(mPlot, QCP::iSelectPlottables);
  flatGraph(mPlot);
  bool ran = false;
  deletePlotOn(mPlot, mPlot, &QCustomPlot::selectionChangedByUser, &ran);
  clickAt(mPlot, pixelOf(mPlot, 5, 5));
  QVERIFY(ran);
}

void TestQCustomPlot::rectSelectionSlotDeletingThePlotIsSafe()
{
  layOut(mPlot, QCP::iSelectPlottables);
  mPlot->setSelectionRectMode(QCP::srmSelect);
  flatGraph(mPlot);
  bool ran = false;
  deletePlotOn(mPlot, mPlot, &QCustomPlot::selectionChangedByUser, &ran);
  dragFromTo(mPlot, pixelOf(mPlot, 1, 9), pixelOf(mPlot, 9, 1));
  QVERIFY(ran);
}

void TestQCustomPlot::rectZoomSlotDeletingThePlotIsSafe()
{
  layOut(mPlot, QCP::Interactions());
  mPlot->setSelectionRectMode(QCP::srmZoom);
  bool ran = false;
  deletePlotOn(mPlot, mPlot->xAxis, &QCPAxis::rangeChanged, &ran);
  dragFromTo(mPlot, pixelOf(mPlot, 2, 2), pixelOf(mPlot, 6, 6));
  QVERIFY(ran);
}

void TestQCustomPlot::releaseSlotDeletingThePlotIsSafe()
{
  layOut(mPlot, QCP::iSelectPlottables);
  bool ran = false;
  deletePlotOn(mPlot, mPlot, &QCustomPlot::mouseRelease, &ran);
  clickAt(mPlot, pixelOf(mPlot, 5, 5));
  QVERIFY(ran);
}

void TestQCustomPlot::wheelSlotDeletingThePlotIsSafe()
{
  layOut(mPlot, QCP::iRangeZoom);
  bool ran = false;
  deletePlotOn(mPlot, mPlot, &QCustomPlot::mouseWheel, &ran);
  const QPointF pos = pixelOf(mPlot, 5, 5);
  QWheelEvent event(pos, mPlot->mapToGlobal(pos), QPoint(), QPoint(0, 120), Qt::NoButton,
                    Qt::NoModifier, Qt::NoScrollPhase, false);
  // Spontaneous, like a real wheel: sendEvent() would take QApplication's non-propagating path.
  QSpontaneKeyEvent::setSpontaneous(&event);
  QCoreApplication::instance()->notify(mPlot, &event);
  QVERIFY(ran);
}

// Through the window, as a real click: QWidgetWindow dispatches it and forwards the right button
// as a context-menu event.
void TestQCustomPlot::rightPressSlotDeletingThePlotIsSafe()
{
  QWidget panel;
  panel.resize(400, 300);
  mPlot->setParent(&panel);
  mPlot->setGeometry(0, 0, 400, 300);
  panel.show();
  QVERIFY(QTest::qWaitForWindowExposed(&panel));
  bool ran = false;
  deletePlotOn(mPlot, mPlot, &QCustomPlot::mousePress, &ran);
  QTest::mouseClick(panel.windowHandle(), Qt::RightButton, Qt::NoModifier, QPoint(200, 150));
  if (mPlot)
    mPlot->setParent(nullptr); // cleanup() deletes it, not the panel
  QVERIFY(ran);
}

void TestQCustomPlot::rightPressSlotDeletingATopLevelPlotIsSafe()
{
  QVERIFY(QTest::qWaitForWindowExposed(mPlot));
  bool ran = false;
  deletePlotOn(mPlot, mPlot, &QCustomPlot::mousePress, &ran);
  QTest::mouseClick(mPlot->windowHandle(), Qt::RightButton, Qt::NoModifier, QPoint(20, 20));
  QVERIFY(ran);
}

namespace {
// A slot of rangeChanged deletes the owner of the axis: nothing of that chain may reach it.
template <typename Axis>
void deleteOwnerOnRangeChange(Axis* axis, QObject* owner)
{
  QObject::connect(axis, &Axis::rangeChanged, owner, [owner] { delete owner; });
}

//! A polar area filling the widget, with radial axes spanning [0, 10], [0, 100] and [0, 1000].
QCPPolarArea* polarPlot(QCustomPlot* plot, QList<QCPPolarAxisRadial*>& radials)
{
  plot->plotLayout()->clear();
  auto* area = new QCPPolarArea(plot);
  plot->plotLayout()->addElement(0, 0, area);
  radials = { area->radialAxis(), area->addRadialAxis(), area->addRadialAxis() };
  for (int i = 0; i < radials.size(); ++i)
    radials[i]->setRange(0, std::pow(10.0, i + 1));
  plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
  plot->resize(400, 400);
  plot->replot();
  return area;
}

//! A point straight above the centre, at the fraction of the radius, away from the radial axes.
QPoint polarPoint(QCPPolarArea* area, double fraction)
{
  return (area->center() - QPointF(0, fraction * area->radius())).toPoint();
}

void dragRadially(QCustomPlot* plot, QCPPolarArea* area)
{
  const QPoint from = polarPoint(area, 0.6), to = polarPoint(area, 0.3);
  sendMouse(plot, QEvent::MouseButtonPress, from, Qt::LeftButton);
  sendMouse(plot, QEvent::MouseMove, to, Qt::LeftButton);
  sendMouse(plot, QEvent::MouseButtonRelease, to, Qt::NoButton);
}

void wheelOnPolar(QCustomPlot* plot, QCPPolarArea* area)
{
  const QPointF pos = polarPoint(area, 0.5);
  QWheelEvent event(pos, plot->mapToGlobal(pos), QPoint(), QPoint(0, 120), Qt::NoButton,
                    Qt::NoModifier, Qt::NoScrollPhase, false);
  QCoreApplication::sendEvent(plot, &event);
}

template <typename Slot>
void onFirstRangeChange(QCPPolarAxisRadial* axis, QObject* context, Slot slot)
{
  QObject::connect(axis, &QCPPolarAxisRadial::rangeChanged, context,
                   slot, Qt::SingleShotConnection);
}
} // namespace

void TestQCustomPlot::angularRangeSlotDeletingTheAreaIsSafe()
{
  QList<QCPPolarAxisRadial*> radials;
  auto* area = polarPlot(mPlot, radials);
  const QPointer<QCPPolarArea> alive(area);
  auto* angular = area->angularAxis();
  deleteOwnerOnRangeChange(angular, area);
  angular->setRange(0, 180);
  QVERIFY(!alive);
}

void TestQCustomPlot::radialRangeSlotDeletingTheAreaIsSafe()
{
  for (bool viaScaleType : { false, true })
  {
    QList<QCPPolarAxisRadial*> radials;
    auto* area = polarPlot(mPlot, radials);
    auto* radial = area->radialAxis();
    radial->setRange(-1, 10);
    const QPointer<QCPPolarArea> alive(area);
    int scaleTypes = 0;
    connect(radial, &QCPPolarAxisRadial::scaleTypeChanged, area, [&scaleTypes] { ++scaleTypes; });
    deleteOwnerOnRangeChange(radial, area);
    if (viaScaleType)
      radial->setScaleType(QCPPolarAxisRadial::stLogarithmic); // sanitises the range first
    else
      radial->setRange(2, 3);
    QVERIFY(!alive);
    QCOMPARE(scaleTypes, 0);
  }
}

// Before: the press-time baselines were index-parallel to the shrunken axis list.
void TestQCustomPlot::radialDragSlotRemovingARadialAxisKeepsTheOthersBaselines()
{
  QList<QCPPolarAxisRadial*> radials;
  auto* area = polarPlot(mPlot, radials);
  onFirstRangeChange(radials[0], area, [area, first = radials[0]] {
    area->removeRadialAxis(first);
  });
  dragRadially(mPlot, area);
  QCOMPARE(area->radialAxisCount(), 2);
  QCOMPARE(radials[1]->range().size(), 100.0);
  QCOMPARE(radials[2]->range().size(), 1000.0);
  QVERIFY(radials[1]->range().lower != 0);
}

void TestQCustomPlot::radialDragSlotDeletingTheAreaIsSafe()
{
  QList<QCPPolarAxisRadial*> radials;
  QPointer<QCPPolarArea> area = polarPlot(mPlot, radials);
  onFirstRangeChange(radials[0], mPlot, [this, area] {
    mPlot->plotLayout()->remove(area);
  });
  dragRadially(mPlot, area);
  QVERIFY(!area);
}

// Before: removing an axis shifted the list under the loop, so the next axis was skipped.
void TestQCustomPlot::radialWheelSlotRemovingARadialAxisZoomsTheOthers()
{
  QList<QCPPolarAxisRadial*> radials;
  auto* area = polarPlot(mPlot, radials);
  onFirstRangeChange(radials[0], area, [area, first = radials[0]] {
    area->removeRadialAxis(first);
  });
  wheelOnPolar(mPlot, area);
  QCOMPARE(area->radialAxisCount(), 2);
  QVERIFY(radials[1]->range().size() != 100.0);
  QVERIFY(radials[2]->range().size() != 1000.0);
}

void TestQCustomPlot::radialWheelSlotDeletingTheAreaIsSafe()
{
  QList<QCPPolarAxisRadial*> radials;
  QPointer<QCPPolarArea> area = polarPlot(mPlot, radials);
  onFirstRangeChange(radials[0], mPlot, [this, area] {
    mPlot->plotLayout()->remove(area);
  });
  wheelOnPolar(mPlot, area);
  QVERIFY(!area);
}

// Before: the radial loop copied the deleted angular axis's lists after its own range change.
void TestQCustomPlot::angularDragSlotDeletingTheAreaIsSafe()
{
  QList<QCPPolarAxisRadial*> radials;
  QPointer<QCPPolarArea> area = polarPlot(mPlot, radials);
  QCPPolarAxisAngular* angular = area->angularAxis();
  angular->setRangeDraggable(true);
  connect(angular, &QCPPolarAxisAngular::rangeChanged, mPlot,
          [this, area] { mPlot->plotLayout()->remove(area); }, Qt::SingleShotConnection);
  const QPoint from = polarPoint(area, 0.6);
  const QPoint to = (area->center() + QPointF(0.6 * area->radius(), 0)).toPoint();
  sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);
  sendMouse(mPlot, QEvent::MouseMove, to, Qt::LeftButton);
  sendMouse(mPlot, QEvent::MouseButtonRelease, to, Qt::NoButton);
  QVERIFY(!area);
}

void TestQCustomPlot::angularWheelSlotDeletingTheAreaIsSafe()
{
  QList<QCPPolarAxisRadial*> radials;
  QPointer<QCPPolarArea> area = polarPlot(mPlot, radials);
  QCPPolarAxisAngular* angular = area->angularAxis();
  angular->setRangeZoomable(true);
  connect(angular, &QCPPolarAxisAngular::rangeChanged, mPlot,
          [this, area] { mPlot->plotLayout()->remove(area); }, Qt::SingleShotConnection);
  wheelOnPolar(mPlot, area);
  QVERIFY(!area);
}

// Before: only the axis's release handler restored it, so the plot stayed non-antialiased.
void TestQCustomPlot::radialDragSlotDeletingTheAreaRestoresAntialiasing()
{
  QList<QCPPolarAxisRadial*> radials;
  QPointer<QCPPolarArea> area = polarPlot(mPlot, radials);
  mPlot->setAntialiasedElements(QCP::aePlottables);
  const auto on = mPlot->antialiasedElements(), off = mPlot->notAntialiasedElements();
  int changes = 0; // the first move turns antialiasing off, the second deletes the area
  connect(radials[0], &QCPPolarAxisRadial::rangeChanged, mPlot,
          [this, area, &changes] {
            if (++changes == 2)
              mPlot->plotLayout()->remove(area);
          });
  const QPoint from = polarPoint(area, 0.6);
  sendMouse(mPlot, QEvent::MouseButtonPress, from, Qt::LeftButton);
  sendMouse(mPlot, QEvent::MouseMove, polarPoint(area, 0.4), Qt::LeftButton);
  sendMouse(mPlot, QEvent::MouseMove, polarPoint(area, 0.3), Qt::LeftButton);
  sendMouse(mPlot, QEvent::MouseButtonRelease, from, Qt::NoButton);
  QVERIFY(!area);
  QCOMPARE(mPlot->antialiasedElements().toInt(), on.toInt());
  QCOMPARE(mPlot->notAntialiasedElements().toInt(), off.toInt());
}
