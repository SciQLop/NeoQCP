#include "test-qcpaxisrect.h"
#include "../expect-debug.h"
#include "../keep-images.h"
#include "../reference-image.h"
#include "axis/labelrenderer.h"
#if NEOQCP_WITH_LATEX
#include "axis/latexlabelrenderer.h"
#endif
// The build must say whether it has LaTeX: both build configurations define the macro as 0 or 1.
// An undefined one is a build that forgot to pass it, not a build without LaTeX.
#ifndef NEOQCP_WITH_LATEX
#error "NEOQCP_WITH_LATEX must be defined by the build configuration (0 or 1)"
#endif

#include <QElapsedTimer>

#include <array>
#include <complex>
#include <numbers>
#include <random>
#include <ranges>
#include <vector>

// "Any range of numbers goes in": one concept for the keys, the values and the data columns.
static_assert(QCPNumberRange<std::vector<double>>);
static_assert(QCPNumberRange<QVector<int>>);
static_assert(QCPNumberRange<std::array<float, 3>>);
static_assert(QCPNumberRange<decltype(std::views::iota(0, 3))>);
static_assert(!QCPNumberRange<QStringList>);

namespace
{
int gMisuses = 0;
void countMisuse(const char*, const char*) { ++gMisuses; }
QByteArray gLastMisuse;
void recordMisuse(const char*, const char* what)
{
    ++gMisuses;
    gLastMisuse = what;
}

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

//! The text box an item measures: its corners are the topLeft and bottomRight anchors, in pixels.
QSize textBoxSize(const QCPItemText& item)
{
    const QPointF size = item.bottomRight->pixelPosition() - item.topLeft->pixelPosition();
    return QSize(qRound(size.x()), qRound(size.y()));
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

// The colour-map grid: three x values, four y values, z[i * 4 + j] = 10 * i + j (x-major), so
// every value is distinct and the last flat value is the corner (2, 3).
std::vector<double> gridValues()
{
    std::vector<double> z;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 4; ++j)
            z.push_back(10.0 * i + j);
    return z;
}

// examples/user-code/spectrogram: Hann windows of nfft samples, overlap samples shared, a
// one-sided power spectral density per window. Copied so the picture test draws the target itself.
struct Spectrogram
{
    std::vector<double> time, frequency, power;
};

Spectrogram spectrogram(const std::vector<double>& signal, int nfft, double rate, int overlap)
{
    std::vector<double> window(nfft);
    double energy = 0;
    for (int n = 0; n < nfft; ++n)
    {
        window[n] = 0.5 - 0.5 * std::cos(2 * std::numbers::pi * n / (nfft - 1));
        energy += window[n] * window[n];
    }
    Spectrogram s;
    for (int k = 1; k <= nfft / 2; ++k)
        s.frequency.push_back(k * rate / nfft);
    for (std::size_t start = 0; start + nfft <= signal.size(); start += nfft - overlap)
    {
        s.time.push_back((start + nfft / 2.0) / rate);
        for (int k = 1; k <= nfft / 2; ++k)
        {
            std::complex<double> sum = 0;
            for (int n = 0; n < nfft; ++n)
                sum += window[n] * signal[start + n]
                    * std::polar(1.0, -2 * std::numbers::pi * k * n / nfft);
            s.power.push_back((k < nfft / 2 ? 2 : 1) * std::norm(sum) / (rate * energy));
        }
    }
    return s;
}

// The spectrum panel of examples/user-code/spectrogram: noise, a 100 Hz tone and a 400 Hz chirp
// over one rect, log frequency, log power colour.
void drawSpectrogram(QCustomPlot& plot)
{
    std::mt19937 rng(19680801);
    std::uniform_real_distribution<double> noise(0.0, 0.01);
    const double dt = 0.0005;
    std::vector<double> t, x;
    for (int i = 0; i < 41000; ++i)
    {
        t.push_back(i * dt);
        const bool chirp = t.back() > 10 && t.back() < 12;
        x.push_back(std::sin(2 * std::numbers::pi * 100 * t.back())
                    + (chirp ? 2 * std::sin(2 * std::numbers::pi * 400 * t.back()) : 0)
                    + noise(rng));
    }
    const Spectrogram s = spectrogram(x, 1024, 1 / dt, 128);

    QCPAxisRect* spectrum = plot.axisRect();
    spectrum->colorMap(s.time, s.frequency, s.power,
                       { .gradient = QCPColorGradient::gpViridis, .dataScale = QCPScale::log() });
    spectrum->yAxis()->setScale(QCPScale::log());
    spectrum->yAxis()->setLabel("Frequency (Hz)");
    spectrum->colorScale()->setLabel("power (1/Hz)");

    // The rest of the target's main(), on its own rect: fit the axes, then the first 20 s.
    plot.rescaleAxes();
    spectrum->xAxis()->setRange(0, 20);
}

// A colour map draws from the plottable pipeline: let the result land before the picture.
bool waitUntilIdle(QCustomPlot& plot)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 10000)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        bool busy = false;
        for (int i = 0; i < plot.plottableCount(); ++i)
            busy = busy || plot.plottable(i)->busy();
        if (!busy)
            return true;
    }
    return false;
}
} // namespace

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

namespace
{
QImage exported(QCustomPlot& plot)
{
    return plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
}

// The ink box of the dark pixels in rows [top, bottom) of image, over the data rect's columns only
// (the left axis' top tick label pokes above the data rect, beside it); empty when there are none.
QRect inkBox(const QImage& image, const QCPAxisRect* rect, int top, int bottom)
{
    QRect box;
    for (int y = std::max(0, top); y < std::min(image.height(), bottom); ++y)
        for (int x = rect->rect().left() + 1; x < rect->rect().right(); ++x)
            if (qGray(image.pixel(x, y)) < 128)
                box |= QRect(x, y, 1, 1);
    return box;
}

int titleTextHeight(QCPAxisRect* rect)
{
    QFont font = rect->axis(QCPAxis::atTop)->labelFont();
    font.setPointSizeF(font.pointSizeF() * 1.2);
    return QFontMetrics(font).height();
}
} // namespace

void TestQCPAxisRect::aTitleTakesTheTopMargin()
{
    QCustomPlot plot;
    plot.resize(400, 300); // the size exported() draws at: the layout read back is that one
    QCPAxisRect* rect = plot.axisRect().get();
    const QImage before = exported(plot);
    const int topBefore = rect->rect().top();
    // Up to 2 px above the data rect: the left axis' top tick is antialiased into the row above it.
    const QRect stray = inkBox(before, rect, rect->outerRect().top(), topBefore - 2);
    QVERIFY2(stray.isEmpty(), qPrintable(QStringLiteral("ink %1,%2 %3x%4 above a data rect at %5,%6 in %7,%8")
                                             .arg(stray.x()).arg(stray.y()).arg(stray.width())
                                             .arg(stray.height()).arg(rect->rect().left())
                                             .arg(rect->rect().top()).arg(rect->outerRect().left())
                                             .arg(rect->outerRect().top())));

    QSignalSpy changed(rect, &QCPAxisRect::titleChanged);
    rect->setTitle(QStringLiteral("interpolation=True"));
    rect->setTitle(QStringLiteral("interpolation=True"));
    QCOMPARE(rect->title(), QStringLiteral("interpolation=True"));
    QCOMPARE(changed.count(), 1);
    QCOMPARE(changed.first().first().toString(), QStringLiteral("interpolation=True"));

    const QImage titled = exported(plot);
    QVERIFY2(rect->rect().top() - rect->outerRect().top() >= titleTextHeight(rect),
             "the top margin holds the whole title");
    const QRect ink = inkBox(titled, rect, rect->outerRect().top(), rect->rect().top() - 2);
    QVERIFY2(!ink.isEmpty(), "the title is drawn in the top margin");
    QVERIFY2(std::abs(ink.center().x() - rect->rect().center().x()) <= 2,
             "the title is centred over the data rect");

    rect->setTitle({});
    exported(plot);
    QCOMPARE(rect->rect().top(), topBefore);
    QCOMPARE(changed.count(), 2);
}

void TestQCPAxisRect::aTitleSitsAboveTheTopAxis()
{
    // With visible top tick labels the minimum margin no longer hides a small title: the title
    // adds exactly its own box above the axis.
    QCustomPlot plot;
    plot.resize(400, 300); // the size exported() draws at: the layout read back is that one
    QCPAxisRect* rect = plot.axisRect().get();
    rect->setupFullAxesBox();
    rect->axis(QCPAxis::atTop)->setTickLabels(true);
    exported(plot);
    const int axisOnly = rect->rect().top() - rect->outerRect().top();
    QVERIFY2(axisOnly > 15, "the top axis' labels exceed the minimum margin");

    rect->setTitle(QStringLiteral("Title"));
    const QImage titled = exported(plot);
    const int withTitle = rect->rect().top() - rect->outerRect().top();
    QVERIFY(withTitle - axisOnly >= titleTextHeight(rect));
    QVERIFY(withTitle - axisOnly <= titleTextHeight(rect) + 8);
    const QRect titleInk
        = inkBox(titled, rect, rect->outerRect().top(), rect->outerRect().top() + withTitle - axisOnly);
    QVERIFY2(!titleInk.isEmpty(), "the title sits in the band above the axis");
}

