#include "test-drawable.h"

#include <drawable.h>
#include <misuse.h>
#include <painting/draw-context.h>
#include <qcustomplot.h>

#include <memory>

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
        context.polygon(corners, QPen(Qt::black), QBrush(Qt::darkRed));
        if (withText)
            context.text(QCP::Data { 4, 4 }, QStringLiteral("x"), QCPTextStyle { });
    }

    void touch() { contentChanged(); }

    QCPItemPosition* mAnchor = nullptr;
    bool withText = false;
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

    std::unique_ptr<QCustomPlot> plot;
};
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
