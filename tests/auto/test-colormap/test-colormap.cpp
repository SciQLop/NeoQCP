#include <QPainterPath>
#include "test-colormap.h"
#include "reference-image.h"
#include <QMainWindow>
#include <painting/contour-extractor.h>
#include <QtWidgets/qtestsupport_widgets.h> // QTest::qWaitForWindowExposed
#include <algorithm>
#include <cmath>
#include <limits>
#include <QDir>
#include <painting/draw-commands.h>

void TestColorMap::init()
{
  mPlot = new QCustomPlot(0);
  mColorMap = new QCPColorMap(mPlot->xAxis, mPlot->yAxis);
}

void TestColorMap::QCPColorScale_rescaleDataRange()
{
  QCPColorScale *scale = new QCPColorScale(mPlot);
  mPlot->plotLayout()->addElement(0, 1, scale);
  
  QCPColorMap *map1 = new QCPColorMap(mPlot->xAxis, mPlot->yAxis);
  QCPColorMap *map2 = new QCPColorMap(mPlot->xAxis, mPlot->yAxis);
  map1->setColorScale(scale);
  map2->setColorScale(scale);
  map1->data()->setSize(2, 2);
  map2->data()->setSize(2, 2);
  
  // normal values:
  map1->data()->setCell(0, 0, 1);
  map1->data()->setCell(1, 0, 2);
  map1->data()->setCell(0, 1, 3);
  map1->data()->setCell(1, 1, 4);
  map1->data()->recalculateDataBounds();
  map2->data()->setCell(0, 0, 6);
  map2->data()->setCell(1, 0, 7);
  map2->data()->setCell(0, 1, 8);
  map2->data()->setCell(1, 1, 9);
  map2->data()->recalculateDataBounds();
  scale->rescaleDataRange(true);
  QCOMPARE(map1->dataRange().lower, 1.0);
  QCOMPARE(map1->dataRange().upper, 9.0);
  QCOMPARE(map2->dataRange().lower, 1.0);
  QCOMPARE(map2->dataRange().upper, 9.0);
  QCOMPARE(scale->dataRange().lower, 1.0);
  QCOMPARE(scale->dataRange().upper, 9.0);
  
  // one singular:
  map1->data()->setCell(0, 0, 2);
  map1->data()->setCell(1, 0, 2);
  map1->data()->setCell(0, 1, 2);
  map1->data()->setCell(1, 1, 2);
  map1->data()->recalculateDataBounds();
  map2->data()->setCell(0, 0, 6);
  map2->data()->setCell(1, 0, 7);
  map2->data()->setCell(0, 1, 8);
  map2->data()->setCell(1, 1, 9);
  map2->data()->recalculateDataBounds();
  scale->rescaleDataRange(true);
  QCOMPARE(map1->dataRange().lower, 2.0);
  QCOMPARE(map1->dataRange().upper, 9.0);
  QCOMPARE(map2->dataRange().lower, 2.0);
  QCOMPARE(map2->dataRange().upper, 9.0);
  QCOMPARE(scale->dataRange().lower, 2.0);
  QCOMPARE(scale->dataRange().upper, 9.0);
  
  // both singular:
  map1->data()->setCell(0, 0, 1);
  map1->data()->setCell(1, 0, 1);
  map1->data()->setCell(0, 1, 1);
  map1->data()->setCell(1, 1, 1);
  map1->data()->recalculateDataBounds();
  map2->data()->setCell(0, 0, 6);
  map2->data()->setCell(1, 0, 6);
  map2->data()->setCell(0, 1, 6);
  map2->data()->setCell(1, 1, 6);
  map2->data()->recalculateDataBounds();
  scale->rescaleDataRange(true);
  QCOMPARE(map1->dataRange().lower, 1.0);
  QCOMPARE(map1->dataRange().upper, 6.0);
  QCOMPARE(map2->dataRange().lower, 1.0);
  QCOMPARE(map2->dataRange().upper, 6.0);
  QCOMPARE(scale->dataRange().lower, 1.0);
  QCOMPARE(scale->dataRange().upper, 6.0);
  
  // both singular at same value (range should center on value):
  scale->setDataRange(QCPRange(0, 1));
  map1->data()->setCell(0, 0, 3);
  map1->data()->setCell(1, 0, 3);
  map1->data()->setCell(0, 1, 3);
  map1->data()->setCell(1, 1, 3);
  map1->data()->recalculateDataBounds();
  map2->data()->setCell(0, 0, 3);
  map2->data()->setCell(1, 0, 3);
  map2->data()->setCell(0, 1, 3);
  map2->data()->setCell(1, 1, 3);
  map2->data()->recalculateDataBounds();
  scale->rescaleDataRange(true);
  QCOMPARE(map1->dataRange().lower, 2.5);
  QCOMPARE(map1->dataRange().upper, 3.5);
  QCOMPARE(map2->dataRange().lower, 2.5);
  QCOMPARE(map2->dataRange().upper, 3.5);
  QCOMPARE(scale->dataRange().lower, 2.5);
  QCOMPARE(scale->dataRange().upper, 3.5);
}

void TestColorMap::QCPColorMapData_fillSetsExactValue()
{
  QCPColorMapData data(2, 2, QCPRange(0, 1), QCPRange(0, 1));
  data.fill(3.5);
  QCOMPARE(data.cell(0, 0), 3.5);
  QCOMPARE(data.cell(1, 0), 3.5);
  QCOMPARE(data.cell(0, 1), 3.5);
  QCOMPARE(data.cell(1, 1), 3.5);
}

void TestColorMap::QCPColorMapData_constructorZeroInitializes()
{
  QCPColorMapData data(3, 2, QCPRange(0, 1), QCPRange(0, 1));
  for (int key = 0; key < 3; ++key)
    for (int value = 0; value < 2; ++value)
      QCOMPARE(data.cell(key, value), 0.0);
}

void TestColorMap::QCPColorMapData_fillIsSafeOnEmptyMap()
{
  // clear()/setSize(0,0) is the one reachable path where mData is null;
  // this can't force the bad_alloc branch (unsafe/nondeterministic to
  // simulate a real allocation failure), but it exercises the same
  // null-guard fill() now needs.
  QCPColorMapData data(2, 2, QCPRange(0, 1), QCPRange(0, 1));
  data.clear();
  QVERIFY(data.isEmpty());
  data.fill(3.5);
  QVERIFY(data.isEmpty());
}

void TestColorMap::QCPColorMapData_cellToCoordHandlesSingleCellDimension()
{
  // A 1-cell key dimension is reachable (e.g. SciQLopHistogram2D with
  // x_bins=1): keyIndex/double(mKeySize-1) divides by zero. The value
  // dimension here has the normal size>1 so this also proves the fix
  // doesn't disturb the ordinary calculation.
  QCPColorMapData data(1, 3, QCPRange(0, 10), QCPRange(0, 1));
  double key = -1, value = -1;
  data.cellToCoord(0, 1, &key, &value);
  QVERIFY2(std::isfinite(key), "cellToCoord must not return NaN for a size-1 key dimension");
  QCOMPARE(key, 5.0);
  QCOMPARE(value, 0.5);
}

// The legend icon used to be a fixed blue-to-red ramp, whatever the gradient.
template <typename Plottable>
static void checkGrayscaleLegendIcon(QCustomPlot* plot)
{
  struct Iconed : Plottable
  {
    using Plottable::Plottable;
    using Plottable::drawLegendIcon;
  };
  auto* p = new Iconed(plot->xAxis, plot->yAxis);
  p->setGradient(QCPColorGradient(QCPColorGradient::gpGrayscale));
  QImage icon(40, 10, QImage::Format_ARGB32);
  icon.fill(Qt::transparent);
  {
    QCPPainter painter(&icon);
    p->drawLegendIcon(&painter, QRectF(0, 0, 40, 10));
  }
  for (const int x : {1, 20, 38})
  {
    const QColor c = icon.pixelColor(x, 5);
    QVERIFY2(c.red() == c.green() && c.green() == c.blue(), qPrintable(c.name()));
  }
  QVERIFY(icon.pixelColor(1, 5).lightness() < icon.pixelColor(38, 5).lightness());
}

void TestColorMap::QCPColorMap2_legendIconShowsTheGradient()
{
  checkGrayscaleLegendIcon<QCPColorMap2>(mPlot);
}

void TestColorMap::QCPHistogram2D_legendIconShowsTheGradient()
{
  checkGrayscaleLegendIcon<QCPHistogram2D>(mPlot);
}

void TestColorMap::QCPColorMap2_selectTestHitSetsDetails()
{
  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setRange(0, 4);

  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  std::vector<double> x = {0, 1, 2, 3, 4};
  std::vector<double> y = {0, 1, 2, 3, 4};
  std::vector<double> z(25, 1.0);
  cm->setData(x, y, z);
  mPlot->replot();

  const double px = mPlot->xAxis->coordToPixel(2.0);
  const double py = mPlot->yAxis->coordToPixel(2.0);

  QVariant details;
  const double result = cm->selectTest(QPointF(px, py), true, &details);

  QVERIFY(result > 0);
  QCOMPARE(result, mPlot->selectionTolerance() * 0.99);
  QVERIFY(details.canConvert<QCPDataSelection>());
  QCOMPARE(details.value<QCPDataSelection>(), QCPDataSelection(QCPDataRange(0, 1)));
}

void TestColorMap::QCPColorMap2_selectTestMissReturnsNegativeOne()
{
  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setRange(0, 4);

  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  std::vector<double> x = {0, 1, 2, 3, 4};
  std::vector<double> y = {0, 1, 2, 3, 4};
  std::vector<double> z(25, 1.0);
  cm->setData(x, y, z);
  mPlot->replot();

  const double px = mPlot->xAxis->coordToPixel(20.0);
  const double py = mPlot->yAxis->coordToPixel(20.0);

  QVariant details;
  const double result = cm->selectTest(QPointF(px, py), true, &details);

  QCOMPARE(result, -1.0);
}

void TestColorMap::QCPColorMap2_contourSettersScheduleReplot()
{
  // One shared colormap for all three setters below: each spins up a real
  // background resample job (setData -> onDataChanged submits it regardless
  // of any synchronous bake), so settling once here -- instead of once per
  // setter in three separate fixtures -- keeps this test from dumping extra
  // worker-thread/RHI cold-start load on whatever test class runs next.
  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setRange(0, 4);

  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  std::vector<double> x = {0, 1, 2, 3, 4};
  std::vector<double> y = {0, 1, 2, 3, 4};
  std::vector<double> z(25, 1.0);
  cm->setData(x, y, z);
  cm->setAutoContourLevels(3);
  mPlot->replot();
  QTRY_VERIFY_WITH_TIMEOUT(!cm->pipeline().isBusy(), 2000);
  QTest::qWait(50); // flush any trailing queued replot from the pipeline settling

  {
    QSignalSpy spy(mPlot, &QCustomPlot::afterReplot);
    cm->setContourPen(QPen(Qt::red, 2));
    QVERIFY2(spy.wait(200), "setContourPen should schedule a queued replot on its own");
  }
  {
    QSignalSpy spy(mPlot, &QCustomPlot::afterReplot);
    cm->setContourLevels({0.2, 0.5, 0.8});
    QVERIFY2(spy.wait(200), "setContourLevels should schedule a queued replot on its own");
  }
  {
    QSignalSpy spy(mPlot, &QCustomPlot::afterReplot);
    cm->setAutoContourLevels(5);
    QVERIFY2(spy.wait(200), "setAutoContourLevels should schedule a queued replot on its own");
  }
  {
    QSignalSpy spy(mPlot, &QCustomPlot::afterReplot);
    cm->setContourLabelEnabled(true);
    QVERIFY2(spy.wait(200), "setContourLabelEnabled should schedule a queued replot on its own");
  }
}

