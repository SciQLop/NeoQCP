#include "test-tick-label-cache.h"
#include "theme.h"
#include "../keep-images.h"

#include <functional>

class LivePlot : public QCustomPlot
{
public:
    // What the widget would show: every paint buffer repainted, then composited.
    QImage live(bool cacheLabels)
    {
        setPlottingHint(QCP::phCacheLabels, cacheLabels);
        for (auto* layer : std::as_const(mLayers))
            layer->invalidatePaintBuffer();
        replot(rpImmediateRefresh);
        const double dpr = bufferDevicePixelRatio();
        QImage image(size() * dpr, QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(dpr);
        image.fill(Qt::white);
        QCPPainter painter(&image);
        for (const auto& buffer : std::as_const(mPaintBuffers))
            buffer->draw(&painter);
        painter.end();
        return image.convertToFormat(QImage::Format_ARGB32);
    }
};

namespace
{
using Step = std::function<void(LivePlot*)>;

void showAllAxes(LivePlot* plot)
{
    plot->resize(400, 300);
    for (auto* axis : {plot->xAxis2, plot->yAxis2})
    {
        axis->setVisible(true);
        axis->setTickLabels(true);
    }
    plot->xAxis->setRange(0, 10);
    plot->yAxis->setRange(-3.5, 1234.5);
    plot->xAxis2->setRange(1e-3, 7.25);
    plot->yAxis2->setRange(0.1, 0.9);
}

void forEachAxis(LivePlot* plot, const std::function<void(QCPAxis*)>& f)
{
    for (auto* axis : plot->axisRect()->axes())
        f(axis);
}

// Where two pictures differ (device pixels), to tell a stale label from a layout shift.
QString describeDifference(const QImage& a, const QImage& b)
{
    if (a.size() != b.size())
        return QString("sizes %1x%2 vs %3x%4").arg(a.width()).arg(a.height()).arg(b.width()).arg(b.height());
    QRect where;
    int count = 0;
    for (int y = 0; y < a.height(); ++y)
        for (int x = 0; x < a.width(); ++x)
            if (a.pixel(x, y) != b.pixel(x, y))
            {
                ++count;
                where |= QRect(x, y, 1, 1);
            }
    return QString("%1 pixels differ in %2,%3 %4x%5 of %6x%7")
        .arg(count).arg(where.x()).arg(where.y()).arg(where.width()).arg(where.height())
        .arg(a.width()).arg(a.height());
}

/*
  A plot whose cache saw the state before \a change and one that never did must show the
  same picture, to the bit: both draw every label from a cached pixmap, so any difference
  is a label the cache failed to forget.
*/
void checkNoStaleLabel(const Step& before, const Step& change, const QString& what)
{
    LivePlot warm;
    showAllAxes(&warm);
    before(&warm);
    warm.live(true);
    change(&warm);
    const QImage fromWarmCache = warm.live(true);

    LivePlot cold;
    showAllAxes(&cold);
    before(&cold);
    change(&cold);
    const QImage fromColdCache = cold.live(true);

    if (fromWarmCache != fromColdCache)
        keepImagesForCi("stale-" + what, fromWarmCache, fromColdCache);
    QVERIFY2(fromWarmCache == fromColdCache,
             qPrintable(what + ": stale cached tick label, " + describeDifference(fromWarmCache, fromColdCache)));
}

struct Ink
{
    QRect bounds;
    double mass = 0.;
};

// Ink in the margins around the axis rect (device pixels), where the tick labels are.
Ink marginInk(const QImage& image, LivePlot* plot)
{
    const double dpr = plot->bufferDevicePixelRatio();
    const QRect r = plot->axisRect()->rect().adjusted(-2, -2, 3, 3);
    const QRect inner(r.topLeft() * dpr, r.size() * dpr);
    const QRgb background = image.pixel(0, 0);
    Ink ink;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
        {
            if (inner.contains(x, y))
                continue;
            const QRgb c = image.pixel(x, y);
            const int d = std::max({qAbs(qRed(c) - qRed(background)),
                                    qAbs(qGreen(c) - qGreen(background)),
                                    qAbs(qBlue(c) - qBlue(background))});
            ink.mass += d;
            if (d > 96)
                ink.bounds |= QRect(x, y, 1, 1);
        }
    return ink;
}

bool within(int a, int b, int tolerance) { return qAbs(a - b) <= tolerance; }

/*
  Against drawText: a cached label is rasterised once, so its glyphs sit at another sub-pixel
  phase than drawText would give them at the final place (and it is put on the nearest device
  pixel so it stays sharp). Edges differ pixel by pixel; where the labels are and how much ink
  they carry must not.
*/
void checkCloseToDirect(LivePlot* plot, const QString& what)
{
    plot->live(true);
    const QImage direct = plot->live(false), cached = plot->live(true);
    QCOMPARE(cached.size(), direct.size());
    const Ink d = marginInk(direct, plot), c = marginInk(cached, plot);
    const int onePixel = int(std::ceil(plot->bufferDevicePixelRatio()));
    const bool sameBounds = within(c.bounds.left(), d.bounds.left(), onePixel)
        && within(c.bounds.right(), d.bounds.right(), onePixel)
        && within(c.bounds.top(), d.bounds.top(), onePixel)
        && within(c.bounds.bottom(), d.bounds.bottom(), onePixel);
    if (!sameBounds || qAbs(c.mass - d.mass) > 0.05 * d.mass)
        keepImagesForCi("direct-" + what, cached, direct);
    QVERIFY2(within(c.bounds.left(), d.bounds.left(), onePixel)
                 && within(c.bounds.right(), d.bounds.right(), onePixel)
                 && within(c.bounds.top(), d.bounds.top(), onePixel)
                 && within(c.bounds.bottom(), d.bounds.bottom(), onePixel),
             qPrintable(what
                        + QString(": labels moved or resized, %1,%2 %3x%4 vs %5,%6 %7x%8")
                              .arg(c.bounds.x()).arg(c.bounds.y())
                              .arg(c.bounds.width()).arg(c.bounds.height())
                              .arg(d.bounds.x()).arg(d.bounds.y())
                              .arg(d.bounds.width()).arg(d.bounds.height())));
    QVERIFY2(qAbs(c.mass - d.mass) <= 0.05 * d.mass,
             qPrintable(what + QString(": ink %1 vs %2").arg(c.mass).arg(d.mass)));
}

const Step nothing = [](LivePlot*) {};
}

void TestTickLabelCache::init() { }

void TestTickLabelCache::cleanup() { }

void TestTickLabelCache::plainLabelsMatch()
{
    LivePlot plot;
    showAllAxes(&plot);
    checkCloseToDirect(&plot, "plain");
}

void TestTickLabelCache::fontChangeRefreshesLabels()
{
    const Step bigFont = [](LivePlot* p)
    { forEachAxis(p, [](QCPAxis* a) { a->setTickLabelFont(QFont("sans", 14, QFont::Bold)); }); };
    checkNoStaleLabel(nothing, bigFont, "font");
    LivePlot plot;
    showAllAxes(&plot);
    bigFont(&plot);
    checkCloseToDirect(&plot, "font");
}

void TestTickLabelCache::colourChangeRefreshesLabels()
{
    const Step red = [](LivePlot* p)
    { forEachAxis(p, [](QCPAxis* a) { a->setTickLabelColor(Qt::red); }); };
    checkNoStaleLabel(nothing, red, "colour");
}

void TestTickLabelCache::themeSwitchRefreshesLabels()
{
    const Step light = [](LivePlot* p) { p->setTheme(QCPTheme::light(p)); };
    const Step dark = [](LivePlot* p) { p->setTheme(QCPTheme::dark(p)); };
    checkNoStaleLabel(light, dark, "light -> dark");
    checkNoStaleLabel(dark, light, "dark -> light");
    LivePlot plot;
    showAllAxes(&plot);
    dark(&plot);
    checkCloseToDirect(&plot, "dark");
}

void TestTickLabelCache::bufferDprChangeKeepsLabelSize()
{
    // Moving the window to another screen changes the buffer ratio (QCustomPlot::render):
    // labels must be rasterised again at the new ratio and keep their logical size.
    for (double dpr : {2.0, 1.5})
    {
        const Step toDpr = [dpr](LivePlot* p) { p->setBufferDevicePixelRatio(dpr); };
        const Step backToOne = [](LivePlot* p) { p->setBufferDevicePixelRatio(1.0); };
        checkNoStaleLabel(nothing, toDpr, QString("dpr 1 -> %1").arg(dpr));
        checkNoStaleLabel(toDpr, backToOne, QString("dpr %1 -> 1").arg(dpr));
        LivePlot plot;
        showAllAxes(&plot);
        toDpr(&plot);
        checkCloseToDirect(&plot, QString("dpr %1").arg(dpr));
    }
}

void TestTickLabelCache::selectedTickLabelsUseSelectedFontAndColour()
{
    const Step styleSelection = [](LivePlot* p)
    {
        forEachAxis(p,
                    [](QCPAxis* a)
                    {
                        a->setSelectedTickLabelFont(QFont("serif", 13, QFont::Bold));
                        a->setSelectedTickLabelColor(Qt::blue);
                    });
    };
    const Step select = [](LivePlot* p)
    { forEachAxis(p, [](QCPAxis* a) { a->setSelectedParts(QCPAxis::spTickLabels); }); };
    const Step selectThenDeselect = [select](LivePlot* p)
    {
        select(p);
        p->live(true);
        forEachAxis(p, [](QCPAxis* a) { a->setSelectedParts(QCPAxis::spNone); });
    };
    checkNoStaleLabel(styleSelection, select, "select");
    checkNoStaleLabel(styleSelection, selectThenDeselect, "deselect");
}

void TestTickLabelCache::rotatedLabelsMatch()
{
    for (double angle : {45.0, -90.0, 90.0, 30.0, -60.0})
    {
        const Step rotate = [angle](LivePlot* p)
        { forEachAxis(p, [angle](QCPAxis* a) { a->setTickLabelRotation(angle); }); };
        checkNoStaleLabel(nothing, rotate, QString("rotation %1").arg(angle));
        LivePlot plot;
        showAllAxes(&plot);
        rotate(&plot);
        checkCloseToDirect(&plot, QString("rotation %1").arg(angle));
    }
}

void TestTickLabelCache::multiLineLabelsMatch()
{
    LivePlot plot;
    showAllAxes(&plot);
    auto ticker = QSharedPointer<QCPAxisTickerText>::create();
    ticker->addTick(2, "first\nline two");
    ticker->addTick(5, "a\nlonger second line");
    ticker->addTick(8, "x");
    plot.xAxis->setTicker(ticker);
    checkCloseToDirect(&plot, "multi-line");
}

void TestTickLabelCache::dateTimePanMatches()
{
    // A pan keeps most label texts and moves them: the cached pictures must follow.
    const double start = 1744430695.491489, span = 4 * 3600.;
    const Step dateAxis = [=](LivePlot* p)
    {
        auto ticker = QSharedPointer<QCPAxisTickerDateTime>::create();
        ticker->setDateTimeFormat("hh:mm:ss\nyyyy-MM-dd");
        ticker->setDateTimeSpec(Qt::UTC);
        p->xAxis->setTicker(ticker);
        p->xAxis->setRange(start, start + span);
    };
    const Step pan = [=](LivePlot* p)
    {
        for (int step = 1; step <= 8; ++step)
        {
            p->xAxis->setRange(start + step * span * 0.037, start + span + step * span * 0.037);
            p->live(true);
        }
    };
    checkNoStaleLabel(dateAxis, pan, "date-time pan");
    LivePlot plot;
    showAllAxes(&plot);
    dateAxis(&plot);
    pan(&plot);
    checkCloseToDirect(&plot, "date-time panned");
}

void TestTickLabelCache::logAxisBeautifulPowersMatch()
{
    const Step logAxis = [](LivePlot* p)
    {
        p->yAxis->setScaleType(QCPAxis::stLogarithmic);
        p->yAxis->setTicker(QSharedPointer<QCPAxisTickerLog>::create());
        p->yAxis->setNumberFormat("eb");
        p->yAxis->setNumberPrecision(0);
        p->yAxis->setRange(1e-3, 1e8);
    };
    const Step crossSign = [](LivePlot* p) { p->yAxis->setNumberFormat("ebc"); };
    checkNoStaleLabel(logAxis, crossSign, "eb -> ebc");
    LivePlot plot;
    showAllAxes(&plot);
    logAxis(&plot);
    checkCloseToDirect(&plot, "log eb");
}

void TestTickLabelCache::linearToLogSwitchRefreshesPowers()
{
    // "1e+03" reads 1·10³ on a linear axis and 10³ on a log one: same text, other picture.
    const Step linear = [](LivePlot* p)
    {
        auto fixed = QSharedPointer<QCPAxisTickerFixed>::create();
        fixed->setTickStep(1000);
        p->yAxis->setTicker(fixed);
        p->yAxis->setNumberFormat("eb");
        p->yAxis->setNumberPrecision(0);
        p->yAxis->setRange(500, 3500);
    };
    const Step toLog = [](LivePlot* p)
    {
        p->yAxis->setScaleType(QCPAxis::stLogarithmic);
        p->yAxis->setTicker(QSharedPointer<QCPAxisTickerLog>::create());
        p->yAxis->setRange(500, 3500);
    };
    checkNoStaleLabel(linear, toLog, "linear -> log");
}

void TestTickLabelCache::manyLabelsMatch()
{
    // More labels than the cache holds per axis.
    const Step many = [](LivePlot* p)
    {
        p->resize(400, 900);
        auto fixed = QSharedPointer<QCPAxisTickerFixed>::create();
        fixed->setTickStep(25);
        p->yAxis->setTicker(fixed);
        p->yAxis->setRange(0, 1000);
    };
    const Step pan = [](LivePlot* p) { p->yAxis->setRange(3, 1003); };
    checkNoStaleLabel(many, pan, "40 labels panned");
    LivePlot plot;
    showAllAxes(&plot);
    many(&plot);
    checkCloseToDirect(&plot, "40 labels");
}

void TestTickLabelCache::pdfExportKeepsVectorText()
{
    // Exports paint with pmNoCaching: text stays text, no label pixmaps in the PDF.
    LivePlot plot;
    showAllAxes(&plot);
    QTemporaryDir dir;
    auto pdf = [&](bool cache)
    {
        plot.live(cache);
        const QString path = dir.filePath(cache ? "cached.pdf" : "direct.pdf");
        QFile file(path);
        if (!plot.savePdf(path, 400, 300) || !file.open(QIODevice::ReadOnly))
            return QByteArray();
        return file.readAll();
    };
    const QByteArray direct = pdf(false), cached = pdf(true);
    QVERIFY(direct.size() > 1000);
    QCOMPARE(cached.count("/Subtype /Image"), direct.count("/Subtype /Image"));
    QCOMPARE(cached.count("/Font"), direct.count("/Font"));
}
