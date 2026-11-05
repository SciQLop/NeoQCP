#include "test-drawable.h"

#include <drawable.h>
#include <misuse.h>
#include <painting/draw-context.h>
#include <painting/line-extruder.h>
#include <qcustomplot.h>

#include <QtWidgets/qtestsupport_widgets.h> // QTest::qWaitForWindowExposed
#include <memory>
#include <vector>

namespace
{
int gMisuseCount = 0;

void countMisuse(const char*, const char*)
{
    ++gMisuseCount;
}

int gAxisMessages = 0;
QtMessageHandler gPreviousHandler = nullptr;

void countAxisMessages(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    if (type == QtDebugMsg && message.contains(QLatin1String("no key or value axis")))
    {
        ++gAxisMessages;
        return; // never hand our own message to QtTest, it would count as unexpected
    }
    if (gPreviousHandler)
        gPreviousHandler(type, context, message);
}

//! A drawn item: one polygon and, when asked, a text or a painter() escape hatch.
class TestShape : public QCPDrawn<QCPAbstractItem>
{
public:
    explicit TestShape(QCustomPlot* plot) : QCPDrawn<QCPAbstractItem>(plot)
    {
        mAnchor = createPosition(QStringLiteral("anchor"));
        mAnchor->setCoords(3, 3);
        setClipAxisRect(plot->axisRect());
    }

    // The old painter virtual is final in the adapter: keep it visible so overriding the new
    // draw() does not hide it (-Woverloaded-virtual).
    using QCPDrawn<QCPAbstractItem>::draw;

    void draw(QCPDrawContext& context) const override
    {
        ++drawCalls;
        if (contentChangeInsideDraw)
            const_cast<TestShape*>(this)->contentChanged();
        if (bars > 0)
        {
            for (int i = 0; i < bars; ++i)
            {
                const double key = 1.0 + 8.0 * i / bars;
                context.rect(QCP::Data { key, 1.0 }, QCP::Data { key + 8.0 / bars, 3.0 },
                             QPen(Qt::black), QBrush(Qt::NoBrush));
            }
            return;
        }
        if (withPolyline)
            context.polyline(mLine, QPen(Qt::black, 2));
        if (escapeHatch)
        {
            context.painter(
                [](QCPPainter& painter, const QCPTransform&)
                {
                    painter.setPen(Qt::black);
                    painter.drawLine(QPointF(0, 0), QPointF(5, 5));
                });
            return;
        }
        const QCP::Position corners[]
            = { QCP::Data { 2, 2 }, QCP::Data { 6, 2 }, QCP::Data { 6, 6 }, QCP::Data { 2, 6 } };
        context.polygon(corners, QPen(Qt::black), QBrush(fill));
        if (withText)
            context.text(QCP::Data { 4, 4 }, QStringLiteral("x"), QCPTextStyle { });
    }

    void touch() { contentChanged(); }

    //! Test hook: how many times the resolve cache mapped a recording again.
    [[nodiscard]] quint64 remaps() const { return mCore.resolveRemapCount(); }

    QCPItemPosition* mAnchor = nullptr;
    std::shared_ptr<const std::vector<QPointF>> mLine
        = std::make_shared<const std::vector<QPointF>>(
            std::initializer_list<QPointF> { { 1, 1 }, { 3, 5 }, { 5, 2 }, { 7, 6 }, { 9, 1 } });
    QColor fill = QColor(Qt::darkRed);
    int bars = 0;
    bool withText = false;
    bool withPolyline = false;
    bool escapeHatch = false;
    bool contentChangeInsideDraw = false;
    mutable int drawCalls = 0;
};

struct Fixture
{
    Fixture()
    {
        plot = std::make_unique<QCustomPlot>();
        plot->resize(300, 200);
        plot->xAxis->setRange(0, 10);
        plot->yAxis->setRange(0, 10);
    }

    //! Shows the plot and reports whether a real QRhi came up (the GPU path needs one).
    bool hasRhi()
    {
        plot->show();
        if (!QTest::qWaitForWindowExposed(plot.get()))
            return false;
        QCoreApplication::processEvents();
        return plot->rhi() != nullptr;
    }

    std::unique_ptr<QCustomPlot> plot;
};

QColor pixelAt(const QImage& frame, QCustomPlot* plot, double key, double value)
{
    const double dpr = frame.devicePixelRatio();
    const QPointF p(plot->xAxis->coordToPixel(key), plot->yAxis->coordToPixel(value));
    return frame.pixelColor(qRound(p.x() * dpr), qRound(p.y() * dpr));
}

bool isBlue(const QColor& c)
{
    return c.blue() > 200 && c.red() < 60 && c.green() < 60;
}
} // namespace

void TestDrawable::init()
{
    gMisuseCount = 0;
    gAxisMessages = 0;
    qcp::setMisuseHandler(countMisuse);
    gPreviousHandler = qInstallMessageHandler(countAxisMessages);
}

void TestDrawable::cleanup()
{
    qInstallMessageHandler(gPreviousHandler);
    gPreviousHandler = nullptr;
    qcp::setMisuseHandler(nullptr);
}

