#include "data-locator.h"
#include <algorithm>
#include <cmath>
#include "plottables/plottable-graph.h"
#include "plottables/plottable-graph2.h"
#include "plottables/plottable-curve.h"
#include "plottables/plottable-colormap.h"
#include "plottables/plottable-colormap2.h"
#include "plottables/plottable-histogram2d.h"
#include "plottables/plottable-multigraph.h"
#include "selection.h"

void QCPDataLocator::setPlottable(QCPAbstractPlottable* plottable)
{
    mPlottable = plottable;
    mHitPlottable = nullptr;
    mValid = false;
    mKey = 0;
    mValue = 0;
    mData = std::numeric_limits<double>::quiet_NaN();
    mDataIndex = -1;
    mComponentValues.clear();
}

bool QCPDataLocator::locate(const QPointF& pixelPos)
{
    mValid = false;
    if (!mPlottable)
        return false;

    // Try specific types in order (derived before base where needed)
    if (qobject_cast<QCPGraph2*>(mPlottable))
        return locateGraph2(pixelPos);
    if (qobject_cast<QCPGraph*>(mPlottable))
        return locateGraph(pixelPos);
    if (qobject_cast<QCPCurve*>(mPlottable))
        return locateCurve(pixelPos);
    if (qobject_cast<QCPColorMap2*>(mPlottable))
        return locateColorMap2(pixelPos);
    if (qobject_cast<QCPColorMap*>(mPlottable))
        return locateColorMap(pixelPos);
    if (qobject_cast<QCPHistogram2D*>(mPlottable))
        return locateHistogram2D(pixelPos);
    if (qobject_cast<QCPMultiGraph*>(mPlottable))
        return locateMultiGraph(pixelPos);

    return false;
}

bool QCPDataLocator::locateGraph(const QPointF& pixelPos)
{
    auto* graph = qobject_cast<QCPGraph*>(mPlottable);
    QVariant details;
    double dist = graph->selectTest(pixelPos, false, &details);
    if (dist < 0)
        return false;

    auto sel = details.value<QCPDataSelection>();
    if (sel.dataRangeCount() == 0)
        return false;

    int index = sel.dataRange(0).begin();
    auto it = graph->data()->at(index);
    mKey = it->key;
    mValue = it->value;
    mDataIndex = index;
    mHitPlottable = mPlottable;
    mValid = true;
    return true;
}

bool QCPDataLocator::locateGraph2(const QPointF& pixelPos)
{
    auto* g2 = qobject_cast<QCPGraph2*>(mPlottable);
    QVariant details;
    double dist = g2->selectTest(pixelPos, false, &details);
    if (dist < 0)
        return false;

    auto sel = details.value<QCPDataSelection>();
    if (sel.dataRangeCount() == 0)
        return false;

    int index = sel.dataRange(0).begin();
    auto* src = g2->dataSource();
    if (!src || index < 0 || index >= src->size())
        return false;

    mKey = src->keyAt(index);
    mValue = src->valueAt(index);
    mDataIndex = index;
    mHitPlottable = mPlottable;
    mValid = true;
    return true;
}

bool QCPDataLocator::locateCurve(const QPointF& pixelPos)
{
    auto* curve = qobject_cast<QCPCurve*>(mPlottable);
    QVariant details;
    double dist = curve->selectTest(pixelPos, false, &details);
    if (dist < 0)
        return false;

    auto sel = details.value<QCPDataSelection>();
    if (sel.dataRangeCount() == 0)
        return false;

    int index = sel.dataRange(0).begin();
    auto it = curve->data()->at(index);
    mKey = it->key;
    mValue = it->value;
    mDataIndex = index;
    mHitPlottable = mPlottable;
    mValid = true;
    return true;
}

bool QCPDataLocator::locateColorMap(const QPointF& pixelPos)
{
    auto* cm = qobject_cast<QCPColorMap*>(mPlottable);
    double key, value;
    cm->pixelsToCoords(pixelPos, key, value);

    auto* mapData = cm->data();
    if (!mapData || mapData->isEmpty())
        return false;

    int keyIndex, valueIndex;
    mapData->coordToCell(key, value, &keyIndex, &valueIndex);

    if (keyIndex < 0 || keyIndex >= mapData->keySize()
        || valueIndex < 0 || valueIndex >= mapData->valueSize())
        return false;

    mKey = key;
    mValue = value;
    mData = mapData->cell(keyIndex, valueIndex);
    mDataIndex = keyIndex * mapData->valueSize() + valueIndex;
    mHitPlottable = mPlottable;
    mValid = true;
    return true;
}

