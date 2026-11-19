#include "test-linked-axes.h"

#include <coords.h>

#include "items/item-span.h"

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

    plot.xAxis->setScale(QCPScale::log());
    plot.xAxis->setRange(1, 100);
    for (const double v : {1.0, 3.3, 10.0})
        QVERIFY2(near(plot.xAxis2->coordToPixel(2 * v), plot.xAxis->coordToPixel(v)),
                 "a link on a log source maps where its source maps");

    plot.xAxis->setScale(QCPScale::linear());
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

    QCPBars* bars = plot.xAxis2->axisRect()->add<QCPBars>(plot.xAxis2, plot.yAxis).get();
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
    // A link through functions keeps its own scale (an identity link shares the source's: see
    // anIdentityLinkSharesTheScale). The functions here are the identity, spelled out.
    QCustomPlot plot;
    plot.xAxis->setRange(-5, 5);
    plot.xAxis2->linkTo(plot.xAxis, [](double v) { return v; }, [](double v) { return v; });
    QCOMPARE(plot.xAxis2->range(), QCPRange(-5, 5));

    plot.xAxis2->setScale(QCPScale::log());
    QCOMPARE(gMisuses, 0);
    QCOMPARE(plot.xAxis->range(), QCPRange(-5, 5));
    // The linked axis sanitizes its own copy: a log range never spans zero.
    QVERIFY2(plot.xAxis2->range().lower * plot.xAxis2->range().upper > 0,
             qPrintable(QStringLiteral("(%1, %2)")
                            .arg(plot.xAxis2->range().lower)
                            .arg(plot.xAxis2->range().upper)));
    QCOMPARE(plot.xAxis2->scale(), QCPScale::log());

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
    auto span = plot.add<QCPItemSpan>(Qt::Vertical);
    span->lowerEdge->setAxes(linked, plot.yAxis);
    span->upperEdge->setAxes(linked, plot.yAxis);
    span->setBorderPen(Qt::NoPen);
    span->setBrush(Qt::red);
    span->setRange(QCPRange(1.9, 2.1));
    plot.show();
    QVERIFY(QTest::qWaitForWindowExposed(&plot));
    plot.replot();
    // A recorded span maps through the axis transform, links included: it lands on the linked
    // axis's keys, which the old span layer could not place.
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
    plot.yAxis->setScale(QCPScale::log());
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

namespace
{
// Two rects stacked, the top one's bottom axis an identity follower of the bottom one's (the
// shared-x target's layout, without the grid's options).
struct Stacked
{
    QCustomPlot plot;
    QCPAxisRect* top = nullptr;
    QCPAxisRect* bottom = nullptr;
    Stacked()
    {
        plot.setAxisRectGrid(2, 1);
        top = plot.axisRect(0, 0).get();
        bottom = plot.axisRect(1, 0).get();
        top->xAxis()->linkTo(bottom->xAxis());
    }
};

std::vector<double> ramp(double from, double to, int n = 9)
{
    std::vector<double> v;
    for (int i = 0; i < n; ++i)
        v.push_back(from + (to - from) * i / (n - 1));
    return v;
}
} // namespace

