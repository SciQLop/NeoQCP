#include "test-repaint.h"
#include "plottables/plottable-multigraph.h"
#include "datasource/soa-multi-datasource.h"
#include "plottables/plottable-bars.h"
#include "items/item-ellipse.h"
#include "items/item-line.h"
#include "items/item-richtext.h"
#include "items/item-pixmap.h"
#include "items/item-rect.h"
#include "items/item-text.h"
#include "items/item-tracer.h"
#include "layoutelements/layoutelement-colorscale.h"
#include "layoutelements/layoutelement-legend.h"
#include "layoutelements/layoutelement-textelement.h"
#include "polar/layoutelement-angularaxis.h"
#include "polar/polargrid.h"
#include "plottables/plottable-bars.h"
#include "plottables/plottable-colormap.h"
#include "plottables/plottable-colormap2.h"
#include "plottables/plottable-curve.h"
#include "plottables/plottable-errorbar.h"
#include "plottables/plottable-financial.h"
#include "plottables/plottable-graph2.h"
#include "plottables/plottable-group.h"
#include "plottables/plottable-histogram.h"
#include "plottables/plottable-histogram2d.h"
#include "plottables/plottable-statisticalbox.h"
#include "plottables/plottable-waterfall.h"
#include <QTemporaryDir>
#include <QtWidgets/qtestsupport_widgets.h>
#include <algorithm>
#include <functional>
#include <memory>

namespace
{
std::unique_ptr<QCustomPlot> settledPlot()
{
    auto plot = std::make_unique<QCustomPlot>();
    plot->resize(400, 300);
    plot->addGraph()->setData(QVector<double>{0, 10}, QVector<double>{0, 10});
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

protected:
    void applyDefaultAntialiasingHint(QCPPainter*) const override { }
    void draw(QCPPainter*) override
    {
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
        return {};
    }
    QCPRange getValueRange(bool& found, QCP::SignDomain = QCP::sdBoth,
                           const QCPRange& = QCPRange()) const override
    {
        found = false;
        return {};
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
    return {}; // the caller checks the file
}

int gMisuses = 0;
void countMisuse(const char*, const char*) { ++gMisuses; }
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
    QCOMPARE(replottingAtEntry, (QList<bool>{false, false}));
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
    mg->setData(std::vector<double>{0, 10}, std::vector<std::vector<double>>{{2, 2}});
    mg->setComponentPens({QPen(Qt::red, 4)});
    plot.xAxis->setRange(0, 10);
    plot.yAxis->setRange(0, 10);
    plot.replot();
    plot.setDataSwapDebounceMs(10000);
    using Source = QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>;
    mg->setDataSource(std::make_shared<Source>(std::vector<double>{0, 10},
                                               std::vector<std::vector<double>>{{8, 8}}));
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

// The legend is on its own buffer ("axes" and "legend"): marking the graph's layer does not redraw it.
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

    auto g = plot.add<QCPGraph>(QVector<double>{0, 1}, QVector<double>{0, 1}).setName("Bx");
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
        { "axis.setLabelRenderer", [](QCustomPlot& plot) { plot.xAxis->setLabelRenderer(nullptr); } },
    };
    return cases;
}

