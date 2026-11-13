#include "test-drawable.h"

#include <drawable.h>
#include <examples/custom-drawable/rangeband.h>
#include <misuse.h>
#include <painting/command-replay-rhi.h>
#include <painting/draw-context.h>
#include <painting/line-extruder.h>
#include <painting/plottable-rhi-layer.h>
#include <plottables/plottable-draw-utils.h>
#include <qcustomplot.h>

#include <QtWidgets/qtestsupport_widgets.h> // QTest::qWaitForWindowExposed
#include <cmath>
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

//! A brush the GPU cannot fill in one colour: the replay rasterises it.
QBrush gradientBrush()
{
    QLinearGradient gradient(0, 0, 1, 0);
    gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
    gradient.setColorAt(0.0, QColor(220, 40, 40));
    gradient.setColorAt(1.0, QColor(40, 40, 220));
    return QBrush(gradient);
}

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
class TestShape : public QCPAbstractItem
{
public:
    explicit TestShape(QCustomPlot* plot) : QCPAbstractItem(plot)
    {
        mAnchor = createPosition(QStringLiteral("anchor"));
        mAnchor->setCoords(3, 3);
        setClipAxisRect(plot->axisRect());
        dirtyCalls = 0; // construction is not a content change
    }

    // The old painter virtual is final in the adapter: keep it visible so overriding the new
    // draw() does not hide it (-Woverloaded-virtual).

    enum Mode
    {
        Polygon,
        Mixed,
        Anchored,
        ZeroValue,
        Disjoint,
        Image,
        Ratio,
        PixelCorner,
        MixedRect,
        FromAnchor,
        Dashed, //!< one dashed stroke: the GPU needs its dashed pipeline for it
        DashedOutline, //!< a dashed outline and no fill: only the stroke has to come out
        DashedUnderRect, //!< a dashed line an opaque rect covers: the record order must survive
        RasterUnderRect, //!< a rasterised fill an opaque rect covers: the same
        ErrorBars, //!< three error bars in a pen no vertex colour can carry
        Raster, //!< a texture-brush rect beside a long polyline: two kept runs of different kinds
        AsymmetricBox //!< keys 2..3, values 8..9: the two sides sit on different pixel intervals
    };

    //! Counts the base setter path: a range change must not come through here.
    void markDirty() override
    {
        ++dirtyCalls;
        QCPAbstractItem::markDirty();
    }