void TestLinkedAxes::rescaleFitsTheUnion()
{
    // Decision 13: a shared axis fits the union of its panels' data, whichever panel is rescaled.
    Stacked s;
    s.bottom->xAxis()->setRescaleMargin(0); // the union is this test's business, not the margin
    s.top->line(ramp(0, 8), ramp(0, 1));
    s.bottom->line(ramp(2, 10), ramp(0, 1));
    s.plot.rescaleAxes();
    QCOMPARE(s.bottom->xAxis()->range(), QCPRange(0, 10));
    QCOMPARE(s.top->xAxis()->range(), QCPRange(0, 10));

    s.bottom->xAxis()->setRange(3, 4);
    s.top->xAxis()->rescale();
    QCOMPARE(s.bottom->xAxis()->range(), QCPRange(0, 10));
    s.bottom->xAxis()->setRange(3, 4);
    s.bottom->xAxis()->rescale();
    QCOMPARE(s.top->xAxis()->range(), QCPRange(0, 10));
    QCOMPARE(gMisuses, 0);

    // A source on a rect taken out of the layout is not walked: its follower fits the link.
    QCustomPlot plot;
    plot.setAxisRectGrid(2, 1);
    QCPAxisRect* held = plot.axisRect(1, 0).get();
    held->xAxis()->setRescaleMargin(0);
    QVERIFY(plot.plotLayout()->take(held));
    plot.axisRect(0, 0)->xAxis()->linkTo(held->xAxis());
    plot.axisRect(0, 0)->line(ramp(0, 8), ramp(0, 1));
    plot.rescaleAxes();
    QCOMPARE(held->xAxis()->range(), QCPRange(0, 8));
    delete held;
}

void TestLinkedAxes::aUnionUnderTheSharedScale()
{
    // The union is taken in the source's sign domain: a log shared axis ignores data <= 0.
    Stacked s;
    s.bottom->xAxis()->setScale(QCPScale::log());
    s.bottom->xAxis()->setRescaleMargin(0); // the sign domain is this test's business, not the margin
    s.bottom->xAxis()->setRange(1, 10);
    s.top->line(std::vector<double> { -1, 0, 1, 10 }, std::vector<double> { 0, 1, 2, 3 });
    s.bottom->line(std::vector<double> { 100, 1000 }, std::vector<double> { 0, 1 });
    s.plot.rescaleAxes();
    QCOMPARE(s.bottom->xAxis()->range(), QCPRange(1, 1000));

    // A panel with no data and one with constant data still give a valid, finite range.
    Stacked t;
    t.bottom->line(std::vector<double> { 5, 5 }, std::vector<double> { 0, 1 });
    t.plot.rescaleAxes();
    const QCPRange r = t.bottom->xAxis()->range();
    QVERIFY(std::isfinite(r.lower) && std::isfinite(r.upper) && r.lower < 5 && r.upper > 5);
    QCOMPARE(t.top->xAxis()->range(), r);
}

void TestLinkedAxes::aMappedFollowerDoesNotWiden()
{
    // A follower through functions shows other units: its data says nothing about the source's.
    QCustomPlot plot;
    plot.setAxisRectGrid(2, 1);
    QCPAxisRect* top = plot.axisRect(0, 0).get();
    QCPAxisRect* bottom = plot.axisRect(1, 0).get();
    bottom->xAxis()->setRescaleMargin(0); // which data widens the source is this test's business
    top->xAxis()->linkTo(bottom->xAxis(), twice, half);
    top->line(ramp(0, 100), ramp(0, 1));
    bottom->line(ramp(2, 10), ramp(0, 1));
    plot.rescaleAxes();
    QCOMPARE(bottom->xAxis()->range(), QCPRange(2, 10));
    QCOMPARE(top->xAxis()->range(), QCPRange(4, 20));
}