bool QCPDataLocator::locateColorMap2(const QPointF& pixelPos)
{
    auto* cm2 = qobject_cast<QCPColorMap2*>(mPlottable);
    double key, value;
    cm2->pixelsToCoords(pixelPos, key, value);

    auto* src = cm2->dataSource();
    if (!src || src->xSize() == 0 || src->ySize() == 0)
        return false;

    // findXBegin uses expandedRange (for viewport rendering), so it may be one
    // index before the actual nearest. Compare with neighbor to find true nearest.
    int xi = std::clamp(src->findXBegin(key), 0, src->xSize() - 1);
    if (xi + 1 < src->xSize()
        && std::abs(src->xAt(xi + 1) - key) < std::abs(src->xAt(xi) - key))
        ++xi;
    int yj = 0;
    double bestDy = std::numeric_limits<double>::max();
    for (int j = 0; j < src->ySize(); ++j)
    {
        double dy = std::abs(src->yAt(xi, j) - value);
        if (dy < bestDy) { bestDy = dy; yj = j; }
    }

    mKey = key;
    mValue = value;
    mData = src->zAt(xi, yj);
    mDataIndex = xi * src->ySize() + yj;
    mHitPlottable = mPlottable;
    mValid = true;
    return true;
}

bool QCPDataLocator::locateHistogram2D(const QPointF& pixelPos)
{
    auto* hist = qobject_cast<QCPHistogram2D*>(mPlottable);
    auto* mapData = hist->pipeline().result();
    if (!mapData || mapData->isEmpty())
        return false;

    double key, value;
    hist->pixelsToCoords(pixelPos, key, value);

    int keyIndex, valueIndex;
    mapData->coordToCell(key, value, &keyIndex, &valueIndex);

    if (keyIndex < 0 || keyIndex >= mapData->keySize()
        || valueIndex < 0 || valueIndex >= mapData->valueSize())
        return false;

    mKey = key;
    mValue = value;
    mData = mapData->cell(keyIndex, valueIndex);
    mDataIndex = keyIndex * mapData->valueSize() + valueIndex;
    mHitPlottable = mPlottable;
    mValid = true;
    return true;
}

bool QCPDataLocator::locateMultiGraph(const QPointF& pixelPos)
{
    auto* mg = qobject_cast<QCPMultiGraph*>(mPlottable);
    QVariant details;
    double dist = mg->selectTest(pixelPos, false, &details);
    if (dist < 0)
        return false;

    auto map = details.toMap();
    if (!map.contains("dataIndex"))
        return false;

    // selectTest stores pre-computed coordinates, safe even when
    // the index refers to resampled (L2) data rather than the source.
    mKey = map["key"].toDouble();
    mValue = map["value"].toDouble();
    mDataIndex = map["dataIndex"].toInt();
    mHitPlottable = mPlottable;
    mValid = true;
    return true;
}

bool QCPDataLocator::locateAtKey(QCPAbstractPlottable* plottable, double key, double value)
{
    mValid = false;
    mKey = 0;
    mValue = 0;
    mData = std::numeric_limits<double>::quiet_NaN();
    mDataIndex = -1;
    mComponentValues.clear();
    mPlottable = plottable;
    mHitPlottable = nullptr;

    if (!mPlottable)
        return false;

    if (qobject_cast<QCPGraph2*>(mPlottable))
        return locateGraph2AtKey(key);
    if (qobject_cast<QCPGraph*>(mPlottable))
        return locateGraphAtKey(key);
    if (qobject_cast<QCPMultiGraph*>(mPlottable))
        return locateMultiGraphAtKey(key);
    if (!std::isnan(value))
    {
        if (qobject_cast<QCPColorMap2*>(mPlottable))
            return locateColorMap2AtKey(key, value);
        if (qobject_cast<QCPColorMap*>(mPlottable))
            return locateColorMapAtKey(key, value);
        if (qobject_cast<QCPHistogram2D*>(mPlottable))
            return locateHistogram2DAtKey(key, value);
    }

    return false;
}

bool QCPDataLocator::locateGraphAtKey(double key)
{
    auto* graph = qobject_cast<QCPGraph*>(mPlottable);
    auto* data = graph->data().data();
    if (!data || data->isEmpty())
        return false;

    auto it = data->findBegin(key, false);
    if (it == data->constEnd())
    {
        --it; // use last point
    }
    else if (it != data->constBegin())
    {
        auto prev = it;
        --prev;
        if (std::abs(prev->key - key) < std::abs(it->key - key))
            it = prev;
    }

    mKey = it->key;
    mValue = it->value;
    mDataIndex = static_cast<int>(it - data->constBegin());
    mHitPlottable = mPlottable;
    mValid = true;
    return true;
}

bool QCPDataLocator::locateGraph2AtKey(double key)
{
    auto* g2 = qobject_cast<QCPGraph2*>(mPlottable);
    auto* src = g2->dataSource();
    if (!src || src->size() == 0)
        return false;

    int idx = src->findEnd(key, false);
    int lo = std::max(0, idx - 1);
    int hi = std::min(idx, src->size() - 1);

    int nearest = lo;
    if (lo != hi && std::abs(src->keyAt(hi) - key) < std::abs(src->keyAt(lo) - key))
        nearest = hi;

    mKey = src->keyAt(nearest);
    mValue = src->valueAt(nearest);
    mDataIndex = nearest;
    mHitPlottable = mPlottable;
    mValid = true;
    return true;
}

