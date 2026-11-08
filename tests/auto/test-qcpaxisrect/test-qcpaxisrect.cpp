#include "test-qcpaxisrect.h"
#include "../expect-debug.h"
#include "../keep-images.h"
#include "axis/labelrenderer.h"
#if NEOQCP_WITH_LATEX
#include "axis/latexlabelrenderer.h"
#endif

namespace
{
int gMisuses = 0;
void countMisuse(const char*, const char*) { ++gMisuses; }

// Plain drawText, counting how often the axis asks it to lay the label out.
struct CountingLabelRenderer final : QCPLabelRenderer
{
    mutable int measures = 0;
    mutable int draws = 0;
    QSize measure(const QFont& font, const QString& text) const override
    {
        ++measures;
        return QCPLabelRenderer::measureWith(nullptr, font, text);
    }
    void draw(QCPPainter* painter, const QRect& rect, const QFont& font, const QColor& color,
              const QString& text, int flags) const override
    {
        ++draws;
        QCPLabelRenderer::drawWith(nullptr, painter, rect, font, color, text, flags);
    }
};

QPointF rectCentre(const QCustomPlot* plot)
{
    return QRectF(plot->axisRect()->rect()).center();
}

// A drag as the user makes it: press, two moves, release.
void dragOn(QCustomPlot* plot, const QPoint& from, const QPoint& to)
{
    auto send = [plot](QEvent::Type type, const QPoint& pos, Qt::MouseButtons buttons) {
        QMouseEvent event(type, pos, plot->mapToGlobal(pos), Qt::LeftButton, buttons,
                          Qt::NoModifier);
        QCoreApplication::sendEvent(plot, &event);
    };
    send(QEvent::MouseButtonPress, from, Qt::LeftButton);
    send(QEvent::MouseMove, (from + to) / 2, Qt::LeftButton);
    send(QEvent::MouseMove, to, Qt::LeftButton);
    send(QEvent::MouseButtonRelease, to, Qt::NoButton);
}

void wheelOn(QCustomPlot* plot, const QPointF& pos, int steps = 1)
{
    QWheelEvent event(pos, plot->mapToGlobal(pos), QPoint(), QPoint(0, 120 * steps), Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(plot, &event);
}

bool sameRange(const QCPRange& a, const QCPRange& b)
{
    return qAbs(a.lower - b.lower) < 1e-9 * qMax(1.0, qAbs(a.lower))
        && qAbs(a.upper - b.upper) < 1e-9 * qMax(1.0, qAbs(a.upper));
}

// The range a drag from one pixel to another leaves, on a linear axis.
QCPRange draggedByPixels(const QCPAxis* axis, double fromPixel, double toPixel)
{
    const QCPRange start = axis->range();
    const double diff = axis->pixelToCoord(fromPixel) - axis->pixelToCoord(toPixel);
    return QCPRange(start.lower + diff, start.upper + diff);
}

// The range one zoom step around a pixel leaves, on a linear axis.
QCPRange zoomedBy(const QCPAxis* axis, double factor, double pixel)
{
    const QCPRange start = axis->range();
    const double center = axis->pixelToCoord(pixel);
    return QCPRange((start.lower - center) * factor + center,
                    (start.upper - center) * factor + center);
}
}

void TestQCPAxisRect::init()
{
  mPlot = new QCustomPlot(0);
  mPlot->show();
  //QTest::qWait(150);
}


void TestQCPAxisRect::cleanup()
{
  delete mPlot;
}

void TestQCPAxisRect::multiAxis()
{
  QCPAxisRect *ar = mPlot->axisRect();
  QCOMPARE(ar->axisCount(QCPAxis::atLeft), 1);
  QCOMPARE(ar->axisCount(QCPAxis::atRight), 1);
  QCOMPARE(ar->axisCount(QCPAxis::atBottom), 1);
  QCOMPARE(ar->axisCount(QCPAxis::atTop), 1);
  
  // test adding of axes:
  QList<QCPAxis*> newAxes = ar->addAxes(QCPAxis::atLeft | QCPAxis::atBottom);
  QCOMPARE(newAxes.size(), 2);
  QCOMPARE(ar->axisCount(QCPAxis::atLeft), 2);
  QCOMPARE(ar->axisCount(QCPAxis::atBottom), 2);
  
  // test removal of axes:
  ar->removeAxis(ar->axis(QCPAxis::atLeft, 0));
  QCOMPARE(mPlot->yAxis, (QCPAxis*)0);
  QCOMPARE(ar->axisCount(QCPAxis::atLeft), 1);
  QCOMPARE(ar->axis(QCPAxis::atLeft, 0), newAxes.at(0));
  expectDebugFrom("QCPAxisRect::removeAxis", "Axis isn't in axis rect: 1234");
  QVERIFY(!ar->removeAxis((QCPAxis*)1234));
}

void TestQCPAxisRect::multiAxisMargins()
{
  QCPAxisRect *ar = mPlot->axisRect();
  
  // test the increasing margin when axes are added:
  mPlot->replot();
  int leftMarginOneAxis = ar->margins().left();
  int bottomMarginOneAxis = ar->margins().bottom();
  ar->addAxes(QCPAxis::atLeft | QCPAxis::atBottom);
  mPlot->replot();
  int leftMarginTwoAxes = ar->margins().left();
  int bottomMarginTwoAxes = ar->margins().bottom();
  QCOMPARE(leftMarginOneAxis*2+ar->axis(QCPAxis::atLeft, 1)->tickLengthIn(), leftMarginTwoAxes);
  QCOMPARE(bottomMarginOneAxis*2+ar->axis(QCPAxis::atBottom, 1)->tickLengthIn(), bottomMarginTwoAxes);
  
  // test increasing margin when offset is added to first axis:
  int secondAxisOffsetBefore = ar->axis(QCPAxis::atLeft, 1)->offset();
  ar->axis(QCPAxis::atLeft, 0)->setOffset(5);
  mPlot->replot();
  QCOMPARE(leftMarginTwoAxes+5, ar->margins().left());
  QCOMPARE(secondAxisOffsetBefore+5, ar->axis(QCPAxis::atLeft, 1)->offset());
  
  // test increasing margin when padding is added to either axis:
  ar->axis(QCPAxis::atLeft, 0)->setOffset(0);
  ar->axis(QCPAxis::atLeft, 0)->setPadding(10);
  mPlot->replot();
  QCOMPARE(leftMarginTwoAxes+5, ar->margins().left()); // +5 because padding is 5 by default and thus included in leftMarginTwoAxes already
  ar->axis(QCPAxis::atLeft, 1)->setPadding(10);
  mPlot->replot();
  QCOMPARE(leftMarginTwoAxes+10, ar->margins().left()); // +10 because padding is 5 by default and thus included in leftMarginTwoAxes already
}

void TestQCPAxisRect::axisRemovalConsequencesToPlottables()
{
  // test consequences to plottables when one of their axes is removed:
  QCPGraph *graph = mPlot->addGraph(mPlot->xAxis, mPlot->yAxis);
  graph->setData(QVector<double>() << 1 << 2 << 3, QVector<double>() << 1 << 0 << -1);
  mPlot->replot();

  QVERIFY(mPlot->axisRect()->removeAxis(mPlot->xAxis));
  gMisuses = 0;
  const auto previous = qcp::setMisuseHandler(countMisuse);
  mPlot->replot();
  qcp::setMisuseHandler(previous);
  QCOMPARE(gMisuses, 1); // the graph without a key axis is not drawn
  mPlot->rescaleAxes();
  expectDebugFrom("QCPAbstractPlottable::rescaleKeyAxis", "invalid key axis");
  expectDebugFrom("QCPAbstractPlottable::rescaleValueAxis", "invalid key or value axis");
  graph->rescaleAxes();

  // test replacement of previously removed axis:
  QCPAxis *newAxis = mPlot->axisRect()->addAxis(QCPAxis::atBottom);
  graph->setKeyAxis(newAxis);
  mPlot->replot();
  mPlot->rescaleAxes();
  graph->rescaleAxes();
}

void TestQCPAxisRect::axisRemovalConsequencesToItems()
{
  // test consequences to items when their axes are removed:
  QCPItemLine *item = new QCPItemLine(mPlot);
  mPlot->replot();

  QVERIFY(mPlot->axisRect()->removeAxis(mPlot->xAxis));
  expectDebugFrom("QCPItemPosition::pixelPosition", "Item position type x is ptPlotCoords, but no axes were defined"); // for start position
  expectDebugFrom("QCPItemPosition::pixelPosition", "Item position type x is ptPlotCoords, but no axes were defined"); // for end position
  mPlot->replot();
  QVERIFY(mPlot->axisRect()->removeAxis(mPlot->yAxis));
  expectDebugFrom("QCPItemPosition::pixelPosition", "Item position type x is ptPlotCoords, but no axes were defined"); // for start position
  expectDebugFrom("QCPItemPosition::pixelPosition", "Item position type y is ptPlotCoords, but no axes were defined"); // for start position
  expectDebugFrom("QCPItemPosition::pixelPosition", "Item position type x is ptPlotCoords, but no axes were defined"); // for end position
  expectDebugFrom("QCPItemPosition::pixelPosition", "Item position type y is ptPlotCoords, but no axes were defined"); // for end position
  mPlot->replot();
  
  
  expectDebugFrom("QCPItemPosition::setPixelPosition", "Item position type x is ptPlotCoords, but no axes were defined");
  expectDebugFrom("QCPItemPosition::setPixelPosition", "Item position type y is ptPlotCoords, but no axes were defined");
  item->start->setPixelPosition(QPointF(1, 2));
  
  // change type to axis-independent coordinates:
  item->start->setType(QCPItemPosition::ptAxisRectRatio);
  item->end->setType(QCPItemPosition::ptAxisRectRatio);
  mPlot->replot(); // shouldn't output any debug messages now because we've changed the type to be independent of axes
  // change back to axis-dependent coordinates:
  item->start->setType(QCPItemPosition::ptPlotCoords);
  item->end->setType(QCPItemPosition::ptPlotCoords);
  
  // test replacement of previously removed axis:
  QCPAxis *newXAxis = mPlot->axisRect()->addAxis(QCPAxis::atBottom);
  QCPAxis *newYAxis = mPlot->axisRect()->addAxis(QCPAxis::atLeft);
  item->start->setAxes(newXAxis, newYAxis);
  item->end->setAxes(newXAxis, newYAxis);
  mPlot->replot();
}

void TestQCPAxisRect::axisRectRemovalConsequencesToPlottables()
{
  // test consequences to plottables when the axis rect is removed:
  QCPGraph *graph = mPlot->addGraph(mPlot->xAxis, mPlot->yAxis);
  graph->setData(QVector<double>() << 1 << 2 << 3, QVector<double>() << 1 << 0 << -1);
  mPlot->replot();

  QVERIFY(mPlot->plotLayout()->removeAt(0));
  mPlot->plotLayout()->simplify();
  QCOMPARE(mPlot->plotLayout()->elementCount(), 0);
  gMisuses = 0;
  const auto previous = qcp::setMisuseHandler(countMisuse);
  mPlot->layer("main")->markDirty();
  mPlot->replot();
  qcp::setMisuseHandler(previous);
  QCOMPARE(gMisuses, 1); // the graph without axes is not drawn
  mPlot->rescaleAxes();
  expectDebugFrom("QCPAbstractPlottable::rescaleKeyAxis", "invalid key axis");
  expectDebugFrom("QCPAbstractPlottable::rescaleValueAxis", "invalid key or value axis");
  graph->rescaleAxes();
  
  // test replacement of previously removed axis:
  qobject_cast<QCPLayoutGrid*>(mPlot->plotLayout())->addElement(0, 0, new QCPAxisRect(mPlot));
  mPlot->axisRect()->addAxes(QCPAxis::atBottom|QCPAxis::atLeft);
  graph->setKeyAxis(mPlot->axisRect()->axis(QCPAxis::atBottom));
  graph->setValueAxis(mPlot->axisRect()->axis(QCPAxis::atLeft));
  mPlot->replot();
  mPlot->rescaleAxes();
  graph->rescaleAxes();
}

void TestQCPAxisRect::axisRectRemovalConsequencesToItems()
{
  // test consequences to items when their axes are removed:
  QCPItemLine *item = new QCPItemLine(mPlot);
  item->setClipAxisRect(mPlot->axisRect());
  item->setClipToAxisRect(true);
  item->start->setAxisRect(mPlot->axisRect());
  item->start->setType(QCPItemPosition::ptAxisRectRatio);
  mPlot->replot();
  
  QVERIFY(mPlot->plotLayout()->removeAt(0));
  mPlot->plotLayout()->simplify();
  QCOMPARE(mPlot->plotLayout()->elementCount(), 0);
  expectDebugFrom("QCPItemPosition::pixelPosition", "Item position type x is ptAxisRectRatio, but no axis rect was defined"); // for start position
  expectDebugFrom("QCPItemPosition::pixelPosition", "Item position type y is ptAxisRectRatio, but no axis rect was defined"); // for start position
  expectDebugFrom("QCPItemPosition::pixelPosition", "Item position type x is ptPlotCoords, but no axes were defined"); // for end position
  expectDebugFrom("QCPItemPosition::pixelPosition", "Item position type y is ptPlotCoords, but no axes were defined"); // for end position
  mPlot->layer("main")->markDirty();
  mPlot->replot();
  
  expectDebugFrom("QCPItemPosition::setPixelPosition", "Item position type x is ptAxisRectRatio, but no axis rect was defined");
  expectDebugFrom("QCPItemPosition::setPixelPosition", "Item position type y is ptAxisRectRatio, but no axis rect was defined");
  item->start->setPixelPosition(QPointF(1, 2));
  
  // change type to axisRect-independent coordinates:
  item->start->setType(QCPItemPosition::ptAbsolute);
  item->end->setType(QCPItemPosition::ptAbsolute);
  mPlot->replot(); // shouldn't output any debug messages now because we've changed the type to be independent of axesRect
  // change back to axisRect-dependent coordinates:
  item->start->setType(QCPItemPosition::ptAxisRectRatio);
  item->end->setType(QCPItemPosition::ptPlotCoords);
  // test replacement of previously removed axis:
  qobject_cast<QCPLayoutGrid*>(mPlot->plotLayout())->addElement(0, 0, new QCPAxisRect(mPlot));
  mPlot->axisRect()->addAxes(QCPAxis::atBottom|QCPAxis::atLeft);
  item->start->setAxisRect(mPlot->axisRect());
  item->end->setAxes(mPlot->axisRect()->axis(QCPAxis::atBottom), mPlot->axisRect()->axis(QCPAxis::atLeft));
  mPlot->replot();
}

void TestQCPAxisRect::axisRectRemovalConveniencePointers()
{
  QVERIFY(mPlot->plotLayout()->removeAt(0));
  mPlot->plotLayout()->simplify();
  QCOMPARE(mPlot->plotLayout()->elementCount(), 0);
  
  QCOMPARE(mPlot->xAxis, (QCPAxis*)0);
  QCOMPARE(mPlot->yAxis, (QCPAxis*)0);
  QCOMPARE(mPlot->xAxis2, (QCPAxis*)0);
  QCOMPARE(mPlot->yAxis2, (QCPAxis*)0);
  QCOMPARE(mPlot->legend, (QCPLegend*)0);
  
  QCPAxisRect *axRect = new QCPAxisRect(mPlot, false);
  mPlot->plotLayout()->addElement(0, 0, axRect);
  QCOMPARE(mPlot->xAxis, (QCPAxis*)0);
  QCOMPARE(mPlot->yAxis, (QCPAxis*)0);
  QCOMPARE(mPlot->xAxis2, (QCPAxis*)0);
  QCOMPARE(mPlot->yAxis2, (QCPAxis*)0);
  
  QCPAxis *ab = axRect->addAxis(QCPAxis::atBottom);
  QCOMPARE(mPlot->xAxis, ab);
  QCOMPARE(mPlot->yAxis, (QCPAxis*)0);
  QCOMPARE(mPlot->xAxis2, (QCPAxis*)0);
  QCOMPARE(mPlot->yAxis2, (QCPAxis*)0);
  QCPAxis *al = axRect->addAxis(QCPAxis::atLeft);
  QCOMPARE(mPlot->xAxis, ab);
  QCOMPARE(mPlot->yAxis, al);
  QCOMPARE(mPlot->xAxis2, (QCPAxis*)0);
  QCOMPARE(mPlot->yAxis2, (QCPAxis*)0);
  QCPAxis *at = axRect->addAxis(QCPAxis::atTop);
  QCOMPARE(mPlot->xAxis, ab);
  QCOMPARE(mPlot->yAxis, al);
  QCOMPARE(mPlot->xAxis2, at);
  QCOMPARE(mPlot->yAxis2, (QCPAxis*)0);
  QCPAxis *ar = axRect->addAxis(QCPAxis::atRight);
  QCOMPARE(mPlot->xAxis, ab);
  QCOMPARE(mPlot->yAxis, al);
  QCOMPARE(mPlot->xAxis2, at);
  QCOMPARE(mPlot->yAxis2, ar);
  
  QCOMPARE(mPlot->legend, (QCPLegend*)0);
  QCPLegend *leg = new QCPLegend();
  axRect->insetLayout()->addElement(leg, Qt::AlignRight|Qt::AlignTop);
  QCOMPARE(mPlot->legend, leg);
}














void TestQCPAxisRect::axisLabelLaidOutOncePerChange()
{
  // SciQLop#143: a full text layout of the axis label on every replot was ~40 % of
  // QCPAxis::draw. The label rarely changes, so a pan must reuse it.
  CountingLabelRenderer renderer;
  mPlot->yAxis->setLabelRenderer(&renderer);
  mPlot->yAxis->setLabel("B_gse\n[nT]");
  mPlot->replot(QCustomPlot::rpImmediateRefresh);
  QVERIFY(renderer.draws > 0);
  const int draws = renderer.draws, measures = renderer.measures;

  for (int i = 0; i < 5; ++i)
  {
    mPlot->yAxis->setRange(i, i + 10);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
  }
  QCOMPARE(renderer.draws, draws);
  QCOMPARE(renderer.measures, measures);

  mPlot->yAxis->setLabel("B_gsm\n[nT]");
  mPlot->replot(QCustomPlot::rpImmediateRefresh);
  QVERIFY(renderer.draws > draws);
  const int afterText = renderer.draws;

  mPlot->yAxis->setLabelColor(Qt::red);
  mPlot->replot(QCustomPlot::rpImmediateRefresh);
  QVERIFY(renderer.draws > afterText);
  mPlot->yAxis->setLabelRenderer(nullptr);
}

void TestQCPAxisRect::axisLabelRedrawnForExports()
{
  // Exports stay vector: a PDF must get the text, not a cached pixmap of it.
  CountingLabelRenderer renderer;
  mPlot->xAxis->setLabelRenderer(&renderer);
  mPlot->xAxis->setLabel("time");
  mPlot->replot(QCustomPlot::rpImmediateRefresh);
  const int draws = renderer.draws;
  mPlot->toPixmap(400, 300);
  QVERIFY(renderer.draws > draws);
  mPlot->xAxis->setLabelRenderer(nullptr);
}

static QRect inkBounds(const QImage& image)
{
  QRect bounds;
  for (int y = 0; y < image.height(); ++y)
    for (int x = 0; x < image.width(); ++x)
      if (qGray(image.pixel(x, y)) < 200)
        bounds |= QRect(x, y, 1, 1);
  return bounds;
}

void TestQCPAxisRect::axisLabelPictureMatchesDirectText()
{
  // The cached picture must land exactly where drawText would put the text, rotated
  // sides included, and a label longer than the axis must not be cropped.
  for (auto type : {QCPAxis::atLeft, QCPAxis::atRight, QCPAxis::atTop, QCPAxis::atBottom})
  {
    for (const QString& label : {QStringLiteral("B_gse\n[nT]"),
                                 QStringLiteral("a label much longer than this short axis is")})
    {
      auto render = [&](bool exportMode) {
        QCPAxisPainterPrivate axis(mPlot);
        axis.type = type;
        axis.label = label;
        axis.labelFont = QFont("sans", 10);
        axis.labelColor = Qt::black;
        axis.labelPadding = 5;
        axis.tickLabelPadding = 5;
        axis.tickLabelRotation = 0;
        axis.tickLabelSide = QCPAxis::lsOutside;
        axis.substituteExponent = false;
        axis.numberMultiplyCross = false;
        axis.tickLengthIn = axis.tickLengthOut = axis.subTickLengthIn = axis.subTickLengthOut = 0;
        axis.offset = 0;
        axis.abbreviateDecimalPowers = false;
        axis.reversedEndings = false;
        axis.viewportRect = QRect(0, 0, 400, 300);
        axis.axisRect = QRect(150, 120, 100, 60);
        QImage image(400, 300, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::white);
        QCPPainter painter(&image);
        if (exportMode)
          painter.setMode(QCPPainter::pmNoCaching);
        axis.draw(&painter);
        axis.draw(&painter); // second draw goes through the cached picture
        return image;
      };
      // Glyph antialiasing differs (upright text turned as a picture vs text drawn rotated),
      // so compare where the ink lands, to the pixel an antialiased edge may add or drop.
      const QImage cachedImage = render(false), directImage = render(true);
      const QRect cached = inkBounds(cachedImage), direct = inkBounds(directImage);
      if (qAbs(cached.left() - direct.left()) > 1 || qAbs(cached.right() - direct.right()) > 1
          || qAbs(cached.top() - direct.top()) > 1 || qAbs(cached.bottom() - direct.bottom()) > 1)
        keepImagesForCi(QString("axis-label-side%1-%2").arg(int(type)).arg(label.size()), cachedImage, directImage);
      QVERIFY2(qAbs(cached.left() - direct.left()) <= 1 && qAbs(cached.right() - direct.right()) <= 1
                   && qAbs(cached.top() - direct.top()) <= 1
                   && qAbs(cached.bottom() - direct.bottom()) <= 1,
               qPrintable(QString("side %1, label '%2'").arg(int(type)).arg(label)));
    }
  }
}

void TestQCPAxisRect::logAxisMarginIsRightOnTheFirstFrame()
{
  // The margin is measured before the first draw: it must already see the log axis's
  // abbreviated powers ("10^8", not "1·10^8"), or the first frame lays out wider than the next.
  QCustomPlot plot;
  plot.resize(400, 300);
  plot.setLocale(QLocale(QLocale::English, QLocale::UnitedStates));
  plot.yAxis->setScaleType(QCPAxis::stLogarithmic);
  plot.yAxis->setTicker(QSharedPointer<QCPAxisTickerLog>::create());
  plot.yAxis->setNumberFormat("eb");
  plot.yAxis->setNumberPrecision(0);
  plot.yAxis->setTickLabelFont(QFont("sans", 30)); // makes the "1·" prefix hard to miss
  plot.yAxis->setRange(1e-3, 1e8);
  plot.replot();
  const int firstFrame = plot.axisRect()->left();
  plot.yAxis->setNumberFormat("ebc"); // measured again, now after a draw
  plot.yAxis->setNumberFormat("eb");
  plot.replot();
  QCOMPARE(firstFrame, plot.axisRect()->left());
}

void TestQCPAxisRect::beautifulPowersInEveryLocale()
{
  // QLocale::exponential() is "E" for most locales, while number formatting writes "1e+08":
  // beautiful powers must not depend on the case of the exponent character.
  for (const QLocale& locale : {QLocale::c(), QLocale(QLocale::English, QLocale::UnitedStates),
                                QLocale(QLocale::French, QLocale::France)})
  {
    QCustomPlot plot;
    plot.setLocale(locale);
    auto measure = [&](bool beautiful) {
      QCPAxisPainterPrivate axis(&plot);
      axis.type = QCPAxis::atLeft;
      axis.tickLabelFont = QFont("sans", 30);
      axis.tickLabelSide = QCPAxis::lsOutside;
      axis.substituteExponent = beautiful;
      axis.abbreviateDecimalPowers = true;
      axis.tickLabels = {QStringLiteral("1e+08")};
      return axis.size();
    };
    QVERIFY2(measure(true) < measure(false), qPrintable(locale.name()));
  }
}

void TestQCPAxisRect::longPlainLabelIsElidedToTheAxis()
{
  // A label longer than its axis used to overflow it at both ends, into the
  // neighbouring plots of a stack. It is elided to the axis length now.
  QCPAxisPainterPrivate axis(mPlot);
  axis.type = QCPAxis::atBottom;
  axis.label = QStringLiteral("Magnetic field vector in GSE plus Btotal (8 or 16 S/s) [nT (all flags)]");
  axis.labelFont = QFont("sans", 10);
  axis.labelColor = Qt::black;
  axis.labelPadding = 5;
  axis.tickLabelSide = QCPAxis::lsOutside;
  axis.tickLengthIn = axis.tickLengthOut = axis.subTickLengthIn = axis.subTickLengthOut = 0;
  axis.viewportRect = QRect(0, 0, 400, 300);
  axis.axisRect = QRect(150, 120, 100, 60);
  QImage image(400, 300, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::white);
  QCPPainter painter(&image);
  axis.draw(&painter);
  painter.end();
  const QRect ink = inkBounds(image.copy(0, 185, 400, 115)); // below the axis: the label
  QVERIFY(!ink.isEmpty());
  QVERIFY2(ink.left() >= 149 && ink.right() <= 251,
           qPrintable(QStringLiteral("label ink spans %1..%2").arg(ink.left()).arg(ink.right())));
}

void TestQCPAxisRect::labelRenderersDecideWhatCanBeElided()
{
  const QFont font("sans", 10);
  const QString plain = QStringLiteral("a label much longer than the room it is given");
  const QString elided = QCPLabelRenderer::elideWith(nullptr, font, plain, 80);
  QVERIFY(elided != plain);
  QVERIFY(QFontMetrics(font).horizontalAdvance(elided) <= 80);

  // A custom renderer's markup must not be cut: by default it is kept whole.
  CountingLabelRenderer custom;
  QCOMPARE(QCPLabelRenderer::elideWith(&custom, font, plain, 80), plain);

#if NEOQCP_WITH_LATEX
  auto* latex = QCPLatexLabelRenderer::instance();
  QCOMPARE(QCPLabelRenderer::elideWith(latex, font, plain, 80), elided);
  const QString math = QStringLiteral("$B_{gse}$ a label much longer than the room it is given");
  QCOMPARE(QCPLabelRenderer::elideWith(latex, font, math, 80), math);
#endif
}

void TestQCPAxisRect::dragMovesEveryAxisByDefault()
{
  mPlot->setInteractions(QCP::iRangeDrag);
  mPlot->resize(400, 300);
  QCPAxis* secondLeft = mPlot->axisRect()->addAxis(QCPAxis::atLeft);
  mPlot->replot();

  const QPointF centre = rectCentre(mPlot);
  const QPointF to = centre - QPointF(0, 50);
  const QCPRange startLeft = mPlot->yAxis->range();
  const QCPRange expectedLeft = draggedByPixels(mPlot->yAxis, centre.y(), to.y());
  const QCPRange expectedSecond = draggedByPixels(secondLeft, centre.y(), to.y());

  dragOn(mPlot, centre.toPoint(), to.toPoint());

  QVERIFY2(!sameRange(mPlot->yAxis->range(), startLeft), "the primary left axis did not move");
  QVERIFY2(sameRange(mPlot->yAxis->range(), expectedLeft),
           "the primary left axis did not move by its own 50 px");
  QVERIFY2(sameRange(secondLeft->range(), expectedSecond),
           "the second left axis did not move by its own 50 px");
}

void TestQCPAxisRect::defaultSetKeepsThePrimaryAxisFirst()
{
  mPlot->resize(400, 300);
  mPlot->replot();
  QCPAxisRect* ar = mPlot->axisRect();

  QCOMPARE(ar->rangeDragAxis(Qt::Horizontal), mPlot->xAxis);
  QCOMPARE(ar->rangeDragAxis(Qt::Vertical), mPlot->yAxis);
  QCOMPARE(ar->rangeZoomAxis(Qt::Horizontal), mPlot->xAxis);
  QCOMPARE(ar->rangeZoomAxis(Qt::Vertical), mPlot->yAxis);
  QCOMPARE(ar->rangeDragAxes(Qt::Horizontal), (QList<QCPAxis*> { mPlot->xAxis, mPlot->xAxis2 }));
  QCOMPARE(ar->rangeDragAxes(Qt::Vertical), (QList<QCPAxis*> { mPlot->yAxis, mPlot->yAxis2 }));
  QCOMPARE(ar->rangeZoomAxes(Qt::Horizontal), (QList<QCPAxis*> { mPlot->xAxis, mPlot->xAxis2 }));
  QCOMPARE(ar->rangeZoomAxes(Qt::Vertical), (QList<QCPAxis*> { mPlot->yAxis, mPlot->yAxis2 }));
}

void TestQCPAxisRect::optedOutAxisStaysPut()
{
  mPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
  mPlot->resize(400, 300);
  mPlot->replot();
  const QPointF centre = rectCentre(mPlot);

  mPlot->xAxis->setRangeDraggable(false);
  const QCPRange start = mPlot->xAxis->range();
  const QCPRange topStart = mPlot->xAxis2->range();
  dragOn(mPlot, centre.toPoint(), (centre + QPointF(40, 0)).toPoint());
  QCOMPARE(mPlot->xAxis->range(), start);
  QVERIFY2(!sameRange(mPlot->xAxis2->range(), topStart), "the other horizontal axis stayed put too");

  mPlot->xAxis->setRangeDraggable(true);
  mPlot->xAxis->setRangeZoomable(false);
  const QCPRange zoomStart = mPlot->xAxis->range();
  const QCPRange topZoomStart = mPlot->xAxis2->range();
  wheelOn(mPlot, centre);
  QCOMPARE(mPlot->xAxis->range(), zoomStart);
  QVERIFY2(!sameRange(mPlot->xAxis2->range(), topZoomStart),
           "the other horizontal axis did not zoom");
}

void TestQCPAxisRect::explicitListWinsWhole()
{
  mPlot->setInteractions(QCP::iRangeDrag);
  mPlot->resize(400, 300);
  mPlot->replot();
  QCPAxisRect* ar = mPlot->axisRect();
  const QPointF centre = rectCentre(mPlot);

  mPlot->xAxis->setRangeDraggable(false);
  ar->setRangeDragAxes(mPlot->xAxis, nullptr);
  const QCPRange xStart = mPlot->xAxis->range();
  const QCPRange yStart = mPlot->yAxis->range();
  const QCPRange y2Start = mPlot->yAxis2->range();

  dragOn(mPlot, centre.toPoint(), (centre + QPointF(50, 50)).toPoint());

  QVERIFY2(!sameRange(mPlot->xAxis->range(), xStart),
           "the explicit list did not override the axis opt-out");
  QCOMPARE(mPlot->yAxis->range(), yStart);
  QCOMPARE(mPlot->yAxis2->range(), y2Start);
}

void TestQCPAxisRect::resetRestoresTheDefault()
{
  mPlot->setInteractions(QCP::iRangeDrag);
  mPlot->resize(400, 300);
  mPlot->replot();
  QCPAxisRect* ar = mPlot->axisRect();
  const QPointF centre = rectCentre(mPlot);

  mPlot->xAxis->setRangeDraggable(false);
  ar->setRangeDragAxes(mPlot->xAxis, nullptr);
  ar->resetRangeDragAxes();

  const QCPRange xStart = mPlot->xAxis->range();
  const QCPRange yStart = mPlot->yAxis->range();
  dragOn(mPlot, centre.toPoint(), (centre + QPointF(50, 50)).toPoint());

  QCOMPARE(mPlot->xAxis->range(), xStart);
  QVERIFY2(!sameRange(mPlot->yAxis->range(), yStart),
           "the reset did not bring the default set back");
}

void TestQCPAxisRect::linkedAxisMovesItsSourceOnce()
{
  mPlot->setInteractions(QCP::iRangeZoom);
  mPlot->resize(400, 300);
  mPlot->replot();
  mPlot->xAxis2->linkTo(mPlot->xAxis);

  const QPointF centre = rectCentre(mPlot);
  const double factor = mPlot->axisRect()->rangeZoomFactor(Qt::Horizontal);
  const QCPRange expected = zoomedBy(mPlot->xAxis, factor, centre.x());

  wheelOn(mPlot, centre);

  QVERIFY2(sameRange(mPlot->xAxis->range(), expected),
           qPrintable(QStringLiteral("source is (%1, %2)")
                          .arg(mPlot->xAxis->range().lower)
                          .arg(mPlot->xAxis->range().upper)));
  QVERIFY2(sameRange(mPlot->xAxis2->range(), mPlot->xAxis->range()),
           "the linked axis and its source disagree after one wheel step");
}

void TestQCPAxisRect::draggingOnlyTheLinkedAxisMovesTheSource()
{
  mPlot->setInteractions(QCP::iRangeDrag);
  mPlot->resize(400, 300);
  mPlot->replot();
  mPlot->xAxis2->linkTo(mPlot->xAxis, [](double v) { return 2.0 * v; },
                        [](double v) { return v / 2.0; });
  mPlot->axisRect()->setRangeDragAxes(mPlot->xAxis2, nullptr);

  const QPointF centre = rectCentre(mPlot);
  const QCPRange start = mPlot->xAxis->range();
  const double diff
      = mPlot->xAxis->pixelToCoord(centre.x()) - mPlot->xAxis->pixelToCoord(centre.x() + 40);
  const QCPRange expected(start.lower + diff, start.upper + diff);

  dragOn(mPlot, centre.toPoint(), (centre + QPointF(40, 0)).toPoint());

  QVERIFY2(sameRange(mPlot->xAxis->range(), expected),
           qPrintable(QStringLiteral("source is (%1, %2)")
                          .arg(mPlot->xAxis->range().lower)
                          .arg(mPlot->xAxis->range().upper)));
  QVERIFY2(sameRange(mPlot->xAxis2->range(),
                     QCPRange(2.0 * expected.lower, 2.0 * expected.upper)),
           "the linked axis no longer shows twice its source");
}

void TestQCPAxisRect::fullAxesBoxZoomsOnce()
{
  mPlot->setInteractions(QCP::iRangeZoom);
  mPlot->resize(400, 300);
  mPlot->replot();
  mPlot->axisRect()->setupFullAxesBox(true);
  mPlot->replot();

  const QPointF centre = rectCentre(mPlot);
  const double factor = mPlot->axisRect()->rangeZoomFactor(Qt::Horizontal);
  const QCPRange expected = zoomedBy(mPlot->xAxis, factor, centre.x());

  wheelOn(mPlot, centre);

  QVERIFY2(sameRange(mPlot->xAxis->range(), expected),
           qPrintable(QStringLiteral("source is (%1, %2)")
                          .arg(mPlot->xAxis->range().lower)
                          .arg(mPlot->xAxis->range().upper)));
  QVERIFY2(sameRange(mPlot->xAxis2->range(), mPlot->xAxis->range()),
           "the full axes box zoomed its two horizontal axes apart");
}
