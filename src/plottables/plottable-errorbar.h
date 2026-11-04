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
#ifndef QCP_PLOTTABLE_ERRORBAR_H
#define QCP_PLOTTABLE_ERRORBAR_H

#include "../axis/range.h"
#include "../global.h"
#include "../painting/painter.h"
#include "plottable1d.h"

class QCPPainter;
class QCPAxis;

class QCP_LIB_DECL QCPErrorBarsData
{
public:
    QCPErrorBarsData();
    explicit QCPErrorBarsData(double error);
    QCPErrorBarsData(double errorMinus, double errorPlus);

    double errorMinus, errorPlus;
};

Q_DECLARE_TYPEINFO(QCPErrorBarsData, Q_PRIMITIVE_TYPE);


/*! \typedef QCPErrorBarsDataContainer

  Container for storing \ref QCPErrorBarsData points. It is a typedef for <tt>QVector<\ref
  QCPErrorBarsData></tt>.

  This is the container in which \ref QCPErrorBars holds its data. Unlike most other data
  containers for plottables, it is not based on \ref QCPDataContainer. This is because the error
  bars plottable is special in that it doesn't store its own key and value coordinate per error
  bar. It adopts the key and value from the plottable to which the error bars shall be applied
  (\ref QCPErrorBars::setDataPlottable). So the stored \ref QCPErrorBarsData doesn't need a
  sortable key, but merely an index (as \c QVector provides), which maps one-to-one to the indices
  of the other plottable's data.

  \see QCPErrorBarsData, QCPErrorBars::setData
*/
using QCPErrorBarsDataContainer = QVector<QCPErrorBarsData>;

template <typename T, typename Self = void>
class QCPErrorBarsOps;

class QCP_LIB_DECL QCPErrorBars : public QCPAbstractPlottable, public QCPPlottableInterface1D
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(QSharedPointer<QCPErrorBarsDataContainer> data READ data WRITE setData)
    Q_PROPERTY(QCPAbstractPlottable* dataPlottable READ dataPlottable WRITE setDataPlottable)
    Q_PROPERTY(ErrorType errorType READ errorType WRITE setErrorType)
    Q_PROPERTY(double whiskerWidth READ whiskerWidth WRITE setWhiskerWidth)
    Q_PROPERTY(double symbolGap READ symbolGap WRITE setSymbolGap)
    /// \endcond

public:
    template <typename T>
    using OpsFor = QCPErrorBarsOps<T>;

    /*!
      Defines in which orientation the error bars shall appear. If your data needs both error
      dimensions, create two \ref QCPErrorBars with different \ref ErrorType.

      \see setErrorType
    */
    enum ErrorType
    {
        etKeyError ///< The errors are for the key dimension (bars appear parallel to the key axis)
        ,
        etValueError ///< The errors are for the value dimension (bars appear parallel to the value
                     ///< axis)
    };
    Q_ENUM(ErrorType)

    explicit QCPErrorBars(QCPAxis* keyAxis, QCPAxis* valueAxis);
    virtual ~QCPErrorBars() override;

    // getters:
    [[nodiscard]] QSharedPointer<QCPErrorBarsDataContainer> data() const { return mDataContainer; }

    [[nodiscard]] QCPAbstractPlottable* dataPlottable() const { return mDataPlottable.data(); }

    [[nodiscard]] ErrorType errorType() const { return mErrorType; }

    [[nodiscard]] double whiskerWidth() const { return mWhiskerWidth; }

    [[nodiscard]] double symbolGap() const { return mSymbolGap; }

    // setters:
    void setData(QSharedPointer<QCPErrorBarsDataContainer> data);
    void setData(const QVector<double>& error);
    void setData(const QVector<double>& errorMinus, const QVector<double>& errorPlus);
    void setDataPlottable(QCPAbstractPlottable* plottable);
    void setErrorType(ErrorType type);
    void setWhiskerWidth(double pixels);
    void setSymbolGap(double pixels);

    // non-property methods:
    void addData(const QVector<double>& error);
    void addData(const QVector<double>& errorMinus, const QVector<double>& errorPlus);
    void addData(double error);
    void addData(double errorMinus, double errorPlus);

    // virtual methods of 1d plottable interface:
    virtual int dataCount() const override;
    virtual double dataMainKey(int index) const override;
    virtual double dataSortKey(int index) const override;
    virtual double dataMainValue(int index) const override;
    virtual QCPRange dataValueRange(int index) const override;
    virtual QPointF dataPixelPosition(int index) const override;
    virtual bool sortKeyIsMainKey() const override;
    virtual QCPDataSelection selectTestRect(const QRectF& rect,
                                            bool onlySelectable) const override;
    virtual int findBegin(double sortKey, bool expandedRange = true) const override;
    virtual int findEnd(double sortKey, bool expandedRange = true) const override;

    // reimplemented virtual methods:
    virtual double selectTest(const QPointF& pos, bool onlySelectable,
                              QVariant* details = nullptr) const override;

    virtual QCPPlottableInterface1D* interface1D() override { return this; }

