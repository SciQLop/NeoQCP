#include "test-third-party-arrays.h"
#include "../../../src/qcp.h"
#include <Eigen/Dense>
#include <array>
#include <cstddef>
#include <ranges>
#include <span>
#include <tuple>
#include <vector>
#include <xtensor/xtensor.hpp>
#include <xtensor/xview.hpp>

#if defined(__cpp_lib_mdspan)
#include <mdspan>
#else
#include <mdspan/mdspan.hpp>
#endif

namespace
{
//! A dense matrix's value column, as the Eigen hook below gives it out.
template <typename Derived>
auto eigenColumn(const Eigen::MatrixBase<Derived>& matrix, Eigen::Index column)
{
    return std::views::iota(Eigen::Index(0), matrix.rows())
        | std::views::transform([&matrix, column](Eigen::Index row)
                                { return double(matrix(row, column)); });
}

//! A 2D tensor's value column, as the xtensor hook below gives it out.
template <typename Container>
auto xtensorColumn(const Container& matrix, std::size_t column)
{
    return std::views::iota(std::size_t(0), matrix.shape()[0])
        | std::views::transform([&matrix, column](std::size_t row)
                                { return double(matrix(row, column)); });
}

//! A user type over a 2D mdspan: the shape of the grid is the mdspan's, and its value columns are
//! its rows, each a span built on the spot.
template <typename M>
struct Grid
{
    M values;
};

#if defined(__cpp_lib_mdspan)
using Grid2D = std::mdspan<const double, std::dextents<std::size_t, 2>>;
#else
using Grid2D = Kokkos::mdspan<const double, Kokkos::dextents<std::size_t, 2>>;
#endif

//! Every component of \a graph against the values the caller expects, row by row.
void requireComponents(const QCPGraph& graph, const std::vector<std::vector<double>>& expected)
{
    QCOMPARE(graph.componentCount(), int(expected.size()));
    for (int column = 0; column < int(expected.size()); ++column)
    {
        const std::vector<double>& expectedColumn = expected[std::size_t(column)];
        for (int row = 0; row < int(expectedColumn.size()); ++row)
            QCOMPARE(graph.componentValueAt(column, row), expectedColumn[std::size_t(row)]);
    }
}

//! `count` keys, 0, 1, 2, ...: the rows every case below is drawn against.
std::vector<double> keyColumn(int count)
{
    std::vector<double> keys(std::size_t(count), 0.0);
    for (int i = 0; i < count; ++i)
        keys[std::size_t(i)] = double(i);
    return keys;
}
} // namespace

namespace Eigen
{
//! The hook of a user who keeps matrices in Eigen: a 2D array gives its columns, so QCPColumns is
//! true for a MatrixXd and false for a vector, which stays one run of numbers. The constraint is
//! what keeps a vector out: an unconstrained overload would take it through the conversion a
//! MatrixXf has to a MatrixXd.
template <typename Derived>
    requires(Derived::ColsAtCompileTime != 1)
auto qcp_columns(const MatrixBase<Derived>& matrix)
{
    return std::views::iota(Eigen::Index(0), matrix.cols())
        | std::views::transform([&matrix](Eigen::Index column)
                                { return eigenColumn(matrix, column); });
}
} // namespace Eigen

namespace xt
{
//! A rank-2 tensor: an unconstrained overload would also take a 1D one, since a tensor converts to
//! a rank-2 tensor, so both hooks below are constrained on the rank of the shape.
template <typename Container>
concept RankTwo = std::tuple_size_v<typename Container::shape_type> == 2;

//! The opt-out of a user who does not want a 2D xtensor flattened: a deleted hook wins the call.
template <typename Container>
    requires RankTwo<Container>
void qcp_numbers(const Container&) = delete;

//! The hook of a user who keeps matrices in xtensor: its columns are the value columns.
template <typename Container>
    requires RankTwo<Container>
auto qcp_columns(const Container& matrix)
{
    return std::views::iota(std::size_t(0), matrix.shape()[1])
        | std::views::transform([&matrix](std::size_t column)
                                { return xtensorColumn(matrix, column); });
}
} // namespace xt

namespace
{
//! The hook of a user who keeps a 2D mdspan: its rows are the value columns, and every one of them
//! is a view the hook builds on the spot, owning nothing.
template <typename M>
auto qcp_columns(const Grid<M>& grid)
{
    return std::views::iota(std::size_t(0), grid.values.extent(0))
        | std::views::transform(
            [&grid](std::size_t row)
            {
                return std::span<const double>(grid.values.data_handle()
                                                   + row * std::size_t(grid.values.stride(0)),
                                               grid.values.extent(1));
            });
}
} // namespace

