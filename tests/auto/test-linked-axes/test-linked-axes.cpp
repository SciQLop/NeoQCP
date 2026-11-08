#include "test-linked-axes.h"

#include <coords.h>

#include "items/item-vspan.h"

#include <QGuiApplication>

#include <QSet>
#include <cmath>
#include <limits>

namespace
{
int gMisuses = 0;
void record(const char*, const char*) { ++gMisuses; }

const auto twice = [](double x) { return 2.0 * x; };
const auto half = [](double x) { return x / 2.0; };
const auto reciprocal = [](double x) { return 1.0 / x; };

bool near(double a, double b) { return std::abs(a - b) <= 1e-9; }

void sendDrag(QCustomPlot& plot, const QPoint& from, const QPoint& to)
{
    auto send = [&plot](QEvent::Type type, const QPoint& pos, Qt::MouseButtons buttons) {
        QMouseEvent e(type, pos, plot.mapToGlobal(pos), Qt::LeftButton, buttons, Qt::NoModifier);
        QCoreApplication::sendEvent(&plot, &e);
    };
    send(QEvent::MouseButtonPress, from, Qt::LeftButton);
    send(QEvent::MouseMove, (from + to) / 2, Qt::LeftButton);
    send(QEvent::MouseMove, to, Qt::LeftButton);
    send(QEvent::MouseButtonRelease, to, Qt::NoButton);
}
} // namespace

void TestLinkedAxes::init()
{
    gMisuses = 0;
    qcp::setMisuseHandler(record);
}

void TestLinkedAxes::cleanup()
{
    qcp::setMisuseHandler(nullptr);
}

void TestLinkedAxes::linkedRangeFollowsTheSource()
{
    QCustomPlot plot;
    plot.xAxis->setRange(1, 10);
    plot.xAxis2->linkTo(plot.xAxis, twice, half);
    QCOMPARE(plot.xAxis2->linkSource(), plot.xAxis);
    QVERIFY(plot.xAxis2->link());
    QCOMPARE(plot.xAxis2->range(), QCPRange(2, 20));

    plot.xAxis->setRange(2, 4);
    QCOMPARE(plot.xAxis2->range(), QCPRange(4, 8));
    QCOMPARE(gMisuses, 0);
}

void TestLinkedAxes::everyMutatorMovesTheSource()
{
    QCustomPlot plot;
    plot.xAxis->setRange(1, 10);
    plot.xAxis2->linkTo(plot.xAxis, twice, half);

    plot.xAxis2->setRange(10, 20);
    QCOMPARE(plot.xAxis->range(), QCPRange(5, 10));
    QCOMPARE(plot.xAxis2->range(), QCPRange(10, 20));

    plot.xAxis2->scaleRange(2, 15);
    QCOMPARE(plot.xAxis2->range(), QCPRange(5, 25));
    QCOMPARE(plot.xAxis->range(), QCPRange(2.5, 12.5));

    plot.xAxis2->moveRange(2);
    QCOMPARE(plot.xAxis2->range(), QCPRange(7, 27));
    QCOMPARE(plot.xAxis->range(), QCPRange(3.5, 13.5));

    plot.xAxis2->setRangeUpper(30);
    QCOMPARE(plot.xAxis2->range(), QCPRange(7, 30));
    QCOMPARE(plot.xAxis->range(), QCPRange(3.5, 15));

    plot.xAxis2->setRangeLower(9);
    QCOMPARE(plot.xAxis2->range(), QCPRange(9, 30));
    QCOMPARE(plot.xAxis->range(), QCPRange(4.5, 15));
    QCOMPARE(gMisuses, 0);
}

