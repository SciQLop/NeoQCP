#include "test-repaint.h"
#include "plottables/plottable-multigraph.h"
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
