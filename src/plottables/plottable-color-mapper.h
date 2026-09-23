#pragma once
#include "../axis/axis.h"
#include "../colorgradient.h"
#include <QColor>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

namespace qcp {

//! Maps a per-point scalar to one of 256 gradient colours. The segment/point
//! drawing code only asks bucket(dataIndex) and color(bucket).
class ColorScalarMapper
{
public:
    static constexpr int kGap = -1;

    ColorScalarMapper() : mGradient(QCPColorGradient::gpJet) { rebuildLut(); }

    [[nodiscard]] bool hasValues() const { return mValues && !mValues->empty(); }
    [[nodiscard]] int size() const { return mValues ? static_cast<int>(mValues->size()) : 0; }

    void setValues(std::shared_ptr<const std::vector<double>> values)
    {
        mValues = std::move(values);
        ++mGeneration;
    }
    void clearValues() { mValues.reset(); ++mGeneration; }

    void setGradient(const QCPColorGradient& gradient) { mGradient = gradient; rebuildLut(); ++mGeneration; }
    [[nodiscard]] const QCPColorGradient& gradient() const { return mGradient; }
    void setRange(const QCPRange& range) { mRange = range; ++mGeneration; }
    [[nodiscard]] QCPRange range() const { return mRange; }
    void setScaleType(QCPAxis::ScaleType type) { mScaleType = type; ++mGeneration; }
    [[nodiscard]] QCPAxis::ScaleType scaleType() const { return mScaleType; }
    [[nodiscard]] quint64 generation() const { return mGeneration; }

    [[nodiscard]] int bucket(int dataIndex) const
    {
        if (dataIndex < 0 || dataIndex >= size())
            return kGap;
        const double v = (*mValues)[dataIndex];
        if (!std::isfinite(v))
            return kGap;
        const bool log = mScaleType == QCPAxis::stLogarithmic;
        if (log && v <= 0)
            return kGap;
        return toBucket(log ? logPosition(v) : linearPosition(v));
    }

    [[nodiscard]] QRgb color(int bucket) const
    {
        Q_ASSERT(bucket >= 0 && bucket < 256);   // callers skip kGap first
        return mLut[bucket];
    }

private:
    double linearPosition(double v) const
    {
        const double span = mRange.upper - mRange.lower;
        return span > 0 ? (v - mRange.lower) / span : 0.0;
    }
    double logPosition(double v) const
    {
        if (mRange.lower <= 0 || mRange.upper <= mRange.lower)
            return 0.0;
        return std::log(v / mRange.lower) / std::log(mRange.upper / mRange.lower);
    }
    static int toBucket(double t)
    {
        // Clamp the position itself before scaling: for a huge or infinite t (a value far
        // outside the colour range, or a position that overflowed to +/-inf), t * 255 can
        // exceed lround()'s representable range, which is undefined behaviour and yields
        // garbage (observed: bucket 0 instead of the correctly-clamped 255).
        t = std::clamp(t, 0.0, 1.0);
        return std::clamp(static_cast<int>(std::lround(t * 255.0)), 0, 255);
    }
    void rebuildLut()
    {
        std::array<double, 256> positions;
        for (int i = 0; i < 256; ++i)
            positions[i] = i / 255.0;
        QCPColorGradient g = mGradient;   // colorize() is non-const (lazy buffer)
        g.colorize(positions.data(), QCPRange(0, 1), mLut.data(), 256);
    }

    std::shared_ptr<const std::vector<double>> mValues;
    QCPColorGradient mGradient;
    QCPRange mRange {0, 1};
    QCPAxis::ScaleType mScaleType = QCPAxis::stLinear;
    std::array<QRgb, 256> mLut {};
    quint64 mGeneration = 0;
};

} // namespace qcp