    void draw(QCPDrawContext& context) const override
    {
        ++drawCalls;
        if (everyFrame)
            context.recordEveryFrame();
        if (contentChangeInsideDraw)
            const_cast<TestShape*>(this)->contentChanged();
        if (rangeInsideDraw)
        {
            const std::optional<QCPItemExtent> range = extent();
            sawRange = true;
            hadRangeInsideDraw = range.has_value();
        }
        if (readsTheme)
            (void)context.theme();
        if (readsTransform)
        {
            const QCP::Pixels corner = context.transform().map(QCP::Data { 6, 6 });
            context.rect(QCP::Data { 2, 2 }, QCP::Pixels { corner.x, corner.y }, QPen(Qt::black),
                         QBrush(fill));
            return;
        }
        if (readsKeyWindow) // one data polyline over the whole window the context says is valid
        {
            keyWindow = context.keyWindow();
            const QPointF keys[] = { { keyWindow.keys.lower, 5.0 }, { keyWindow.keys.upper, 5.0 } };
            context.polyline(
                std::make_shared<const std::vector<QPointF>>(std::begin(keys), std::end(keys)),
                QPen(Qt::black, 2));
            return;
        }
        if (bars > 0)
        {
            const QBrush brush = filledBars ? (gradientBars ? gradientBrush() : QBrush(fill))
                                            : QBrush(Qt::NoBrush);
            for (int i = 0; i < bars; ++i)
            {
                const double key = 1.0 + 8.0 * i / bars;
                context.rect(QCP::Data { key, 1.0 }, QCP::Data { key + 8.0 / bars, 3.0 },
                             QPen(Qt::black), brush);
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
            QCPItemPosition* anchor = externalAnchor ? externalAnchor : mAnchor;
            const QCP::Position corners[]
                = { QCP::AnchorOffset { anchor, QCP::PixelOffset { } },
                    QCP::AnchorOffset { anchor, QCP::PixelOffset { 20, 0 } },
                    QCP::AnchorOffset { anchor, QCP::PixelOffset { 20, 20 } },
                    QCP::AnchorOffset { anchor, QCP::PixelOffset { 0, 20 } } };
            context.polygon(corners, QPen(Qt::black, 4), QBrush(fill));
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
        if (mode == FromAnchor) // data positions read from an item position the setters move
        {
            const QPointF at = mAnchor->coords();
            const QCP::Position corners[]
                = { QCP::Data { at.x(), at.y() }, QCP::Data { at.x() + 1, at.y() + 1 } };
            context.polygon(corners, QPen(Qt::black), QBrush(fill));
            return;
        }
        if (mode == MixedRect) // one data corner, one pixel corner, nothing beside it
        {
            context.rect(QCP::Data { 2, 2 }, QCP::Pixels { 50, 50 }, QPen(Qt::black, 2),
                         QBrush(Qt::NoBrush));
            return;
        }
        if (mode == PixelCorner) // one data corner, one pixel corner; a ratio span beside it
        {
            context.rect(QCP::Data { 2, 2 }, QCP::Pixels { 50, 50 }, QPen(Qt::black, 2),
                         QBrush(Qt::NoBrush));
            const QCP::Position span[]
                = { QCP::DataXRatioY { 2, 0.2 }, QCP::DataXRatioY { 6, 0.2 },
                    QCP::DataXRatioY { 6, 0.8 }, QCP::DataXRatioY { 2, 0.8 } };
            context.polygon(span, QPen(Qt::black, 2), QBrush(Qt::NoBrush));
            return;
        }
        if (mode == Dashed)
        {
            context.polyline(mLine, QPen(Qt::black, 2, Qt::DashLine));
            return;
        }
        if (mode == DashedOutline) // a dashed outline with no fill: nothing else can ink it
        {
            context.rect(QCP::Data { 2, 2 }, QCP::Data { 8, 8 }, QPen(Qt::black, 2, Qt::DashLine),
                         QBrush(Qt::NoBrush));
            return;
        }
        if (mode == DashedUnderRect) // a thick dashed line, then an opaque rect over its middle
        {
            context.polyline(mLine, QPen(Qt::blue, 6, Qt::DashLine));
            context.rect(QCP::Data { 2, 2 }, QCP::Data { 8, 8 }, QPen(Qt::NoPen),
                         QBrush(Qt::green));
            return;
        }
        if (mode == RasterUnderRect) // a gradient fill the GPU rasterises, then an opaque rect
        {
            context.rect(QCP::Data { 2, 2 }, QCP::Data { 7, 7 }, QPen(Qt::NoPen), gradientBrush());
            context.rect(QCP::Data { 3, 3 }, QCP::Data { 6, 6 }, QPen(Qt::NoPen),
                         QBrush(Qt::green));
            return;
        }
        if (mode == ErrorBars) // a gradient pen: the stroke has to be rasterised, not lost
        {
            auto centres = std::make_shared<const std::vector<QPointF>>(
                std::vector<QPointF> { { 3, 5 }, { 5, 5 }, { 7, 5 } });
            auto spans = std::make_shared<const std::vector<QPointF>>(
                std::vector<QPointF> { { 0.4, 2 }, { 0.4, 2 }, { 0.4, 2 } });
            context.errorBars(std::move(centres), std::move(spans), QPen(gradientBrush(), 2));
            return;
        }
        if (mode == Raster) // a texture-brush rect and a long polyline, kept differently
        {
            context.rect(QCP::Data { 2, 2 }, QCP::Data { 7, 7 }, QPen(Qt::NoPen), gradientBrush());
            context.polyline(mLine, QPen(Qt::black, 2));
            return;
        }
        if (mode == AsymmetricBox) // keys 2..3, values 8..9: far apart on the two screen axes
        {
            const QCP::Position corners[] = { QCP::Data { 2, 8 }, QCP::Data { 3, 8 },
                                              QCP::Data { 3, 9 }, QCP::Data { 2, 9 } };
            context.polygon(corners, QPen(Qt::black), QBrush(fill));
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

    //! Test hook: how many rasterised images the replay keeps for this object.
    [[nodiscard]] int rasterCount() const { return mCore.rasterCount(); }

    //! Test hook: how many recordings the object has built.
    [[nodiscard]] int recordings() const { return mCore.recordingCount(); }

    //! The adapter's dataExtent(), which the layer index reads.
    [[nodiscard]] std::optional<QCPItemExtent> extent() const { return dataExtent(); }

    QCPItemPosition* mAnchor = nullptr;
    //! An anchor owned by another item, for the lifetime test.
    QCPItemPosition* externalAnchor = nullptr;
    std::shared_ptr<const std::vector<QPointF>> mLine
        = std::make_shared<const std::vector<QPointF>>(
            std::initializer_list<QPointF> { { 1, 1 }, { 3, 5 }, { 5, 2 }, { 7, 6 }, { 9, 1 } });
    QColor fill = QColor(Qt::darkRed);
    Mode mode = Polygon;
    int bars = 0;
    //! Fill the bar rects too, so a replay builds fills and strokes.
    bool filledBars = false;
    bool gradientBars = false; //!< the bar fill cannot be a GPU vertex colour
    bool withText = false;
    bool withPolyline = false;
    bool escapeHatch = false;
    bool readsTheme = false;
    bool readsTransform = false;
    //! Record a data polyline over the key window the context hands out.
    bool readsKeyWindow = false;
    mutable QCPKeyWindow keyWindow;
    //! Declare that the recording reads something no setter tells the object about.
    bool everyFrame = false;
    bool contentChangeInsideDraw = false;
    //! Ask for the object's own range while recording it, as a custom draw() may.
    bool rangeInsideDraw = false;
    mutable bool sawRange = false;
    mutable bool hadRangeInsideDraw = false;
    mutable int drawCalls = 0;
    mutable int dirtyCalls = 0;
};

//! A drawn plottable: one data-space line, so the default icon has something to fit.
class TestLine : public QCPAbstractPlottable
{
public:
    TestLine(QCPAxis* keyAxis, QCPAxis* valueAxis)
            : QCPAbstractPlottable(keyAxis, valueAxis)
    {
    }

    using QCPAbstractPlottable::drawLegendIcon; // the painter overload stays reachable

    void draw(QCPDrawContext& context) const override
    {
        ++drawCalls;
        context.polyline(mLine, this->pen());
        if (labelAt)
        {
            QCPTextStyle style;
            style.font.setPointSize(20);
            style.color = QColor(Qt::blue);
            context.text(QCP::Data { labelAt->x(), labelAt->y() }, QStringLiteral("W"), style);
        }
        if (rasterRect) // a gradient fill: the replay can only draw it as an image
            context.rect(QCP::Data { 2, 2 }, QCP::Data { 14, 6 }, QPen(Qt::NoPen), gradientBrush());
    }

    //! The icon is a recording too: one that reads the theme must not outlive the theme it read.
    void drawLegendIcon(QCPDrawContext& context) const override
    {
        ++iconCalls;
        const QColor colour = readsTheme ? context.theme().foreground() : this->pen().color();
        context.polyline(mLine, QPen(colour, 4));
    }

    //! The adapter's old virtual, which the legend calls.
    void paintIcon(QCPPainter* painter, const QRectF& rect) const { drawLegendIcon(painter, rect); }

    //! The click path QCustomPlot uses: selectTest fills the details, selectEvent applies them.
    void clickSelect(const QPointF& pos)
    {
        QVariant details;
        if (selectTest(pos, true, &details) < 0)
            return;
        bool changed = false;
        selectEvent(nullptr, false, details, &changed);
    }

    mutable int drawCalls = 0;
    mutable int iconCalls = 0;
    bool readsTheme = false;
    //! A data-anchored label the painter draws: a shifted layer cannot reveal it.
    std::optional<QPointF> labelAt;
    //! A gradient rect half outside the visible range: its image is clipped to the view.
    bool rasterRect = false;
    std::shared_ptr<const std::vector<QPointF>> mLine
        = std::make_shared<const std::vector<QPointF>>(
            std::initializer_list<QPointF> { { 1, 1 }, { 9, 9 } });
};

//! A drawn plottable whose legend icon calls a setter: a misuse, like contentChanged() in draw().
class TestIconSetter : public QCPAbstractPlottable
{
public:
    TestIconSetter(QCPAxis* keyAxis, QCPAxis* valueAxis)
            : QCPAbstractPlottable(keyAxis, valueAxis)
    {
    }

    using QCPAbstractPlottable::drawLegendIcon; // the painter overload stays reachable

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

//! A plottable with nothing but a recording: the base answers every old virtual from it.
class BareLine : public QCPAbstractPlottable1D<QCPGraphData>
{
public:
    BareLine(QCPAxis* keyAxis, QCPAxis* valueAxis)
            : QCPAbstractPlottable1D<QCPGraphData>(keyAxis, valueAxis)
    {
    }

    void draw(QCPDrawContext& context) const override
    {
        ++drawCalls;
        context.polyline(mLine, this->pen());
    }

    //! The old virtual the legend calls, which the base now answers from the recording.
    void paintIcon(QCPPainter* painter, const QRectF& rect) const { drawLegendIcon(painter, rect); }

    int recordings() const { return mCore.recordingCount(); }

    int recordedCommands() const { return mCore.recordedCommandCount(); }

    mutable int drawCalls = 0;
    std::shared_ptr<const std::vector<QPointF>> mLine
        = std::make_shared<const std::vector<QPointF>>(
            std::initializer_list<QPointF> { { 1, 1 }, { 9, 9 } });
};

//! An item with nothing but a recording: the base owns the recording the same way.
class BareShape : public QCPAbstractItem
{
public:
    explicit BareShape(QCustomPlot* plot) : QCPAbstractItem(plot)
    {
        mAnchor = createPosition(QStringLiteral("anchor"));
        mAnchor->setCoords(5, 5);
        setClipAxisRect(plot->axisRect());
    }

    void draw(QCPDrawContext& context) const override
    {
        ++drawCalls;
        context.polyline(mLine, QPen(Qt::black, 2));
    }

    int recordings() const { return mCore.recordingCount(); }

    int recordedCommands() const { return mCore.recordedCommandCount(); }

    mutable int drawCalls = 0;
    QCPItemPosition* mAnchor = nullptr;
    std::shared_ptr<const std::vector<QPointF>> mLine
        = std::make_shared<const std::vector<QPointF>>(
            std::initializer_list<QPointF> { { 1, 1 }, { 9, 9 } });
};

//! Bars with their recording count exposed: a style setter must make the recording stale.
class CountingBars : public QCPBars
{
public:
    using QCPBars::QCPBars;

    int recordings() const { return mCore.recordingCount(); }
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

//! The box the inked pixels of \a frame occupy, in device pixels.
QRect inkedBounds(const QImage& frame)
{
    QRect box;
    for (int y = 0; y < frame.height(); ++y)
        for (int x = 0; x < frame.width(); ++x)
            if (frame.pixelColor(x, y) != QColor(Qt::white))
                box = box.isNull() ? QRect(x, y, 1, 1) : box.united(QRect(x, y, 1, 1));
    return box;
}

//! Hides everything but the drawn objects, so the ink measured is theirs.
void hideAxes(QCustomPlot* plot)
{
    plot->xAxis->setVisible(false);
    plot->yAxis->setVisible(false);
    plot->xAxis->grid()->setVisible(false);
    plot->yAxis->grid()->setVisible(false);
}

bool isBlue(const QColor& c)
{
    return c.blue() > 200 && c.red() < 60 && c.green() < 60;
}

bool isGreenInk(const QColor& c)
{
    return c.green() > 150 && c.red() < 100;
}

//! Anything but the white background, the test inkedBounds() uses.
bool isInk(const QColor& c)
{
    return c != QColor(Qt::white);
}

//! How many pixels of \a frame inside \a box \a matches accepts.
int countPixels(const QImage& frame, QRect box, bool (*matches)(const QColor&))
{
    int count = 0;
    const QRect area = box.intersected(frame.rect());
    for (int y = area.top(); y <= area.bottom(); ++y)
        for (int x = area.left(); x <= area.right(); ++x)
            if (matches(frame.pixelColor(x, y)))
                ++count;
    return count;
}

//! \a a to \a b in data coordinates, as a device-pixel rect.
QRect deviceRect(QCustomPlot* plot, const QImage& frame, const QPointF& a, const QPointF& b)
{
    const QRectF pixels
        = QRectF(QPointF(plot->xAxis->coordToPixel(a.x()), plot->yAxis->coordToPixel(a.y())),
                 QPointF(plot->xAxis->coordToPixel(b.x()), plot->yAxis->coordToPixel(b.y())))
              .normalized();
    const double dpr = frame.devicePixelRatio();
    return QRectF(pixels.topLeft() * dpr, pixels.size() * dpr).toAlignedRect();
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
    QCOMPARE(shape->dirtyCalls, 0); // a range change is not a content change
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
    QCOMPARE(shape->dirtyCalls, 0); // a zoom is not a content change
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

void TestDrawable::themeReaderReRecordsOnThemeChange()
{
    Fixture f;
    auto* plain = new TestShape(f.plot.get());
    auto* reader = new TestShape(f.plot.get());
    reader->readsTheme = true;
    f.plot->replot();
    QCOMPARE(plain->drawCalls, 1);
    QCOMPARE(reader->drawCalls, 1);

    // Theme facts come from the view: a recording that read them cannot outlive a new theme.
    const quint64 epoch = f.plot->viewEpoch();
    f.plot->applyTheme();
    QVERIFY2(f.plot->viewEpoch() != epoch, "a theme change must bump the view epoch");
    f.plot->replot();
    QCOMPARE(plain->drawCalls, 1); // a polygon that never read the theme stays cached
    QCOMPARE(reader->drawCalls, 2);
}

void TestDrawable::transformReaderReRecordsOnEveryPan()
{
    Fixture f;
    auto* shape = new TestShape(f.plot.get());
    shape->readsTransform = true;
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 1);

    f.plot->xAxis->setRange(2, 12); // a pan moved the pixels the recording baked
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 2);

    f.plot->xAxis->setRange(4, 6); // a zoom changed their scale as well
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 3);
    QCOMPARE(shape->dirtyCalls, 0); // a view change is not a content change
}

void TestDrawable::everyFrameRecordingRebuildsOncePerFrame()
{
    Fixture f;
    auto* shape = new TestShape(f.plot.get());
    shape->everyFrame = true;
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 1);

    // The queries of the frame the paint opened read what the paint recorded.
    const auto at = [&](double key, double value)
    {
        return QCP::Pixels { f.plot->xAxis->coordToPixel(key), f.plot->yAxis->coordToPixel(value) };
    };
    (void)shape->hitTest(at(4, 4), nullptr);
    (void)shape->extent();
    QCOMPARE(shape->drawCalls, 1);

    // The next frame is a new one: that is what everyFrame() is for.
    shape->layer()->markDirty(); // the second replot must repaint the layer the shape sits in
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 2);
}

void TestDrawable::itemTextMeasuresOnceForItsQueries()
{
    Fixture f;
    auto* item = new QCPItemText(f.plot.get());
    item->setText(QStringLiteral("measured once"));
    item->setFont(QFont(QStringLiteral("DejaVu Sans"), 14));
    item->position->setCoords(5, 5);
    f.plot->replot();

    // The box the recording measured serves the hit tests and the anchors of the frame.
    const quint64 measured = qcpTextMeasureCount();
    const QPointF centre(f.plot->xAxis->coordToPixel(5), f.plot->yAxis->coordToPixel(5));
    for (int i = 0; i < 5; ++i)
    {
        QVERIFY(item->selectTest(centre, false) >= 0.0);
        (void)item->top->pixelPosition();
        (void)item->bottomRight->pixelPosition();
    }
    QCOMPARE(qcpTextMeasureCount(), measured);
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

void TestDrawable::recordingFromInsideDrawIsAMisuse()
{
    Fixture f;
    auto* shape = new TestShape(f.plot.get());
    shape->rangeInsideDraw = true;
    f.plot->replot(); // must not re-enter draw() from inside draw()

    QCOMPARE(shape->drawCalls, 1);
    QVERIFY2(shape->sawRange, "draw() did not ask for its own range");
    QCOMPARE(gMisuseCount, 1);
    QVERIFY2(!shape->hadRangeInsideDraw, "the first recording has no earlier one to answer with");

    // A later recording answers from the previous one instead of recursing again.
    shape->touch();
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 2);
    QCOMPARE(gMisuseCount, 2);
    QVERIFY2(shape->hadRangeInsideDraw, "the last recording was not handed back");
}

void TestDrawable::missingAxisHidesWithAMessage()
{
    Fixture f;
    auto* shape = new TestShape(f.plot.get());
    shape->mAnchor->setAxes(nullptr, nullptr);
    f.plot->replot();
    f.plot->replot();
    // Whether a drawing needs the axes depends on what it records, so it is drawn once to learn
    // that; the refusal then stands until the object changes, and it is never painted.
    QCOMPARE(shape->drawCalls, 1);
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

void TestDrawable::panBuildsNoFillEither()
{
    Fixture f;
    if (!f.hasRhi())
        QSKIP("No QRhi available - the built geometry is a GPU path only");
    auto* shape = new TestShape(f.plot.get());
    shape->bars = 10000;
    shape->filledBars = true;
    f.plot->replot();
    const quint64 fills = qcpFillBuildCount();
    const quint64 strokes = QCPLineExtruder::extruderCallCount();
    QVERIFY2(fills > 0, "the first frame must tessellate the fills");
    QVERIFY2(strokes > 0, "the first frame must extrude the outlines");

    // A pan moves the whole recording by one delta: neither the fills nor the strokes are built
    // again, they are the kept triangles handed back with an offset.
    f.plot->xAxis->setRange(1, 11);
    f.plot->replot();
    QCOMPARE(qcpFillBuildCount(), fills);
    QCOMPARE(QCPLineExtruder::extruderCallCount(), strokes);
}

void TestDrawable::panMovesAllDataEntriesByOneDelta()
{
    Fixture f;
    if (!f.hasRhi())
        QSKIP("No QRhi available - the kept geometry is a GPU path only");
    auto* shape = new TestShape(f.plot.get());
    shape->bars = 10;
    shape->filledBars = true;
    f.plot->replot();

    f.plot->xAxis->setRange(1, 11); // a pan: same span, one pixel offset
    f.plot->replot();

    auto* layer = f.plot->plottableRhiLayer(shape->layer());
    QVERIFY(layer);
    const auto& entries = layer->drawEntries();
    QVERIFY(!entries.isEmpty());
    const float offsetX = entries.first().offsetX;
    const float offsetY = entries.first().offsetY;
    QVERIFY2(
        std::abs(offsetX) > 0.5f,
        qPrintable(QStringLiteral("the pan must offset the kept geometry, got %1").arg(offsetX)));
    QVERIFY2(qFuzzyIsNull(offsetY), "the pan is horizontal");
    // Every entry moves with the data, including the fills: a rebuild would leave their offset at
    // zero and the entry list at two per bar.
    for (const auto& entry : entries)
    {
        QCOMPARE(entry.offsetX, offsetX);
        QCOMPARE(entry.offsetY, offsetY);
    }
}

void TestDrawable::dataOnlyDrawingLetsItsLayerTranslate()
{
    // One drawn object on a layer used to disable the translate path (it offered no stall offset),
    // so every pan repainted the layer and re-uploaded its vertices. A recording that is one piece
    // in data space may be shifted instead: nothing is drawn and nothing is uploaded.
    Fixture f;
    if (!f.hasRhi())
        QSKIP("No QRhi available - the translate path is a GPU one");
    hideAxes(f.plot.get());
    auto* line = new TestLine(f.plot->xAxis, f.plot->yAxis);
    line->mLine = std::make_shared<const std::vector<QPointF>>(
        std::initializer_list<QPointF> { { 1, 1 }, { 9, 9 } });
    f.plot->replot();
    auto* layer = f.plot->plottableRhiLayer(line->layer());
    QVERIFY(layer);
    QTRY_VERIFY_WITH_TIMEOUT(layer->solidVertexUploads() > 0, 5000);
    const int uploads = layer->solidVertexUploads();
    const int draws = line->drawCalls;
    const double anchor = f.plot->xAxis->coordToPixel(2.0);
    const QRect before = inkedBounds(f.plot->grabFramebuffer());
    QVERIFY2(!before.isNull(), "the line must be painted");

    f.plot->xAxis->setRange(1, 11); // a pan: the layer texture is shifted, not repainted
    f.plot->replot();
    // Synchronous: replot() is where a layer that will be rebuilt is cleared.
    QVERIFY2(!layer->isDirty(), "a translated layer has nothing to rebuild");
    QCOMPARE(line->drawCalls, draws);
    QTest::qWait(50); // give a scheduled upload the time to happen before counting
    QCOMPARE(layer->solidVertexUploads(), uploads);

    const QRect after = inkedBounds(f.plot->grabFramebuffer());
    const double moved = f.plot->xAxis->coordToPixel(2.0) - anchor;
    QVERIFY2(qAbs((after.left() - before.left()) - moved) <= 2,
             qPrintable(QStringLiteral("the line moved %1 px, the axis %2 px")
                            .arg(after.left() - before.left())
                            .arg(moved)));
}

void TestDrawable::translatedLayerRevealsAnEnteringLabel()
{
    // The painter draws a recording's text into the layer's paint buffer, which only holds what was
    // inside the clip when it was painted. A translated layer shifts that buffer, so a label that
    // was outside the key range can never appear while the user pans: a recording holding one must
    // be repainted, not shifted.
    Fixture f;
    if (!f.hasRhi())
        QSKIP("No QRhi available - the translate path is a GPU one");
    hideAxes(f.plot.get());
    auto* line = new TestLine(f.plot->xAxis, f.plot->yAxis);
    line->mLine = std::make_shared<const std::vector<QPointF>>(
        std::initializer_list<QPointF> { { 1, 1 }, { 9, 9 } });
    line->labelAt = QPointF(12.0, 5.0); // just right of the visible key range
    f.plot->replot(QCustomPlot::rpImmediateRefresh);

    f.plot->xAxis->setRange(5, 15); // a pan: the label's anchor is well inside now
    f.plot->replot(QCustomPlot::rpImmediateRefresh);
    const QImage frame = f.plot->grabFramebuffer();
    const QRect box = deviceRect(f.plot.get(), frame, { 12.0, 4.4 }, { 14.0, 5.6 });
    const int ink = countPixels(frame, box, isBlue);
    QVERIFY2(
        ink > 10,
        qPrintable(
            QStringLiteral("the label did not enter the view on the pan: %1 pixels").arg(ink)));
}

void TestDrawable::translatedLayerRevealsAClippedRaster()
{
    // A rasterised command is an image clipped to the view it was painted for. Shifting the layer
    // moves that image, but nothing was ever painted for the part the clip cut away: the recording
    // has to be replayed so the newly revealed half is drawn.
    Fixture f;
    if (!f.hasRhi())
        QSKIP("No QRhi available - the raster fallback is a GPU path");
    hideAxes(f.plot.get());
    auto* line = new TestLine(f.plot->xAxis, f.plot->yAxis);
    line->rasterRect = true; // a gradient from (2,2) to (14,6): half of it is outside the view
    f.plot->replot(QCustomPlot::rpImmediateRefresh);

    f.plot->xAxis->setRange(4, 14); // a pan: the rect is inside the view, its right half is new
    f.plot->replot(QCustomPlot::rpImmediateRefresh);
    const QImage frame = f.plot->grabFramebuffer();
    const QRect box = deviceRect(f.plot.get(), frame, { 10.5, 2.5 }, { 13.5, 5.5 });
    const int ink = countPixels(frame, box, isInk);
    QVERIFY2(
        ink > 100,
        qPrintable(QStringLiteral("the revealed half of the raster is blank: %1 pixels").arg(ink)));
}

void TestDrawable::translatedBarsRevealTheNextBar()
{
    // The guard for the restriction above: bars are pure GPU rect geometry, so a recording of them
    // still answers a stall offset, and the pan row's gain must survive.
    Fixture f;
    if (!f.hasRhi())
        QSKIP("No QRhi available - the translate path is a GPU one");
    hideAxes(f.plot.get());
    auto* bars = new QCPBars(f.plot->xAxis, f.plot->yAxis);
    QVector<double> keys(10000), values(10000);
    for (int i = 0; i < 10000; ++i)
    {
        keys[i] = i;
        values[i] = 0.5;
    }
    bars->setData(keys, values);
    f.plot->xAxis->setRange(0, 100); // the first hundred bars
    f.plot->yAxis->setRange(0, 1);
    f.plot->replot();
    auto* layer = f.plot->plottableRhiLayer(bars->layer());
    QVERIFY(layer);
    QTRY_VERIFY_WITH_TIMEOUT(layer->solidVertexUploads() > 0, 5000); // the first frame is painted

    f.plot->xAxis->setRange(10, 110); // a pan: bars that were off screen enter it
    f.plot->replot();
    QVERIFY2(!layer->isDirty(), "the bars' layer has to translate, not repaint");

    const QColor entered = pixelAt(f.plot->grabFramebuffer(), f.plot.get(), 105.0, 0.25);
    QVERIFY2(
        isInk(entered),
        qPrintable(
            QStringLiteral("the bar that entered the view is not drawn: %1").arg(entered.name())));
}

void TestDrawable::rasterPanKeepsImage()
{
    Fixture f;
    if (!f.hasRhi())
        QSKIP("No QRhi available - the raster image and the kept triangles are a GPU path only");
    auto* shape = new TestShape(f.plot.get());
    shape->mode = TestShape::Raster;
    std::vector<QPointF> points;
    points.reserve(5000);
    for (int i = 0; i < 5000; ++i)
        points.emplace_back(1.0 + 8.0 * i / 4999.0, 4.0 + std::sin(i * 0.05));
    shape->mLine = std::make_shared<const std::vector<QPointF>>(std::move(points));
    f.plot->replot();
    QCOMPARE(shape->rasterCount(), 1);
    const quint64 strokes = QCPLineExtruder::extruderCallCount();
    QVERIFY2(strokes > 0, "the first frame must extrude the polyline");

    // A pan moves the whole recording by one delta: the image is kept and shifted, and the polyline
    // is handed back as the triangles it was built from.
    f.plot->xAxis->setRange(1, 11);
    f.plot->replot();
    QCOMPARE(shape->rasterCount(), 1);
    QCOMPARE(QCPLineExtruder::extruderCallCount(), strokes);
}

void TestDrawable::recordedErrorBarsAreDrawn()
{
    Fixture f;
    if (!f.hasRhi())
        QSKIP("No QRhi available - the raster fallback is a GPU path");
    auto* shape = new TestShape(f.plot.get());
    shape->mode = TestShape::ErrorBars;
    f.plot->replot(QCustomPlot::rpImmediateRefresh);
    const QImage frame = f.plot->grabFramebuffer();
    // The bar over key 5 spans values 3 to 7: a gradient pen cannot be a vertex colour, so the
    // stroke is an image. Count its ink down the bar's column.
    int ink = 0;
    for (int i = 0; i <= 80; ++i)
    {
        const QColor c = pixelAt(frame, f.plot.get(), 5.0, 3.2 + 3.6 * i / 80.0);
        ink += c.red() + c.green() + c.blue() < 720 ? 1 : 0;
    }
    QVERIFY2(ink > 10,
             qPrintable(QStringLiteral("the error bars left no ink: %1 pixels").arg(ink)));
}

void TestDrawable::brokenDashedPipelineStillDraws()
{
    Fixture f;
    if (!f.hasRhi())
        QSKIP("No QRhi available - the dashed pipeline is a GPU path");
    auto* shape = new TestShape(f.plot.get());
    shape->mode = TestShape::Dashed;
    shape->mLine
        = std::make_shared<const std::vector<QPointF>>(std::vector<QPointF> { { 1, 5 }, { 9, 5 } });
    auto* layer = f.plot->plottableRhiLayer(shape->layer());
    QVERIFY(layer);
    layer->breakDashedPipelineForTest();
    f.plot->replot(QCustomPlot::rpImmediateRefresh);

    // Dashes leave gaps, so the count down the line, not one pixel, says the stroke is there.
    const auto ink = [&](const QImage& frame)
    {
        int count = 0;
        for (int i = 0; i <= 60; ++i)
        {
            const QColor c = pixelAt(frame, f.plot.get(), 1.2 + 7.6 * i / 60.0, 5.0);
            count += c.red() + c.green() + c.blue() < 720 ? 1 : 0;
        }
        return count;
    };
    QVERIFY2(ink(f.plot->grabFramebuffer()) > 10,
             "a dashed stroke was lost when the dashed pipeline could not be built");

    // The stroke became an image, and an image is kept: the pan only shifts it.
    f.plot->xAxis->setRange(0, 11);
    f.plot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY2(ink(f.plot->grabFramebuffer()) > 10, "the pan lost the rasterised dash");
}

// The dashed pipeline is missing, so the dashed stroke becomes an image: the image has to hold the
// stroke too, or a dashed outline leaves nothing at all.
void TestDrawable::brokenDashedPipelineKeepsTheOutline()
{
    Fixture f;
    if (!f.hasRhi())
        QSKIP("No QRhi available - the dashed pipeline is a GPU path");
    auto* bars = new QCPBars(f.plot->xAxis, f.plot->yAxis);
    bars->setPen(QPen(Qt::black, 2, Qt::DashLine));
    bars->setBrush(Qt::NoBrush);
    bars->setWidth(1.0);
    bars->setData(QVector<double> { 2.0 }, QVector<double> { 1.0 });
    f.plot->replot(QCustomPlot::rpImmediateRefresh);
    auto* layer = f.plot->plottableRhiLayer(bars->layer());
    QVERIFY(layer);
    layer->breakDashedPipelineForTest();
    QVERIFY(!layer->dashedAvailable());
    // A re-record made now has no dashed pipeline to draw its stroke.
    bars->setData(QVector<double> { 2.0 }, QVector<double> { 1.5 });
    f.plot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(!layer->dashedAvailable());

    const QImage frame = f.plot->grabFramebuffer();
    int ink = 0;
    for (int i = 0; i <= 60; ++i)
    {
        const QColor c = pixelAt(frame, f.plot.get(), 1.55 + 0.9 * i / 60.0, 1.5);
        ink += c.red() + c.green() + c.blue() < 720 ? 1 : 0;
    }
    QVERIFY2(ink > 5, "a dashed outline was lost when the dashed pipeline could not be built");
}

// A recording that rasterises its dashes holds an image the pan would shift: it cannot be moved
// as a whole, the layer has to replay it.
void TestDrawable::rasterisedDashesStopTheLayerFromTranslating()
{
    Fixture f;
    if (!f.hasRhi())
        QSKIP("No QRhi available - the dashed pipeline is a GPU path");
    auto* bars = new QCPBars(f.plot->xAxis, f.plot->yAxis);
    bars->setPen(QPen(Qt::black, 2));
    bars->setBrush(QColor(200, 60, 60));
    bars->setWidth(1.0);
    bars->setData(QVector<double> { 2.0 }, QVector<double> { 1.0 });
    f.plot->replot(QCustomPlot::rpImmediateRefresh);
    f.plot->xAxis->setRange(0.5, 10.5);
    QVERIFY2(!bars->stallPixelOffset().isNull(),
             "a recording of solid strokes and fills can be shifted as a whole");

    // No dashed pipeline: the stroke of the re-record is an image, and an image cannot be shifted.
    auto* layer = f.plot->plottableRhiLayer(bars->layer());
    QVERIFY(layer);
    layer->breakDashedPipelineForTest();
    bars->setPen(QPen(Qt::black, 2, Qt::DashLine));
    bars->setData(QVector<double> { 2.0 }, QVector<double> { 1.5 });
    f.plot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(!layer->dashedAvailable());
    f.plot->xAxis->setRange(1.0, 11.0);
    QVERIFY2(bars->stallPixelOffset().isNull(),
             "a recording that rasterises its dashes cannot be shifted as a whole");
}

void TestDrawable::zoomRebuildsOnce()
{
    Fixture f;
    if (!f.hasRhi())
        QSKIP("No QRhi available - the kept geometry is a GPU path only");
    auto* shape = new TestShape(f.plot.get());
    shape->bars = 100;
    shape->filledBars = true;
    // Another object on the same layer, re-recorded before the last frame: it clears the layer, so
    // the replay really runs on a frame where the shape itself changed nothing.
    auto* ticker = new TestShape(f.plot.get());
    ticker->escapeHatch = true;
    f.plot->replot();
    const quint64 fills = qcpFillBuildCount();
    const quint64 strokes = QCPLineExtruder::extruderCallCount();

    f.plot->xAxis->setRange(4, 6); // a zoom: a new scale, so the geometry is built once
    f.plot->replot();
    const quint64 afterZoom = qcpFillBuildCount();
    const quint64 strokesAfterZoom = QCPLineExtruder::extruderCallCount();
    QVERIFY2(afterZoom > fills, "the zoom must build the fills again");
    QVERIFY2(strokesAfterZoom > strokes, "the zoom must extrude the outlines again");

    ticker->touch();
    f.plot->replot(); // same transform, same recording: the kept geometry is re-emitted as is
    QCOMPARE(qcpFillBuildCount(), afterZoom);
    QCOMPARE(QCPLineExtruder::extruderCallCount(), strokesAfterZoom);
}

void TestDrawable::tenThousandBarsAreOneDraw()
{
    Fixture f;
    if (!f.hasRhi())
        QSKIP("No QRhi available - the merged entries are a GPU path only");
    auto* shape = new TestShape(f.plot.get());
    shape->bars = 10000;
    shape->filledBars = true;
    f.plot->replot();

    auto* layer = f.plot->plottableRhiLayer(shape->layer());
    QVERIFY(layer);
    // Every bar shares the clip, the offset and the alpha of the one before it and its vertices
    // start where theirs end: the whole recording is one draw, not one per rect side.
    QCOMPARE(int(layer->drawEntries().size()), 1);
    const auto& entry = layer->drawEntries().first();
    QVERIFY(entry.fillVertexCount > 0);
    QVERIFY(entry.vertexCount > entry.fillVertexCount);
}

void TestDrawable::gradientBarsStackAsOne()
{
    Fixture f;
    if (!f.hasRhi())
        QSKIP("No QRhi available - the rasterised fills are a GPU path only");
    // The graph is created first, so it must stay under every bar.
    QCPGraph* graph = f.plot->addGraph();
    graph->setData(QVector<double> { 1, 9 }, QVector<double> { 2, 2 });
    graph->setPen(QPen(Qt::red, 24));
    auto* shape = new TestShape(f.plot.get());
    shape->bars = 10;
    shape->filledBars = true;
    shape->gradientBars = true; // more images than one layer can take
    shape->setLayer(QLatin1String("main")); // the graph's layer: only then can the two mix
    f.plot->replot(QCustomPlot::rpImmediateRefresh);

    const QImage frame = f.plot->grabFramebuffer();
    // Every bar's crossing pixel must be the graph's red: the recording is painted whole by the
    // layer's QPainter pass, under the layer's GPU content. A split at the cap would leave the
    // images it did take above the graph and the rest under it.
    for (int i = 0; i < 10; ++i)
    {
        const QColor colour = pixelAt(frame, f.plot.get(), 1.0 + 8.0 * (i + 0.5) / 10.0, 2);
        QVERIFY2(
            colour.red() > 200 && colour.green() < 60 && colour.blue() < 60,
            qPrintable(
                QStringLiteral("bar %1 is not under the graph: %2").arg(i).arg(colour.name())));
    }
}

void TestDrawable::fadeSplitsTheRun()
{
    Fixture f;
    if (!f.hasRhi())
        QSKIP("No QRhi available - the merged entries are a GPU path only");
    auto* first = new TestLine(f.plot->xAxis, f.plot->yAxis);
    first->setPen(QPen(Qt::black, 2));
    auto* second = new TestLine(f.plot->xAxis, f.plot->yAxis);
    second->mLine = std::make_shared<const std::vector<QPointF>>(
        std::initializer_list<QPointF> { { 1, 8 }, { 9, 8 } });
    second->setPen(QPen(Qt::black, 2));
    f.plot->replot();

    auto* layer = f.plot->plottableRhiLayer(first->layer());
    QVERIFY(layer);
    QCOMPARE(second->layer(), first->layer());
    // Two objects on one layer, nothing to tell their strokes apart: one run.
    QCOMPARE(int(layer->drawEntries().size()), 1);

    // The second one fades: its alpha no longer matches, so it must keep its own run instead of
    // being folded into the first object's and drawn at the wrong opacity.
    second->setBusyShowDelayMs(0);
    second->setBusy(true);
    QTRY_VERIFY_WITH_TIMEOUT(second->visuallyBusy(), 5000);
    f.plot->replot();
    QCOMPARE(int(layer->drawEntries().size()), 2);
    QVERIFY(layer->drawEntries().last().alpha < 0.99f);
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
    QCPGraph* graph = f.plot->addGraph();
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

void TestDrawable::batchHitTestNamesTheRect()
{
    // A batch is one command over many rects: the hit test must still say which bar was clicked,
    // and it says it once per scan instead of once per command.
    Fixture f;
    auto* shape = new TestShape(f.plot.get());
    shape->bars = 10; // ten data rects: keys 1..9, values 1..3, one pen and one brush
    shape->filledBars = true;
    f.plot->replot();
    const auto at = [&](double key, double value)
    {
        return QCP::Pixels { f.plot->xAxis->coordToPixel(key), f.plot->yAxis->coordToPixel(value) };
    };

    // Bar i covers keys 1 + 0.8 i .. 1 + 0.8 (i + 1), so the seventh one spans 6.6 .. 7.4.
    QVariant details;
    const std::optional<double> hit = shape->hitTest(at(7.0, 2.0), &details);
    QVERIFY2(hit.has_value(), "a click inside the seventh bar must hit the recording");
    QCOMPARE(*hit, 0.0); // inside a visible fill
    QCOMPARE(details.userType(), int(QMetaType::Int));
    QCOMPARE(details.toInt(), 7);

    // A hit on the first bar names the first rect: the index is the batch's, not the command's.
    QVariant first;
    QVERIFY(shape->hitTest(at(1.4, 2.0), &first).has_value());
    QCOMPARE(first.toInt(), 0);

    // A miss is still a miss, and it leaves the details alone.
    QVERIFY(!shape->hitTest(at(20.0, 2.0), &details).has_value());
}

void TestDrawable::hitTestWithAVerticalKeyAxis()
{
    // A horizontal plot: the key axis is vertical and the value axis horizontal, so the transform
    // swaps the two pixel coordinates. The bounds check must map each side on its own axis.
    Fixture f;
    auto* shape = new TestShape(f.plot.get());
    shape->mAnchor->setAxes(f.plot->yAxis, f.plot->xAxis); // drawn through the vertical key frame
    f.plot->replot();
    const auto at = [&](double key, double value)
    {
        return QCP::Pixels { f.plot->xAxis->coordToPixel(value), f.plot->yAxis->coordToPixel(key) };
    };

    const std::optional<double> inside = shape->hitTest(at(4, 4), nullptr);
    QVERIFY2(inside.has_value(), "a click on the shape must hit through a vertical key axis");
    QCOMPARE(*inside, 0.0); // inside a visible fill

    QVERIFY2(!shape->hitTest(at(4, 12), nullptr).has_value(),
             "a click beyond the shape's value side must miss");
    QVERIFY2(!shape->hitTest(at(-3, 4), nullptr).has_value(),
             "a click beyond the shape's key side must miss");
}

void TestDrawable::hitTestWithAVerticalKeyAxisChecksEachSideOnItsOwnAxis()
{
    // A box whose two sides land far apart on screen: keys 2..3 are a narrow vertical band,
    // values 8..9 a band further right. Each side must be compared with the click coordinate
    // along its own screen axis, or the pre-check rejects a click at the centre.
    Fixture f;
    auto* shape = new TestShape(f.plot.get());
    shape->mode = TestShape::AsymmetricBox;
    shape->mAnchor->setAxes(f.plot->yAxis, f.plot->xAxis); // drawn through the vertical key frame
    f.plot->replot();

    const QCP::Pixels centre { f.plot->xAxis->coordToPixel(8.5), f.plot->yAxis->coordToPixel(2.5) };
    const std::optional<double> inside = shape->hitTest(centre, nullptr);
    QVERIFY2(inside.has_value(),
             qPrintable(QStringLiteral("a click at the box centre (%1, %2) must hit")
                            .arg(centre.x)
                            .arg(centre.y)));
    QCOMPARE(*inside, 0.0); // inside a visible fill
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

void TestDrawable::frameEpochsAreNeverShared()
{
    // A plot is deleted and the next one takes its address: the frame memo keys on that pointer and
    // on the frame epoch, so a new plot must never be handed a frame the old one built. The layout
    // alternates, so a stale frame would put the hit test at the wrong pixel.
    int misses = 0;
    int sharedEpochs = 0;
    quint64 previousEpoch = 0;
    for (int i = 0; i < 200; ++i)
    {
        auto plot = std::make_unique<QCustomPlot>();
        plot->resize(400, 300);
        plot->xAxis->setRange(0, 10);
        plot->yAxis->setRange(0, 10);
        const bool loose = i % 2 == 0;
        plot->axisRect()->setMargins(loose ? QMargins(10, 10, 10, 10) : QMargins(70, 50, 30, 40));
        auto* shape = new TestShape(plot.get());
        shape->mode = TestShape::Ratio;
        plot->replot();
        if (i > 0 && plot->frameEpoch() == previousEpoch)
            ++sharedEpochs;
        previousEpoch = plot->frameEpoch();
        const double middle = plot->xAxis->coordToPixel(4);
        const double ratioY = plot->axisRect()->top() + 0.2 * plot->axisRect()->height();
        if (!shape->hitTest(QCP::Pixels { middle, ratioY }, nullptr).has_value())
            ++misses;
    }
    QCOMPARE(sharedEpochs, 0);
    QCOMPARE(misses, 0);
}

void TestDrawable::twoRectsOneLayerDrawInTheirOwnFrames()
{
    // Two items on one layer, each on its own axis rect: one replot paints them both, and the frame
    // the second one reads must be its own rect's, not the one the first one left in the memo.
    QCustomPlot plot;
    plot.resize(600, 300);
    plot.plotLayout()->clear();
    auto* first = new QCPAxisRect(&plot);
    auto* second = new QCPAxisRect(&plot);
    plot.plotLayout()->addElement(0, 0, first);
    plot.plotLayout()->addElement(0, 1, second);
    for (QCPAxisRect* rect : { first, second })
    {
        rect->axis(QCPAxis::atBottom)->setRange(0, 10);
        rect->axis(QCPAxis::atLeft)->setRange(0, 10);
    }
    const auto lineOn = [&plot](QCPAxisRect* rect)
    {
        auto* line = new QCPItemLine(&plot);
        line->start->setAxes(rect->axis(QCPAxis::atBottom), rect->axis(QCPAxis::atLeft));
        line->end->setAxes(rect->axis(QCPAxis::atBottom), rect->axis(QCPAxis::atLeft));
        line->start->setCoords(1, 1);
        line->end->setCoords(9, 9);
        return line;
    };
    QCPItemLine* a = lineOn(first);
    QCPItemLine* b = lineOn(second);
    plot.replot();

    const std::pair<QCPAbstractItem*, QCPAxisRect*> ends[] = { { a, first }, { b, second } };
    for (const auto& [line, rect] : ends)
    {
        const QCP::Pixels end { rect->axis(QCPAxis::atBottom)->coordToPixel(9),
                                rect->axis(QCPAxis::atLeft)->coordToPixel(9) };
        QVERIFY2(line->hitTest(end, nullptr).has_value(), "each line ends in its own rect");
    }
}

void TestDrawable::defaultDataRangeFollowsTheExtentRules()
{
    Fixture f;
    auto* shape = new TestShape(f.plot.get());
    f.plot->replot();

    const std::optional<QCPDataBox> polygon = shape->drawnDataRange();
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
    const std::optional<QCPDataBox> disjoint = shape->drawnDataRange();
    QVERIFY(disjoint);
    QCOMPARE(disjoint->keyLower, 1.0);
    QCOMPARE(disjoint->keyUpper, 9.0);
    QCOMPARE(disjoint->valueLower, 1.0);
    QCOMPARE(disjoint->valueUpper, 9.0);

    shape->mode = TestShape::Mixed; // a ratio on the value axis: that side is unbounded
    shape->touch();
    f.plot->replot();
    const std::optional<QCPDataBox> mixed = shape->drawnDataRange();
    QVERIFY(mixed);
    QCOMPARE(mixed->keyLower, 2.0);
    QVERIFY(std::isinf(mixed->valueLower) && mixed->valueLower < 0);

    shape->mode = TestShape::Anchored; // an anchored position: both sides unbounded
    shape->touch();
    f.plot->replot();
    QVERIFY(!shape->drawnDataRange().has_value());
    QVERIFY(!shape->extent().has_value());

    shape->mode = TestShape::ZeroValue; // a box reaching zero cannot be indexed on a log axis
    shape->touch();
    f.plot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    f.plot->yAxis->setRange(0.1, 100);
    f.plot->replot();
    const std::optional<QCPDataBox> logarithmic = shape->drawnDataRange();
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

void TestDrawable::legendIconFollowsTheTheme()
{
    Fixture f;
    auto* line = new TestLine(f.plot->xAxis, f.plot->yAxis);
    line->readsTheme = true;
    const QRectF icon(0, 0, 100, 50);
    const auto ink = [&]
    {
        QImage image(100, 50, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::white);
        QCPPainter painter(&image);
        line->paintIcon(&painter, icon);
        painter.end();
        return image.pixelColor(50, 25);
    };
    const QColor before = ink();
    QVERIFY2(before.lightness() < 200, qPrintable(before.name()));

    // The icon read the theme, so a new one leaves it stale - like the drawing it stands for.
    f.plot->theme()->setForeground(Qt::red);
    f.plot->applyTheme();
    const QColor after = ink();
    QVERIFY2(after.red() > 150 && after.green() < 120 && after.blue() < 120,
             qPrintable(QStringLiteral("the icon kept its old colour: %1").arg(after.name())));
}

void TestDrawable::flatLineHasALegendIcon()
{
    // A horizontal line has no value extent: fitting it to the icon rect must widen that side, not
    // reject the box and leave the icon blank.
    Fixture f;
    auto* line = new TestLine(f.plot->xAxis, f.plot->yAxis);
    line->mLine = std::make_shared<const std::vector<QPointF>>(
        std::initializer_list<QPointF> { { 1, 3 }, { 9, 3 } });
    const QRectF icon(0, 0, 100, 50);
    QImage image(100, 50, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QCPPainter painter(&image);
    line->paintIcon(&painter, icon);
    painter.end();

    QVERIFY2(image.pixelColor(50, 25).lightness() < 200,
             qPrintable(QString("centre is %1").arg(image.pixelColor(50, 25).name())));
}

void TestDrawable::logAxisRescaleKeepsThePositivePart()
{
    // The recording reaches zero, so the item index drops the value side on a log axis; a
    // plottable's range is data, and rescaleAxes must still fit the positive part.
    Fixture f;
    auto* line = new TestLine(f.plot->xAxis, f.plot->yAxis);
    line->mLine = std::make_shared<const std::vector<QPointF>>(
        std::initializer_list<QPointF> { { 1, 0 }, { 2, 5 }, { 3, 10 } });
    f.plot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    f.plot->yAxis->setRange(0.1, 100);

    f.plot->yAxis->rescale();

    QVERIFY2(f.plot->yAxis->range().lower > 0, "a log range must stay positive");
    QVERIFY2(f.plot->yAxis->range().upper < 50, "the range must fit the data, not stay at 100");
}

void TestDrawable::setPenReRecordsOnce()
{
    Fixture f;
    auto* line = new TestLine(f.plot->xAxis, f.plot->yAxis);
    f.plot->replot();
    QCOMPARE(line->drawCalls, 1);

    line->setPen(QPen(Qt::red, 3)); // an old-base setter: not contentChanged()
    f.plot->replot();
    QCOMPARE(line->drawCalls, 2);
}

void TestDrawable::setCoordsMovesTheItem()
{
    Fixture f;
    auto* shape = new TestShape(f.plot.get());
    shape->mode = TestShape::FromAnchor; // a box at the item position's coords, (3,3)
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 1);
    const std::optional<QCPDataBox> before = shape->drawnDataRange();
    QVERIFY(before);
    QCOMPARE(before->keyLower, 3.0);
    QCOMPARE(before->valueLower, 3.0);

    shape->mAnchor->setCoords(5, 6); // an old-base setter, through QCPAbstractItem::markDirty
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 2);
    const std::optional<QCPDataBox> after = shape->drawnDataRange();
    QVERIFY(after);
    QCOMPARE(after->keyLower, 5.0);
    QCOMPARE(after->valueLower, 6.0);
}

void TestDrawable::selectionChangeReRecords()
{
    Fixture f;
    auto* shape = new TestShape(f.plot.get());
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 1);

    shape->setSelected(true); // QCPAbstractItem::setSelected marks the layer, not the recording
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 2);
}

void TestDrawable::clickSelectsADrawnPlottable()
{
    Fixture f;
    auto* line = new TestLine(f.plot->xAxis, f.plot->yAxis);
    line->setSelectable(QCP::stWhole);
    f.plot->replot();
    QVERIFY(!line->selected());

    // A click right on the recorded line: the adapter must hand selectEvent a data selection,
    // or it sets an empty one and the plottable stays unselected.
    line->clickSelect(QPointF(f.plot->xAxis->coordToPixel(5), f.plot->yAxis->coordToPixel(5)));
    QVERIFY(line->selected());
    QCOMPARE(line->selection().dataRangeCount(), 1);
}

void TestDrawable::clickOutsideTheAxisRectMisses()
{
    Fixture f;
    auto* line = new TestLine(f.plot->xAxis, f.plot->yAxis);
    line->setSelectable(QCP::stWhole);
    // Data (1, 1), one end of the recorded line, now maps outside the axis rect.
    f.plot->xAxis->setRange(5, 10);
    f.plot->yAxis->setRange(5, 10);
    f.plot->replot();
    const QPointF outside(f.plot->xAxis->coordToPixel(1), f.plot->yAxis->coordToPixel(1));
    QVERIFY(!f.plot->axisRect()->rect().contains(outside.toPoint()));

    // Built-in plottables refuse a click outside the axis rect, though the geometry is there.
    QVERIFY(line->selectTest(outside, true) < 0);

    f.plot->setInteractions(QCP::iSelectPlottablesBeyondAxisRect);
    QVERIFY(line->selectTest(outside, true) >= 0);
}

void TestDrawable::mixedCornerRectFollowsAPan()
{
    // A rect with one data corner and one pixel corner cannot be re-emitted with one delta. This
    // replaces the offset assertion the old test made (every entry at offset zero) with the pixels
    // the user sees: the data corner follows the axis, the pixel corner stays where it is.
    Fixture f;
    if (!f.hasRhi())
        QSKIP("No QRhi available - the kept triangles are a GPU path only");
    hideAxes(f.plot.get());
    auto* shape = new TestShape(f.plot.get());
    shape->mode = TestShape::MixedRect; // data (2, 2) to pixel (50, 50)
    f.plot->replot(QCustomPlot::rpImmediateRefresh);
    const QRect before = inkedBounds(f.plot->grabFramebuffer());
    const double corner = f.plot->yAxis->coordToPixel(2.0);
    QVERIFY2(!before.isNull(), "the rect must be painted");

    f.plot->yAxis->setRange(2, 12); // a pan: only the data corner's pixels move
    f.plot->replot(QCustomPlot::rpImmediateRefresh);
    const QRect after = inkedBounds(f.plot->grabFramebuffer());
    const double moved = f.plot->yAxis->coordToPixel(2.0) - corner;
    QVERIFY2(qAbs((after.bottom() - before.bottom()) - moved) <= 2,
             qPrintable(QStringLiteral("the data corner moved %1 px, the axis %2 px")
                            .arg(after.bottom() - before.bottom())
                            .arg(moved)));
    QVERIFY2(
        qAbs(after.top() - before.top()) <= 1,
        qPrintable(QStringLiteral("the pixel corner moved %1 px").arg(after.top() - before.top())));
}

void TestDrawable::anchoredShapeFollowsItsParent()
{
    // An anchored position is resolved from the item it hangs on: moving that item must move the
    // shape, with no pan and no re-record to help.
    Fixture f;
    if (!f.hasRhi())
        QSKIP("No QRhi available - the kept triangles are a GPU path only");
    hideAxes(f.plot.get());
    auto* host = new QCPItemLine(f.plot.get());
    host->setPen(QPen(Qt::NoPen)); // the anchor's item must not be part of the ink
    host->start->setCoords(3, 3);
    auto* shape = new TestShape(f.plot.get());
    shape->mode = TestShape::Anchored; // a quad at the anchor, 20 px across
    shape->externalAnchor = host->start;
    f.plot->replot(QCustomPlot::rpImmediateRefresh);
    const QRect before = inkedBounds(f.plot->grabFramebuffer());
    QVERIFY2(!before.isNull(), "the anchored shape must be painted");
    const int records = shape->recordings();
    const quint64 strokes = QCPLineExtruder::extruderCallCount();

    host->start->setCoords(6, 6);
    f.plot->replot(QCustomPlot::rpImmediateRefresh);
    // The recording is kept - the anchor moved, nothing the object knows changed - and its vertices
    // are rebuilt because a recording of positions is neither data-only nor static, so it may not
    // be re-emitted from the kept geometry of another frame.
    QCOMPARE(shape->recordings(), records);
    QVERIFY2(QCPLineExtruder::extruderCallCount() > strokes,
             "the anchored geometry was not resolved again");
    const QRect after = inkedBounds(f.plot->grabFramebuffer());
    const QPointF moved(f.plot->xAxis->coordToPixel(6) - f.plot->xAxis->coordToPixel(3),
                        f.plot->yAxis->coordToPixel(6) - f.plot->yAxis->coordToPixel(3));
    QVERIFY2(qAbs((after.left() - before.left()) - moved.x()) <= 2,
             qPrintable(QStringLiteral("the shape moved %1 px, the anchor %2 px")
                            .arg(after.left() - before.left())
                            .arg(moved.x())));
    QVERIFY2(qAbs((after.top() - before.top()) - moved.y()) <= 2,
             qPrintable(QStringLiteral("the shape moved %1 px, the anchor %2 px")
                            .arg(after.top() - before.top())
                            .arg(moved.y())));
}

void TestDrawable::verticalSpanFollowsAnXPan()
{
    // A span with data keys and ratio values: an x pan moves its sides, not its top or bottom.
    Fixture f;
    if (!f.hasRhi())
        QSKIP("No QRhi available - the kept triangles are a GPU path only");
    hideAxes(f.plot.get());
    auto* shape = new TestShape(f.plot.get());
    shape->mode = TestShape::Ratio; // keys 2 to 6, values 0.2 to 0.8 of the axis rect
    f.plot->replot(QCustomPlot::rpImmediateRefresh);
    const QRect before = inkedBounds(f.plot->grabFramebuffer());
    const double edge = f.plot->xAxis->coordToPixel(2.0);
    QVERIFY2(!before.isNull(), "the span must be painted");

    f.plot->xAxis->setRange(1, 11); // a pan
    f.plot->replot(QCustomPlot::rpImmediateRefresh);
    const QRect after = inkedBounds(f.plot->grabFramebuffer());
    const double moved = f.plot->xAxis->coordToPixel(2.0) - edge;
    QVERIFY2(qAbs((after.left() - before.left()) - moved) <= 2,
             qPrintable(QStringLiteral("the span's side moved %1 px, the axis %2 px")
                            .arg(after.left() - before.left())
                            .arg(moved)));
    QVERIFY2(qAbs(after.top() - before.top()) <= 1, "the ratio side moved");
    QVERIFY2(qAbs(after.bottom() - before.bottom()) <= 1, "the ratio side moved");
}

void TestDrawable::dashedUnderOpaqueRectKeepsTheOrderOnAPan()
{
    // A pan frame re-submits the kept runs. While it walked one kind at a time (all solids, then
    // all dashes, then all rasters) a dashed line recorded under an opaque rect came back on top
    // of it: the record order has to survive the reuse.
    Fixture f;
    if (!f.hasRhi())
        QSKIP("No QRhi available - the kept triangles are a GPU path only");
    hideAxes(f.plot.get());
    auto* shape = new TestShape(f.plot.get());
    shape->mode = TestShape::DashedUnderRect; // blue dashes, then a green rect from (2,2) to (8,8)
    f.plot->replot(QCustomPlot::rpImmediateRefresh);
    QImage frame = f.plot->grabFramebuffer();
    int covered
        = countPixels(frame, deviceRect(f.plot.get(), frame, { 2.5, 2.5 }, { 7.5, 7.5 }), isBlue);
    QVERIFY2(covered == 0,
             qPrintable(QStringLiteral("the dashes show through the rect on the "
                                       "first frame: %1 pixels")
                            .arg(covered)));

    f.plot->xAxis->setRange(1, 11); // a pan: the kept runs are re-submitted
    f.plot->replot(QCustomPlot::rpImmediateRefresh);
    frame = f.plot->grabFramebuffer();
    covered
        = countPixels(frame, deviceRect(f.plot.get(), frame, { 2.5, 2.5 }, { 7.5, 7.5 }), isBlue);
    QVERIFY2(covered == 0,
             qPrintable(QStringLiteral("the dashes were drawn over the rect on the "
                                       "pan frame: %1 pixels")
                            .arg(covered)));
}

void TestDrawable::rasterUnderOpaqueRectKeepsTheOrderOnAPan()
{
    // The same for a rasterised command: the gradient fill goes to an image, the opaque rect over
    // it is a solid run, and the image must stay under it on a reuse frame too.
    Fixture f;
    if (!f.hasRhi())
        QSKIP("No QRhi available - the kept triangles are a GPU path only");
    hideAxes(f.plot.get());
    auto* shape = new TestShape(f.plot.get());
    shape->mode = TestShape::RasterUnderRect; // a gradient from (2,2) to (7,7), green over 3..6
    f.plot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY2(isGreenInk(pixelAt(f.plot->grabFramebuffer(), f.plot.get(), 4.5, 4.5)),
             "the opaque rect is not over the raster on the first frame");

    f.plot->xAxis->setRange(1, 11); // a pan: the kept runs are re-submitted
    f.plot->replot(QCustomPlot::rpImmediateRefresh);
    const QColor centre = pixelAt(f.plot->grabFramebuffer(), f.plot.get(), 4.5, 4.5);
    QVERIFY2(isGreenInk(centre),
             qPrintable(QStringLiteral("the raster was drawn over the rect on the pan frame: %1")
                            .arg(centre.name())));
}

void TestDrawable::pannedTimeAxisKeepsTheShapeOnAxis()
{
    Fixture f;
    if (!f.hasRhi())
        QSKIP("No QRhi available - the kept triangles are a GPU path only");
    hideAxes(f.plot.get()); // only the shape's ink is measured
    const double start = 1.7e9;
    const double span = 99.9;
    f.plot->xAxis->setRange(start, start + span);
    auto* line = new TestLine(f.plot->xAxis, f.plot->yAxis);
    line->mLine = std::make_shared<const std::vector<QPointF>>(
        std::initializer_list<QPointF> { { start + 10.0, 2.0 }, { start + 90.0, 8.0 } });
    f.plot->replot(QCustomPlot::rpImmediateRefresh);
    const int before = inkedBounds(f.plot->grabFramebuffer()).left();
    const double lineStart = f.plot->xAxis->coordToPixel(start + 10.0);
    QVERIFY2(before > 0, "the line must be painted");

    // A drag that leaves the two spans an ulp apart: the kept triangles move by one delta, and it
    // must be the delta of the visible pixels, not the one measured at data 0.
    const double draggedSpan = span * (1.0 + 4e-9);
    f.plot->xAxis->setRange(start + 1.0, start + 1.0 + draggedSpan);
    f.plot->replot(QCustomPlot::rpImmediateRefresh);
    const int after = inkedBounds(f.plot->grabFramebuffer()).left();
    const double moved = f.plot->xAxis->coordToPixel(start + 10.0) - lineStart;
    QVERIFY2(qAbs((after - before) - moved) <= 1.0,
             qPrintable(QStringLiteral("the shape moved %1 px, the axis %2 px")
                            .arg(after - before)
                            .arg(moved)));
}

void TestDrawable::removedAnchorItemDoesNotDangle()
{
    Fixture f;
    auto* host = new QCPItemLine(f.plot.get());
    host->start->setCoords(3, 3);
    auto* shape = new TestShape(f.plot.get());
    shape->mode = TestShape::Anchored;
    shape->externalAnchor = host->start;
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 1);

    // The recording keeps a raw anchor pointer. Removing the item that owns it must invalidate
    // the recording: before the fix the next replay resolved a freed position (ASan UAF).
    f.plot->removeItem(host);
    shape->externalAnchor = shape->mAnchor; // the next recording names a live anchor
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 2);
}

void TestDrawable::rangeBandExampleDraws()
{
    // The example in examples/custom-drawable, drawn through the same replay as everything else:
    // its fill is where it says, and its label is inked.
    Fixture f;
    const QColor fill(120, 180, 240);
    auto* band = new RangeBand(f.plot->xAxis, f.plot->yAxis);
    band->setBrush(fill);
    band->setLabel(QStringLiteral("band"));
    band->setData(QVector<double> { 1, 2, 3, 4, 5, 6, 7, 8, 9 }, QVector<double>(9, 3.0),
                  QVector<double>(9, 5.0));
    // The probes sit on ticks: a dotted grid line's dash phase there differs by platform.
    f.plot->xAxis->grid()->setVisible(false);
    f.plot->yAxis->grid()->setVisible(false);
    f.plot->replot();
    const QImage frame = f.plot->toPixmap(300, 200).toImage();

    QCOMPARE(pixelAt(frame, f.plot.get(), 2.0, 4.0), fill);
    QCOMPARE(pixelAt(frame, f.plot.get(), 8.0, 3.5), fill);
    QCOMPARE(pixelAt(frame, f.plot.get(), 5.0, 2.0), QColor(Qt::white)); // outside the band

    const double dpr = frame.devicePixelRatio();
    const QRect aboveLabel(qRound(f.plot->xAxis->coordToPixel(5.0) * dpr) - 40,
                           qRound(f.plot->yAxis->coordToPixel(5.0) * dpr) - 24, 80, 24);
    int inked = 0;
    for (int y = aboveLabel.top(); y <= aboveLabel.bottom(); ++y)
        for (int x = aboveLabel.left(); x <= aboveLabel.right(); ++x)
            if (qGray(frame.pixel(x, y)) < 100)
                ++inked;
    QVERIFY2(inked > 10, qPrintable(QStringLiteral("%1 dark pixels, no label").arg(inked)));
}

void TestDrawable::adapterExposesThePublicSelectAndRanges()
{
    // I6: QCPAbstractPlottable declares selectTest, getKeyRange and getValueRange public. The
    // adapter's overrides may not narrow that: a user's `bars->selectTest(...)` and
    // `band->getKeyRange(...)` must keep compiling.
    Fixture f;
    auto* bars = new QCPBars(f.plot->xAxis, f.plot->yAxis);
    bars->setData(QVector<double> { 5 }, QVector<double> { 4 });
    auto* band = new RangeBand(f.plot->xAxis, f.plot->yAxis);
    band->setData(QVector<double> { 1, 9 }, QVector<double> { 3, 3 }, QVector<double> { 5, 5 });
    f.plot->replot();

    const QPointF onTheBar(f.plot->xAxis->coordToPixel(5), f.plot->yAxis->coordToPixel(2));
    QVERIFY(bars->selectTest(onTheBar, false) >= 0);
    QVERIFY(bars->selectTest(QPointF(-50, -50), false) < 0);

    bool foundKey = false;
    const QCPRange keys = band->getKeyRange(foundKey);
    QVERIFY(foundKey);
    QCOMPARE(keys.lower, 1.0);
    QCOMPARE(keys.upper, 9.0);
    bool foundValue = false;
    const QCPRange values = band->getValueRange(foundValue);
    QVERIFY(foundValue);
    QCOMPARE(values.lower, 3.0);
    QCOMPARE(values.upper, 5.0);
}

void TestDrawable::rangeBandRescalesToItsData()
{
    // I5: the label's anchor is a data position, so it must not unbound the recording. While it
    // did, getKeyRange/getValueRange reported nothing and rescaleAxes() left the band alone.
    Fixture f;
    auto* band = new RangeBand(f.plot->xAxis, f.plot->yAxis);
    QVector<double> keys, lower(9, 3.0), upper(9, 5.0);
    for (int i = 1; i <= 9; ++i)
        keys << double(i);
    band->setData(keys, lower, upper);
    f.plot->xAxis->setRange(0, 100);
    f.plot->yAxis->setRange(0, 100);

    f.plot->rescaleAxes();

    const QCPRange keys2 = f.plot->xAxis->range();
    QVERIFY2(keys2.lower > 0.5 && keys2.lower <= 1.0 && keys2.upper >= 9.0 && keys2.upper < 10.0,
             qPrintable(QStringLiteral("key range %1..%2, not the band's 1..9")
                            .arg(keys2.lower)
                            .arg(keys2.upper)));
    const QCPRange values = f.plot->yAxis->range();
    QVERIFY2(values.lower > 2.0 && values.lower <= 3.0 && values.upper >= 5.0 && values.upper < 6.0,
             qPrintable(QStringLiteral("value range %1..%2, not the band's 3..5")
                            .arg(values.lower)
                            .arg(values.upper)));
}

void TestDrawable::rangeBandLegendIconIsInked()
{
    // I5: the recording that bounds the band also feeds its legend icon. The data sits outside the
    // visible range, so the only pixels of the fill in the image are the icon's.
    Fixture f;
    const QColor fill(120, 180, 240);
    auto* band = new RangeBand(f.plot->xAxis, f.plot->yAxis);
    QVector<double> keys, lower(5, 3.0), upper(5, 5.0);
    for (int i = 0; i < 5; ++i)
        keys << 100.0 + i;
    band->setData(keys, lower, upper);
    band->setBrush(fill);
    f.plot->xAxis->setRange(0, 10);
    f.plot->yAxis->setRange(0, 10);
    f.plot->legend->setVisible(true);
    f.plot->replot();

    const QImage frame = f.plot->toPixmap(300, 200).toImage();
    int inked = 0;
    for (int y = 0; y < frame.height(); ++y)
        for (int x = 0; x < frame.width(); ++x)
            if (frame.pixelColor(x, y) == fill)
                ++inked;
    QVERIFY2(inked > 20,
             qPrintable(QStringLiteral("%1 fill pixels, the icon is blank").arg(inked)));
}

void TestDrawable::exportStacksLikeTheScreen()
{
    // Spec 5: a layer paints its QPainter content first, then the QPainter form of its GPU
    // commands. The glyph of an item text is painter content, so the bars of its own layer,
    // submitted before it, still cover it - on screen and in every export. Replaying each object
    // whole stacked the glyph above the bars in the exports only.
    Fixture f;
    if (!f.hasRhi())
        QSKIP("No QRhi available - this compares the screen with the export paths");

    auto* bars = new QCPBars(f.plot->xAxis, f.plot->yAxis);
    bars->setWidthType(QCPBars::wtPlotCoords);
    bars->setWidth(6);
    bars->setData(QVector<double> { 5 }, QVector<double> { 8 }); // 0..8, so it covers the text
    bars->setPen(QPen(Qt::NoPen));
    bars->setBrush(QColor(Qt::red));

    auto* text = new QCPItemText(f.plot.get()); // after the bars: topmost in the item index
    text->setLayer(QLatin1String("main")); // items default to "annotations", above the bars
    text->setText(QStringLiteral("X"));
    text->setPen(QPen(Qt::black));
    QFont font = text->font();
    font.setPixelSize(20);
    text->setFont(font);
    text->setPositionAlignment(Qt::AlignCenter);
    text->position->setCoords(5, 4);

    const QRect glyphBox(
        QPoint(qRound(f.plot->xAxis->coordToPixel(5)), qRound(f.plot->yAxis->coordToPixel(4)))
            - QPoint(10, 10),
        QSize(20, 20));
    const auto inkIn = [](const QImage& image, QRect box)
    {
        const double dpr = image.devicePixelRatio();
        int ink = 0;
        for (int y = box.top(); y <= box.bottom(); ++y)
            for (int x = box.left(); x <= box.right(); ++x)
            {
                const QColor colour = image.pixelColor(qRound(x * dpr), qRound(y * dpr));
                if (colour.red() < 80 && colour.green() < 80 && colour.blue() < 80)
                    ++ink;
            }
        return ink;
    };

    f.plot->replot(QCustomPlot::rpImmediateRefresh);
    const int onScreen = inkIn(f.plot->grabFramebuffer(), glyphBox);
    QVERIFY2(onScreen == 0,
             qPrintable(QStringLiteral("%1 glyph pixels over the bars on screen").arg(onScreen)));

    const QImage pixmap = f.plot->toPixmap(300, 200).toImage();
    const int inPixmap = inkIn(pixmap, glyphBox);
    QVERIFY2(inPixmap == 0,
             qPrintable(QStringLiteral("%1 glyph pixels over the bars in toPixmap").arg(inPixmap)));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("out.png"));
    QVERIFY(f.plot->savePng(path, 300, 200));
    const QImage png = QImage(path).convertToFormat(QImage::Format_ARGB32);
    QVERIFY(!png.isNull());
    const int inPng = inkIn(png, glyphBox);
    QVERIFY2(inPng == 0,
             qPrintable(QStringLiteral("%1 glyph pixels over the bars in savePng").arg(inPng)));
}