protected:
    // property members:
    QSharedPointer<QCPErrorBarsDataContainer> mDataContainer;
    QPointer<QCPAbstractPlottable> mDataPlottable;
    ErrorType mErrorType;
    double mWhiskerWidth;
    double mSymbolGap;

    // reimplemented virtual methods:
    virtual void draw(QCPPainter* painter) override;
    virtual void drawLegendIcon(QCPPainter* painter, const QRectF& rect) const override;
    virtual QCPRange getKeyRange(bool& foundRange,
                                 QCP::SignDomain inSignDomain = QCP::sdBoth) const override;
    virtual QCPRange getValueRange(bool& foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth,
                                   const QCPRange& inKeyRange = QCPRange()) const override;

    // non-virtual methods:
    void getErrorBarLines(QCPErrorBarsDataContainer::const_iterator it, QVector<QLineF>& backbones,
                          QVector<QLineF>& whiskers) const;
    void getVisibleDataBounds(QCPErrorBarsDataContainer::const_iterator& begin,
                              QCPErrorBarsDataContainer::const_iterator& end,
                              const QCPDataRange& rangeRestriction) const;
    double pointDistance(const QPointF& pixelPoint,
                         QCPErrorBarsDataContainer::const_iterator& closestData) const;
    // helpers:
    void getDataSegments(QList<QCPDataRange>& selectedSegments,
                         QList<QCPDataRange>& unselectedSegments) const;
    bool errorBarVisible(int index) const;
    bool rectIntersectsLine(const QRectF& pixelRect, const QLineF& line) const;

    friend class QCustomPlot;
    friend class QCPLegend;
};

// The fluent handle of a QCPErrorBars; see QCPPlottableOps for the subclass contract.
template <typename T, typename Self>
class QCPErrorBarsOps : public QCPPlottableOps<T, qcp::OpsRet<Self, QCPErrorBarsOps<T>>>
{
    using Base = QCPPlottableOps<T, qcp::OpsRet<Self, QCPErrorBarsOps<T>>>;

public:
    using typename Base::Ret;
    using Base::Base;

    // The plottable whose points the bars belong to; a dead handle is refused with one misuse.
    template <typename P>
    Ret setDataPlottable(const QCPHandle<P>& plottable)
    {
        return this->apply(
            [&](T& o)
            {
                QCP_CHECK_OR_RETURN(plottable, "the data plottable handle is dead");
                o.setDataPlottable(plottable.get());
            });
    }

    Ret setErrorType(QCPErrorBars::ErrorType type)
    {
        return this->apply([&](T& o) { o.setErrorType(type); });
    }

    Ret setWhiskerWidth(double pixels)
    {
        return this->apply([&](T& o) { o.setWhiskerWidth(pixels); });
    }

    Ret setSymbolGap(double pixels)
    {
        return this->apply([&](T& o) { o.setSymbolGap(pixels); });
    }
};

#endif // QCP_PLOTTABLE_ERRORBAR_H
