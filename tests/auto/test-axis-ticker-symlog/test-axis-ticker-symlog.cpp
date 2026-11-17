#include "test-axis-ticker-symlog.h"
#include "qcustomplot.h"

#include <algorithm>
#include <cmath>

namespace
{
struct Ticks
{
    QVector<double> major, sub;
};

Ticks generate(QCPAxisTicker& ticker, const QCPRange& range)
{
    Ticks t;
    ticker.generate(range, QLocale::c(), QLatin1Char('g'), 6, t.major, &t.sub, nullptr);
    return t;
}

Ticks generate(const QCPScale& scale, const QCPRange& range)
{
    QCPAxisTickerSymlog ticker(scale);
    return generate(ticker, range);
}

bool contains(const QVector<double>& ticks, double value)
{
    return std::any_of(ticks.cbegin(), ticks.cend(),
                       [value](double tick) { return qFuzzyCompare(tick, value); });
}

int totalTicks(const Ticks& ticks)
{
    return ticks.major.size() + ticks.sub.size();
}

bool sameTicks(const QVector<double>& got, const QVector<double>& expected)
{
    if (got.size() != expected.size())
        return false;
    for (int i = 0; i < got.size(); ++i)
        if (!qFuzzyCompare(got[i], expected[i]))
            return false;
    return true;
}

QString printed(const QVector<double>& ticks)
{
    QStringList parts;
    for (double t : ticks)
        parts << QString::number(t, 'g', 9);
    return parts.join(QStringLiteral(", "));
}

QVector<double> bounded(const QVector<double>& ticks, double bound, bool inside)
{
    QVector<double> kept;
    for (double t : ticks)
        if ((qAbs(t) <= bound) == inside)
            kept.append(t);
    return kept;
}
} // namespace

// matplotlib 3.11.2, SymmetricalLogLocator(linthresh, base=10).tick_values(lower, upper), with its
// default numticks of 15: decades from the one holding the threshold outward, plus zero when the
// range straddles it. The library thins the decades by its own tick count (the log ticker's step
// formula in the task brief), which leaves the stride 1 of matplotlib at the default count of 5.
void TestAxisTickerSymlog::symlogTicksMatchMatplotlib_data()
{
    QTest::addColumn<double>("threshold");
    QTest::addColumn<double>("base");
    QTest::addColumn<QCPRange>("range");
    QTest::addColumn<QVector<double>>("expected");

    QTest::newRow("symlog(2) over three decades")
        << 2.0 << 10.0 << QCPRange(-1000, 1000)
        << QVector<double> { -1000, -100, -10, -1, 0, 1, 10, 100, 1000 };
    QTest::newRow("symlog(2) one decade each side")
        << 2.0 << 10.0 << QCPRange(-50, 50) << QVector<double> { -10, -1, 0, 1, 10 };
    QTest::newRow("symlog(0.015) two decades each side")
        << 0.015 << 10.0 << QCPRange(-1, 1)
        << QVector<double> { -1, -0.1, -0.01, 0, 0.01, 0.1, 1 };
    QTest::newRow("symlog(2) entirely negative")
        << 2.0 << 10.0 << QCPRange(-1000, -10) << QVector<double> { -1000, -100, -10 };
    QTest::newRow("symlog(2) crossing only the negative side of the band")
        << 2.0 << 10.0 << QCPRange(-50, 1) << QVector<double> { -10, -1, 0 };
    QTest::newRow("symlog(2) base 3")
        << 2.0 << 3.0 << QCPRange(3, 30) << QVector<double> { 3, 9, 27 };
}

void TestAxisTickerSymlog::symlogTicksMatchMatplotlib()
{
    QFETCH(double, threshold);
    QFETCH(double, base);
    QFETCH(QCPRange, range);
    QFETCH(QVector<double>, expected);

    const auto ticks = generate(QCPScale::symlog(threshold, base), range).major;
    QVERIFY2(sameTicks(ticks, expected),
             qPrintable(QStringLiteral("got %1").arg(printed(ticks))));
}

