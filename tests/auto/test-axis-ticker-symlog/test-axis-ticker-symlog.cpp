#include "test-axis-ticker-symlog.h"
#include "qcustomplot.h"

#include <algorithm>

namespace
{
struct Ticks
{
    QVector<double> major, sub;
};

Ticks generate(const QCPScale& scale, const QCPRange& range)
{
    QCPAxisTickerSymlog ticker(scale);
    Ticks t;
    ticker.generate(range, QLocale::c(), QLatin1Char('g'), 6, t.major, &t.sub, nullptr);
    return t;
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
    QTest::addColumn<QCPRange>("range");
    QTest::addColumn<QVector<double>>("expected");

    QTest::newRow("symlog(2) over three decades")
        << 2.0 << QCPRange(-1000, 1000)
        << QVector<double> { -1000, -100, -10, -1, 0, 1, 10, 100, 1000 };
    QTest::newRow("symlog(2) one decade each side")
        << 2.0 << QCPRange(-50, 50) << QVector<double> { -10, -1, 0, 1, 10 };
    QTest::newRow("symlog(0.015) two decades each side")
        << 0.015 << QCPRange(-1, 1)
        << QVector<double> { -1, -0.1, -0.01, 0, 0.01, 0.1, 1 };
}

void TestAxisTickerSymlog::symlogTicksMatchMatplotlib()
{
    QFETCH(double, threshold);
    QFETCH(QCPRange, range);
    QFETCH(QVector<double>, expected);

    const auto ticks = generate(QCPScale::symlog(threshold), range).major;
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