static_assert(QCPNumberRange<Eigen::VectorXd>);
static_assert(QCPNumberRange<Eigen::VectorXf>);
static_assert(QCPColumns<Eigen::MatrixXd>);
static_assert(QCPNumberRange<xt::xtensor<double, 1>>);
static_assert(!QCPNumberRange<xt::xtensor<double, 2>>);
static_assert(QCPColumns<xt::xtensor<double, 2>>);

void TestThirdPartyArrays::anEigenVectorIsReadInPlace()
{
    const std::vector<double> keys { 0.0, 1.0, 2.0, 3.0 };
    const Eigen::VectorXd values = Eigen::VectorXd::LinSpaced(4, 10.0, 40.0);

    QCustomPlot plot;
    const QCPHandle<QCPGraph> graph = plot.axisRect()->line(keys, values);
    QVERIFY(graph);
    QCOMPARE(graph->componentCount(), 1);
    for (int row = 0; row < 4; ++row)
        QCOMPARE(graph->componentValueAt(0, row), values[row]);
}

void TestThirdPartyArrays::anEigenFloatVectorIsRead()
{
    Eigen::VectorXf values(3);
    values << 1.5f, 2.5f, 3.5f;

    const QCPGraph graph(keyColumn(3), values);
    requireComponents(graph, { { 1.5, 2.5, 3.5 } });
}

void TestThirdPartyArrays::anEigenSegmentAndColumnAreRead()
{
    const Eigen::VectorXd vector = Eigen::VectorXd::LinSpaced(6, 0.0, 5.0);
    requireComponents(QCPGraph(keyColumn(3), vector.segment(1, 3)), { { 1.0, 2.0, 3.0 } });

    Eigen::MatrixXd matrix(3, 2);
    matrix << 1.0, 4.0, 2.0, 5.0, 3.0, 6.0;
    requireComponents(QCPGraph(keyColumn(3), matrix.col(1)), { { 4.0, 5.0, 6.0 } });
}

void TestThirdPartyArrays::anEigenMatrixGivesComponents()
{
    Eigen::MatrixXd matrix(3, 2);
    matrix << 1.0, 4.0, 2.0, 5.0, 3.0, 6.0;

    const QCPGraph graph(keyColumn(3), matrix);
    requireComponents(graph, { { 1.0, 2.0, 3.0 }, { 4.0, 5.0, 6.0 } });
}

void TestThirdPartyArrays::anXtensorIsRead()
{
    xt::xtensor<double, 1> values = { 1.0, 2.0, 3.0 };

    const QCPGraph graph(keyColumn(3), values);
    requireComponents(graph, { { 1.0, 2.0, 3.0 } });
}

void TestThirdPartyArrays::aSteppedXtensorViewIsRead()
{
    xt::xtensor<double, 1> values = { 0.0, 1.0, 2.0, 3.0, 4.0, 5.0 };

    const QCPGraph graph(keyColumn(3), xt::view(values, xt::range(0, 6, 2)));
    requireComponents(graph, { { 0.0, 2.0, 4.0 } });
}

void TestThirdPartyArrays::anXtensor2DIsColumnsNotNumbers()
{
    xt::xtensor<double, 2> matrix = { { 1.0, 4.0 }, { 2.0, 5.0 }, { 3.0, 6.0 } };

    // The deleted hook is the opt-out: the matrix does not flatten into one component, the
    // columns hook reads it as one component per column.
    const QCPGraph graph(keyColumn(3), matrix);
    requireComponents(graph, { { 1.0, 2.0, 3.0 }, { 4.0, 5.0, 6.0 } });
}

void TestThirdPartyArrays::anMdspanGridGivesComponents()
{
    const std::array<double, 6> rows { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };
    const Grid<Grid2D> grid { Grid2D(rows.data(), 2, 3) };

    const QCPGraph graph(keyColumn(3), grid);
    requireComponents(graph, { { 1.0, 2.0, 3.0 }, { 4.0, 5.0, 6.0 } });
}

void TestThirdPartyArrays::aStridedViewIsRead()
{
    const std::vector<double> samples { 0.0, 1.0, 2.0, 3.0, 4.0, 5.0 };
    // A stride is an iota of the destination indices into the source: std::views::stride is not on
    // every supported standard library.
    const auto everyOther = std::views::iota(std::size_t(0), samples.size() / 2)
        | std::views::transform([&samples](std::size_t index) { return samples[index * 2]; });

    const QCPGraph graph(keyColumn(3), everyOther);
    requireComponents(graph, { { 0.0, 2.0, 4.0 } });
}

void TestThirdPartyArrays::aTransformViewOfFloatIsRead()
{
    const std::vector<float> measured { 1.5f, 2.5f, 3.5f };
    const auto doubled
        = measured | std::views::transform([](float value) { return 2.0f * value; });

    const QCPGraph graph(keyColumn(3), doubled);
    requireComponents(graph, { { 3.0, 5.0, 7.0 } });
}

QTEST_MAIN(TestThirdPartyArrays)