void TestDrawable::aPanInsideTheKeyWindowKeepsTheRecording()
{
    // A recording made for a window wider than the view survives a pan that stays inside it: the
    // replay moves the kept geometry by one delta, exactly like a recording with no window at all.
    Fixture f;
    auto* shape = new TestShape(f.plot.get());
    shape->readsKeyWindow = true;
    f.plot->xAxis->setRange(10, 20);
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 1);

    f.plot->xAxis->setRange(12.5, 22.5); // a quarter of a view width
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 1);
    QCOMPARE(shape->recordings(), 1);
}

void TestDrawable::aPanPastTheKeyWindowRecords()
{
    Fixture f;
    auto* shape = new TestShape(f.plot.get());
    shape->readsKeyWindow = true;
    f.plot->xAxis->setRange(10, 20);
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 1);

    f.plot->xAxis->setRange(25, 35); // one and a half view widths: the view left the window
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 2);
}

void TestDrawable::aZoomRecords()
{
    // A zoom changes pixels per key, so the half of the window that is off screen is no longer the
    // half the recording covers.
    Fixture f;
    auto* shape = new TestShape(f.plot.get());
    shape->readsKeyWindow = true;
    f.plot->xAxis->setRange(10, 20);
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 1);

    f.plot->xAxis->setRange(12.5, 17.5); // half the span, same centre
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 2);
}