void TestLinkedAxes::writeBackSettles()
{
    QCustomPlot plot;
    plot.xAxis->setRange(1, 10);
    plot.xAxis2->linkTo(
        plot.xAxis, [](double x) { return x * 3.0; }, [](double x) { return x / 3.0; });

    int linkedSignals = 0;
    int sourceSignals = 0;
    connect(plot.xAxis2, &QCPAxis::rangeChanged, &plot, [&linkedSignals] { ++linkedSignals; });
    connect(plot.xAxis, &QCPAxis::rangeChanged, &plot, [&sourceSignals] { ++sourceSignals; });

    {
        const QCPEmissionScope scope(&plot);
        plot.xAxis2->setRange(1, 2);
    }
    QCOMPARE(sourceSignals, 1);
    QCOMPARE(linkedSignals, 1);
    const QCPRange source = plot.xAxis->range();
    QVERIFY(near(plot.xAxis2->range().lower, 3.0 * source.lower));
    QVERIFY(near(plot.xAxis2->range().upper, 3.0 * source.upper));

    // Writing the range it already has changes nothing.
    plot.xAxis2->setRange(plot.xAxis2->range());
    QCOMPARE(sourceSignals, 1);
    QCOMPARE(linkedSignals, 1);
    QCOMPARE(gMisuses, 0);
}

void TestLinkedAxes::linkedPixelsMatchTheSourceOnOneRect()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.xAxis->setRange(1, 10);
    plot.xAxis2->linkTo(plot.xAxis, twice, half);
    plot.replot();
    for (const double v : {1.0, 3.3, 10.0})
    {
        QVERIFY2(near(plot.xAxis2->coordToPixel(2 * v), plot.xAxis->coordToPixel(v)),
                 "a linear 2x link maps where its source maps");
        QVERIFY2(near(plot.xAxis2->pixelToCoord(plot.xAxis2->coordToPixel(2 * v)), 2 * v),
                 "pixelToCoord undoes coordToPixel");
    }

    plot.xAxis->setScaleType(QCPAxis::stLogarithmic);
    plot.xAxis->setRange(1, 100);
    for (const double v : {1.0, 3.3, 10.0})
        QVERIFY2(near(plot.xAxis2->coordToPixel(2 * v), plot.xAxis->coordToPixel(v)),
                 "a link on a log source maps where its source maps");

    plot.xAxis->setScaleType(QCPAxis::stLinear);
    plot.xAxis->setRange(1, 10);
    plot.xAxis->setRangeReversed(true);
    QVERIFY2(plot.xAxis2->rangeReversed(), "the link follows the source's reversal");
    for (const double v : {1.0, 3.3, 10.0})
        QVERIFY2(near(plot.xAxis2->coordToPixel(2 * v), plot.xAxis->coordToPixel(v)),
                 "a link on a reversed source maps where its source maps");
    QCOMPARE(gMisuses, 0);
}

void TestLinkedAxes::linkOnAnotherRectUsesItsOwnLength()
{
    QCustomPlot plot;
    plot.resize(600, 300);
    QCPAxisRect* second = new QCPAxisRect(&plot);
    plot.plotLayout()->addElement(0, 1, second);
    plot.plotLayout()->setColumnStretchFactor(0, 1);
    plot.plotLayout()->setColumnStretchFactor(1, 3);
    plot.xAxis->setRange(1, 10);
    QCPAxis* linked = second->axis(QCPAxis::atBottom);
    QVERIFY(linked);
    linked->linkTo(plot.xAxis);
    plot.replot();

    const QRect source = plot.xAxis->axisRect()->rect();
    const QRect own = linked->axisRect()->rect();
    QVERIFY2(own.width() > source.width(), "the second rect is wider");
    const double linkedPixel = linked->coordToPixel(plot.xAxis->range().center());
    QVERIFY2(near(linkedPixel, own.left() + own.width() / 2.0),
             "the linked axis maps the source's centre to its own centre");
    QVERIFY2(!near(linkedPixel, source.left() + source.width() / 2.0),
             "the linked axis uses its own length, not the source's");
    QCOMPARE(gMisuses, 0);
}

