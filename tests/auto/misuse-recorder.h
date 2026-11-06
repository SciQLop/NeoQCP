#ifndef NEOQCP_TEST_MISUSE_RECORDER_H
#define NEOQCP_TEST_MISUSE_RECORDER_H

#include "misuse.h"
#include <string>
#include <vector>

// The recording misuse handler, shared by the test classes that expect a refusal: install it with
// qcp::setMisuseHandler(test_misuse::record) and read test_misuse::calls afterwards.
namespace test_misuse
{
struct Call
{
    std::string where, what;
};

inline std::vector<Call> calls;

inline void record(const char* where, const char* what)
{
    calls.push_back({ where, what });
}
} // namespace test_misuse

#endif // NEOQCP_TEST_MISUSE_RECORDER_H