void TestColorMap::QCPColorMap2_hidesStaleQuadWhenPannedPastData()
{
  // Regression: once panning moves the axes far enough that the last
  // resampled image no longer overlaps them at all, and no fresher result
  // has arrived yet to replace it (e.g. a slow out-of-process data source
  // still fetching the newly-panned-into range), draw() must not leave the
  // previous GPU quad on screen. It should hide it -- not freeze it in
  // place while the axes keep moving underneath it.
  mPlot->show();
  if (!QTest::qWaitForWindowExposed(mPlot))
    QSKIP("window not exposed in this environment");
  QCoreApplication::processEvents();
  if (!mPlot->rhi())
    QSKIP("no QRhi available in this environment");

  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setRange(0, 4);

  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  std::vector<double> x = {0, 1, 2, 3, 4};
  std::vector<double> y = {0, 1, 2, 3, 4};
  std::vector<double> z(25, 1.0);
  cm->setData(x, y, z);
  mPlot->replot();
  QTRY_VERIFY_WITH_TIMEOUT(!cm->pipeline().isBusy(), 2000);
  QTest::qWait(50); // flush the trailing queued replot from the pipeline settling
  mPlot->replot();
  QCoreApplication::processEvents();

  QVERIFY2(cm->mCore.recordedCommandCount() > 0,
           "sanity: a real resample should have recorded the image");

  // Pan to a same-size range (pure pan, not a zoom) sharing no overlap with
  // the data at all, then replot immediately -- before any fresh (empty,
  // out-of-data) resample has a chance to land. This is the window a slow
  // remote data source leaves stale content sitting in during production.
  mPlot->xAxis->setRange(1000, 1004);
  mPlot->replot();

  // The image carries the grid's data corners, not a pixel rect: the old grid now lies
  // outside the axes, so the stale picture cannot be frozen over them.
  const auto* image = std::get_if<qcp::ImageCmd>(&cm->mCore.recordedList().commands.front());
  QVERIFY2(image, "the grid is recorded as an image");
  const auto* a = std::get_if<QCP::Data>(&image->a);
  const auto* b = std::get_if<QCP::Data>(&image->b);
  QVERIFY(a && b);
  QVERIFY2(std::max(a->x, b->x) < mPlot->xAxis->range().lower,
           "the stale grid must lie outside the panned axes, where nothing can show it");
}

void TestColorMap::QCPContourExtractor_extractUvKeepsSourceRegistration()
{
  // A grid wider than maxDim is point-sampled with stride sk; the emitted
  // UVs must still reference the SOURCE indices (srcK/(kSize-1)) and the
  // tail strip past the last stride multiple must stay covered.
  constexpr int kSize = 900; // sk=3, last stride multiple 897, tail 897..899
  constexpr int vSize = 4;
  QCPColorMapData data(kSize, vSize, QCPRange(0, kSize - 1), QCPRange(0, vSize - 1));
  for (int ki = 0; ki < kSize; ++ki)
    for (int vi = 0; vi < vSize; ++vi)
      data.setCell(ki, vi, double(ki));
  data.recalculateDataBounds();

  // Level mid-grid: registered to source index 450 -> u = 450/899.
  // The old code emitted reduced-index UVs (150/299), off by ~1.1e-3.
  {
    const QVector<float> uv = QCPContourExtractor::extractUv(&data, {450.0});
    QVERIFY2(!uv.isEmpty(), "mid-grid level must produce segments");
    QVERIFY(uv.size() % 4 == 0);
    constexpr double expected = 450.0 / 899.0;
    for (qsizetype i = 0; i < uv.size(); i += 2)
      QVERIFY2(qAbs(double(uv[i]) - expected) < 3e-4,
               qPrintable(QString("u=%1, expected %2").arg(double(uv[i])).arg(expected)));
  }

  // Level inside the tail strip (past source index 897): the old code never
  // sampled there, so it produced no segments at all.
  {
    const QVector<float> uv = QCPContourExtractor::extractUv(&data, {898.5});
    QVERIFY2(!uv.isEmpty(), "tail-strip level must produce segments");
    double maxU = 0;
    for (qsizetype i = 0; i < uv.size(); i += 2)
    {
      QVERIFY2(uv[i] >= 0.0f && uv[i] <= 1.0f, "u must stay in [0,1]");
      maxU = qMax(maxU, double(uv[i]));
    }
    QVERIFY2(maxU > 0.999, qPrintable(QString("maxU=%1, tail must reach u=1").arg(maxU)));
  }
}

void TestColorMap::QCPColorMapData_recalculateDataBoundsSkipsInfinite()
{
  QCPColorMapData data(2, 2, QCPRange(0, 1), QCPRange(0, 1));
  data.setCell(0, 0, 1.0);
  data.setCell(1, 0, 2.0);
  data.setCell(0, 1, 3.0);
  data.setCell(1, 1, 4.0);
  // Infinities are stored but must not poison the cached bounds (which feed
  // auto contour levels and the color mapping).
  data.setCell(1, 1, std::numeric_limits<double>::infinity());
  QCOMPARE(data.cell(1, 1), std::numeric_limits<double>::infinity());
  data.recalculateDataBounds();
  QCOMPARE(data.dataBounds().lower, 1.0);
  QCOMPARE(data.dataBounds().upper, 3.0);
}

namespace
{
// Settles the colormap pipeline like the existing tests do, then forces one
// more draw so updateContourGpu has run against the delivered result.
bool fontDrawsGlyphs()
{
  QPainterPath probe;
  probe.addText(0, 0, QFont(), "123");
  return !probe.isEmpty();
}

void settleColorMap(QCustomPlot* plot, QCPColorMap2* cm)
{
  plot->replot();
  QTRY_VERIFY_WITH_TIMEOUT(!cm->pipeline().isBusy(), 2000);
  QTest::qWait(50); // flush the trailing queued replot from the pipeline settling
  plot->replot();
}

int countRedPixels(const QImage& img)
{
  int red = 0;
  for (int py = 0; py < img.height(); ++py)
    for (int px = 0; px < img.width(); ++px)
    {
      // pixelColor() honours the image format: a grabFramebuffer() image is not ARGB32.
      const QColor c = img.pixelColor(px, py);
      if (c.red() > 200 && c.green() < 100 && c.blue() < 100)
        ++red;
    }
  return red;
}

QCPColorMap* classicColorMap(QCustomPlot* plot, int size = 10)
{
  auto* map = new QCPColorMap(plot->xAxis, plot->yAxis);
  map->data()->setSize(size, size);
  map->data()->setRange(QCPRange(0, 10), QCPRange(0, 10));
  for (int k = 0; k < size; ++k)
    for (int v = 0; v < size; ++v)
      map->data()->setCell(k, v, double(k * 17 + v * 31));
  map->setGradient(QCPColorGradient::gpHot);
  map->rescaleDataRange();
  return map;
}

} // namespace

void TestColorMap::QCPColorMap2_contourMirrorsReversedAxis()
{
  // z = x on a 5x5 grid, level at 1.0 -> a vertical line at x = 1. The lines live in data
  // space: reversing the key axis is the replay's business. The recording must be reused as
  // it is, and the line follows the mirrored image because both carry the same data corners.
  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setRange(0, 4);
  std::vector<double> x = {0, 1, 2, 3, 4}, y = {0, 1, 2, 3, 4}, z(25, 0.0);
  // NOTE: source Z layout is x-outer: z[xIndex * ySize + yIndex].
  for (int ki = 0; ki < 5; ++ki)
    for (int vi = 0; vi < 5; ++vi)
      z[ki * 5 + vi] = double(ki); // z = x -> vertical contours

  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  cm->setData(x, y, z);
  settleColorMap(mPlot, cm);
  cm->setContourLevels({1.0});
  mPlot->replot();
  QTRY_VERIFY_WITH_TIMEOUT(!cm->pipeline().isBusy(), 2000);
  QTest::qWait(50);
  mPlot->replot();

  const auto lines = [](const QCPDrawnCore& core)
  {
    std::vector<std::vector<QPointF>> out;
    for (const auto& command : core.recordedList().commands)
      if (const auto* line = std::get_if<qcp::PolylineDataCmd>(&command))
        out.push_back(*line->points);
    return out;
  };
  const auto plain = lines(cm->mCore);
  QVERIFY2(!plain.empty(), "sanity: the level must record contour points");
  const quint64 recordings = cm->mCore.recordingCount();

  // setRangeReversed() emits no signal: the replot reuses the recording, the transform mirrors it.
  mPlot->xAxis->setRangeReversed(true);
  mPlot->replot();

  QVERIFY2(mPlot->xAxis->rangeReversed(), "sanity: the key axis is reversed");
  QCOMPARE(cm->mCore.recordingCount(), recordings);
  const auto reversed = lines(cm->mCore);
  QCOMPARE(reversed.size(), plain.size());
  const auto samePoint = [](const QPointF& l, const QPointF& r)
  {
    if (std::isnan(l.x()) && std::isnan(r.x()))
      return std::isnan(l.y()) && std::isnan(r.y());
    return l == r;
  };
  for (size_t i = 0; i < plain.size(); ++i)
  {
    QCOMPARE(reversed[i].size(), plain[i].size());
    QVERIFY2(std::equal(reversed[i].cbegin(), reversed[i].cend(), plain[i].cbegin(), samePoint),
             "data-space contour points must not be mirrored");
  }
}

void TestColorMap::QCPColorMap2_contourExportDrawsFallback()
{
  // The classic map now paints as a GPU command: on export it stacks above
  // painter-drawn plottables at the end of the layer's pass, as on screen. This
  // test is about the color map 2, so keep the fixture's classic map out.
  mColorMap->setVisible(false);
  // toPixmap() paints with pmNoCaching, where the RHI contour layer never
  // receives the lines. The QPainter fallback must draw them instead --
  // previously the export silently dropped every contour.
  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setRange(0, 4);
  std::vector<double> x = {0, 1, 2, 3, 4}, y = {0, 1, 2, 3, 4}, z(25, 0.0);
  // NOTE: source Z layout is x-outer: z[xIndex * ySize + yIndex].
  for (int ki = 0; ki < 5; ++ki)
    for (int vi = 0; vi < 5; ++vi)
      z[ki * 5 + vi] = double(ki); // z = x -> vertical contours

  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  cm->setData(x, y, z);
  settleColorMap(mPlot, cm);
  cm->setContourLevels({1.0, 2.0, 3.0});
  cm->setContourPen(QPen(Qt::red, 2));
  mPlot->replot();
  QTRY_VERIFY_WITH_TIMEOUT(!cm->pipeline().isBusy(), 2000);
  QTest::qWait(50);

  const QPixmap pm = mPlot->toPixmap();
  QVERIFY(!pm.isNull());
  const int red = countRedPixels(pm.toImage());
  QVERIFY2(red > 50, qPrintable(QString("export must draw contour lines, red pixels=%1").arg(red)));
}

