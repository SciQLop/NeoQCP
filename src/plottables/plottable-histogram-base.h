#pragma once
#include <global.h>

//! What the 1-D and the 2-D histogram have in common: a bin count and a normalization.
//! They are not a QObject base because moc cannot mix a Q_OBJECT into a base that sits
//! behind classes with different QObject ancestors; each plottable declares the setters,
//! the refreshBinning() slot and the histogramReady() signal itself and forwards here.
namespace qcp::histogram {

//! The bin count of one histogram axis: at least one bin, and it never changes silently.
class BinCount
{
public:
    explicit constexpr BinCount(int bins) : mBins(bins) {}

    [[nodiscard]] constexpr int get() const { return mBins; }

    //! False when nothing changed, including when `bins` is not positive.
    bool set(int bins);

private:
    int mBins;
};

//! What the user asked for. Column normalization overrides it at draw time, so get() keeps
//! returning the requested value while the shown cells are column-normalized.
class Normalization
{
public:
    [[nodiscard]] constexpr QCP::HistogramNormalization get() const { return mValue; }

    //! False when nothing changed.
    bool set(QCP::HistogramNormalization value);

private:
    QCP::HistogramNormalization mValue = QCP::HistogramNormalization::Count;
};

} // namespace qcp::histogram
