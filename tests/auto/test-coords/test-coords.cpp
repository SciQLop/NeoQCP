#include "test-coords.h"

#include <qcustomplot.h>
#include <coords.h>

#include <memory>
#include <random>
#include <vector>

namespace
{
template <typename A, typename B>
concept Addable = requires(A a, B b) { a + b; };

// A laid-out plot: coordToPixel reads the axis rect, which only exists after one layout pass.
std::unique_ptr<QCustomPlot> layoutPlot()
{
  auto plot = std::make_unique<QCustomPlot>();
  plot->resize(640, 480);
  plot->replot();
  return plot;
}

bool nearlyEqual(double actual, double expected, double tolerance = 1e-9)
{
  if (qIsNaN(actual) || qIsNaN(expected))
    return qIsNaN(actual) && qIsNaN(expected);
  return qAbs(actual - expected) <= tolerance;
}

QPointF pixelsOf(const QCPTransform& transform, double key, double value)
{
  const QCP::Pixels pixel = transform.map(QCP::Data { key, value });
  return QPointF(pixel.x, pixel.y);
}

struct ScaleCase
{
  const char* name;
  bool logarithmic;
  bool reversed;
  bool verticalKey;
};

const ScaleCase kCases[] = {
  { "linear", false, false, false },
  { "log", true, false, false },
  { "reversed", false, true, false },
  { "log reversed", true, true, false },
  { "vertical key", false, false, true },
};

// Inside the range, outside it, and (for a log axis) the two sign cases the axis draws 200 px
// outside the rect.
std::vector<double> sampleValues(std::mt19937& rng)
{
  std::vector<double> values;
  std::uniform_real_distribution<double> inside(1.0, 1000.0);
  std::uniform_real_distribution<double> spread(-1e4, 1e4);
  for (int i = 0; i < 1000; ++i)
    values.push_back(inside(rng));
  for (int i = 0; i < 50; ++i)
    values.push_back(spread(rng));
  values.push_back(0.0);
  values.push_back(-0.0);
  values.push_back(1.0);
  values.push_back(-1.0);
  return values;
}

void configureLogAxes(QCPAxis* keyAxis, QCPAxis* valueAxis, const ScaleCase& testCase)
{
  const auto scale = testCase.logarithmic ? QCPAxis::stLogarithmic : QCPAxis::stLinear;
  keyAxis->setScaleType(scale);
  valueAxis->setScaleType(scale);
  keyAxis->setRange(1, 1000);
  valueAxis->setRange(1, 1000);
  keyAxis->setRangeReversed(testCase.reversed);
  valueAxis->setRangeReversed(testCase.reversed);
}
} // namespace

void TestCoords::mapMatchesCoordToPixel()
{
  std::mt19937 rng(42);
  for (const ScaleCase& testCase : kCases)
  {
    auto plot = layoutPlot();
    QCPAxis* keyAxis = plot->xAxis;
    QCPAxis* valueAxis = plot->yAxis;
    if (testCase.verticalKey)
    {
      keyAxis = plot->axisRect()->addAxis(QCPAxis::atLeft);
      valueAxis = plot->axisRect()->addAxis(QCPAxis::atBottom);
    }
    configureLogAxes(keyAxis, valueAxis, testCase);
    plot->replot();

    const QCPTransform transform(keyAxis, valueAxis);
    QVERIFY2(transform.valid(), testCase.name);

    for (double sample : sampleValues(rng))
    {
      // A vertical key axis puts the value on x, exactly as QCPItemPosition::pixelPosition does.
      const QPointF expected = testCase.verticalKey
          ? QPointF(valueAxis->coordToPixel(sample), keyAxis->coordToPixel(sample))
          : QPointF(keyAxis->coordToPixel(sample), valueAxis->coordToPixel(sample));
      const QPointF actual = pixelsOf(transform, sample, sample);
      QVERIFY2(nearlyEqual(actual.x(), expected.x()),
               qPrintable(QStringLiteral("%1: key %2 mapped to %3 px, axis says %4")
                            .arg(QLatin1String(testCase.name))
                            .arg(sample)
                            .arg(actual.x())
                            .arg(expected.x())));
      QVERIFY2(nearlyEqual(actual.y(), expected.y()),
               qPrintable(QStringLiteral("%1: value %2 mapped to %3 px, axis says %4")
                            .arg(QLatin1String(testCase.name))
                            .arg(sample)
                            .arg(actual.y())
                            .arg(expected.y())));
    }
  }
}