void TestLinkedAxes::decreasingLinkOrdersTheRange()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.xAxis->setRange(0.1, 1);
    plot.xAxis2->linkTo(plot.xAxis, reciprocal, reciprocal);
    plot.replot();

    QCOMPARE(plot.xAxis2->range(), QCPRange(1, 10));
    QVERIFY(plot.xAxis2->rangeReversed());
    QCOMPARE(plot.xAxis2->pixelOrientation(), -1);
    QVERIFY2(near(plot.xAxis2->coordToPixel(10), plot.xAxis->coordToPixel(0.1)),
             "the smallest linked value sits where the source's lower bound sits");
    QVERIFY2(near(plot.xAxis2->coordToPixel(1), plot.xAxis->coordToPixel(1.0)),
             "the largest linked value sits where the source's upper bound sits");
    QCOMPARE(gMisuses, 0);
}

void TestLinkedAxes::barsOnADecreasingLink()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.xAxis->setRange(0.1, 1);
    plot.yAxis->setRange(0, 10);
    plot.xAxis2->linkTo(plot.xAxis, reciprocal, reciprocal);

    QCPBars* bars = new QCPBars(plot.xAxis2, plot.yAxis);
    bars->setPen(Qt::NoPen);
    bars->setBrush(Qt::red);
    bars->setWidth(0.5);
    bars->setData({2.0}, {5.0});
    plot.replot();

    const QImage image = plot.toPixmap().toImage();
    int first = image.width();
    int last = -1;
    for (int x = 0; x < image.width(); ++x)
    {
        for (int y = 0; y < image.height(); ++y)
        {
            const QRgb pixel = image.pixel(x, y);
            if (qRed(pixel) > 200 && qGreen(pixel) < 80 && qBlue(pixel) < 80)
            {
                first = std::min(first, x);
                last = std::max(last, x);
            }
        }
    }
    QVERIFY2(last >= first, "the bar is drawn");
    const double atLower = plot.xAxis2->coordToPixel(2.0 - 0.25);
    const double atUpper = plot.xAxis2->coordToPixel(2.0 + 0.25);
    const double left = std::min(atLower, atUpper);
    const double right = std::max(atLower, atUpper);
    QVERIFY2(std::abs(first - left) <= 2.0 && std::abs(last - right) <= 2.0,
             "the bar spans the pixels of its two key bounds");
    QCOMPARE(plot.xAxis2->pixelOrientation(), atLower < atUpper ? 1 : -1);
    QCOMPARE(gMisuses, 0);
}

void TestLinkedAxes::nonFiniteInverseIsRefused()
{
    QCustomPlot plot;
    plot.xAxis->setRange(0.1, 1);
    plot.xAxis2->linkTo(plot.xAxis, reciprocal, reciprocal);
    const QCPRange source = plot.xAxis->range();
    const QCPRange linked = plot.xAxis2->range();

    plot.xAxis2->setRange(0, 5); // the inverse of 0 is infinite
    QCOMPARE(gMisuses, 0); // a data condition, not a misuse
    QCOMPARE(plot.xAxis->range(), source);
    QCOMPARE(plot.xAxis2->range(), linked);
}

void TestLinkedAxes::reversingTheSourceRefreshesTheLink()
{
    QCustomPlot plot;
    plot.xAxis->setRange(1, 10);
    plot.xAxis2->linkTo(plot.xAxis, twice, half);
    QVERIFY(!plot.xAxis2->rangeReversed());

    plot.xAxis->setRangeReversed(true);
    QVERIFY(plot.xAxis2->rangeReversed());
    plot.xAxis->setRangeReversed(false);
    QVERIFY(!plot.xAxis2->rangeReversed());

    // The linked axis follows its source: it does not own its direction.
    plot.xAxis2->setRangeReversed(true);
    QCOMPARE(gMisuses, 1);
    QVERIFY(!plot.xAxis2->rangeReversed());

    // A decreasing link reverses on a plain source and un-reverses on a reversed one.
    QCustomPlot decreasing;
    decreasing.xAxis->setRange(0.1, 1);
    decreasing.xAxis2->linkTo(decreasing.xAxis, reciprocal, reciprocal);
    QVERIFY(decreasing.xAxis2->rangeReversed());
    decreasing.xAxis->setRangeReversed(true);
    QVERIFY(!decreasing.xAxis2->rangeReversed());
}