void TestLinkedAxes::anIdentityLinkSharesTheScale()
{
    // P-3: an identity link shows the source's range in the same units, so it shares the scale.
    QCustomPlot plot;
    plot.xAxis->setRange(1, 10);
    plot.xAxis->setScale(QCPScale::log());
    plot.xAxis2->linkTo(plot.xAxis);
    QCOMPARE(plot.xAxis2->scale(), QCPScale::log()); // taken at link time

    QSignalSpy followerScale(plot.xAxis2, &QCPAxis::scaleChanged);
    plot.xAxis->setScale(QCPScale::symlog(2));
    QCOMPARE(plot.xAxis2->scale(), QCPScale::symlog(2)); // a source's change reaches it
    QCOMPARE(followerScale.count(), 1);

    plot.xAxis2->setScale(QCPScale::linear());
    QCOMPARE(plot.xAxis->scale(), QCPScale::linear()); // written to the source
    QCOMPARE(plot.xAxis2->scale(), QCPScale::linear());

    // A custom ticker on the follower survives a link to a source of the same scale.
    QCustomPlot same;
    const auto ticker = QSharedPointer<QCPAxisTickerFixed>::create();
    same.xAxis2->setTicker(ticker);
    same.xAxis2->linkTo(same.xAxis);
    QCOMPARE(same.xAxis2->ticker(), QSharedPointer<QCPAxisTicker>(ticker));
    // A different scale at the link brings its own ticker, as setScale does.
    QCustomPlot other;
    other.xAxis->setScale(QCPScale::log());
    const auto fixed = QSharedPointer<QCPAxisTickerFixed>::create();
    other.xAxis2->setTicker(fixed);
    other.xAxis2->linkTo(other.xAxis);
    QVERIFY(other.xAxis2->ticker() != QSharedPointer<QCPAxisTicker>(fixed));
    QVERIFY(qSharedPointerDynamicCast<QCPAxisTickerLog>(other.xAxis2->ticker()));

    // A link through functions keeps its own scale.
    QCustomPlot mapped;
    mapped.yAxis2->linkTo(mapped.yAxis, twice, half);
    mapped.yAxis->setScale(QCPScale::log());
    QCOMPARE(mapped.yAxis2->scale(), QCPScale::linear());
    QCOMPARE(gMisuses, 0);
}

void TestLinkedAxes::identityLinksFlatten()
{
    // C4: identity links stay a star. Linking to an identity follower links to its source; an axis
    // that drives identity followers hands them to its new source.
    QCustomPlot plot;
    QCPAxisRect* second = new QCPAxisRect(&plot);
    plot.plotLayout()->addElement(1, 0, second);
    QCPAxis* a = plot.xAxis;
    QCPAxis* b = plot.xAxis2;
    QCPAxis* c = second->axis(QCPAxis::atBottom);
    QCPAxis* d = second->axis(QCPAxis::atTop);

    b->linkTo(a);
    c->linkTo(b); // b follows a: c follows a
    QCOMPARE(c->linkSource(), a);
    QCOMPARE(gMisuses, 0);

    QCustomPlot plot2;
    QCPAxisRect* other = new QCPAxisRect(&plot2);
    plot2.plotLayout()->addElement(1, 0, other);
    QCPAxis* root = other->axis(QCPAxis::atBottom);
    plot2.xAxis2->linkTo(plot2.xAxis); // xAxis drives an identity follower
    plot2.xAxis->linkTo(root);         // xAxis follows root: its follower moves to root too
    QCOMPARE(plot2.xAxis->linkSource(), root);
    QCOMPARE(plot2.xAxis2->linkSource(), root);
    root->setRange(3, 4);
    QCOMPARE(plot2.xAxis2->range(), QCPRange(3, 4));
    QCOMPARE(gMisuses, 0);
    plot2.xAxis->unlink(); // the old driver leaving does not take its handed-over follower along
    QCOMPARE(plot2.xAxis2->linkSource(), root);

    // A re-pointed follower takes the new source's scale.
    QCustomPlot plot4;
    QCPAxisRect* fourth = new QCPAxisRect(&plot4);
    plot4.plotLayout()->addElement(1, 0, fourth);
    QCPAxis* logRoot = fourth->axis(QCPAxis::atBottom);
    logRoot->setRange(1, 100);
    logRoot->setScale(QCPScale::log());
    plot4.xAxis2->linkTo(plot4.xAxis);
    plot4.xAxis->linkTo(logRoot);
    QCOMPARE(plot4.xAxis2->scale(), QCPScale::log());

    // Through functions, an axis that drives identity followers may not follow anything.
    QCustomPlot plot5;
    QCPAxisRect* fifth = new QCPAxisRect(&plot5);
    plot5.plotLayout()->addElement(1, 0, fifth);
    plot5.xAxis2->linkTo(plot5.xAxis);
    plot5.xAxis->linkTo(fifth->axis(QCPAxis::atBottom), twice, half);
    QCOMPARE(gMisuses, 1);
    QCOMPARE(plot5.xAxis->linkSource(), nullptr);
    gMisuses = 0;

    // A link through functions does not flatten: chaining to a mapped follower is still refused.
    d->linkTo(a, twice, half);
    QCOMPARE(gMisuses, 0);
    QCustomPlot plot3;
    plot3.xAxis2->linkTo(plot3.xAxis, twice, half);
    QCPAxisRect* third = new QCPAxisRect(&plot3);
    plot3.plotLayout()->addElement(1, 0, third);
    third->axis(QCPAxis::atBottom)->linkTo(plot3.xAxis2);
    QCOMPARE(gMisuses, 1);
    QCOMPARE(third->axis(QCPAxis::atBottom)->linkSource(), nullptr);
}