void TestDrawable::aValueZoomKeepsTheRecording()
{
    // Only the key scale is part of the window: a value zoom keeps the recording and re-resolves
    // its commands through the frame that replays them.
    Fixture f;
    auto* shape = new TestShape(f.plot.get());
    shape->readsKeyWindow = true;
    f.plot->xAxis->setRange(10, 20);
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 1);
    const quint64 remaps = shape->remaps();

    f.plot->yAxis->setRange(0, 20);
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 1);
    QCOMPARE(shape->recordings(), 1);

    if (f.hasRhi())
    {
        f.plot->replot(); // the same transform: the resolve cache already holds these pixels
        QCOMPARE(shape->remaps(), remaps + 1);
    }
}

void TestDrawable::aLogKeyAxisRecordsEveryTransform()
{
    // Pixels per key are not constant on a log axis, so no window can describe them: the recording
    // is marked transform-dependent and remade on every pan.
    Fixture f;
    auto* shape = new TestShape(f.plot.get());
    shape->readsKeyWindow = true;
    f.plot->xAxis->setScaleType(QCPAxis::stLogarithmic);
    f.plot->xAxis->setRange(1, 10);
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 1);

    f.plot->xAxis->setRange(2, 20);
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 2);

    f.plot->xAxis->setRange(4, 40);
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 3);
}