void TestLinkedAxes::dataOnALinkedAxisMapsThroughTheLink()
{
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.xAxis->setRange(1, 10);
    plot.yAxis->setRange(0, 5);
    plot.xAxis2->linkTo(plot.xAxis, twice, half);
    plot.replot();

    const QCPTransform linked(plot.xAxis2, plot.yAxis);
    const QCPTransform plain(plot.xAxis, plot.yAxis);
    const QCPTransform again(plot.xAxis2, plot.yAxis);
    QVERIFY2(linked == again, "two snapshots of one link compare equal");

    const QVector<double> linkedKeys {2.0, 6.6, 20.0};
    const QVector<double> plainKeys {1.0, 3.3, 10.0};
    const QVector<double> values {0.0, 2.5, 5.0};
    for (int i = 0; i < linkedKeys.size(); ++i)
    {
        const QCP::Pixels throughLink = linked.map({linkedKeys.at(i), values.at(i)});
        const QCP::Pixels direct = plain.map({plainKeys.at(i), values.at(i)});
        QVERIFY2(near(throughLink.x, direct.x) && near(throughLink.y, direct.y),
                 "a linked key maps where its source maps");
    }

    QVector<QPointF> mapped(linkedKeys.size());
    linked.map(std::span<const double>(linkedKeys.constData(), linkedKeys.size()),
               std::span<const double>(values.constData(), values.size()),
               std::span<QPointF>(mapped.data(), mapped.size()));
    for (int i = 0; i < mapped.size(); ++i)
    {
        const QCP::Pixels one = linked.map({linkedKeys.at(i), values.at(i)});
        QVERIFY2(near(mapped.at(i).x(), one.x) && near(mapped.at(i).y(), one.y),
                 "the bulk map agrees with map()");
    }

    plot.xAxis->setRange(2, 11);
    const QCPTransform panned(plot.xAxis2, plot.yAxis);
    QVERIFY2(!linked.isTranslationOf(panned), "a linked axis is never a translation");
    QVERIFY(!(linked == panned));
}

void TestLinkedAxes::severalAxesShareASource()
{
    QCustomPlot plot;
    plot.xAxis->setRange(1, 10);
    QCPAxis* identity = plot.axisRect()->addAxis(QCPAxis::atBottom);
    QCPAxis* doubled = plot.axisRect()->addAxis(QCPAxis::atBottom);
    identity->linkTo(plot.xAxis);
    doubled->linkTo(plot.xAxis, twice, half);
    QCOMPARE(gMisuses, 0);
    QCOMPARE(identity->range(), QCPRange(1, 10));
    QCOMPARE(doubled->range(), QCPRange(2, 20));

    plot.xAxis->setRange(2, 4);
    QCOMPARE(identity->range(), QCPRange(2, 4));
    QCOMPARE(doubled->range(), QCPRange(4, 8));

    doubled->setRange(10, 20); // writes back through the inverse, the other link follows
    QCOMPARE(plot.xAxis->range(), QCPRange(5, 10));
    QCOMPARE(identity->range(), QCPRange(5, 10));
    QCOMPARE(doubled->range(), QCPRange(10, 20));

    identity->setRange(1, 3);
    QCOMPARE(plot.xAxis->range(), QCPRange(1, 3));
    QCOMPARE(doubled->range(), QCPRange(2, 6));
    QCOMPARE(gMisuses, 0);
}

