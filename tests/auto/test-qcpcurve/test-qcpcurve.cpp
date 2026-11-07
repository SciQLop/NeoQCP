#include "test-qcpcurve.h"
#include "../misuse-recorder.h"
#include "painting/draw-commands.h"
#include "vector2d.h"
#include <QMainWindow>
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <set>
#include <vector>

namespace
{
//! A curve that says what it recorded: mCore is the adapter's, protected.
class InspectableCurve : public QCPCurve
{
public:
  using QCPCurve::QCPCurve;

  const QCPDrawList& recording() const { return mCore.recordedList(); }

  int polylineNaNs() const { return mCore.recordedPolylineNaNs(); }
};

InspectableCurve* inspectable(QCPCurve* curve)
{
  return static_cast<InspectableCurve*>(curve);
}

//! Five points, keys out of order in x but sorted by t, as a curve may be.
void setFivePoints(QCPCurve* curve)
{
  curve->setData(QVector<double> { 0, 1, 2, 3, 4 }, QVector<double> { 0, 1, 0, 2, 3 });
}

std::vector<const qcp::PolylineDataCmd*> polylines(const QCPDrawList& list)
{
  std::vector<const qcp::PolylineDataCmd*> lines;
  for (const qcp::QCPDrawCommand& command : list.commands)
      if (const auto* line = std::get_if<qcp::PolylineDataCmd>(&command))
          lines.push_back(line);
  return lines;
}

int polygonCount(const QCPDrawList& list)
{
  return static_cast<int>(std::count_if(
      list.commands.begin(), list.commands.end(), [](const qcp::QCPDrawCommand& command)
      { return std::holds_alternative<qcp::PolygonCmd>(command); }));
}

QImage renderOf(QCustomPlot* plot)
{
  return plot->toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
}

//! A setter every drawing path must honour: the layer it dirties and the pixels it changes.
struct CurveSetterCase
{
  QString name;
  std::function<void(QCPCurve*)> prepare;
  std::function<void(QCPCurve*)> change;
};

const std::vector<CurveSetterCase>& curveSetterCases()
{
  const auto markersOn = [](QCPCurve* curve)
  { curve->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 7)); };
  static const std::vector<CurveSetterCase> cases = {
      { QStringLiteral("setPen"),
        { },
        [](QCPCurve* curve) { curve->setPen(QPen(Qt::darkGreen, 4)); } },
      { QStringLiteral("setBrush"),
        { },
        [](QCPCurve* curve) { curve->setBrush(QBrush(QColor(0, 160, 255, 120))); } },
      { QStringLiteral("setLineStyle"),
        { },
        [](QCPCurve* curve) { curve->setLineStyle(QCPCurve::lsNone); } },
      { QStringLiteral("setScatterStyle"),
        { },
        [](QCPCurve* curve)
        { curve->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 9)); } },
      { QStringLiteral("setScatterSkip"), markersOn,
        [](QCPCurve* curve) { curve->setScatterSkip(1); } },
      { QStringLiteral("setColorValues"),
        { },
        [](QCPCurve* curve)
        {
            std::vector<double> values(static_cast<size_t>(curve->data()->size()));
            for (size_t i = 0; i < values.size(); ++i)
                values[i] = static_cast<double>(i);
            curve->setColorValues(std::move(values));
        } },
  };
  return cases;
}
} // namespace

void TestQCPCurve::init()
{
  mPlot = new QCustomPlot(0);
  mPlot->resize(400, 300);
  mCurve = new InspectableCurve(mPlot->xAxis, mPlot->yAxis);
}

void TestQCPCurve::cleanup()
{
  delete mPlot;
}