// A linear-to-log switch can keep the range and the axis width, so the window's scale test alone
// would keep a recording made for a linear axis.
void TestDrawable::switchingTheKeyAxisToLogRecords()
{
    Fixture f;
    auto* shape = new TestShape(f.plot.get());
    shape->readsKeyWindow = true;
    f.plot->xAxis->setRange(1, 10);
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 1);

    f.plot->xAxis->setScaleType(QCPAxis::stLogarithmic);
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 2);
}

void TestDrawable::keyWindowCoversOneViewWidthEachSide()
{
    Fixture f;
    auto* shape = new TestShape(f.plot.get());
    shape->readsKeyWindow = true;
    f.plot->xAxis->setRange(10, 20);
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 1);

    QCOMPARE(shape->keyWindow.keys.lower, 0.0);
    QCOMPARE(shape->keyWindow.keys.upper, 30.0);
    const double width = f.plot->axisRect()->rect().width();
    QVERIFY2(qAbs(shape->keyWindow.pixelsPerKey - width / 10.0) < 1e-9,
             qPrintable(QStringLiteral("pixelsPerKey %1, axis rect %2 px")
                            .arg(shape->keyWindow.pixelsPerKey)
                            .arg(width)));
}

void TestDrawable::aResizeRecords()
{
    // The axis rect's size is part of the scale: a resize changes pixels per key even though the
    // data range did not move, so the kept window is no longer the one the recording drew.
    Fixture f;
    f.plot->show();
    QVERIFY(QTest::qWaitForWindowExposed(f.plot.get()));
    auto* plain = new TestShape(f.plot.get());
    auto* shape = new TestShape(f.plot.get());
    shape->readsKeyWindow = true;
    f.plot->xAxis->setRange(10, 20);
    f.plot->replot();
    QCOMPARE(plain->drawCalls, 1);
    QCOMPARE(shape->drawCalls, 1);
    const int width = f.plot->axisRect()->rect().width();

    f.plot->resize(400, 300);
    f.plot->replot();
    QVERIFY2(f.plot->axisRect()->rect().width() != width, "the resize must change the axis rect");
    QCOMPARE(plain->drawCalls, 1); // a resize alone is not a content or view change for it
    QCOMPARE(shape->drawCalls, 2);
}