void TestLinkedAxes::aLinksSourceCanBeLinkedAgain()
{
    QCustomPlot plot;
    QCPAxis* first = plot.axisRect()->addAxis(QCPAxis::atBottom);
    QCPAxis* second = plot.axisRect()->addAxis(QCPAxis::atBottom);
    first->linkTo(plot.xAxis, twice, half);
    QCOMPARE(first->linkSource(), plot.xAxis);

    plot.axisRect()->removeAxis(first); // a dead link must not keep the source busy
    plot.xAxis->linkTo(second, twice, half);
    QCOMPARE(gMisuses, 0);
    QCOMPARE(plot.xAxis->linkSource(), second);

    second->setRange(1, 5);
    QCOMPARE(plot.xAxis->range(), QCPRange(2, 10));
}

void TestLinkedAxes::scaleTypeOnALinkedAxisLeavesTheSourceAlone()
{
    QCustomPlot plot;
    plot.xAxis->setRange(-5, 5);
    plot.xAxis2->linkTo(plot.xAxis);
    QCOMPARE(plot.xAxis2->range(), QCPRange(-5, 5));

    plot.xAxis2->setScaleType(QCPAxis::stLogarithmic);
    QCOMPARE(gMisuses, 0);
    QCOMPARE(plot.xAxis->range(), QCPRange(-5, 5));
    QCOMPARE(plot.xAxis2->range(), QCPRange(-5, 5));
    QCOMPARE(plot.xAxis2->scaleType(), QCPAxis::stLogarithmic);

    plot.xAxis->setRange(1, 10);
    QCOMPARE(plot.xAxis2->range(), QCPRange(1, 10));
}

void TestLinkedAxes::refusedLinks()
{
    QCustomPlot plot;
    QCustomPlot other;

    plot.xAxis2->linkTo(nullptr);
    QCOMPARE(gMisuses, 1);
    QVERIFY(!plot.xAxis2->linkSource());

    plot.xAxis2->linkTo(plot.xAxis2);
    QCOMPARE(gMisuses, 2);
    QVERIFY(!plot.xAxis2->linkSource());

    plot.xAxis2->linkTo(plot.yAxis); // another orientation
    QCOMPARE(gMisuses, 3);
    QVERIFY(!plot.xAxis2->linkSource());

    plot.xAxis2->linkTo(other.xAxis); // another plot
    QCOMPARE(gMisuses, 4);
    QVERIFY(!plot.xAxis2->linkSource());

    plot.xAxis2->linkTo(plot.xAxis, twice, {}); // one function empty
    QCOMPARE(gMisuses, 5);
    QVERIFY(!plot.xAxis2->linkSource());

    plot.xAxis2->linkTo(plot.xAxis, twice, half); // this one works
    QCOMPARE(gMisuses, 5);
    QCOMPARE(plot.xAxis2->linkSource(), plot.xAxis);

    QCPAxis* extra = plot.axisRect()->addAxis(QCPAxis::atBottom);
    QVERIFY(extra);
    extra->linkTo(plot.xAxis); // a source drives several links, so this one works
    QCOMPARE(gMisuses, 5);
    QCOMPARE(extra->linkSource(), plot.xAxis);

    plot.xAxis->linkTo(extra); // refused: that axis is a source
    QCOMPARE(gMisuses, 6);
    QVERIFY(!plot.xAxis->linkSource());

    extra->linkTo(plot.xAxis2); // refused: that axis is linked itself
    QCOMPARE(gMisuses, 7);
    QCOMPARE(extra->linkSource(), plot.xAxis); // the refused call left the first link alone
}

