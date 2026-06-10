#pragma once
#include "abstract-datasource-2d.h"
#include "algorithms-2d.h"
#include <QtGlobal>
#include <memory>
#include <ranges>
#include <span>

template <IndexableNumericRange XC, IndexableNumericRange YC, IndexableNumericRange ZC>
class QCPSoADataSource2D final : public QCPAbstractDataSource2D
{
public:
    using X = std::ranges::range_value_t<XC>;
    using Y = std::ranges::range_value_t<YC>;
    using Z = std::ranges::range_value_t<ZC>;

    QCPSoADataSource2D(XC x, YC y, ZC z, std::shared_ptr<const void> dataGuard = {})
        : mX(std::move(x)), mY(std::move(y)), mZ(std::move(z)),
          mDataGuard(std::move(dataGuard))
    {
        const auto nx = std::ranges::size(mX);
        const auto ny = std::ranges::size(mY);
        const auto nz = std::ranges::size(mZ);
        // Shape lies from callers must degrade to an empty source, not become
        // OOB reads (release) or aborts (debug) — this is a public entry point.
        const bool valid = nx > 0 && nz > 0 && nz % nx == 0
            && (ny == nz || ny == nz / nx);
        if (valid)
        {
            mYSize = static_cast<int>(nz / nx);
            mYIs2D = (ny == nz);
        }
        else
        {
            if (nx != 0 || ny != 0 || nz != 0)
                qWarning("QCPSoADataSource2D: inconsistent shapes (nx=%zu, ny=%zu, nz=%zu) — dropping data",
                         static_cast<std::size_t>(nx), static_cast<std::size_t>(ny),
                         static_cast<std::size_t>(nz));
            mX = {};
            mY = {};
            mZ = {};
            mYSize = 0;
            mYIs2D = false;
        }
    }

    const XC& x() const { return mX; }
    const YC& y() const { return mY; }
    const ZC& z() const { return mZ; }

    int xSize() const override { return static_cast<int>(std::ranges::size(mX)); }
    int ySize() const override { return mYSize; }
    bool yIs2D() const override { return mYIs2D; }

    double xAt(int i) const override { return static_cast<double>(mX[i]); }

    double yAt(int i, int j) const override
    {
        return mYIs2D ? static_cast<double>(mY[i * mYSize + j])
                      : static_cast<double>(mY[j]);
    }

    double zAt(int i, int j) const override
    {
        return static_cast<double>(mZ[i * mYSize + j]);
    }

    QCPRange xRange(bool& found, QCP::SignDomain sd = QCP::sdBoth) const override
    {
        return qcp::algo2d::xRange(mX, found, sd);
    }

    QCPRange yRange(bool& found, QCP::SignDomain sd = QCP::sdBoth) const override
    {
        return qcp::algo2d::yRange(mY, found, sd);
    }

    QCPRange zRange(bool& found, int xBegin = 0, int xEnd = -1) const override
    {
        return qcp::algo2d::zRange(mZ, mYSize, found, xBegin, xEnd);
    }

    int findXBegin(double sortKey) const override
    {
        return qcp::algo2d::findXBegin(mX, sortKey);
    }

    int findXEnd(double sortKey) const override
    {
        return qcp::algo2d::findXEnd(mX, sortKey);
    }

    const double* rawX() const override
    {
        if constexpr (std::is_same_v<X, double>)
            return std::ranges::data(mX);
        else
            return nullptr;
    }

    const double* rawY() const override
    {
        if constexpr (std::is_same_v<Y, double>)
            return std::ranges::data(mY);
        else
            return nullptr;
    }

    const double* rawZ() const override
    {
        if constexpr (std::is_same_v<Z, double>)
            return std::ranges::data(mZ);
        else
            return nullptr;
    }

private:
    XC mX;
    YC mY;
    ZC mZ;
    int mYSize;
    bool mYIs2D;
    std::shared_ptr<const void> mDataGuard;
};
