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
/*! \file */
#ifndef QCP_PLOTTABLE_BARS_H
#define QCP_PLOTTABLE_BARS_H

#include "../axis/range.h"
#include "../datacontainer.h"
#include "../global.h"
#include "../plottables/plottable1d.h"

class QCPPainter;
class QCPAxis;
class QCPBars;

template <typename T, typename Self = void>
class QCPBarsGroupOps;

class QCP_LIB_DECL QCPBarsGroup : public QObject
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(SpacingType spacingType READ spacingType WRITE setSpacingType)
    Q_PROPERTY(double spacing READ spacing WRITE setSpacing)
    /// \endcond

public:
    template <typename T>
    using OpsFor = QCPBarsGroupOps<T>;

    /*!
      Defines the ways the spacing between bars in the group can be specified. Thus it defines what
      the number passed to \ref setSpacing actually means.

      \see setSpacingType, setSpacing
    */
    enum SpacingType
    {
        stAbsolute ///< Bar spacing is in absolute pixels
        ,
        stAxisRectRatio ///< Bar spacing is given by a fraction of the axis rect size
        ,
        stPlotCoords ///< Bar spacing is in key coordinates and thus scales with the key axis range
    };
    Q_ENUM(SpacingType)

    explicit QCPBarsGroup(QCustomPlot* parentPlot);
    virtual ~QCPBarsGroup();

    // getters:
    [[nodiscard]] SpacingType spacingType() const { return mSpacingType; }

    [[nodiscard]] double spacing() const { return mSpacing; }

    // setters:
    void setSpacingType(SpacingType spacingType);
    void setSpacing(double spacing);

    // non-virtual methods:
    [[nodiscard]] QList<QCPBars*> bars() const { return mBars; }

    [[nodiscard]] QCPBars* bars(int index) const;

    [[nodiscard]] int size() const { return mBars.size(); }

    [[nodiscard]] bool isEmpty() const { return mBars.isEmpty(); }

    void clear();

    [[nodiscard]] bool contains(QCPBars* bars) const { return mBars.contains(bars); }

    void append(QCPBars* bars);
    void insert(int i, QCPBars* bars);
    void remove(QCPBars* bars);

protected:
    // non-property members:
    QCustomPlot* mParentPlot;
    SpacingType mSpacingType;
    double mSpacing;
    QList<QCPBars*> mBars;

    // non-virtual methods:
    void registerBars(QCPBars* bars);
    void unregisterBars(QCPBars* bars);

    // virtual methods:
    double keyPixelOffset(const QCPBars* bars, double keyCoord);
    double getPixelSpacing(const QCPBars* bars, double keyCoord);

private:
    Q_DISABLE_COPY(QCPBarsGroup)

    friend class QCPBars;
};
Q_DECLARE_METATYPE(QCPBarsGroup::SpacingType)

class QCP_LIB_DECL QCPBarsData
{
public:
    QCPBarsData();
    QCPBarsData(double key, double value);

    inline double sortKey() const { return key; }

    inline static QCPBarsData fromSortKey(double sortKey) { return QCPBarsData(sortKey, 0); }

    inline static bool sortKeyIsMainKey() { return true; }

    inline double mainKey() const { return key; }

    inline double mainValue() const { return value; }

    inline QCPRange valueRange() const
    {
        return QCPRange(value, value);
    } // note that bar base value isn't held in each QCPBarsData and thus can't/shouldn't be
      // returned here

    double key, value;
};

Q_DECLARE_TYPEINFO(QCPBarsData, Q_PRIMITIVE_TYPE);


/*! \typedef QCPBarsDataContainer

  Container for storing \ref QCPBarsData points. The data is stored sorted by \a key.

  This template instantiation is the container in which QCPBars holds its data. For details about
  the generic container, see the documentation of the class template \ref QCPDataContainer.

  \see QCPBarsData, QCPBars::setData
*/
using QCPBarsDataContainer = QCPDataContainer<QCPBarsData>;

template <typename T, typename Self = void>
class QCPBarsOps;

class QCP_LIB_DECL QCPBars : public QCPAbstractPlottable1D<QCPBarsData>
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(double width READ width WRITE setWidth)
    Q_PROPERTY(WidthType widthType READ widthType WRITE setWidthType)
    Q_PROPERTY(QCPBarsGroup* barsGroup READ barsGroup WRITE setBarsGroup)
    Q_PROPERTY(double baseValue READ baseValue WRITE setBaseValue)
    Q_PROPERTY(double stackingGap READ stackingGap WRITE setStackingGap)
    Q_PROPERTY(QCPBars* barBelow READ barBelow)
    Q_PROPERTY(QCPBars* barAbove READ barAbove)
    /// \endcond