void TestLinkedAxes::deletedSourceLeavesAPlainAxis()
{
    QCustomPlot plot;
    QCPAxis* source = plot.axisRect()->addAxis(QCPAxis::atBottom);
    source->setRange(1, 10);
    plot.xAxis2->linkTo(source, twice, half);
    QCOMPARE(plot.xAxis2->range(), QCPRange(2, 20));

    plot.axisRect()->removeAxis(source);
    QVERIFY(!plot.xAxis2->linkSource());
    QVERIFY(!plot.xAxis2->link());
    QCOMPARE(plot.xAxis2->range(), QCPRange(2, 20));
    plot.xAxis2->setRange(3, 4);
    QCOMPARE(plot.xAxis2->range(), QCPRange(3, 4));

    // A rect removed with its axis drops the link the same way.
    QCPAxisRect* second = new QCPAxisRect(&plot);
    plot.plotLayout()->addElement(1, 0, second);
    QCPAxis* bottom = second->axis(QCPAxis::atBottom);
    QVERIFY(bottom);
    bottom->setRange(1, 10);
    plot.xAxis2->linkTo(bottom, twice, half);
    QCOMPARE(plot.xAxis2->range(), QCPRange(2, 20));
    QVERIFY(plot.plotLayout()->remove(second));
    QVERIFY(!plot.xAxis2->linkSource());
    QCOMPARE(plot.xAxis2->range(), QCPRange(2, 20));
    QCOMPARE(gMisuses, 0);
}

void TestLinkedAxes::slotDeletingAnAxisDuringDrag()
{
    {
        QCustomPlot plot;
        plot.resize(400, 300);
        plot.show();
        plot.setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
        plot.xAxis->setRange(1, 10);
        QCPAxis* linked = plot.axisRect()->addAxis(QCPAxis::atTop);
        QVERIFY(linked);
        linked->linkTo(plot.xAxis, twice, half);
        const QPointer<QCPAxis> watcher(linked);
        connect(
            plot.xAxis, &QCPAxis::rangeChanged, &plot,
            [&plot, linked] { plot.axisRect()->removeAxis(linked); }, Qt::SingleShotConnection);
        const QRect rect = plot.axisRect()->rect();
        sendDrag(plot, rect.center(), rect.center() + QPoint(20, 0));
        QVERIFY2(!watcher, "the slot deleted the linked axis");
    }
    {
        QCustomPlot plot;
        plot.resize(400, 300);
        plot.show();
        plot.setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
        QCPAxis* source = plot.axisRect()->addAxis(QCPAxis::atBottom);
        QVERIFY(source);
        source->setRange(1, 10);
        QCPAxis* linked = plot.axisRect()->addAxis(QCPAxis::atTop);
        QVERIFY(linked);
        linked->linkTo(source, twice, half);
        plot.axisRect()->setRangeDragAxes(source, plot.yAxis);
        const QPointer<QCPAxis> sourceWatcher(source);
        const QPointer<QCPAxis> linkedWatcher(linked);
        connect(
            source, &QCPAxis::rangeChanged, &plot,
            [&plot, source] { plot.axisRect()->removeAxis(source); }, Qt::SingleShotConnection);
        const QRect rect = plot.axisRect()->rect();
        sendDrag(plot, rect.center(), rect.center() + QPoint(20, 0));
        QVERIFY2(!sourceWatcher, "the slot deleted the source");
        QVERIFY(linkedWatcher);
        QVERIFY2(!linkedWatcher->linkSource(), "the linked axis became plain");
        QVERIFY(linkedWatcher->range().lower < linkedWatcher->range().upper);
    }
    QCOMPARE(gMisuses, 0);
}

void TestLinkedAxes::aSlotDeletingTheLinkedAxisDuringAWriteBackIsSafe()
{
    QCustomPlot plot;
    plot.setGeometry(0, 0, 400, 300);
    QCPAxis* source = plot.xAxis;
    QCPAxis* linked = plot.axisRect()->addAxis(QCPAxis::atTop);
    linked->linkTo(source);
    const QPointer<QCPAxis> linkedWatcher(linked);
    // A plain API call opens no emission scope: the source's signal runs inside the funnel.
    connect(
        source, &QCPAxis::rangeChanged, &plot,
        [&plot, linked] { plot.axisRect()->removeAxis(linked); }, Qt::SingleShotConnection);
    linked->setRange(5, 6);
    QVERIFY2(!linkedWatcher, "the slot deleted the linked axis");
    QCOMPARE(source->range(), QCPRange(5, 6));
    QCOMPARE(gMisuses, 0);
}