void TestQCPAxisRect::aHiddenRectHidesItsTitleAndColourScale()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    QCPAxisRect* rect = plot.axisRect().get();
    rect->setTitle(QStringLiteral("Hidden with its rect"));
    std::vector<double> z(4, 1.0);
    z[3] = 2.0;
    rect->colorMap(std::vector<double> { 0, 1 }, std::vector<double> { 0, 1 }, z);
    QImage shown = exported(plot);
    const QRect band(rect->outerRect().left(), rect->outerRect().top(), rect->outerRect().width(),
                     rect->rect().top() - 2 - rect->outerRect().top());
    const QRect bar(rect->rect().right() + 2, rect->rect().top(),
                    rect->outerRect().right() - rect->rect().right() - 2, rect->rect().height());
    auto inked = [](const QImage& image, const QRect& area)
    {
        for (int y = area.top(); y <= area.bottom(); ++y)
            for (int x = area.left(); x <= area.right(); ++x)
                if (image.valid(x, y) && qGray(image.pixel(x, y)) < 200)
                    return true;
        return false;
    };
    QVERIFY2(inked(shown, band), "the title is drawn while the rect is visible");
    QVERIFY2(inked(shown, bar), "the colour scale is drawn while the rect is visible");

    rect->setVisible(false);
    const QImage hidden = exported(plot);
    QVERIFY2(!inked(hidden, band), "a hidden rect's title is hidden too");
    QVERIFY2(!inked(hidden, bar), "a hidden rect's colour scale is hidden too");
}

void TestQCPAxisRect::aTitleSitsAboveATopColourScale()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    QCPAxisRect* rect = plot.axisRect().get();
    auto map = rect->colorMap(std::vector<double> { 0, 1 }, std::vector<double> { 0, 1 },
                              std::vector<double> { 0, 1, 2, 3 });
    rect->colorScale().setType(QCPAxis::atTop);
    exported(plot);
    const int scaleOnly = rect->rect().top() - rect->outerRect().top();
    QCPColorScale* scale = map->colorScale().get();
    QVERIFY(scale);

    rect->setTitle(QStringLiteral("Above the bar"));
    const QImage titled = exported(plot);
    const int withTitle = rect->rect().top() - rect->outerRect().top();
    QVERIFY(withTitle - scaleOnly >= titleTextHeight(rect));
    QVERIFY2(scale->outerRect().top() >= rect->outerRect().top() + withTitle - scaleOnly,
             "the bar sits below the title");
    QVERIFY2(!inkBox(titled, rect, rect->outerRect().top(), scale->outerRect().top()).isEmpty(),
             "the title is drawn above the bar");
}

void TestQCPAxisRect::aTitleTakesTheTopAxisLabelColour()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    QCPAxisRect* rect = plot.axisRect().get();
    rect->setTitle(QStringLiteral("Red"));
    rect->axis(QCPAxis::atTop)->setLabelColor(Qt::red);
    const QImage titled = exported(plot);
    const QRect ink = inkBox(titled, rect, rect->outerRect().top(), rect->rect().top() - 2);
    QVERIFY(!ink.isEmpty());
    bool red = false;
    for (int y = ink.top(); y <= ink.bottom() && !red; ++y)
        for (int x = ink.left(); x <= ink.right() && !red; ++x)
        {
            const QColor c = titled.pixelColor(x, y);
            red = c.red() > 150 && c.green() < 80 && c.blue() < 80;
        }
    QVERIFY2(red, "the title is drawn in the top axis' label colour");
}

void TestQCPAxisRect::aCentredTextIsHitWhereItIsDrawn()
{
    // A text element's hit box is the drawn text, wherever its flags put it in the element's rect.
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.plotLayout()->insertRow(0);
    auto* text = new QCPTextElement(&plot, QStringLiteral("centred"));
    plot.plotLayout()->addElement(0, 0, text);
    exported(plot);
    plot.replot(QCustomPlot::rpImmediateRefresh);
    const QRect area = text->rect();
    QVERIFY(area.width() > 200);
    QVERIFY2(text->selectTest(QPointF(area.center()), false) >= 0, "the text is hit where it is drawn");
    QVERIFY2(text->selectTest(QPointF(area.left() + 3, area.center().y()), false) < 0,
             "the empty left of the element is not the text");
}

void TestQCPAxisRect::titlesInARowAlign()
{
    QCustomPlot plot;
    plot.resize(400, 300); // the size exported() draws at: the layout read back is that one
    plot.setAxisRectGrid(1, 2);
    plot.axisRect(0, 1)->setTitle(QStringLiteral("right only"));
    exported(plot);
    QVERIFY(plot.axisRect(0, 1)->rect().top() > plot.axisRect(0, 1)->outerRect().top() + 15);
    QCOMPARE(plot.axisRect(0, 0)->rect().top(), plot.axisRect(0, 1)->rect().top());
}

namespace
{
// The ratio rect of \a parent's inner rect, (0, 0) top left, as addInset places an inset's data rect.
QRect ratioOf(const QCPAxisRect* parent, const QRectF& ratio)
{
    const QRect r = parent->rect();
    return QRect(int(r.x() + r.width() * ratio.x()), int(r.y() + r.height() * ratio.y()),
                 int(r.width() * ratio.width()), int(r.height() * ratio.height()));
}

bool near(const QRect& a, const QRect& b, int tolerance = 1)
{
    return std::abs(a.left() - b.left()) <= tolerance && std::abs(a.top() - b.top()) <= tolerance
        && std::abs(a.right() - b.right()) <= tolerance
        && std::abs(a.bottom() - b.bottom()) <= tolerance;
}

QString describe(const QRect& r)
{
    return QStringLiteral("%1,%2 %3x%4").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
}

// A shown plot whose frames go through QRhi, or false: a GPU pixel test needs the real backend.
bool showOnTheGpu(QCustomPlot& plot)
{
    plot.show();
    if (!QTest::qWaitForWindowExposed(&plot))
        return false;
    QCoreApplication::processEvents();
    return plot.rhi() != nullptr;
}

// The pixels of \a rect of \a image that \a match accepts.
template <typename Match>
int countColour(const QImage& image, const QRect& rect, Match match)
{
    int count = 0;
    for (int y = rect.top(); y <= rect.bottom(); ++y)
        for (int x = rect.left(); x <= rect.right(); ++x)
            if (match(image.pixelColor(x, y)))
                ++count;
    return count;
}

// \a rect in plot pixels, in the pixels of a frame drawn at \a scale.
QRect scaledRect(const QRect& rect, double scale)
{
    return QRect(qRound(rect.left() * scale), qRound(rect.top() * scale),
                 qRound(rect.width() * scale), qRound(rect.height() * scale));
}

bool isRed(const QColor& c)
{
    return c.red() > 170 && c.green() < 90 && c.blue() < 90;
}

bool isBlue(const QColor& c)
{
    return c.blue() > 150 && c.red() < 120 && c.green() < 120;
}

// The layers an inset made, in plot order.
QStringList insetLayerNames(QCustomPlot& plot)
{
    QStringList names;
    for (int i = 0; i < plot.layerCount(); ++i)
    {
        const QString name = plot.layer(i)->name();
        if (name.startsWith(QLatin1String("inset ")))
            names.append(name);
    }
    return names;
}

} // namespace

void TestQCPAxisRect::anInsetSitsInTheParentsAxisRatio()
{
    // On the default rect, whose inset layout keeps 12 px from the frame for the legend: the inset's
    // data rect is placed in the parent's data rect itself, not in that smaller one.
    QCustomPlot plot;
    plot.resize(400, 300);
    QCPAxisRect* parent = plot.axisRect().get();
    const QRectF ratio(0.5, 0.03, 0.47, 0.47);
    auto inset = parent->addInset(ratio);
    QVERIFY(inset);
    QCOMPARE(inset->axes().size(), 4);
    plot.replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY2(near(inset->rect(), ratioOf(parent, ratio)),
             qPrintable(describe(inset->rect()) + " for " + describe(ratioOf(parent, ratio))));
    QVERIFY2(inset->outerRect().contains(inset->rect()), "its tick labels sit outside its data rect");
}