public:
    template <typename T>
    using OpsFor = QCPBarsOps<T>;

    /*!
      Defines the ways the width of the bar can be specified. Thus it defines what the number passed
      to \ref setWidth actually means.

      \see setWidthType, setWidth
    */
    enum WidthType
    {
        wtAbsolute ///< Bar width is in absolute pixels
        ,
        wtAxisRectRatio ///< Bar width is given by a fraction of the axis rect size
        ,
        wtPlotCoords ///< Bar width is in key coordinates and thus scales with the key axis range
    };
    Q_ENUM(WidthType)

    explicit QCPBars(QCPAxis* keyAxis, QCPAxis* valueAxis);
    virtual ~QCPBars() override;

    // getters:
    [[nodiscard]] double width() const { return mWidth; }

    [[nodiscard]] WidthType widthType() const { return mWidthType; }

    [[nodiscard]] QCPBarsGroup* barsGroup() const { return mBarsGroup; }

    [[nodiscard]] double baseValue() const { return mBaseValue; }

    [[nodiscard]] double stackingGap() const { return mStackingGap; }

    [[nodiscard]] QCPBars* barBelow() const { return mBarBelow.data(); }

    [[nodiscard]] QCPBars* barAbove() const { return mBarAbove.data(); }

    [[nodiscard]] QSharedPointer<QCPBarsDataContainer> data() const { return mDataContainer; }

    // setters:
    void setData(QSharedPointer<QCPBarsDataContainer> data);
    void setData(const QVector<double>& keys, const QVector<double>& values,
                 bool alreadySorted = false);
    void setWidth(double width);
    void setWidthType(WidthType widthType);
    void setBarsGroup(QCPBarsGroup* barsGroup);
    void setBaseValue(double baseValue);
    void setStackingGap(double pixels);

    // non-property methods:
    void addData(const QVector<double>& keys, const QVector<double>& values,
                 bool alreadySorted = false);
    void addData(double key, double value);
    void moveBelow(QCPBars* bars);
    void moveAbove(QCPBars* bars);

    // reimplemented virtual methods:
    virtual QCPDataSelection selectTestRect(const QRectF& rect,
                                            bool onlySelectable) const override;
    virtual double selectTest(const QPointF& pos, bool onlySelectable,
                              QVariant* details = nullptr) const override;
    virtual QCPRange getKeyRange(bool& foundRange,
                                 QCP::SignDomain inSignDomain = QCP::sdBoth) const override;
    virtual QCPRange getValueRange(bool& foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth,
                                   const QCPRange& inKeyRange = QCPRange()) const override;
    virtual QPointF dataPixelPosition(int index) const override;

protected:
    // property members:
    double mWidth;
    WidthType mWidthType;
    QCPBarsGroup* mBarsGroup;
    double mBaseValue;
    double mStackingGap;
    QPointer<QCPBars> mBarBelow, mBarAbove;

    // reimplemented virtual methods:
    virtual void draw(QCPPainter* painter) override;
    virtual void drawLegendIcon(QCPPainter* painter, const QRectF& rect) const override;

    // non-virtual methods:
    void getVisibleDataBounds(QCPBarsDataContainer::const_iterator& begin,
                              QCPBarsDataContainer::const_iterator& end) const;
    QRectF getBarRect(double key, double value) const;
    void getPixelWidth(double key, double& lower, double& upper) const;
    double getStackedBaseValue(double key, bool positive) const;
    static void connectBars(QCPBars* lower, QCPBars* upper);

    friend class QCustomPlot;
    friend class QCPLegend;
    friend class QCPBarsGroup;
};
Q_DECLARE_METATYPE(QCPBars::WidthType)

// The fluent handle of a bar plottable; see QCPPlottableOps for the subclass contract.
template <typename T, typename Self>
class QCPBarsOps : public QCPPlottableOps<T, qcp::OpsRet<Self, QCPBarsOps<T>>>
{
    using Base = QCPPlottableOps<T, qcp::OpsRet<Self, QCPBarsOps<T>>>;

public:
    using typename Base::Ret;
    using Base::Base;

    Ret setData(QSharedPointer<QCPBarsDataContainer> data)
    {
        return this->apply([&](T& p) { p.setData(std::move(data)); });
    }
    Ret setData(const QVector<double>& keys, const QVector<double>& values, bool alreadySorted = false)
    {
        return this->apply([&](T& p) { p.setData(keys, values, alreadySorted); });
    }
    Ret setWidth(double width)
    {
        return this->apply([&](T& p) { p.setWidth(width); });
    }
    Ret setWidthType(QCPBars::WidthType type)
    {
        return this->apply([&](T& p) { p.setWidthType(type); });
    }
    Ret setBaseValue(double baseValue)
    {
        return this->apply([&](T& p) { p.setBaseValue(baseValue); });
    }
    Ret setStackingGap(double pixels)
    {
        return this->apply([&](T& p) { p.setStackingGap(pixels); });
    }

    // A dead group handle is refused with one misuse; use with() and a null group to ungroup.
    Ret setBarsGroup(const QCPHandle<QCPBarsGroup>& group)
    {
        return this->apply(
            [&](T& p)
            {
                QCP_CHECK_OR_RETURN(group, "the bars group handle is dead");
                p.setBarsGroup(group.get());
            });
    }
};

// The handle of a bars group: created by QCustomPlot::add<QCPBarsGroup>(), owned by the plot.
template <typename T, typename Self>
class QCPBarsGroupOps : public QCPObjectOps<T, qcp::OpsRet<Self, QCPBarsGroupOps<T>>>
{
    using Base = QCPObjectOps<T, qcp::OpsRet<Self, QCPBarsGroupOps<T>>>;

public:
    using typename Base::Ret;
    using Base::Base;

    Ret setSpacingType(QCPBarsGroup::SpacingType type)
    {
        return this->apply([&](T& p) { p.setSpacingType(type); });
    }
    Ret setSpacing(double spacing)
    {
        return this->apply([&](T& p) { p.setSpacing(spacing); });
    }
};

#endif // QCP_PLOTTABLE_BARS_H
