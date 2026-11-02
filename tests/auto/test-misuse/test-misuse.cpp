#include "test-misuse.h"
#include <string>
#include <vector>

namespace
{
struct Call
{
    std::string where, what;
};
std::vector<Call> gCalls;

void record(const char* where, const char* what) { gCalls.push_back({where, what}); }
void other(const char*, const char*) {}

int guarded(bool ok)
{
    QCP_CHECK_OR_RETURN(ok, "no", -1);
    return 1;
}
} // namespace

void TestMisuse::init()
{
    gCalls.clear();
    qcp::setMisuseHandler(record);
}

void TestMisuse::cleanup() { qcp::setMisuseHandler(nullptr); }

void TestMisuse::checkPassesSilently()
{
    QCP_CHECK(true, "x");
    QVERIFY(gCalls.empty());
}

void TestMisuse::checkReportsWhereAndWhat()
{
    QCP_CHECK(false, "bad axis");
    QCOMPARE(gCalls.size(), size_t(1));
    QCOMPARE(gCalls[0].what, std::string("bad axis"));
    QVERIFY(gCalls[0].where.find("checkReportsWhereAndWhat") != std::string::npos);
}

void TestMisuse::checkOrReturnRefuses()
{
    QCOMPARE(guarded(false), -1);
    QCOMPARE(gCalls.size(), size_t(1));
    QCOMPARE(guarded(true), 1);
    QCOMPARE(gCalls.size(), size_t(1));
}

void TestMisuse::setMisuseHandlerReturnsPrevious()
{
    QVERIFY(qcp::setMisuseHandler(other) == record);
}
