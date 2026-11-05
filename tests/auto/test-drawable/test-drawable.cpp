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

    enum Mode
    {
        Polygon,
        Mixed,
        Anchored,
        ZeroValue,
        Disjoint,
        Image,
        Ratio
    };

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
        if (mode == Mixed)
        {
            const QCP::Position corners[] = { QCP::Data { 2, 2 }, QCP::DataXRatioY { 6, 0.5 } };
            context.polygon(corners, QPen(Qt::black), QBrush(fill));
            return;
        }
        if (mode == Disjoint)
        {
            context.rect(QCP::Data { 1, 1 }, QCP::Data { 2, 2 }, QPen(Qt::black), QBrush(fill));
            context.rect(QCP::Data { 8, 8 }, QCP::Data { 9, 9 }, QPen(Qt::black), QBrush(fill));
            return;
        }
        if (mode == Anchored)
        {
            const QCP::Position corners[]
                = { QCP::AnchorOffset { mAnchor, QCP::PixelOffset { } },
                    QCP::AnchorOffset { mAnchor, QCP::PixelOffset { 20, 20 } } };
            context.polygon(corners, QPen(Qt::black), QBrush(fill));
            return;
        }
        if (mode == Ratio) // data keys, axis-ratio values: the value side cannot be indexed
        {
            const QCP::Position corners[]
                = { QCP::DataXRatioY { 2, 0.2 }, QCP::DataXRatioY { 6, 0.2 },
                    QCP::DataXRatioY { 6, 0.8 }, QCP::DataXRatioY { 2, 0.8 } };
            context.polygon(corners, QPen(Qt::black, 2), QBrush(Qt::NoBrush));
            return;
        }
        if (mode == Image) // a raster box over data (3,3) to (5,5): its outline is a known rect
        {
            QImage image(2, 2, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::darkGray);
            context.image(QCP::Data { 3, 3 }, QCP::Data { 5, 5 }, image);
            return;
        }
        const double valueTop = mode == ZeroValue ? 0.0 : 2.0;
        const QCP::Position corners[] = { QCP::Data { 2, valueTop }, QCP::Data { 6, valueTop },
                                          QCP::Data { 6, 6 }, QCP::Data { 2, 6 } };
        context.polygon(corners, QPen(Qt::black), QBrush(fill));
        if (withText)
            context.text(QCP::Data { 4, 4 }, QStringLiteral("x"), QCPTextStyle { });
    }

    void touch() { contentChanged(); }

    //! Test hook: how many times the resolve cache mapped a recording again.
    [[nodiscard]] quint64 remaps() const { return mCore.resolveRemapCount(); }

    //! The adapter's dataExtent(), which the layer index reads.
    [[nodiscard]] std::optional<QCPItemExtent> extent() const { return dataExtent(); }

    QCPItemPosition* mAnchor = nullptr;
    std::shared_ptr<const std::vector<QPointF>> mLine
        = std::make_shared<const std::vector<QPointF>>(
            std::initializer_list<QPointF> { { 1, 1 }, { 3, 5 }, { 5, 2 }, { 7, 6 }, { 9, 1 } });
    QColor fill = QColor(Qt::darkRed);
    Mode mode = Polygon;
    int bars = 0;
    bool withText = false;
    bool withPolyline = false;
    bool escapeHatch = false;
    bool contentChangeInsideDraw = false;
    mutable int drawCalls = 0;
};

//! A drawn plottable: one data-space line, so the default icon has something to fit.
class TestLine : public QCPDrawn<QCPAbstractPlottable>
{
public:
    TestLine(QCPAxis* keyAxis, QCPAxis* valueAxis)
            : QCPDrawn<QCPAbstractPlottable>(keyAxis, valueAxis)
    {
    }

    using QCPDrawn<QCPAbstractPlottable>::draw;

    void draw(QCPDrawContext& context) const override
    {
        context.polyline(mLine, QPen(Qt::black, 2));
    }

    //! The adapter's old virtual, which the legend calls.
    void paintIcon(QCPPainter* painter, const QRectF& rect) const { drawLegendIcon(painter, rect); }

    std::shared_ptr<const std::vector<QPointF>> mLine
        = std::make_shared<const std::vector<QPointF>>(
            std::initializer_list<QPointF> { { 1, 1 }, { 9, 9 } });
};

