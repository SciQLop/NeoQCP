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

//! A hook that returns a view only a non-const object can iterate: the adaptor must not make the
//! copy it iterates const.
struct Filtered
{
    std::vector<double> values { 1, 2, 3, 4 };
};

auto qcp_numbers(const Filtered& filtered)
{
    return filtered.values | std::views::filter([](double value) { return value > 2.0; });
}

//! A hook that returns a const container by value: the result is decayed, then owned and read.
struct Frozen
{
};

const std::vector<double> qcp_numbers(const Frozen&)
{
    return { 7.0, 8.0 };
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

//! A columns hook that hands out a const reference to storage it owns: the adaptor reads it where
//! it is, so the concept and the call agree on the const container.
struct SharedColumns
{
    std::vector<std::vector<double>> columns { { 1.0, 2.0, 3.0 }, { 4.0, 5.0, 6.0 } };
};

const std::vector<std::vector<double>>& qcp_columns(const SharedColumns& shared)
{
    return shared.columns;
}

//! Columns only a non-const object iterates, reached through a hook that hands out a const
//! reference: the adaptor would read the const container, so the concept must refuse it instead of
//! letting the call fail inside its body.
struct ConstColumns
{
    std::vector<std::vector<double>> values { { 1.0, 2.0 }, { 3.0, 4.0 } };

    std::vector<std::vector<double>>::iterator begin() { return values.begin(); }
    std::vector<std::vector<double>>::iterator end() { return values.end(); }
};

struct WrappedColumns
{
    ConstColumns columns;
};

const ConstColumns& qcp_columns(const WrappedColumns& wrapped)
{
    return wrapped.columns;
}

//! A column handed over as a proxy that materializes on demand.
struct ColumnProxy
{
    const double* values = nullptr;

    operator std::vector<double>() const { return { values, values + 2 }; }
};

//! A columns range that names a column as its value type but hands the reading loop a proxy: the
//! concept must judge the element reference, which the reading call refuses, and say so itself.
class ProxiedColumns
{
public:
    struct Iterator
    {
        using iterator_concept = std::forward_iterator_tag;
        using value_type = std::vector<double>;
        using difference_type = std::ptrdiff_t;

        ColumnProxy operator*() const { return ColumnProxy { rows->data() }; }
        Iterator& operator++()
        {
            ++rows;
            return *this;
        }
        Iterator operator++(int)
        {
            Iterator copy = *this;
            ++rows;
            return copy;
        }
        bool operator==(const Iterator&) const = default;

        const std::array<double, 2>* rows = nullptr;
    };

    [[nodiscard]] Iterator begin() const { return Iterator { mRows.data() }; }
    [[nodiscard]] Iterator end() const { return Iterator { mRows.data() + mRows.size() }; }

    std::array<std::array<double, 2>, 2> mRows { { { 1.0, 2.0 }, { 3.0, 4.0 } } };
};

//! A range a hook hands out by value that cannot be moved: the adaptor owns what it reads, so the
//! call refuses it instead of failing where the view over it is built.
struct ImmovableColumns : std::vector<std::vector<double>>
{
    ImmovableColumns() = default;
    ImmovableColumns(const ImmovableColumns&) = delete;
    ImmovableColumns(ImmovableColumns&&) = delete;
    ImmovableColumns& operator=(const ImmovableColumns&) = delete;
    ImmovableColumns& operator=(ImmovableColumns&&) = delete;
};

struct ImmovableNumbers : std::vector<double>
{
    ImmovableNumbers() = default;
    ImmovableNumbers(const ImmovableNumbers&) = delete;
    ImmovableNumbers(ImmovableNumbers&&) = delete;
    ImmovableNumbers& operator=(const ImmovableNumbers&) = delete;
    ImmovableNumbers& operator=(ImmovableNumbers&&) = delete;
};

struct BuriedData
{
};

ImmovableNumbers qcp_numbers(const BuriedData&) { return {}; }
ImmovableColumns qcp_columns(const BuriedData&) { return {}; }

//! A hook handing out its storage as a const rvalue: the adaptor owns what it gets, so it copies it.
struct ExpiringData
{
    std::vector<double> numbers { 5.0, 6.0 };
    std::vector<std::vector<double>> columns { { 1.0 }, { 2.0 } };
};

const std::vector<double>&& qcp_numbers(const ExpiringData& d) { return std::move(d.numbers); }
const std::vector<std::vector<double>>&& qcp_columns(const ExpiringData& d)
{
    return std::move(d.columns);
}

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
static_assert(QCPNumberRange<Filtered>);
static_assert(QCPNumberRange<Frozen>);
static_assert(!QCPNumberRange<QStringList>);
static_assert(!QCPNumberRange<std::vector<QString>>);
static_assert(!QCPNumberRange<Grid>);

//! A 2D user type: row-major storage, one strided view per column, built on the spot.
struct Matrix
{
    int rows = 0;
    int cols = 0;
    std::vector<double> values;

    [[nodiscard]] double at(int row, int col) const
    {
        return values[std::size_t(row) * cols + col];
    }
};

auto qcp_columns(const Matrix& matrix)
{
    const auto column = [&matrix](int col)
    {
        return std::views::iota(0, matrix.rows)
            | std::views::transform([&matrix, col](int row) { return matrix.at(row, col); });
    };
    return std::views::iota(0, matrix.cols) | std::views::transform(column);
}

//! Both a range of numbers and a set of columns: the columns hook makes it 2D (P-7).
struct DoublesWithColumns : std::vector<double>
{
    using std::vector<double>::vector;
};

std::array<std::vector<double>, 2> qcp_columns(const DoublesWithColumns& doubles)
{
    const std::vector<double> column(doubles.begin(), doubles.end());
    return { column, column };
}

static_assert(QCPColumns<std::vector<std::vector<double>>>);
static_assert(QCPColumns<Matrix>);
static_assert(QCPColumns<DoublesWithColumns>);
static_assert(!QCPColumns<std::vector<double>>);
static_assert(!QCPColumns<Samples>);
static_assert(QCPNumberRange<DoublesWithColumns>);
static_assert(QCPColumns<SharedColumns>);
static_assert(!QCPColumns<WrappedColumns>);
static_assert(QCPNumberRange<std::ranges::range_value_t<ProxiedColumns>>);
static_assert(!QCPColumns<ProxiedColumns>);
static_assert(!QCPColumns<BuriedData>);
static_assert(!QCPNumberRange<BuriedData>);
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

void TestDataAdapters::aFilterHookIsRead()
{
    QVERIFY((qcp::toDoubles(Filtered { }) == std::vector<double> { 3.0, 4.0 }));

    const Filtered filtered;
    const QCPGraph graph(filtered, filtered);
    QCOMPARE(graph.dataCount(), 2);
    QCOMPARE(graph.dataMainKey(0), 3.0);
    QCOMPARE(graph.dataMainValue(1), 4.0);
}

void TestDataAdapters::aConstHookResultIsRead()
{
    QVERIFY((qcp::toDoubles(Frozen { }) == std::vector<double> { 7.0, 8.0 }));
    QVERIFY((std::ranges::equal(qcp::numbers(Frozen { }), std::vector<double> { 7.0, 8.0 })));
}

void TestDataAdapters::aByValueHookBuildsAGraph()
{
    const QCPGraph graph(Waveform { }, Waveform { });
    QCOMPARE(graph.dataCount(), 2);
    QCOMPARE(graph.dataMainKey(0), 2.0);
    QCOMPARE(graph.dataMainValue(0), 2.0);
    QCOMPARE(graph.dataMainKey(1), 4.0);
    QCOMPARE(graph.dataMainValue(1), 4.0);
}

void TestDataAdapters::setDataMovesItsStorage()
{
    QCPGraph graph;
    std::vector<double> keys { 1.0, 2.0, 3.0 };
    std::vector<double> values { 4.0, 5.0, 6.0 };
    const double* const keyBuffer = keys.data();
    const double* const valueBuffer = values.data();

    graph.setData(std::move(keys), std::move(values), true);

    const QCPMultiDataSource* const source = graph.dataSource();
    QVERIFY(source != nullptr);
    QCOMPARE(source->rawKeyData(), keyBuffer);
    QCOMPARE(source->rawColumn(0).data, static_cast<const void*>(valueBuffer));
}

void TestDataAdapters::mixedRangeTypesAreRead()
{
    const QVector<float> floats { 5.0f, 6.0f, 7.0f, 8.0f };
    const HookedGraph fromFloats(std::vector<double> { 1.0, 2.0, 3.0, 4.0 }, floats);
    QCOMPARE(fromFloats.dataCount(), 4);
    QCOMPARE(fromFloats.dataMainValue(0), 5.0);

    const auto shorter
        = std::views::iota(0, 9) | std::views::take_while([](int i) { return i < 4; });
    const HookedGraph fromView(std::vector<double> { 1.0, 2.0, 3.0, 4.0 }, shorter);
    QCOMPARE(fromView.dataCount(), 4);
    QCOMPARE(fromView.dataMainKey(3), 4.0);
}

void TestDataAdapters::aVectorOfVectorsGivesComponents()
{
    const std::vector<double> keys { 0.0, 1.0, 2.0 };
    const std::vector<std::vector<double>> columns { { 1.0, 2.0, 3.0 }, { 4.0, 5.0, 6.0 } };

    const QCPGraph graph(keys, columns);
    QCOMPARE(graph.componentCount(), 2);
    QCOMPARE(graph.dataCount(), 3);
    QCOMPARE(graph.componentValueAt(0, 2), 3.0);
    QCOMPARE(graph.componentValueAt(1, 2), 6.0);

    QCPGraph assigned;
    assigned.setData(keys, columns);
    QCOMPARE(assigned.componentCount(), 2);
    QCOMPARE(assigned.componentValueAt(0, 0), 1.0);
    QCOMPARE(assigned.componentValueAt(1, 2), 6.0);

    // The waterfall adapter transforms the values it serves (offsets, gain), so only its shape is
    // comparable here.
    const QCPWaterfallGraph waterfall(keys, columns);
    QCOMPARE(waterfall.componentCount(), 2);
    QCOMPARE(waterfall.dataCount(), 3);
}

void TestDataAdapters::aColumnsHookAdaptsAMatrix()
{
    const Matrix matrix { 3, 2, { 1.0, 4.0, 2.0, 5.0, 3.0, 6.0 } };
    const std::vector<double> keys { 0.0, 1.0, 2.0 };

    const QCPGraph graph(keys, matrix);
    QCOMPARE(graph.componentCount(), 2);
    QCOMPARE(graph.dataCount(), 3);
    QCOMPARE(graph.componentValueAt(0, 0), 1.0);
    QCOMPARE(graph.componentValueAt(0, 2), 3.0);
    QCOMPARE(graph.componentValueAt(1, 0), 4.0);
    QCOMPARE(graph.componentValueAt(1, 2), 6.0);
}

void TestDataAdapters::aNumbersHookDoesNotMakeColumns()
{
    static_assert(!QCPColumns<std::vector<double>>);
    static_assert(!QCPColumns<Samples>);
}

void TestDataAdapters::aColumnsTypeIsNotFlattened()
{
    static_assert(QCPNumberRange<DoublesWithColumns>);
    static_assert(QCPColumns<DoublesWithColumns>);

    const DoublesWithColumns both { 1.0, 2.0, 3.0 };
    const std::vector<double> keys { 0.0, 1.0, 2.0 };

    const QCPGraph graph(keys, both);
    QCOMPARE(graph.componentCount(), 2);
    QCOMPARE(graph.componentValueAt(1, 2), 3.0);
}

void TestDataAdapters::columnsAreCopiedOnce()
{
    std::vector<CountedRange> counted;
    counted.emplace_back(3);
    counted.emplace_back(3);
    const std::vector<double> keys { 0.0, 1.0, 2.0 };

    const QCPGraph readOnce(keys, counted);
    QCOMPARE(readOnce.componentCount(), 2);
    QCOMPARE(readOnce.componentValueAt(1, 2), 2.0);
    QCOMPARE(counted[0].reads(), 3);
    QCOMPARE(counted[1].reads(), 3);

    // One pass into a buffer of the graph's own: the copy is a different buffer from the caller's,
    // holding the same values, and the two columns never share one.
    const std::vector<std::vector<double>> columns { { 1.0, 2.0, 3.0 }, { 4.0, 5.0, 6.0 } };
    QCPGraph copyingGraph;
    copyingGraph.setData(keys, columns);
    const QCPMultiDataSource* const copyingSource = copyingGraph.dataSource();
    QVERIFY(copyingSource != nullptr);
    for (int column = 0; column < 2; ++column)
    {
        const QCPRawColumn copied = copyingSource->rawColumn(column);
        QCOMPARE(copied.type, QCPRawColumn::Type::Double);
        QVERIFY(copied.data != nullptr);
        QVERIFY(copied.data != static_cast<const void*>(columns[std::size_t(column)].data()));
        QCOMPARE(copyingSource->valueAt(column, 2), columns[std::size_t(column)][2]);
    }
    QVERIFY(copyingSource->rawColumn(0).data != copyingSource->rawColumn(1).data);
}

void TestDataAdapters::columnsAreTakenOverWhenMoved()
{
    QCPGraph graph;
    std::vector<double> keys { 0.0, 1.0, 2.0 };
    std::vector<std::vector<double>> columns { { 1.0, 2.0, 3.0 }, { 4.0, 5.0, 6.0 } };
    const double* const keyBuffer = keys.data();
    const double* const firstBuffer = columns[0].data();
    const double* const secondBuffer = columns[1].data();

    graph.setData(std::move(keys), std::move(columns), true);

    const QCPMultiDataSource* const source = graph.dataSource();
    QVERIFY(source != nullptr);
    QCOMPARE(source->rawKeyData(), keyBuffer);
    QCOMPARE(source->rawColumn(0).data, static_cast<const void*>(firstBuffer));
    QCOMPARE(source->rawColumn(1).data, static_cast<const void*>(secondBuffer));
}

void TestDataAdapters::columnsConceptMatchesTheRead()
{
    const SharedColumns columns;
    const std::vector<double> keys { 0.0, 1.0, 2.0 };

    const QCPGraph graph(keys, columns);
    QCOMPARE(graph.componentCount(), 2);
    QCOMPARE(graph.componentValueAt(0, 0), 1.0);
    QCOMPARE(graph.componentValueAt(0, 2), 3.0);
    QCOMPARE(graph.componentValueAt(1, 0), 4.0);
    QCOMPARE(graph.componentValueAt(1, 2), 6.0);
}

void TestDataAdapters::setDataTakesAnyRange()
{
    QCustomPlot plot;
    auto graph = plot.add<QCPGraph>();
    const std::vector<double> keys { 1.0, 2.0, 3.0 };
    const std::vector<double> values { 4.0, 5.0, 6.0 };

    graph.setData(keys, std::span<const double>(values));
    QCOMPARE(graph->dataCount(), 3);
    QCOMPARE(graph->dataMainValue(2), 6.0);

    // A single-pass view is read once, as a container is.
    const auto view
        = std::views::iota(0, 3) | std::views::transform([](int i) { return 10.0 + i; });
    graph.setData(keys, view);
    QCOMPARE(graph->dataMainKey(2), 3.0);
    QCOMPARE(graph->dataMainValue(0), 10.0);
    QCOMPARE(graph->dataMainValue(2), 12.0);

    // The by-value container overloads keep winning for their own arguments.
    graph.setData(std::span(keys), std::span(values), true);
    QCOMPARE(graph->dataCount(), 3);
    QCOMPARE(graph->dataMainValue(1), 5.0);
}

void TestDataAdapters::narrowAndIntegerRangesGiveTheSamePoints()
{
    const std::vector<double> keys { 1.0, 2.0, 3.0, 4.0 };
    const std::vector<double> values { 5.0, 6.0, 7.0, 8.0 };
    const std::array<double, 4> array { 5.0, 6.0, 7.0, 8.0 };
    const std::vector<int> integers { 5, 6, 7, 8 };

    const QCPGraph fromDoubles(keys, values);
    const QCPGraph fromSpan(keys, std::span<const double, 4>(array));
    const QCPGraph fromIntegers(keys, integers);

    QCOMPARE(fromSpan.dataCount(), fromDoubles.dataCount());
    QCOMPARE(fromIntegers.dataCount(), fromDoubles.dataCount());
    for (int i = 0; i < fromDoubles.dataCount(); ++i)
    {
        QCOMPARE(fromSpan.dataMainKey(i), fromDoubles.dataMainKey(i));
        QCOMPARE(fromSpan.dataMainValue(i), fromDoubles.dataMainValue(i));
        QCOMPARE(fromIntegers.dataMainKey(i), fromDoubles.dataMainKey(i));
        QCOMPARE(fromIntegers.dataMainValue(i), fromDoubles.dataMainValue(i));
    }
}

void TestDataAdapters::aConstRvalueIsCopied()
{
    // A const rvalue is owned, so it is copied into a non-const local: the concept says so instead
    // of the call failing inside its body, and a type that cannot be copied is refused.
    static_assert(std::invocable<decltype(qcp::numbers), const std::vector<double>&&>);
    static_assert(std::invocable<decltype(qcp::columns),
                                 const std::vector<std::vector<double>>&&>);
    static_assert(!std::invocable<decltype(qcp::numbers), const ImmovableNumbers&&>);
    static_assert(!std::invocable<decltype(qcp::columns), const ImmovableColumns&&>);
    static_assert(std::invocable<decltype(qcp::numbers), const ExpiringData&>);
    static_assert(std::invocable<decltype(qcp::columns), const ExpiringData&>);

    const ExpiringData expiring;
    const auto fromHook = qcp::numbers(expiring);
    QVERIFY((std::ranges::equal(fromHook, std::vector<double> { 5.0, 6.0 })));
    QVERIFY(std::ranges::data(fromHook) != expiring.numbers.data());
    QCOMPARE(std::ranges::distance(qcp::columns(expiring)), 2);

    std::vector<double> values { 3.0, 4.0 };
    const auto adapted = qcp::numbers(std::move(values));
    using Owned = std::ranges::owning_view<std::vector<double>>;
    static_assert(std::same_as<std::remove_cvref_t<decltype(adapted)>, Owned>);
    QVERIFY((std::ranges::equal(adapted, std::vector<double> { 3.0, 4.0 })));
    QVERIFY(adapted.base().data() != values.data());

    std::vector<std::vector<double>> columns { { 1.0, 2.0 }, { 3.0, 4.0 } };
    const auto adaptedColumns = qcp::columns(std::move(columns));
    QCOMPARE(std::ranges::distance(adaptedColumns), 2);
    auto column = std::ranges::begin(adaptedColumns);
    QVERIFY((qcp::toDoubles(*column) == std::vector<double> { 1.0, 2.0 }));
    ++column;
    QVERIFY((qcp::toDoubles(*column) == std::vector<double> { 3.0, 4.0 }));

    // A const lvalue is still the caller's storage and is read where it is.
    const std::vector<double> shared { 7.0, 8.0 };
    const auto inPlace = qcp::numbers(std::as_const(shared));
    static_assert(std::same_as<std::remove_cvref_t<decltype(inPlace)>, std::span<const double>>);
    QCOMPARE(inPlace.data(), shared.data());
}

void TestDataAdapters::columnsAreNotNumbers()
{
    static_assert(!QCPNumbersOnly<DoublesWithColumns>);
    static_assert(QCPNumbersOnly<std::vector<double>>);
    static_assert(!QCPNumbersOnly<std::vector<std::vector<double>>>);

    // A type with a columns hook is drawn as columns or refused: never flattened row-major.
    static_assert(!std::constructible_from<QCPBars, std::vector<double>, DoublesWithColumns>);
    static_assert(!std::constructible_from<QCPCurve, std::vector<double>, DoublesWithColumns>);
    static_assert(
        !std::constructible_from<QCPCurve, DoublesWithColumns, std::vector<double>, std::vector<double>>);
    static_assert(!std::constructible_from<QCPErrorBars, DoublesWithColumns>);
    static_assert(!std::constructible_from<QCPErrorBars, DoublesWithColumns, DoublesWithColumns>);
    static_assert(!std::constructible_from<QCPFinancial, std::vector<double>, DoublesWithColumns,
                                             std::vector<double>, std::vector<double>,
                                             std::vector<double>>);
    static_assert(!std::constructible_from<QCPStatisticalBox, std::vector<double>,
                                             DoublesWithColumns, std::vector<double>,
                                             std::vector<double>, std::vector<double>,
                                             std::vector<double>>);
    static_assert(!std::constructible_from<QCPPolarGraph, std::vector<double>, DoublesWithColumns>);
    static_assert(!std::constructible_from<HookedGraph, std::vector<double>, DoublesWithColumns>);

    // The two plottables with a columns form take it as columns.
    static_assert(std::constructible_from<QCPGraph, std::vector<double>, DoublesWithColumns>);
    static_assert(std::constructible_from<QCPWaterfallGraph, std::vector<double>, DoublesWithColumns>);
}
