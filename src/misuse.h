#ifndef QCP_MISUSE_H
#define QCP_MISUSE_H

#include "global.h"
#include <QtGlobal>

namespace qcp
{
using MisuseHandler = void (*)(const char* where, const char* what);

// Returns the previous handler. nullptr restores the default (fatal in debug, warning in release).
QCP_LIB_DECL MisuseHandler setMisuseHandler(MisuseHandler handler);
QCP_LIB_DECL void reportMisuse(const char* where, const char* what);
} // namespace qcp

#define QCP_CHECK(cond, what)                                                                      \
    do                                                                                             \
    {                                                                                              \
        if (!(cond))                                                                               \
            qcp::reportMisuse(Q_FUNC_INFO, what);                                                  \
    } while (false)

// The release-build refusal: reports, then returns the trailing arguments (nothing for void).
#define QCP_CHECK_OR_RETURN(cond, what, ...)                                                       \
    do                                                                                             \
    {                                                                                              \
        if (!(cond))                                                                               \
        {                                                                                          \
            qcp::reportMisuse(Q_FUNC_INFO, what);                                                  \
            return __VA_ARGS__;                                                                    \
        }                                                                                          \
    } while (false)

#endif // QCP_MISUSE_H