void TestQCPCurve::dataManipulation()
{
  QVERIFY(mCurve->data()->isEmpty());
  
  QVector<double> t, x, y;
  t << -1 << 1 << -2 << 2;
  x <<  1 << 2 << 3 << 4;
  y << 10 << 9 << 12 << 15;
  
  mCurve->setData(t, x, y);
  QCOMPARE(mCurve->data()->size(), 4);
  // data should be sorted by t:
  QCOMPARE((mCurve->data()->constBegin()+0)->t, -2.0);
  QCOMPARE((mCurve->data()->begin()+1)->t, -1.0);
  QCOMPARE((mCurve->data()->constBegin()+2)->t, 1.0);
  QCOMPARE((mCurve->data()->constBegin()+3)->t, 2.0);
  QCOMPARE((mCurve->data()->constBegin()+0)->value, 12.0);
  QCOMPARE((mCurve->data()->constBegin()+1)->value, 10.0);
  QCOMPARE((mCurve->data()->constBegin()+2)->value, 9.0);
  QCOMPARE((mCurve->data()->constBegin()+3)->value, 15.0);
  QCOMPARE((mCurve->data()->constBegin()+0)->key, 3.0);
  QCOMPARE((mCurve->data()->constBegin()+1)->key, 1.0);
  QCOMPARE((mCurve->data()->constBegin()+2)->key, 2.0);
  QCOMPARE((mCurve->data()->constBegin()+3)->key, 4.0);
  
  // data removal:
  mCurve->data()->removeBefore(0);
  QCOMPARE(mCurve->data()->size(), 2);
  QCOMPARE((mCurve->data()->constBegin()+0)->t, 1.0);
  QCOMPARE((mCurve->data()->constBegin()+1)->t, 2.0);
  
  mCurve->setData(t, x, y);
  mCurve->data()->removeAfter(0);
  QCOMPARE(mCurve->data()->size(), 2);
  QCOMPARE((mCurve->data()->constBegin()+0)->t, -2.0);
  QCOMPARE((mCurve->data()->constBegin()+1)->t, -1.0);
  
  mCurve->setData(t, x, y);
  mCurve->data()->remove(-1.1, -0.9);
  QCOMPARE(mCurve->data()->size(), 3);
  QCOMPARE((mCurve->data()->constBegin()+0)->t, -2.0);
  QCOMPARE((mCurve->data()->constBegin()+1)->t, 1.0);
  QCOMPARE((mCurve->data()->constBegin()+2)->t, 2.0);
  
  mCurve->setData(t, x, y);
  mCurve->data()->remove(-2.1, -1.9);
  QCOMPARE(mCurve->data()->size(), 3);
  QCOMPARE((mCurve->data()->constBegin()+0)->t, -1.0);
  QCOMPARE((mCurve->data()->constBegin()+1)->t, 1.0);
  QCOMPARE((mCurve->data()->constBegin()+2)->t, 2.0);
  
  mCurve->setData(t, x, y);
  mCurve->data()->remove(1.9, 2.1);
  QCOMPARE(mCurve->data()->size(), 3);
  QCOMPARE((mCurve->data()->constBegin()+0)->t, -2.0);
  QCOMPARE((mCurve->data()->constBegin()+1)->t, -1.0);
  QCOMPARE((mCurve->data()->constBegin()+2)->t, 1.0);
  
  mCurve->setData(t, x, y);
  mCurve->data()->remove(-1.1, 1.1);
  QCOMPARE(mCurve->data()->size(), 2);
  QCOMPARE((mCurve->data()->constBegin()+0)->t, -2.0);
  QCOMPARE((mCurve->data()->constBegin()+1)->t, 2.0);
  
  mCurve->setData(t, x, y);
  mCurve->data()->clear();
  QCOMPARE(mCurve->data()->size(), 0);
  
  // add single data points:
  mCurve->addData(5, 12, 6);
  QCOMPARE(mCurve->data()->size(), 1);
  mCurve->addData(4, 14, 5);
  QCOMPARE(mCurve->data()->size(), 2);
  QCOMPARE((mCurve->data()->constBegin()+0)->value, 5.0);
  QCOMPARE((mCurve->data()->constBegin()+1)->value, 6.0);
  
  mCurve->setData(t, x, y);
  mCurve->addData(3, 12, 4);
  QCOMPARE(mCurve->data()->size(), 5);
  QCOMPARE((mCurve->data()->constBegin()+0)->value, 12.0);
  QCOMPARE((mCurve->data()->constBegin()+1)->value, 10.0);
  QCOMPARE((mCurve->data()->constBegin()+2)->value, 9.0);
  QCOMPARE((mCurve->data()->constBegin()+3)->value, 15.0);
  QCOMPARE((mCurve->data()->constBegin()+4)->value, 4.0);
  
  mCurve->setData(t, x, y);
  mCurve->addData(QVector<double>() << 3 << 4 << 5, QVector<double>() << 17 << 16 << 15, QVector<double>() << 4 << 5 << 6);
  QCOMPARE(mCurve->data()->size(), 7);
  QCOMPARE((mCurve->data()->constBegin()+0)->value, 12.0);
  QCOMPARE((mCurve->data()->constBegin()+1)->value, 10.0);
  QCOMPARE((mCurve->data()->constBegin()+2)->value, 9.0);
  QCOMPARE((mCurve->data()->constBegin()+3)->value, 15.0);
  QCOMPARE((mCurve->data()->constBegin()+4)->value, 4.0);
  QCOMPARE((mCurve->data()->constBegin()+5)->value, 5.0);
  QCOMPARE((mCurve->data()->constBegin()+6)->value, 6.0);
}

