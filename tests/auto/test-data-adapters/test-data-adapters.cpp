#include "test-data-adapters.h"
#include "../../../src/qcp.h"
#include <array>
#include <concepts>
#include <numeric>
#include <ranges>
#include <span>
#include <string>
#include <vector>

namespace
{
//! A user type with a contiguous buffer: the hook hands out a span, qcp::numbers keeps it.
struct Samples
{
    double values[4] { 1, 2, 3, 4 };
};

std::span<const double> qcp_numbers(const Samples& samples)
{
    return std::span<const double>(samples.values);
}

//! The hook of the base class is found for a derived type too (the ellipsis loses to it).
struct DerivedSamples : Samples
{
};

//! A user type with no storage to share: the hook builds the numbers, qcp::numbers owns them.
struct Waveform
{
    double frequency = 2.0;
};

std::vector<double> qcp_numbers(const Waveform& waveform)
{
    return { waveform.frequency, 2 * waveform.frequency };
}

//! A container that opts out: a deleted hook is the best match, so the call does not compile.
struct Grid : std::vector<double>
{
    using std::vector<double>::vector;
};

void qcp_numbers(const Grid&) = delete;

//! A sized range that counts the elements read out of it, to prove there is exactly one pass.
class CountedRange
{
public:
    explicit CountedRange(std::size_t size) : mValues(size)
    {
        std::iota(mValues.begin(), mValues.end(), 0.0);
    }

    struct Iterator
    {
        using iterator_concept = std::forward_iterator_tag;
        using value_type = double;
        using difference_type = std::ptrdiff_t;

        double operator*() const
        {
            ++*reads;
            return *base;
        }

        Iterator& operator++()
        {
            ++base;
            return *this;
        }

        Iterator operator++(int)
        {
            Iterator copy = *this;
            ++base;
            return copy;
        }

        bool operator==(const Iterator&) const = default;

        const double* base = nullptr;
        int* reads = nullptr;
    };

    [[nodiscard]] Iterator begin() const { return Iterator { mValues.data(), &mReads }; }

    [[nodiscard]] Iterator end() const
    {
        return Iterator { mValues.data() + mValues.size(), &mReads };
    }

    [[nodiscard]] std::size_t size() const { return mValues.size(); }

    [[nodiscard]] int reads() const { return mReads; }

private:
    std::vector<double> mValues;
    mutable int mReads = 0;
};

//! The user type of the last test: a plottable that inherits QCPPlottable1D's data constructor.
class HookedGraph : public QCPPlottable1D<QCPGraphData>
{
public:
    using QCPPlottable1D<QCPGraphData>::QCPPlottable1D;

    void draw(QCPDrawContext&) const override { }
};

static_assert(QCPNumberRange<std::vector<double>>);
static_assert(QCPNumberRange<QVector<double>>);
static_assert(QCPNumberRange<std::array<double, 4>>);
static_assert(QCPNumberRange<Samples>);
static_assert(QCPNumberRange<Waveform>);
static_assert(!QCPNumberRange<QStringList>);
static_assert(!QCPNumberRange<std::vector<QString>>);
static_assert(!QCPNumberRange<Grid>);
} // namespace

void TestDataAdapters::aVectorIsReadInPlace()
{
    std::vector<double> values { 1.5, 2.5, 3.5 };
    const auto adapted = qcp::numbers(values);
    static_assert(std::same_as<std::remove_cvref_t<decltype(adapted)>, std::span<const double>>);
    QCOMPARE(adapted.size(), values.size());
    QCOMPARE(adapted.data(), values.data());

    QVector<double> qtValues { 4.0, 5.0 };
    QVERIFY((qcp::toDoubles(qtValues) == std::vector<double> { 4.0, 5.0 }));

    const auto owned = qcp::numbers(std::vector<double> { 6.0 });
    using Owned = std::ranges::owning_view<std::vector<double>>;
    static_assert(std::same_as<std::remove_cvref_t<decltype(owned)>, Owned>);
    QVERIFY((std::ranges::equal(owned, std::vector<double> { 6.0 })));
}

void TestDataAdapters::aHookAdaptsAUserType()
{
    const Samples samples;
    const auto adapted = qcp::numbers(samples);
    static_assert(std::same_as<std::remove_cvref_t<decltype(adapted)>, std::span<const double>>);
    QCOMPARE(adapted.data(), samples.values);
    QVERIFY((qcp::toDoubles(samples) == std::vector<double> { 1.0, 2.0, 3.0, 4.0 }));

    const auto waveform = qcp::numbers(Waveform { });
    QVERIFY((std::ranges::equal(waveform, std::vector<double> { 2.0, 4.0 })));
}

void TestDataAdapters::aDeletedHookRefusesARange()
{
    static_assert(!std::invocable<decltype(qcp::numbers), const Grid&>);
    static_assert(!QCPNumberRange<Grid>);
    QVERIFY(QCPNumberRange<std::vector<double>>);
}

void TestDataAdapters::aHookOnABaseIsFound()
{
    const DerivedSamples derived;
    QVERIFY((qcp::toDoubles(derived) == std::vector<double> { 1.0, 2.0, 3.0, 4.0 }));
}

void TestDataAdapters::aHookReturnsByValue()
{
    const auto adapted = qcp::numbers(Waveform { });
    using Owned = std::ranges::owning_view<std::vector<double>>;
    static_assert(std::same_as<std::remove_cvref_t<decltype(adapted)>, Owned>);
    QVERIFY((std::ranges::equal(adapted, std::vector<double> { 2.0, 4.0 })));
}

void TestDataAdapters::aNonCommonRangeIsRead()
{
    const auto range = std::views::iota(0, 5) | std::views::take_while([](int i) { return i < 3; });
    static_assert(!std::ranges::common_range<decltype(range)>);
    QVERIFY((qcp::toDoubles(range) == std::vector<double> { 0.0, 1.0, 2.0 }));
}

void TestDataAdapters::copiesOnce()
{
    const CountedRange range(1000);
    const auto values = qcp::toDoubles(range);
    QCOMPARE(values.size(), std::size_t(1000));
    QCOMPARE(values.capacity(), values.size());
    QCOMPARE(range.reads(), 1000);
    QCOMPARE(values.front(), 0.0);
    QCOMPARE(values.back(), 999.0);
}

void TestDataAdapters::aHookedTypeReachesEveryPlotType()
{
    const Samples keys;
    const Samples values;

    const QCPGraph graph(keys, values);
    QCOMPARE(graph.dataCount(), 4);
    QCOMPARE(graph.dataMainValue(0), 1.0);

    const QCPCurve curve(keys, values);
    QCOMPARE(curve.dataCount(), 4);

    const QCPBars bars(keys, values);
    QCOMPARE(bars.dataCount(), 4);

    const QCPErrorBars errors(Samples { });
    QCOMPARE(errors.dataCount(), 4);

    const QCPFinancial financial(keys, values, values, values, values);
    QCOMPARE(financial.dataCount(), 4);

    const QCPStatisticalBox box(keys, values, values, values, values, values);
    QCOMPARE(box.dataCount(), 4);

    const QCPPolarGraph polar(keys, values);
    QCOMPARE(polar.dataCount(), 4);

    const QCPWaterfallGraph waterfall(keys, values);
    QCOMPARE(waterfall.dataCount(), 4);

    const HookedGraph hooked(keys, values);
    QCOMPARE(hooked.dataCount(), 4);
}
