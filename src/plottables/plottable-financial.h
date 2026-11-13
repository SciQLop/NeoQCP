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
#ifndef QCP_PLOTTABLE_FINANCIAL_H
#define QCP_PLOTTABLE_FINANCIAL_H

#include "../axis/range.h"
#include "../datacontainer.h"
#include "../global.h"
#include "../painting/painter.h"
#include "../plottables/plottable1d.h"

class QCPPainter;
class QCPAxis;

class QCP_LIB_DECL QCPFinancialData
{
public:
    QCPFinancialData();
    QCPFinancialData(double key, double open, double high, double low, double close);

    inline double sortKey() const { return key; }

    inline static QCPFinancialData fromSortKey(double sortKey)
    {
        return QCPFinancialData(sortKey, 0, 0, 0, 0);
    }

    inline static bool sortKeyIsMainKey() { return true; }

    inline double mainKey() const { return key; }

    inline double mainValue() const { return open; }

    inline QCPRange valueRange() const
    {
        return QCPRange(low, high);
    } // open and close must lie between low and high, so we don't need to check them

    double key, open, high, low, close;
};

Q_DECLARE_TYPEINFO(QCPFinancialData, Q_PRIMITIVE_TYPE);


/*! \typedef QCPFinancialDataContainer

  Container for storing \ref QCPFinancialData points. The data is stored sorted by \a key.

  This template instantiation is the container in which QCPFinancial holds its data. For details
  about the generic container, see the documentation of the class template \ref QCPDataContainer.

  \see QCPFinancialData, QCPFinancial::setData
*/
using QCPFinancialDataContainer = QCPDataContainer<QCPFinancialData>;

template <typename T, typename Self = void>
class QCPFinancialOps;

class QCP_LIB_DECL QCPFinancial : public QCPAbstractPlottable1D<QCPFinancialData>
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(ChartStyle chartStyle READ chartStyle WRITE setChartStyle)
    Q_PROPERTY(double width READ width WRITE setWidth)
    Q_PROPERTY(WidthType widthType READ widthType WRITE setWidthType)
    Q_PROPERTY(bool twoColored READ twoColored WRITE setTwoColored)
    Q_PROPERTY(QBrush brushPositive READ brushPositive WRITE setBrushPositive)
    Q_PROPERTY(QBrush brushNegative READ brushNegative WRITE setBrushNegative)
    Q_PROPERTY(QPen penPositive READ penPositive WRITE setPenPositive)
    Q_PROPERTY(QPen penNegative READ penNegative WRITE setPenNegative)
    /// \endcond

public:
    template <typename T>
    using OpsFor = QCPFinancialOps<T>;

    /*!
      Defines the ways the width of the financial bar can be specified. Thus it defines what the
      number passed to \ref setWidth actually means.

      \see setWidthType, setWidth
    */
    enum WidthType
    {
        wtAbsolute ///< width is in absolute pixels
        ,
        wtAxisRectRatio ///< width is given by a fraction of the axis rect size
        ,
        wtPlotCoords ///< width is in key coordinates and thus scales with the key axis range
    };
    Q_ENUM(WidthType)

    /*!
      Defines the possible representations of OHLC data in the plot.

      \see setChartStyle
    */
    enum ChartStyle
    {
        csOhlc ///< Open-High-Low-Close bar representation
        ,
        csCandlestick ///< Candlestick representation
    };
    Q_ENUM(ChartStyle)

    explicit QCPFinancial(QCPAxis* keyAxis, QCPAxis* valueAxis);
    virtual ~QCPFinancial() override;

    // getters:
    [[nodiscard]] QSharedPointer<QCPFinancialDataContainer> data() const { return mDataContainer; }

    [[nodiscard]] ChartStyle chartStyle() const { return mChartStyle; }

    [[nodiscard]] double width() const { return mWidth; }

    [[nodiscard]] WidthType widthType() const { return mWidthType; }

    [[nodiscard]] bool twoColored() const { return mTwoColored; }

    [[nodiscard]] QBrush brushPositive() const { return mBrushPositive; }

    [[nodiscard]] QBrush brushNegative() const { return mBrushNegative; }

    [[nodiscard]] QPen penPositive() const { return mPenPositive; }

    [[nodiscard]] QPen penNegative() const { return mPenNegative; }

    // setters:
    void setData(QSharedPointer<QCPFinancialDataContainer> data);
    void setData(const QVector<double>& keys, const QVector<double>& open,
                 const QVector<double>& high, const QVector<double>& low,
                 const QVector<double>& close, bool alreadySorted = false);
    void setChartStyle(ChartStyle style);
    void setWidth(double width);
    void setWidthType(WidthType widthType);
    void setTwoColored(bool twoColored);
    void setBrushPositive(const QBrush& brush);
    void setBrushNegative(const QBrush& brush);
    void setPenPositive(const QPen& pen);
    void setPenNegative(const QPen& pen);

    // non-property methods:
    void addData(const QVector<double>& keys, const QVector<double>& open,
                 const QVector<double>& high, const QVector<double>& low,
                 const QVector<double>& close, bool alreadySorted = false);
    void addData(double key, double open, double high, double low, double close);

    // reimplemented virtual methods:
    virtual QCPDataSelection selectTestRect(const QRectF& rect,
                                            bool onlySelectable) const override;
    virtual double selectTest(const QPointF& pos, bool onlySelectable,
                              QVariant* details = nullptr) const override;
    virtual QCPRange getKeyRange(bool& foundRange,
                                 QCP::SignDomain inSignDomain = QCP::sdBoth) const override;
    virtual QCPRange getValueRange(bool& foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth,
                                   const QCPRange& inKeyRange = QCPRange()) const override;

    // static methods:
    static QCPFinancialDataContainer timeSeriesToOhlc(const QVector<double>& time,
                                                      const QVector<double>& value,
                                                      double timeBinSize, double timeBinOffset = 0);