void TestQCPCurve::dataSharing()
{
  QCPCurve *curve2 = new QCPCurve(mPlot->xAxis, mPlot->yAxis);
  
  QSharedPointer<QCPCurveDataContainer> data = curve2->data();
  mCurve->setData(data); // now shares data with curve2
  
  QVector<double> t, x, y;
  t << -1 << 1 << -2 << 2;
  x <<  1 << 2 <<  0 << 3;
  y <<  10 << 9 <<  12 << 15;
  curve2->setData(t, x, y);
  
  QCOMPARE(mCurve->data()->size(), 4);
  QCOMPARE(curve2->data()->size(), 4);
  data->add(QCPCurveData(4, 9, 5)); // modify from outside
  QCOMPARE(mCurve->data()->size(), 5);
  QCOMPARE(curve2->data()->size(), 5);
  data.clear(); // clear external reference, internals should live on
  QVERIFY(!data);
  QCOMPARE(mCurve->data()->size(), 5);
  QCOMPARE(curve2->data()->size(), 5);
  
  // decouple data again:
  mCurve->setData(QSharedPointer<QCPCurveDataContainer>(new QCPCurveDataContainer));
  mCurve->addData(1, 1);
  curve2->addData(4, 5);
  QCOMPARE(mCurve->data()->size(), 1);
  QCOMPARE(curve2->data()->size(), 6);
}


// The whole curve is one polyline of data-space vertices: keys that are not sorted must not be
// cut into a key window, and no clipping helper may rewrite the data.
void TestQCPCurve::curveIsOnePolylineCommand()
{
  setFivePoints(mCurve);
  mPlot->replot(QCustomPlot::rpImmediateRefresh);

  const QCPDrawList& recording = inspectable(mCurve)->recording();
  const std::vector<const qcp::PolylineDataCmd*> lines = polylines(recording);
  QCOMPARE(lines.size(), std::size_t(1));
  QCOMPARE(lines.front()->points->size(), std::size_t(5));
  QCOMPARE(recording.commands.size(), std::size_t(1));
  QVERIFY(qcp::commandKind(*std::get_if<qcp::PolylineDataCmd>(&recording.commands.front())).gpu);
}

// QCPCurve keeps its 2.x line style names but only a plain line and no line are expressible: a
// step or an impulse would need vertices the curve's data does not have.
void TestQCPCurve::curveRefusesAStepStyle()
{
  test_misuse::calls.clear();
  qcp::setMisuseHandler(test_misuse::record);
  mCurve->setLineStyle(static_cast<QCPCurve::LineStyle>(QCPAbstractLine::lsStepLeft));
  qcp::setMisuseHandler(nullptr);

  QCOMPARE(test_misuse::calls.size(), std::size_t(1));
  QCOMPARE(mCurve->lineStyle(), QCPCurve::lsLine);
}