void TestLinkedAxes::rescaleLeavesTheDefaultMargin()
{
    // matplotlib's axes.margin: a fit leaves 5 % of the data span on each side, so a marker sitting
    // on an extreme value is not cut in half.
    QCustomPlot plot;
    QCOMPARE(plot.xAxis->rescaleMargin(), 0.05);
    plot.axisRect()->line(ramp(0, 10), ramp(0, 1));
    plot.xAxis->rescale();
    QCOMPARE(plot.xAxis->range(), QCPRange(-0.5, 10.5));
    QCOMPARE(gMisuses, 0);
}

void TestLinkedAxes::rescaleMarginOnALogAxisIsInDecades()
{
    // The fraction is of the span in the scale's own space: decades on a log axis.
    QCustomPlot plot;
    plot.xAxis->setScale(QCPScale::log());
    plot.axisRect()->line(std::vector<double> { 1, 10, 100, 1000 }, ramp(0, 1, 4));
    plot.xAxis->rescale();
    QVERIFY(near(plot.xAxis->range().lower, std::pow(10.0, -0.15)));
    QVERIFY(near(plot.xAxis->range().upper, std::pow(10.0, 3.15)));
    QCOMPARE(gMisuses, 0);
}

void TestLinkedAxes::rescaleMarginZeroFitsExactly()
{
    QCustomPlot plot;
    plot.xAxis->setRescaleMargin(0);
    plot.axisRect()->line(ramp(0, 10), ramp(0, 1));
    plot.xAxis->rescale();
    QCOMPARE(plot.xAxis->range(), QCPRange(0, 10));
    QCOMPARE(gMisuses, 0);
}

void TestLinkedAxes::rescaleAxesMarginArgumentOverridesTheAxes()
{
    // One margin for every axis of the call; the axes' own properties are left alone.
    QCustomPlot plot;
    plot.xAxis->setRescaleMargin(0.1);
    plot.yAxis->setRescaleMargin(0.1);
    plot.axisRect()->line(ramp(0, 10), ramp(0, 10));
    plot.rescaleAxes(false, 0.0);
    QCOMPARE(plot.xAxis->range(), QCPRange(0, 10));
    QCOMPARE(plot.yAxis->range(), QCPRange(0, 10));
    QCOMPARE(plot.xAxis->rescaleMargin(), 0.1);
    QCOMPARE(plot.yAxis->rescaleMargin(), 0.1);
    QCOMPARE(gMisuses, 0);
}

void TestLinkedAxes::rescaleOfAnIdentityFollowerUsesTheSourceMargin()
{
    // The margin is read from the axis that receives the range: the link's source.
    Stacked s;
    s.bottom->xAxis()->setRescaleMargin(0.1);
    s.top->xAxis()->setRescaleMargin(0.5);
    s.top->line(ramp(0, 8), ramp(0, 1));
    s.bottom->line(ramp(2, 10), ramp(0, 1));
    s.top->xAxis()->rescale();
    QCOMPARE(s.bottom->xAxis()->range(), QCPRange(-1, 11));
    QCOMPARE(s.top->xAxis()->range(), QCPRange(-1, 11));
    QCOMPARE(gMisuses, 0);
}