// On a log value axis the grid rows are log-spaced: an export line or a label placed as if they
// were evenly spaced lands far from where the colour changes.
void TestColorMap::QCPColorMap2_contourExportFollowsALogValueAxis()
{
  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setScaleType(QCPAxis::stLogarithmic);
  mPlot->yAxis->setRange(1, 1000);
  const int ny = 31;
  std::vector<double> x = {0, 1, 2, 3, 4}, y(ny), z(5 * ny);
  for (int vi = 0; vi < ny; ++vi)
    y[vi] = std::pow(10.0, 3.0 * vi / (ny - 1));
  for (int ki = 0; ki < 5; ++ki)
    for (int vi = 0; vi < ny; ++vi)
      z[ki * ny + vi] = std::log10(y[vi]); // horizontal contours at y = 10^level
  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  cm->setData(x, y, z);
  settleColorMap(mPlot, cm);
  cm->setContourLevels({1.0, 2.0});
  cm->setContourPen(QPen(Qt::red, 2));
  cm->setContourLabelEnabled(true);
  QTRY_VERIFY_WITH_TIMEOUT(!cm->pipeline().isBusy(), 2000);

  const QImage img = mPlot->toPixmap().toImage();
  const auto isRed = [](QColor c) { return c.red() > 200 && c.green() < 80 && c.blue() < 80; };
  const int column = qRound(mPlot->xAxis->coordToPixel(0.5)); // clear of the labels at x = 2
  for (double level : {1.0, 2.0})
  {
    const double row = mPlot->yAxis->coordToPixel(std::pow(10.0, level));
    bool found = false;
    for (int dy = -5; dy <= 5 && !found; ++dy)
      found = isRed(img.pixelColor(column, qRound(row) + dy));
    QVERIFY2(found, qPrintable(QString("no export line at y=10^%1 (pixel row %2)").arg(level).arg(row)));
  }
  // The resampled grid is piecewise constant per source row: a line is exact to half a row.
  const double halfRow = 0.5 * (mPlot->yAxis->coordToPixel(y[0]) - mPlot->yAxis->coordToPixel(y[1])) + 1;
  QVERIFY(!cm->lastContourLabels().isEmpty());
  for (const auto& label : cm->lastContourLabels())
  {
    const double row = mPlot->yAxis->coordToPixel(std::pow(10.0, label.level));
    QVERIFY2(qAbs(label.position.y() - row) < halfRow,
             qPrintable(QString("label %1 at row %2, its line is at %3")
                            .arg(label.level).arg(label.position.y()).arg(row)));
  }
}

// Auto levels change with the colour scale type; the labels must not keep the old ones.
// A reversed axis flips the image; the export must show each value at its own coordinate,
// exactly like the GPU quad does.
void TestColorMap::QCPColorMap2_exportDrawsTheImageOnAReversedAxis()
{
  // The classic map now paints as a GPU command: on export it stacks above
  // painter-drawn plottables at the end of the layer's pass, as on screen. This
  // test is about the color map 2, so keep the fixture's classic map out.
  mColorMap->setVisible(false);
  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 4);
  mPlot->xAxis->setRangeReversed(true);
  mPlot->yAxis->setRange(0, 4);
  std::vector<double> x = {0, 1, 2, 3, 4}, y = {0, 1, 2, 3, 4}, z(25);
  for (int ki = 0; ki < 5; ++ki)
    for (int vi = 0; vi < 5; ++vi)
      z[ki * 5 + vi] = ki; // dark at x = 0, bright at x = 4
  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  cm->setData(x, y, z);
  cm->setGradient(QCPColorGradient(QCPColorGradient::gpGrayscale));
  cm->setDataRange(QCPRange(0, 4));
  settleColorMap(mPlot, cm);

  const QImage img = mPlot->toPixmap().toImage();
  const int row = qRound(mPlot->yAxis->coordToPixel(2));
  const int low = qGray(img.pixel(qRound(mPlot->xAxis->coordToPixel(0.5)), row));
  const int high = qGray(img.pixel(qRound(mPlot->xAxis->coordToPixel(3.5)), row));
  QVERIFY2(low < high, qPrintable(QString("x=0.5 is %1, x=3.5 is %2: the image is mirrored").arg(low).arg(high)));
}

void TestColorMap::QCPColorMap2_contourLabelsFollowAColourScaleToggle()
{
  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setRange(0, 4);
  const int n = 41;
  std::vector<double> x(n), y(n), z(n * n);
  for (int i = 0; i < n; ++i)
    x[i] = y[i] = 4.0 * i / (n - 1);
  for (int ki = 0; ki < n; ++ki)
    for (int vi = 0; vi < n; ++vi)
      z[ki * n + vi] = std::pow(10.0, 3.0 * ki / (n - 1)); // 1..1000 along x
  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  cm->setData(x, y, z);
  settleColorMap(mPlot, cm);
  cm->setAutoContourLevels(3);
  cm->setContourLabelEnabled(true);
  mPlot->toPixmap();
  cm->setDataScaleType(QCPAxis::stLogarithmic);
  settleColorMap(mPlot, cm);
  mPlot->toPixmap();

  const QVector<double> levels = cm->lastContourLevels();
  QVERIFY(!cm->lastContourLabels().isEmpty());
  for (const auto& label : cm->lastContourLabels())
    QVERIFY2(levels.contains(label.level),
             qPrintable(QString("label level %1 is not a current level").arg(label.level)));
}

void TestColorMap::QCPColorMap2_contourDegenerateDataClearsLines()
{
  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setRange(0, 4);
  std::vector<double> x = {0, 1, 2, 3, 4}, y = {0, 1, 2, 3, 4}, z(25, 0.0);
  // NOTE: source Z layout is x-outer: z[xIndex * ySize + yIndex].
  for (int ki = 0; ki < 5; ++ki)
    for (int vi = 0; vi < 5; ++vi)
      z[ki * 5 + vi] = double(ki); // z = x -> vertical contours

  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  cm->setData(x, y, z);
  settleColorMap(mPlot, cm);
  cm->setContourLevels({1.0, 2.0, 3.0});
  mPlot->replot();
  QTRY_VERIFY_WITH_TIMEOUT(!cm->pipeline().isBusy(), 2000);
  QTest::qWait(50);
  mPlot->replot();
  const auto lineCount = [](const QCPDrawnCore& core)
  {
    int count = 0;
    for (const auto& command : core.recordedList().commands)
      count += std::get_if<qcp::PolylineDataCmd>(&command) != nullptr;
    return count;
  };
  QVERIFY2(lineCount(cm->mCore) > 0, "sanity: levels must record contour lines");

  // Constant data has degenerate bounds (lower == upper): the rebuild must
  // clear the previously built lines instead of leaving them stale.
  cm->setData(x, y, std::vector<double>(25, 1.0));
  mPlot->replot();
  QTRY_VERIFY_WITH_TIMEOUT(!cm->pipeline().isBusy(), 2000);
  QTest::qWait(50);
  mPlot->replot();
  QVERIFY2(lineCount(cm->mCore) == 0, "degenerate data must clear stale contour lines");
  QVERIFY2(cm->lastContourLevels().isEmpty(), "degenerate data must clear resolved levels");
}

void TestColorMap::QCPColorMap2_autoContourLevelsGeometricInLogScale()
{
  // z spans three decades (1..1000). Linear auto levels would bunch every
  // line at the top decade; under a log Z scale they must space geometrically.
  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setRange(0, 4);
  std::vector<double> x = {0, 1, 2, 3, 4}, y = {0, 1, 2, 3, 4}, z(25, 0.0);
  for (int vi = 0; vi < 5; ++vi)
    for (int ki = 0; ki < 5; ++ki)
      z[ki * 5 + vi] = std::pow(10.0, 3.0 * (ki * 5 + vi) / 24.0);

  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  cm->setData(x, y, z);
  settleColorMap(mPlot, cm);
  cm->setDataScaleType(QCPAxis::stLogarithmic);
  cm->setAutoContourLevels(3);
  mPlot->replot();
  QTRY_VERIFY_WITH_TIMEOUT(!cm->pipeline().isBusy(), 2000);
  QTest::qWait(50);
  mPlot->replot();

  const QVector<double> levels = cm->lastContourLevels();
  QVERIFY2(levels.size() == 3,
           qPrintable(QString("expected 3 auto levels, got %1").arg(levels.size())));
  // Equal ratios, not equal differences: bounds are ~[1,1000].
  const double r0 = levels[1] / levels[0], r1 = levels[2] / levels[1];
  QVERIFY2(qAbs(r0 - r1) / r0 < 0.02,
           qPrintable(QString("levels must be geometric: %1 %2 %3")
                          .arg(levels[0])
                          .arg(levels[1])
                          .arg(levels[2])));
  QVERIFY2(levels[0] > 2.0 && levels[0] < 12.0,
           qPrintable(QString("first level=%1, expected ~5.6").arg(levels[0])));
}

void TestColorMap::QCPColorMap2_contourExportAfterRhiDraw()
{
  // The classic map now paints as a GPU command: it stacks over painter-drawn
  // content of the same layer, on screen and on export. This test is about the
  // color map 2, so keep the fixture's classic map out.
  mColorMap->setVisible(false);
  // An on-screen RHI draw uploads contour lines to the GPU layer only. A
  // later export (pmNoCaching bypasses RHI) must still draw them via the
  // QPainter fallback -- previously they vanished because the fallback
  // stayed empty while the GPU flag claimed everything was fine.
  mPlot->show();
  if (!QTest::qWaitForWindowExposed(mPlot))
    QSKIP("window not exposed in this environment");
  QCoreApplication::processEvents();
  if (!mPlot->rhi())
    QSKIP("no QRhi available in this environment");

  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setRange(0, 4);
  std::vector<double> x = {0, 1, 2, 3, 4}, y = {0, 1, 2, 3, 4}, z(25, 0.0);
  // NOTE: source Z layout is x-outer: z[xIndex * ySize + yIndex].
  for (int ki = 0; ki < 5; ++ki)
    for (int vi = 0; vi < 5; ++vi)
      z[ki * 5 + vi] = double(ki); // z = x -> vertical contours

  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  cm->setData(x, y, z);
  cm->setContourLevels({1.0, 2.0, 3.0});
  cm->setContourPen(QPen(Qt::red, 2));
  settleColorMap(mPlot, cm);
  QCoreApplication::processEvents();
  int recordedLines = 0;
  for (const auto& command : cm->mCore.recordedList().commands)
    recordedLines += std::get_if<qcp::PolylineDataCmd>(&command) != nullptr;
  QVERIFY2(recordedLines > 0, "sanity: the on-screen draw must have recorded the lines");

  const QPixmap pm = mPlot->toPixmap();
  QVERIFY(!pm.isNull());
  const int red = countRedPixels(pm.toImage());
  QVERIFY2(red > 50,
           qPrintable(QString("export after RHI draw must draw contour lines, red pixels=%1").arg(red)));
}