// Colour by scalar goes through the shared coloured polyline: one colour per vertex, taken from
// the data index the vertex came from.
void TestQCPCurve::curveColoursByValue()
{
  setFivePoints(mCurve);
  mCurve->setColorGradient(QCPColorGradient(QCPColorGradient::gpJet));
  mCurve->setColorRange(QCPRange(0, 4));
  mCurve->setColorValues(std::vector<double> { 0, 1, 2, 3, 4 });
  QVERIFY(mCurve->hasColorValues());
  mPlot->replot(QCustomPlot::rpImmediateRefresh);

  const std::vector<const qcp::PolylineDataCmd*> lines
      = polylines(inspectable(mCurve)->recording());
  QCOMPARE(lines.size(), std::size_t(1));
  const std::shared_ptr<const std::vector<QRgb>>& colours = lines.front()->colours;
  QVERIFY2(colours, "the polyline carries no per-vertex colours");
  QCOMPARE(colours->size(), lines.front()->points->size());
  const std::set<QRgb> distinct(colours->begin(), colours->end());
  QVERIFY2(distinct.size() > 1, "every vertex took the same colour");
}

// Colour values belong to the data they were set for: new data of another size draws uncoloured,
// as a multigraph does, instead of putting the old colours on the wrong points.
void TestQCPCurve::newDataOfAnotherSizeDrawsUncoloured()
{
  setFivePoints(mCurve);
  mCurve->setColorRange(QCPRange(0, 4));
  mCurve->setColorValues(std::vector<double> { 0, 1, 2, 3, 4 });
  mCurve->setData(QVector<double> { 0, 1, 2 }, QVector<double> { 0, 1, 0 });
  mPlot->replot(QCustomPlot::rpImmediateRefresh);

  const std::vector<const qcp::PolylineDataCmd*> lines
      = polylines(inspectable(mCurve)->recording());
  QCOMPARE(lines.size(), std::size_t(1));
  QVERIFY2(!lines.front()->colours, "the old colours were put on the new points");
}

// The fill is one polygon of data-space positions, not a triangulation: the replay rasterises it,
// which is what a self-crossing curve needs.
void TestQCPCurve::curveFillIsAPolygon()
{
  setFivePoints(mCurve);
  mCurve->setBrush(QBrush(QColor(0, 160, 255, 120)));
  mPlot->replot(QCustomPlot::rpImmediateRefresh);

  const QCPDrawList& recording = inspectable(mCurve)->recording();
  QCOMPARE(polygonCount(recording), 1);
  for (const qcp::QCPDrawCommand& command : recording.commands)
      if (const auto* polygon = std::get_if<qcp::PolygonCmd>(&command))
      {
          QCOMPARE(polygon->points.size(), std::size_t(5));
          QVERIFY(qcp::allPositionsIn<QCP::Data>(polygon->points));
          QCOMPARE(polygon->pen.style(), Qt::NoPen);
          QVERIFY(polygon->brush.style() != Qt::NoBrush);
      }

  mCurve->setBrush(Qt::NoBrush);
  mPlot->replot(QCustomPlot::rpImmediateRefresh);
  QCOMPARE(polygonCount(inspectable(mCurve)->recording()), 0);
}

// The selected data range draws with the selection decorator's pen, as it did before the port,
// while the unselected ranges keep the curve's own pen.
void TestQCPCurve::selectedSegmentUsesTheSelectedPen()
{
  mCurve->setData(QVector<double> { 0, 1, 2, 3, 4, 5, 6, 7 },
                  QVector<double> { 0, 1, 0, 2, 3, 2, 1, 0 });
  mCurve->setSelectable(QCP::stDataRange);
  mCurve->setSelection(QCPDataSelection(QCPDataRange(2, 6)));
  const QPen selectedPen(QColor(220, 0, 0), 7);
  mCurve->selectionDecorator()->setPen(selectedPen);
  mPlot->replot(QCustomPlot::rpImmediateRefresh);

  const std::vector<const qcp::PolylineDataCmd*> lines
      = polylines(inspectable(mCurve)->recording());
  QCOMPARE(lines.size(), std::size_t(3));
  int selected = 0;
  int unselected = 0;
  for (const qcp::PolylineDataCmd* line : lines)
  {
      if (line->pen.color() == selectedPen.color() && line->pen.width() == selectedPen.width())
          ++selected;
      else
          ++unselected;
  }
  QCOMPARE(selected, 1);
  QCOMPARE(unselected, 2);
}