void TestLinkedAxes::plottableRescaleOnlyEnlargeDoesNotGrow()
{
    // The margin widens the data range before the union, so a repeated onlyEnlarge call stays put.
    QCustomPlot plot;
    const auto graph = plot.axisRect()->line(ramp(0, 10), ramp(0, 10));
    graph->rescaleAxes(true);
    const QCPRange once = plot.xAxis->range();
    QCOMPARE(once, QCPRange(-0.5, 10.5));
    graph->rescaleAxes(true);
    QCOMPARE(plot.xAxis->range(), once);
    QCOMPARE(gMisuses, 0);
}

void TestLinkedAxes::aNegativeOrNanMarginIsRefused()
{
    QCustomPlot plot;
    QCOMPARE(plot.xAxis->rescaleMargin(), 0.05);
    plot.xAxis->setRescaleMargin(-0.1);
    QCOMPARE(plot.xAxis->rescaleMargin(), 0.05);
    plot.xAxis->setRescaleMargin(std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(plot.xAxis->rescaleMargin(), 0.05);
    plot.xAxis->setRescaleMargin(std::numeric_limits<double>::infinity());
    QCOMPARE(plot.xAxis->rescaleMargin(), 0.05);
    QCOMPARE(gMisuses, 3);
    plot.xAxis->setRescaleMargin(0.2);
    QCOMPARE(plot.xAxis->rescaleMargin(), 0.2);
    QCOMPARE(gMisuses, 3);
}

void TestLinkedAxes::constantDataTakesNoMargin()
{
    // A constant plottable recentres the current range: its fitted span is zero, so the margin
    // widens nothing and the branch is the one it always was.
    QCustomPlot plot;
    plot.xAxis->setRange(0, 10);
    const auto graph
        = plot.axisRect()->line(std::vector<double> { 7, 7 }, std::vector<double> { 0, 1 });
    plot.xAxis->rescale();
    QCOMPARE(plot.xAxis->range(), QCPRange(2, 12));

    plot.xAxis->setRange(0, 10);
    plot.xAxis->setRescaleMargin(0.4);
    graph->rescaleKeyAxis();
    QCOMPARE(plot.xAxis->range(), QCPRange(2, 12));
    QCOMPARE(gMisuses, 0);
}

void TestLinkedAxes::rescaleMarginOnANegativeLogAxis()
{
    // On a negative log range toScale swaps the bounds: the fit has to widen outward on both ends.
    QCustomPlot plot;
    plot.xAxis->setScale(QCPScale::log());
    plot.xAxis->setRange(QCPRange(-1000, -1));
    plot.axisRect()->line(std::vector<double> { -1000, -100, -10, -1 }, ramp(0, 1, 4));
    plot.xAxis->rescale();
    QVERIFY(near(plot.xAxis->range().lower, -std::pow(10.0, 3.15)));
    QVERIFY(near(plot.xAxis->range().upper, -std::pow(10.0, -0.15)));
    QCOMPARE(gMisuses, 0);
}

void TestLinkedAxes::plottableRescaleOnAnIdentityFollowerUsesTheSourceMargin()
{
    // setRange on a follower hands the range to the link's source, so a plottable fit reads the
    // margin that will receive it, as the axis fit does.
    Stacked s;
    s.bottom->xAxis()->setRescaleMargin(0.1);
    s.top->xAxis()->setRescaleMargin(0.5);
    const auto graph = s.top->line(ramp(0, 10), ramp(0, 1));
    graph->rescaleKeyAxis();
    QCOMPARE(s.bottom->xAxis()->range(), QCPRange(-1, 11));
    QCOMPARE(gMisuses, 0);
}