void TestCoords::inverseRoundTrips()
{
  std::mt19937 rng(7);
  for (const ScaleCase& testCase : kCases)
  {
    auto plot = layoutPlot();
    QCPAxis* keyAxis = plot->xAxis;
    QCPAxis* valueAxis = plot->yAxis;
    if (testCase.verticalKey)
    {
      keyAxis = plot->axisRect()->addAxis(QCPAxis::atLeft);
      valueAxis = plot->axisRect()->addAxis(QCPAxis::atBottom);
    }
    const auto scale = testCase.logarithmic ? QCPAxis::stLogarithmic : QCPAxis::stLinear;
    keyAxis->setScaleType(scale);
    valueAxis->setScaleType(scale);
    keyAxis->setRange(2, 2000);
    valueAxis->setRange(2, 2000);
    keyAxis->setRangeReversed(testCase.reversed);
    valueAxis->setRangeReversed(testCase.reversed);
    plot->replot();

    const QCPTransform transform(keyAxis, valueAxis);
    std::uniform_real_distribution<double> fx(double(keyAxis->axisRect()->left()),
                                              double(keyAxis->axisRect()->right()));
    std::uniform_real_distribution<double> fy(double(valueAxis->axisRect()->top()),
                                              double(valueAxis->axisRect()->bottom()));
    for (int i = 0; i < 200; ++i)
    {
      const QCP::Pixels start { fx(rng), fy(rng) };
      const QCP::Pixels roundTrip = transform.map(transform.inverse(start));
      QVERIFY2(nearlyEqual(roundTrip.x, start.x, 1e-6) && nearlyEqual(roundTrip.y, start.y, 1e-6),
               qPrintable(QStringLiteral("%1: %2,%3 came back as %4,%5")
                            .arg(QLatin1String(testCase.name))
                            .arg(start.x)
                            .arg(start.y)
                            .arg(roundTrip.x)
                            .arg(roundTrip.y)));
    }
  }
}

void TestCoords::batchMatchesScalar()
{
  std::mt19937 rng(11);
  for (const ScaleCase& testCase : kCases)
  {
    auto plot = layoutPlot();
    QCPAxis* keyAxis = plot->xAxis;
    QCPAxis* valueAxis = plot->yAxis;
    if (testCase.verticalKey)
    {
      keyAxis = plot->axisRect()->addAxis(QCPAxis::atLeft);
      valueAxis = plot->axisRect()->addAxis(QCPAxis::atBottom);
    }
    configureLogAxes(keyAxis, valueAxis, testCase);
    plot->replot();

    const QCPTransform transform(keyAxis, valueAxis);
    std::vector<double> keys(10000), values(10000);
    std::vector<QPointF> out(10000);
    std::uniform_real_distribution<double> inside(1.0, 1000.0);
    for (std::size_t i = 0; i < keys.size(); ++i)
    {
      keys[i] = inside(rng);
      values[i] = inside(rng);
    }

    transform.map(keys, values, out);
    for (std::size_t i = 0; i < keys.size(); ++i)
    {
      const QPointF scalar = pixelsOf(transform, keys[i], values[i]);
      QVERIFY2(nearlyEqual(out[i].x(), scalar.x()) && nearlyEqual(out[i].y(), scalar.y()),
               qPrintable(QStringLiteral("%1: batch %2,%3 vs scalar %4,%5")
                            .arg(QLatin1String(testCase.name))
                            .arg(out[i].x())
                            .arg(out[i].y())
                            .arg(scalar.x())
                            .arg(scalar.y())));
    }
  }
}