const QVector<AxisAppearanceCase>& equalValueCases()
{
    static const QVector<AxisAppearanceCase> cases{
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
    static const QVector<PlottableAppearanceCase> cases{
        { "graph.setLineStyle", [](Plot plot) -> QCPAbstractPlottable* { return plot.graph(0); },
          [](QCPAbstractPlottable* p) { static_cast<QCPGraph*>(p)->setLineStyle(QCPGraph::lsStepLeft); } },
        { "graph.setScatterSkip", [](Plot plot) -> QCPAbstractPlottable* { return plot.graph(0); },
          [](QCPAbstractPlottable* p) { static_cast<QCPGraph*>(p)->setScatterSkip(2); } },
        { "graph.setAdaptiveSampling", [](Plot plot) -> QCPAbstractPlottable* { return plot.graph(0); },
          [](QCPAbstractPlottable* p) { static_cast<QCPGraph*>(p)->setAdaptiveSampling(false); } },
        { "graph.setPen", [](Plot plot) -> QCPAbstractPlottable* { return plot.graph(0); },
          [](QCPAbstractPlottable* p) { p->setPen(QPen(Qt::darkGreen, 3)); } },
        { "graph.setBrush", [](Plot plot) -> QCPAbstractPlottable* { return plot.graph(0); },
          [](QCPAbstractPlottable* p) { p->setBrush(QBrush(Qt::yellow)); } },
        { "graph.setName", [](Plot plot) -> QCPAbstractPlottable* { return plot.graph(0); },
          [](QCPAbstractPlottable* p) { p->setName(QStringLiteral("renamed")); } },
        { "curve.setPen", [](Plot plot) -> QCPAbstractPlottable* { return new QCPCurve(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { p->setPen(QPen(Qt::darkCyan, 3)); } },
        { "curve.setBrush", [](Plot plot) -> QCPAbstractPlottable* { return new QCPCurve(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { p->setBrush(QBrush(Qt::cyan)); } },
        { "curve.setScatterStyle", [](Plot plot) -> QCPAbstractPlottable* { return new QCPCurve(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p)
          { static_cast<QCPCurve*>(p)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 6)); } },
        { "bars.setWidth", [](Plot plot) -> QCPAbstractPlottable* { return new QCPBars(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPBars*>(p)->setWidth(0.5); } },
        { "bars.setWidthType", [](Plot plot) -> QCPAbstractPlottable* { return new QCPBars(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPBars*>(p)->setWidthType(QCPBars::wtAbsolute); } },
        { "bars.setBaseValue", [](Plot plot) -> QCPAbstractPlottable* { return new QCPBars(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPBars*>(p)->setBaseValue(0.5); } },
        { "bars.setStackingGap", [](Plot plot) -> QCPAbstractPlottable* { return new QCPBars(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPBars*>(p)->setStackingGap(2); } },
        { "financial.setWidth", [](Plot plot) -> QCPAbstractPlottable* { return new QCPFinancial(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPFinancial*>(p)->setWidth(0.3); } },
        { "financial.setChartStyle", [](Plot plot) -> QCPAbstractPlottable* { return new QCPFinancial(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPFinancial*>(p)->setChartStyle(QCPFinancial::csOhlc); } },
        { "financial.setPenNegative", [](Plot plot) -> QCPAbstractPlottable* { return new QCPFinancial(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPFinancial*>(p)->setPenNegative(QPen(Qt::magenta, 2)); } },
        { "statisticalbox.setWidth", [](Plot plot) -> QCPAbstractPlottable* { return new QCPStatisticalBox(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPStatisticalBox*>(p)->setWidth(0.6); } },
        { "statisticalbox.setWhiskerPen", [](Plot plot) -> QCPAbstractPlottable* { return new QCPStatisticalBox(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPStatisticalBox*>(p)->setWhiskerPen(QPen(Qt::magenta, 2)); } },
        { "statisticalbox.setMedianPen", [](Plot plot) -> QCPAbstractPlottable* { return new QCPStatisticalBox(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPStatisticalBox*>(p)->setMedianPen(QPen(Qt::darkGreen, 3)); } },
        { "errorbars.setWhiskerWidth", [](Plot plot) -> QCPAbstractPlottable* { return new QCPErrorBars(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPErrorBars*>(p)->setWhiskerWidth(10); } },
        { "errorbars.setErrorType", [](Plot plot) -> QCPAbstractPlottable* { return new QCPErrorBars(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPErrorBars*>(p)->setErrorType(QCPErrorBars::etKeyError); } },
        { "colormap.setDataRange", [](Plot plot) -> QCPAbstractPlottable* { return new QCPColorMap(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPColorMap*>(p)->setDataRange(QCPRange(0, 5)); } },
        { "colormap.setDataScaleType", [](Plot plot) -> QCPAbstractPlottable* { return new QCPColorMap(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPColorMap*>(p)->setDataScaleType(QCPAxis::stLogarithmic); } },
        { "colormap.setGradient", [](Plot plot) -> QCPAbstractPlottable* { return new QCPColorMap(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPColorMap*>(p)->setGradient(QCPColorGradient::gpJet); } },
        { "colormap.setInterpolate", [](Plot plot) -> QCPAbstractPlottable* { return new QCPColorMap(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPColorMap*>(p)->setInterpolate(false); } },
        { "colormap.setTightBoundary", [](Plot plot) -> QCPAbstractPlottable* { return new QCPColorMap(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPColorMap*>(p)->setTightBoundary(true); } },
        { "colormap2.setGapThreshold", [](Plot plot) -> QCPAbstractPlottable* { return new QCPColorMap2(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPColorMap2*>(p)->setGapThreshold(0.5); } },
        { "colormap2.setContourPen", [](Plot plot) -> QCPAbstractPlottable* { return new QCPColorMap2(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPColorMap2*>(p)->setContourPen(QPen(Qt::magenta, 2)); } },
        { "colormap2.setAutoContourLevels", [](Plot plot) -> QCPAbstractPlottable* { return new QCPColorMap2(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPColorMap2*>(p)->setAutoContourLevels(8); } },
        { "histogram.setBins", [](Plot plot) -> QCPAbstractPlottable* { return new QCPHistogram(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPHistogram*>(p)->setBins(12); } },
        { "histogram.setStyle", [](Plot plot) -> QCPAbstractPlottable* { return new QCPHistogram(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPHistogram*>(p)->setStyle(QCPHistogram::sStep); } },
        { "histogram2d.setContourPen", [](Plot plot) -> QCPAbstractPlottable* { return new QCPHistogram2D(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPHistogram2D*>(p)->setContourPen(QPen(Qt::magenta, 2)); } },
        { "multigraph.setLineStyle", [](Plot plot) -> QCPAbstractPlottable* { return new QCPMultiGraph(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPMultiGraph*>(p)->setLineStyle(QCPMultiGraph::lsNone); } },
        { "multigraph.setAdaptiveSampling", [](Plot plot) -> QCPAbstractPlottable* { return new QCPMultiGraph(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPMultiGraph*>(p)->setAdaptiveSampling(false); } },
        { "waterfall.setOffsetMode", [](Plot plot) -> QCPAbstractPlottable* { return new QCPWaterfallGraph(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPWaterfallGraph*>(p)->setOffsetMode(QCPWaterfallGraph::omCustom); } },
        { "waterfall.setGain", [](Plot plot) -> QCPAbstractPlottable* { return new QCPWaterfallGraph(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPWaterfallGraph*>(p)->setGain(2.0); } },
        { "waterfall.setNormalize", [](Plot plot) -> QCPAbstractPlottable* { return new QCPWaterfallGraph(plot.xAxis, plot.yAxis); },
          [](QCPAbstractPlottable* p) { static_cast<QCPWaterfallGraph*>(p)->setNormalize(false); } },
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
    static const char* const names[] = {"graph.setPen",  "graph.setName",  "curve.setPen",
                                        "bars.setWidth", "colormap.setGradient",
                                        "multigraph.setLineStyle", "waterfall.setGain"};
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
                            .arg(drawn.x()).arg(drawn.y()).arg(drawn.width()).arg(drawn.height())
                            .arg(after.x()).arg(after.y()).arg(after.width()).arg(after.height())));
}

void TestRepaint::multiGraphLineStyleResetsComponents()
{
    // A component may carry its own style; the global setter must still reset it, so comparing the
    // global member alone is not enough to decide "nothing changed".
    QCustomPlot plot;
    auto* mg = new QCPMultiGraph(plot.xAxis, plot.yAxis);
    auto source = std::make_shared<QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double>{0.0, 1.0}, std::vector<std::vector<double>>{{0.0, 1.0}});
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
    for (auto* bars : {first, second})
    {
        bars->setBarsGroup(group);
        bars->setData({1.0, 2.0, 3.0}, {0.5, 1.0, 0.5});
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
    static const QVector<RemainingCase> cases{
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
        { "legend.setFont",
          [](QCustomPlot& plot) -> QObject*
          {
              return plot.legend;
          },
          [](QObject* o) { static_cast<QCPLegend*>(o)->setFont(QFont(QStringLiteral("Helvetica"), 17)); } },
        { "legend.setIconSize",
          [](QCustomPlot& plot) -> QObject*
          {
              return plot.legend;
          },
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
          { static_cast<QCPPolarAxisAngular*>(o)->setTickLabelMode(QCPPolarAxisAngular::lmRotated); } },
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
          [](QObject* o) { static_cast<QCPPolarGrid*>(o)->setRadialZeroLinePen(QPen(Qt::magenta, 2)); } },
        { "layerable.setAntialiased",
          [](QCustomPlot& plot) -> QObject*
          {
              return plot.graph(0);
          },
          [](QObject* o) { static_cast<QCPLayerable*>(o)->setAntialiased(false); } },
        { "span.setPen",
          [](QCustomPlot& plot) -> QObject*
          {
              return new QCPItemHSpan(&plot);
          },
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
          { static_cast<QCPItemLine*>(o)->setHead(QCPLineEnding(QCPLineEnding::esDisc, 12, 12)); } },
        { "richText.setHtml",
          [](QCustomPlot& plot) -> QObject*
          {
              auto* item = new QCPItemRichText(&plot);
              item->position->setCoords(1, 1);
              return item;
          },
          [](QObject* o) { static_cast<QCPItemRichText*>(o)->setHtml(QStringLiteral("<b>x</b>")); } },
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
          [](QObject* o) { static_cast<QCPPolarAxisAngular*>(o)->setNumberFormat(QStringLiteral("f")); } },
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
        { "legend.setBorderPen",
          [](QCustomPlot& plot) -> QObject* { return plot.legend; },
          [](QObject* o) { static_cast<QCPLegend*>(o)->setBorderPen(QPen(Qt::darkBlue, 2)); } },
        { "polarAxis.setTickLabelColor",
          [](QCustomPlot& plot) -> QObject* { return new QCPPolarAxisAngular(&plot); },
          [](QObject* o)
          { static_cast<QCPPolarAxisAngular*>(o)->setTickLabelColor(Qt::darkBlue); } },
        { "polarAxis.setBasePen",
          [](QCustomPlot& plot) -> QObject* { return new QCPPolarAxisAngular(&plot); },
          [](QObject* o) { static_cast<QCPPolarAxisAngular*>(o)->setBasePen(QPen(Qt::darkBlue, 3)); } },
        { "polarGrid.setAngularPen",
          [](QCustomPlot& plot) -> QObject*
          {
              auto* polar = new QCPPolarAxisAngular(&plot);
              auto* grid = new QCPPolarGrid(polar);
              return grid;
          },
          [](QObject* o) { static_cast<QCPPolarGrid*>(o)->setAngularPen(QPen(Qt::darkGreen, 3)); } },
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
                            .arg(drawn.x()).arg(drawn.y()).arg(drawn.width()).arg(drawn.height())
                            .arg(after.x()).arg(after.y()).arg(after.width()).arg(after.height())));
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
    QVERIFY2(qAbs(corner.hue() - QColor(Qt::darkMagenta).hue()) < 20,
             qPrintable(corner.name()));
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