void TestQCPAxisRect::anInsetFollowsItsParent()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.setAxisRectGrid(1, 2);
    QCPAxisRect* parent = plot.axisRect(0, 0).get();
    const QRectF ratio(0.1, 0.1, 0.5, 0.5);
    auto inset = parent->addInset(ratio);
    plot.replot(QCustomPlot::rpImmediateRefresh);
    plot.resize(600, 500);
    plot.replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY2(near(inset->rect(), ratioOf(parent, ratio)),
             qPrintable(describe(inset->rect()) + " for " + describe(ratioOf(parent, ratio))));
    const auto rects = plot.axisRects();
    QCOMPARE(rects.size(), 3);
    QCOMPARE(rects.last().get(), inset.get());
    QCOMPARE(plot.axisRect(0, 0).get(), parent);
    QCOMPARE(plot.axisRect(0).get(), parent);
}

void TestQCPAxisRect::anInsetDiesWithItsParent()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.setAxisRectGrid(2, 1);
    auto inset = plot.axisRect(1, 0)->addInset(QRectF(0.2, 0.2, 0.5, 0.5));
    auto graph = inset->line(std::vector<double> { 0, 1 }, std::vector<double> { 0, 1 });
    QVERIFY(graph);
    plot.setAxisRectGrid(1, 1);
    QVERIFY2(!inset, "the inset goes with its parent");
    QVERIFY2(!graph, "and so does its plottable");
    QCOMPARE(plot.axisRectCount(), 1);
    plot.toPixmap(400, 300);
}

void TestQCPAxisRect::anInsetTakesTheMouseFirst()
{
    mPlot->resize(400, 300);
    mPlot->setInteractions(QCP::iRangeDrag);
    QCPAxisRect* parent = mPlot->axisRect().get();
    auto inset = parent->addInset(QRectF(0.4, 0.1, 0.5, 0.5));
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    const QCPRange parentX = parent->xAxis()->range();
    const QCPRange insetX = inset->xAxis()->range();
    const QPoint start = inset->rect().center();
    QTest::mousePress(mPlot, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(mPlot, start + QPoint(30, 0));
    QTest::mouseRelease(mPlot, Qt::LeftButton, Qt::NoModifier, start + QPoint(30, 0));
    QVERIFY2(inset->xAxis()->range() != insetX, "the drag moved the inset");
    QCOMPARE(parent->xAxis()->range(), parentX);
}

void TestQCPAxisRect::anInsetDrawsAboveItsParent()
{
    // A parent line added after the inset must not ink over the inset's data rect.
    QCustomPlot plot;
    plot.resize(400, 300);
    QCPAxisRect* parent = plot.axisRect().get();
    auto inset = parent->addInset(QRectF(0.25, 0.25, 0.5, 0.5));
    parent->xAxis()->setRange(0, 1);
    parent->yAxis()->setRange(0, 1);
    parent->line(std::vector<double> { 0, 1 }, std::vector<double> { 0.5, 0.5 },
                 { .pen = QPen(Qt::red, 9) });
    const QImage image = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
    const QRect inside = inset->rect().adjusted(3, 3, -3, -3);
    int red = 0;
    for (int y = inside.top(); y <= inside.bottom(); ++y)
        for (int x = inside.left(); x <= inside.right(); ++x)
        {
            const QColor c = image.pixelColor(x, y);
            if (c.red() > 150 && c.green() < 80 && c.blue() < 80)
                ++red;
        }
    QCOMPARE(red, 0);
    const QColor outside = image.pixelColor(parent->rect().left() + 5, parent->rect().center().y());
    QVERIFY2(outside.red() > 150 && outside.green() < 80, "the parent line is drawn outside the inset");
}

void TestQCPAxisRect::anInsetExportsLikeTheScreen()
{
    // Review Focus 5: a never-shown plot lays out a grid, titles and an inset as a shown one does.
    auto build = [](QCustomPlot& plot)
    {
        plot.resize(400, 300);
        plot.setAxisRectGrid(2, 1);
        plot.axisRect(0, 0)->setTitle(QStringLiteral("top"));
        plot.axisRect(1, 0)->setTitle(QStringLiteral("bottom"));
        return plot.axisRect(1, 0)->addInset(QRectF(0.5, 0.1, 0.4, 0.5));
    };
    QCustomPlot shownPlot;
    auto shownInset = build(shownPlot);
    shownPlot.show();
    shownPlot.replot(QCustomPlot::rpImmediateRefresh);
    QCustomPlot hiddenPlot;
    auto hiddenInset = build(hiddenPlot);
    hiddenPlot.toPixmap(400, 300);
    QCOMPARE(hiddenInset->rect(), shownInset->rect());
    QCOMPARE(hiddenPlot.axisRect(1, 0)->rect(), shownPlot.axisRect(1, 0)->rect());
}

void TestQCPAxisRect::anInsetGridShowsOnTheGpu()
{
    // An inset's grid is painted on its own background layer: the plot's GPU grid layer draws a
    // rect's grid at the "grid" layer, far under the inset's opaque background, where it never shows.
    QCustomPlot plot;
    plot.resize(400, 300);
    if (!showOnTheGpu(plot))
        QSKIP("no QRhi available in this environment");
    QCPAxisRect* parent = plot.axisRect().get();
    parent->xAxis()->setRange(0, 3);
    parent->yAxis()->setRange(0, 3);
    auto inset = parent->addInset(QRectF(0.3, 0.15, 0.6, 0.6));
    inset->xAxis()->setRange(0, 3);
    inset->yAxis()->setRange(0, 3);
    inset->xAxis()->grid()->setPen(QPen(Qt::blue));
    inset->yAxis()->grid()->setPen(QPen(Qt::blue));
    plot.replot(QCustomPlot::rpImmediateRefresh);

    const QImage frame = plot.grabFramebuffer();
    const double scale = frame.width() / double(plot.width());
    const QRect data = inset->rect();
    const int row = qRound((data.top() + data.height() * 0.3) * scale);
    int drawn = 0;
    for (int x = qRound((data.left() + 2) * scale); x < qRound((data.right() - 1) * scale); ++x)
    {
        if (!isBlue(frame.pixelColor(x, row)))
            continue;
        ++drawn;
        double nearest = 1e9;
        for (double tick : inset->xAxis()->tickVector())
            nearest = std::min(nearest, std::abs(x - inset->xAxis()->coordToPixel(tick) * scale));
        QVERIFY2(nearest <= 2.5,
                 qPrintable(QStringLiteral("a grid pixel at %1 is not at a tick (%2)")
                                .arg(x)
                                .arg(nearest)));
    }
    QVERIFY2(drawn > 0, "the inset's grid must show on the GPU frame");
}

void TestQCPAxisRect::aLegendKeepsItsPixelsOverAnInset()
{
    // The legend draws above an inset's data, GPU lines included: an inset placed over it keeps
    // the legend's pixels.
    QCustomPlot plot;
    plot.resize(400, 300);
    if (!showOnTheGpu(plot))
        QSKIP("no QRhi available in this environment");
    QCPAxisRect* parent = plot.axisRect().get();
    parent->xAxis()->setRange(0, 1);
    parent->yAxis()->setRange(0, 1);
    plot.legend->setVisible(true);
    plot.legend->setBrush(QBrush(Qt::yellow));
    parent->line(std::vector<double> { 0, 1 }, std::vector<double> { 0.5, 0.5 },
                 { .pen = QPen(Qt::green, 3), .name = QStringLiteral("signal") });
    // The inset's own line must not add a row: its red legend icon would count as red pixels.
    plot.setAutoAddPlottableToLegend(false);

    auto inset = parent->addInset(QRectF(0.5, 0.03, 0.47, 0.5));
    inset->xAxis()->setRange(0, 1);
    inset->yAxis()->setRange(0, 1);
    QCPColorGradient redGradient;
    redGradient.setColorStopAt(0.0, QColor(Qt::red));
    redGradient.setColorStopAt(1.0, QColor(Qt::red));
    auto map = inset->colorMap(std::vector<double> { 0, 1 }, std::vector<double> { 0, 1 },
                               std::vector<double> { 0.5, 0.5, 0.5, 0.5 },
                               { .gradient = redGradient, .colorBar = false });
    QVERIFY(map);
    plot.replot();
    QTRY_VERIFY_WITH_TIMEOUT(!map->pipeline().isBusy(), 2000);
    plot.replot(QCustomPlot::rpImmediateRefresh);
    QCoreApplication::processEvents();
    (void)plot.grabFramebuffer(); // the frame the map's image lands in is the next one
    plot.replot(QCustomPlot::rpImmediateRefresh);
    QCoreApplication::processEvents();

    const QImage frame = plot.grabFramebuffer();
    const double scale = frame.width() / double(plot.width());
    const QRect legendBox = scaledRect(plot.legend->rect().adjusted(1, 1, -1, -1), scale);
    const int yellow = countColour(frame, legendBox,
                                   [](const QColor& c)
                                   { return c.red() > 170 && c.green() > 170 && c.blue() < 120; });
    const int red = countColour(frame, legendBox, isRed);
    QVERIFY2(yellow > 20, "the legend's background must still be there");
    QVERIFY2(red == 0, qPrintable(QStringLiteral("the inset's GPU image covers the legend: %1 red pixels").arg(red)));
}

void TestQCPAxisRect::anInsetDrawsAboveItsParentOnTheGpu()
{
    // The export path's anInsetDrawsAboveItsParent, on the screen frame: the inset's own layers
    // compose after the parent's GPU line.
    QCustomPlot plot;
    plot.resize(400, 300);
    if (!showOnTheGpu(plot))
        QSKIP("no QRhi available in this environment");
    QCPAxisRect* parent = plot.axisRect().get();
    parent->xAxis()->setRange(0, 1);
    parent->yAxis()->setRange(0, 1);
    auto inset = parent->addInset(QRectF(0.25, 0.25, 0.5, 0.5));
    parent->line(std::vector<double> { 0, 1 }, std::vector<double> { 0.5, 0.5 },
                 { .pen = QPen(Qt::red, 9) });
    plot.replot(QCustomPlot::rpImmediateRefresh);

    const QImage frame = plot.grabFramebuffer();
    const double scale = frame.width() / double(plot.width());
    const QRect inside = inset->rect().adjusted(3, 3, -3, -3);
    QCOMPARE(countColour(frame, scaledRect(inside, scale), isRed), 0);
    const QPoint outside(parent->rect().left() + 5, parent->rect().center().y());
    QVERIFY2(isRed(frame.pixelColor(qRound(outside.x() * scale), qRound(outside.y() * scale))),
             "the parent's line is drawn outside the inset");
}

void TestQCPAxisRect::theNewestInsetIsOnTop()
{
    // Each inset inserts its layers above the older insets', so the one added last draws on top; an
    // inset of an inset stays inside its parent's stack.
    QCustomPlot plot;
    plot.resize(400, 300);
    QCPAxisRect* parent = plot.axisRect().get();
    auto older = parent->addInset(QRectF(0.1, 0.1, 0.6, 0.6));
    auto newer = parent->addInset(QRectF(0.3, 0.3, 0.6, 0.6));
    auto nested = older->addInset(QRectF(0.05, 0.05, 0.3, 0.3));
    older->setBackground(QBrush(Qt::red));
    newer->setBackground(QBrush(Qt::green));
    nested->setBackground(QBrush(Qt::blue));
    const QImage image
        = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);

    const QRect overlap = older->rect().intersected(newer->rect());
    QVERIFY2(!overlap.isEmpty(), "the two insets overlap");
    const QColor second = image.pixelColor(overlap.center());
    QVERIFY2(second.green() > 150 && second.red() < 100,
             qPrintable(QStringLiteral("the newest inset must be on top, found %1,%2,%3")
                            .arg(second.red())
                            .arg(second.green())
                            .arg(second.blue())));
    const QColor insideNested = image.pixelColor(nested->rect().center());
    QVERIFY2(insideNested.blue() > 150 && insideNested.red() < 100,
             "an inset of an inset draws above its parent inset");
}