void TestColorMap::QCPColorMap2_contourExportCachesFallback()
{
  // The classic map now paints as a GPU command: on export it stacks above
  // painter-drawn plottables at the end of the layer's pass, as on screen. This
  // test is about the color map 2, so keep the fixture's classic map out.
  mColorMap->setVisible(false);
  // Repeated exports of one frame must reuse the export recording, not re-march the full grid
  // on every paint pass. The first export makes it (the screen one is only 300 cells wide).
  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setRange(0, 4);
  std::vector<double> x = {0, 1, 2, 3, 4}, y = {0, 1, 2, 3, 4}, z(25, 0.0);
  // NOTE: source Z layout is x-outer: z[xIndex * ySize + yIndex].
  for (int ki = 0; ki < 5; ++ki)
    for (int vi = 0; vi < 5; ++vi)
      z[ki * 5 + vi] = double(ki); // z = x -> vertical contours

  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  cm->setData(x, y, z);
  cm->setContourLevels({1.0, 2.0, 3.0});
  cm->setContourPen(QPen(Qt::red, 2));
  settleColorMap(mPlot, cm);
  QTRY_VERIFY_WITH_TIMEOUT(!cm->pipeline().isBusy(), 2000);

  const QImage img1 = mPlot->toPixmap().toImage();
  const quint64 recordings = cm->mCore.recordingCount();
  const QImage img2 = mPlot->toPixmap().toImage();
  QCOMPARE(cm->mCore.recordingCount(), recordings);
  QVERIFY2(countRedPixels(img1) > 50, "export must draw contour lines");
  QVERIFY(img1 == img2);
}

void TestColorMap::QCPColorMapData_fillNonFiniteKeepsBoundsDegenerate()
{
  QCPColorMapData data(2, 2, QCPRange(0, 1), QCPRange(0, 1));
  data.fill(std::numeric_limits<double>::infinity());
  QCOMPARE(data.cell(0, 0), std::numeric_limits<double>::infinity());
  QCOMPARE(data.dataBounds().lower, 0.0);
  QCOMPARE(data.dataBounds().upper, 0.0);
  data.fill(std::numeric_limits<double>::quiet_NaN());
  QCOMPARE(data.dataBounds().lower, 0.0);
  QCOMPARE(data.dataBounds().upper, 0.0);
  data.fill(2.5);
  QCOMPARE(data.dataBounds().lower, 2.5);
  QCOMPARE(data.dataBounds().upper, 2.5);
}

void TestColorMap::cleanup()
{
  delete mPlot;
}

// QCPColorScale::rescaleDataRange() only looked at classic QCPColorMaps: with a QCPColorMap2
// (or a QCPHistogram2D) attached it did nothing.
void TestColorMap::QCPColorScale_rescaleDataRangeIncludesColorMap2()
{
  auto* scale = new QCPColorScale(mPlot);
  mPlot->plotLayout()->addElement(0, 1, scale);
  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  cm->setColorScale(scale);
  std::vector<double> x = {0, 1, 2};
  std::vector<double> y = {0, 1};
  std::vector<double> z = {5, 6, 7, 8, 9, 10};
  cm->setData(x, y, z);

  bool found = false;
  scale->rescaleDataRange(false);
  QCOMPARE(scale->dataRange().lower, 5.0);
  QCOMPARE(scale->dataRange().upper, 10.0);

  // All the data, like a classic map: not the bounds of the resampled view, which averaging
  // narrows and which change with the zoom. A ramp over far more columns than pixels.
  std::vector<double> manyX(4000), twoY = {0, 1}, ramp(4000 * 2);
  for (int i = 0; i < 4000; ++i)
  {
    manyX[i] = i;
    ramp[i] = ramp[4000 + i] = 10.0 * i / 3999;
  }
  cm->setData(manyX, twoY, ramp);
  mPlot->xAxis->setRange(0, 4000);
  mPlot->yAxis->setRange(0, 1);
  mPlot->replot();
  QTRY_VERIFY_WITH_TIMEOUT(!cm->busy(), 5000);
  const QCPRange view = cm->colorDataBounds(found, false);
  QVERIFY2(found && view.size() < 10.0, "the resampled view should be narrower than the data");
  scale->rescaleDataRange(false);
  QCOMPARE(scale->dataRange().lower, 0.0);
  QCOMPARE(scale->dataRange().upper, 10.0);
}

// A resize changes no axis range, so draw() itself asks for a sharper resample. The request turns
// the pipeline busy, and a busyChanged slot may delete the colour map: it must not run mid-paint.
void TestColorMap::QCPColorMap2_drawEmitsNoBusyChange()
{
  mPlot->show();
  if (!QTest::qWaitForWindowExposed(mPlot))
    QSKIP("window not exposed in this environment");
  QCoreApplication::processEvents();
  if (!mPlot->rhi())
    QSKIP("no QRhi available in this environment");

  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setRange(0, 4);
  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  std::vector<double> x = {0, 1, 2, 3, 4}, y = {0, 1, 2, 3, 4}, z(25, 1.0);
  cm->setData(x, y, z);
  settleColorMap(mPlot, cm);

  int busyChanges = 0, busyChangesMidReplot = 0;
  connect(cm, &QCPAbstractPlottable::busyChanged, this, [&](bool) {
    ++busyChanges;
    busyChangesMidReplot += mPlot->isReplotting();
  });
  mPlot->resize(600, 450); // its resize event replots at once
  QCoreApplication::sendPostedEvents();
  QTRY_VERIFY_WITH_TIMEOUT(busyChanges > 0, 2000); // sanity: the sharper resample did run
  QCOMPARE(busyChangesMidReplot, 0);
  QTRY_VERIFY_WITH_TIMEOUT(!cm->pipeline().isBusy(), 2000);
}

namespace {
template <typename Plottable, typename Signal>
void removeOn(Plottable* plottable, Signal signal)
{
  QCustomPlot* plot = plottable->parentPlot();
  QObject::connect(plottable, signal, plot,
                   [plot, plottable] { QVERIFY(plot->removePlottable(plottable)); },
                   Qt::SingleShotConnection);
}

// Each colour setter of a GPU colour map: a slot deleting the map from the signal must end the
// setter, so the map is neither touched nor made to emit again.
template <typename Map>
void colourSettersSurviveADeletingSlot(QCustomPlot* plot)
{
  const auto check = [plot](auto signal, auto apply) {
    auto* map = new Map(plot->xAxis, plot->yAxis);
    map->setDataRange(QCPRange(-1, 10));
    removeOn(map, signal);
    apply(map);
    QCOMPARE(plot->plottableCount(), 0);
  };
  // The log scale sanitises the range, so dataRangeChanged comes first.
  check(&Map::dataRangeChanged, [](Map* m) { m->setDataScaleType(QCPAxis::stLogarithmic); });
  check(&Map::dataScaleTypeChanged, [](Map* m) { m->setDataScaleType(QCPAxis::stLogarithmic); });
  check(&Map::dataRangeChanged, [](Map* m) { m->setDataRange(QCPRange(2, 3)); });
  check(&Map::gradientChanged,
        [](Map* m) { m->setGradient(QCPColorGradient(QCPColorGradient::gpHot)); });
  auto* scale = new QCPColorScale(plot);
  scale->setGradient(QCPColorGradient(QCPColorGradient::gpHot));
  scale->setDataScaleType(QCPAxis::stLogarithmic);
  check(&Map::gradientChanged, [scale](Map* m) { m->setColorScale(scale); });
  check(&Map::dataScaleTypeChanged, [scale](Map* m) { m->setColorScale(scale); });
  delete scale;
}
} // namespace

void TestColorMap::QCPColorMap2_colourSlotDeletingTheMapIsSafe()
{
  QVERIFY(mPlot->removePlottable(mColorMap));
  colourSettersSurviveADeletingSlot<QCPColorMap2>(mPlot);
}

void TestColorMap::QCPHistogram2D_colourSlotDeletingTheMapIsSafe()
{
  QVERIFY(mPlot->removePlottable(mColorMap));
  colourSettersSurviveADeletingSlot<QCPHistogram2D>(mPlot);
}

// The classic map sanitises its range for the log scale after it signals the scale type.
void TestColorMap::QCPColorMap_scaleTypeSlotDeletingTheMapIsSafe()
{
  mColorMap->setDataRange(QCPRange(-1, 10));
  removeOn(mColorMap, &QCPColorMap::dataScaleTypeChanged);
  mColorMap->setDataScaleType(QCPAxis::stLogarithmic);
  QCOMPARE(mPlot->plottableCount(), 0);
}

// The scale signals its sanitised range, then its scale type.
void TestColorMap::QCPColorScale_rangeSlotDeletingTheScaleIsSafe()
{
  auto* scale = new QCPColorScale(mPlot);
  scale->setDataRange(QCPRange(-1, 10));
  const QPointer<QCPColorScale> alive(scale);
  int scaleTypeSignals = 0;
  connect(scale, &QCPColorScale::dataScaleTypeChanged, mPlot, [&] { ++scaleTypeSignals; });
  connect(scale, &QCPColorScale::dataRangeChanged, mPlot, [scale] { delete scale; },
          Qt::SingleShotConnection);
  scale->setDataScaleType(QCPAxis::stLogarithmic);
  QVERIFY(!alive);
  QCOMPARE(scaleTypeSignals, 0);
}

// A vertical key axis turns the picture: the grid's key runs down the screen. A hot block at
// key upper, value lower is a wide strip at the top-left; a merely rotated image would put a
// tall strip there instead.
void TestColorMap::colourMap2HonoursAVerticalKeyAxis()
{
  QVERIFY(mPlot->removePlottable(mColorMap));
  mPlot->resize(200, 200);
  mPlot->axisRect()->setAutoMargins(QCP::msNone);
  mPlot->axisRect()->setMargins(QMargins(0, 0, 0, 0));
  mPlot->yAxis->setRange(0, 7);
  mPlot->xAxis->setRange(0, 3);

  auto* map = new QCPColorMap2(mPlot->yAxis, mPlot->xAxis);
  std::vector<double> keys, values, z;
  for (int k = 0; k < 8; ++k)
    keys.push_back(k);
  for (int v = 0; v < 4; ++v)
    values.push_back(v);
  for (int k = 0; k < 8; ++k)
    for (int v = 0; v < 4; ++v)
      z.push_back(k >= 6 && v <= 1 ? 1.0 : 0.0);
  map->setData(std::move(keys), std::move(values), std::move(z));
  map->setGradient(QCPColorGradient(QCPColorGradient::gpGrayscale));
  map->setDataRange(QCPRange(0, 1));
  settleColorMap(mPlot, map);

  const QImage exported = mPlot->toPixmap(200, 200).toImage().convertToFormat(QImage::Format_ARGB32);
  const QRect area = mPlot->axisRect()->rect();
  const auto at = [&](double fx, double fy) {
    return exported.pixel(area.left() + qRound(fx * area.width()),
                          area.top() + qRound(fy * area.height()));
  };
  QVERIFY2(qGray(at(0.4, 0.15)) > 200, "the key-upper, value-lower block is not at the top-left");
  QVERIFY2(qGray(at(0.15, 0.4)) < 50, "the picture is rotated, not turned onto the vertical key axis");
}

