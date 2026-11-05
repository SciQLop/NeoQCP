#include "test-repaint.h"
#include "axis/labelrenderer.h"
#include "datasource/soa-multi-datasource.h"
#include "items/item-bracket.h"
#include "items/item-curve.h"
#include "items/item-ellipse.h"
#include "items/item-line.h"
#include "items/item-pixmap.h"
#include "items/item-rect.h"
#include "items/item-richtext.h"
#include "items/item-text.h"
#include "items/item-tracer.h"
#include "items/item-vspan.h"
#include "layoutelements/layoutelement-colorscale.h"
#include "layoutelements/layoutelement-legend.h"
#include "layoutelements/layoutelement-textelement.h"
#include "painting/command-replay-rhi.h"
#include "painting/draw-context.h"
#include "painting/plottable-rhi-layer.h"
#include "painting/raster-fallback.h"
#include "plottables/plottable-bars.h"
#include "plottables/plottable-colormap.h"
#include "plottables/plottable-colormap2.h"
#include "plottables/plottable-curve.h"
#include "plottables/plottable-errorbar.h"
#include "plottables/plottable-financial.h"
#include "plottables/plottable-graph2.h"
#include "plottables/plottable-group.h"
#include "plottables/plottable-histogram2d.h"
#include "plottables/plottable-multigraph.h"
#include "plottables/plottable-statisticalbox.h"
#include "plottables/plottable-waterfall.h"
#include "polar/layoutelement-angularaxis.h"
#include "polar/polargrid.h"
#include <QTemporaryDir>
#include <QtWidgets/qtestsupport_widgets.h>
#include <algorithm>
#include <drawable.h>
#include <functional>
#include <memory>

namespace
{
std::unique_ptr<QCustomPlot> settledPlot()
{
    auto plot = std::make_unique<QCustomPlot>();
    plot->resize(400, 300);
    plot->addGraph()->setData(QVector<double> { 0, 10 }, QVector<double> { 0, 10 });
    plot->graph(0)->setPen(QPen(Qt::red, 4));
    plot->xAxis->setRange(0, 10);
    plot->yAxis->setRange(0, 10);
    plot->replot();
    for (int i = 0; i < 10 && plot->isRepaintQueued(); ++i)
        QCoreApplication::processEvents();
    return plot;
}

class DrawHook : public QCPLayerable
{
public:
    using QCPLayerable::QCPLayerable;
    std::function<void()> onDraw;
    std::function<void(QCPPainter*)> onPaint;

protected:
    void applyDefaultAntialiasingHint(QCPPainter*) const override { }

    void draw(QCPPainter* painter) override
    {
        if (onPaint)
            onPaint(painter);
        if (onDraw)
            onDraw();
    }
};

// Pending data that counts commits, like a pipeline result waiting for its swap window.
class PendingStub : public QCPAbstractPlottable
{
public:
    PendingStub(QCPAxis* k, QCPAxis* v) : QCPAbstractPlottable(k, v) { }

    int commits = 0;

    bool hasPendingData() const override { return commits == 0; }

    bool commitPendingData() override
    {
        ++commits;
        return true;
    }

    double selectTest(const QPointF&, bool, QVariant* = nullptr) const override { return -1; }

    QCPRange getKeyRange(bool& found, QCP::SignDomain = QCP::sdBoth) const override
    {
        found = false;
        return { };
    }

    QCPRange getValueRange(bool& found, QCP::SignDomain = QCP::sdBoth,
                           const QCPRange& = QCPRange()) const override
    {
        found = false;
        return { };
    }

    void draw(QCPPainter*) override { }

    void drawLegendIcon(QCPPainter*, const QRectF&) const override { }
};

// Busy, like a pipeline that has not delivered yet: the plot keeps its layer's last frame.
class NoContentStub : public PendingStub
{
public:
    using PendingStub::PendingStub;

    bool canProduceContent() const override { return false; }
};

// Fills its axis rect with its pen colour. A non-null stall makes its layer translate on replot.
class FillStub : public PendingStub
{
public:
    using PendingStub::PendingStub;
    QPointF stall;

    bool hasPendingData() const override { return false; }

    QPointF stallPixelOffset() const override { return stall; }

    void draw(QCPPainter* painter) override
    {
        painter->fillRect(mKeyAxis->axisRect()->rect(), pen().color());
    }
};

const QColor kBlue(0, 0, 255);

void waitUntilSettled(QCustomPlot& plot)
{
    QTRY_VERIFY(!plot.isRepaintQueued());
    QCoreApplication::processEvents();
}

int pixelsOfColor(const QImage& image, const QColor& color)
{
    int count = 0;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
        {
            const QColor c = image.pixelColor(x, y);
            if (qAbs(c.red() - color.red()) < 40 && qAbs(c.green() - color.green()) < 40
                && qAbs(c.blue() - color.blue()) < 40)
                ++count;
        }
    return count;
}

// Returns the exported image, or a null image for a vector export.
QImage exportWith(QCustomPlot& plot, const QString& how, const QString& dir)
{
    if (how == QLatin1String("toPixmap"))
        return plot.toPixmap(400, 300).toImage();
    if (how == QLatin1String("savePng"))
    {
        const QString file = dir + QStringLiteral("/plot.png");
        return plot.savePng(file, 400, 300) ? QImage(file) : QImage();
    }
    if (how == QLatin1String("toPainter"))
    {
        QPixmap pixmap(400, 300);
        pixmap.fill(Qt::white);
        QCPPainter painter(&pixmap);
        plot.toPainter(&painter, 400, 300);
        painter.end();
        return pixmap.toImage();
    }
    [[maybe_unused]] const bool saved = plot.savePdf(dir + QStringLiteral("/plot.pdf"), 400, 300);
    return { }; // the caller checks the file
}

int gMisuses = 0;

void countMisuse(const char*, const char*)
{
    ++gMisuses;
}
} // namespace

void TestRepaint::markDirtySchedulesOneRepaint()
{
    auto plot = settledPlot();
    QSignalSpy after(plot.get(), &QCustomPlot::afterReplot);
    for (int i = 0; i < 1000; ++i)
        plot->graph(0)->markDirty();
    QVERIFY(plot->isRepaintQueued());
    QCOMPARE(after.count(), 0);
    QTRY_COMPARE(after.count(), 1);
    QCoreApplication::processEvents();
    QCOMPARE(after.count(), 1);
    QVERIFY(!plot->isRepaintQueued());
}

void TestRepaint::dirtyAfterDrawRepaintsNextTurn()
{
    auto plot = settledPlot();
    int count = 0;
    QList<bool> replottingAtEntry;
    connect(plot.get(), &QCustomPlot::afterReplot, plot.get(),
            [&]
            {
                replottingAtEntry << plot->isReplotting();
                if (++count == 1)
                {
                    plot->graph(0)->setPen(QPen(kBlue, 4));
                    plot->graph(0)->markDirty();
                }
            });
    plot->replot();
    QCOMPARE(count, 1);
    QVERIFY(plot->isRepaintQueued());
    QTRY_COMPARE(count, 2);
    for (int i = 0; i < 3; ++i)
        QCoreApplication::processEvents();
    QCOMPARE(count, 2);
    QCOMPARE(replottingAtEntry, (QList<bool> { false, false }));
    const QImage screen = screenOf(plot.get());
    QVERIFY(pixelsOfColor(screen, kBlue) > 50);
    QCOMPARE(pixelsOfColor(screen, Qt::red), 0);
}

void TestRepaint::markDuringDrawRepaintsNextTurn()
{
    auto plot = settledPlot();
    auto* hook = new DrawHook(plot.get(), QStringLiteral("overlay"));
    int draws = 0;
    hook->onDraw = [&]
    {
        if (++draws > 1)
            return;
        plot->graph(0)->setPen(QPen(kBlue, 4));
        plot->graph(0)->markDirty(); // "main" is drawn before "overlay"
    };
    QSignalSpy after(plot.get(), &QCustomPlot::afterReplot);
    plot->replot();
    QCOMPARE(after.count(), 1);
    QVERIFY(plot->isRepaintQueued());
    QTRY_COMPARE(after.count(), 2);
    for (int i = 0; i < 3; ++i)
        QCoreApplication::processEvents();
    QCOMPARE(after.count(), 2);
    const QImage screen = screenOf(plot.get());
    QVERIFY(pixelsOfColor(screen, kBlue) > 50);
    QCOMPARE(pixelsOfColor(screen, Qt::red), 0);
}

void TestRepaint::markAfterDrawSurvivesAnotherDirtyBuffer()
{
    auto plot = settledPlot();
    auto* hook = new DrawHook(plot.get(), QStringLiteral("overlay"));
    int draws = 0;
    hook->onDraw = [&]
    {
        if (++draws > 1)
            return;
        plot->graph(0)->setPen(QPen(kBlue, 4));
        plot->graph(0)->markDirty();
    };
    int replots = 0;
    connect(plot.get(), &QCustomPlot::afterReplot, plot.get(),
            [&]
            {
                if (++replots == 1)
                    hook->markDirty();
            });
    plot->replot();
    waitUntilSettled(*plot);
    const QImage screen = screenOf(plot.get());
    QVERIFY(pixelsOfColor(screen, kBlue) > 50);
    QCOMPARE(pixelsOfColor(screen, Qt::red), 0);
}

void TestRepaint::appearanceChangeIsNotHiddenByTranslation()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    auto* fill = new FillStub(plot.xAxis, plot.yAxis);
    fill->setPen(QPen(Qt::red));
    plot.replot();
    fill->stall = QPointF(5, 0); // a pan in progress: the layer would shift its old frame
    fill->setPen(QPen(kBlue));
    fill->markDirty();
    waitUntilSettled(plot);
    const QImage screen = screenOf(&plot);
    QVERIFY(pixelsOfColor(screen, kBlue) > 1000);
    QCOMPARE(pixelsOfColor(screen, Qt::red), 0);
}

void TestRepaint::requestDuringReplotRunsNextTurn()
{
    auto plot = settledPlot();
    auto* hook = new DrawHook(plot.get(), QStringLiteral("overlay"));
    int draws = 0;
    hook->onDraw = [&]
    {
        if (++draws == 1)
            plot->requestRepaint();
    };
    QSignalSpy after(plot.get(), &QCustomPlot::afterReplot);
    plot->replot();
    QVERIFY(plot->isRepaintQueued());
    QTRY_COMPARE(after.count(), 2);
    for (int i = 0; i < 3; ++i)
        QCoreApplication::processEvents();
    QCOMPARE(after.count(), 2);
}

void TestRepaint::afterReplotSlotDirtyingEveryTimeDoesNotStarve()
{
    auto plot = settledPlot();
    int count = 0;
    connect(plot.get(), &QCustomPlot::afterReplot, plot.get(),
            [&]
            {
                ++count;
                plot->graph(0)->markDirty();
            });
    bool timerFired = false;
    QTimer::singleShot(0, plot.get(), [&] { timerFired = true; });
    plot->replot();
    QCOMPARE(count, 1);
    for (int i = 0; i < 5; ++i)
    {
        const int before = count;
        QCoreApplication::processEvents();
        QVERIFY2(count - before <= 1, qPrintable(QString::number(count - before)));
    }
    QVERIFY(timerFired);
    QVERIFY(count > 1);
    QVERIFY(count <= 6);
}

void TestRepaint::hiddenPlotQueuesNoVisibleWork()
{
    auto plot = settledPlot();
    plot->setSkipReplotsWhenHidden(true);
    plot->show();
    QVERIFY(QTest::qWaitForWindowExposed(plot.get()));
    plot->hide();
    for (int i = 0; i < 10 && plot->isRepaintQueued(); ++i)
        QCoreApplication::processEvents();
    QSignalSpy after(plot.get(), &QCustomPlot::afterReplot);
    plot->graph(0)->setPen(QPen(kBlue, 4));
    plot->graph(0)->markDirty();
    QCoreApplication::processEvents();
    QCOMPARE(after.count(), 0);
    QVERIFY(!plot->isRepaintQueued());
    plot->show();
    QTRY_VERIFY(after.count() > 0);
    waitUntilSettled(*plot);
    const QImage screen = screenOf(plot.get());
    QVERIFY(pixelsOfColor(screen, kBlue) > 50);
    QCOMPARE(pixelsOfColor(screen, Qt::red), 0);
}

void TestRepaint::markDirtyWithoutPlotIsNoOp()
{
    const auto previous = qcp::setMisuseHandler(countMisuse);
    gMisuses = 0;
    DrawHook orphan(nullptr);
    orphan.markDirty();
    auto plot = settledPlot();
    auto* layerless = new DrawHook(plot.get());
    layerless->setLayer(static_cast<QCPLayer*>(nullptr));
    waitUntilSettled(*plot); // joining and leaving a layer repaints it
    layerless->markDirty();
    QVERIFY(!plot->isRepaintQueued());
    QCOMPARE(gMisuses, 0);
    qcp::setMisuseHandler(previous);
}

void TestRepaint::layerKeptWhileBusyIsNotRequeued()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    new NoContentStub(plot.xAxis, plot.yAxis);
    plot.replot();
    for (int i = 0; i < 3; ++i)
    {
        plot.replot();
        QVERIFY(!plot.isRepaintQueued());
    }
}

void TestRepaint::syncReplotStillWorks()
{
    auto plot = settledPlot();
    QSignalSpy after(plot.get(), &QCustomPlot::afterReplot);
    plot->replot();
    QCOMPARE(after.count(), 1);
}

void TestRepaint::syncReplotCancelsTheQueuedOne()
{
    auto plot = settledPlot();
    QSignalSpy after(plot.get(), &QCustomPlot::afterReplot);
    plot->graph(0)->markDirty();
    plot->replot();
    QCOMPARE(after.count(), 1);
    QVERIFY(!plot->isRepaintQueued());
    for (int i = 0; i < 3; ++i)
        QCoreApplication::processEvents();
    QCOMPARE(after.count(), 1);
}

void TestRepaint::exportSettlesFirst_data()
{
    QTest::addColumn<QString>("how");
    QTest::newRow("toPixmap") << QStringLiteral("toPixmap");
    QTest::newRow("savePng") << QStringLiteral("savePng");
    QTest::newRow("savePdf") << QStringLiteral("savePdf");
    QTest::newRow("toPainter") << QStringLiteral("toPainter");
}

void TestRepaint::exportSettlesFirst()
{
    QFETCH(QString, how);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto plot = settledPlot();
    auto* pending = new PendingStub(plot->xAxis, plot->yAxis);
    plot->setDataSwapDebounceMs(10000);
    plot->requestDataSwap();
    const QColor newColor(0, 0, 255);
    plot->graph(0)->setPen(QPen(newColor, 4));
    plot->graph(0)->markDirty();

    const QImage image = exportWith(*plot, how, dir.path());

    QCOMPARE(pending->commits, 1);
    QVERIFY(plot->isRepaintQueued()); // the screen catches up next turn
    if (how == QLatin1String("savePdf"))
        QVERIFY(QFileInfo(dir.path() + QStringLiteral("/plot.pdf")).size() > 0);
    else
    {
        QVERIFY(!image.isNull());
        QVERIFY(pixelsOfColor(image, newColor) > 50);
        QCOMPARE(pixelsOfColor(image, Qt::red), 0);
    }
}

void TestRepaint::exportShowsReadyStagedData()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    auto* mg = new QCPMultiGraph(plot.xAxis, plot.yAxis);
    mg->setData(std::vector<double> { 0, 10 }, std::vector<std::vector<double>> { { 2, 2 } });
    mg->setComponentPens({ QPen(Qt::red, 4) });
    plot.xAxis->setRange(0, 10);
    plot.yAxis->setRange(0, 10);
    plot.replot();
    plot.setDataSwapDebounceMs(10000);
    using Source = QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>;
    mg->setDataSource(std::make_shared<Source>(std::vector<double> { 0, 10 },
                                               std::vector<std::vector<double>> { { 8, 8 } }));
    QVERIFY(mg->hasPendingData());

    const QImage image = plot.toPixmap(400, 300).toImage();

    QVERIFY(!mg->hasPendingData());
    QVERIFY(plot.isRepaintQueued());
    const int x = qRound(plot.xAxis->coordToPixel(5));
    const auto redNear = [&](double value)
    {
        const int y = qRound(plot.yAxis->coordToPixel(value));
        int red = 0;
        for (int dy = -2; dy <= 2; ++dy)
            red += pixelsOfColor(image.copy(x, y + dy, 1, 1), Qt::red);
        return red;
    };
    QVERIFY(redNear(8) > 0);
    QCOMPARE(redNear(2), 0);
}

// The legend is on its own buffer ("axes" and "legend"): marking the graph's layer does not redraw
// it.
void TestRepaint::legendShowsAddedGraphAndItsName()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.legend->setVisible(true);
    plot.xAxis->setRange(0, 10);
    plot.yAxis->setRange(0, 10);
    plot.replot();
    waitUntilSettled(plot);
    const QImage empty = screenOf(&plot);

    auto g = plot.add<QCPGraph>(QVector<double> { 0, 1 }, QVector<double> { 0, 1 }).setName("Bx");
    waitUntilSettled(plot);
    const QImage oneRow = screenOf(&plot);
    const QRect rowArea = plot.legend->outerRect();
    QVERIFY(oneRow.copy(rowArea) != empty.copy(rowArea));

    g.setName("Bx2, a longer name");
    waitUntilSettled(plot);
    const QRect renamedArea = plot.legend->outerRect();
    QVERIFY(screenOf(&plot).copy(renamedArea) != oneRow.copy(renamedArea));
}

void TestRepaint::setLayerRepaintsTheLayerLeft()
{
    auto plot = settledPlot();
    plot->layer(QStringLiteral("overlay"))->setVisible(false);
    plot->replot();
    waitUntilSettled(*plot);
    QVERIFY(pixelsOfColor(screenOf(plot.get()), Qt::red) > 50);

    QVERIFY(plot->graph(0)->setLayer(QStringLiteral("overlay")));
    QVERIFY(plot->isRepaintQueued());
    waitUntilSettled(*plot);
    QCOMPARE(pixelsOfColor(screenOf(plot.get()), Qt::red), 0);
}