void TestQCPAxisRect::aPlottableMovedOntoAnInsetIsDrawn()
{
    // A plottable moved onto an inset's axes joins the inset's layer, so its opaque background does
    // not hide it; moved off, it goes back to the plot's data layer.
    QCustomPlot plot;
    plot.resize(400, 300);
    QCPAxisRect* parent = plot.axisRect().get();
    parent->xAxis()->setRange(0, 1);
    parent->yAxis()->setRange(0, 1);
    auto inset = parent->addInset(QRectF(0.25, 0.25, 0.5, 0.5));
    inset->xAxis()->setRange(0, 1);
    inset->yAxis()->setRange(0, 1);
    auto graph = parent->line(std::vector<double> { 0, 1 }, std::vector<double> { 0.5, 0.5 },
                              { .pen = QPen(Qt::red, 9) });

    qcp::ops::setAxisRect(*graph, inset.get());
    plot.replot();
    QImage image = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
    QVERIFY2(countColour(image, inset->rect().adjusted(3, 3, -3, -3), isRed) > 0,
             "a plottable moved onto an inset is drawn in it");

    qcp::ops::setAxisRect(*graph, parent);
    plot.replot();
    image = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
    QCOMPARE(countColour(image, inset->rect().adjusted(3, 3, -3, -3), isRed), 0);
    QCOMPARE(graph->layer(), plot.layer(QStringLiteral("main")).get());
}


void TestQCPAxisRect::anItemFollowsTheAreaItIsIn()
{
    // An item whose positions name an inset measures there and draws there; moved back to the
    // parent rect, it goes under the inset's opaque background again.
    QCustomPlot plot;
    plot.resize(400, 300);
    QCPAxisRect* parent = plot.axisRect().get();
    auto inset = parent->addInset(QRectF(0.25, 0.25, 0.5, 0.5));
    auto item = plot.add<QCPItemRect>();
    for (QCPItemPosition* position : { item->topLeft, item->bottomRight })
    {
        position->setType(QCPItemPosition::ptAxisRectRatio);
        position->setAxisRect(inset.get());
    }
    item->topLeft->setCoords(0.2, 0.2);
    item->bottomRight->setCoords(0.8, 0.8);
    item->setBrush(QBrush(Qt::red));
    item->setPen(QPen(Qt::NoPen));

    plot.replot();
    QImage image = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
    QVERIFY2(countColour(image, inset->rect().adjusted(4, 4, -4, -4), isRed) > 100,
             "an item placed on an inset is drawn in it");

    for (QCPItemPosition* position : { item->topLeft, item->bottomRight })
        position->setAxisRect(parent);
    plot.replot();
    image = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
    QCOMPARE(countColour(image, inset->rect().adjusted(4, 4, -4, -4), isRed), 0);
    QCOMPARE(item->layer(), plot.layer(QStringLiteral("annotations")).get());
}


void TestQCPAxisRect::thePlotBackgroundReachesAnInset()
{
    // An inset that follows the theme follows a later setBackground() too.
    QCustomPlot plot;
    plot.resize(400, 300);
    auto inset = plot.axisRect()->addInset(QRectF(0.25, 0.25, 0.5, 0.5));
    plot.setBackground(QBrush(QColor(255, 255, 0)));
    QCOMPARE(inset->backgroundBrush().color(), QColor(255, 255, 0));
    plot.replot();
    const QImage image
        = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
    const QColor inside = image.pixelColor(inset->rect().center());
    QVERIFY2(inside.red() > 200 && inside.green() > 200 && inside.blue() < 80,
             qPrintable(QStringLiteral("the inset follows the plot's background, found %1,%2,%3")
                            .arg(inside.red())
                            .arg(inside.green())
                            .arg(inside.blue())));
}


void TestQCPAxisRect::aUserBackgroundSurvivesTheTheme()
{
    // The theme colours an inset that never chose a background; one that did keeps it.
    QCustomPlot plot;
    plot.resize(400, 300);
    auto plain = plot.axisRect()->addInset(QRectF(0.05, 0.05, 0.3, 0.3));
    auto chosen = plot.axisRect()->addInset(QRectF(0.5, 0.5, 0.3, 0.3));
    chosen->setBackground(QBrush(QColor(0, 200, 0)));
    auto* theme = QCPTheme::dark();
    plot.setTheme(theme);
    QCOMPARE(plain->backgroundBrush().color(), QColor(QStringLiteral("#1e1e1e")));
    QCOMPARE(chosen->backgroundBrush().color(), QColor(0, 200, 0));
    delete theme;
}


void TestQCPAxisRect::anInsetsTitleAndScaleGoOnItsAxesLayer()
{
    // An inset's own title and colour scale draw on its axes layer, above its data and above its
    // parent inset's opaque background.
    QCustomPlot plot;
    plot.resize(400, 300);
    auto inset = plot.axisRect()->addInset(QRectF(0.1, 0.1, 0.5, 0.5));
    auto nested = inset->addInset(QRectF(0.3, 0.3, 0.6, 0.6));
    nested->axis(QCPAxis::atTop)->setLabelColor(QColor(Qt::magenta));
    nested->setTitle(QStringLiteral("nested"));
    nested->colorMap(std::vector<double> { 0, 1 }, std::vector<double> { 0, 1 },
                     std::vector<double> { 0, 1, 2, 3 });
    plot.replot();

    QCOMPARE(nested->colorScale()->layer(), nested->xAxis()->layer());
    const QImage image
        = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
    const QRect band(nested->rect().left(), nested->rect().top() - 22, nested->rect().width(), 22);
    const int title = countColour(image, band.intersected(image.rect()),
                                  [](const QColor& c)
                                  { return c.red() > 150 && c.blue() > 150 && c.green() < 120; });
    QVERIFY2(title > 0, "the nested inset's title is drawn above its parent's background");
}