void TestLinkedAxes::reversingALinkedAxisToItsCurrentDirectionIsSilent()
{
    QCustomPlot plot;
    plot.axisRect()->setupFullAxesBox(true);
    gMisuses = 0;
    plot.xAxis2->setRangeReversed(plot.xAxis2->rangeReversed()); // it already has that direction
    QCOMPARE(gMisuses, 0);
    plot.xAxis2->setRangeReversed(!plot.xAxis2->rangeReversed()); // a real change is refused
    QCOMPARE(gMisuses, 1);
}

void TestLinkedAxes::aFullAxesBoxWithoutRangeSyncLeavesPlainAxes()
{
    QCustomPlot plot;
    plot.axisRect()->setupFullAxesBox(true);
    gMisuses = 0;
    plot.axisRect()->setupFullAxesBox(false); // the second call must leave a plain pair again
    QCOMPARE(gMisuses, 0);
    QVERIFY(!plot.xAxis2->linkSource());
    QVERIFY(!plot.yAxis2->linkSource());
    const QCPRange before = plot.xAxis->range();
    plot.xAxis2->setRange(1, 2);
    QCOMPARE(plot.xAxis->range(), before);
}

void TestLinkedAxes::aSlotDeletingTheSourceDuringARefreshIsSafe()
{
    QCustomPlot plot;
    plot.setGeometry(0, 0, 400, 300);
    QCPAxis* source = plot.xAxis;
    QCPAxis* linked = plot.axisRect()->addAxis(QCPAxis::atTop);
    linked->linkTo(source);
    const QPointer<QCPAxis> sourceWatcher(source);
    const QPointer<QCPAxis> linkedWatcher(linked);
    connect(
        linked, &QCPAxis::rangeChanged, &plot,
        [&plot, source] { plot.axisRect()->removeAxis(source); }, Qt::SingleShotConnection);
    source->setRange(2, 3);
    QVERIFY2(!sourceWatcher, "the slot deleted the source");
    QVERIFY(linkedWatcher);
    QVERIFY2(!linkedWatcher->linkSource(), "the linked axis became plain");
    QCOMPARE(linkedWatcher->range(), QCPRange(2, 3));
    QCOMPARE(gMisuses, 0);
}

void TestLinkedAxes::aSpanOnALinkedAxisLandsAtItsCoordToPixel()
{
    if (QGuiApplication::platformName() != QLatin1String("xcb"))
        QSKIP("the span layer needs a platform with an RHI backend");
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.xAxis->setRange(0.1, 1);
    QCPAxis* linked = plot.axisRect()->addAxis(QCPAxis::atBottom);
    linked->setVisible(true);
    linked->linkTo(plot.xAxis, reciprocal, reciprocal);
    auto* span = new QCPItemVSpan(&plot);
    span->lowerEdge->setAxes(linked, plot.yAxis);
    span->upperEdge->setAxes(linked, plot.yAxis);
    span->setPen(Qt::NoPen);
    span->setBorderPen(Qt::NoPen);
    span->setBrush(Qt::red);
    span->setRange(QCPRange(1.9, 2.1));
    plot.show();
    QVERIFY(QTest::qWaitForWindowExposed(&plot));
    plot.replot();
    // The span layer places a band from the rect's own axis ranges; a span on a linked axis is
    // not a linear function of them and must be painted by the painter path.
    QVERIFY2(plot.spanRhiLayerIfExists(), "the span layer was not made");
    QVERIFY2(!plot.spanRhiLayerIfExists()->hasSpans(), "the span went to the GPU layer");
    const QImage image = plot.grab().toImage();
    const QRect rect = plot.axisRect()->rect();
    const double scale = image.width() / double(plot.width());
    QList<int> redColumns;
    for (int x = rect.left() + 1; x < rect.right(); ++x)
    {
        const QColor c = image.pixelColor(x, qRound(rect.center().y() * scale));
        if (c.red() > 150 && c.green() < 100 && c.blue() < 100)
            redColumns.append(x);
    }
    QVERIFY2(!redColumns.isEmpty(), "the span was not drawn");
    const double expected = linked->coordToPixel(2.0) * scale;
    const double middle = (redColumns.first() + redColumns.last()) / 2.0;
    QVERIFY2(std::abs(middle - expected) <= 2.0,
             qPrintable(QStringLiteral("span middle at %1, coordToPixel %2")
                            .arg(middle)
                            .arg(expected)));
}