namespace
{
struct AxisAppearanceCase
{
    const char* name;
    std::function<void(QCustomPlot&)> change;
};

// One entry per "yes" row of docs/plans/neoqcp-3/milestone-2-setters.md, section "axes".
const QVector<AxisAppearanceCase>& axisAppearanceCases()
{
    static const QVector<AxisAppearanceCase> cases = {
        { "grid.setSubGridVisible",
          [](QCustomPlot& plot) { plot.xAxis->grid()->setSubGridVisible(true); } },
        { "grid.setAntialiasedSubGrid",
          [](QCustomPlot& plot) { plot.xAxis->grid()->setAntialiasedSubGrid(true); } },
        { "grid.setAntialiasedZeroLine",
          [](QCustomPlot& plot) { plot.xAxis->grid()->setAntialiasedZeroLine(true); } },
        { "grid.setPen",
          [](QCustomPlot& plot) { plot.xAxis->grid()->setPen(QPen(Qt::darkGreen)); } },
        { "grid.setSubGridPen",
          [](QCustomPlot& plot) { plot.xAxis->grid()->setSubGridPen(QPen(Qt::darkCyan)); } },
        { "grid.setZeroLinePen",
          [](QCustomPlot& plot) { plot.xAxis->grid()->setZeroLinePen(QPen(Qt::darkMagenta)); } },
        { "axis.setScaleType",
          [](QCustomPlot& plot) { plot.xAxis->setScaleType(QCPAxis::stLogarithmic); } },
        { "axis.setSelectedParts",
          [](QCustomPlot& plot) { plot.xAxis->setSelectedParts(QCPAxis::spAxis); } },
        { "axis.setTicker", [](QCustomPlot& plot)
          { plot.xAxis->setTicker(QSharedPointer<QCPAxisTickerFixed>::create()); } },
        { "axis.setTicks", [](QCustomPlot& plot) { plot.xAxis->setTicks(false); } },
        { "axis.setTickLabels", [](QCustomPlot& plot) { plot.xAxis->setTickLabels(false); } },
        { "axis.setTickLabelPadding", [](QCustomPlot& plot)
          { plot.xAxis->setTickLabelPadding(plot.xAxis->tickLabelPadding() + 3); } },
        { "axis.setTickLabelFont",
          [](QCustomPlot& plot)
          {
              QFont font = plot.xAxis->tickLabelFont();
              font.setPointSizeF(font.pointSizeF() + 2);
              plot.xAxis->setTickLabelFont(font);
          } },
        { "axis.setTickLabelColor",
          [](QCustomPlot& plot) { plot.xAxis->setTickLabelColor(Qt::blue); } },
        { "axis.setTickLabelRotation",
          [](QCustomPlot& plot) { plot.xAxis->setTickLabelRotation(35); } },
        { "axis.setTickLabelSide",
          [](QCustomPlot& plot) { plot.xAxis->setTickLabelSide(QCPAxis::lsInside); } },
        { "axis.setNumberFormat",
          [](QCustomPlot& plot) { plot.xAxis->setNumberFormat(QStringLiteral("e")); } },
        { "axis.setNumberPrecision", [](QCustomPlot& plot)
          { plot.xAxis->setNumberPrecision(plot.xAxis->numberPrecision() + 2); } },
        { "axis.setTickLength", [](QCustomPlot& plot) { plot.xAxis->setTickLength(9); } },
        { "axis.setTickLengthIn", [](QCustomPlot& plot) { plot.xAxis->setTickLengthIn(7); } },
        { "axis.setTickLengthOut", [](QCustomPlot& plot) { plot.xAxis->setTickLengthOut(5); } },
        { "axis.setSubTicks", [](QCustomPlot& plot) { plot.xAxis->setSubTicks(false); } },
        { "axis.setSubTickLength", [](QCustomPlot& plot) { plot.xAxis->setSubTickLength(6); } },
        { "axis.setSubTickLengthIn", [](QCustomPlot& plot) { plot.xAxis->setSubTickLengthIn(5); } },
        { "axis.setSubTickLengthOut",
          [](QCustomPlot& plot) { plot.xAxis->setSubTickLengthOut(4); } },
        { "axis.setBasePen", [](QCustomPlot& plot) { plot.xAxis->setBasePen(QPen(Qt::red, 2)); } },
        { "axis.setTickPen", [](QCustomPlot& plot) { plot.xAxis->setTickPen(QPen(Qt::red, 2)); } },
        { "axis.setSubTickPen",
          [](QCustomPlot& plot) { plot.xAxis->setSubTickPen(QPen(Qt::red, 2)); } },
        { "axis.setLabelFont",
          [](QCustomPlot& plot)
          {
              QFont font = plot.xAxis->labelFont();
              font.setPointSizeF(font.pointSizeF() + 2);
              plot.xAxis->setLabelFont(font);
          } },
        { "axis.setLabelColor", [](QCustomPlot& plot) { plot.xAxis->setLabelColor(Qt::blue); } },
        { "axis.setLabel", [](QCustomPlot& plot) { plot.xAxis->setLabel(QStringLiteral("X")); } },
        { "axis.setLabelPadding",
          [](QCustomPlot& plot) { plot.xAxis->setLabelPadding(plot.xAxis->labelPadding() + 3); } },
        { "axis.setPadding",
          [](QCustomPlot& plot) { plot.xAxis->setPadding(plot.xAxis->padding() + 3); } },
        { "axis.setOffset",
          [](QCustomPlot& plot) { plot.xAxis->setOffset(plot.xAxis->offset() + 3); } },
        { "axis.setSelectedTickLabelFont",
          [](QCustomPlot& plot)
          {
              QFont font = plot.xAxis->tickLabelFont();
              font.setPointSizeF(font.pointSizeF() + 2);
              plot.xAxis->setSelectedTickLabelFont(font);
          } },
        { "axis.setSelectedLabelFont",
          [](QCustomPlot& plot)
          {
              QFont font = plot.xAxis->labelFont();
              font.setPointSizeF(font.pointSizeF() + 2);
              plot.xAxis->setSelectedLabelFont(font);
          } },
        { "axis.setSelectedTickLabelColor",
          [](QCustomPlot& plot) { plot.xAxis->setSelectedTickLabelColor(Qt::magenta); } },
        { "axis.setSelectedLabelColor",
          [](QCustomPlot& plot) { plot.xAxis->setSelectedLabelColor(Qt::magenta); } },
        { "axis.setSelectedBasePen",
          [](QCustomPlot& plot) { plot.xAxis->setSelectedBasePen(QPen(Qt::magenta, 3)); } },
        { "axis.setSelectedTickPen",
          [](QCustomPlot& plot) { plot.xAxis->setSelectedTickPen(QPen(Qt::magenta, 3)); } },
        { "axis.setSelectedSubTickPen",
          [](QCustomPlot& plot) { plot.xAxis->setSelectedSubTickPen(QPen(Qt::magenta, 3)); } },
        { "axis.setLowerEnding",
          [](QCustomPlot& plot) { plot.xAxis->setLowerEnding(QCPLineEnding::esSpikeArrow); } },
        { "axis.setUpperEnding",
          [](QCustomPlot& plot) { plot.xAxis->setUpperEnding(QCPLineEnding::esSpikeArrow); } },
        { "axisRect.setBackground(brush)",
          [](QCustomPlot& plot) { plot.axisRect(0)->setBackground(QBrush(Qt::yellow)); } },
        { "axisRect.setBackground(pixmap)",
          [](QCustomPlot& plot) { plot.axisRect(0)->setBackground(QPixmap(8, 8)); } },
        { "axisRect.setBackground(pixmap, scaled, mode)", [](QCustomPlot& plot)
          { plot.axisRect(0)->setBackground(QPixmap(8, 8), true, Qt::KeepAspectRatio); } },
        { "axisRect.setBackgroundScaled",
          [](QCustomPlot& plot) { plot.axisRect(0)->setBackgroundScaled(false); } },
        { "axisRect.setBackgroundScaledMode", [](QCustomPlot& plot)
          { plot.axisRect(0)->setBackgroundScaledMode(Qt::KeepAspectRatio); } },
        { "axis.setRangeReversed", [](QCustomPlot& plot) { plot.xAxis->setRangeReversed(true); } },
        { "axis.setLabelRenderer",
          [](QCustomPlot& plot) { plot.xAxis->setLabelRenderer(nullptr); } },
    };
    return cases;
}

const QVector<AxisAppearanceCase>& equalValueCases()
{
    static const QVector<AxisAppearanceCase> cases {
        { "axis.setLabel", [](QCustomPlot& plot) { plot.xAxis->setLabel(QStringLiteral("x")); } },
        { "axis.setTickLengthIn", [](QCustomPlot& plot) { plot.xAxis->setTickLengthIn(7); } },
        { "axis.setTickLabelColor",
          [](QCustomPlot& plot) { plot.xAxis->setTickLabelColor(Qt::blue); } },
        { "axis.setPadding", [](QCustomPlot& plot) { plot.xAxis->setPadding(6); } },
        { "grid.setPen", [](QCustomPlot& plot) { plot.xAxis->grid()->setPen(QPen(Qt::red, 2)); } },
        { "axisRect.setBackgroundScaled",
          [](QCustomPlot& plot) { plot.axisRect(0)->setBackgroundScaled(false); } },
    };
    return cases;
}
} // namespace

void TestRepaint::axisSettersScheduleRepaint_data()
{
    QTest::addColumn<QString>("setter");
    for (const AxisAppearanceCase& c : axisAppearanceCases())
        QTest::newRow(c.name) << QString::fromLatin1(c.name);
}

void TestRepaint::axisSettersScheduleRepaint()
{
    QFETCH(QString, setter);
    auto plot = settledPlot();
    QVERIFY(!plot->isRepaintQueued());

    const auto& cases = axisAppearanceCases();
    auto it = std::find_if(cases.cbegin(), cases.cend(), [&setter](const AxisAppearanceCase& c)
                           { return setter == QLatin1String(c.name); });
    QVERIFY(it != cases.cend());
    it->change(*plot);

    QVERIFY2(plot->isRepaintQueued(), qPrintable(setter + QStringLiteral(" scheduled no repaint")));
}