void TestQCPAxisRect::anInsetLayerNamesAreStable()
{
    // A layer name counts the plot's insets instead of naming where the rect landed in memory.
    QCustomPlot first;
    first.resize(400, 300);
    auto older = first.axisRect()->addInset(QRectF(0.1, 0.1, 0.3, 0.3));
    first.axisRect()->addInset(QRectF(0.2, 0.2, 0.3, 0.3));
    const QStringList expected { QStringLiteral("inset 1 back"), QStringLiteral("inset 1 data"),
                                 QStringLiteral("inset 1 axes"), QStringLiteral("inset 2 back"),
                                 QStringLiteral("inset 2 data"), QStringLiteral("inset 2 axes") };
    QCOMPARE(insetLayerNames(first), expected);

    QCustomPlot second;
    second.resize(400, 300);
    second.axisRect()->addInset(QRectF(0.1, 0.1, 0.3, 0.3));
    QCOMPARE(insetLayerNames(second), expected.mid(0, 3));

    older.remove();
    QCOMPARE(insetLayerNames(first), expected.mid(3));
}


void TestQCPAxisRect::addInsetRefusesBadRatios()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    const int before = plot.axisRects().size();
    const auto previous = qcp::setMisuseHandler(recordMisuse);
    const QList<QRectF> bad { QRectF(0, 0, 0, 0.5), QRectF(0, 0, -0.2, 0.5),
                              QRectF(0, 0, 0.5, -1), QRectF(qQNaN(), 0, 0.5, 0.5),
                              QRectF(0, 0, 0.5, qInf()) };
    for (const QRectF& ratio : bad)
    {
        gMisuses = 0;
        const auto inset = plot.axisRect()->addInset(ratio);
        QVERIFY2(!inset, "a ratio rect of no positive, finite size makes no inset");
        QCOMPARE(gMisuses, 1);
    }
    QCOMPARE(plot.axisRects().size(), before);

    // A rect whose plot is being torn down makes nothing either.
    auto* doomed = new QCustomPlot;
    doomed->resize(400, 300);
    QCPAxisRect* rect = doomed->axisRect().get();
    QCPAxis* axis = rect->xAxis();
    QObject collector;
    bool refused = false;
    int reported = 0;
    QObject::connect(axis, &QObject::destroyed, &collector,
                     [&]
                     {
                         gMisuses = 0;
                         refused = !rect->addInset(QRectF(0.1, 0.1, 0.2, 0.2));
                         reported = gMisuses;
                     });
    delete doomed;
    qcp::setMisuseHandler(previous);
    QVERIFY2(refused, "an inset needs a live plot");
    QCOMPARE(reported, 1);
}


void TestQCPAxisRect::aBrokenInsetLayerLeavesNothingBehind()
{
    // A layer already named like the inset's: the layers made before the failure go too, and so
    // does the rect.
    QCustomPlot plot;
    plot.resize(400, 300);
    QVERIFY(plot.addLayer(QStringLiteral("inset 1 data")));
    const auto previous = qcp::setMisuseHandler(recordMisuse);
    gMisuses = 0;
    const auto inset = plot.axisRect()->addInset(QRectF(0.1, 0.1, 0.3, 0.3));
    qcp::setMisuseHandler(previous);
    QVERIFY2(!inset, "the inset is not made when its layers cannot be");
    QCOMPARE(gMisuses, 1);
    QVERIFY2(!plot.layer(QStringLiteral("inset 1 back")),
             "the layers made before the failure are removed");
    QCOMPARE(insetLayerNames(plot), QStringList { QStringLiteral("inset 1 data") });
    QCOMPARE(plot.axisRects().size(), 1);
}


void TestQCPAxisRect::lineTakesDataAndOptions()
{
  QCPAxisRect *ar = mPlot->axisRect();
  const std::vector<double> keys { 0, 1, 2 };
  const std::vector<double> values { 3, 2, 1 };
  const QCPLineOptions options { .pen = QPen(Qt::red, 3), .name = QStringLiteral("signal") };

  // The data goes through the graph's data constructor: one pass over the ranges, no setData after.
  const QCPHandle<QCPGraph> graph = ar->line(keys, values, options);
  QVERIFY(graph);
  QCOMPARE(graph->pen(), options.pen);
  QCOMPARE(graph->name(), QStringLiteral("signal"));
  QCOMPARE(graph->dataCount(), 3);
  QCOMPARE(graph->dataMainValue(0), 3.0);
  QCOMPARE(graph->dataMainValue(2), 1.0);
  QCOMPARE(graph->keyAxis().get(), ar->xAxis().get());
}

void TestQCPAxisRect::lineWithNoPenDrawsMarkersOnly()
{
  QCPAxisRect *ar = mPlot->axisRect();
  const QCPHandle<QCPGraph> markers =
      ar->line(std::vector<double> { 0, 1 }, std::vector<int> { 0, 1 },
               { .pen = QPen(Qt::NoPen),
                 .scatterStyle = QCPScatterStyle(QCPScatterStyle::ssDisc, 5) });
  QVERIFY(markers);
  QCOMPARE(markers->pen().style(), Qt::NoPen);
  QCOMPARE(markers->scatterStyle().shape(), QCPScatterStyle::ssDisc);
  QCOMPARE(markers->scatterStyle().size(), 5.0);
  QVERIFY(markers->name().isEmpty());
}

void TestQCPAxisRect::textTakesAPositionAndAStyle()
{
  QCPAxisRect *ar = mPlot->axisRect();
  QCPTextStyle style;
  style.font = QFont(QStringLiteral("Serif"), 14);
  style.color = QColor(Qt::darkGreen);
  style.pen = QPen(Qt::darkBlue);
  style.brush = QBrush(QColor(255, 255, 255, 200));
  style.alignment = Qt::AlignRight | Qt::AlignBottom;
  style.rotationDegrees = 12.0;
  style.padding = QMarginsF(4, 5, 6, 7);

  const QCPHandle<QCPItemText> label =
      ar->text(QCP::AxisRatio { 0.05, 0.25 }, QStringLiteral("t"), style);
  QVERIFY(label);
  QCOMPARE(label->text(), QStringLiteral("t"));
  QCOMPARE(label->font(), style.font);
  QCOMPARE(label->color(), style.color);
  QCOMPARE(label->pen(), style.pen);
  QCOMPARE(label->brush(), style.brush);
  QCOMPARE(label->positionAlignment(), style.alignment);
  QCOMPARE(label->rotation(), style.rotationDegrees);
  QCOMPARE(label->padding(), QMargins(4, 5, 6, 7));

  // The position keeps the space it was given: a ratio of the rect, not the plot's axes.
  QCOMPARE(label->position->type(), QCPItemPosition::ptAxisRectRatio);
  QCOMPARE(label->position->coords(), QPointF(0.05, 0.25));
  QVERIFY(label->clipToAxisRect());
}

void TestQCPAxisRect::textAppliesEveryStyleField()
{
  QCPAxisRect *ar = mPlot->axisRect();
  QCPTextStyle style;
  // These equal QCPTextStyle's own defaults, which the item does not share: the call must still
  // apply them, not compare against the defaults and skip.
  QCOMPARE(style.alignment, Qt::Alignment(Qt::AlignLeft | Qt::AlignTop));
  style.textFlags = Qt::AlignRight | Qt::AlignVCenter;
  style.color = QColor(); // unset: the plot's foreground shows through

  const QCPHandle<QCPItemText> label =
      ar->text(QCP::Pixels { 20, 20 }, QStringLiteral("plain"), style);
  QVERIFY(label);
  QCOMPARE(label->positionAlignment(), Qt::Alignment(Qt::AlignLeft | Qt::AlignTop));
  QCOMPARE(label->textAlignment(), Qt::Alignment(Qt::AlignRight | Qt::AlignVCenter));
  QCOMPARE(label->color(), QColor(Qt::black)); // no theme on this plot, so black is its foreground
}

void TestQCPAxisRect::textItemMathTextIsOnByDefault()
{
  const QCPHandle<QCPItemText> label = mPlot->add<QCPItemText>();
  QVERIFY(label);
  // Today's look: the item has always been measured and drawn through the label renderer.
  QCOMPARE(label->mathText(), true);
}

