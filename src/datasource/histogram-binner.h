#pragma once
#include "abstract-datasource.h"
#include <plottables/plottable-colormap.h>
#include <cmath>
#include <algorithm>

namespace qcp::algo {

inline QCPColorMapData* bin2d(const QCPAbstractDataSource& src, int keyBins, int valueBins)
{
    const int n = src.size();
    if (n == 0 || keyBins <= 0 || valueBins <= 0)
        return nullptr;

    bool foundKey = false, foundVal = false;
    QCPRange keyRange = src.keyRange(foundKey);
    QCPRange valRange = src.valueRange(foundVal);
    if (!foundKey || !foundVal)
        return nullptr;

    if (keyRange.size() == 0) { keyRange.lower -= 0.5; keyRange.upper += 0.5; }
    if (valRange.size() == 0) { valRange.lower -= 0.5; valRange.upper += 0.5; }

    auto* data = new QCPColorMapData(keyBins, valueBins, keyRange, valRange);
    data->fill(0);

    const double kBinWidth = keyRange.size() / keyBins;
    const double vBinWidth = valRange.size() / valueBins;

    for (int i = 0; i < n; ++i)
    {
        double k = src.keyAt(i);
        double v = src.valueAt(i);
        if (!std::isfinite(k) || !std::isfinite(v))
            continue;

        int kb = std::clamp(static_cast<int>((k - keyRange.lower) / kBinWidth), 0, keyBins - 1);
        int vb = std::clamp(static_cast<int>((v - valRange.lower) / vBinWidth), 0, valueBins - 1);
        data->setCell(kb, vb, data->cell(kb, vb) + 1.0);
    }

    data->recalculateDataBounds();
    return data;
}

} // namespace qcp::algo
