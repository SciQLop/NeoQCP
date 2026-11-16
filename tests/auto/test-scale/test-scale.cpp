#include "test-scale.h"
#include "../../../src/qcp.h"
#include "../misuse-recorder.h"
#include <cmath>

namespace
{
//! The brief quotes matplotlib's values to five decimals; the formulas are exact to within 1e-4.
bool close(double got, double expected)
{
    return std::abs(got - expected) <= 1e-4;
}

//! A round trip through the scale space and back gives the value: no widening for large magnitudes.
bool sameValue(double got, double expected)
{
    return std::abs(got - expected) <= 1e-9 * std::max(1.0, std::abs(expected));
}
} // namespace

void TestScale::initTestCase()
{
    qcp::setMisuseHandler(test_misuse::record);
}

void TestScale::init()
{
    test_misuse::calls.clear();
}

void TestScale::cleanupTestCase()
{
    qcp::setMisuseHandler(nullptr);
}

void TestScale::linearIsIdentity()
{
    QCOMPARE(QCPScale().kind(), QCPScale::Kind::Linear);
    QCOMPARE(QCPScale(), QCPScale::linear());

    const QCPScale scale = QCPScale::linear();
    QVERIFY(scale.isLinear());
    QVERIFY(!scale.isLog());
    QVERIFY(!scale.isSymlog());
    QCOMPARE(scale.toScale(3.5), 3.5);
    QCOMPARE(scale.fromScale(3.5), 3.5);
    QCOMPARE(scale.sanitized(QCPRange(-3, 1)), QCPRange(-3, 1));
}

void TestScale::logIsLogBase()
{
    const QCPScale scale = QCPScale::log();
    QVERIFY(scale.isLog());
    QVERIFY(!scale.isLinear());
    QVERIFY(!scale.isSymlog());
    QCOMPARE(scale.base(), 10.0);
    QVERIFY(close(scale.toScale(1000), 3.0));
    QVERIFY(close(QCPScale::log(2).toScale(8), 3.0));
    QVERIFY(close(scale.toScale(-1000), 3.0)); // the log scale is defined on |v|
    QVERIFY(close(scale.fromScale(3), 1000.0)); // the inverse is the positive value
    QCOMPARE(scale.sanitized(QCPRange(-5, 5)), QCPRange(-5, 5).sanitizedForLogScale());
}

void TestScale::symlogMatchesMatplotlib()
{
    const QCPScale scale = QCPScale::symlog(2);
    QVERIFY(scale.isSymlog());
    QVERIFY(!scale.isLinear());
    QVERIFY(!scale.isLog());
    QCOMPARE(scale.threshold(), 2.0);
    QCOMPARE(scale.base(), 10.0);
    QCOMPARE(scale.linearScale(), 1.0);

    QVERIFY(close(scale.toScale(1), 1.11111));
    QVERIFY(close(scale.toScale(20), 4.22222));
    QVERIFY(close(scale.toScale(-20), -4.22222));
    QVERIFY(close(scale.toScale(100), 5.62020));
    QCOMPARE(scale.toScale(0), 0.0);
    QVERIFY(close(scale.toScale(2), 2.22222));
    // The log side meets the band at the threshold: no jump between the two formulas.
    QVERIFY(close(scale.toScale(std::nextafter(2.0, 3.0)), 2.22222));
    QVERIFY(close(scale.toScale(-20), -scale.toScale(20)));
}

void TestScale::roundTrips()
{
    const QCPScale symlog = QCPScale::symlog(2);
    for (const double value : { -1e6, -20.0, -2.0, -0.5, 0.0, 0.5, 2.0, 20.0, 1e6 })
        QVERIFY(sameValue(symlog.fromScale(symlog.toScale(value)), value));

    const QCPScale log = QCPScale::log();
    for (const double value : { 0.5, 2.0, 20.0, 1000.0, 1e6 })
        QVERIFY(sameValue(log.fromScale(log.toScale(value)), value));
}

void TestScale::badParametersAreRefused()
{
    const auto refused = [](auto make, const QCPScale& expected)
    {
        const size_t before = test_misuse::calls.size();
        const QCPScale scale = make();
        QCOMPARE(test_misuse::calls.size(), before + 1);
        QCOMPARE(scale, expected);
    };

    refused([] { return QCPScale::symlog(0.0); }, QCPScale::symlog());
    refused([] { return QCPScale::symlog(qQNaN()); }, QCPScale::symlog());
    refused([] { return QCPScale::symlog(qInf()); }, QCPScale::symlog());
    refused([] { return QCPScale::symlog(2, 10, 0.0); }, QCPScale::symlog());
    refused([] { return QCPScale::symlog(2, 10, -1.0); }, QCPScale::symlog());
    refused([] { return QCPScale::symlog(2, 1); }, QCPScale::symlog());
    refused([] { return QCPScale::log(1.0); }, QCPScale::log());
    refused([] { return QCPScale::log(qQNaN()); }, QCPScale::log());
}

void TestScale::hashMatchesEquality()
{
    QCOMPARE(qHash(QCPScale::symlog(2)), qHash(QCPScale::symlog(2)));
    QCOMPARE(qHash(QCPScale()), qHash(QCPScale::linear()));
    QVERIFY(qHash(QCPScale::log()) != qHash(QCPScale::symlog()));
    QVERIFY(qHash(QCPScale::linear(), 7) != qHash(QCPScale::linear(), 0));
}