void TestQCPAxisRect::textCopiesTheStyleMathText()
{
  QCPAxisRect *ar = mPlot->axisRect();
  // A default QCPTextStyle is a drawing context's, whose mathText is off: the item follows it, so
  // a caller opts in per label.
  const QCPHandle<QCPItemText> plain = ar->text(QCP::Pixels { 20, 20 }, QStringLiteral("x"));
  QVERIFY(plain);
  QCOMPARE(plain->mathText(), false);

  const QCPHandle<QCPItemText> math = ar->text(
      QCP::Pixels { 20, 20 }, QStringLiteral("$x$"), { .mathText = true });
  QVERIFY(math);
  QCOMPARE(math->mathText(), true);

  // The flag the shortcut copies is the one the item measures with, not a field kept beside it.
  mPlot->replot();
  const int plainFlags = int(Qt::TextDontClip | plain->textAlignment());
  QCOMPARE(textBoxSize(*plain),
           QCP::paintedMetrics(plain->font())
               .boundingRect(0, 0, 0, 0, plainFlags, QStringLiteral("x"))
               .size());
  const int mathFlags = int(Qt::TextDontClip | math->textAlignment());
  QCOMPARE(textBoxSize(*math),
           QCPLabelRenderer::measureRectWith(QCPLabelRenderer::defaultRenderer(), math->font(),
                                             QStringLiteral("$x$"), mathFlags)
               .size());
}

void TestQCPAxisRect::flippingMathTextChangesTheMeasuredBox()
{
  const QFont font(QStringLiteral("Serif"), 14);
  const QString text = QStringLiteral("MMMM a text item");
  const QCPHandle<QCPItemText> label = mPlot->add<QCPItemText>();
  label->setFont(font);
  label->setText(text);
  label->setPositionAlignment(Qt::AlignLeft | Qt::AlignTop);
  mPlot->replot();
  QVERIFY(label->mathText());
  const QSize measured = textBoxSize(*label);

  // The renderer measures a plain string a pixel wider and taller than the font's own metrics on
  // some platform fonts, the same on others: a flip after a measurement must land on the plain
  // measurement, and move the box wherever the two differ, never keep the first one.
  label->setMathText(false);
  mPlot->replot();
  const QSize plainBox = textBoxSize(*label);
  const int flags = int(Qt::TextDontClip | label->textAlignment());
  const QSize plainMetrics
      = QCP::paintedMetrics(font).boundingRect(0, 0, 0, 0, flags, text).size();
  QCOMPARE(plainBox, plainMetrics);
  QCOMPARE(plainBox == measured, measured == plainMetrics);
}

void TestQCPAxisRect::aPlainTextItemMeasuresWithFontMetrics()
{
  const QFont font(QStringLiteral("Serif"), 14);
  const QString text = QStringLiteral("MMMM a text item");
  const QCPHandle<QCPItemText> label = mPlot->add<QCPItemText>();
  label->setFont(font);
  label->setText(text);
  label->setMathText(false);
  label->setPositionAlignment(Qt::AlignLeft | Qt::AlignTop);

  const int flags = int(Qt::TextDontClip | label->textAlignment());
  QCOMPARE(textBoxSize(*label),
           QCP::paintedMetrics(font).boundingRect(0, 0, 0, 0, flags, text).size());
}

void TestQCPAxisRect::aMathTextItemMeasuresThroughTheRenderer()
{
  const QFont font(QStringLiteral("Serif"), 14);
  const QString text = QStringLiteral("MMMM a text item");
  const QCPHandle<QCPItemText> label = mPlot->add<QCPItemText>();
  label->setFont(font);
  label->setText(text);
  label->setPositionAlignment(Qt::AlignLeft | Qt::AlignTop);
  QVERIFY(label->mathText());

  const int flags = int(Qt::TextDontClip | label->textAlignment());
  QCOMPARE(textBoxSize(*label),
           QCPLabelRenderer::measureRectWith(QCPLabelRenderer::defaultRenderer(), font, text, flags)
               .size());
}

void TestQCPAxisRect::aMathTextItemTypesetsLatex()
{
#if NEOQCP_WITH_LATEX
  const QFont font(QStringLiteral("Serif"), 14);
  const QString math = QStringLiteral("$\\alpha^2$");
  const QCPHandle<QCPItemText> label = mPlot->add<QCPItemText>();
  label->setFont(font);
  label->setText(math);
  label->setTextAlignment(Qt::AlignLeft);
  label->setPositionAlignment(Qt::AlignLeft | Qt::AlignTop);

  const int flags = int(Qt::TextDontClip | Qt::AlignLeft);
  QCOMPARE(textBoxSize(*label),
           QCPLabelRenderer::measureRectWith(QCPLatexLabelRenderer::instance(), font, math, flags)
               .size());
  // The typeset span is not the literal source a font would measure.
  QVERIFY(textBoxSize(*label).width()
          != QCP::paintedMetrics(font).boundingRect(0, 0, 0, 0, flags, math).width());
#else
  QSKIP("built without LaTeX");
#endif
}

void TestQCPAxisRect::ellipseTakesTwoCornersInTheirOwnSpaces()
{
  QCPAxisRect *ar = mPlot->axisRect();
  const QBrush fill(QColor(0, 0, 255, 40));
  const QCPHandle<QCPItemEllipse> shaded = ar->ellipse(
      QCP::Data { 1, 2 }, QCP::Pixels { 300, 220 }, { .pen = QPen(Qt::darkCyan), .brush = fill });
  QVERIFY(shaded);
  QCOMPARE(shaded->topLeft->type(), QCPItemPosition::ptPlotCoords);
  QCOMPARE(shaded->topLeft->coords(), QPointF(1, 2));
  QCOMPARE(shaded->bottomRight->type(), QCPItemPosition::ptAbsolute);
  QCOMPARE(shaded->bottomRight->coords(), QPointF(300, 220));
  QCOMPARE(shaded->pen().color(), QColor(Qt::darkCyan));
  QCOMPARE(shaded->brush(), fill);
  QVERIFY(shaded->clipToAxisRect());
}