// Inside the band, a symlog axis is linear, so its sub grid is the one a linear axis draws over the
// threshold's decade. Both ends of the band are marked: they are the kink of the mapping.
void TestAxisTickerSymlog::subTicksFillTheBand()
{
    const auto ticks = generate(QCPScale::symlog(2), QCPRange(-50, 50));
    const auto inside = bounded(ticks.sub, 2, true);
    const QVector<double> expected { -2,   -1.8, -1.6, -1.4, -1.2, -0.8, -0.6, -0.4, -0.2,
                                     0.2, 0.4,  0.6,  0.8,  1.2,  1.4,  1.6,  1.8,  2 };
    QVERIFY2(sameTicks(inside, expected),
             qPrintable(QStringLiteral("got %1").arg(printed(inside))));
}

// Outside the band the mapping is logarithmic, so the sub grid is the log axis's: 2..9 x base^k
// from the decade that holds the threshold outward.
void TestAxisTickerSymlog::subTicksOutsideTheBandKeepTheMantissas()
{
    const auto ticks = generate(QCPScale::symlog(2), QCPRange(-50, 50));
    const auto outside = bounded(ticks.sub, 2, false);
    const QVector<double> expected { -50, -40, -30, -20, -9, -8, -7, -6, -5, -4, -3,
                                     3,   4,   5,   6,   7,  8,  9,  20, 30, 40, 50 };
    QVERIFY2(sameTicks(outside, expected),
             qPrintable(QStringLiteral("got %1").arg(printed(outside))));
}

// matplotlib: a range that stays inside the band takes its own ends and zero.
void TestAxisTickerSymlog::aRangeInsideTheBandTakesItsEnds()
{
    const auto ticks = generate(QCPScale::symlog(2), QCPRange(-1.5, 1.5));
    const QVector<double> expected { -1.5, 0, 1.5 };
    QVERIFY2(sameTicks(ticks.major, expected),
             qPrintable(QStringLiteral("got %1").arg(printed(ticks.major))));
}

// matplotlib would show no tick here: the only decade below the range is 1. A symlog axis that
// gives no tick at all is useless, so the ticker falls back to evenly spaced ticks.
void TestAxisTickerSymlog::noDecadeInRangeFallsBackToEvenSteps()
{
    const auto ticks = generate(QCPScale::symlog(2), QCPRange(3, 4)).major;
    QVERIFY(ticks.size() >= 3);
    const double step = ticks[1] - ticks[0];
    for (int i = 2; i < ticks.size(); ++i)
        QVERIFY(qFuzzyCompare(ticks[i] - ticks[i - 1], step));
}

// A base that is not ten still has a decade, and the mantissas between its decades are the
// integers below the base: 2 x 3^k here. Inside the band it is still linear.
void TestAxisTickerSymlog::aBaseOtherThanTenKeepsItsMantissas()
{
    const auto ticks = generate(QCPScale::symlog(2, 3), QCPRange(3, 30));
    const QVector<double> expected { 6, 18 };
    QVERIFY2(sameTicks(ticks.sub, expected),
             qPrintable(QStringLiteral("got %1").arg(printed(ticks.sub))));
}

// A wide range keeps one decade out of several. The decades skipped by the thinning still get a
// sub tick, so a decade is never invisible, and the grid stays the mantissas of the kept decades.
void TestAxisTickerSymlog::thinnedDecadesBecomeSubTicks()
{
    const auto ticks = generate(QCPScale::symlog(2), QCPRange(3, 1e12));
    const QVector<double> expected { 100, 1e4, 1e6, 1e8, 1e10, 1e12 };
    QVERIFY2(sameTicks(ticks.major, expected),
             qPrintable(QStringLiteral("got %1").arg(printed(ticks.major))));
    QVERIFY2(!contains(ticks.major, 10) && !contains(ticks.major, 1000),
             qPrintable(QStringLiteral("majors %1").arg(printed(ticks.major))));
    QVERIFY2(contains(ticks.sub, 10) && contains(ticks.sub, 1000),
             qPrintable(QStringLiteral("subs %1").arg(printed(ticks.sub))));
    // A decade the thinning skipped is a sub tick of its own, and the decades it does show keep
    // their mantissas.
    QVERIFY2(contains(ticks.sub, 1e5),
             qPrintable(QStringLiteral("subs %1").arg(printed(ticks.sub))));
    QVERIFY2(contains(ticks.sub, 2e2) && contains(ticks.sub, 9e4),
             qPrintable(QStringLiteral("subs %1").arg(printed(ticks.sub))));
}