// Same picture for a histogram, whose cells are not resampled: key column upper, value row
// lower is the screen's top-left, one cell tall and two cells wide.
void TestColorMap::histogram2dHonoursAVerticalKeyAxis()
{
  QVERIFY(mPlot->removePlottable(mColorMap));
  mPlot->resize(200, 200);
  mPlot->axisRect()->setAutoMargins(QCP::msNone);
  mPlot->axisRect()->setMargins(QMargins(0, 0, 0, 0));

  auto* hist = new QCPHistogram2D(mPlot->yAxis, mPlot->xAxis);
  hist->setBins(4, 2);
  std::vector<double> keys, values;
  for (int k = 0; k < 4; ++k)
    for (int v = 0; v < 2; ++v)
      for (int n = 0; n < (k == 3 && v == 0 ? 6 : 1); ++n)
      {
        keys.push_back(0.5 + k);
        values.push_back(0.5 + v);
      }
  hist->setData(std::move(keys), std::move(values));
  hist->setGradient(QCPColorGradient(QCPColorGradient::gpGrayscale));
  hist->setDataRange(QCPRange(0, 6));
  QSignalSpy spy(&hist->pipeline(), &QCPHistogramPipeline::finished);
  QVERIFY(spy.wait(2000));
  mPlot->yAxis->setRange(0.5, 3.5);
  mPlot->xAxis->setRange(0.5, 1.5);

  const QImage exported = mPlot->toPixmap(200, 200).toImage().convertToFormat(QImage::Format_ARGB32);
  const QRect area = mPlot->axisRect()->rect();
  const auto at = [&](double fx, double fy) {
    return exported.pixel(area.left() + qRound(fx * area.width()),
                          area.top() + qRound(fy * area.height()));
  };
  QVERIFY2(qGray(at(0.4, 0.15)) > 200, "the hot cell is not at the top-left");
  QVERIFY2(qGray(at(0.15, 0.4)) < 60, "the picture is rotated, not turned onto the vertical key axis");
}

// The label tile is painted in screen pixels too: its geometry must follow the axes' own
// orientation, or every label lands at a swapped coordinate on a vertical key axis.
void TestColorMap::contourLabelsFollowAVerticalKeyAxis()
{
  QVERIFY(mPlot->removePlottable(mColorMap));
  auto* hist = new QCPHistogram2D(mPlot->yAxis, mPlot->xAxis);
  hist->setBins(3, 2);
  const double keyOfColumn[3] = { 1.0, 11.0, 21.0 };
  const int samplesOfColumn[3] = { 1, 3, 3 };
  std::vector<double> keys, values;
  for (int column = 0; column < 3; ++column)
    for (int row = 0; row < 2; ++row)
      for (int sample = 0; sample < samplesOfColumn[column]; ++sample)
      {
        keys.push_back(keyOfColumn[column]);
        values.push_back(row == 0 ? 0.25 : 0.75);
      }
  hist->setData(std::move(keys), std::move(values));
  mPlot->yAxis->setRange(0, 30);
  mPlot->xAxis->setRange(0, 1);
  hist->setContourLevels({ 2.0 });
  hist->setContourPen(QPen(Qt::red, 2));
  hist->setContourLabelEnabled(true);
  QSignalSpy spy(&hist->pipeline(), &QCPHistogramPipeline::finished);
  QVERIFY(spy.wait(2000));
  mPlot->toPixmap();

  const auto& labels = hist->lastContourLabels();
  QCOMPARE(labels.size(), 1);
  // The label sits at the segment's midpoint: average the segment's two endpoints.
  double key = 0, value = 0;
  int points = 0;
  for (const auto& command : hist->mCore.recordedList().commands)
    if (const auto* line = std::get_if<qcp::PolylineDataCmd>(&command))
      for (const QPointF& candidate : *line->points)
        if (!std::isnan(candidate.x()) && points < 2)
        {
          key += candidate.x();
          value += candidate.y();
          ++points;
        }
  QVERIFY2(points == 2, "the recording holds one contour segment");
  key /= points;
  value /= points;
  const QPointF linePixel(mPlot->xAxis->coordToPixel(value), mPlot->yAxis->coordToPixel(key));
  QVERIFY2(std::abs(labels.first().position.x() - linePixel.x()) < 2.0
               && std::abs(labels.first().position.y() - linePixel.y()) < 2.0,
           qPrintable(QString("label at (%1, %2), its line at (%3, %4)")
                          .arg(labels.first().position.x())
                          .arg(labels.first().position.y())
                          .arg(linePixel.x())
                          .arg(linePixel.y())));
}

// A taken colour map drops its plot's scale, and joins the scale of the rect it is added to.
void TestColorMap::colourMap2JoinsItsNewPlotsScaleWhenAdded()
{
  QVERIFY(mPlot->removePlottable(mColorMap)); // the fixture's map shares the rect's scale
  auto* map = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  const auto oldScale = mPlot->axisRect()->colorScale();
  QVERIFY(oldScale);
  QVERIFY(qcp::pointers(oldScale->users()).contains(map));

  QCustomPlot other;
  other.resize(400, 300);
  auto taken = mPlot->take(qcp::makeHandle<QCPHandle<QCPColorMap2>>(map));
  QVERIFY(taken);
  QVERIFY2(oldScale->users().isEmpty(), "the taken map still uses its old plot's scale");

  auto added = other.add(std::move(taken));
  QVERIFY(added);
  QVERIFY2(other.axisRect()->colorScale(), "the re-added map did not make the new rect's scale");
  QCOMPARE(added->colorScale(), other.axisRect()->colorScale().get());
  QVERIFY(qcp::pointers(other.axisRect()->colorScale()->users()).contains(added.get()));
}

// The scale signals its gradient before it applies it to its users: the slot must run once the
// users are up to date, and deleting the scale there must not leave it walking freed memory.
void TestColorMap::QCPColorScale_gradientSlotDeletingTheScaleIsSafe()
{
  auto* scale = new QCPColorScale(mPlot);
  scale->setGradient(QCPColorGradient(QCPColorGradient::gpHot));
  auto* map = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  map->setColorScale(scale);
  const QPointer<QCPColorScale> alive(scale);
  QCPColorGradient seen = map->gradient();
  connect(scale, &QCPColorScale::gradientChanged, mPlot,
          [&seen, map, scale](const QCPColorGradient&) {
              seen = map->gradient();
              delete scale;
          },
          Qt::SingleShotConnection);
  scale->setGradient(QCPColorGradient(QCPColorGradient::gpCold));
  QVERIFY(!alive);
  QCOMPARE(seen, QCPColorGradient(QCPColorGradient::gpCold));
}

namespace {
// The map kept a raw pointer to its scale: after the scale died, the next setColorScale
// disconnected from freed memory.
template <typename Map>
void forgetsADeletedColorScale(QCustomPlot* plot, Map* map)
{
  auto* scale = new QCPColorScale(plot);
  map->setColorScale(scale);
  int forgotten = 0;
  QObject::connect(map, &Map::colorScaleChanged, plot, [&forgotten] { ++forgotten; });
  delete scale;
  QCOMPARE(forgotten, 1);
  QCOMPARE(map->colorScale(), nullptr);
  map->setColorScale(nullptr);
  auto* other = new QCPColorScale(plot);
  map->setColorScale(other);
  QCOMPARE(map->colorScale(), other);
  other->setDataRange(QCPRange(1, 2)); // connected
  QCOMPARE(map->dataRange(), QCPRange(1, 2));
  delete other;
}
} // namespace

void TestColorMap::QCPColorMap2_forgetsADeletedColorScale()
{
  forgetsADeletedColorScale(mPlot, new QCPColorMap2(mPlot->xAxis, mPlot->yAxis));
}

void TestColorMap::QCPHistogram2D_forgetsADeletedColorScale()
{
  forgetsADeletedColorScale(mPlot, new QCPHistogram2D(mPlot->xAxis, mPlot->yAxis));
}

void TestColorMap::QCPColorMap_forgetsADeletedColorScale()
{
  forgetsADeletedColorScale(mPlot, mColorMap);
}

// The classic map applied the scale's gradient, range and type, then connected, unguarded.
void TestColorMap::QCPColorMap_colorScaleSlotDeletingTheMapIsSafe()
{
  auto* scale = new QCPColorScale(mPlot);
  scale->setGradient(QCPColorGradient(QCPColorGradient::gpHot));
  scale->setDataScaleType(QCPAxis::stLogarithmic);
  removeOn(mColorMap, &QCPColorMap::gradientChanged);
  mColorMap->setColorScale(scale);
  QCOMPARE(mPlot->plottableCount(), 0);
  QCOMPARE(scale->dataScaleType(), QCPAxis::stLogarithmic);
}

// One scale, three kinds of user: rescaling must see the bounds of each of them.
void TestColorMap::QCPColorScale_rescaleSeesEveryUser()
{
    auto* scale = new QCPColorScale(mPlot);
    mPlot->plotLayout()->addElement(0, 1, scale);

    auto* map = new QCPColorMap(mPlot->xAxis, mPlot->yAxis);
    map->setColorScale(scale);
    map->data()->setSize(1, 1);
    map->data()->setCell(0, 0, 1.0);
    map->data()->recalculateDataBounds();

    auto* map2 = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
    map2->setColorScale(scale);
    map2->setData(std::vector<double> { 0, 1 }, std::vector<double> { 0, 1 },
                  std::vector<double>(4, 5.0));

    auto* hist = new QCPHistogram2D(mPlot->xAxis, mPlot->yAxis);
    hist->setColorScale(scale);
    hist->setBins(1, 1);
    hist->setData(std::vector<double>(9, 0.5), std::vector<double>(9, 0.5));
    QTRY_VERIFY_WITH_TIMEOUT(!hist->pipeline().isBusy(), 2000);
    QTRY_VERIFY_WITH_TIMEOUT(hist->pipeline().result() != nullptr, 2000);

    scale->rescaleDataRange(false);
    QCOMPARE(scale->dataRange().lower, 1.0);
    QCOMPARE(scale->dataRange().upper, 9.0);
    QCOMPARE(map->dataRange().upper, 9.0);
    QCOMPARE(map2->dataRange().upper, 9.0);
    QCOMPARE(hist->dataRange().upper, 9.0);
}

// A setter on one user reaches the scale, and from there every other user.
void TestColorMap::QCPColorScale_aSetterOnOneUserReachesTheOthers()
{
    auto* scale = new QCPColorScale(mPlot);
    auto* map = new QCPColorMap(mPlot->xAxis, mPlot->yAxis);
    auto* map2 = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
    map->setColorScale(scale);
    map2->setColorScale(scale);

    const QCPColorGradient hot(QCPColorGradient::gpHot);
    map2->setGradient(hot);
    QCOMPARE(map->gradient(), hot);
    QCOMPARE(scale->gradient(), hot);

    map2->setDataRange(QCPRange(1, 2));
    QCOMPARE(map->dataRange(), QCPRange(1, 2));
    QCOMPARE(scale->dataRange(), QCPRange(1, 2));

    map2->setDataScaleType(QCPAxis::stLogarithmic);
    QCOMPARE(map->dataScaleType(), QCPAxis::stLogarithmic);
    QCOMPARE(scale->dataScaleType(), QCPAxis::stLogarithmic);
}