void TestLinkedAxes::aLinkedAxisGridLineLandsAtItsTick()
{
    if (QGuiApplication::platformName() != QLatin1String("xcb"))
        QSKIP("the grid layer needs a platform with an RHI backend");
    QCustomPlot plot;
    plot.resize(400, 300);
    plot.xAxis->setRange(0.1, 1);
    QCPAxis* linked = plot.axisRect()->addAxis(QCPAxis::atBottom);
    linked->setVisible(true);
    linked->linkTo(plot.xAxis, reciprocal, reciprocal);
    linked->grid()->setVisible(true);
    linked->grid()->setPen(QPen(Qt::blue));
    plot.show();
    QVERIFY(QTest::qWaitForWindowExposed(&plot));
    plot.replot();
    const QImage image = plot.grab().toImage();
    const QRect rect = plot.axisRect()->rect();
    const double scale = image.width() / double(plot.width());
    QSet<int> columns;
    for (int x = rect.left() + 2; x < rect.right() - 1; ++x)
    {
        const QColor c = image.pixelColor(x, qRound(rect.center().y() * scale));
        if (c.blue() > 150 && c.red() < 100 && c.green() < 100)
            columns.insert(x);
    }
    QVERIFY2(!columns.isEmpty(), "the linked axis' grid was not drawn");
    for (int column : columns)
    {
        double nearest = std::numeric_limits<double>::max();
        for (double tick : linked->tickVector())
            nearest = std::min(nearest, std::abs(column - linked->coordToPixel(tick) * scale));
        QVERIFY2(nearest <= 2.0,
                 qPrintable(QStringLiteral("grid line at %1 is not at a tick (%2)")
                                .arg(column)
                                .arg(nearest)));
    }
}

void TestLinkedAxes::aVerticalLinkFollowsALogReversedSource()
{
    QCustomPlot plot;
    plot.setGeometry(0, 0, 400, 300);
    plot.yAxis->setScaleType(QCPAxis::stLogarithmic);
    plot.yAxis->setRange(0.1, 1000);
    plot.yAxis->setRangeReversed(true);
    QCPAxisRect* second = new QCPAxisRect(&plot);
    plot.plotLayout()->addElement(0, 1, second);
    plot.plotLayout()->setColumnStretchFactor(0, 2);
    plot.plotLayout()->setColumnStretchFactor(1, 1);
    QCPAxis* linked = second->addAxis(QCPAxis::atLeft);
    linked->setVisible(true);
    linked->linkTo(plot.yAxis);
    QVERIFY(linked->linkSource() == plot.yAxis);
    plot.replot();

    const auto fraction = [](const QCPAxis* axis, double value)
    {
        const QRect rect = axis->axisRect()->rect();
        return (rect.bottom() - axis->coordToPixel(value)) / double(rect.height());
    };
    for (double value : {0.2, 1.0, 10.0, 500.0})
    {
        QVERIFY2(std::abs(fraction(plot.yAxis, value) - fraction(linked, value)) < 1e-9,
                 "a vertical link must keep the source's fraction along its own height");
    }
}