void TestDrawable::panDoesNotReRecord()
{
    Fixture f;
    auto* shape = new TestShape(f.plot.get());
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 1);

    f.plot->xAxis->setRange(2, 12);
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 1);
}

void TestDrawable::zoomDoesNotReRecord()
{
    Fixture f;
    auto* shape = new TestShape(f.plot.get());
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 1);

    f.plot->xAxis->setRange(4, 6);
    f.plot->yAxis->setRange(4, 6);
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 1);
}

void TestDrawable::setterReRecordsOnce()
{
    Fixture f;
    auto* shape = new TestShape(f.plot.get());
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 1);

    shape->touch();
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 2);

    f.plot->xAxis->setRange(2, 12); // a view change is not a content change
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 2);
}

void TestDrawable::unrelatedMarkDoesNotReRecord()
{
    Fixture f;
    auto* touched = new TestShape(f.plot.get());
    auto* other = new TestShape(f.plot.get());
    f.plot->replot();
    QCOMPARE(touched->drawCalls, 1);
    QCOMPARE(other->drawCalls, 1);

    touched->touch();
    f.plot->replot();
    QCOMPARE(touched->drawCalls, 2);
    QCOMPARE(other->drawCalls, 1);
}

void TestDrawable::viewEpochReRecordsEscapeHatch()
{
    Fixture f;
    f.plot->show(); // a hidden widget holds its resize event back
    QVERIFY(QTest::qWaitForWindowExposed(f.plot.get()));
    auto* plain = new TestShape(f.plot.get());
    auto* hatch = new TestShape(f.plot.get());
    hatch->escapeHatch = true;
    f.plot->replot();
    QCOMPARE(plain->drawCalls, 1);
    QCOMPARE(hatch->drawCalls, 1);

    const quint64 epoch = f.plot->viewEpoch();
    f.plot->resize(400, 300);
    QVERIFY2(f.plot->viewEpoch() != epoch, "a resize must bump the view epoch");
    f.plot->replot();
    QCOMPARE(plain->drawCalls, 1);
    QCOMPARE(hatch->drawCalls, 2);
}

void TestDrawable::contentChangedInsideDrawIsAMisuse()
{
    Fixture f;
    auto* shape = new TestShape(f.plot.get());
    shape->contentChangeInsideDraw = true;
    f.plot->replot();
    QCOMPARE(gMisuseCount, 1);

    f.plot->replot();
    QCOMPARE(gMisuseCount, 1);
    QCOMPARE(shape->drawCalls, 1); // no generation bump: nothing re-recorded
}

void TestDrawable::missingAxisHidesWithAMessage()
{
    Fixture f;
    auto* shape = new TestShape(f.plot.get());
    shape->mAnchor->setAxes(nullptr, nullptr);
    f.plot->replot();
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 0);
    QCOMPARE(gAxisMessages, 1); // once per object, not once per frame
}

void TestDrawable::panReusesResolvedGeometry()
{
    Fixture f;
    if (!f.hasRhi())
        QSKIP("No QRhi available - the extruded vertices are a GPU path only");
    auto* shape = new TestShape(f.plot.get());
    shape->bars = 10000;
    f.plot->replot();
    const quint64 before = QCPLineExtruder::extruderCallCount();
    QVERIFY2(before > 0, "the first frame must extrude the bars");

    f.plot->xAxis->setRange(1, 11); // a pan: same span, one pixel offset
    f.plot->replot();
    QCOMPARE(QCPLineExtruder::extruderCallCount(), before);

    f.plot->xAxis->setRange(4, 6); // a zoom: a new scale, so nothing can be shifted
    f.plot->replot();
    QVERIFY(QCPLineExtruder::extruderCallCount() > before);
}

void TestDrawable::exportDoesNotPoisonTheResolveCache()
{
    Fixture f;
    if (!f.hasRhi())
        QSKIP("No QRhi available - the resolve cache is a GPU path only");
    auto* shape = new TestShape(f.plot.get());
    shape->withPolyline = true;
    f.plot->replot();
    const quint64 remaps = shape->remaps();
    QVERIFY2(remaps > 0, "the first frame must resolve the line");

    f.plot->toPixmap(800, 600); // an export: its own transform, its own pixels
    QCOMPARE(shape->remaps(), remaps);

    f.plot->replot(); // the screen frame still resolves with the screen transform
    QCOMPARE(shape->remaps(), remaps);
}

void TestDrawable::drawnPolygonStacksAboveAnEarlierGpuGraph()
{
    Fixture f;
    if (!f.hasRhi())
        QSKIP("No QRhi available - this checks the order of two GPU submissions");
    auto* graph = f.plot->addGraph();
    graph->setData(QVector<double> { 1, 9 }, QVector<double> { 4, 4 });
    graph->setPen(QPen(Qt::red, 24));
    auto* shape = new TestShape(f.plot.get()); // created after the graph: it must land above it
    shape->fill = QColor(Qt::blue);
    f.plot->replot(QCustomPlot::rpImmediateRefresh);

    const QColor colour = pixelAt(f.plot->grabFramebuffer(), f.plot.get(), 4, 4);
    QVERIFY2(isBlue(colour), qPrintable(QString("the item drew as %1").arg(colour.name())));
}