void TestCoords::translationDetected()
{
  auto plot = layoutPlot();
  plot->xAxis->setRange(0, 100);
  plot->yAxis->setRange(0, 10);
  plot->replot();

  const QCPTransform before(plot->xAxis, plot->yAxis);

  plot->xAxis->setRange(25, 125); // same width, shifted window
  plot->replot();
  QPointF delta;
  QVERIFY2(before.isTranslationOf(QCPTransform(plot->xAxis, plot->yAxis), &delta),
           "a shifted range is a translation");
  const QPointF expected = pixelsOf(before, 0, 0)
    - pixelsOf(QCPTransform(plot->xAxis, plot->yAxis), 0, 0);
  QVERIFY(nearlyEqual(delta.x(), expected.x()));
  QVERIFY(nearlyEqual(delta.y(), expected.y()));
  QVERIFY(!qFuzzyIsNull(delta.x()));

  plot->xAxis->setRange(0, 50); // zoomed: the data-to-pixel slope changed
  plot->replot();
  QVERIFY(!before.isTranslationOf(QCPTransform(plot->xAxis, plot->yAxis), &delta));

  plot->xAxis->setRange(1, 100);
  plot->xAxis->setScaleType(QCPAxis::stLogarithmic);
  plot->replot();
  QVERIFY2(!before.isTranslationOf(QCPTransform(plot->xAxis, plot->yAxis), &delta),
           "a log axis is not treated as a translation");

  plot->xAxis->setScaleType(QCPAxis::stLinear);
  plot->xAxis->setRange(0, 100);
  plot->xAxis->setRangeReversed(true);
  plot->replot();
  QVERIFY(!before.isTranslationOf(QCPTransform(plot->xAxis, plot->yAxis), &delta));
}

void TestCoords::positionSetMatchesSetters()
{
  auto plot = layoutPlot();
  auto* item = new QCPItemLine(plot.get());
  auto* reference = new QCPItemLine(plot.get());
  QCPItemPosition* position = item->start;
  QCPItemPosition* expected = reference->start;

  auto sameInitialState = [&]
  {
    position->setParentAnchor(nullptr, false);
    expected->setParentAnchor(nullptr, false);
    position->setTypeX(QCPItemPosition::ptAbsolute);
    position->setTypeY(QCPItemPosition::ptAbsolute);
    expected->setTypeX(QCPItemPosition::ptAbsolute);
    expected->setTypeY(QCPItemPosition::ptAbsolute);
  };
  sameInitialState();
  position->setAxes(plot->xAxis, plot->yAxis);
  position->setAxisRect(plot->axisRect());
  expected->setAxes(plot->xAxis, plot->yAxis);
  expected->setAxisRect(plot->axisRect());
  plot->xAxis->setRange(-10, 10);
  plot->yAxis->setRange(-5, 5);
  plot->replot();

  auto samePixels = [&](const char* what)
  {
    const bool same = position->pixelPosition() == expected->pixelPosition();
    if (!same)
      qWarning("%s: set() gave %f,%f, the 2.x setters gave %f,%f", what,
               position->pixelPosition().x(), position->pixelPosition().y(),
               expected->pixelPosition().x(), expected->pixelPosition().y());
    return same;
  };

  sameInitialState();
  position->set(QCP::Data { 3.5, -2.25 });
  expected->setType(QCPItemPosition::ptPlotCoords);
  expected->setCoords(3.5, -2.25);
  QVERIFY2(samePixels("Data"), "Data must equal ptPlotCoords + coords");

  sameInitialState();
  position->set(QCP::Pixels { 120.0, 90.0 });
  expected->setType(QCPItemPosition::ptAbsolute);
  expected->setCoords(120.0, 90.0);
  QVERIFY2(samePixels("Pixels"), "Pixels must equal ptAbsolute + coords");

  sameInitialState();
  position->set(QCP::AxisRatio { 0.25, 0.75 });
  expected->setType(QCPItemPosition::ptAxisRectRatio);
  expected->setCoords(0.25, 0.75);
  QVERIFY2(samePixels("AxisRatio"), "AxisRatio must equal ptAxisRectRatio + coords");

  sameInitialState();
  position->set(QCP::MixedX { 4.0, 0.25 });
  expected->setTypeX(QCPItemPosition::ptPlotCoords);
  expected->setTypeY(QCPItemPosition::ptAxisRectRatio);
  expected->setCoords(4.0, 0.25);
  QVERIFY2(samePixels("MixedX"), "MixedX must mix plot coords and a rect ratio");

  sameInitialState();
  position->set(QCP::MixedY { 0.1, -3.0 });
  expected->setTypeX(QCPItemPosition::ptAxisRectRatio);
  expected->setTypeY(QCPItemPosition::ptPlotCoords);
  expected->setCoords(0.1, -3.0);
  QVERIFY2(samePixels("MixedY"), "MixedY must mix a rect ratio and plot coords");

  sameInitialState();
  position->set(QCP::AnchorOffset { item->end, QCP::PixelOffset { 12.0, -7.0 } });
  expected->setParentAnchor(reference->end, false);
  expected->setCoords(12.0, -7.0);
  QVERIFY2(samePixels("AnchorOffset"), "AnchorOffset must equal a parent anchor and an offset");

  // asPosition() reads the same spaces back.
  position->set(QCP::Data { -1.5, 2.5 });
  QCP::Position read = position->asPosition();
  const auto data = std::get_if<QCP::Data>(&read);
  QVERIFY(data);
  QCOMPARE(data->x, -1.5);
  QCOMPARE(data->y, 2.5);

  position->set(QCP::AxisRatio { 0.5, 0.25 });
  read = position->asPosition();
  const auto ratio = std::get_if<QCP::AxisRatio>(&read);
  QVERIFY(ratio);
  QCOMPARE(ratio->x, 0.5);
  QCOMPARE(ratio->y, 0.25);

  position->set(QCP::MixedX { 6.0, 0.4 });
  read = position->asPosition();
  const auto mixed = std::get_if<QCP::MixedX>(&read);
  QVERIFY(mixed);
  QCOMPARE(mixed->dataX, 6.0);
  QCOMPARE(mixed->ratioY, 0.4);

  position->set(QCP::Pixels { 33.0, 44.0 });
  read = position->asPosition();
  const auto pixels = std::get_if<QCP::Pixels>(&read);
  QVERIFY(pixels);
  QCOMPARE(pixels->x, 33.0);
  QCOMPARE(pixels->y, 44.0);

  position->set(QCP::AnchorOffset { item->end, QCP::PixelOffset { 5.0, 6.0 } });
  read = position->asPosition();
  const auto anchored = std::get_if<QCP::AnchorOffset>(&read);
  QVERIFY(anchored);
  QCOMPARE(anchored->anchor, item->end);
  QCOMPARE(anchored->offset.dx, 5.0);
  QCOMPARE(anchored->offset.dy, 6.0);
}

