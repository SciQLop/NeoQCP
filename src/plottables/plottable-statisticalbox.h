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
#ifndef QCP_PLOTTABLE_STATISTICALBOX_H
#define QCP_PLOTTABLE_STATISTICALBOX_H

#include "../axis/range.h"
#include "../datacontainer.h"
#include "../global.h"
#include "../painting/painter.h"
#include "../plottables/plottable1d.h"
#include <vector>

class QCPPainter;
class QCPAxis;

class QCP_LIB_DECL QCPStatisticalBoxData
{
public:
    QCPStatisticalBoxData();
    QCPStatisticalBoxData(double key, double minimum, double lowerQuartile, double median,
                          double upperQuartile, double maximum,
                          const QVector<double>& outliers = QVector<double>());

    inline double sortKey() const { return key; }

    inline static QCPStatisticalBoxData fromSortKey(double sortKey)
    {
        return QCPStatisticalBoxData(sortKey, 0, 0, 0, 0, 0);
    }

    inline static bool sortKeyIsMainKey() { return true; }

    inline double mainKey() const { return key; }

    inline double mainValue() const { return median; }

    inline QCPRange valueRange() const
    {
        QCPRange result(minimum, maximum);
        for (double outlier : outliers)
            result.expand(outlier);
        return result;
    }

    double key, minimum, lowerQuartile, median, upperQuartile, maximum;
    QVector<double> outliers;
};

Q_DECLARE_TYPEINFO(QCPStatisticalBoxData, Q_MOVABLE_TYPE);


/*! \typedef QCPStatisticalBoxDataContainer

  Container for storing \ref QCPStatisticalBoxData points. The data is stored sorted by \a key.

  This template instantiation is the container in which QCPStatisticalBox holds its data. For
  details about the generic container, see the documentation of the class template \ref
  QCPDataContainer.

  \see QCPStatisticalBoxData, QCPStatisticalBox::setData
*/
using QCPStatisticalBoxDataContainer = QCPDataContainer<QCPStatisticalBoxData>;

template <typename T, typename Self = void>
class QCPStatisticalBoxOps;

class QCP_LIB_DECL QCPStatisticalBox : public QCPAbstractPlottable1D<QCPStatisticalBoxData>
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(double width READ width WRITE setWidth)
    Q_PROPERTY(double whiskerWidth READ whiskerWidth WRITE setWhiskerWidth)
    Q_PROPERTY(QPen whiskerPen READ whiskerPen WRITE setWhiskerPen)
    Q_PROPERTY(QPen whiskerBarPen READ whiskerBarPen WRITE setWhiskerBarPen)
    Q_PROPERTY(bool whiskerAntialiased READ whiskerAntialiased WRITE setWhiskerAntialiased)
    Q_PROPERTY(QPen medianPen READ medianPen WRITE setMedianPen)
    Q_PROPERTY(QCPScatterStyle outlierStyle READ outlierStyle WRITE setOutlierStyle)
    /// \endcond