void TestQCPAxisRect::multiAxis()
{
  QCPAxisRect *ar = mPlot->axisRect();
  QCOMPARE(ar->axisCount(QCPAxis::atLeft), 1);
  QCOMPARE(ar->axisCount(QCPAxis::atRight), 1);
  QCOMPARE(ar->axisCount(QCPAxis::atBottom), 1);
  QCOMPARE(ar->axisCount(QCPAxis::atTop), 1);
  
  // test adding of axes:
  QList<QCPHandle<QCPAxis>> newAxes = ar->addAxes(QCPAxis::atLeft | QCPAxis::atBottom);
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

void TestQCPAxisRect::xAxisAndYAxisAreTheFirstBottomAndLeft()
{
  QCPAxisRect *ar = mPlot->axisRect();
  const QCPHandle<QCPAxis> bottom = ar->xAxis();
  const QCPHandle<QCPAxis> left = ar->yAxis();
  QCOMPARE(bottom.get(), ar->axis(QCPAxis::atBottom).get());
  QCOMPARE(left.get(), ar->axis(QCPAxis::atLeft).get());

  // The pair add<T> binds to: a plottable added without axes lands on xAxis() and yAxis().
  const QCPHandle<QCPGraph> graph = ar->add<QCPGraph>();
  QVERIFY(graph);
  QCOMPARE(graph->keyAxis().get(), bottom.get());
  QCOMPARE(graph->valueAxis().get(), left.get());

  // A second axis of a side comes after the first: the rect hands out the first, not the last.
  ar->addAxis(QCPAxis::atBottom);
  ar->addAxis(QCPAxis::atLeft);
  QCOMPARE(ar->xAxis().get(), bottom.get());
  QCOMPARE(ar->yAxis().get(), left.get());
}

void TestQCPAxisRect::xAxisAndYAxisAreEmptyOnceTheAxisIsRemoved()
{
  QCPAxisRect *ar = mPlot->axisRect();
  QVERIFY(ar->removeAxis(ar->xAxis()));
  // Asking for an axis the rect no longer has is a question, not misuse: empty, and silent.
  gMisuses = 0;
  const auto previous = qcp::setMisuseHandler(countMisuse);
  QVERIFY(!ar->xAxis());
  QCOMPARE(gMisuses, 0);
  qcp::setMisuseHandler(previous);
  QVERIFY(ar->yAxis());

  QVERIFY(ar->removeAxis(ar->yAxis()));
  QVERIFY(!ar->yAxis());

  // The rect regained its bottom axis: xAxis() hands the new one out again.
  QCPAxis *again = ar->addAxis(QCPAxis::atBottom);
  QCOMPARE(ar->xAxis().get(), again);
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
  // A frame that lost its axes rescales quietly: the plottable is inert, and saying so once is
  // drawableAxes()'s job, not the rescale's.
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
  QCPItemLine *item = mPlot->add<QCPItemLine>();
  mPlot->replot();

  QVERIFY(mPlot->axisRect()->removeAxis(mPlot->xAxis));
  // The item is recorded from its axes: without them the recording is refused, once for the item,
  // and it draws nothing.
  expectMissingAxis();
  mPlot->replot();
  QVERIFY(mPlot->axisRect()->removeAxis(mPlot->yAxis));
  mPlot->replot(); // the item already reported its missing axis
  
  
  gMisuses = 0;
  const auto previousHandler = qcp::setMisuseHandler(countMisuse);
  item->start->setPixelPosition(QPointF(1, 2));
  qcp::setMisuseHandler(previousHandler);
  QCOMPARE(gMisuses, 1); // one report for the position, not one per slot
  
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
  // A frame that lost its axes rescales quietly: the plottable is inert, and saying so once is
  // drawableAxes()'s job, not the rescale's.
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
  QCPItemLine *item = mPlot->add<QCPItemLine>();
  item->setClipAxisRect(mPlot->axisRect());
  item->setClipToAxisRect(true);
  item->start->setAxisRect(mPlot->axisRect());
  item->start->setType(QCPItemPosition::ptAxisRectRatio);
  mPlot->replot();
  
  QVERIFY(mPlot->plotLayout()->removeAt(0));
  mPlot->plotLayout()->simplify();
  QCOMPARE(mPlot->plotLayout()->elementCount(), 0);
  // Both positions lost their axis rect and their axes: the recording is refused, once.
  expectMissingAxis();
  mPlot->layer("main")->markDirty();
  mPlot->replot();
  
  gMisuses = 0;
  const auto previousHandler = qcp::setMisuseHandler(countMisuse);
  item->start->setPixelPosition(QPointF(1, 2));
  qcp::setMisuseHandler(previousHandler);
  QCOMPARE(gMisuses, 1); // one report for the position, not one per slot
  
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
  plot.yAxis->setScale(QCPScale::log());
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
  QCOMPARE(qcp::pointers(ar->rangeDragAxes(Qt::Horizontal)), (QList<QCPAxis*> { mPlot->xAxis, mPlot->xAxis2 }));
  QCOMPARE(qcp::pointers(ar->rangeDragAxes(Qt::Vertical)), (QList<QCPAxis*> { mPlot->yAxis, mPlot->yAxis2 }));
  QCOMPARE(qcp::pointers(ar->rangeZoomAxes(Qt::Horizontal)), (QList<QCPAxis*> { mPlot->xAxis, mPlot->xAxis2 }));
  QCOMPARE(qcp::pointers(ar->rangeZoomAxes(Qt::Vertical)), (QList<QCPAxis*> { mPlot->yAxis, mPlot->yAxis2 }));
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

void TestQCPAxisRect::optingASourceOutStopsItsLinkedAxis()
{
    mPlot->resize(400, 300);
    mPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    mPlot->axisRect()->setupFullAxesBox(true);
    mPlot->xAxis->setRangeDraggable(false);
    mPlot->replot();
    const QPoint from = rectCentre(mPlot).toPoint();
    const QCPRange before = mPlot->xAxis->range();
    dragOn(mPlot, from, from + QPoint(40, 0));
    QCOMPARE(mPlot->xAxis->range(), before); // the source opted out...
    QCOMPARE(mPlot->xAxis2->range(), before); // ...so its linked axis stays put too
    mPlot->xAxis->setRangeDraggable(true);
    mPlot->xAxis->setRangeZoomable(false);
    wheelOn(mPlot, rectCentre(mPlot));
    QCOMPARE(mPlot->xAxis->range(), before);
    QCOMPARE(mPlot->xAxis2->range(), before);
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

void TestQCPAxisRect::aDragOnALinkedAxisMovesItsSourceBySourcePixels()
{
    mPlot->resize(400, 300);
    mPlot->setInteractions(QCP::iRangeDrag);
    auto* linkedRect = new QCPAxisRect(mPlot);
    mPlot->plotLayout()->addElement(0, 1, linkedRect);
    mPlot->plotLayout()->setColumnStretchFactor(0, 2);
    mPlot->plotLayout()->setColumnStretchFactor(1, 1);
    QCPAxis* linked = linkedRect->axis(QCPAxis::atBottom);
    linked->linkTo(
        mPlot->xAxis, [](double v) { return 1.0 / v; }, [](double v) { return 1.0 / v; });
    mPlot->xAxis->setRange(1, 10);
    linkedRect->setRangeDragAxes(linked, nullptr);
    mPlot->replot();

    const double sourceWidth = mPlot->axisRect()->rect().width();
    const double linkedWidth = linkedRect->rect().width();
    QVERIFY2(linkedWidth < sourceWidth, "the linked rect is the narrower one");
    const QPointF centre = QRectF(linkedRect->rect()).center();
    // The same fraction of the data: 40 px on the half-width rect is 80 px on the source.
    const double sourceTravel = 40.0 * sourceWidth / linkedWidth;
    const QCPRange expected
        = draggedByPixels(mPlot->xAxis, centre.x(), centre.x() - sourceTravel);
    const double sizeBefore = mPlot->xAxis->range().size();

    dragOn(mPlot, centre.toPoint(), centre.toPoint() - QPoint(40, 0));

    QVERIFY2(qAbs(mPlot->xAxis->range().size() - sizeBefore) < 1e-9,
             "a drag translates the source, it does not scale it");
    QVERIFY2(sameRange(mPlot->xAxis->range(), expected),
             qPrintable(QStringLiteral("got (%1, %2), expected (%3, %4)")
                            .arg(mPlot->xAxis->range().lower)
                            .arg(mPlot->xAxis->range().upper)
                            .arg(expected.lower)
                            .arg(expected.upper)));
}

void TestQCPAxisRect::aWheelOnALinkedAxisZoomsItsSourceAroundTheSourceValue()
{
    mPlot->resize(400, 300);
    mPlot->setInteractions(QCP::iRangeZoom);
    auto* linkedRect = new QCPAxisRect(mPlot);
    mPlot->plotLayout()->addElement(0, 1, linkedRect);
    mPlot->plotLayout()->setColumnStretchFactor(0, 2);
    mPlot->plotLayout()->setColumnStretchFactor(1, 1);
    QCPAxis* linked = linkedRect->axis(QCPAxis::atBottom);
    linked->linkTo(
        mPlot->xAxis, [](double v) { return 1.0 / v; }, [](double v) { return 1.0 / v; });
    mPlot->xAxis->setRange(1, 10);
    linkedRect->setRangeZoomAxes(linked, nullptr);
    mPlot->replot();

    const QRect linkedArea = linkedRect->rect();
    const QPointF pos = QRectF(linkedArea).center();
    const double fraction = (pos.x() - linkedArea.left()) / double(linkedArea.width());
    const QRect sourceArea = mPlot->axisRect()->rect();
    const double sourcePixel = sourceArea.left() + fraction * sourceArea.width();
    const QCPRange expected = zoomedBy(
        mPlot->xAxis, mPlot->axisRect()->rangeZoomFactor(Qt::Horizontal), sourcePixel);

    wheelOn(mPlot, pos);

    QVERIFY2(sameRange(mPlot->xAxis->range(), expected),
             qPrintable(QStringLiteral("got (%1, %2), expected (%3, %4)")
                            .arg(mPlot->xAxis->range().lower)
                            .arg(mPlot->xAxis->range().upper)
                            .arg(expected.lower)
                            .arg(expected.upper)));
}

void TestQCPAxisRect::aLinkedScaledAxisDragMovesItsSourceLinearly()
{
    mPlot->resize(400, 300);
    mPlot->setInteractions(QCP::iRangeDrag);
    QCPAxis* linked = mPlot->axisRect()->addAxis(QCPAxis::atTop);
    // Identity functions spelled out: a link through functions keeps its own scale, so the log
    // follower of a linear source (what this test drags) exists. An identity link shares the scale.
    linked->linkTo(mPlot->xAxis, [](double v) { return v; }, [](double v) { return v; });
    mPlot->xAxis->setRange(1, 10);
    linked->setScale(QCPScale::log());
    mPlot->axisRect()->setRangeDragAxes(linked, nullptr);
    mPlot->replot();

    const QPointF centre = rectCentre(mPlot);
    const QCPRange expected = draggedByPixels(mPlot->xAxis, centre.x(), centre.x() - 40.0);

    dragOn(mPlot, centre.toPoint(), centre.toPoint() - QPoint(40, 0));

    // The linked axis' own scale type picks its ticks; the source moves by its own linear pixels.
    QVERIFY2(sameRange(mPlot->xAxis->range(), expected),
             qPrintable(QStringLiteral("got (%1, %2), expected (%3, %4)")
                            .arg(mPlot->xAxis->range().lower)
                            .arg(mPlot->xAxis->range().upper)
                            .arg(expected.lower)
                            .arg(expected.upper)));
}

void TestQCPAxisRect::colorMapInOneCall()
{
    QCPAxisRect *ar = mPlot->axisRect();
    const std::vector<double> x { 0, 1, 2 };
    const std::vector<double> y { 10, 20, 30, 40 };
    const std::vector<double> z = gridValues();

    const QCPHandle<QCPColorMap2> map = ar->colorMap(
        x, y, z, { .gradient = QCPColorGradient::gpJet, .dataScale = QCPScale::symlog(2) });
    QVERIFY(map);

    // The data reaches the map's own storage, x-major: z[i * ySize() + j].
    const QCPDataSource2D *source = map->dataSource();
    QCOMPARE(source->xSize(), 3);
    QCOMPARE(source->ySize(), 4);
    QCOMPARE(source->zAt(0, 0), 0.0);
    QCOMPARE(source->zAt(0, 3), 3.0);
    QCOMPARE(source->zAt(1, 0), 10.0);
    QCOMPARE(source->zAt(2, 3), z.back());
    QCOMPARE(map->keyAxis().get(), ar->xAxis().get());

    // The options went through the map's setters, which forward to the rect's shared colour scale,
    // and the range was fitted on the data given.
    const auto scale = ar->colorScale();
    QVERIFY(scale);
    QCOMPARE(map->colorScale().get(), scale.get());
    QCOMPARE(scale->gradient(), QCPColorGradient(QCPColorGradient::gpJet));
    QCOMPARE(scale->dataScale(), QCPScale::symlog(2));
    QCOMPARE(scale->dataRange(), QCPRange(0, 23));
    QCOMPARE(map->gradient(), QCPColorGradient(QCPColorGradient::gpJet));
    QCOMPARE(map->dataScale(), QCPScale::symlog(2));
    QCOMPARE(map->dataRange(), QCPRange(0, 23));
}

void TestQCPAxisRect::colorMapFromColumns()
{
    QCPAxisRect *ar = mPlot->axisRect();
    const std::vector<double> x { 0, 1, 2 };
    const std::vector<double> y { 10, 20, 30, 40 };
    // One column of z per x value: columns[i][j] is z at x[i], y[j].
    std::vector<std::vector<double>> columns(3, std::vector<double>(4));
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 4; ++j)
            columns[i][j] = 10.0 * i + j;

    const QCPHandle<QCPColorMap2> map = ar->colorMap(x, y, columns);
    QVERIFY(map);

    // The columns flatten x-major: the same source values as the flat form.
    const QCPDataSource2D *source = map->dataSource();
    QCOMPARE(source->xSize(), 3);
    QCOMPARE(source->ySize(), 4);
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 4; ++j)
            QCOMPARE(source->zAt(i, j), 10.0 * i + j);
}

void TestQCPAxisRect::raggedColumnsAreRefused()
{
    QCPAxisRect *ar = mPlot->axisRect();
    const std::vector<double> y { 10, 20 };

    gMisuses = 0;
    const auto previous = qcp::setMisuseHandler(recordMisuse);
    const QCPHandle<QCPColorMap2> ragged =
        ar->colorMap(std::vector<double> { 0, 1 }, y, std::vector<std::vector<double>> { { 1, 2 }, { 3 } });
    qcp::setMisuseHandler(previous);
    QVERIFY2(!ragged, "ragged columns make no map");
    QCOMPARE(gMisuses, 1); // one report says why
    QVERIFY2(gLastMisuse.contains("ragged"), gLastMisuse.constData());

    // Two whole columns for three x values: the count is what is wrong, not a column.
    gMisuses = 0;
    const auto again = qcp::setMisuseHandler(recordMisuse);
    const QCPHandle<QCPColorMap2> tooFew = ar->colorMap(
        std::vector<double> { 0, 1, 2 }, y, std::vector<std::vector<double>> { { 1, 2 }, { 3, 4 } });
    qcp::setMisuseHandler(again);
    QVERIFY2(!tooFew, "one column per x value, or no map");
    QCOMPARE(gMisuses, 1);
    QVERIFY2(gLastMisuse.contains("one per x value"), gLastMisuse.constData());
}

void TestQCPAxisRect::colorMapWithoutColorBar()
{
    QCPAxisRect *ar = mPlot->axisRect();
    const QCPHandle<QCPColorMap2> map =
        ar->colorMap(std::vector<double> { 0, 1 }, std::vector<double> { 0, 1 },
                     std::vector<double> { 1, 2, 3, 4 }, { .colorBar = false });
    QVERIFY(map);

    // Joined to no colour scale, and its own range still fits the data.
    QVERIFY(!map->colorScale());
    QCOMPARE(map->dataRange(), QCPRange(1, 4));

    // The rect's bar is the one the add made; nothing uses it, so it takes no room.
    const auto scale = ar->colorScale();
    QVERIFY(scale);
    mPlot->replot();
    QVERIFY2(!scale->visible(), "the bar of an unused colour scale stays hidden");
}

void TestQCPAxisRect::colorMapFromAView()
{
    QCPAxisRect *ar = mPlot->axisRect();
    // A view goes in like a container: one pass over it fills the map's x column.
    const auto keys = std::views::iota(0, 3);
    const QCPHandle<QCPColorMap2> map = ar->colorMap(keys, std::vector<double> { 10, 20, 30, 40 },
                                                     gridValues());
    QVERIFY(map);
    const QCPDataSource2D *source = map->dataSource();
    QCOMPARE(source->xSize(), 3);
    QCOMPARE(source->xAt(2), 2.0);
    QCOMPARE(source->zAt(2, 3), 23.0);
}

void TestQCPAxisRect::spectrogramSingleRect()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    QFont font = qcp::test::referenceFont();
    font.setPixelSize(10);
    plot.setFont(font);
    for (QCPAxis *axis : plot.axisRect()->axes())
    {
        axis->setTickLabelFont(font);
        axis->setLabelFont(font);
    }
    drawSpectrogram(plot);
    plot.replot();
    QVERIFY2(waitUntilIdle(plot), "the colour map did not finish its first picture in 10 s");
    plot.replot();
    const QImage image
        = plot.toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);

    QString why;
    QVERIFY2(qcp::test::matchesReference(image, QStringLiteral("test-qcpaxisrect"),
                                         QStringLiteral("spectrogram-single-rect.png"), &why),
             qPrintable(why));
}