namespace
{
struct PlottableAppearanceCase
{
    const char* name;
    // Creates the plottable; the test settles afterwards so only the setter's own dirty counts.
    std::function<QCPAbstractPlottable*(QCustomPlot&)> prepare;
    std::function<void(QCPAbstractPlottable*)> change;
};

const QVector<PlottableAppearanceCase>& plottableAppearanceCases()
{
    using Plot = QCustomPlot&;
    static const QVector<PlottableAppearanceCase> cases {
        { "graph.setLineStyle", [](Plot plot) -> QCPAbstractPlottable* { return plot.graph(0); },
          [](QCPAbstractPlottable* p)
          { static_cast<QCPGraph*>(p)->setLineStyle(QCPGraph::lsStepLeft); } },
        { "graph.setScatterSkip", [](Plot plot) -> QCPAbstractPlottable* { return plot.graph(0); },
          [](QCPAbstractPlottable* p) { static_cast<QCPGraph*>(p)->setScatterSkip(2); } },
        { "graph.setAdaptiveSampling",
          [](Plot plot) -> QCPAbstractPlottable* { return plot.graph(0); },
          [](QCPAbstractPlottable* p) { static_cast<QCPGraph*>(p)->setAdaptiveSampling(false); } },
        { "graph.setPen", [](Plot plot) -> QCPAbstractPlottable* { return plot.graph(0); },
          [](QCPAbstractPlottable* p) { p->setPen(QPen(Qt::darkGreen, 3)); } },
        { "graph.setBrush", [](Plot plot) -> QCPAbstractPlottable* { return plot.graph(0); },
          [](QCPAbstractPlottable* p) { p->setBrush(QBrush(Qt::yellow)); } },
        { "graph.setName", [](Plot plot) -> QCPAbstractPlottable* { return plot.graph(0); },
          [](QCPAbstractPlottable* p) { p->setName(QStringLiteral("renamed")); } },
        { "curve.setPen",
          [](Plot plot) -> QCPAbstractPlottable* { return new QCPCurve(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { p->setPen(QPen(Qt::darkCyan, 3)); } },
        { "curve.setBrush",
          [](Plot plot) -> QCPAbstractPlottable* { return new QCPCurve(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { p->setBrush(QBrush(Qt::cyan)); } },
        { "curve.setScatterStyle",
          [](Plot plot) -> QCPAbstractPlottable* { return new QCPCurve(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p)
          {
              static_cast<QCPCurve*>(p)->setScatterStyle(
                  QCPScatterStyle(QCPScatterStyle::ssCircle, 6));
          } },
        { "bars.setWidth",
          [](Plot plot) -> QCPAbstractPlottable* { return new QCPBars(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPBars*>(p)->setWidth(0.5); } },
        { "bars.setWidthType", [](Plot plot) -> QCPAbstractPlottable*
          { return new QCPBars(plot.xAxis, plot.yAxis); }, [](QCPAbstractPlottable* p)
          { static_cast<QCPBars*>(p)->setWidthType(QCPBars::wtAbsolute); } },
        { "bars.setBaseValue",
          [](Plot plot) -> QCPAbstractPlottable* { return new QCPBars(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPBars*>(p)->setBaseValue(0.5); } },
        { "bars.setStackingGap",
          [](Plot plot) -> QCPAbstractPlottable* { return new QCPBars(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPBars*>(p)->setStackingGap(2); } },
        { "financial.setWidth", [](Plot plot) -> QCPAbstractPlottable*
          { return new QCPFinancial(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPFinancial*>(p)->setWidth(0.3); } },
        { "financial.setChartStyle", [](Plot plot) -> QCPAbstractPlottable*
          { return new QCPFinancial(plot.xAxis, plot.yAxis); }, [](QCPAbstractPlottable* p)
          { static_cast<QCPFinancial*>(p)->setChartStyle(QCPFinancial::csOhlc); } },
        { "financial.setPenNegative", [](Plot plot) -> QCPAbstractPlottable*
          { return new QCPFinancial(plot.xAxis, plot.yAxis); }, [](QCPAbstractPlottable* p)
          { static_cast<QCPFinancial*>(p)->setPenNegative(QPen(Qt::magenta, 2)); } },
        { "statisticalbox.setWidth", [](Plot plot) -> QCPAbstractPlottable*
          { return new QCPStatisticalBox(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPStatisticalBox*>(p)->setWidth(0.6); } },
        { "statisticalbox.setWhiskerPen", [](Plot plot) -> QCPAbstractPlottable*
          { return new QCPStatisticalBox(plot.xAxis, plot.yAxis); }, [](QCPAbstractPlottable* p)
          { static_cast<QCPStatisticalBox*>(p)->setWhiskerPen(QPen(Qt::magenta, 2)); } },
        { "statisticalbox.setMedianPen", [](Plot plot) -> QCPAbstractPlottable*
          { return new QCPStatisticalBox(plot.xAxis, plot.yAxis); }, [](QCPAbstractPlottable* p)
          { static_cast<QCPStatisticalBox*>(p)->setMedianPen(QPen(Qt::darkGreen, 3)); } },
        { "errorbars.setWhiskerWidth", [](Plot plot) -> QCPAbstractPlottable*
          { return new QCPErrorBars(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPErrorBars*>(p)->setWhiskerWidth(10); } },
        { "errorbars.setErrorType", [](Plot plot) -> QCPAbstractPlottable*
          { return new QCPErrorBars(plot.xAxis, plot.yAxis); }, [](QCPAbstractPlottable* p)
          { static_cast<QCPErrorBars*>(p)->setErrorType(QCPErrorBars::etKeyError); } },
        { "colormap.setDataRange", [](Plot plot) -> QCPAbstractPlottable*
          { return new QCPColorMap(plot.xAxis, plot.yAxis); }, [](QCPAbstractPlottable* p)
          { static_cast<QCPColorMap*>(p)->setDataRange(QCPRange(0, 5)); } },
        { "colormap.setDataScaleType", [](Plot plot) -> QCPAbstractPlottable*
          { return new QCPColorMap(plot.xAxis, plot.yAxis); }, [](QCPAbstractPlottable* p)
          { static_cast<QCPColorMap*>(p)->setDataScaleType(QCPAxis::stLogarithmic); } },
        { "colormap.setGradient", [](Plot plot) -> QCPAbstractPlottable*
          { return new QCPColorMap(plot.xAxis, plot.yAxis); }, [](QCPAbstractPlottable* p)
          { static_cast<QCPColorMap*>(p)->setGradient(QCPColorGradient::gpJet); } },
        { "colormap.setInterpolate", [](Plot plot) -> QCPAbstractPlottable*
          { return new QCPColorMap(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPColorMap*>(p)->setInterpolate(false); } },
        { "colormap.setTightBoundary", [](Plot plot) -> QCPAbstractPlottable*
          { return new QCPColorMap(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPColorMap*>(p)->setTightBoundary(true); } },
        { "colormap2.setGapThreshold", [](Plot plot) -> QCPAbstractPlottable*
          { return new QCPColorMap2(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPColorMap2*>(p)->setGapThreshold(0.5); } },
        { "colormap2.setContourPen", [](Plot plot) -> QCPAbstractPlottable*
          { return new QCPColorMap2(plot.xAxis, plot.yAxis); }, [](QCPAbstractPlottable* p)
          { static_cast<QCPColorMap2*>(p)->setContourPen(QPen(Qt::magenta, 2)); } },
        { "colormap2.setAutoContourLevels", [](Plot plot) -> QCPAbstractPlottable*
          { return new QCPColorMap2(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPColorMap2*>(p)->setAutoContourLevels(8); } },
        { "histogram.setBins", [](Plot plot) -> QCPAbstractPlottable*
          { return new QCPHistogram(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPHistogram*>(p)->setBins(12); } },
        { "histogram.setStyle", [](Plot plot) -> QCPAbstractPlottable*
          { return new QCPHistogram(plot.xAxis, plot.yAxis); }, [](QCPAbstractPlottable* p)
          { static_cast<QCPHistogram*>(p)->setStyle(QCPHistogram::sStep); } },
        { "histogram2d.setContourPen", [](Plot plot) -> QCPAbstractPlottable*
          { return new QCPHistogram2D(plot.xAxis, plot.yAxis); }, [](QCPAbstractPlottable* p)
          { static_cast<QCPHistogram2D*>(p)->setContourPen(QPen(Qt::magenta, 2)); } },
        { "multigraph.setLineStyle", [](Plot plot) -> QCPAbstractPlottable*
          { return new QCPMultiGraph(plot.xAxis, plot.yAxis); }, [](QCPAbstractPlottable* p)
          { static_cast<QCPMultiGraph*>(p)->setLineStyle(QCPMultiGraph::lsNone); } },
        { "multigraph.setAdaptiveSampling", [](Plot plot) -> QCPAbstractPlottable*
          { return new QCPMultiGraph(plot.xAxis, plot.yAxis); }, [](QCPAbstractPlottable* p)
          { static_cast<QCPMultiGraph*>(p)->setAdaptiveSampling(false); } },
        { "waterfall.setOffsetMode", [](Plot plot) -> QCPAbstractPlottable*
          { return new QCPWaterfallGraph(plot.xAxis, plot.yAxis); }, [](QCPAbstractPlottable* p)
          { static_cast<QCPWaterfallGraph*>(p)->setOffsetMode(QCPWaterfallGraph::omCustom); } },
        { "waterfall.setGain", [](Plot plot) -> QCPAbstractPlottable*
          { return new QCPWaterfallGraph(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPWaterfallGraph*>(p)->setGain(2.0); } },
        { "waterfall.setNormalize", [](Plot plot) -> QCPAbstractPlottable*
          { return new QCPWaterfallGraph(plot.xAxis, plot.yAxis); }, [](QCPAbstractPlottable* p)
          { static_cast<QCPWaterfallGraph*>(p)->setNormalize(false); } },
    };
    return cases;
}
} // namespace

void TestRepaint::plottableSettersScheduleRepaint_data()
{
    QTest::addColumn<QString>("setter");
    for (const PlottableAppearanceCase& c : plottableAppearanceCases())
        QTest::newRow(c.name) << QString::fromLatin1(c.name);
}

void TestRepaint::plottableSettersScheduleRepaint()
{
    QFETCH(QString, setter);
    const auto& cases = plottableAppearanceCases();
    auto it = std::find_if(cases.cbegin(), cases.cend(), [&setter](const PlottableAppearanceCase& c)
                           { return setter == QLatin1String(c.name); });
    QVERIFY(it != cases.cend());

    auto plot = settledPlot();
    QCPAbstractPlottable* plottable = it->prepare(*plot);
    QVERIFY(plottable);
    waitUntilSettled(*plot);
    QVERIFY(!plot->isRepaintQueued());

    it->change(plottable);
    QVERIFY2(plot->isRepaintQueued(), qPrintable(setter + QStringLiteral(" scheduled no repaint")));
}

void TestRepaint::plottableEqualValueDoesNotDirty_data()
{
    QTest::addColumn<QString>("setter");
    static const char* const names[]
        = { "graph.setPen",         "graph.setName",           "curve.setPen",     "bars.setWidth",
            "colormap.setGradient", "multigraph.setLineStyle", "waterfall.setGain" };
    for (const char* name : names)
        QTest::newRow(name) << QString::fromLatin1(name);
}

void TestRepaint::plottableEqualValueDoesNotDirty()
{
    QFETCH(QString, setter);
    const auto& cases = plottableAppearanceCases();
    auto it = std::find_if(cases.cbegin(), cases.cend(), [&setter](const PlottableAppearanceCase& c)
                           { return setter == QLatin1String(c.name); });
    QVERIFY(it != cases.cend());

    auto plot = settledPlot();
    QCPAbstractPlottable* plottable = it->prepare(*plot);
    QVERIFY(plottable);
    waitUntilSettled(*plot);

    it->change(plottable);
    QVERIFY2(plot->isRepaintQueued(), qPrintable(setter + QStringLiteral(" changed nothing")));
    waitUntilSettled(*plot);

    it->change(plottable); // same value: there is nothing new to draw
    QVERIFY2(!plot->isRepaintQueued(),
             qPrintable(setter + QStringLiteral(" repainted an equal value")));
}

void TestRepaint::plottableGroupVisibilityRepaints()
{
    auto plot = settledPlot();
    QCPPlottableGroup* group = plot->addPlottableGroup(QStringLiteral("g"));
    QVERIFY(group);
    QVERIFY(group->addMember(plot->graph(0)));
    waitUntilSettled(*plot);

    group->setVisible(false);
    QVERIFY(plot->isRepaintQueued());
}

void TestRepaint::axisMarginChangeDuringPanRepaints()
{
    // A margin change moves the axis rect, so every layer must be redrawn at the new geometry: a
    // pan in progress would otherwise translate the old frame and leave the fill at the old edge.
    QCustomPlot plot;
    plot.resize(400, 300);
    auto* fill = new FillStub(plot.xAxis, plot.yAxis);
    fill->setPen(QPen(kBlue));
    plot.xAxis->setLabel(QStringLiteral("x"));
    plot.yAxis->setLabel(QStringLiteral("y"));
    plot.replot();
    waitUntilSettled(plot);
    const QRect before = plot.axisRect()->rect();

    fill->stall = QPointF(5, 0); // a pan in progress
    plot.xAxis->setTickLengthOut(60); // outside ticks take margin: the rect moves
    waitUntilSettled(plot);
    const QRect after = plot.axisRect()->rect();
    QVERIFY2(after != before, "the margin change must move the axis rect");

    // The fill is drawn over exactly the axis rect, so its bounding box is the rect: anything else
    // means the layer was shifted by the pending pan.
    const QImage screen = screenOf(&plot);
    int minX = screen.width(), minY = screen.height(), maxX = -1, maxY = -1;
    for (int y = 0; y < screen.height(); ++y)
    {
        for (int x = 0; x < screen.width(); ++x)
        {
            const QColor c = screen.pixelColor(x, y);
            if (c.blue() > 200 && c.red() < 80 && c.green() < 80)
            {
                minX = qMin(minX, x);
                minY = qMin(minY, y);
                maxX = qMax(maxX, x);
                maxY = qMax(maxY, y);
            }
        }
    }
    QVERIFY(maxX >= minX);
    // The fill covers exactly the axis rect; one pixel of slack for the antialiased border. A layer
    // translated by the pending pan would be off by the stall, not by one pixel.
    const QRect drawn(QPoint(minX, minY), QPoint(maxX, maxY));
    QVERIFY2(qAbs(drawn.left() - after.left()) <= 1 && qAbs(drawn.top() - after.top()) <= 1
                 && qAbs(drawn.right() - after.right()) <= 1
                 && qAbs(drawn.bottom() - after.bottom()) <= 1,
             qPrintable(QStringLiteral("drawn %1,%2 %3x%4 vs rect %5,%6 %7x%8")
                            .arg(drawn.x())
                            .arg(drawn.y())
                            .arg(drawn.width())
                            .arg(drawn.height())
                            .arg(after.x())
                            .arg(after.y())
                            .arg(after.width())
                            .arg(after.height())));
}

void TestRepaint::multiGraphLineStyleResetsComponents()
{
    // A component may carry its own style; the global setter must still reset it, so comparing the
    // global member alone is not enough to decide "nothing changed".
    QCustomPlot plot;
    auto* mg = new QCPMultiGraph(plot.xAxis, plot.yAxis);
    auto source = std::make_shared<QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double> { 0.0, 1.0 }, std::vector<std::vector<double>> { { 0.0, 1.0 } });
    mg->setDataSource(source);
    QCOMPARE(mg->componentCount(), 1);

    mg->setComponentLineStyle(0, QCPMultiGraph::lsStepLeft);
    QCOMPARE(mg->component(0).lineStyle, QCPMultiGraph::lsStepLeft);

    mg->setLineStyle(QCPMultiGraph::lsLine);
    QCOMPARE(mg->component(0).lineStyle, QCPMultiGraph::lsLine);
}

void TestRepaint::barsGroupSpacingRepaints()
{
    // The bars group is a QObject, not a layerable: it has to mark its member bars, a plot-wide
    // request would dirty no layer and the bars would keep their old spacing.
    QCustomPlot plot;
    plot.resize(400, 300);
    auto* group = new QCPBarsGroup(&plot);
    auto* first = new QCPBars(plot.xAxis, plot.yAxis);
    auto* second = new QCPBars(plot.xAxis, plot.yAxis);
    for (auto* bars : { first, second })
    {
        bars->setBarsGroup(group);
        bars->setData({ 1.0, 2.0, 3.0 }, { 0.5, 1.0, 0.5 });
    }
    plot.xAxis->setRange(0.5, 3.5);
    plot.yAxis->setRange(0, 1.5);
    plot.replot();
    waitUntilSettled(plot);
    const QImage before = screenOf(&plot);

    group->setSpacing(0.3);
    waitUntilSettled(plot);
    QVERIFY2(before != screenOf(&plot), "the new spacing never reached the screen");
}

void TestRepaint::axisUnlistedHandsOffsetToSibling()
{
    // Removing the innermost axis of a side hands its offset to the next one, which now dirties
    // layers: that must not happen while the plot is being torn down (~QCPAxis -> unlistAxis).
    auto plot = settledPlot();
    QCPAxisRect* rect = plot->axisRect();
    QCPAxis* inner = rect->addAxis(QCPAxis::atRight);
    QCPAxis* outer = rect->addAxis(QCPAxis::atRight);
    inner->setOffset(30);
    waitUntilSettled(*plot);

    rect->removeAxis(inner); // outer inherits the offset, and the changed margins repaint
    QVERIFY(plot->isRepaintQueued());

    waitUntilSettled(*plot);
    delete outer; // the rect takes the axis out of its lists on the way out
    waitUntilSettled(*plot);
    plot->replot();
    QVERIFY(!plot->isRepaintQueued());
}

void TestRepaint::equalValueDoesNotDirty_data()
{
    QTest::addColumn<QString>("setter");
    for (const AxisAppearanceCase& c : equalValueCases())
        QTest::newRow(c.name) << QString::fromLatin1(c.name);
}

void TestRepaint::equalValueDoesNotDirty()
{
    QFETCH(QString, setter);
    const auto& cases = equalValueCases();
    auto it = std::find_if(cases.cbegin(), cases.cend(), [&setter](const AxisAppearanceCase& c)
                           { return setter == QLatin1String(c.name); });
    QVERIFY(it != cases.cend());

    auto plot = settledPlot();
    waitUntilSettled(*plot);

    it->change(*plot);
    QVERIFY2(plot->isRepaintQueued(), qPrintable(setter + QStringLiteral(" changed nothing")));
    waitUntilSettled(*plot);

    it->change(*plot); // same value: there is nothing new to draw
    QVERIFY2(!plot->isRepaintQueued(),
             qPrintable(setter + QStringLiteral(" repainted an equal value")));
}

namespace
{
struct RemainingCase
{
    const char* name;
    std::function<QObject*(QCustomPlot&)> prepare;
    std::function<void(QObject*)> change;
};

const QVector<RemainingCase>& remainingCases()
{
    static const QVector<RemainingCase> cases {
        { "text.setColor",
          [](QCustomPlot& plot) -> QObject*
          {
              auto* item = new QCPItemText(&plot);
              item->position->setCoords(1, 1);
              return item;
          },
          [](QObject* o) { static_cast<QCPItemText*>(o)->setColor(Qt::green); } },
        { "text.setRotation",
          [](QCustomPlot& plot) -> QObject*
          {
              auto* item = new QCPItemText(&plot);
              item->position->setCoords(1, 1);
              return item;
          },
          [](QObject* o) { static_cast<QCPItemText*>(o)->setRotation(15); } },
        { "line.setPen",
          [](QCustomPlot& plot) -> QObject*
          {
              auto* item = new QCPItemLine(&plot);
              item->start->setCoords(0, 0);
              item->end->setCoords(1, 1);
              return item;
          },
          [](QObject* o) { static_cast<QCPItemLine*>(o)->setPen(QPen(Qt::darkGreen, 3)); } },
        { "ellipse.setBrush",
          [](QCustomPlot& plot) -> QObject*
          {
              auto* item = new QCPItemEllipse(&plot);
              item->topLeft->setCoords(1, 1);
              item->bottomRight->setCoords(3, 3);
              return item;
          },
          [](QObject* o) { static_cast<QCPItemEllipse*>(o)->setBrush(QBrush(Qt::cyan)); } },
        { "rect.setPen",
          [](QCustomPlot& plot) -> QObject*
          {
              auto* item = new QCPItemRect(&plot);
              item->topLeft->setCoords(1, 1);
              item->bottomRight->setCoords(3, 3);
              return item;
          },
          [](QObject* o) { static_cast<QCPItemRect*>(o)->setPen(QPen(Qt::magenta, 3)); } },
        { "span.setRange",
          [](QCustomPlot& plot) -> QObject*
          {
              auto* span = new QCPItemHSpan(&plot);
              return span;
          },
          [](QObject* o) { static_cast<QCPItemHSpan*>(o)->setRange(QCPRange(2.5, 7.5)); } },
        { "legend.setFont", [](QCustomPlot& plot) -> QObject* { return plot.legend; },
          [](QObject* o)
          { static_cast<QCPLegend*>(o)->setFont(QFont(QStringLiteral("Helvetica"), 17)); } },
        { "legend.setIconSize", [](QCustomPlot& plot) -> QObject* { return plot.legend; },
          [](QObject* o) { static_cast<QCPLegend*>(o)->setIconSize(37, 37); } },
        { "groupLegend.setExpanded",
          [](QCustomPlot& plot) -> QObject*
          {
              auto* legend = new QCPLegend();
              plot.plotLayout()->addElement(1, 0, legend);
              auto* multi = new QCPMultiGraph(plot.xAxis, plot.yAxis);
              auto* group = new QCPGroupLegendItem(legend, multi);
              legend->addItem(group);
              return group;
          },
          [](QObject* o) { static_cast<QCPGroupLegendItem*>(o)->setExpanded(true); } },
        { "polarAxis.setTickLabelMode",
          [](QCustomPlot& plot) -> QObject*
          {
              auto* polar = new QCPPolarAxisAngular(&plot);
              plot.plotLayout()->addElement(1, 0, polar);
              return polar;
          },
          [](QObject* o)
          {
              static_cast<QCPPolarAxisAngular*>(o)->setTickLabelMode(
                  QCPPolarAxisAngular::lmRotated);
          } },
        { "polarAxis.setSelectedSubTickPen",
          [](QCustomPlot& plot) -> QObject*
          {
              auto* polar = new QCPPolarAxisAngular(&plot);
              plot.plotLayout()->addElement(1, 0, polar);
              return polar;
          },
          [](QObject* o)
          { static_cast<QCPPolarAxisAngular*>(o)->setSelectedSubTickPen(QPen(Qt::magenta, 3)); } },
        { "polarGrid.setRadialZeroLinePen",
          [](QCustomPlot& plot) -> QObject*
          {
              auto* polar = new QCPPolarAxisAngular(&plot);
              return polar->grid();
          },
          [](QObject* o)
          { static_cast<QCPPolarGrid*>(o)->setRadialZeroLinePen(QPen(Qt::magenta, 2)); } },
        { "layerable.setAntialiased", [](QCustomPlot& plot) -> QObject* { return plot.graph(0); },
          [](QObject* o) { static_cast<QCPLayerable*>(o)->setAntialiased(false); } },
        { "span.setPen", [](QCustomPlot& plot) -> QObject* { return new QCPItemHSpan(&plot); },
          [](QObject* o) { static_cast<QCPItemHSpan*>(o)->setPen(QPen(Qt::darkRed, 3)); } },
        { "line.setHead",
          [](QCustomPlot& plot) -> QObject*
          {
              auto* item = new QCPItemLine(&plot);
              item->start->setCoords(0, 0);
              item->end->setCoords(1, 1);
              return item;
          },
          [](QObject* o)
          {
              static_cast<QCPItemLine*>(o)->setHead(QCPLineEnding(QCPLineEnding::esDisc, 12, 12));
          } },
        { "richText.setHtml",
          [](QCustomPlot& plot) -> QObject*
          {
              auto* item = new QCPItemRichText(&plot);
              item->position->setCoords(1, 1);
              return item;
          },
          [](QObject* o)
          { static_cast<QCPItemRichText*>(o)->setHtml(QStringLiteral("<b>x</b>")); } },
        { "polarAxis.setAngle",
          [](QCustomPlot& plot) -> QObject*
          {
              auto* polar = new QCPPolarAxisAngular(&plot);
              plot.plotLayout()->addElement(1, 0, polar);
              return polar;
          },
          [](QObject* o) { static_cast<QCPPolarAxisAngular*>(o)->setAngle(45); } },
        { "polarAxis.setNumberFormat",
          [](QCustomPlot& plot) -> QObject*
          {
              auto* polar = new QCPPolarAxisAngular(&plot);
              plot.plotLayout()->addElement(1, 0, polar);
              return polar;
          },
          [](QObject* o)
          { static_cast<QCPPolarAxisAngular*>(o)->setNumberFormat(QStringLiteral("f")); } },
        { "polarAxis.setBackground",
          [](QCustomPlot& plot) -> QObject*
          {
              auto* polar = new QCPPolarAxisAngular(&plot);
              plot.plotLayout()->addElement(1, 0, polar);
              return polar;
          },
          [](QObject* o)
          {
              static const QPixmap pixmap(8, 8); // one instance, so the cacheKey repeats
              static_cast<QCPPolarAxisAngular*>(o)->setBackground(pixmap);
          } },
        { "pixmap.setScaled",
          [](QCustomPlot& plot) -> QObject*
          {
              auto* item = new QCPItemPixmap(&plot);
              item->setPixmap(QPixmap(4, 4));
              item->topLeft->setCoords(1, 1);
              item->bottomRight->setCoords(3, 3);
              return item;
          },
          [](QObject* o) { static_cast<QCPItemPixmap*>(o)->setScaled(true); } },
        { "tracer.setSize",
          [](QCustomPlot& plot) -> QObject*
          {
              auto* item = new QCPItemTracer(&plot);
              item->setGraph(plot.graph(0));
              item->setGraphKey(5);
              return item;
          },
          [](QObject* o) { static_cast<QCPItemTracer*>(o)->setSize(12); } },
        { "tracer.setStyle",
          [](QCustomPlot& plot) -> QObject*
          {
              auto* item = new QCPItemTracer(&plot);
              item->setGraph(plot.graph(0));
              item->setGraphKey(5);
              return item;
          },
          [](QObject* o) { static_cast<QCPItemTracer*>(o)->setStyle(QCPItemTracer::tsSquare); } },
        { "textElement.setText",
          [](QCustomPlot& plot) -> QObject*
          {
              auto* el = new QCPTextElement(&plot, QStringLiteral("title"));
              plot.plotLayout()->insertRow(0);
              plot.plotLayout()->addElement(0, 0, el);
              return el;
          },
          [](QObject* o) { static_cast<QCPTextElement*>(o)->setText(QStringLiteral("changed")); } },
        { "textElement.setTextColor",
          [](QCustomPlot& plot) -> QObject*
          {
              auto* el = new QCPTextElement(&plot, QStringLiteral("title"));
              plot.plotLayout()->insertRow(0);
              plot.plotLayout()->addElement(0, 0, el);
              return el;
          },
          [](QObject* o) { static_cast<QCPTextElement*>(o)->setTextColor(Qt::darkRed); } },
        { "colorScale.setBarWidth",
          [](QCustomPlot& plot) -> QObject*
          {
              auto* scale = new QCPColorScale(&plot);
              plot.plotLayout()->addElement(1, 1, scale);
              return scale;
          },
          [](QObject* o) { static_cast<QCPColorScale*>(o)->setBarWidth(30); } },
        { "colorScale.setLabel",
          [](QCustomPlot& plot) -> QObject*
          {
              auto* scale = new QCPColorScale(&plot);
              plot.plotLayout()->addElement(1, 1, scale);
              return scale;
          },
          [](QObject* o) { static_cast<QCPColorScale*>(o)->setLabel(QStringLiteral("z")); } },
        { "legend.setBorderPen", [](QCustomPlot& plot) -> QObject* { return plot.legend; },
          [](QObject* o) { static_cast<QCPLegend*>(o)->setBorderPen(QPen(Qt::darkBlue, 2)); } },
        { "polarAxis.setTickLabelColor", [](QCustomPlot& plot) -> QObject*
          { return new QCPPolarAxisAngular(&plot); }, [](QObject* o)
          { static_cast<QCPPolarAxisAngular*>(o)->setTickLabelColor(Qt::darkBlue); } },
        { "polarAxis.setBasePen", [](QCustomPlot& plot) -> QObject*
          { return new QCPPolarAxisAngular(&plot); }, [](QObject* o)
          { static_cast<QCPPolarAxisAngular*>(o)->setBasePen(QPen(Qt::darkBlue, 3)); } },
        { "polarGrid.setAngularPen",
          [](QCustomPlot& plot) -> QObject*
          {
              auto* polar = new QCPPolarAxisAngular(&plot);
              auto* grid = new QCPPolarGrid(polar);
              return grid;
          },
          [](QObject* o)
          { static_cast<QCPPolarGrid*>(o)->setAngularPen(QPen(Qt::darkGreen, 3)); } },
        { "layer.setVisible",
          [](QCustomPlot& plot) -> QObject* { return plot.layer(QStringLiteral("overlay")); },
          [](QObject* o) { static_cast<QCPLayer*>(o)->setVisible(false); } },
    };
    return cases;
}
} // namespace

void TestRepaint::remainingEqualValueDoesNotDirty_data()
{
    QTest::addColumn<QString>("setter");
    for (const RemainingCase& c : remainingCases())
        QTest::newRow(c.name) << QString::fromLatin1(c.name);
}

void TestRepaint::remainingEqualValueDoesNotDirty()
{
    QFETCH(QString, setter);
    const auto& cases = remainingCases();
    auto it = std::find_if(cases.cbegin(), cases.cend(), [&setter](const RemainingCase& c)
                           { return setter == QLatin1String(c.name); });
    QVERIFY(it != cases.cend());

    auto plot = settledPlot();
    QObject* object = it->prepare(*plot);
    waitUntilSettled(*plot);
    waitUntilSettled(*plot);

    it->change(object);
    QVERIFY2(plot->isRepaintQueued(), qPrintable(setter + QStringLiteral(" changed nothing")));
    waitUntilSettled(*plot);

    it->change(object); // same value: nothing new to draw
    QVERIFY2(!plot->isRepaintQueued(),
             qPrintable(setter + QStringLiteral(" repainted an equal value")));
}

void TestRepaint::remainingSettersScheduleRepaint_data()
{
    QTest::addColumn<QString>("setter");
    for (const RemainingCase& c : remainingCases())
        QTest::newRow(c.name) << QString::fromLatin1(c.name);
}

void TestRepaint::remainingSettersScheduleRepaint()
{
    QFETCH(QString, setter);
    const auto& cases = remainingCases();
    auto it = std::find_if(cases.cbegin(), cases.cend(), [&setter](const RemainingCase& c)
                           { return setter == QLatin1String(c.name); });
    QVERIFY(it != cases.cend());

    auto plot = settledPlot();
    QObject* object = it->prepare(*plot);
    waitUntilSettled(*plot);
    waitUntilSettled(*plot); // a prepare may queue twice
    QVERIFY(!plot->isRepaintQueued());

    it->change(object);
    QVERIFY2(plot->isRepaintQueued(), qPrintable(setter + QStringLiteral(" scheduled no repaint")));
}

void TestRepaint::orphanPlottableKeepsName()
{
    // A plottable with null axes is a valid detached object: it has no legend to mark, but its name
    // is still its own.
    QCPGraph graph(nullptr, nullptr);
    graph.setName(QStringLiteral("orphan"));
    QCOMPARE(graph.name(), QStringLiteral("orphan"));
}

void TestRepaint::axisRectMarginsMoveTheContent()
{
    // A margin change on the axis rect moves the inner rect, so the axes, grid and plottables must
    // be redrawn there: a pending pan translation must not keep them at the old pixels.
    QCustomPlot plot;
    plot.resize(400, 300);
    auto* fill = new FillStub(plot.xAxis, plot.yAxis);
    fill->setPen(QPen(kBlue));
    plot.replot();
    waitUntilSettled(plot);
    const QRect before = plot.axisRect()->rect();

    fill->stall = QPointF(5, 0); // a pan in progress
    plot.axisRect()->setMinimumMargins(QMargins(40, 30, 10, 10));
    waitUntilSettled(plot);
    const QRect after = plot.axisRect()->rect();
    QVERIFY2(after != before, "the margin change must move the axis rect");

    const QImage screen = screenOf(&plot);
    int minX = screen.width(), minY = screen.height(), maxX = -1, maxY = -1;
    for (int y = 0; y < screen.height(); ++y)
        for (int x = 0; x < screen.width(); ++x)
        {
            const QColor c = screen.pixelColor(x, y);
            if (c.blue() > 200 && c.red() < 80 && c.green() < 80)
            {
                minX = qMin(minX, x);
                minY = qMin(minY, y);
                maxX = qMax(maxX, x);
                maxY = qMax(maxY, y);
            }
        }
    QVERIFY(maxX >= minX);
    const QRect drawn(QPoint(minX, minY), QPoint(maxX, maxY));
    QVERIFY2(qAbs(drawn.left() - after.left()) <= 1 && qAbs(drawn.top() - after.top()) <= 1
                 && qAbs(drawn.right() - after.right()) <= 1
                 && qAbs(drawn.bottom() - after.bottom()) <= 1,
             qPrintable(QStringLiteral("drawn %1,%2 %3x%4 vs rect %5,%6 %7x%8")
                            .arg(drawn.x())
                            .arg(drawn.y())
                            .arg(drawn.width())
                            .arg(drawn.height())
                            .arg(after.x())
                            .arg(after.y())
                            .arg(after.width())
                            .arg(after.height())));
}

void TestRepaint::polarPlotSettles()
{
    // An unscaled background pixmap used to mark dirty from inside drawBackground(), so every draw
    // queued another repaint.
    auto plot = settledPlot();
    auto* polar = new QCPPolarAxisAngular(plot.get());
    plot->plotLayout()->addElement(1, 0, polar);
    polar->setBackground(QPixmap(8, 8));
    polar->setBackgroundScaled(false);
    polar->setRange(0, 360);
    if (auto* radial = polar->radialAxis())
        radial->setRange(0, 1);
    plot->replot();
    waitUntilSettled(*plot);
    waitUntilSettled(*plot);

    int replots = 0;
    QObject::connect(plot.get(), &QCustomPlot::afterReplot, [&replots] { ++replots; });
    QTest::qWait(200);
    QCOMPARE(replots, 0);
    QVERIFY(!plot->isRepaintQueued());
}

void TestRepaint::exportSettlesPendingTheme()
{
    auto plot = settledPlot();
    QCPTheme dark;
    dark.setBackground(Qt::black);
    plot->setTheme(&dark);
    // Mutating the theme only queues a 0 ms timer; the export must not show the old colours.
    dark.setBackground(Qt::darkMagenta);
    const QImage image = plot->toPixmap(200, 150).toImage();
    const QColor corner = image.pixelColor(1, 1);
    QVERIFY2(qAbs(corner.hue() - QColor(Qt::darkMagenta).hue()) < 20, qPrintable(corner.name()));
}

void TestRepaint::layoutSettersIgnoreEqualValues()
{
    // The layout calls these on every replot with the values it just computed. A setter that marks
    // an unchanged value would dirty a layer on every frame, and nothing could ever translate.
    auto plot = settledPlot();
    waitUntilSettled(*plot);

    auto* rect = plot->axisRect();
    rect->setMargins(rect->margins());
    rect->setMinimumMargins(rect->minimumMargins());
    rect->setMinimumSize(rect->minimumSize());
    rect->setMaximumSize(rect->maximumSize());
    rect->setAutoMargins(rect->autoMargins());
    auto* grid = qobject_cast<QCPLayoutGrid*>(plot->plotLayout());
    QVERIFY(grid);
    grid->setRowSpacing(grid->rowSpacing());
    grid->setColumnSpacing(grid->columnSpacing());

    QVERIFY(!plot->isRepaintQueued());
}

void TestRepaint::graphTracerSettles()
{
    // The tracer recomputes its position inside draw(); that must not dirty the layer again, or
    // every replot schedules the next one forever.
    auto plot = settledPlot();
    auto* tracer = new QCPItemTracer(plot.get());
    tracer->setGraph(plot->graph(0));
    tracer->setGraphKey(5);
    waitUntilSettled(*plot);

    int replots = 0;
    QObject::connect(plot.get(), &QCustomPlot::afterReplot, [&replots]() { ++replots; });
    QTest::qWait(200);
    QCOMPARE(replots, 0);
    QVERIFY(!plot->isRepaintQueued());
}

void TestRepaint::secondLegendRowFollowsRename()
{
    // The default legend lives in an axis rect's inset layout: the rename walk has to descend
    // through axis rects, not just through QCPLayout children.
    auto plot = settledPlot();
    plot->addLayer(QStringLiteral("legend2"));
    auto* legend = new QCPLegend();
    plot->axisRect()->insetLayout()->addElement(legend, Qt::AlignTop | Qt::AlignRight);
    // Only now does the legend know its plot, which setLayer() needs.
    QVERIFY(legend->setLayer(QStringLiteral("legend2")));
    plot->graph(0)->addToLegend(legend);
    waitUntilSettled(*plot);
    waitUntilSettled(*plot);
    auto* row = legend->itemWithPlottable(plot->graph(0));
    QVERIFY(row);
    QVERIFY(!legend->outerRect().isEmpty());

    auto* layer = plot->layer(QStringLiteral("legend2"));
    QVERIFY(layer);
    QVERIFY2(!layer->dirtiedAfterDraw(), "a settled legend layer must not be dirty");

    plot->graph(0)->setName(QStringLiteral("renamed"));
    QCOMPARE(row->plottable()->name(), QStringLiteral("renamed"));
    QVERIFY2(layer->dirtiedAfterDraw(), "the rename did not mark the legend row");
}

QImage TestRepaint::screenOf(QCustomPlot* plot)
{
    if (plot->mRhi)
        return plot->grabFramebuffer();
    QImage image(plot->viewport().size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QCPPainter painter(&image);
    for (const auto& buffer : plot->mPaintBuffers)
        buffer->draw(&painter);
    painter.end();
    return image;
}

namespace
{
struct ColorMapDataCase
{
    const char* name;
    std::function<void(QCPColorMapData&)> change;
};

const QVector<ColorMapDataCase>& colorMapDataCases()
{
    static const QVector<ColorMapDataCase> cases {
        { "setCell", [](QCPColorMapData& data) { data.setCell(5, 4, 1.0); } },
        { "setSize", [](QCPColorMapData& data) { data.setSize(12, 9); } },
        { "setRange",
          [](QCPColorMapData& data) { data.setRange(QCPRange(1, 3), QCPRange(2, 4)); } },
        { "fill", [](QCPColorMapData& data) { data.fill(0.5); } },
    };
    return cases;
}
} // namespace

void TestRepaint::colorMapDataSettersRepaint_data()
{
    QTest::addColumn<QString>("setter");
    for (const auto& colorMapCase : colorMapDataCases())
        QTest::newRow(colorMapCase.name) << QString::fromLatin1(colorMapCase.name);
}

void TestRepaint::colorMapDataSettersRepaint()
{
    QFETCH(QString, setter);
    auto plot = settledPlot();
    auto map = plot->add<QCPColorMap>();
    map->data()->setSize(10, 8);
    map->data()->setRange(QCPRange(0, 10), QCPRange(0, 10));
    map->setDataRange(QCPRange(0, 1));
    waitUntilSettled(*plot);
    QVERIFY(!plot->isRepaintQueued());

    const auto found = std::find_if(colorMapDataCases().cbegin(), colorMapDataCases().cend(),
                                    [&](const ColorMapDataCase& colorMapCase)
                                    { return setter == QLatin1String(colorMapCase.name); });
    QVERIFY(found != colorMapDataCases().cend());
    found->change(*map->data());
    QVERIFY2(plot->isRepaintQueued(), qPrintable(setter + " scheduled no repaint"));
}

void TestRepaint::colorMapDataPixelFollowsSetCell()
{
    auto plot = settledPlot();
    auto map = plot->add<QCPColorMap>();
    map->data()->setSize(10, 8);
    map->data()->setRange(QCPRange(0, 10), QCPRange(0, 10));
    map->setDataRange(QCPRange(0, 1));
    for (int key = 0; key < 10; ++key)
        for (int value = 0; value < 8; ++value)
            map->data()->setCell(key, value, 0.0);
    waitUntilSettled(*plot);
    const QImage before = screenOf(plot.get());

    map->data()->setCell(5, 4, 1.0);
    waitUntilSettled(*plot);
    QVERIFY(before != screenOf(plot.get()));
}

namespace
{
// The mean x of the blue pixels: where a vertical blue line was drawn, or -1 if none was.
double blueColumn(const QImage& image)
{
    double sum = 0;
    int count = 0;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
        {
            const QColor c = image.pixelColor(x, y);
            if (c.blue() > 200 && c.red() < 80 && c.green() < 80)
            {
                sum += x;
                ++count;
            }
        }
    return count ? sum / count : -1;
}
} // namespace

void TestRepaint::itemOnOwnLayerFollowsRangeChange()
{
    // An item on a layer that holds no plottable: a zoom and a pan must redraw it at its new
    // pixels.
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.addLayer("items");
    auto* line = new QCPItemLine(&plot);
    line->setLayer("items");
    line->setPen(QPen(kBlue, 3));
    line->start->setCoords(5, 0);
    line->end->setCoords(5, 10);
    plot.xAxis->setRange(0, 10);
    plot.yAxis->setRange(0, 10);
    plot.replot();
    waitUntilSettled(plot);
    QVERIFY(qAbs(blueColumn(screenOf(&plot)) - plot.xAxis->coordToPixel(5)) <= 2);

    plot.xAxis->setRange(0, 20); // a zoom
    waitUntilSettled(plot);
    QVERIFY2(qAbs(blueColumn(screenOf(&plot)) - plot.xAxis->coordToPixel(5)) <= 2,
             qPrintable(QStringLiteral("zoom: drawn at %1, expected %2")
                            .arg(blueColumn(screenOf(&plot)))
                            .arg(plot.xAxis->coordToPixel(5))));

    plot.xAxis->setRange(2, 22); // a pan
    waitUntilSettled(plot);
    QVERIFY2(qAbs(blueColumn(screenOf(&plot)) - plot.xAxis->coordToPixel(5)) <= 2,
             qPrintable(QStringLiteral("pan: drawn at %1, expected %2")
                            .arg(blueColumn(screenOf(&plot)))
                            .arg(plot.xAxis->coordToPixel(5))));
}

void TestRepaint::anchoredChildOnOtherLayerFollowsRange()
{
    // A position that follows an anchor of another item moves when that item's axis moves, so the
    // follower's layer has to be marked too, or it keeps last frame's pixels.
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.addLayer(QStringLiteral("labels"));
    plot.xAxis->setRange(0, 10);
    plot.yAxis->setRange(0, 10);
    plot.xAxis2->setRange(100, 200);
    plot.yAxis2->setRange(0, 10);
    auto* parent = new QCPItemLine(&plot);
    parent->start->setAxes(plot.xAxis2, plot.yAxis2);
    parent->end->setAxes(plot.xAxis2, plot.yAxis2);
    parent->start->setCoords(120, 2);
    parent->end->setCoords(120, 8);
    auto* child = new QCPItemText(&plot);
    QVERIFY(child->setLayer(QStringLiteral("labels")));
    child->position->setParentAnchor(parent->end);
    child->setText(QStringLiteral("follower"));
    plot.replot();
    waitUntilSettled(plot);
    waitUntilSettled(plot);
    auto* labels = plot.layer(QStringLiteral("labels"));
    auto* parentLayer = parent->layer();
    QVERIFY(labels && parentLayer);
    QVERIFY2(!labels->dirtiedAfterDraw(), "a settled labels layer must not be dirty");
    QVERIFY2(!parentLayer->dirtiedAfterDraw(), "a settled parent layer must not be dirty");

    plot.xAxis2->setRange(150, 250); // moves the parent, and with it the follower
    QVERIFY2(parentLayer->dirtiedAfterDraw(), "the parent's layer was not marked");
    QVERIFY2(labels->dirtiedAfterDraw(), "the layer of the anchored follower was not marked");
}

namespace
{
// A line on the main layer and a text on "labels" whose position follows the line's end.
struct AnchoredPair
{
    QCPItemLine* parent;
    QCPLayer* labels;
};

AnchoredPair makeAnchoredPair(QCustomPlot& plot)
{
    plot.resize(400, 300);
    plot.addLayer(QStringLiteral("labels"));
    plot.xAxis->setRange(0, 10);
    plot.yAxis->setRange(0, 10);
    plot.xAxis2->setRange(100, 200);
    plot.yAxis2->setRange(0, 10);
    auto* parent = new QCPItemLine(&plot);
    parent->start->setCoords(2, 2);
    parent->end->setCoords(2, 8);
    auto* child = new QCPItemText(&plot);
    child->setLayer(QStringLiteral("labels"));
    child->position->setParentAnchor(parent->end);
    child->setText(QStringLiteral("follower"));
    return { parent, plot.layer(QStringLiteral("labels")) };
}
} // namespace

void TestRepaint::movedParentRepaintsAnchoredChild()
{
    QCustomPlot plot;
    const AnchoredPair pair = makeAnchoredPair(plot);
    plot.replot();
    waitUntilSettled(plot);
    waitUntilSettled(plot);
    QVERIFY2(!pair.labels->dirtiedAfterDraw(), "a settled labels layer must not be dirty");

    pair.parent->end->setCoords(5, 8); // the follower moves with it
    QVERIFY2(pair.labels->dirtiedAfterDraw(), "the layer of the anchored follower was not marked");
}

void TestRepaint::parentOnNewAxesStillMovesAnchoredChild()
{
    QCustomPlot plot;
    const AnchoredPair pair = makeAnchoredPair(plot);
    plot.replot();
    waitUntilSettled(plot);
    plot.yAxis->setRange(0, 11); // the labels layer now knows the axes its follower uses
    plot.replot();
    waitUntilSettled(plot);

    pair.parent->start->setAxes(plot.xAxis2, plot.yAxis2);
    pair.parent->end->setAxes(plot.xAxis2, plot.yAxis2);
    plot.replot();
    waitUntilSettled(plot);
    waitUntilSettled(plot);
    QVERIFY2(!pair.labels->dirtiedAfterDraw(), "a settled labels layer must not be dirty");

    plot.xAxis2->setRange(150, 250); // moves the parent, and with it the follower
    QVERIFY2(pair.labels->dirtiedAfterDraw(),
             "the follower's layer missed the axes its parent moved to");
}

void TestRepaint::rangeChangeMarksOnlyLayersUsingTheAxis()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.addLayer(QStringLiteral("a"));
    plot.addLayer(QStringLiteral("b"));
    for (QCPAxis* axis : { plot.xAxis, plot.yAxis, plot.xAxis2, plot.yAxis2 })
        axis->setRange(0, 10);
    auto* onFirst = new QCPItemLine(&plot);
    QVERIFY(onFirst->setLayer(QStringLiteral("a")));
    onFirst->start->setCoords(1, 1);
    onFirst->end->setCoords(2, 2);
    auto* onSecond = new QCPItemLine(&plot);
    QVERIFY(onSecond->setLayer(QStringLiteral("b")));
    onSecond->start->setAxes(plot.xAxis2, plot.yAxis2);
    onSecond->end->setAxes(plot.xAxis2, plot.yAxis2);
    onSecond->start->setCoords(1, 1);
    onSecond->end->setCoords(2, 2);
    plot.replot();
    waitUntilSettled(plot);
    waitUntilSettled(plot);
    auto* a = plot.layer(QStringLiteral("a"));
    auto* b = plot.layer(QStringLiteral("b"));
    QVERIFY(a && b);
    QVERIFY2(!a->dirtiedAfterDraw() && !b->dirtiedAfterDraw(), "both item layers are settled");

    plot.xAxis2->setRange(2, 12);
    QVERIFY2(b->dirtiedAfterDraw(), "the layer whose item uses xAxis2 was not marked");
    QVERIFY2(!a->dirtiedAfterDraw(), "the layer on the untouched axis was marked");
}

void TestRepaint::setAxesMovesTheLayerToTheNewAxis()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.addLayer(QStringLiteral("b"));
    for (QCPAxis* axis : { plot.xAxis, plot.yAxis, plot.xAxis2, plot.yAxis2 })
        axis->setRange(0, 10);
    auto* line = new QCPItemLine(&plot);
    QVERIFY(line->setLayer(QStringLiteral("b")));
    line->start->setCoords(1, 1);
    line->end->setCoords(2, 2);
    plot.replot();
    waitUntilSettled(plot);
    waitUntilSettled(plot);
    auto* b = plot.layer(QStringLiteral("b"));
    QVERIFY(b);
    QVERIFY2(!b->dirtiedAfterDraw(), "a settled item layer must not be dirty");

    line->start->setAxes(plot.xAxis2, plot.yAxis2);
    line->end->setAxes(plot.xAxis2, plot.yAxis2);
    waitUntilSettled(plot);
    waitUntilSettled(plot);

    plot.xAxis2->setRange(3, 13);
    QVERIFY2(b->dirtiedAfterDraw(), "the layer did not follow its item onto xAxis2");
}

void TestRepaint::itemTakenAndAddedKeepsAxes()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.addLayer(QStringLiteral("b"));
    for (QCPAxis* axis : { plot.xAxis, plot.yAxis, plot.xAxis2, plot.yAxis2 })
        axis->setRange(0, 10);
    auto* line = new QCPItemLine(&plot);
    line->start->setAxes(plot.xAxis2, plot.yAxis2);
    line->end->setAxes(plot.xAxis2, plot.yAxis2);
    line->start->setCoords(1, 1);
    line->end->setCoords(2, 2);
    plot.replot();
    waitUntilSettled(plot);
    waitUntilSettled(plot);

    QCPAbstractItem* item = line;
    auto handle = qcp::makeHandle<QCPHandle<QCPAbstractItem>>(item);
    auto taken = plot.take(handle);
    QVERIFY(taken);
    auto added = plot.add(std::move(taken));
    QVERIFY(added);
    QVERIFY(added.get()->setLayer(QStringLiteral("b")));
    // adopt() puts a re-added item back on the plot's default axes, as for a new item.
    auto* readded = qobject_cast<QCPItemLine*>(added.get());
    QVERIFY(readded);
    readded->start->setAxes(plot.xAxis2, plot.yAxis2);
    readded->end->setAxes(plot.xAxis2, plot.yAxis2);
    plot.replot();
    waitUntilSettled(plot);
    waitUntilSettled(plot);
    auto* b = plot.layer(QStringLiteral("b"));
    QVERIFY(b);
    QVERIFY2(!b->dirtiedAfterDraw(), "a settled item layer must not be dirty");

    plot.xAxis2->setRange(4, 14);
    QVERIFY2(b->dirtiedAfterDraw(), "a taken and re-added item left its layer without axes");
}

namespace
{
class CountingLine : public QCPItemLine
{
public:
    explicit CountingLine(QCustomPlot* plot) : QCPItemLine(plot) { }

    double selectTest(const QPointF& pos, bool onlySelectable,
                      QVariant* details = nullptr) const override
    {
        ++mSelectCalls;
        return QCPItemLine::selectTest(pos, onlySelectable, details);
    }

    mutable int mSelectCalls = 0;
};

// An item that deliberately does not override pixelBounds(): it must never be culled.
class PlainItem : public QCPAbstractItem
{
public:
    explicit PlainItem(QCustomPlot* plot) : QCPAbstractItem(plot) { }

    void draw(QCPPainter* painter) override
    {
        ++mDrawCalls;
        painter->setPen(QPen(Qt::darkMagenta, 2));
        painter->drawLine(mCentre + QPointF(-4, 0), mCentre + QPointF(4, 0));
        painter->drawLine(mCentre + QPointF(0, -4), mCentre + QPointF(0, 4));
    }

    double selectTest(const QPointF& pos, bool onlySelectable, QVariant*) const override
    {
        ++mSelectCalls;
        Q_UNUSED(onlySelectable)
        return QCPVector2D(pos - mCentre).length();
    }

    QPointF mCentre;
    mutable int mDrawCalls = 0;
    mutable int mSelectCalls = 0;
};

// The item's data extent mapped to pixels: exactly what the old pixelBounds() returned.
QRectF extentPixelRect(const QCPItemExtent& extent)
{
    const QPointF a(extent.keyAxis->coordToPixel(extent.box.keyLower),
                    extent.valueAxis->coordToPixel(extent.box.valueLower));
    const QPointF b(extent.keyAxis->coordToPixel(extent.box.keyUpper),
                    extent.valueAxis->coordToPixel(extent.box.valueUpper));
    const double pad = extent.pixelPad;
    return QRectF(a, b).normalized().adjusted(-pad, -pad, pad, pad);
}

QCPAbstractItem* makeBoundedItem(const QString& kind, QCustomPlot* plot)
{
    if (kind == QLatin1String("line"))
    {
        auto item = plot->add<QCPItemLine>();
        item->setPen(QPen(Qt::darkRed, 6));
        item->setHead(QCPLineEnding(QCPLineEnding::esSpikeArrow, 14, 9));
        item->setTail(QCPLineEnding(QCPLineEnding::esDisc, 12));
        item->start->setCoords(2, 2);
        item->end->setCoords(8, 7);
        return item.get();
    }
    if (kind == QLatin1String("rect"))
    {
        auto item = plot->add<QCPItemRect>();
        item->setPen(QPen(Qt::darkRed, 6));
        item->topLeft->setCoords(2, 2);
        item->bottomRight->setCoords(8, 7);
        return item.get();
    }
    if (kind == QLatin1String("ellipse"))
    {
        auto item = plot->add<QCPItemEllipse>();
        item->setPen(QPen(Qt::darkRed, 6));
        item->topLeft->setCoords(2, 2);
        item->bottomRight->setCoords(8, 7);
        return item.get();
    }
    if (kind == QLatin1String("curve"))
    {
        auto item = plot->add<QCPItemCurve>();
        item->setPen(QPen(Qt::darkRed, 5));
        item->setHead(QCPLineEnding(QCPLineEnding::esSpikeArrow, 14, 9));
        item->start->setCoords(2, 2);
        item->startDir->setCoords(7, 9);
        item->endDir->setCoords(3, 9);
        item->end->setCoords(8, 3);
        return item.get();
    }
    if (kind == QLatin1String("bracket"))
    {
        auto item = plot->add<QCPItemBracket>();
        item->setPen(QPen(Qt::darkRed, 5));
        item->setLength(90);
        item->left->setCoords(2, 5);
        item->right->setCoords(8, 5);
        return item.get();
    }
    return nullptr;
}
} // namespace

void TestRepaint::itemExtentContainsDrawing_data()
{
    QTest::addColumn<QString>("item");
    QTest::newRow("line") << QStringLiteral("line");
    QTest::newRow("rect") << QStringLiteral("rect");
    QTest::newRow("ellipse") << QStringLiteral("ellipse");
    QTest::newRow("curve") << QStringLiteral("curve");
    QTest::newRow("bracket") << QStringLiteral("bracket");
}

void TestRepaint::itemExtentContainsDrawing()
{
    QFETCH(QString, item);
    auto plot = settledPlot();
    QCPAbstractItem* subject = makeBoundedItem(item, plot.get());
    QVERIFY(subject);

    // Returns the number of painted pixels, or -1 with a message when one is outside the bounds.
    int paintedPixels = 0;
    QString mistake;
    const auto paintedOutside = [&](const QImage& drawn, const QImage& hidden, const QRectF& bounds)
    {
        paintedPixels = 0;
        for (int y = 0; y < drawn.height(); ++y)
        {
            for (int x = 0; x < drawn.width(); ++x)
            {
                if (drawn.pixel(x, y) == hidden.pixel(x, y))
                    continue;
                ++paintedPixels;
                if (!bounds.contains(QPointF(x + 0.5, y + 0.5)))
                    mistake = QStringLiteral("%1: pixel %2,%3 is painted outside %4,%5 %6x%7")
                                  .arg(item)
                                  .arg(x)
                                  .arg(y)
                                  .arg(bounds.x())
                                  .arg(bounds.y())
                                  .arg(bounds.width())
                                  .arg(bounds.height());
            }
        }
        return paintedPixels;
    };

    waitUntilSettled(*plot);
    const std::optional<QCPItemExtent> extent = subject->dataExtent();
    QVERIFY2(extent.has_value(), "the item reports no data extent");
    const QRectF bounds = extentPixelRect(*extent);
    const QImage painted = screenOf(plot.get());
    subject->setVisible(false);
    waitUntilSettled(*plot);
    const QImage hidden = screenOf(plot.get());
    QVERIFY2(paintedOutside(painted, hidden, bounds) > 0, "the item painted nothing");
    QVERIFY2(mistake.isEmpty(), qPrintable(mistake));

    // A selected item draws with its selected pen, which may be much wider.
    const QPen wideSelected(Qt::darkBlue, 13);
    if (auto* line = qobject_cast<QCPItemLine*>(subject))
        line->setSelectedPen(wideSelected);
    else if (auto* rect = qobject_cast<QCPItemRect*>(subject))
        rect->setSelectedPen(wideSelected);
    else if (auto* ellipse = qobject_cast<QCPItemEllipse*>(subject))
        ellipse->setSelectedPen(wideSelected);
    else if (auto* curve = qobject_cast<QCPItemCurve*>(subject))
        curve->setSelectedPen(wideSelected);
    else if (auto* bracket = qobject_cast<QCPItemBracket*>(subject))
        bracket->setSelectedPen(wideSelected);
    subject->setVisible(true);
    subject->setSelected(true);
    waitUntilSettled(*plot);
    const std::optional<QCPItemExtent> selectedExtent = subject->dataExtent();
    QVERIFY2(selectedExtent.has_value(), "the selected item reports no data extent");
    const QRectF selectedBounds = extentPixelRect(*selectedExtent);
    const QImage selected = screenOf(plot.get());
    subject->setVisible(false);
    waitUntilSettled(*plot);
    const QImage selectedHidden = screenOf(plot.get());
    mistake.clear();
    QVERIFY2(paintedOutside(selected, selectedHidden, selectedBounds) > 0,
             "the selected item painted nothing");
    QVERIFY2(mistake.isEmpty(), qPrintable(mistake));
}

void TestRepaint::itemScrolledIntoViewIsDrawn()
{
    auto plot = settledPlot();
    auto line = plot->add<QCPItemLine>();
    line->setPen(QPen(Qt::darkGreen, 5));
    line->start->setCoords(100, 2);
    line->end->setCoords(100, 8);
    waitUntilSettled(*plot);
    QCOMPARE(pixelsOfColor(screenOf(plot.get()), Qt::darkGreen), 0);
    plot->xAxis->setRange(95, 105);
    waitUntilSettled(*plot);
    QVERIFY2(pixelsOfColor(screenOf(plot.get()), Qt::darkGreen) > 0,
             "the item stayed culled after the range brought it into view");
}

void TestRepaint::culledItemIsNotHitTested()
{
    auto plot = settledPlot();
    auto* near = new CountingLine(plot.get());
    near->start->setCoords(4, 4);
    near->end->setCoords(6, 6);
    auto* far = new CountingLine(plot.get());
    far->start->setCoords(1000, 4);
    far->end->setCoords(1000, 6);
    waitUntilSettled(*plot);

    const QPointF pos(plot->xAxis->coordToPixel(5), plot->yAxis->coordToPixel(5));
    const QList<QCPLayerable*> hits = plot->layerableListAt(pos, true);
    QVERIFY(hits.contains(near));
    QCOMPARE(far->mSelectCalls, 0);
}

void TestRepaint::takenItemUsesNoBoundsOfItsOldPlot()
{
    // Each plot counts its paint generations from zero, so the stamp an item got in one plot can
    // equal another plot's generation. Every offset is tried: the first draw in the new plot must
    // not be needed for the hit test to use the item's new place.
    for (int offset = 0; offset < 60; ++offset)
    {
        auto from = settledPlot();
        auto to = settledPlot();
        to->xAxis->setRange(0, 100);
        waitUntilSettled(*to);
        auto line = from->add<QCPItemLine>();
        line->start->setCoords(5, 4);
        line->end->setCoords(5, 6);
        for (int i = 0; i < 30; ++i) // the old stamp ends above the new plot's generation
            from->layer(QStringLiteral("main"))->markDirty();
        waitUntilSettled(*from); // cached bounds: the middle of the old plot
        std::unique_ptr<QCPItemLine> taken = from->take(line);
        QVERIFY(taken);
        for (int i = 0; i < offset; ++i)
            to->layer(QStringLiteral("main"))->markDirty();
        auto* moved = to->add(std::move(taken)).get();
        moved->start->setCoords(5, 4);
        moved->end->setCoords(5, 6);
        const QPointF pos(to->xAxis->coordToPixel(5), to->yAxis->coordToPixel(5));
        QVERIFY2(
            to->layerableListAt(pos, true).contains(moved),
            qPrintable(QStringLiteral("offset %1: culled by its old plot's bounds").arg(offset)));
    }
}

void TestRepaint::staleBoundsFallBack()
{
    auto plot = settledPlot();
    auto* far = new CountingLine(plot.get());
    far->start->setCoords(1000, 4);
    far->end->setCoords(1000, 6);
    waitUntilSettled(*plot); // its cached bounds now say "far outside"

    const QPointF pos(plot->xAxis->coordToPixel(5), plot->yAxis->coordToPixel(5));
    far->start->setCoords(4, 4); // moved under the cursor, no replot in between
    far->end->setCoords(6, 6);
    QVERIFY2(plot->layerableListAt(pos, true).contains(far),
             "a moved item was culled by bounds from before the move");

    // Same through a range change: the item enters the view without a replot.
    auto plot2 = settledPlot();
    auto* line = new CountingLine(plot2.get());
    line->start->setCoords(100, 4);
    line->end->setCoords(100, 6);
    waitUntilSettled(*plot2);
    plot2->xAxis->setRange(95, 105);
    QVERIFY2(plot2->layerableListAt(pos, true).contains(line),
             "an item scrolled into view was culled by bounds from before the range change");
}

void TestRepaint::exportUsesTheIndex()
{
    auto plot = settledPlot();
    auto line = plot->add<QCPItemLine>();
    line->setPen(QPen(Qt::darkRed, 5));
    line->start->setCoords(3, 5);
    line->end->setCoords(7, 5);
    waitUntilSettled(*plot);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(plot->savePng(dir.filePath(QStringLiteral("export.png")), 800, 600, 2.0));

    // The export leaves its own layout in place until the next screen frame (pre-existing), so the
    // screen redraws once before the user can click: the export must not have poisoned the cache.
    plot->replot();
    waitUntilSettled(*plot);
    const QPointF onItem(plot->xAxis->coordToPixel(5), plot->yAxis->coordToPixel(5));
    const QPointF elsewhere(plot->xAxis->coordToPixel(9), plot->yAxis->coordToPixel(9));

    QVERIFY2(plot->layerableListAt(onItem, true).contains(line.get()),
             "an export at another size culled the item on the screen");
    QVERIFY(!plot->layerableListAt(elsewhere, true).contains(line.get()));
}

void TestRepaint::positionFollowingItsOwnItemDoesNotRecurse()
{
    auto plot = settledPlot();
    auto* line = new QCPItemLine(plot.get());
    line->start->setCoords(1, 2);
    line->end->setCoords(3, 4);
    // A position may follow another position of its own item: the marking walk has to terminate.
    QVERIFY(line->end->setParentAnchor(line->start));
    line->start->setCoords(5, 6);
    plot->replot();
    waitUntilSettled(*plot);
    QCOMPARE(line->end->pixelPosition(), line->start->pixelPosition());
}

void TestRepaint::crossItemAnchorCycleIsRefused()
{
    auto plot = settledPlot();
    auto* first = new QCPItemRect(plot.get());
    auto* second = new QCPItemRect(plot.get());
    first->topLeft->setCoords(1, 2);
    first->bottomRight->setCoords(3, 4);
    second->topLeft->setCoords(5, 6);
    second->bottomRight->setCoords(7, 8);
    QVERIFY(first->topLeft->setParentAnchor(second->bottom));
    // second's bottom anchor is computed from second's positions, first's bottom from first's: each
    // following the other is a cycle, and pixelPosition() would recurse until the stack runs out.
    const bool accepted = second->topLeft->setParentAnchor(first->bottom);
    if (accepted)
        (void)first->topLeft->pixelPosition(); // only terminates when the cycle was refused
    QVERIFY(!accepted);
    plot->replot();
    waitUntilSettled(*plot);
}

void TestRepaint::userItemWithoutBoundsIsNeverCulled()
{
    auto plot = settledPlot();
    auto* plain = new PlainItem(plot.get());
    plain->mCentre = QPointF(plot->xAxis->coordToPixel(4), plot->yAxis->coordToPixel(4));
    waitUntilSettled(*plot);
    QVERIFY2(plain->mDrawCalls > 0, "the item was never drawn");
    QVERIFY(!plain->dataExtent().has_value());

    plot->layerableListAt(plain->mCentre, true);
    QVERIFY2(plain->mSelectCalls > 0, "an item without a data extent was culled from hit testing");
}

void TestRepaint::crossAxisAnchorCycleIsRefused()
{
    // pixelPosition() asks a parent for its whole position, so a cycle may switch axes on the way:
    // a's x follows b, and b's y would follow a.
    QCustomPlot plot;
    plot.resize(400, 300);
    auto* a = new QCPItemLine(&plot);
    auto* b = new QCPItemLine(&plot);
    QVERIFY(a->start->setParentAnchorX(b->start));
    const bool accepted = b->start->setParentAnchorY(a->start);
    if (accepted)
        (void)a->start->pixelPosition(); // where the cycle bites
    QVERIFY2(!accepted, "a cycle through the other axis was accepted");
    plot.replot();
}

void TestRepaint::layoutDescribesTheScreenAfterAnExport()
{
    // An export at another size lays the plot out at that size. Once it returns, coordinates, axis
    // rects and hit tests must describe the widget again, not the image.
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.xAxis->setRange(0, 10);
    plot.yAxis->setRange(0, 10);
    plot.replot();
    const QRect screenRect = plot.axisRect()->rect();
    const double screenX = plot.xAxis->coordToPixel(5);

    const QPixmap image = plot.toPixmap(800, 600);
    QCOMPARE(image.size(), QSize(800, 600));
    QCOMPARE(plot.axisRect()->rect(), screenRect);
    QCOMPARE(plot.xAxis->coordToPixel(5), screenX);

    QCPPainter painter;
    QPixmap target(900, 700);
    painter.begin(&target);
    plot.toPainter(&painter, 900, 700);
    painter.end();
    QCOMPARE(plot.axisRect()->rect(), screenRect);
}

void TestRepaint::itemsDefaultToAnnotations()
{
    // Items are annotations: they belong above the data whatever the current layer is, so a label
    // is not hidden by a GPU line drawn after it.
    QCustomPlot plot;
    auto* text = new QCPItemText(&plot);
    auto* line = new QCPItemLine(&plot);
    auto* span = new QCPItemVSpan(&plot);

    QCPLayer* annotations = plot.layer(QStringLiteral("annotations"));
    QVERIFY(annotations);
    QCOMPARE(annotations->mode(), QCPLayer::lmBuffered);
    QVERIFY(plot.layer(QStringLiteral("main"))->index() < annotations->index());
    QVERIFY(annotations->index() < plot.layer(QStringLiteral("axes"))->index());
    QCOMPARE(text->layer(), annotations);
    QCOMPARE(line->layer(), annotations);
    QCOMPARE(span->layer(), annotations);

    QVERIFY(plot.setCurrentLayer(QStringLiteral("grid")));
    auto* graph = plot.addGraph();
    QCOMPARE(graph->layer(), plot.layer(QStringLiteral("grid")));
    auto* other = new QCPItemText(&plot);
    QCOMPARE(other->layer(), annotations);
}

void TestRepaint::setLayerStillWins()
{
    QCustomPlot plot;
    auto* item = new QCPItemLine(&plot);
    QVERIFY(item->setLayer(QStringLiteral("main")));
    QCOMPARE(item->layer(), plot.layer(QStringLiteral("main")));

    // Without the layer, an item falls back to the current layer like any other layerable.
    QVERIFY(plot.setCurrentLayer(QStringLiteral("legend")));
    QVERIFY(plot.removeLayer(plot.layer(QStringLiteral("annotations"))));
    auto* fallback = new QCPItemLine(&plot);
    QCOMPARE(fallback->layer(), plot.currentLayer());
}

void TestRepaint::hiddenPlotLaysOutAtItsSize()
{
    // Qt sends a hidden widget no resize event until it is shown. The plot must still lay out at
    // the size it was given, or coordToPixel() disagrees with width() and with its own exports.
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.replot();
    QCOMPARE(plot.viewport(), QRect(0, 0, 400, 300));
    QVERIFY(plot.axisRect()->outerRect().right() < 400);
    QVERIFY(plot.axisRect()->outerRect().bottom() < 300);
}

namespace
{
// A shown plot that owns a QRhi, or nullptr offscreen: the GPU cases skip there.
QCustomPlot* shownGpuPlot(QWidget& host)
{
    host.resize(400, 300);
    auto* plot = new QCustomPlot(&host);
    plot->setGeometry(0, 0, 400, 300);
    host.show();
    if (!QTest::qWaitForWindowExposed(&host))
        return nullptr;
    QElapsedTimer waited;
    waited.start();
    while (!plot->rhi() && waited.elapsed() < 5000)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QTest::qWait(10);
    }
    return plot->rhi() ? plot : nullptr;
}

// The columns where the middle row of the plot is inked.
QVector<int> inkedColumns(QCustomPlot* plot, const QImage& image)
{
    const double dpr = plot->bufferDevicePixelRatio();
    const int y = qBound(0, int(plot->yAxis->coordToPixel(0.5) * dpr), image.height() - 1);
    const QRect rect = plot->axisRect()->rect();
    const int first = qBound(0, int(rect.left() * dpr) + 2, image.width() - 1);
    const int last = qBound(0, int(rect.right() * dpr) - 2, image.width() - 1);
    QVector<int> columns;
    for (int x = first; x <= last; ++x)
        if (qGray(image.pixel(x, y)) < 110)
            columns.append(x);
    return columns;
}
} // namespace

void TestRepaint::textAboveGraphLine()
{
    // The text is created first; the graph comes later with a thick solid pen crossing it. Items
    // are annotations, so the text's opaque background stays on top of the GPU line.
    QWidget host;
    QCustomPlot* plot = shownGpuPlot(host);
    if (!plot)
        QSKIP("no QRhi on this platform");
    plot->xAxis->setRange(0, 1);
    plot->yAxis->setRange(0, 1);
    const QColor background(40, 160, 90);
    auto text = plot->add<QCPItemText>();
    text->position->setCoords(0.5, 0.5);
    text->setText(QString());
    text->setPositionAlignment(Qt::AlignCenter);
    text->setBrush(background);
    text->setPadding(QMargins(10, 10, 10, 10));

    auto* graph = plot->addGraph();
    graph->setData(QVector<double> { 0, 1 }, QVector<double> { 0.5, 0.5 });
    graph->setPen(QPen(Qt::black, 8));
    plot->replot();
    waitUntilSettled(*plot);

    const QImage image = screenOf(plot);
    const double dpr = plot->bufferDevicePixelRatio();
    const QColor atCrossing = image.pixelColor(qRound(plot->xAxis->coordToPixel(0.5) * dpr),
                                               qRound(plot->yAxis->coordToPixel(0.5) * dpr));
    QVERIFY2(qAbs(atCrossing.red() - background.red()) < 40
                 && qAbs(atCrossing.green() - background.green()) < 40
                 && qAbs(atCrossing.blue() - background.blue()) < 40,
             qPrintable("the line is over the text: " + atCrossing.name()));
}

void TestRepaint::barsStackWithTextAbove()
{
    // A filled bar on the GPU pass, an item above it: the text's opaque background is what the
    // crossing pixel shows, and the bar is still drawn beside it.
    QWidget host;
    QCustomPlot* plot = shownGpuPlot(host);
    if (!plot)
        QSKIP("no QRhi on this platform");
    plot->xAxis->setRange(0, 2);
    plot->yAxis->setRange(0, 2);
    const QColor barColor(30, 90, 200);
    auto* bars = new QCPBars(plot->xAxis, plot->yAxis);
    bars->setData(QVector<double> { 1.0 }, QVector<double> { 1.6 });
    bars->setWidth(0.8);
    bars->setPen(Qt::NoPen);
    bars->setBrush(barColor);
    const QColor background(240, 200, 60);
    auto text = plot->add<QCPItemText>();
    text->position->setCoords(1.0, 1.0);
    text->setText(QString());
    text->setPositionAlignment(Qt::AlignCenter);
    text->setBrush(background);
    text->setPadding(QMargins(8, 8, 8, 8));
    plot->replot();
    waitUntilSettled(*plot);

    const QImage image = screenOf(plot);
    const double dpr = plot->bufferDevicePixelRatio();
    const auto pixelAt = [&](double key, double value)
    {
        return image.pixelColor(qRound(plot->xAxis->coordToPixel(key) * dpr),
                                qRound(plot->yAxis->coordToPixel(value) * dpr));
    };
    const auto near = [](const QColor& a, const QColor& b)
    {
        return qAbs(a.red() - b.red()) < 40 && qAbs(a.green() - b.green()) < 40
            && qAbs(a.blue() - b.blue()) < 40;
    };
    QVERIFY2(near(pixelAt(1.0, 1.0), background),
             qPrintable("the bar is over the text: " + pixelAt(1.0, 1.0).name()));
    QVERIFY2(near(pixelAt(0.72, 1.0), barColor),
             qPrintable("the bar is not drawn beside the text: " + pixelAt(0.72, 1.0).name()));
}

void TestRepaint::textBorderAndBackgroundMatchTheOldLook()
{
    // The item records one text command now; the picture must be the one its QPainter draw made:
    // measured through the label renderer, padded, rotated about the position, border, background,
    // glyphs on top. Drawn here by hand on the same layer, so the same painter state, and compared.
    auto plot = std::make_unique<QCustomPlot>();
    plot->resize(300, 200);
    plot->xAxis->setRange(0, 1);
    plot->yAxis->setRange(0, 1);
    const QColor border(200, 40, 40);
    const QColor background(240, 220, 90);
    const QMargins padding(7, 5, 7, 5);
    const QString words = QStringLiteral("Rotated");
    const Qt::Alignment textAlignment = Qt::AlignTop | Qt::AlignHCenter;
    auto item = plot->add<QCPItemText>();
    item->position->setCoords(0.4, 0.5);
    item->setText(words);
    item->setPositionAlignment(Qt::AlignCenter);
    item->setTextAlignment(textAlignment);
    item->setPen(QPen(border, 3));
    item->setBrush(background);
    item->setPadding(padding);
    item->setRotation(30);
    plot->replot();
    const QImage painted = plot->toPixmap(300, 200).toImage();

    const QPointF position = item->position->pixelPosition();
    const QFont font = item->font();
    item->setVisible(false);
    auto* hook = new DrawHook(plot.get(), QStringLiteral("annotations"));
    hook->onPaint = [&](QCPPainter* painter)
    {
        painter->setAntialiasing(true);
        painter->translate(position);
        painter->rotate(30);
        const QRect glyphs = QCPLabelRenderer::measureRectWith(QCPLabelRenderer::defaultRenderer(),
                                                               font, words,
                                                               int(Qt::TextDontClip | textAlignment));
        QRectF box = QRectF(glyphs).adjusted(-padding.left(), -padding.top(), padding.right(),
                                             padding.bottom());
        const QPointF drawPoint(-box.width() / 2.0, -box.height() / 2.0); // Qt::AlignCenter
        QRect glyphRect(glyphs);
        glyphRect.moveTopLeft(drawPoint.toPoint() + QPoint(padding.left(), padding.top()));
        box.moveTopLeft(drawPoint.toPoint());
        painter->setPen(QPen(border, 3));
        painter->setBrush(background);
        painter->drawRect(box);
        painter->setBrush(Qt::NoBrush);
        QCPLabelRenderer::drawWith(QCPLabelRenderer::defaultRenderer(), painter, glyphRect, font,
                                   Qt::black, words, int(Qt::TextDontClip | textAlignment));
    };
    plot->replot();
    const QImage expected = plot->toPixmap(300, 200).toImage();

    const auto near = [](const QColor& a, const QColor& b)
    {
        return qAbs(a.red() - b.red()) <= 16 && qAbs(a.green() - b.green()) <= 16
            && qAbs(a.blue() - b.blue()) <= 16;
    };
    QVERIFY2(pixelsOfColor(expected, background) > 1000, "the reference picture has no background");
    QVERIFY2(pixelsOfColor(expected, border) > 50, "the reference picture has no border");
    int differing = 0;
    for (int y = 0; y < painted.height(); ++y)
        for (int x = 0; x < painted.width(); ++x)
            if (!near(painted.pixelColor(x, y), expected.pixelColor(x, y)))
                ++differing;
    QVERIFY2(differing == 0, qPrintable(QStringLiteral("%1 pixels differ").arg(differing)));
}

void TestRepaint::dashedLineStacksLikeSolid()
{
    // A pen style must not decide stacking: both cases have the graph on the GPU pass, above the
    // rect item that was created after it.
    const auto darkPixelsOverTheRect = [](Qt::PenStyle style)
    {
        QWidget host;
        QCustomPlot* plot = shownGpuPlot(host);
        if (!plot)
            return -1;
        auto* graph = plot->addGraph();
        QVector<double> keys, values;
        for (int i = 0; i <= 400; ++i)
        {
            keys << i / 400.0;
            values << 0.5;
        }
        graph->setData(keys, values);
        graph->setPen(QPen(Qt::black, 5, style));
        plot->xAxis->setRange(0, 1);
        plot->yAxis->setRange(0, 1);
        auto rect = plot->add<QCPItemRect>();
        rect->setLayer(QStringLiteral("main")); // under the graph, same layer
        rect->topLeft->setCoords(0.2, 0.7);
        rect->bottomRight->setCoords(0.8, 0.3);
        rect->setPen(Qt::NoPen);
        rect->setBrush(QColor(210, 40, 40));
        plot->replot();
        waitUntilSettled(*plot);
        const QImage image = screenOf(plot);
        const double dpr = plot->bufferDevicePixelRatio();
        const QRect box(QPoint(int(plot->xAxis->coordToPixel(0.25) * dpr),
                               int(plot->yAxis->coordToPixel(0.7) * dpr)),
                        QPoint(int(plot->xAxis->coordToPixel(0.75) * dpr),
                               int(plot->yAxis->coordToPixel(0.3) * dpr)));
        int dark = 0;
        int red = 0;
        for (int y = box.top(); y <= box.bottom(); ++y)
            for (int x = box.left(); x <= box.right(); ++x)
            {
                const int gray = qGray(image.pixel(x, y));
                if (gray < 60) // the black line; the brush's red sits at 97
                    ++dark;
                else if (gray > 90 && gray < 150)
                    ++red;
            }
        return red > 1000 ? dark : -1; // the rect was not drawn: the case proves nothing
    };

    const int solid = darkPixelsOverTheRect(Qt::SolidLine);
    if (solid < 0)
        QSKIP("no QRhi on this platform");
    const int dashed = darkPixelsOverTheRect(Qt::DashLine);
    QVERIFY2(solid > 0, "the solid line is not over the rect");
    // Both are drawn on the GPU pass, above the rect created after them; the dashes only cover
    // part of the line, so fewer dark pixels - but never none, which is what a QPainter line under
    // the rect would give.
    QVERIFY2(dashed > 0, "the dashed line is under the rect: a pen style changed stacking");
    QVERIFY(dashed < solid);
}

void TestRepaint::dashPhaseSurvivesPan()
{
    // The dash phase is measured along the drawn line, so a pan restarts it where the visible line
    // starts - exactly what the QPainter export does. Pinning that: after a pan the GPU pattern and
    // the export agree, and the pattern is still a dashed one.
    QWidget host;
    QCustomPlot* plot = shownGpuPlot(host);
    if (!plot)
        QSKIP("no QRhi on this platform");
    auto* graph = plot->addGraph();
    QVector<double> keys, values;
    for (int i = 0; i <= 400; ++i)
    {
        keys << i / 400.0;
        values << 0.5;
    }
    graph->setData(keys, values);
    graph->setPen(QPen(Qt::black, 4, Qt::DashLine));
    plot->xAxis->setRange(0, 1);
    plot->yAxis->setRange(0, 1);
    plot->replot();
    waitUntilSettled(*plot);

    const auto runs = [](const QVector<int>& columns)
    {
        int count = 0;
        for (int i = 1; i < columns.size(); ++i)
            if (columns[i] != columns[i - 1] + 1)
                ++count;
        return count;
    };
    const double step = 7.0 / plot->axisRect()->width();
    plot->xAxis->setRange(step, 1 + step);
    waitUntilSettled(*plot);

    const QVector<int> screenColumns = inkedColumns(plot, screenOf(plot));
    QVERIFY2(screenColumns.size() > 20, "the pan drew no dashed line at all");
    const int screenRuns = runs(screenColumns);
    const int exportRuns = runs(inkedColumns(plot, plot->toPixmap(400, 300).toImage()));
    QVERIFY(screenRuns > 3);
    QVERIFY2(qAbs(screenRuns - exportRuns) <= 1,
             qPrintable(QStringLiteral("after the pan: screen %1 dashes, export %2")
                            .arg(screenRuns)
                            .arg(exportRuns)));
}

void TestRepaint::dashedLineMatchesExportDashes()
{
    QWidget host;
    QCustomPlot* plot = shownGpuPlot(host);
    if (!plot)
        QSKIP("no QRhi on this platform");
    auto* graph = plot->addGraph();
    QVector<double> keys, values;
    for (int i = 0; i <= 400; ++i)
    {
        keys << i / 400.0;
        values << 0.5;
    }
    graph->setData(keys, values);
    graph->setPen(QPen(Qt::black, 4, Qt::DashLine));
    plot->xAxis->setRange(0, 1);
    plot->yAxis->setRange(0, 1);
    plot->replot();
    waitUntilSettled(*plot);

    const auto runs = [](const QVector<int>& columns)
    {
        int count = 0;
        for (int i = 1; i < columns.size(); ++i)
            if (columns[i] != columns[i - 1] + 1)
                ++count;
        return count;
    };
    const int screenRuns = runs(inkedColumns(plot, screenOf(plot)));
    const QImage exported = plot->toPixmap(400, 300).toImage();
    const int exportRuns = runs(inkedColumns(plot, exported));
    QVERIFY(screenRuns > 3);
    // The GPU pattern and the QPainter one start at the same place; allow one dash of slack.
    QVERIFY2(
        qAbs(screenRuns - exportRuns) <= 1,
        qPrintable(QStringLiteral("screen %1 dashes, export %2").arg(screenRuns).arg(exportRuns)));
}

void TestRepaint::solidLinesKeepTheirVertexFormat()
{
    QWidget host;
    QCustomPlot* plot = shownGpuPlot(host);
    if (!plot)
        QSKIP("no QRhi on this platform");
    auto* graph = plot->addGraph();
    QVector<double> keys, values;
    for (int i = 0; i <= 100; ++i)
    {
        keys << i / 100.0;
        values << qSin(i / 12.0);
    }
    graph->setData(keys, values);
    graph->setPen(QPen(Qt::black, 3, Qt::SolidLine));
    plot->replot();
    waitUntilSettled(*plot);

    auto* layer = plot->plottableRhiLayer(graph->layer());
    QVERIFY(layer);
    // A solid line never reaches the dashed pipeline, and its vertices keep the 6-float format.
    QVERIFY(layer->dashedDrawEntries().isEmpty());

    QVERIFY(!layer->drawEntries().isEmpty());
    QCOMPARE(layer->drawEntries().first().fillVertexCount, 0); // a line has no fill
    QVERIFY(layer->drawEntries().first().vertexCount > 0);
}

// A pen style must not move a plottable up or down its layer: the GPU pass has to draw the
// submissions in the order they were added, whatever pipelines they need.
void TestRepaint::mergeStopsAtAnUnlikeNeighbour()
{
    QWidget host;
    QCustomPlot* plot = shownGpuPlot(host);
    if (!plot)
        QSKIP("no QRhi on this platform");
    plot->xAxis->setRange(0, 100);
    plot->yAxis->setRange(0, 50);
    QCPDrawContext context(plot->xAxis, plot->yAxis, plot->axisRect(), nullptr);
    const QBrush brush(QColor(40, 160, 90));
    context.rect(QCP::Data { 10, 10 }, QCP::Data { 40, 40 }, QPen(Qt::black, 2.0), brush);
    context.rect(QCP::Data { 45, 10 }, QCP::Data { 70, 40 }, QPen(Qt::black, 2.0, Qt::DashLine),
                 QBrush(Qt::NoBrush));
    context.rect(QCP::Data { 75, 10 }, QCP::Data { 100, 40 }, QPen(Qt::black, 2.0), brush);

    auto* layer = plot->plottableRhiLayer(plot->currentLayer());
    QVERIFY(layer);
    layer->clear();
    QCPResolveCache cache;
    qcp::QCPRasterFallback fallback;
    qcpReplayWithRhi(context.drawList(), *layer, *plot->scatterRhiLayer(plot->currentLayer()),
                     context.frame(), cache, fallback, plot->bufferDevicePixelRatio(),
                     plot->rhiOutputSize());
    // The dashed submission sits between the two solid ones. Folding the second solid rect into
    // the first would draw it below that dash, so the merge must stop at an unlike neighbour.
    QCOMPARE(int(layer->drawEntries().size()), 2);
    QCOMPARE(int(layer->dashedDrawEntries().size()), 1);
}

void TestRepaint::dashesMergeOnlyWithEqualPhase()
{
    QWidget host;
    QCustomPlot* plot = shownGpuPlot(host);
    if (!plot)
        QSKIP("no QRhi on this platform");
    plot->xAxis->setRange(0, 10);
    plot->yAxis->setRange(0, 10);
    QCPDrawContext context(plot->xAxis, plot->yAxis, plot->axisRect(), nullptr);
    const auto first = std::make_shared<const std::vector<QPointF>>(
        std::initializer_list<QPointF> { { 1, 1 }, { 2, 2 }, { 3, 1 } });
    const auto second = std::make_shared<const std::vector<QPointF>>(
        std::initializer_list<QPointF> { { 1, 3 }, { 2, 4 }, { 3, 3 } });
    const auto third = std::make_shared<const std::vector<QPointF>>(
        std::initializer_list<QPointF> { { 1, 5 }, { 2, 6 }, { 3, 5 } });
    const QPen dash(Qt::black, 2.0, Qt::DashLine);
    context.polyline(first, dash);
    context.polyline(second, dash);
    QPen shifted = dash;
    shifted.setDashOffset(3.0);
    context.polyline(third, shifted);

    auto* layer = plot->plottableRhiLayer(plot->currentLayer());
    QVERIFY(layer);
    layer->clear();
    QCPResolveCache cache;
    qcp::QCPRasterFallback fallback;
    qcpReplayWithRhi(context.drawList(), *layer, *plot->scatterRhiLayer(plot->currentLayer()),
                     context.frame(), cache, fallback, plot->bufferDevicePixelRatio(),
                     plot->rhiOutputSize());
    // The first two dashes share pattern, width and phase: one run. The shifted one restarts the
    // pattern elsewhere, so it cannot join them without changing which pixels are inked.
    QCOMPARE(int(layer->dashedDrawEntries().size()), 2);
    QVERIFY(layer->drawEntries().isEmpty());
}

void TestRepaint::dashedUnderSolidStaysUnder()
{
    QWidget host;
    QCustomPlot* plot = shownGpuPlot(host);
    if (!plot)
        QSKIP("no QRhi on this platform (qoffscreen)");
    plot->resize(400, 300);
    plot->xAxis->setRange(0, 1);
    plot->yAxis->setRange(0, 1);

    // Dashed first, solid second: the solid line crosses the dashed one and must be on top.
    auto* dashed = plot->addGraph();
    QPen alwaysOnDash(Qt::red, 6, Qt::CustomDashLine);
    alwaysOnDash.setDashPattern({ 1000.0, 1.0 }); // on for 6000 px: the crossing is mid-dash
    dashed->setPen(alwaysOnDash);
    dashed->setData(QVector<double> { 0, 1 }, QVector<double> { 0.5, 0.5 });
    auto* solid = plot->addGraph();
    solid->setPen(QPen(Qt::blue, 6, Qt::SolidLine));
    solid->setData(QVector<double> { 0.5, 0.5 }, QVector<double> { 0, 1 });
    waitUntilSettled(*plot);
    const QPoint centre(int(plot->xAxis->coordToPixel(0.5)), int(plot->yAxis->coordToPixel(0.5)));
    const QColor crossing = screenOf(plot).pixelColor(centre);
    QVERIFY2(crossing.blue() > 120 && crossing.red() < 90,
             "a solid line added after a dashed one must stay above it");

    // The other way round: a dash that is on at the crossing stays above the solid line, so the
    // fix is submission order, not "dashed always below solid".
    QWidget host2;
    QCustomPlot* plot2 = shownGpuPlot(host2);
    if (!plot2)
        QSKIP("no QRhi on this platform (qoffscreen)");
    plot2->resize(400, 300);
    plot2->xAxis->setRange(0, 1);
    plot2->yAxis->setRange(0, 1);
    auto* solid2 = plot2->addGraph();
    solid2->setPen(QPen(Qt::blue, 6, Qt::SolidLine));
    solid2->setData(QVector<double> { 0.5, 0.5 }, QVector<double> { 0, 1 });
    auto* dashed2 = plot2->addGraph();
    QPen longDash(Qt::red, 6, Qt::CustomDashLine);
    longDash.setDashPattern({ 1000.0, 1.0 }); // on for 6000 px: the crossing is mid-dash
    dashed2->setPen(longDash);
    dashed2->setData(QVector<double> { 0, 1 }, QVector<double> { 0.5, 0.5 });
    waitUntilSettled(*plot2);
    const QPoint centre2(int(plot2->xAxis->coordToPixel(0.5)),
                         int(plot2->yAxis->coordToPixel(0.5)));
    const QColor crossing2 = screenOf(plot2).pixelColor(centre2);
    QVERIFY2(crossing2.red() > 120 && crossing2.blue() < 90,
             "a dash that is on must stay above a solid line added before it");
}

void TestRepaint::panDoesNotMarkTheLayerDirty()
{
    QWidget host;
    QCustomPlot* plot = shownGpuPlot(host);
    if (!plot)
        QSKIP("no QRhi on this platform");
    auto* graph = plot->addGraph();
    QVector<double> keys, values;
    for (int i = 0; i <= 100; ++i)
    {
        keys << i / 100.0;
        values << qSin(i / 12.0);
    }
    graph->setData(keys, values);
    plot->rescaleAxes();
    plot->replot();
    QTRY_VERIFY_WITH_TIMEOUT(!plot->isRepaintQueued(), 5000);
    auto* layer = plot->plottableRhiLayer(graph->layer());
    QVERIFY(layer);
    // The upload happens in the compositor's frame, not in replot(): wait for it.
    QTRY_VERIFY_WITH_TIMEOUT(layer->solidVertexUploads() > 0, 5000);

    // A pan moves the per-draw offsets only: the vertices are already on the GPU, and
    // uploadResources re-uploads them exactly when isDirty() is set.
    QVERIFY(!layer->isDirty());
    const int uploads = layer->solidVertexUploads();
    QVERIFY(uploads > 0);
    layer->setAllOffsets(7.0f, 0.0f);
    QVERIFY(!layer->isDirty());
}

void TestRepaint::gpuRectFillsBeforeStroking()
{
    QWidget host;
    QCustomPlot* plot = shownGpuPlot(host);
    if (!plot)
        QSKIP("no QRhi on this platform");
    QCPDrawContext context(plot->xAxis, plot->yAxis, plot->axisRect(), nullptr);
    const QBrush brush(QColor(40, 160, 90));
    context.rect(QCP::Data { 10, 10 }, QCP::Data { 80, 60 }, QPen(Qt::black, 2.0), brush);

    auto* layer = plot->plottableRhiLayer(plot->currentLayer());
    QVERIFY(layer);
    layer->clear();
    QCPResolveCache cache;
    qcp::QCPRasterFallback fallback;
    qcpReplayWithRhi(context.drawList(), *layer, *plot->scatterRhiLayer(plot->currentLayer()),
                     context.frame(), cache, fallback, plot->bufferDevicePixelRatio(),
                     plot->rhiOutputSize());
    const auto& entries = layer->drawEntries();
    // One rect is one entry: its fill vertices, then its outline, are one contiguous range that
    // the GPU draws with a single call.
    QCOMPARE(int(entries.size()), 1);
    QVERIFY2(entries.first().fillVertexCount > 0, "the fill must be submitted first");
    QVERIFY(entries.first().vertexCount > entries.first().fillVertexCount);
}

void TestRepaint::unequalFillBetweenBecomesAnImage()
{
    QWidget host;
    QCustomPlot* plot = shownGpuPlot(host);
    if (!plot)
        QSKIP("no QRhi on this platform");
    QCPDrawContext context(plot->xAxis, plot->yAxis, plot->axisRect(), nullptr);
    const auto upper = std::make_shared<const std::vector<QPointF>>(
        std::vector<QPointF> { { 0, 10 }, { 10, 12 }, { 20, 14 } });
    const auto lower = std::make_shared<const std::vector<QPointF>>(
        std::vector<QPointF> { { 0, 0 }, { 10, 1 } });
    context.fillBetween(upper, lower, QBrush(QColor(60, 60, 200)));

    auto* layer = plot->plottableRhiLayer(plot->currentLayer());
    QVERIFY(layer);
    layer->clear();
    QCPResolveCache cache;
    qcp::QCPRasterFallback fallback;
    qcpReplayWithRhi(context.drawList(), *layer, *plot->scatterRhiLayer(plot->currentLayer()),
                     context.frame(), cache, fallback, plot->bufferDevicePixelRatio(),
                     plot->rhiOutputSize());
    // Pairing two series of different lengths is not a ribbon: the fill is rasterised instead, and
    // it still stacks where the command was recorded.
    QVERIFY2(layer->drawEntries().isEmpty(), "a mismatched ribbon was filled on the GPU");
    QCOMPARE(int(layer->imageDrawEntries().size()), 1);
}

namespace
{
// Data units are widget pixels and the axis rect is the whole widget, so a test can name the pixel
// it samples.
void usePixelRanges(QCustomPlot* plot)
{
    plot->xAxis->setVisible(false);
    plot->xAxis2->setVisible(false);
    plot->yAxis->setVisible(false);
    plot->yAxis2->setVisible(false);
    plot->axisRect()->setAutoMargins(QCP::msNone);
    plot->axisRect()->setMargins(QMargins(0, 0, 0, 0));
    plot->xAxis->setRange(0, 400);
    plot->yAxis->setRange(0, 300);
    plot->replot();
    waitUntilSettled(*plot);
}

// The colour at data \a point in \a image.
QColor sampleAt(QCustomPlot* plot, const QImage& image, double x, double y)
{
    const double dpr = plot->bufferDevicePixelRatio();
    return image.pixelColor(int(plot->xAxis->coordToPixel(x) * dpr),
                            int(plot->yAxis->coordToPixel(y) * dpr));
}

// The colour at widget pixel (\a x, \a y) in \a image, whatever the ranges are now.
QColor pixelAt(QCustomPlot* plot, const QImage& image, int x, int y)
{
    const double dpr = plot->bufferDevicePixelRatio();
    return image.pixelColor(int(x * dpr), int(y * dpr));
}

QImage flatTexture(const QColor& colour)
{
    QImage texture(4, 4, QImage::Format_ARGB32_Premultiplied);
    texture.fill(colour);
    return texture;
}
} // namespace

// A brush style must not decide stacking: a texture rect and a polyline keep their submission
// order, exactly as two solid commands do.
void TestRepaint::textureBrushStacksLikeSolid()
{
    QWidget host;
    QCustomPlot* plot = shownGpuPlot(host);
    if (!plot)
        QSKIP("no QRhi on this platform");
    usePixelRanges(plot);
    auto* layer = plot->plottableRhiLayer(plot->currentLayer());
    QVERIFY(layer);
    const QImage texture = flatTexture(QColor(240, 140, 20));
    const auto crossingColour = [&](bool rectLast)
    {
        QCPDrawContext context(plot->xAxis, plot->yAxis, plot->axisRect(), nullptr);
        const auto line = std::make_shared<const std::vector<QPointF>>(
            std::vector<QPointF> { { 60, 150 }, { 340, 150 } });
        const auto addRect = [&context, &texture]
        {
            context.rect(QCP::Data { 60, 60 }, QCP::Data { 340, 240 }, QPen(Qt::NoPen),
                         QBrush(texture));
        };
        if (rectLast)
        {
            context.polyline(line, QPen(Qt::blue, 8));
            addRect();
        }
        else
        {
            addRect();
            context.polyline(line, QPen(Qt::blue, 8));
        }
        layer->clear();
        QCPResolveCache cache;
        qcp::QCPRasterFallback fallback;
        qcpReplayWithRhi(context.drawList(), *layer, *plot->scatterRhiLayer(plot->currentLayer()),
                         context.frame(), cache, fallback, plot->bufferDevicePixelRatio(),
                         plot->rhiOutputSize());
        return sampleAt(plot, screenOf(plot), 200, 150);
    };

    const QColor lineOnTop = crossingColour(false);
    QVERIFY2(
        lineOnTop.blue() > 120 && lineOnTop.red() < 90,
        qPrintable(
            QStringLiteral("the polyline lost to the texture rect: %1").arg(lineOnTop.name())));
    const QColor rectOnTop = crossingColour(true);
    QVERIFY2(rectOnTop.red() > 180 && rectOnTop.green() > 90 && rectOnTop.blue() < 70,
             qPrintable(QStringLiteral("a rect added last did not cover the polyline: %1")
                            .arg(rectOnTop.name())));
}

// A free-form fill has no fan centre, so the GPU used to leave it to the painter pass, which put it
// under everything. It keeps its place now, and its stroke still paints over it.
void TestRepaint::freePolygonFillKeepsItsPlace()
{
    QWidget host;
    QCustomPlot* plot = shownGpuPlot(host);
    if (!plot)
        QSKIP("no QRhi on this platform");
    usePixelRanges(plot);
    QCPDrawContext context(plot->xAxis, plot->yAxis, plot->axisRect(), nullptr);
    const std::vector<QCP::Position> points { QCP::Data { 50, 50 },   QCP::Data { 250, 50 },
                                              QCP::Data { 250, 250 }, QCP::Data { 200, 250 },
                                              QCP::Data { 200, 100 }, QCP::Data { 100, 100 },
                                              QCP::Data { 100, 250 }, QCP::Data { 50, 250 } };
    context.polygon(points, QPen(Qt::NoPen), QBrush(QColor(20, 160, 80)));
    const auto line = std::make_shared<const std::vector<QPointF>>(
        std::vector<QPointF> { { 30, 175 }, { 110, 175 } });
    context.polyline(line, QPen(Qt::blue, 8));

    auto* layer = plot->plottableRhiLayer(plot->currentLayer());
    QVERIFY(layer);
    layer->clear();
    QCPResolveCache cache;
    qcp::QCPRasterFallback fallback;
    qcpReplayWithRhi(context.drawList(), *layer, *plot->scatterRhiLayer(plot->currentLayer()),
                     context.frame(), cache, fallback, plot->bufferDevicePixelRatio(),
                     plot->rhiOutputSize());
    const QImage image = screenOf(plot);

    const QColor inside = sampleAt(plot, image, 75, 120);
    QVERIFY2(inside.green() > 120 && inside.red() < 90,
             qPrintable(
                 QStringLiteral("the fill is missing inside the polygon: %1").arg(inside.name())));
    const QColor crossing = sampleAt(plot, image, 75, 175);
    QVERIFY2(
        crossing.blue() > 120 && crossing.red() < 90,
        qPrintable(
            QStringLiteral("the stroke did not paint over the fill: %1").arg(crossing.name())));
    const QColor concavity = sampleAt(plot, image, 150, 175);
    QVERIFY2(concavity.red() > 200 && concavity.green() > 200 && concavity.blue() > 200,
             qPrintable(QStringLiteral("the concavity was filled: %1").arg(concavity.name())));
}

namespace
{
// Three overlapping "bars" as one recording: their fills and outlines are six commands, so the
// merge has to keep the order the painter would have used.
class OverlappingBars : public QCPDrawn<QCPAbstractItem>
{
public:
    explicit OverlappingBars(QCustomPlot* plot) : QCPDrawn<QCPAbstractItem>(plot)
    {
        setClipAxisRect(plot->axisRect());
        // An item with no position reports no extent, and the index then never asks it to draw.
        createPosition(QStringLiteral("anchor"))->setCoords(45, 100);
    }

    using QCPDrawn<QCPAbstractItem>::draw;

    void draw(QCPDrawContext& context) const override
    {
        const QColor fills[] = { QColor(210, 40, 40), QColor(40, 170, 70), QColor(210, 40, 40) };
        for (int i = 0; i < 3; ++i)
        {
            const double left = 45.0 + 100.0 * i;
            context.rect(QCP::Data { left, 100.0 }, QCP::Data { left + 150.0, 250.0 },
                         QPen(QColor(20, 20, 20), 6), QBrush(fills[i]));
        }
    }
};
} // namespace

// Three overlapping bars merge into one vertex range: the range still keeps the painter's order, so
// a later bar's fill covers the earlier bar's outline and its own outline paints over that fill.
void TestRepaint::overlappingMergedBarsKeepPaintersOrder()
{
    QWidget host;
    QCustomPlot* plot = shownGpuPlot(host);
    if (!plot)
        QSKIP("no QRhi on this platform");
    usePixelRanges(plot); // data units are widget pixels, so the samples below name their pixel

    const QColor firstFill(210, 40, 40);
    const QColor secondFill(40, 170, 70);
    const QColor outline(20, 20, 20);
    auto* bars = new OverlappingBars(plot); // widget x 45..195, 145..295, 245..395
    plot->replot();
    waitUntilSettled(*plot);

    auto* layer = plot->plottableRhiLayer(bars->layer());
    QVERIFY(layer);
    // Six solid commands with the same clip, offset and alpha, their vertices one after the other:
    // the whole stack is one submission, which is what makes its vertex order decide the pixels.
    QCOMPARE(int(layer->drawEntries().size()), 1);
    QVERIFY(layer->dashedDrawEntries().isEmpty());

    const QImage screen = screenOf(plot);
    const QImage exported = plot->toPixmap(400, 300).toImage();
    const auto near = [](const QColor& a, const QColor& b)
    {
        return qAbs(a.red() - b.red()) <= 40 && qAbs(a.green() - b.green()) <= 40
            && qAbs(a.blue() - b.blue()) <= 40;
    };
    const auto disagreement = [&](int x, int y, const QColor& expected)
    {
        const QColor onScreen = pixelAt(plot, screen, x, y);
        const QColor painted = pixelAt(plot, exported, x, y);
        if (!near(onScreen, expected) || !near(onScreen, painted))
            return QStringLiteral("screen %1, export %2, expected %3")
                .arg(onScreen.name(), painted.name(), expected.name());
        return QString();
    };

    const struct
    {
        int x;
        int y;
        QColor colour;
        const char* what;
    } samples[] = {
        { 195, 175, secondFill, "the second bar's fill over the first bar's outline" },
        { 295, 175, firstFill, "the third bar's fill over the second bar's outline" },
        { 145, 175, outline, "the second bar's outline over the first bar's fill" },
        { 245, 175, outline, "the third bar's outline over the second bar's fill" },
    };

    for (const auto& sample : samples)
    {
        const QString failure = disagreement(sample.x, sample.y, sample.colour);
        QVERIFY2(failure.isEmpty(),
                 qPrintable(QStringLiteral("%1 at %2,%3: %4")
                                .arg(QLatin1String(sample.what))
                                .arg(sample.x)
                                .arg(sample.y)
                                .arg(failure)));
    }
}

namespace
{
// A gradient the GPU cannot draw: it is rasterised once and reused.
QBrush gradientBrush()
{
    QLinearGradient gradient(0, 0, 1, 0);
    gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
    gradient.setColorAt(0.0, QColor(220, 40, 40));
    gradient.setColorAt(1.0, QColor(40, 40, 220));
    return QBrush(gradient);
}

// A brush the GPU cannot fill in one colour, painting flat: the style makes it a raster, the stops
// decide what the pixels are.
QBrush flatGradientBrush(const QColor& colour)
{
    QLinearGradient gradient(0, 0, 1, 0);
    gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
    gradient.setColorAt(0.0, colour);
    gradient.setColorAt(1.0, colour);
    return QBrush(gradient);
}
} // namespace

// The image lives in the layer's cache: a pan moves the quad, it does not rasterise or upload
// again.
void TestRepaint::pannedFallbackDoesNotReupload()
{
    QWidget host;
    QCustomPlot* plot = shownGpuPlot(host);
    if (!plot)
        QSKIP("no QRhi on this platform");
    usePixelRanges(plot);
    QCPDrawContext context(plot->xAxis, plot->yAxis, plot->axisRect(), nullptr);
    context.rect(QCP::Data { 60, 60 }, QCP::Data { 340, 240 }, QPen(Qt::NoPen), gradientBrush());

    auto* layer = plot->plottableRhiLayer(plot->currentLayer());
    QVERIFY(layer);
    QCPResolveCache cache;
    qcp::QCPRasterFallback fallback;
    const auto replay = [&]
    {
        qcpReplayWithRhi(context.drawList(), *layer, *plot->scatterRhiLayer(plot->currentLayer()),
                         context.frame(), cache, fallback, plot->bufferDevicePixelRatio(),
                         plot->rhiOutputSize());
    };

    layer->clear();
    replay();
    QCOMPARE(int(layer->imageDrawEntries().size()), 1);
    QCOMPARE(fallback.rasterCount(), 1);
    QImage frame = screenOf(plot);
    QCOMPARE(layer->imageUploads(), 1);

    // Pan: the recording is replayed with a new offset, the image is the same one.
    layer->clear();
    layer->setAllOffsets(7.0f, 0.0f);
    replay();
    QCOMPARE(int(layer->imageDrawEntries().size()), 1);
    QCOMPARE(fallback.rasterCount(), 1);
    frame = screenOf(plot);
    QCOMPARE(layer->imageUploads(), 1);
    const QColor shifted = sampleAt(plot, frame, 200, 150);
    QVERIFY2(shifted.red() > 80 && shifted.blue() > 80,
             qPrintable(QStringLiteral("the panned image left the frame: %1").arg(shifted.name())));
}

// The GPU image of a gradient matches what the painter produces for the same recording.
void TestRepaint::gradientFillMatchesExport()
{
    QWidget host;
    QCustomPlot* plot = shownGpuPlot(host);
    if (!plot)
        QSKIP("no QRhi on this platform");
    usePixelRanges(plot);
    QCPDrawContext context(plot->xAxis, plot->yAxis, plot->axisRect(), nullptr);
    context.rect(QCP::Data { 60, 60 }, QCP::Data { 340, 240 }, QPen(Qt::NoPen), gradientBrush());

    auto* layer = plot->plottableRhiLayer(plot->currentLayer());
    QVERIFY(layer);
    layer->clear();
    QCPResolveCache cache;
    qcp::QCPRasterFallback fallback;
    qcpReplayWithRhi(context.drawList(), *layer, *plot->scatterRhiLayer(plot->currentLayer()),
                     context.frame(), cache, fallback, plot->bufferDevicePixelRatio(),
                     plot->rhiOutputSize());
    const QImage screen = screenOf(plot);

    // The export paints the same recording through QPainter, which is what a non-GPU target does.
    auto* hook = new DrawHook(plot, QStringLiteral("overlay"));
    hook->onPaint = [&](QCPPainter* painter)
    { qcpReplayWithPainter(context.drawList(), *painter, context.frame()); };
    const QImage exported = plot->toPixmap(400, 300).toImage();
    delete hook;

    const double dpr = plot->bufferDevicePixelRatio();
    const double ratio = exported.height() / 300.0;
    for (int x : { 100, 200, 300 })
    {
        const QColor gpu = sampleAt(plot, screen, x, 150);
        const QColor painter = exported.pixelColor(int(x * ratio), int(150 * ratio));
        const bool close = qAbs(gpu.red() - painter.red()) <= 12
            && qAbs(gpu.green() - painter.green()) <= 12 && qAbs(gpu.blue() - painter.blue()) <= 12;
        QVERIFY2(close,
                 qPrintable(QStringLiteral("x=%1: GPU %2, export %3 (dpr %4)")
                                .arg(x)
                                .arg(gpu.name(), painter.name())
                                .arg(dpr)));
    }
}

// An image is never bigger than the viewport, and a layer keeps at most four of them: the rest is
// painted by the layer's QPainter pass, with one warning per plot.
void TestRepaint::rasterFallbackIsCapped()
{
    QWidget host;
    QCustomPlot* plot = shownGpuPlot(host);
    if (!plot)
        QSKIP("no QRhi on this platform");
    usePixelRanges(plot);
    auto* layer = plot->plottableRhiLayer(plot->currentLayer());
    QVERIFY(layer);
    const QSize viewport = plot->rhiOutputSize();
    const auto replayWith
        = [&](QCPDrawContext& context, QCPResolveCache& cache, qcp::QCPRasterFallback& fallback)
    {
        qcpReplayWithRhi(context.drawList(), *layer, *plot->scatterRhiLayer(plot->currentLayer()),
                         context.frame(), cache, fallback, plot->bufferDevicePixelRatio(),
                         viewport);
    };

    // A command far bigger than the viewport is clipped to it before it is rasterised.
    QCPDrawContext oversized(plot->xAxis, plot->yAxis, plot->axisRect(), nullptr);
    oversized.rect(QCP::Data { -900, -900 }, QCP::Data { 1200, 1100 }, QPen(Qt::NoPen),
                   gradientBrush());
    layer->clear();
    QCPResolveCache cache;
    qcp::QCPRasterFallback fallback;
    replayWith(oversized, cache, fallback);
    QCOMPARE(int(layer->imageDrawEntries().size()), 1);
    const QSize device = layer->imageDrawEntries().first().deviceSize;
    QVERIFY2(device.width() <= viewport.width() && device.height() <= viewport.height(),
             qPrintable(QStringLiteral("the image is %1x%2, the viewport %3x%4")
                            .arg(device.width())
                            .arg(device.height())
                            .arg(viewport.width())
                            .arg(viewport.height())));

    // Six fallback commands in one layer: four images, two painter draws.
    QCPDrawContext many(plot->xAxis, plot->yAxis, plot->axisRect(), nullptr);
    const std::array<QRectF, 6> boxes { QRectF(10, 20, 110, 110),   QRectF(145, 20, 110, 110),
                                        QRectF(280, 20, 110, 110),  QRectF(10, 170, 110, 110),
                                        QRectF(145, 170, 110, 110), QRectF(280, 170, 110, 110) };
    for (const QRectF& box : boxes)
        many.rect(QCP::Data { box.left(), box.top() }, QCP::Data { box.right(), box.bottom() },
                  QPen(Qt::NoPen), gradientBrush());

    layer->clear();
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("rasterised styles")));
    replayWith(many, cache, fallback);
    QCOMPARE(int(layer->imageDrawEntries().size()), 4);
    int painterDrawn = 0;
    for (std::size_t i = 0; i < many.drawList().commands.size(); ++i)
        if (fallback.painterHandled(many.drawList().id, quint64(i)))
            ++painterDrawn;
    QCOMPARE(painterDrawn, 2);

    // The painter pass paints exactly those two, so the split is where the cap says it is.
    QImage painted(viewport, QImage::Format_ARGB32_Premultiplied);
    painted.fill(Qt::white);
    {
        QCPPainter painter(&painted);
        painter.setAntialiasing(true);
        qcpReplayWithPainter(many.drawList(), painter, many.frame(), QCPReplayKinds::PainterOnly,
                             &fallback);
    }
    // White is (255, 255, 255): anything darker is paint, at the box's place in the frame.
    const auto inked = [&](const QRectF& box)
    {
        const QColor colour = sampleAt(plot, painted, box.center().x(), box.center().y());
        return colour.red() + colour.green() + colour.blue() < 720;
    };
    QVERIFY2(!inked(boxes[0]), "the painter painted an image the GPU already draws");
    QVERIFY2(inked(boxes[4]) && inked(boxes[5]), "the painter skipped the commands past the cap");

    // One warning per plot, not per fallback: the next frame gets a fresh fallback and is silent.
    static int warnings = 0;
    warnings = 0;
    const QtMessageHandler previous = qInstallMessageHandler(
        +[](QtMsgType type, const QMessageLogContext&, const QString& message)
        {
            if (type == QtWarningMsg && message.contains(QStringLiteral("rasterised styles")))
                ++warnings;
        });
    layer->clear();
    qcp::QCPRasterFallback fresh;
    replayWith(many, cache, fresh);
    qInstallMessageHandler(previous);
    QCOMPARE(warnings, 0);
}

// A zoom changes the transform, not the recording: the image covers the viewport either way, but
// its pixels are the fill of the new range.
void TestRepaint::zoomedFallbackIsRepainted()
{
    QWidget host;
    QCustomPlot* plot = shownGpuPlot(host);
    if (!plot)
        QSKIP("no QRhi on this platform");
    usePixelRanges(plot);
    auto* layer = plot->plottableRhiLayer(plot->currentLayer());
    QVERIFY(layer);
    QCPDrawContext recording(plot->xAxis, plot->yAxis, plot->axisRect(), nullptr);
    // Far wider than the viewport: any range crops it to the same device rect.
    recording.rect(QCP::Data { 0, 60 }, QCP::Data { 400, 240 }, QPen(Qt::NoPen), gradientBrush());

    QCPResolveCache cache;
    qcp::QCPRasterFallback fallback;
    const auto replayThrough = [&](const QCPDrawFrame& frame)
    {
        layer->clear();
        qcpReplayWithRhi(recording.drawList(), *layer, *plot->scatterRhiLayer(plot->currentLayer()),
                         frame, cache, fallback, plot->bufferDevicePixelRatio(),
                         plot->rhiOutputSize());
        return pixelAt(plot, screenOf(plot), 100, 150);
    };

    const QColor wide = replayThrough(recording.frame());
    QCOMPARE(fallback.rasterCount(), 1);

    // Twice the resolution over the same pixels: the fill's gradient is squeezed, so the pixel's
    // colour changes while the cropped bounds keep their rect.
    plot->xAxis->setRange(0, 200);
    plot->replot();
    waitUntilSettled(*plot);
    QCPDrawContext zoomed(plot->xAxis, plot->yAxis, plot->axisRect(), nullptr);
    const QColor squeezed = replayThrough(zoomed.frame());
    QCOMPARE(fallback.rasterCount(), 2);
    QVERIFY2(squeezed != wide,
             qPrintable(QStringLiteral("the zoom re-used the old bitmap: %1 then %2")
                            .arg(wide.name(), squeezed.name())));
}

// A shape clipped by the viewport keeps the same image rect when the range moves: its pixels are
// the only thing that says the image is stale.
void TestRepaint::clippedFallbackFollowsTheShape()
{
    QWidget host;
    QCustomPlot* plot = shownGpuPlot(host);
    if (!plot)
        QSKIP("no QRhi on this platform");
    usePixelRanges(plot);
    auto* layer = plot->plottableRhiLayer(plot->currentLayer());
    QVERIFY(layer);
    QCPDrawContext recording(plot->xAxis, plot->yAxis, plot->axisRect(), nullptr);
    // A U wider than the viewport, with its concavity at widget x 150..250.
    const std::vector<QCP::Position> points { QCP::Data { -200, 50 }, QCP::Data { 600, 50 },
                                              QCP::Data { 600, 250 }, QCP::Data { 250, 250 },
                                              QCP::Data { 250, 100 }, QCP::Data { 150, 100 },
                                              QCP::Data { 150, 250 }, QCP::Data { -200, 250 } };
    recording.polygon(points, QPen(Qt::NoPen), QBrush(QColor(20, 160, 80)));

    QCPResolveCache cache;
    qcp::QCPRasterFallback fallback;
    const auto replayThrough = [&](const QCPDrawFrame& frame)
    {
        layer->clear();
        qcpReplayWithRhi(recording.drawList(), *layer, *plot->scatterRhiLayer(plot->currentLayer()),
                         frame, cache, fallback, plot->bufferDevicePixelRatio(),
                         plot->rhiOutputSize());
        return screenOf(plot);
    };

    const QColor before = pixelAt(plot, replayThrough(recording.frame()), 100, 150);
    QVERIFY2(before.green() > 120 && before.red() < 90,
             qPrintable(QStringLiteral("the fill is missing at pixel 100: %1").arg(before.name())));

    // The same width, moved right: the shape slides 80 pixels left, so pixel 100 enters the
    // concavity. The cropped image rect is unchanged.
    plot->xAxis->setRange(80, 480);
    plot->replot();
    waitUntilSettled(*plot);
    QCPDrawContext moved(plot->xAxis, plot->yAxis, plot->axisRect(), nullptr);
    const QColor after = pixelAt(plot, replayThrough(moved.frame()), 100, 150);
    QVERIFY2(fallback.rasterCount() == 2, "the moved range re-used the old bitmap");
    QVERIFY2(
        after.red() > 200 && after.green() > 200 && after.blue() > 200,
        qPrintable(
            QStringLiteral("pixel 100 left the shape but is still filled: %1").arg(after.name())));
}

// The command a recording destroyed had the address its successor now holds: the second image must
// be its own, not the first's.
void TestRepaint::twoRecordingsDoNotShareAnImage()
{
    QWidget host;
    QCustomPlot* plot = shownGpuPlot(host);
    if (!plot)
        QSKIP("no QRhi on this platform");
    usePixelRanges(plot);
    auto* layer = plot->plottableRhiLayer(plot->currentLayer());
    QVERIFY(layer);
    QCPResolveCache cache;
    qcp::QCPRasterFallback fallback;
    // The context is created and destroyed inside the call: the next one gets its freed memory.
    const auto replayInNewRecording = [&](const QColor& colour)
    {
        QCPDrawContext context(plot->xAxis, plot->yAxis, plot->axisRect(), nullptr);
        context.rect(QCP::Data { 60, 60 }, QCP::Data { 340, 240 }, QPen(Qt::NoPen),
                     flatGradientBrush(colour));
        layer->clear();
        qcpReplayWithRhi(context.drawList(), *layer, *plot->scatterRhiLayer(plot->currentLayer()),
                         context.frame(), cache, fallback, plot->bufferDevicePixelRatio(),
                         plot->rhiOutputSize());
        return pixelAt(plot, screenOf(plot), 200, 150);
    };

    const QColor first = replayInNewRecording(Qt::red);
    QCOMPARE(fallback.rasterCount(), 1);
    QVERIFY2(first.red() > 180 && first.green() < 80,
             qPrintable(QStringLiteral("the first recording is not red: %1").arg(first.name())));

    const QColor second = replayInNewRecording(Qt::green);
    QCOMPARE(fallback.rasterCount(), 2);
    QVERIFY2(second.green() > 120 && second.red() < 80,
             qPrintable(QStringLiteral("the second recording shows the first one's image: %1")
                            .arg(second.name())));
}

// A layer whose image pipeline failed to build cannot take one more image: the one predicate the
// replay asks must say so, and the painter pass paints the command.
void TestRepaint::brokenImagePipelineFallsBackToThePainter()
{
    QWidget host;
    QCustomPlot* plot = shownGpuPlot(host);
    if (!plot)
        QSKIP("no QRhi on this platform");
    usePixelRanges(plot);
    auto* layer = plot->plottableRhiLayer(plot->currentLayer());
    QVERIFY(layer);
    QCPDrawContext context(plot->xAxis, plot->yAxis, plot->axisRect(), nullptr);
    context.rect(QCP::Data { 60, 60 }, QCP::Data { 340, 240 }, QPen(Qt::NoPen), gradientBrush());

    layer->clear();
    layer->breakImagePipelineForTest();
    QVERIFY2(!layer->canAddImage(), "a broken image pipeline still takes an image");

    QCPResolveCache cache;
    qcp::QCPRasterFallback fallback;
    qcpReplayWithRhi(context.drawList(), *layer, *plot->scatterRhiLayer(plot->currentLayer()),
                     context.frame(), cache, fallback, plot->bufferDevicePixelRatio(),
                     plot->rhiOutputSize());
    QVERIFY2(layer->imageDrawEntries().isEmpty(), "the layer took an image it cannot draw");
    QVERIFY2(fallback.painterHandled(context.drawList().id, 0),
             "the painter did not get the command the layer refused");

    QImage painted(plot->rhiOutputSize(), QImage::Format_ARGB32_Premultiplied);
    painted.fill(Qt::white);
    {
        QCPPainter painter(&painted);
        qcpReplayWithPainter(context.drawList(), painter, context.frame(),
                             QCPReplayKinds::PainterOnly, &fallback);
    }
    const QColor colour = pixelAt(plot, painted, 200, 150);
    QVERIFY2(
        colour.red() + colour.green() + colour.blue() < 720,
        qPrintable(
            QStringLiteral("the painter pass skipped the refused command: %1").arg(colour.name())));
}

// The image left the layer: its texture must not sit on the GPU for the rest of the plot's life,
// and the frame that draws no image at all is the one that has to notice.
void TestRepaint::unusedFallbackTexturesAreReleased()
{
    QWidget host;
    QCustomPlot* plot = shownGpuPlot(host);
    if (!plot)
        QSKIP("no QRhi on this platform");
    usePixelRanges(plot);
    auto* layer = plot->plottableRhiLayer(plot->currentLayer());
    QVERIFY(layer);
    QCPDrawContext withImage(plot->xAxis, plot->yAxis, plot->axisRect(), nullptr);
    withImage.rect(QCP::Data { 60, 60 }, QCP::Data { 340, 240 }, QPen(Qt::NoPen), gradientBrush());
    const auto line = std::make_shared<const std::vector<QPointF>>(
        std::vector<QPointF> { { 60, 150 }, { 340, 150 } });
    QCPDrawContext withoutImage(plot->xAxis, plot->yAxis, plot->axisRect(), nullptr);
    withoutImage.polyline(line, QPen(Qt::blue, 4));

    QCPResolveCache cache;
    qcp::QCPRasterFallback fallback;
    const auto replay = [&](const QCPDrawContext& context)
    {
        layer->clear();
        qcpReplayWithRhi(context.drawList(), *layer, *plot->scatterRhiLayer(plot->currentLayer()),
                         context.frame(), cache, fallback, plot->bufferDevicePixelRatio(),
                         plot->rhiOutputSize());
        return screenOf(plot);
    };

    QVERIFY(!replay(withImage).isNull());
    QCOMPARE(layer->imageTextureCount(), 1);

    // A frame with no image at all: the sweep is not behind the image path.
    QVERIFY(!replay(withoutImage).isNull());
    QCOMPARE(layer->imageTextureCount(), 0);
}
