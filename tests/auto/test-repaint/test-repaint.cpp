#include "test-repaint.h"
#include <QTemporaryDir>
#include <QtWidgets/qtestsupport_widgets.h>
#include <functional>
#include <memory>

namespace
{
std::unique_ptr<QCustomPlot> settledPlot()
{
    auto plot = std::make_unique<QCustomPlot>();
    plot->resize(400, 300);
    plot->addGraph()->setData(QVector<double>{0, 10}, QVector<double>{0, 10});
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
                    plot->graph(0)->markDirty();
            });
    plot->replot();
    QCOMPARE(count, 1);
    QVERIFY(plot->isRepaintQueued());
    QTRY_COMPARE(count, 2);
    for (int i = 0; i < 3; ++i)
        QCoreApplication::processEvents();
    QCOMPARE(count, 2);
    QCOMPARE(replottingAtEntry, (QList<bool>{false, false}));
}

void TestRepaint::markDuringDrawRepaintsNextTurn()
{
    auto plot = settledPlot();
    auto* hook = new DrawHook(plot.get(), QStringLiteral("overlay"));
    int draws = 0;
    hook->onDraw = [&]
    {
        if (++draws == 1)
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
    plot->graph(0)->markDirty();
    QCoreApplication::processEvents();
    QCOMPARE(after.count(), 0);
    QVERIFY(!plot->isRepaintQueued());
    plot->show();
    QTRY_VERIFY(after.count() > 0);
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
    plot->graph(0)->setPen(QPen(Qt::red, 4));
    plot->replot();
    auto* pending = new PendingStub(plot->xAxis, plot->yAxis);
    plot->setDataSwapDebounceMs(10000);
    plot->requestDataSwap();
    const QColor newColor(0, 0, 255);
    plot->graph(0)->setPen(QPen(newColor, 4));
    plot->graph(0)->markDirty();

    const QImage image = exportWith(*plot, how, dir.path());

    QCOMPARE(pending->commits, 1);
    QVERIFY(!plot->isRepaintQueued());
    if (how == QLatin1String("savePdf"))
        QVERIFY(QFileInfo(dir.path() + QStringLiteral("/plot.pdf")).size() > 0);
    else
    {
        QVERIFY(!image.isNull());
        QVERIFY(pixelsOfColor(image, newColor) > 50);
        QCOMPARE(pixelsOfColor(image, Qt::red), 0);
    }
}