bool QCPDataLocator::locateMultiGraphAtKey(double key)
{
    auto* mg = qobject_cast<QCPMultiGraph*>(mPlottable);
    auto* src = mg->dataSource();
    if (!src || src->size() == 0)
        return false;

    int idx = src->findEnd(key, false);
    int lo = std::max(0, idx - 1);
    int hi = std::min(idx, src->size() - 1);

    int nearest = lo;
    if (lo != hi && std::abs(src->keyAt(hi) - key) < std::abs(src->keyAt(lo) - key))
        nearest = hi;

    mKey = src->keyAt(nearest);
    mValue = src->valueAt(0, nearest);
    mDataIndex = nearest;

    int cols = src->columnCount();
    mComponentValues.resize(cols);
    for (int c = 0; c < cols; ++c)
        mComponentValues[c] = src->valueAt(c, nearest);

    mHitPlottable = mPlottable;
    mValid = true;
    return true;
}

bool QCPDataLocator::locateColorMap2AtKey(double key, double value)
{
    auto* cm2 = qobject_cast<QCPColorMap2*>(mPlottable);
    auto* src = cm2->dataSource();
    if (!src || src->xSize() == 0 || src->ySize() == 0)
        return false;

    bool foundX = false, foundY = false;
    const QCPRange xr = src->xRange(foundX);
    const QCPRange yr = src->yRange(foundY);
    if (foundX && (key < xr.lower || key > xr.upper))
        return false;
    if (foundY && (value < yr.lower || value > yr.upper))
        return false;

    int xi = std::clamp(src->findXBegin(key), 0, src->xSize() - 1);
    if (xi + 1 < src->xSize()
        && std::abs(src->xAt(xi + 1) - key) < std::abs(src->xAt(xi) - key))
        ++xi;

    int yj = 0;
    double bestDy = std::numeric_limits<double>::max();
    for (int j = 0; j < src->ySize(); ++j)
    {
        double dy = std::abs(src->yAt(xi, j) - value);
        if (dy < bestDy) { bestDy = dy; yj = j; }
    }

    mKey = src->xAt(xi);
    mValue = src->yAt(xi, yj);
    mData = src->zAt(xi, yj);
    mDataIndex = xi * src->ySize() + yj;
    mHitPlottable = mPlottable;
    mValid = true;
    return true;
}

bool QCPDataLocator::locateColorMapAtKey(double key, double value)
{
    auto* cm = qobject_cast<QCPColorMap*>(mPlottable);
    auto* mapData = cm->data();
    if (!mapData || mapData->isEmpty())
        return false;

    const QCPRange kr = mapData->keyRange();
    const QCPRange vr = mapData->valueRange();
    if (key < kr.lower || key > kr.upper || value < vr.lower || value > vr.upper)
        return false;

    int keyIndex, valueIndex;
    mapData->coordToCell(key, value, &keyIndex, &valueIndex);

    if (keyIndex < 0 || keyIndex >= mapData->keySize()
        || valueIndex < 0 || valueIndex >= mapData->valueSize())
        return false;

    double cellKey, cellValue;
    mapData->cellToCoord(keyIndex, valueIndex, &cellKey, &cellValue);
    mKey = cellKey;
    mValue = cellValue;
    mData = mapData->cell(keyIndex, valueIndex);
    mDataIndex = keyIndex * mapData->valueSize() + valueIndex;
    mHitPlottable = mPlottable;
    mValid = true;
    return true;
}

bool QCPDataLocator::locateHistogram2DAtKey(double key, double value)
{
    auto* hist = qobject_cast<QCPHistogram2D*>(mPlottable);
    auto* mapData = hist->pipeline().result();
    if (!mapData || mapData->isEmpty())
        return false;

    const QCPRange kr = mapData->keyRange();
    const QCPRange vr = mapData->valueRange();
    if (key < kr.lower || key > kr.upper || value < vr.lower || value > vr.upper)
        return false;

    int keyIndex, valueIndex;
    mapData->coordToCell(key, value, &keyIndex, &valueIndex);

    if (keyIndex < 0 || keyIndex >= mapData->keySize()
        || valueIndex < 0 || valueIndex >= mapData->valueSize())
        return false;

    double cellKey, cellValue;
    mapData->cellToCoord(keyIndex, valueIndex, &cellKey, &cellValue);
    mKey = cellKey;
    mValue = cellValue;
    mData = mapData->cell(keyIndex, valueIndex);
    mDataIndex = keyIndex * mapData->valueSize() + valueIndex;
    mHitPlottable = mPlottable;
    mValid = true;
    return true;
}