// The even ticks a range without a decade falls back to keep the linear axis's sub ticks.
void TestAxisTickerSymlog::subTicksInTheFallback()
{
    const auto ticks = generate(QCPScale::symlog(2), QCPRange(3, 4));
    QVERIFY(!ticks.sub.isEmpty());
    for (double tick : ticks.sub)
        QVERIFY2(tick >= 3 && tick <= 4, qPrintable(QString::number(tick, 'g', 9)));
    QVERIFY2(!contains(ticks.major, ticks.sub.front()),
             qPrintable(QStringLiteral("a sub tick is also a major: %1").arg(printed(ticks.major))));
}

// A legal scale may be extreme: a base of 1e6, of 2^40, or barely above 1 turns every decade
// count into millions, and a threshold far below one makes the band's sub grid huge. None of that
// may be materialised: the work per replot is bounded by the ticks that can be shown.
void TestAxisTickerSymlog::extremeScalesStayBounded_data()
{
    QTest::addColumn<QString>("kind");
    QTest::addColumn<double>("threshold");
    QTest::addColumn<double>("base");
    QTest::addColumn<QCPRange>("range");

    QTest::newRow("symlog base 1e6")
        << QStringLiteral("symlog") << 2.0 << 1e6 << QCPRange(-1e250, 1e250);
    QTest::newRow("symlog base 2^40")
        << QStringLiteral("symlog") << 2.0 << std::pow(2.0, 40) << QCPRange(-1e250, 1e250);
    QTest::newRow("symlog base 1.0001")
        << QStringLiteral("symlog") << 2.0 << 1.0001 << QCPRange(-1e250, 1e250);
    QTest::newRow("log base 1e6")
        << QStringLiteral("log") << 0.0 << 1e6 << QCPRange(1e-250, 1e250);
    QTest::newRow("log base 1.0001")
        << QStringLiteral("log") << 0.0 << 1.0001 << QCPRange(1e-250, 1e250);
}

void TestAxisTickerSymlog::extremeScalesStayBounded()
{
    QFETCH(QString, kind);
    QFETCH(double, threshold);
    QFETCH(double, base);
    QFETCH(QCPRange, range);

    const bool symlog = kind == QLatin1String("symlog");
    const QCPScale scale = symlog ? QCPScale::symlog(threshold, base) : QCPScale::log(base);

    QElapsedTimer timer;
    timer.start();
    Ticks ticks;
    if (symlog)
    {
        QCPAxisTickerSymlog ticker(scale);
        ticks = generate(ticker, range);
    }
    else
    {
        QCPAxisTickerLog ticker;
        ticker.setLogBase(scale.base());
        ticks = generate(ticker, range);
    }
    QVERIFY2(timer.elapsed() < 2000, qPrintable(QString::number(timer.elapsed())));

    QVERIFY2(totalTicks(ticks) <= 400,
             qPrintable(QStringLiteral("%1 ticks: %2 / %3")
                            .arg(totalTicks(ticks))
                            .arg(printed(ticks.major), printed(ticks.sub))));
    for (double tick : ticks.major)
        QVERIFY2(std::isfinite(tick) && (symlog || tick > 0),
                 qPrintable(QString::number(tick, 'g', 9)));
    for (double tick : ticks.sub)
        QVERIFY2(std::isfinite(tick), qPrintable(QString::number(tick, 'g', 9)));
}