// Attaching takes the scale's gradient, range and scale type.
void TestColorMap::QCPColorScale_attachingTakesTheScaleValues()
{
    auto* scale = new QCPColorScale(mPlot);
    const QCPColorGradient hot(QCPColorGradient::gpHot);
    scale->setGradient(hot);
    scale->setDataRange(QCPRange(2, 4));
    scale->setDataScaleType(QCPAxis::stLogarithmic);

    mColorMap->setColorScale(scale);
    QCOMPARE(mColorMap->gradient(), hot);
    QCOMPARE(mColorMap->dataRange(), scale->dataRange());
    QCOMPARE(mColorMap->dataScaleType(), scale->dataScaleType());
}

// Deleting the scale forgets it on every user, keeps their values, and tells each of them once.
void TestColorMap::QCPColorScale_deletingTheScaleIsNoticed()
{
    auto* scale = new QCPColorScale(mPlot);
    auto* map = new QCPColorMap(mPlot->xAxis, mPlot->yAxis);
    auto* map2 = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
    auto* hist = new QCPHistogram2D(mPlot->xAxis, mPlot->yAxis);
    map->setColorScale(scale);
    map2->setColorScale(scale);
    hist->setColorScale(scale);
    scale->setGradient(QCPColorGradient(QCPColorGradient::gpHot));
    scale->setDataRange(QCPRange(3, 7));

    int forgotten[3] = {};
    connect(map, &QCPColorMap::colorScaleChanged, map, [&] { ++forgotten[0]; });
    connect(map2, &QCPColorMap2::colorScaleChanged, map2, [&] { ++forgotten[1]; });
    connect(hist, &QCPHistogram2D::colorScaleChanged, hist, [&] { ++forgotten[2]; });

    delete scale;
    for (int i = 0; i < 3; ++i)
        QCOMPARE(forgotten[i], 1);
    for (QCPAbstractColorMapped* user : { static_cast<QCPAbstractColorMapped*>(map),
                                          static_cast<QCPAbstractColorMapped*>(map2),
                                          static_cast<QCPAbstractColorMapped*>(hist) })
    {
        QCOMPARE(user->colorScale(), nullptr);
        QCOMPARE(user->gradient(), QCPColorGradient(QCPColorGradient::gpHot));
        QCOMPARE(user->dataRange(), QCPRange(3, 7));
    }
}

// A user that goes away leaves the scale's list.
void TestColorMap::QCPColorScale_deletingAUserLeavesTheScaleList()
{
    auto* scale = new QCPColorScale(mPlot);
    auto* map = new QCPColorMap(mPlot->xAxis, mPlot->yAxis);
    auto* map2 = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
    map->setColorScale(scale);
    map2->setColorScale(scale);
    QCOMPARE(scale->users().size(), 2);

    delete map;
    QCOMPARE(scale->users().size(), 1);
    QCOMPARE(scale->users().first(), static_cast<QCPAbstractColorMapped*>(map2));
}

// The colour-map image is a GPU quad composited after the layer's paint buffer: labels painted
// into that buffer end up under the image.
void TestColorMap::QCPColorMap2_contourLabelsShowOnTheGpuFrame()
{
  // The classic map now paints as a GPU command: it stacks over painter-drawn
  // content of the same layer, on screen and on export. This test is about the
  // color map 2, so keep the fixture's classic map out.
  mColorMap->setVisible(false);
  if (!fontDrawsGlyphs())
    QSKIP("no usable font: labels draw no pixels");
  mPlot->resize(400, 300);
  mPlot->show();
  if (!QTest::qWaitForWindowExposed(mPlot) || (QCoreApplication::processEvents(), !mPlot->rhi()))
    QSKIP("no QRhi available in this environment");
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setRange(0, 4);
  const int n = 41;
  std::vector<double> x(n), y(n), z(n * n);
  for (int i = 0; i < n; ++i)
    x[i] = y[i] = 4.0 * i / (n - 1);
  for (int ki = 0; ki < n; ++ki)
    for (int vi = 0; vi < n; ++vi)
      z[ki * n + vi] = x[ki];
  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  cm->setData(x, y, z);
  cm->setContourLevels({1.0, 2.0, 3.0});
  cm->setContourPen(QPen(Qt::red, 1));
  settleColorMap(mPlot, cm);
  QVERIFY2(cm->mCore.recordedCommandCount() > 0, "sanity: the image is recorded");

  mPlot->replot(QCustomPlot::rpImmediateRefresh);
  const QImage without = mPlot->grabFramebuffer();
  cm->setContourLabelEnabled(true);
  mPlot->replot(QCustomPlot::rpImmediateRefresh);
  const QImage with = mPlot->grabFramebuffer();
  QVERIFY(!cm->lastContourLabels().isEmpty());

  int changed = 0;
  for (int py = 0; py < with.height(); ++py)
    for (int px = 0; px < with.width(); ++px)
      changed += with.pixel(px, py) != without.pixel(px, py);
  QVERIFY2(changed > 100, qPrintable(QString("labels must show on screen: %1 pixels changed").arg(changed)));
}

namespace
{
//! A shown plot with a 41×41 colour map (z = x) and vertical contours at 1, 2, 3; null without QRhi.
QCPColorMap2* contouredGpuMap(QCustomPlot* plot)
{
  plot->resize(400, 300);
  plot->show();
  if (!QTest::qWaitForWindowExposed(plot) || (QCoreApplication::processEvents(), !plot->rhi()))
    return nullptr;
  plot->xAxis->setRange(0, 4);
  plot->yAxis->setRange(0, 4);
  const int n = 41;
  std::vector<double> x(n), y(n), z(n * n);
  for (int i = 0; i < n; ++i)
    x[i] = y[i] = 4.0 * i / (n - 1);
  for (int ki = 0; ki < n; ++ki)
    for (int vi = 0; vi < n; ++vi)
      z[ki * n + vi] = x[ki];
  auto* cm = new QCPColorMap2(plot->xAxis, plot->yAxis);
  cm->setData(x, y, z);
  // A grayscale image and a red pen: red pixels are the contour lines, nothing else.
  cm->setGradient(QCPColorGradient::gpGrayscale);
  cm->setDataRange(QCPRange(0, 4));
  cm->setContourLevels({1.0, 2.0, 3.0});
  cm->setContourPen(QPen(Qt::red, 2));
  settleColorMap(plot, cm);
  return cm;
}
}

// Panning past the data hides the GPU quad and its lines; panning back before a new resample
// lands shows the image again, and the lines must come back with it.
void TestColorMap::QCPColorMap2_contourLinesComeBackAfterAPanAwayAndBack()
{
  auto* cm = contouredGpuMap(mPlot);
  if (!cm)
    QSKIP("no QRhi available in this environment");
  QVERIFY2(countRedPixels(mPlot->grabFramebuffer()) > 20, "sanity: the lines are drawn");
  mPlot->xAxis->setRange(1000, 1004);
  mPlot->replot();
  QVERIFY2(countRedPixels(mPlot->grabFramebuffer()) < 20, "nothing stale draws over the panned axes");
  mPlot->xAxis->setRange(0, 4);
  mPlot->replot();
  QVERIFY2(countRedPixels(mPlot->grabFramebuffer()) > 20, "the contour lines must be back with the image");
}

// The label overlay is a full axis-rect texture: a repaint that changes nothing must not upload it.
void TestColorMap::QCPColorMap2_unchangedLabelsAreNotUploadedAgain()
{
  auto* cm = contouredGpuMap(mPlot);
  if (!cm)
    QSKIP("no QRhi available in this environment");
  cm->setContourLabelEnabled(true);
  mPlot->replot(QCustomPlot::rpImmediateRefresh);
  mPlot->grabFramebuffer(); // renders, so the labels are recorded
  QVERIFY(!cm->lastContourLabels().isEmpty());
  const quint64 recordings = cm->mCore.recordingCount();
  cm->layer()->markDirty();
  mPlot->replot(QCustomPlot::rpImmediateRefresh);
  QCOMPARE(cm->mCore.recordingCount(), recordings);
  QVERIFY(!cm->lastContourLabels().isEmpty());
}

void TestColorMap::QCPColorMap2_contourLabelsDrawSpacedInsideThePlot()
{
  // The classic map now paints as a GPU command: on export it stacks above
  // painter-drawn plottables at the end of the layer's pass, as on screen. This
  // test is about the color map 2, so keep the fixture's classic map out.
  mColorMap->setVisible(false);
  if (!fontDrawsGlyphs())
    QSKIP("no usable font: labels draw no pixels");
  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setRange(0, 4);
  const int n = 41;
  std::vector<double> x(n), y(n), z(n * n);
  for (int i = 0; i < n; ++i)
    x[i] = y[i] = 4.0 * i / (n - 1);
  // NOTE: source Z layout is x-outer: z[xIndex * ySize + yIndex].
  for (int ki = 0; ki < n; ++ki)
    for (int vi = 0; vi < n; ++vi)
      z[ki * n + vi] = x[ki]; // z = x -> vertical contours
  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  cm->setData(x, y, z);
  settleColorMap(mPlot, cm);
  cm->setContourLevels({1.0, 2.0, 3.0});
  cm->setContourPen(QPen(Qt::red, 1));
  QTRY_VERIFY_WITH_TIMEOUT(!cm->pipeline().isBusy(), 2000);

  const QImage without = mPlot->toPixmap().toImage();
  QVERIFY(cm->lastContourLabels().isEmpty());
  cm->setContourLabelEnabled(true);
  const QImage with = mPlot->toPixmap().toImage();
  const auto labels = cm->lastContourLabels();

  int changed = 0;
  for (int py = 0; py < with.height(); ++py)
    for (int px = 0; px < with.width(); ++px)
      changed += with.pixel(px, py) != without.pixel(px, py);
  QVERIFY2(changed > 100, qPrintable(QString("labels must draw: %1 pixels changed").arg(changed)));
  QSet<double> levels;
  const QRectF area = mPlot->axisRect()->rect();
  for (const auto& label : labels)
  {
    levels.insert(label.level);
    QVERIFY(area.contains(label.position));
  }
  QCOMPARE(levels, (QSet<double>{1.0, 2.0, 3.0}));
  for (int i = 0; i < labels.size(); ++i)
    for (int j = i + 1; j < labels.size(); ++j)
    {
      const double d = QLineF(labels[i].position, labels[j].position).length();
      QVERIFY2(d >= QCPColorMap2::kContourLabelClearance, "labels must not overlap");
      if (labels[i].level == labels[j].level)
        QVERIFY2(d >= QCPColorMap2::kContourLabelSpacing, "labels of one level must be spaced");
    }
}

// rescaleDataRange() on a colour map 2 fits the resampled view; with true it fits all the data.
// Called through the base, as generic code does.
void TestColorMap::QCPColorMap2_rescaleKeepsItsViewMeaning()
{
  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  std::vector<double> manyX(4000), twoY = {0, 1}, ramp(4000 * 2);
  for (int i = 0; i < 4000; ++i)
  {
    manyX[i] = i;
    ramp[i] = ramp[4000 + i] = 10.0 * i / 3999;
  }
  cm->setData(manyX, twoY, ramp);
  mPlot->xAxis->setRange(0, 4000);
  mPlot->yAxis->setRange(0, 1);
  mPlot->replot();
  QTRY_VERIFY_WITH_TIMEOUT(!cm->busy(), 5000);
  QCPAbstractColorMapped* mapped = cm;
  mapped->rescaleDataRange(false);
  QVERIFY2(cm->dataRange().size() < 10.0, "the view's range should be narrower than the data");
  mapped->rescaleDataRange(true);
  QCOMPARE(cm->dataRange().lower, 0.0);
  QCOMPARE(cm->dataRange().upper, 10.0);
}