void TestQCPAxisRect::flatDataOfTheWrongSizeIsRefused()
{
    QCPAxisRect *ar = mPlot->axisRect();
    const std::vector<double> x { 0, 1 };
    const std::vector<double> y { 10, 20 };

    // Five values for a 2 x 2 grid. The map alone would drop them with a bare warning and stay
    // empty, so the shortcut refuses the shapes and says why.
    gMisuses = 0;
    const auto previous = qcp::setMisuseHandler(recordMisuse);
    const QCPHandle<QCPColorMap2> mismatched
        = ar->colorMap(x, y, std::vector<double> { 1, 2, 3, 4, 5 });
    qcp::setMisuseHandler(previous);
    QVERIFY2(!mismatched, "a z that is not one value per grid point makes no map");
    QCOMPARE(gMisuses, 1);
    QVERIFY2(gLastMisuse.contains("one value per grid point"), gLastMisuse.constData());

    // A grid needs both axes: no x value or no y value is refused the same way.
    gMisuses = 0;
    const auto noKeysHandler = qcp::setMisuseHandler(recordMisuse);
    const QCPHandle<QCPColorMap2> noKeys
        = ar->colorMap(std::vector<double> { }, y, std::vector<double> { });
    qcp::setMisuseHandler(noKeysHandler);
    QVERIFY2(!noKeys, "no x values, no grid, no map");
    QCOMPARE(gMisuses, 1);
    QVERIFY2(gLastMisuse.contains("at least one value"), gLastMisuse.constData());

    gMisuses = 0;
    const auto noValuesHandler = qcp::setMisuseHandler(recordMisuse);
    const QCPHandle<QCPColorMap2> noValues
        = ar->colorMap(x, std::vector<double> { }, std::vector<double> { });
    qcp::setMisuseHandler(noValuesHandler);
    QVERIFY2(!noValues, "no y values, no grid, no map");
    QCOMPARE(gMisuses, 1);
    QVERIFY2(gLastMisuse.contains("at least one value"), gLastMisuse.constData());
}

void TestQCPAxisRect::columnsOfTheWrongLengthAreRefused()
{
    QCPAxisRect *ar = mPlot->axisRect();
    // Three whole columns of five for four y values: the columns agree with each other, so neither
    // the ragged check nor the count check has anything to say; the grid is the mismatch.
    const std::vector<std::vector<double>> columns(3, std::vector<double> { 1, 2, 3, 4, 5 });
    gMisuses = 0;
    const auto previous = qcp::setMisuseHandler(recordMisuse);
    const QCPHandle<QCPColorMap2> map = ar->colorMap(std::vector<double> { 0, 1, 2 },
                                                    std::vector<double> { 10, 20, 30, 40 }, columns);
    qcp::setMisuseHandler(previous);
    QVERIFY2(!map, "a column needs one value per y value");
    QCOMPARE(gMisuses, 1);
    QVERIFY2(gLastMisuse.contains("one value per y value"), gLastMisuse.constData());
}