protected:
    // property members:
    ChartStyle mChartStyle;
    double mWidth;
    WidthType mWidthType;
    bool mTwoColored;
    QBrush mBrushPositive, mBrushNegative;
    QPen mPenPositive, mPenNegative;

    // reimplemented virtual methods:
    void draw(QCPDrawContext& context) const override;
    void drawLegendIcon(QCPDrawContext& icon) const override;
    void paintLegendIcon(QCPPainter* painter, const QRectF& rect) const;

    // non-virtual methods:
    void recordSegment(QCPDrawContext& context, const QCPDataRange& segment, bool selected) const;
    double getPixelWidth(double key, double keyPixel) const;
    double ohlcSelectTest(const QPointF& pos,
                          const QCPFinancialDataContainer::const_iterator& begin,
                          const QCPFinancialDataContainer::const_iterator& end,
                          QCPFinancialDataContainer::const_iterator& closestDataPoint) const;
    double candlestickSelectTest(const QPointF& pos,
                                 const QCPFinancialDataContainer::const_iterator& begin,
                                 const QCPFinancialDataContainer::const_iterator& end,
                                 QCPFinancialDataContainer::const_iterator& closestDataPoint) const;
    void getVisibleDataBounds(QCPFinancialDataContainer::const_iterator& begin,
                              QCPFinancialDataContainer::const_iterator& end) const;
    QRectF selectionHitBox(QCPFinancialDataContainer::const_iterator it) const;

    friend class QCustomPlot;
    friend class QCPLegend;
};
Q_DECLARE_METATYPE(QCPFinancial::ChartStyle)

// The fluent handle of a financial plottable; see QCPPlottableOps for the subclass contract.
template <typename T, typename Self>
class QCPFinancialOps : public QCPPlottableOps<T, qcp::OpsRet<Self, QCPFinancialOps<T>>>
{
    using Base = QCPPlottableOps<T, qcp::OpsRet<Self, QCPFinancialOps<T>>>;

public:
    using typename Base::Ret;
    using Base::Base;

    Ret setData(QSharedPointer<QCPFinancialDataContainer> data)
    {
        return this->apply([&](T& p) { p.setData(std::move(data)); });
    }
    Ret setData(const QVector<double>& keys, const QVector<double>& open, const QVector<double>& high,
            const QVector<double>& low, const QVector<double>& close, bool alreadySorted = false)
    {
        return this->apply([&](T& p) { p.setData(keys, open, high, low, close, alreadySorted); });
    }
    Ret setChartStyle(QCPFinancial::ChartStyle style)
    {
        return this->apply([&](T& p) { p.setChartStyle(style); });
    }
    Ret setWidth(double width)
    {
        return this->apply([&](T& p) { p.setWidth(width); });
    }
    Ret setWidthType(QCPFinancial::WidthType type)
    {
        return this->apply([&](T& p) { p.setWidthType(type); });
    }
    Ret setTwoColored(bool twoColored)
    {
        return this->apply([&](T& p) { p.setTwoColored(twoColored); });
    }
    Ret setBrushPositive(const QBrush& brush)
    {
        return this->apply([&](T& p) { p.setBrushPositive(brush); });
    }
    Ret setBrushNegative(const QBrush& brush)
    {
        return this->apply([&](T& p) { p.setBrushNegative(brush); });
    }
    Ret setPenPositive(const QPen& pen)
    {
        return this->apply([&](T& p) { p.setPenPositive(pen); });
    }
    Ret setPenNegative(const QPen& pen)
    {
        return this->apply([&](T& p) { p.setPenNegative(pen); });
    }
};

#endif // QCP_PLOTTABLE_FINANCIAL_H
