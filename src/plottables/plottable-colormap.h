/***************************************************************************
**                                                                        **
**  QCustomPlot, an easy to use, modern plotting widget for Qt            **
**  Copyright (C) 2011-2022 Emanuel Eichhammer                            **
**                                                                        **
**  This program is free software: you can redistribute it and/or modify  **
**  it under the terms of the GNU General Public License as published by  **
**  the Free Software Foundation, either version 3 of the License, or     **
**  (at your option) any later version.                                   **
**                                                                        **
**  This program is distributed in the hope that it will be useful,       **
**  but WITHOUT ANY WARRANTY; without even the implied warranty of        **
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         **
**  GNU General Public License for more details.                          **
**                                                                        **
**  You should have received a copy of the GNU General Public License     **
**  along with this program.  If not, see http://www.gnu.org/licenses/.   **
**                                                                        **
****************************************************************************
**           Author: Emanuel Eichhammer                                   **
**  Website/Contact: https://www.qcustomplot.com/                         **
**             Date: 06.11.22                                             **
**          Version: 2.1.1                                                **
****************************************************************************/

#ifndef QCP_PLOTTABLE_COLORMAP_H
#define QCP_PLOTTABLE_COLORMAP_H

#include "../axis/range.h"
#include "../colorgradient.h"
#include "../global.h"
#include "../plottables/plottable.h"
#include "plottable-colormapped.h"

class QCPPainter;
class QCPAxis;
class QCPColorMap;
class QCPColorScale;

class QCP_LIB_DECL QCPColorMapData
{
public:
    QCPColorMapData(int keySize, int valueSize, const QCPRange& keyRange,
                    const QCPRange& valueRange);
    ~QCPColorMapData();
    QCPColorMapData(const QCPColorMapData& other);
    QCPColorMapData& operator=(const QCPColorMapData& other);

    // getters:
    int keySize() const { return mKeySize; }

    int valueSize() const { return mValueSize; }

    QCPRange keyRange() const { return mKeyRange; }

    QCPRange valueRange() const { return mValueRange; }

    QCPRange dataBounds() const { return mDataBounds; }

    const double* rawData() const { return mData; }

    double* rawData() { return mData; }

    double data(double key, double value);
    double cell(int keyIndex, int valueIndex) const;
    unsigned char alpha(int keyIndex, int valueIndex);

    // setters:
    void setSize(int keySize, int valueSize);
    void setKeySize(int keySize);
    void setValueSize(int valueSize);
    void setRange(const QCPRange& keyRange, const QCPRange& valueRange);
    void setKeyRange(const QCPRange& keyRange);
    void setValueRange(const QCPRange& valueRange);
    void setData(double key, double value, double z);
    void setCell(int keyIndex, int valueIndex, double z);
    void setAlpha(int keyIndex, int valueIndex, unsigned char alpha);

    // non-property methods:
    void recalculateDataBounds();
    void clear();
    void clearAlpha();
    void fill(double z);
    void fillAlpha(unsigned char alpha);

    bool isEmpty() const { return mIsEmpty; }

    void coordToCell(double key, double value, int* keyIndex, int* valueIndex) const;
    void cellToCoord(int keyIndex, int valueIndex, double* key, double* value) const;

protected:
    // property members:
    int mKeySize, mValueSize;
    QCPRange mKeyRange, mValueRange;
    bool mIsEmpty;

    // non-property members:
    double* mData;
    unsigned char* mAlpha;
    QCPRange mDataBounds;
    bool mDataModified;

    void dataChanged();
    bool createAlpha(bool initializeOpaque = true);

    QPointer<QCPColorMap> mOwner; // the map that draws this data, null when none does

    friend class QCPColorMap;
};

template <typename T, typename Self = void>
class QCPColorMapOps;

class QCP_LIB_DECL QCPColorMap : public QCPAbstractColorMapped
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(bool interpolate READ interpolate WRITE setInterpolate)
    Q_PROPERTY(bool tightBoundary READ tightBoundary WRITE setTightBoundary)
    /// \endcond

