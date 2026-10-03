#include "test-axis-ticker-log.h"
#include "qcustomplot.h"

namespace
{
struct Ticks
{
    QVector<double> major, sub;
};

Ticks generate(const QCPRange& range)
{
    QCPAxisTickerLog ticker;
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
}

// Ranges shorter than about two decades get ticks at "round" mantissas, the way
// matplotlib and d3 do, instead of evenly spaced values that bunch up on a log axis.
void TestAxisTickerLog::majorTicks_data()
{
    QTest::addColumn<QCPRange>("range");
    QTest::addColumn<QVector<double>>("expected");
    QTest::newRow("one decade: 1, 2, 5") << QCPRange(1.5e6, 1.6e7) << QVector<double> { 2e6, 5e6, 1e7 };
    QTest::newRow("0.7 decade: 1, 2, 3, 5, 7")
        << QCPRange(3e6, 1.6e7) << QVector<double> { 3e6, 5e6, 7e6, 1e7 };
    QTest::newRow("0.4 decade: every digit")
        << QCPRange(6e6, 1.6e7) << QVector<double> { 6e6, 7e6, 8e6, 9e6, 1e7 };
    QTest::newRow("1.5 decades: 1, 2, 5")
        << QCPRange(1.5e6, 5e7) << QVector<double> { 2e6, 5e6, 1e7, 2e7, 5e7 };
    QTest::newRow("small values") << QCPRange(1.5e-3, 1.6e-2) << QVector<double> { 2e-3, 5e-3, 1e-2 };
    QTest::newRow("negative range mirrors")
        << QCPRange(-1.6e7, -1.5e6) << QVector<double> { -1e7, -5e6, -2e6 };
    QTest::newRow("wide range keeps decades")
        << QCPRange(1e3, 1e9) << QVector<double> { 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9 };
}

void TestAxisTickerLog::majorTicks()
{
    QFETCH(QCPRange, range);
    QFETCH(QVector<double>, expected);
    const auto ticks = generate(range).major;
    QVERIFY2(sameTicks(ticks, expected), qPrintable(QString("got %1").arg(
        [&] { QStringList s; for (double t : ticks) s << QString::number(t, 'g', 4); return s.join(", "); }())));
}

void TestAxisTickerLog::shortRangeSubTicksFillTheMissingDigits()
{
    const auto ticks = generate(QCPRange(1.5e6, 1.6e7));
    QVERIFY(sameTicks(ticks.sub, { 3e6, 4e6, 6e6, 7e6, 8e6, 9e6 }));
}

// Too short for even one digit step: evenly spaced ticks are the right answer there.
void TestAxisTickerLog::tinyRangeFallsBackToEvenSteps()
{
    const auto ticks = generate(QCPRange(1.0e7, 1.3e7)).major;
    QVERIFY(ticks.size() >= 3);
    const double step = ticks[1] - ticks[0];
    for (int i = 2; i < ticks.size(); ++i)
        QVERIFY(qFuzzyCompare(ticks[i] - ticks[i - 1], step));
}
