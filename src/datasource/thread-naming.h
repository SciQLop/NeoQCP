#pragma once

#ifdef __linux__
#  include <pthread.h>
#endif

namespace qcp
{

// QThreadPool gives no per-thread init hook (unlike BS::thread_pool), so
// pool worker threads name themselves once, the first time they run any
// task -- makes them show up as e.g. "binWorker" instead of generic
// "QThread"/"Thread (pooled)" in /proc, `ps -T`, thread_cpu_top.hot_threads().
inline void nameThisPoolThreadOnce(const char* name) noexcept
{
#ifdef __linux__
    thread_local bool named = false;
    if (!named)
    {
        ::pthread_setname_np(::pthread_self(), name);
        named = true;
    }
#endif
}

} // namespace qcp