// A click midway along a segment is far from both data points: the hit test must measure the
// segments, not only the vertices.
void TestQCPCurve::clickOnASegmentSelectsTheCurve()
{
  mCurve->setData(QVector<double> { 0, 1, 2 }, QVector<double> { 1, 1, 1 });
  mPlot->replot(QCustomPlot::rpImmediateRefresh);

  const QPointF pos(mPlot->xAxis->coordToPixel(0.5), mPlot->yAxis->coordToPixel(1.0));
  const QPointF first(mPlot->xAxis->coordToPixel(0.0), mPlot->yAxis->coordToPixel(1.0));
  const QPointF second(mPlot->xAxis->coordToPixel(1.0), mPlot->yAxis->coordToPixel(1.0));
  const double toNearestPoint
      = std::min(QCPVector2D(pos - first).length(), QCPVector2D(pos - second).length());
  QVERIFY2(toNearestPoint > mPlot->selectionTolerance() * 3,
           "the click is not between the data points");

  QVariant details;
  const double distance = mCurve->selectTest(pos, false, &details);
  QVERIFY2(distance >= 0.0, "the segment between two data points was not hit");
  QVERIFY2(distance < mPlot->selectionTolerance(), "the hit is farther than the tolerance");
  QVERIFY(!details.value<QCPDataSelection>().isEmpty());
}

// A NaN in the data breaks the line: the vertex stays in the polyline and both replays split it
// there.
void TestQCPCurve::nanBreaksTheCurveLine()
{
  mCurve->setData(QVector<double> { 0, 1, 2, 3 }, QVector<double> { 0, 1, qQNaN(), 3 });
  mPlot->replot(QCustomPlot::rpImmediateRefresh);

  const std::vector<const qcp::PolylineDataCmd*> lines
      = polylines(inspectable(mCurve)->recording());
  QCOMPARE(lines.size(), std::size_t(1));
  QCOMPARE(lines.front()->points->size(), std::size_t(4));
  QCOMPARE(inspectable(mCurve)->polylineNaNs(), 1);
}

void TestQCPCurve::noSetterIsIgnored_data()
{
  QTest::addColumn<QString>("setter");
  for (const CurveSetterCase& c : curveSetterCases())
      QTest::newRow(qPrintable(c.name)) << c.name;
}

void TestQCPCurve::noSetterIsIgnored()
{
  QFETCH(QString, setter);
  const auto& cases = curveSetterCases();
  const auto it = std::find_if(cases.begin(), cases.end(),
                               [&](const CurveSetterCase& c) { return c.name == setter; });
  QVERIFY(it != cases.end());

  class DirtyAwarePlot : public QCustomPlot
  {
  public:
      using QCustomPlot::hasInvalidatedPaintBuffers;
  };

  DirtyAwarePlot plot;
  plot.resize(400, 300);
  auto* curve = new QCPCurve(plot.xAxis, plot.yAxis);
  setFivePoints(curve);
  plot.xAxis->setRange(0, 4);
  plot.yAxis->setRange(0, 3);
  if (it->prepare)
      it->prepare(curve);
  plot.replot(QCustomPlot::rpImmediateRefresh);
  QVERIFY2(!plot.hasInvalidatedPaintBuffers(), "the frame was not clean before the change");
  const QImage before = renderOf(&plot);

  it->change(curve);

  QVERIFY2(plot.hasInvalidatedPaintBuffers(),
           qPrintable(setter + QStringLiteral(" marked nothing dirty")));
  plot.replot(QCustomPlot::rpImmediateRefresh);
  const QImage after = renderOf(&plot);
  QVERIFY2(before != after, qPrintable(setter + QStringLiteral(" changed nothing it draws")));
}
