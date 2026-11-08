#include "test-linked-axes.h"

#include <coords.h>

#include <cmath>

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
    QCOMPARE(gMisuses, 1);
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
    extra->linkTo(plot.xAxis); // refused: that axis already has a link of its own
    QCOMPARE(gMisuses, 6);
    QVERIFY(!extra->linkSource());

    plot.xAxis->linkTo(extra); // refused: that axis is a source
    QCOMPARE(gMisuses, 7);
    QVERIFY(!plot.xAxis->linkSource());

    extra->linkTo(plot.xAxis2); // refused: that axis is linked itself
    QCOMPARE(gMisuses, 8);
    QVERIFY(!extra->linkSource());
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
