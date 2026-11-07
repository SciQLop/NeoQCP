#include "plottable-histogram-base.h"

namespace qcp::histogram {

bool BinCount::set(int bins)
{
    if (bins <= 0 || bins == mBins)
        return false;
    mBins = bins;
    return true;
}

bool Normalization::set(QCP::HistogramNormalization value)
{
    if (value == mValue)
        return false;
    mValue = value;
    return true;
}

} // namespace qcp::histogram