void TestDrawable::aVerticalKeyAxisUsesTheSameKeyWindow()
{
    // pixelsPerKey is a magnitude: the vertical key axis takes its pixels from the axis rect's
    // height, and the same window still lets a pan inside it keep the recording.
    Fixture f;
    auto* shape = new TestShape(f.plot.get());
    shape->mAnchor->setAxes(f.plot->yAxis, f.plot->xAxis); // key axis vertical, value horizontal
    shape->readsKeyWindow = true;
    f.plot->yAxis->setRange(10, 20);
    f.plot->xAxis->setRange(0, 10);
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 1);

    QCOMPARE(shape->keyWindow.keys.lower, 0.0);
    QCOMPARE(shape->keyWindow.keys.upper, 30.0);
    const double height = f.plot->axisRect()->rect().height();
    QVERIFY2(qAbs(shape->keyWindow.pixelsPerKey - height / 10.0) < 1e-9,
             qPrintable(QStringLiteral("pixelsPerKey %1, axis rect %2 px")
                            .arg(shape->keyWindow.pixelsPerKey)
                            .arg(height)));

    f.plot->yAxis->setRange(12.5, 22.5); // inside the window, one delta
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 1);
}

void TestDrawable::aKeyWindowPanReusesTheKeptGeometry()
{
    // The window is a data-space promise, so the recording still moves with the data: a pan inside
    // it hands the kept vertices back with one offset instead of building them again.
    Fixture f;
    if (!f.hasRhi())
        QSKIP("No QRhi available - the kept geometry is a GPU path only");
    auto* shape = new TestShape(f.plot.get());
    shape->readsKeyWindow = true;
    f.plot->xAxis->setRange(10, 20);
    f.plot->replot();
    const quint64 extruded = QCPLineExtruder::extruderCallCount();
    QVERIFY2(extruded > 0, "the first frame must extrude the line");
    QCOMPARE(shape->drawCalls, 1);

    f.plot->xAxis->setRange(12.5, 22.5); // inside the window: a translation
    f.plot->replot();
    QCOMPARE(shape->drawCalls, 1);
    QCOMPARE(QCPLineExtruder::extruderCallCount(), extruded);
}