// A maximum lowered after the bounds were cached is only seen when the call asks to recalculate.
void TestColorMap::QCPColorMap_rescaleThroughTheBaseRecalculates()
{
  auto* map = new QCPColorMap(mPlot->xAxis, mPlot->yAxis);
  map->data()->setSize(2, 2);
  map->data()->setRange(QCPRange(0, 1), QCPRange(0, 1));
  map->data()->setCell(0, 0, 1);
  map->data()->setCell(1, 1, 2);
  map->data()->recalculateDataBounds();
  map->data()->setCell(1, 1, 1.5); // a lower maximum: the cached bounds keep 2
  QCPAbstractColorMapped* mapped = map;
  mapped->rescaleDataRange(true);
  QCOMPARE(map->dataRange().upper, 1.5);
}

// A classic colour map records one image and nothing else: the corners span the
// data range grown by half a cell, so the picture covers the outer half cells.
void TestColorMap::colourMapIsOneImageCommand()
{
  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 10);
  mPlot->yAxis->setRange(0, 10);
  auto* map = classicColorMap(mPlot, 10);
  map->setInterpolate(false);
  mPlot->replot();

  QCOMPARE(map->mCore.recordingCount(), 1);
  QCOMPARE(map->mCore.recordedCommandCount(), 1);
  const auto& commands = map->mCore.recordedList().commands;
  QCOMPARE(int(commands.size()), 1);
  const auto* image = std::get_if<qcp::ImageCmd>(&commands.front());
  QVERIFY2(image, "a colour map records one image and nothing else");
  QVERIFY(image->pixels && !image->pixels->image.isNull());
  QVERIFY(!image->pixels->smooth);
  const auto* a = std::get_if<QCP::Data>(&image->a);
  const auto* b = std::get_if<QCP::Data>(&image->b);
  QVERIFY(a && b);
  const double half = 0.5 * 10.0 / 9.0;
  QVERIFY2(qAbs(a->x + half) < 1e-12,
           "a is the top-left texel, half a cell left of the key range");
  QVERIFY2(qAbs(a->y - (10.0 + half)) < 1e-12, "a is half a cell above the value range");
  QVERIFY2(qAbs(b->x - (10.0 + half)) < 1e-12, "b is half a cell right of the key range");
  QVERIFY2(qAbs(b->y + half) < 1e-12,
           "b is the bottom-right texel, half a cell below the value range");
}

// With a vertical key axis the image is transposed; the GPU frame and the
// painter export must agree on where every cell lands.
void TestColorMap::verticalKeyAxisMapMatchesTheExport()
{
  mPlot->resize(400, 300);
  mPlot->axisRect()->setAutoMargins(QCP::msNone);
  mPlot->axisRect()->setMargins(QMargins(20, 20, 20, 20));
  mPlot->setBackground(QBrush(Qt::white));
  mPlot->axisRect()->setBackground(QBrush(Qt::white));
  mPlot->xAxis->setVisible(false);
  mPlot->yAxis->setVisible(false);
  mPlot->show();
  if (!QTest::qWaitForWindowExposed(mPlot) || (QCoreApplication::processEvents(), !mPlot->rhi()))
    QSKIP("no QRhi available in this environment");

  auto* map = new QCPColorMap(mPlot->yAxis, mPlot->xAxis);
  map->data()->setSize(40, 30);
  map->data()->setRange(QCPRange(0, 10), QCPRange(0, 12));
  for (int k = 0; k < 40; ++k)
    for (int v = 0; v < 30; ++v)
      map->data()->setCell(k, v, double((k * 13 + v * 7) % 40));
  map->setGradient(QCPColorGradient::gpHot);
  map->setInterpolate(false);
  map->rescaleDataRange();
  mPlot->yAxis->setRange(0, 10);
  mPlot->xAxis->setRange(0, 12);
  mPlot->replot(QCustomPlot::rpImmediateRefresh);

  const QImage gpu = mPlot->grabFramebuffer();
  const QImage exported = mPlot->toPixmap(gpu.width(), gpu.height()).toImage();
  QCOMPARE(gpu.size(), exported.size());
  QVERIFY(gpu.width() > 40 && gpu.height() > 40);
  for (int y = 0; y < gpu.height(); y += 10)
    for (int x = 0; x < gpu.width(); x += 10)
    {
      const QRgb frame = gpu.pixel(x, y);
      const QRgb picture = exported.pixel(x, y);
      const bool close = qAbs(qRed(frame) - qRed(picture)) <= 2
          && qAbs(qGreen(frame) - qGreen(picture)) <= 2
          && qAbs(qBlue(frame) - qBlue(picture)) <= 2;
      QVERIFY2(close, qPrintable(QStringLiteral("pixel (%1, %2) differs").arg(x).arg(y)));
    }
}

// The tight boundary keeps the corners on the data range and crops half a cell
// of the oversampled image on each side instead.
void TestColorMap::tightBoundaryCropsHalfACell()
{
  auto* map = classicColorMap(mPlot, 10);
  map->setInterpolate(false); // oversampling factor 11, image 110x110
  map->setTightBoundary(true);
  mPlot->replot();

  const auto& tightCommands = map->mCore.recordedList().commands;
  QCOMPARE(int(tightCommands.size()), 1);
  const auto* tight = std::get_if<qcp::ImageCmd>(&tightCommands.front());
  QVERIFY(tight);
  const auto* a = std::get_if<QCP::Data>(&tight->a);
  const auto* b = std::get_if<QCP::Data>(&tight->b);
  QVERIFY(a && b);
  QCOMPARE(a->x, 0.0);
  QCOMPARE(a->y, 10.0);
  QCOMPARE(b->x, 10.0);
  QCOMPARE(b->y, 0.0);
  QCOMPARE(tight->pixels->image.size(), QSize(110, 110));
  QCOMPARE(tight->pixels->source, QRectF(5.5, 5.5, 99, 99));

  // Without oversampling one texel is one cell, so half a cell is half a texel.
  map->setInterpolate(true);
  mPlot->replot();
  const auto& smoothCommands = map->mCore.recordedList().commands;
  QCOMPARE(int(smoothCommands.size()), 1);
  const auto* smooth = std::get_if<qcp::ImageCmd>(&smoothCommands.front());
  QVERIFY(smooth);
  QCOMPARE(smooth->pixels->image.size(), QSize(10, 10));
  QCOMPARE(smooth->pixels->source, QRectF(0.5, 0.5, 9, 9));
  QVERIFY(smooth->pixels->smooth);

  // A loose boundary draws the whole image and grows the corners by half a cell.
  map->setInterpolate(false);
  map->setTightBoundary(false);
  mPlot->replot();
  const auto& looseCommands = map->mCore.recordedList().commands;
  QCOMPARE(int(looseCommands.size()), 1);
  const auto* loose = std::get_if<qcp::ImageCmd>(&looseCommands.front());
  QVERIFY(loose);
  const auto* looseA = std::get_if<QCP::Data>(&loose->a);
  const auto* looseB = std::get_if<QCP::Data>(&loose->b);
  QVERIFY(looseA && looseB);
  const double half = 0.5 * 10.0 / 9.0;
  QVERIFY(qAbs(looseA->x + half) < 1e-12);
  QVERIFY(qAbs(looseB->x - (10.0 + half)) < 1e-12);
  QCOMPARE(loose->pixels->source, QRectF(0, 0, 110, 110));
}

// A pan moves the picture without rebuilding it and without a new recording;
// new cells do both.
void TestColorMap::aPanDoesNotRebuildTheImage()
{
  auto* map = classicColorMap(mPlot, 10);
  mPlot->replot();
  QCOMPARE(map->mImageBuildCount, 1);
  QCOMPARE(map->mCore.recordingCount(), 1);

  map->data()->setCell(0, 0, 1.0);
  mPlot->replot();
  QCOMPARE(map->mImageBuildCount, 2);
  QCOMPARE(map->mCore.recordingCount(), 2);

  for (int i = 0; i < 5; ++i)
  {
    mPlot->xAxis->setRange(1.0 + i, 11.0 + i);
    mPlot->replot();
  }
  QCOMPARE(map->mImageBuildCount, 2);
  QCOMPARE(map->mCore.recordingCount(), 2);
}

// The exported picture of a plain map must stay the one the painter drew before
// the map recorded an image. The reference was rendered by the previous code.
// The image is 399x399 and the plot area 398x398, so both codes sample the very
// same texel for every pixel.
void TestColorMap::exportMatchesTheOldCode()
{
  mPlot->resize(400, 400);
  mPlot->axisRect()->setAutoMargins(QCP::msNone);
  mPlot->axisRect()->setMargins(QMargins(1, 1, 1, 1));
  mPlot->setBackground(QBrush(Qt::white));
  mPlot->axisRect()->setBackground(QBrush(Qt::white));
  mPlot->xAxis->setVisible(false);
  mPlot->yAxis->setVisible(false);

  auto* map = new QCPColorMap(mPlot->xAxis, mPlot->yAxis);
  map->data()->setSize(399, 399);
  map->data()->setRange(QCPRange(0, 10), QCPRange(0, 10));
  for (int k = 0; k < 399; ++k)
    for (int v = 0; v < 399; ++v)
      map->data()->setCell(k, v, double((k * 31 + v * 17) % 200));
  map->setGradient(QCPColorGradient::gpHot);
  map->setInterpolate(false);
  map->rescaleDataRange();
  mPlot->xAxis->setRange(0, 10);
  mPlot->yAxis->setRange(0, 10);

  const QImage exported
      = mPlot->toPixmap(400, 400).toImage().convertToFormat(QImage::Format_ARGB32);
  QString why;
  const bool matches = qcp::test::matchesReference(
      exported, QStringLiteral("test-colormap"), QStringLiteral("classic-colormap-export.png"),
      &why, qcp::test::Tolerance{0, 0}, mPlot->axisRect()->rect());
  if (qEnvironmentVariableIsSet("QCP_WRITE_REFERENCE"))
    QSKIP("reference image written");
  QVERIFY2(matches, qPrintable(why));
}

