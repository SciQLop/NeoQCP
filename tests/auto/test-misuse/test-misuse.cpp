#include "test-misuse.h"
#include "../misuse-recorder.h"

namespace
{
void other(const char*, const char*) {}

int guarded(bool ok)
{
    QCP_CHECK_OR_RETURN(ok, "no", -1);
    return 1;
}
} // namespace

void TestMisuse::init()
{
    test_misuse::calls.clear();
    qcp::setMisuseHandler(test_misuse::record);
}

void TestMisuse::cleanup() { qcp::setMisuseHandler(nullptr); }

void TestMisuse::checkPassesSilently()
{
    QCP_CHECK(true, "x");
    QVERIFY(test_misuse::calls.empty());
}

void TestMisuse::checkReportsWhereAndWhat()
{
    QCP_CHECK(false, "bad axis");
    QCOMPARE(test_misuse::calls.size(), size_t(1));
    QCOMPARE(test_misuse::calls[0].what, std::string("bad axis"));
    QVERIFY(test_misuse::calls[0].where.find("checkReportsWhereAndWhat") != std::string::npos);
}

void TestMisuse::checkOrReturnRefuses()
{
    QCOMPARE(guarded(false), -1);
    QCOMPARE(test_misuse::calls.size(), size_t(1));
    QCOMPARE(guarded(true), 1);
    QCOMPARE(test_misuse::calls.size(), size_t(1));
}

void TestMisuse::setMisuseHandlerReturnsPrevious()
{
    QVERIFY(qcp::setMisuseHandler(other) == test_misuse::record);
}
