#include "misuse.h"
#include <QDebug>
#include <atomic>

namespace qcp
{
namespace
{
void defaultHandler(const char* where, const char* what)
{
#ifdef QT_NO_DEBUG
    qWarning("NeoQCP misuse in %s: %s", where, what);
#else
    qFatal("%s: %s", where, what);
#endif
}

std::atomic<MisuseHandler>& handler()
{
    static std::atomic<MisuseHandler> h{defaultHandler};
    return h;
}
} // namespace

MisuseHandler setMisuseHandler(MisuseHandler newHandler)
{
    return handler().exchange(newHandler ? newHandler : defaultHandler);
}

void reportMisuse(const char* where, const char* what)
{
    handler().load()(where, what);
}
} // namespace qcp