public:
    template <typename T>
    using OpsFor = QCPStatisticalBoxOps<T>;

    explicit QCPStatisticalBox(QCPAxis* keyAxis, QCPAxis* valueAxis);

    // getters:
    [[nodiscard]] QSharedPointer<QCPStatisticalBoxDataContainer> data() const { return mDataContainer; }

    [[nodiscard]] double width() const { return mWidth; }

    [[nodiscard]] double whiskerWidth() const { return mWhiskerWidth; }

    [[nodiscard]] QPen whiskerPen() const { return mWhiskerPen; }

    [[nodiscard]] QPen whiskerBarPen() const { return mWhiskerBarPen; }

    [[nodiscard]] bool whiskerAntialiased() const { return mWhiskerAntialiased; }

    [[nodiscard]] QPen medianPen() const { return mMedianPen; }

    [[nodiscard]] QCPScatterStyle outlierStyle() const { return mOutlierStyle; }

    // setters:
    void setData(QSharedPointer<QCPStatisticalBoxDataContainer> data);
    void setData(const QVector<double>& keys, const QVector<double>& minimum,
                 const QVector<double>& lowerQuartile, const QVector<double>& median,
                 const QVector<double>& upperQuartile, const QVector<double>& maximum,
                 bool alreadySorted = false);
    void setWidth(double width);
    void setWhiskerWidth(double width);
    void setWhiskerPen(const QPen& pen);
    void setWhiskerBarPen(const QPen& pen);
    void setWhiskerAntialiased(bool enabled);
    void setMedianPen(const QPen& pen);
    void setOutlierStyle(const QCPScatterStyle& style);

    // non-property methods:
    void addData(const QVector<double>& keys, const QVector<double>& minimum,
                 const QVector<double>& lowerQuartile, const QVector<double>& median,
                 const QVector<double>& upperQuartile, const QVector<double>& maximum,
                 bool alreadySorted = false);
    void addData(double key, double minimum, double lowerQuartile, double median,
                 double upperQuartile, double maximum,
                 const QVector<double>& outliers = QVector<double>());

    // reimplemented virtual methods:
    virtual QCPDataSelection selectTestRect(const QRectF& rect,
                                            bool onlySelectable) const override;
    virtual double selectTest(const QPointF& pos, bool onlySelectable,
                              QVariant* details = nullptr) const override;
    virtual QCPRange getKeyRange(bool& foundRange,
                                 QCP::SignDomain inSignDomain = QCP::sdBoth) const override;
    virtual QCPRange getValueRange(bool& foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth,
                                   const QCPRange& inKeyRange = QCPRange()) const override;

protected:
    // property members:
    double mWidth;
    double mWhiskerWidth;
    QPen mWhiskerPen, mWhiskerBarPen;
    bool mWhiskerAntialiased;
    QPen mMedianPen;
    QCPScatterStyle mOutlierStyle;

    // reimplemented virtual methods:
    void draw(QCPDrawContext& context) const override;
    virtual void drawLegendIcon(QCPPainter* painter, const QRectF& rect) const override;

    // non-virtual methods:
    void recordSegment(QCPDrawContext& context, const QCPDataRange& segment, bool selected) const;
    void getVisibleDataBounds(QCPStatisticalBoxDataContainer::const_iterator& begin,
                              QCPStatisticalBoxDataContainer::const_iterator& end) const;
    QRectF getQuartileBox(QCPStatisticalBoxDataContainer::const_iterator it) const;
    QVector<QLineF>
    getWhiskerBackboneLines(QCPStatisticalBoxDataContainer::const_iterator it) const;

    friend class QCustomPlot;
    friend class QCPLegend;
};

// The fluent handle of a QCPStatisticalBox; see QCPPlottableOps for the subclass contract.
template <typename T, typename Self>
class QCPStatisticalBoxOps : public QCPPlottableOps<T, qcp::OpsRet<Self, QCPStatisticalBoxOps<T>>>
{
    using Base = QCPPlottableOps<T, qcp::OpsRet<Self, QCPStatisticalBoxOps<T>>>;

public:
    using typename Base::Ret;
    using Base::Base;

    Ret setWidth(double width)
    {
        return this->apply([&](T& o) { o.setWidth(width); });
    }

    Ret setWhiskerWidth(double width)
    {
        return this->apply([&](T& o) { o.setWhiskerWidth(width); });
    }

    Ret setWhiskerPen(const QPen& pen)
    {
        return this->apply([&](T& o) { o.setWhiskerPen(pen); });
    }

    Ret setWhiskerBarPen(const QPen& pen)
    {
        return this->apply([&](T& o) { o.setWhiskerBarPen(pen); });
    }

    Ret setWhiskerAntialiased(bool enabled)
    {
        return this->apply([&](T& o) { o.setWhiskerAntialiased(enabled); });
    }

    Ret setMedianPen(const QPen& pen)
    {
        return this->apply([&](T& o) { o.setMedianPen(pen); });
    }

    Ret setOutlierStyle(const QCPScatterStyle& style)
    {
        return this->apply([&](T& o) { o.setOutlierStyle(style); });
    }
};

#endif // QCP_PLOTTABLE_STATISTICALBOX_H