public:
    template <typename T>
    using OpsFor = QCPColorMapOps<T>;

    explicit QCPColorMap(QCPAxis* keyAxis, QCPAxis* valueAxis);
    virtual ~QCPColorMap() override;

    // getters:
    [[nodiscard]] QCPColorMapData* data() const { return mMapData; }

    [[nodiscard]] bool interpolate() const { return mInterpolate; }

    [[nodiscard]] bool tightBoundary() const { return mTightBoundary; }

    // setters:
    void setData(QCPColorMapData* data, bool copy = false);
    void setInterpolate(bool enabled);
    void setTightBoundary(bool enabled);

    // non-property methods:
    void rescaleDataRange(bool recalculateDataBounds = false);
    QCPRange colorDataBounds(bool& found) const override;
    Q_SLOT void updateLegendIcon(Qt::TransformationMode transformMode = Qt::SmoothTransformation,
                                 const QSize& thumbSize = QSize(32, 18));

    // reimplemented virtual methods:
    virtual double selectTest(const QPointF& pos, bool onlySelectable,
                              QVariant* details = nullptr) const override;
    virtual QCPRange getKeyRange(bool& foundRange,
                                 QCP::SignDomain inSignDomain = QCP::sdBoth) const override;
    virtual QCPRange getValueRange(bool& foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth,
                                   const QCPRange& inKeyRange = QCPRange()) const override;

protected:
    // property members:
    QCPColorMapData* mMapData;
    bool mInterpolate;
    bool mTightBoundary;
    QPointer<QCPColorScale> mColorScale;

    // non-property members:
    QImage mMapImage, mUndersampledMapImage;
    QPixmap mLegendIcon;
    bool mMapImageInvalidated;

    // introduced virtual methods:
    virtual void updateMapImage();
    void colorMappingChanged() override;

    // reimplemented virtual methods:
    virtual void draw(QCPPainter* painter) override;
    virtual void drawLegendIcon(QCPPainter* painter, const QRectF& rect) const override;

    friend class QCustomPlot;
    friend class QCPLegend;
    friend class QCPColorMapData;
};

// The fluent handle of a colour map; see QCPPlottableOps for the subclass contract.
template <typename T, typename Self>
class QCPColorMapOps : public QCPPlottableOps<T, qcp::OpsRet<Self, QCPColorMapOps<T>>>
{
    using Base = QCPPlottableOps<T, qcp::OpsRet<Self, QCPColorMapOps<T>>>;

public:
    using Base::Base;
    using typename Base::Ret;

    // The map takes the data over (it frees it, as setData(data, false) does).
    Ret setData(std::unique_ptr<QCPColorMapData> data)
    {
        return this->apply([&](T& p) { p.setData(data.release(), false); });
    }

    // The map keeps its own copy.
    Ret setData(const QCPColorMapData& data)
    {
        return this->apply([&](T& p) { p.setData(const_cast<QCPColorMapData*>(&data), true); });
    }

    Ret setInterpolate(bool enabled)
    {
        return this->apply([&](T& p) { p.setInterpolate(enabled); });
    }

    Ret setTightBoundary(bool enabled)
    {
        return this->apply([&](T& p) { p.setTightBoundary(enabled); });
    }

    Ret setGradient(const QCPColorGradient& gradient)
    {
        return this->apply([&](T& p) { p.setGradient(gradient); });
    }

    Ret setDataRange(const QCPRange& range)
    {
        return this->apply([&](T& p) { p.setDataRange(range); });
    }

    Ret setDataScaleType(QCPAxis::ScaleType type)
    {
        return this->apply([&](T& p) { p.setDataScaleType(type); });
    }

    // The scale the map follows; an empty or dead handle detaches it.
    Ret setColorScale(const QCPHandle<QCPColorScale>& scale)
    {
        return this->apply([&](T& p) { p.setColorScale(scale.get()); });
    }
};

#endif // QCP_PLOTTABLE_COLORMAP_H