//! A drawn plottable whose legend icon calls a setter: a misuse, like contentChanged() in draw().
class TestIconSetter : public QCPDrawn<QCPAbstractPlottable>
{
public:
    TestIconSetter(QCPAxis* keyAxis, QCPAxis* valueAxis)
            : QCPDrawn<QCPAbstractPlottable>(keyAxis, valueAxis)
    {
    }

    using QCPDrawn<QCPAbstractPlottable>::draw;
    using QCPDrawn<QCPAbstractPlottable>::drawLegendIcon; // the painter overload stays reachable

    void draw(QCPDrawContext& context) const override
    {
        ++drawCalls;
        context.polyline(mLine, QPen(Qt::black, 2));
    }

    void drawLegendIcon(QCPDrawContext& context) const override
    {
        ++iconCalls;
        context.polyline(mLine, QPen(Qt::black, 2));
        const_cast<TestIconSetter*>(this)->contentChanged();
    }

    //! The adapter's old virtual, which the core's icon path hangs off.
    void paintIcon(QCPPainter* painter, const QRectF& rect) const { drawLegendIcon(painter, rect); }

    mutable int drawCalls = 0;
    mutable int iconCalls = 0;
    std::shared_ptr<const std::vector<QPointF>> mLine
        = std::make_shared<const std::vector<QPointF>>(
            std::initializer_list<QPointF> { { 1, 1 }, { 9, 9 } });
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

void TestDrawable::contentChangedInsideLegendIconIsAMisuse()
{
    Fixture f;
    auto* line = new TestIconSetter(f.plot->xAxis, f.plot->yAxis);
    QImage image(100, 50, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QCPPainter painter(&image);
    line->paintIcon(&painter, QRectF(0, 0, 100, 50));
    line->paintIcon(&painter, QRectF(0, 0, 100, 50)); // the icon is already recorded
    painter.end();

    QCOMPARE(line->iconCalls, 1);
    QCOMPARE(gMisuseCount, 1); // once, not once per icon paint

    f.plot->replot(); // a bumped generation would re-record the screen list too
    QCOMPARE(line->drawCalls, 1);
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

    const QPixmap exported = f.plot->toPixmap(800, 600); // its own transform, its own pixels
    QVERIFY(!exported.isNull());
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

void TestDrawable::defaultHitTestMatchesGeometry()
{
    Fixture f;
    auto* shape = new TestShape(f.plot.get()); // filled polygon over data (2,2)-(6,6)
    f.plot->replot();
    const auto at = [&](double key, double value)
    {
        return QCP::Pixels { f.plot->xAxis->coordToPixel(key), f.plot->yAxis->coordToPixel(value) };
    };

    const std::optional<double> inside = shape->hitTest(at(4, 4), nullptr);
    QVERIFY(inside.has_value());
    QCOMPARE(*inside, 0.0); // inside a visible fill

    const double edge = f.plot->xAxis->coordToPixel(2);
    const std::optional<double> near
        = shape->hitTest(QCP::Pixels { edge - 3, at(0, 4).y }, nullptr);
    QVERIFY2(near.has_value(), "3 px from the outline is inside the selection tolerance");
    QVERIFY2(*near > 1.0 && *near < 4.0, qPrintable(QString("distance %1").arg(*near)));

    QVERIFY(!shape->hitTest(QCP::Pixels { edge - 60, at(0, 4).y }, nullptr).has_value());
}

void TestDrawable::hitTestBesideTextIsTheGap()
{
    Fixture f;
    auto* shape = new TestShape(f.plot.get());
    shape->mode = TestShape::Image; // a box over data (3,3) to (5,5)
    f.plot->replot();

    const double left = f.plot->xAxis->coordToPixel(3);
    const double right = f.plot->xAxis->coordToPixel(5);
    const double top = f.plot->yAxis->coordToPixel(5);
    const double bottom = f.plot->yAxis->coordToPixel(3);
    const double middle = (top + bottom) / 2.0;
    QVERIFY(right > left && bottom > top);

    const std::optional<double> beside = shape->hitTest(QCP::Pixels { right + 3, middle }, nullptr);
    QVERIFY2(beside.has_value(), "3 px beside the box at mid-height is inside the tolerance");
    QVERIFY2(*beside > 2.0 && *beside < 4.0, qPrintable(QString("distance %1").arg(*beside)));
    QVERIFY(!shape->hitTest(QCP::Pixels { right + 30, middle }, nullptr).has_value());
}

void TestDrawable::pixelAnchoredShapeIsHittable()
{
    Fixture f;
    auto* shape = new TestShape(f.plot.get());
    shape->mode = TestShape::Ratio; // data keys, ratio values: the value side is unbounded
    f.plot->replot();

    const double middle = f.plot->xAxis->coordToPixel(4);
    const double ratioY
        = f.plot->axisRect()->top() + 0.2 * f.plot->axisRect()->height(); // the top outline
    const std::optional<double> ratioHit = shape->hitTest(QCP::Pixels { middle, ratioY }, nullptr);
    QVERIFY2(ratioHit.has_value(), "an unbounded value side must not reject the click");
    QVERIFY(*ratioHit < 1.0);

    // The same on a log value axis: the box reaches zero, so that side cannot be indexed either.
    shape->mode = TestShape::ZeroValue; // data (2,0) to (6,6)
    shape->touch();
    f.plot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    f.plot->yAxis->setRange(0.1, 100);
    f.plot->replot();

    const std::optional<double> logHit = shape->hitTest(
        QCP::Pixels { f.plot->xAxis->coordToPixel(4), f.plot->yAxis->coordToPixel(6) }, nullptr);
    QVERIFY2(logHit.has_value(), "a box reaching zero on a log axis must not reject the click");
    QVERIFY(*logHit < 1.0);
}

void TestDrawable::defaultDataRangeFollowsTheExtentRules()
{
    Fixture f;
    auto* shape = new TestShape(f.plot.get());
    f.plot->replot();

    const std::optional<QCPDataBox> polygon = shape->dataRange();
    QVERIFY(polygon);
    QCOMPARE(polygon->keyLower, 2.0);
    QCOMPARE(polygon->keyUpper, 6.0);
    QCOMPARE(polygon->valueLower, 2.0);
    QCOMPARE(polygon->valueUpper, 6.0);
    const std::optional<QCPItemExtent> extent = shape->extent();
    QVERIFY(extent);
    QCOMPARE(extent->box.keyLower, polygon->keyLower);
    QCOMPARE(extent->box.valueUpper, polygon->valueUpper);

    shape->mode = TestShape::Disjoint; // two commands far apart: the box holds both, not their gap
    shape->touch();
    f.plot->replot();
    const std::optional<QCPDataBox> disjoint = shape->dataRange();
    QVERIFY(disjoint);
    QCOMPARE(disjoint->keyLower, 1.0);
    QCOMPARE(disjoint->keyUpper, 9.0);
    QCOMPARE(disjoint->valueLower, 1.0);
    QCOMPARE(disjoint->valueUpper, 9.0);

    shape->mode = TestShape::Mixed; // a ratio on the value axis: that side is unbounded
    shape->touch();
    f.plot->replot();
    const std::optional<QCPDataBox> mixed = shape->dataRange();
    QVERIFY(mixed);
    QCOMPARE(mixed->keyLower, 2.0);
    QVERIFY(std::isinf(mixed->valueLower) && mixed->valueLower < 0);

    shape->mode = TestShape::Anchored; // an anchored position: both sides unbounded
    shape->touch();
    f.plot->replot();
    QVERIFY(!shape->dataRange().has_value());
    QVERIFY(!shape->extent().has_value());

    shape->mode = TestShape::ZeroValue; // a box reaching zero cannot be indexed on a log axis
    shape->touch();
    f.plot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    f.plot->yAxis->setRange(0.1, 100);
    f.plot->replot();
    const std::optional<QCPDataBox> logarithmic = shape->dataRange();
    QVERIFY(logarithmic);
    QCOMPARE(logarithmic->keyLower, 2.0);
    QVERIFY(std::isinf(logarithmic->valueLower) && logarithmic->valueLower < 0);
    QVERIFY(!(shape->extent()->box.valueLower > 0.0));
}

void TestDrawable::defaultLegendIconIsTheRecordingFitted()
{
    Fixture f;
    auto* line = new TestLine(f.plot->xAxis, f.plot->yAxis);
    const QRectF icon(0, 0, 100, 50);
    QImage image(100, 50, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QCPPainter painter(&image);
    line->paintIcon(&painter, icon);
    painter.end();

    // The recording is data (1,1) to (9,9), fitted to the rect: the line runs bottom-left to
    // top-right, so the centre is ink and the top-left corner is empty.
    QVERIFY2(image.pixelColor(50, 25).lightness() < 200,
             qPrintable(QString("centre is %1").arg(image.pixelColor(50, 25).name())));
    QVERIFY(image.pixelColor(4, 4).lightness() > 240);
}
