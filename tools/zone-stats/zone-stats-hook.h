/*------------------------------------------------------------------------------
-- This file is a part of the NeoQCP project.
-- Copyright (C) 2025, Plasma Physics Laboratory - CNRS
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; either version 2 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307 USA
-------------------------------------------------------------------------------*/
#pragma once

// Headless zone statistics: the 'zone_stats' build option points NEOQCP_PROFILING_HOOK at this
// header, so the PROFILE_* macros of src/Profiling.hpp time their scopes and a table is written to
// the file named by $NEOQCP_ZONE_STATS when the process exits. No GUI, no Tracy.
//
// Each call site keeps one Site (its counters live for the whole process); the same zone name in
// several translation units is summed into one row. "share" is this zone's total time over the sum
// of the outermost zones' totals, so a row nested in another one can be read as its contribution.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <vector>

namespace neoqcp::zonestats
{
using Clock = std::chrono::steady_clock;

struct Site;

inline std::mutex &registryMutex();
inline Site *&firstSite();
inline std::atomic<std::uint64_t> &frames();
inline void ensureDumpAtExit();

struct Site
{
  explicit Site(const char *zoneName) : name(zoneName)
  {
    const std::lock_guard<std::mutex> lock(registryMutex());
    next = firstSite();
    firstSite() = this;
    ensureDumpAtExit();
  }

  const char *name;
  std::atomic<std::uint64_t> nanoseconds { 0 };
  std::atomic<std::uint64_t> outerNanoseconds { 0 };
  std::atomic<std::uint64_t> calls { 0 };
  Site *next { nullptr };
};

inline std::mutex &registryMutex()
{
  static std::mutex mutex;
  return mutex;
}

inline Site *&firstSite()
{
  static Site *first = nullptr;
  return first;
}

inline std::atomic<std::uint64_t> &frames()
{
  static std::atomic<std::uint64_t> count { 0 };
  return count;
}

// How deep this thread is in zones: only the outermost one counts towards the share denominator.
inline int &nesting()
{
  static thread_local int level = 0;
  return level;
}

class Scope
{
public:
  explicit Scope(Site &site) : mSite(site), mStart(Clock::now()) { ++nesting(); }

  ~Scope()
  {
    const auto elapsed = std::uint64_t(
        std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - mStart).count());
    mSite.nanoseconds.fetch_add(elapsed, std::memory_order_relaxed);
    mSite.calls.fetch_add(1, std::memory_order_relaxed);
    if (--nesting() == 0)
      mSite.outerNanoseconds.fetch_add(elapsed, std::memory_order_relaxed);
  }

  Scope(const Scope &) = delete;
  Scope &operator=(const Scope &) = delete;

private:
  Site &mSite;
  Clock::time_point mStart;
};

// One Site per call site (Tag), constructed on the site's first execution.
template <int Tag>
inline Site &siteFor(const char *name)
{
  static Site site { name };
  return site;
}

inline void countFrame()
{
  frames().fetch_add(1, std::memory_order_relaxed);
  ensureDumpAtExit();
}

inline void dump()
{
  const char *path = std::getenv("NEOQCP_ZONE_STATS");
  if (path == nullptr || *path == '\0')
    return;
  std::vector<const Site *> sites;
  for (const Site *site = firstSite(); site != nullptr; site = site->next)
    sites.push_back(site);
  // The same name can come from several translation units: sort by name, then sum equal names.
  std::sort(sites.begin(), sites.end(),
            [](const Site *a, const Site *b) { return std::strcmp(a->name, b->name) < 0; });
  struct Row
  {
    const char *name;
    std::uint64_t calls;
    std::uint64_t nanoseconds;
    std::uint64_t outerNanoseconds;
  };
  std::vector<Row> rows;
  for (const Site *site : sites)
  {
    const Row row { site->name, site->calls.load(std::memory_order_relaxed),
                    site->nanoseconds.load(std::memory_order_relaxed),
                    site->outerNanoseconds.load(std::memory_order_relaxed) };
    if (!rows.empty() && std::strcmp(rows.back().name, row.name) == 0)
    {
      rows.back().calls += row.calls;
      rows.back().nanoseconds += row.nanoseconds;
      rows.back().outerNanoseconds += row.outerNanoseconds;
    }
    else
      rows.push_back(row);
  }
  std::uint64_t outerTotal = 0;
  for (const Row &row : rows)
    outerTotal += row.outerNanoseconds;
  std::stable_sort(rows.begin(), rows.end(),
                   [](const Row &a, const Row &b) { return a.nanoseconds > b.nanoseconds; });

  std::FILE *file = std::fopen(path, "w");
  if (file == nullptr)
    return;
  std::fprintf(file, "zone\tcalls\ttotal_ms\tmean_us\tshare\n");
  for (const Row &row : rows)
  {
    const double milliseconds = double(row.nanoseconds) * 1e-6;
    const double meanMicroseconds = row.calls ? double(row.nanoseconds) * 1e-3 / double(row.calls) : 0.0;
    const double share = outerTotal ? 100.0 * double(row.nanoseconds) / double(outerTotal) : 0.0;
    std::fprintf(file, "%s\t%llu\t%.3f\t%.3f\t%.1f%%\n", row.name,
                 static_cast<unsigned long long>(row.calls), milliseconds, meanMicroseconds, share);
  }
  std::fprintf(file, "frames\t%llu\n", static_cast<unsigned long long>(frames().load()));
  std::fclose(file);
}

inline void ensureDumpAtExit()
{
  static const bool registered = (std::atexit(&dump), true);
  (void)registered;
}
} // namespace neoqcp::zonestats

#define NEOQCP_ZONE_STATS_CAT_(a, b) a##b
#define NEOQCP_ZONE_STATS_CAT(a, b) NEOQCP_ZONE_STATS_CAT_(a, b)
#ifdef __COUNTER__
#  define NEOQCP_ZONE_STATS_TAG __COUNTER__
#else
#  define NEOQCP_ZONE_STATS_TAG __LINE__
#endif

// A declaration, so a zone can be the only statement of an unbraced if, a case label, or a loop.
#define PROFILE_HERE_N(name)                                                                      \
  const ::neoqcp::zonestats::Scope NEOQCP_ZONE_STATS_CAT(neoqcp_zone_stats_, __LINE__)            \
  {                                                                                               \
    ::neoqcp::zonestats::siteFor<NEOQCP_ZONE_STATS_TAG>(name)                                     \
  }
#define PROFILE_HERE PROFILE_HERE_N(__func__)
#define PROFILE_HERE_NC(name, color) PROFILE_HERE_N(name)
#define PROFILE_FRAME_MARK ::neoqcp::zonestats::countFrame()
#define PROFILE_PASS_VALUE(value) (void)0
#define PROFILE_PASS_VALUE_N(name, value) (void)0
#define PROFILE_PASS_TXT(txt, size) (void)0