void TestDrawable::aPlottableRecordsWithoutTheAdapter()
{
    // The base owns the recording: a plottable that overrides only draw(QCPDrawContext&) records,
    // hit-tests and draws its legend icon without any adapter in between.
    Fixture f;
    auto* line = new BareLine(f.plot->xAxis, f.plot->yAxis);
    QCOMPARE(line->recordings(), 0); // nothing recorded before the first frame
    f.plot->replot();
    QCOMPARE(line->recordings(), 1);
    QCOMPARE(line->recordedCommands(), 1);
    QCOMPARE(line->drawCalls, 1);

    const QPointF onTheLine(f.plot->xAxis->coordToPixel(5), f.plot->yAxis->coordToPixel(5));
    QVERIFY2(line->selectTest(onTheLine, false) < 1.0, "the recorded line is not hittable");

    const QRectF icon(0, 0, 100, 50);
    QImage image(100, 50, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QCPPainter painter(&image);
    line->paintIcon(&painter, icon);
    painter.end();
    QVERIFY2(image.pixelColor(50, 25).lightness() < 200, "the legend icon is blank");
}

void TestDrawable::anItemRecordsWithoutTheAdapter()
{
    Fixture f;
    auto* shape = new BareShape(f.plot.get());
    QCOMPARE(shape->recordings(), 0);
    f.plot->replot();
    QCOMPARE(shape->recordings(), 1);
    QCOMPARE(shape->recordedCommands(), 1);
    QCOMPARE(shape->drawCalls, 1);

    const QPointF onTheLine(f.plot->xAxis->coordToPixel(5), f.plot->yAxis->coordToPixel(5));
    QVERIFY2(shape->selectTest(onTheLine, false) < 1.0, "the recorded item is not hittable");
}

void TestDrawable::aStyleSetterReRecords()
{
    // setPen is a content change: the recording is remade, not replayed in the old colour.
    Fixture f;
    auto* bars = new CountingBars(f.plot->xAxis, f.plot->yAxis);
    bars->setData(QVector<double> { 5 }, QVector<double> { 4 });
    f.plot->replot();
    const int before = bars->recordings();
    QCOMPARE(before, 1);

    bars->setPen(QPen(Qt::red));
    f.plot->replot();
    QCOMPARE(bars->recordings(), before + 1);
}