// One hot cell at key upper, value lower, in a 2x2 map: each axis layout puts it in
// a known quarter of the export. Guards the image corners on every orientation.
void TestColorMap::theHotCellLandsInItsCornerOnEveryAxisLayout()
{
  struct Layout
  {
    const char* name;
    bool keyIsVertical, keyReversed, valueReversed;
    bool hotOnRight, hotOnTop;
  };
  const Layout layouts[] = {
    { "normal", false, false, false, true, false },
    { "key reversed", false, true, false, false, false },
    { "value reversed", false, false, true, true, true },
    { "vertical key", true, false, false, false, true },
    { "vertical key reversed", true, true, false, false, false },
  };
  mColorMap->setVisible(false);
  mPlot->resize(200, 200);
  mPlot->axisRect()->setAutoMargins(QCP::msNone);
  mPlot->axisRect()->setMargins(QMargins(0, 0, 0, 0));
  for (const Layout& layout : layouts)
  {
    QCPAxis* keyAxis = layout.keyIsVertical ? mPlot->yAxis : mPlot->xAxis;
    QCPAxis* valueAxis = layout.keyIsVertical ? mPlot->xAxis : mPlot->yAxis;
    keyAxis->setRangeReversed(layout.keyReversed);
    valueAxis->setRangeReversed(layout.valueReversed);
    auto* map = new QCPColorMap(keyAxis, valueAxis);
    map->data()->setSize(2, 2);
    map->data()->setRange(QCPRange(0, 1), QCPRange(0, 1));
    map->data()->setCell(1, 0, 1.0);
    map->setGradient(QCPColorGradient::gpGrayscale);
    map->setDataRange(QCPRange(0, 1));
    keyAxis->setRange(-0.5, 1.5);
    valueAxis->setRange(-0.5, 1.5);

    const QImage exported
        = mPlot->toPixmap(200, 200).toImage().convertToFormat(QImage::Format_ARGB32);
    const QRect area = mPlot->axisRect()->rect();
    const int x = layout.hotOnRight ? area.left() + area.width() * 3 / 4
                                    : area.left() + area.width() / 4;
    const int y = layout.hotOnTop ? area.top() + area.height() / 4
                                  : area.top() + area.height() * 3 / 4;
    QVERIFY2(qGray(exported.pixel(x, y)) > 200, layout.name);
    QVERIFY2(qGray(exported.pixel(area.left() + area.right() - x, y)) < 50, layout.name);
    QVERIFY2(qGray(exported.pixel(x, area.top() + area.bottom() - y)) < 50, layout.name);

    QVERIFY(mPlot->removePlottable(map));
    keyAxis->setRangeReversed(false);
    valueAxis->setRangeReversed(false);
  }
}

void TestColorMap::colourMap2IsOneImageCommand()
{
  mColorMap->setVisible(false);
  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setRange(0, 4);
  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  std::vector<double> x = { 0, 1, 2, 3, 4 };
  std::vector<double> y = { 0, 1, 2, 3, 4 };
  std::vector<double> z(25);
  for (int i = 0; i < 25; ++i)
    z[i] = i;
  cm->setData(x, y, z);
  settleColorMap(mPlot, cm);

  const QCPColorMapData* result = cm->pipeline().result();
  QVERIFY(result);
  QCOMPARE(cm->mCore.recordedCommandCount(), 1);
  const auto& commands = cm->mCore.recordedList().commands;
  QCOMPARE(int(commands.size()), 1);
  const auto* image = std::get_if<qcp::ImageCmd>(&commands.front());
  QVERIFY2(image, "the resampled grid is one image command");
  QVERIFY(image->pixels && !image->pixels->image.isNull());
  QVERIFY2(!image->pixels->smooth, "a colour map draws its cells nearest-neighbour");
  const auto* a = std::get_if<QCP::Data>(&image->a);
  const auto* b = std::get_if<QCP::Data>(&image->b);
  QVERIFY2(a && b, "the grid carries its own data corners, not a pixel quad");
  QCOMPARE(a->x, result->keyRange().lower);
  QCOMPARE(a->y, result->valueRange().upper);
  QCOMPARE(b->x, result->keyRange().upper);
  QCOMPARE(b->y, result->valueRange().lower);
}

// A pan over the same-size window asks for no new resample and no new binning: the colourisation
// stays the one the first result produced, and the recording is replayed, not rebuilt.
void TestColorMap::aPanDoesNotRecolour()
{
  mColorMap->setVisible(false);
  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setRange(0, 4);

  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  std::vector<double> x(50), y(50);
  std::vector<double> z(2500);
  for (int i = 0; i < 50; ++i)
  {
    x[i] = i / 49.0 * 100.0;
    y[i] = i / 49.0 * 100.0;
    for (int j = 0; j < 50; ++j)
      z[i * 50 + j] = std::sin(i * 0.3) + std::cos(j * 0.3);
  }
  cm->setData(x, y, z);
  settleColorMap(mPlot, cm);
  const int builds = cm->mRenderer.buildCount();
  const quint64 recordings = cm->mCore.recordingCount();
  // A pan moves the axes of an existing recording: it must not recolour. (Should a resample
  // land later, the pipeline result marks the recording stale; that is the result, not the pan.)
  for (int step = 1; step <= 5; ++step)
  {
    mPlot->xAxis->setRange(step, step + 4);
    mPlot->replot();
  }
  QCOMPARE(cm->mRenderer.buildCount(), builds);
  QCOMPARE(cm->mCore.recordingCount(), recordings);
  QTRY_VERIFY_WITH_TIMEOUT(!cm->pipeline().isBusy(), 2000);

  auto* hist = new QCPHistogram2D(mPlot->xAxis, mPlot->yAxis);
  std::vector<double> keys(5000), values(5000);
  for (int i = 0; i < 5000; ++i)
  {
    keys[i] = std::fmod(i * 0.61803398875, 1.0) * 100.0;
    values[i] = std::fmod(i * 0.75487766625, 1.0) * 100.0;
  }
  hist->setBins(20, 20);
  hist->setData(std::move(keys), std::move(values));
  mPlot->replot();
  QTRY_VERIFY_WITH_TIMEOUT(!hist->pipeline().isBusy(), 2000);
  QTest::qWait(50);
  mPlot->replot();
  const int histBuilds = hist->mRenderer.buildCount();
  const quint64 histRecordings = hist->mCore.recordingCount();
  for (int step = 1; step <= 5; ++step)
  {
    mPlot->xAxis->setRange(step * 10, step * 10 + 20);
    mPlot->replot();
  }
  QCOMPARE(hist->mRenderer.buildCount(), histBuilds);
  QCOMPARE(hist->mCore.recordingCount(), histRecordings);
  QTRY_VERIFY_WITH_TIMEOUT(!hist->pipeline().isBusy(), 2000);
}

// Each contour level is one polyline of data points, its segments NaN-separated, drawn over the
// image in the same recording.
void TestColorMap::contoursAreDataPolylines()
{
  mColorMap->setVisible(false);
  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setRange(0, 4);
  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  std::vector<double> x = { 0, 1, 2, 3, 4 };
  std::vector<double> y = { 0, 1, 2, 3, 4 };
  std::vector<double> z(25);
  for (int i = 0; i < 25; ++i)
    z[i] = i % 5;
  cm->setData(x, y, z);
  cm->setContourLevels({ 1.0, 2.0, 3.0 });
  cm->setContourPen(QPen(Qt::red, 2));
  settleColorMap(mPlot, cm);

  int images = 0, lines = 0;
  bool nanSeparates = false;
  for (const auto& command : cm->mCore.recordedList().commands)
  {
    if (std::get_if<qcp::ImageCmd>(&command))
      ++images;
    const auto* line = std::get_if<qcp::PolylineDataCmd>(&command);
    if (!line)
      continue;
    ++lines;
    QVERIFY(line->points && !line->points->empty());
    for (const QPointF& point : *line->points)
    {
      if (std::isnan(point.x()))
      {
        nanSeparates = true;
        continue;
      }
      QVERIFY2(point.x() >= -1e-9 && point.x() <= 4 + 1e-9, "contour points are keys");
      QVERIFY2(point.y() >= -1e-9 && point.y() <= 4 + 1e-9, "contour points are values");
    }
  }
  QCOMPARE(images, 1);
  QCOMPARE(lines, 3);
  QVERIFY2(nanSeparates, "segments of one level are separated by a NaN");
}

// The lines are GPU commands recorded after the image: on a screen they must land on top of it.
void TestColorMap::contoursDrawOverTheImageOnTheGpu()
{
  mPlot->show();
  if (!QTest::qWaitForWindowExposed(mPlot))
    QSKIP("window not exposed in this environment");
  QCoreApplication::processEvents();
  if (!mPlot->rhi())
    QSKIP("no QRhi available in this environment");
  mColorMap->setVisible(false);
  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setRange(0, 4);

  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  std::vector<double> x(41), y(41);
  std::vector<double> z(41 * 41);
  for (int i = 0; i < 41; ++i)
  {
    x[i] = i / 10.0;
    y[i] = i / 10.0;
    for (int j = 0; j < 41; ++j)
      z[i * 41 + j] = i / 10.0;
  }
  cm->setData(x, y, z);
  cm->setGradient(QCPColorGradient::gpGrayscale);
  cm->setDataRange(QCPRange(0, 4));
  cm->setContourLevels({ 1.0, 2.0, 3.0 });
  cm->setContourPen(QPen(Qt::red, 2));
  settleColorMap(mPlot, cm);

  const QImage frame = mPlot->grabFramebuffer();
  QVERIFY2(countRedPixels(frame) > 20,
      "the contour lines must be drawn over the colour map on the GPU frame");
}

// An export before the first bin landed still bins synchronously and draws the image.
void TestColorMap::histogram2DExportBeforeTheFirstBinHasTheImage()
{
  mColorMap->setVisible(false);
  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 100);
  mPlot->yAxis->setRange(0, 100);
  auto* hist = new QCPHistogram2D(mPlot->xAxis, mPlot->yAxis);
  std::vector<double> keys(2000), values(2000);
  for (int i = 0; i < 2000; ++i)
  {
    keys[i] = std::fmod(i * 0.61803398875, 1.0) * 100.0;
    values[i] = std::fmod(i * 0.75487766625, 1.0) * 100.0;
  }
  hist->setBins(20, 20);
  hist->setData(std::move(keys), std::move(values));

  // No replot, no wait: the export itself must run the binning.
  const QImage image = mPlot->toPixmap().toImage();
  QVERIFY(hist->mCore.recordedCommandCount() >= 1);
  const auto& commands = hist->mCore.recordedList().commands;
  const auto* cmd = std::get_if<qcp::ImageCmd>(&commands.front());
  QVERIFY2(cmd, "the export records the binned grid as an image");
  QVERIFY(!cmd->pixels->image.isNull());
  const QRect area = mPlot->axisRect()->rect();
  QVERIFY2(image.pixelColor(area.center()) != image.pixelColor(QPoint(2, 2)),
      "the export draws the binned image");
}

// The image is a data-space command of the generic plottable layer; no colour-map layer exists.
void TestColorMap::noColourMapLayerIsCreated()
{
  mColorMap->setVisible(false);
  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setRange(0, 4);
  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  std::vector<double> x = { 0, 1, 2, 3, 4 };
  std::vector<double> y = { 0, 1, 2, 3, 4 };
  std::vector<double> z(25, 1.0);
  cm->setData(x, y, z);
  settleColorMap(mPlot, cm);

  const auto& commands = cm->mCore.recordedList().commands;
  QVERIFY(!commands.empty());
  const qcp::CommandKind kind = qcp::commandKind(commands.front());
  QVERIFY2(kind.gpu && kind.dataOnly,
      "the image is a data-space GPU command of the generic plottable layer");
  for (QObject* child : mPlot->findChildren<QObject*>())
    QVERIFY2(QString::fromLatin1(child->metaObject()->className()) != "QCPColormapRhiLayer",
        "no dedicated colour-map layer may be created");
}