void TestCoords::ratioTransformMatchesPixels()
{
  auto plot = layoutPlot();
  const QCPRatioTransform transform(plot->axisRect());
  QVERIFY(transform.valid());
  const QCP::Pixels centre = transform.map(QCP::AxisRatio { 0.5, 0.5 });
  QVERIFY(nearlyEqual(centre.x, plot->axisRect()->left() + plot->axisRect()->width() * 0.5));
  QVERIFY(nearlyEqual(centre.y, plot->axisRect()->top() + plot->axisRect()->height() * 0.5));
  const QCP::AxisRatio back = transform.inverse(centre);
  QVERIFY(nearlyEqual(back.x, 0.5) && nearlyEqual(back.y, 0.5));

  // 2.x agreement: an item position of type ptAxisRectRatio lands on the same pixels.
  auto* item = new QCPItemText(plot.get());
  item->position->setAxisRect(plot->axisRect());
  item->position->setType(QCPItemPosition::ptAxisRectRatio);
  item->position->setCoords(0.5, 0.5);
  QCOMPARE(item->position->pixelPosition(), QPointF(centre.x, centre.y));
}

void TestCoords::mixingSpacesDoesNotCompile()
{
  // A requires-expression over invented parameters is a hard error, not false, so the absence of an
  // operator is asserted through a concept whose parameters are dependent.
  static_assert(!Addable<QCP::Data, QCP::Pixels>,
                "a data point and a pixel point are different spaces: no operator may join them");
  static_assert(!Addable<QCP::Pixels, QCP::Data>);
  static_assert(Addable<QCP::Pixels, QCP::PixelOffset>);
  static_assert(Addable<QCPItemAnchor*, QCP::PixelOffset>);
  QVERIFY(true);
}
