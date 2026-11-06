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

#ifndef QCP_RADIALAXIS_H
#define QCP_RADIALAXIS_H

#include "../axis/axisticker.h"
#include "../axis/labelpainter.h"
#include "../axis/range.h"
#include "../global.h"
#include "../handle-ops.h"
#include "../layout.h"

class QCPPainter;
class QCustomPlot;
class QCPPolarAxisRadial;
class QCPPolarAxisAngular;

template <typename T, typename Self = void>
class QCPPolarAxisRadialOps;

class QCP_LIB_DECL QCPPolarAxisRadial : public QCPLayerable
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES

    /// \endcond

public:
    /*!
      Defines the reference of the angle at which a radial axis is tilted (\ref setAngle).
    */
    enum AngleReference
    {
        arAbsolute ///< The axis tilt is given in absolute degrees. The zero is to the right and
                   ///< positive angles are measured clockwise (screen y points down).
        ,
        arAngularAxis ///< The axis tilt is measured in the angular coordinate system given by the
                      ///< parent angular axis.
    };
    Q_ENUM(AngleReference)

    /*!
      Defines the scale of an axis.
      \see setScaleType
    */
    enum ScaleType
    {
        stLinear ///< Linear scaling
        ,
        stLogarithmic ///< Logarithmic scaling with correspondingly transformed axis coordinates
                      ///< (possibly also \ref setTicker to a \ref QCPAxisTickerLog instance).
    };
    Q_ENUM(ScaleType)

    /*!
      Defines the selectable parts of an axis.
      \see setSelectableParts, setSelectedParts
    */
    enum SelectablePart
    {
        spNone = 0 ///< None of the selectable parts
        ,
        spAxis = 0x001 ///< The axis backbone and tick marks
        ,
        spTickLabels = 0x002 ///< Tick labels (numbers) of this axis (as a whole, not individually)
        ,
        spAxisLabel = 0x004 ///< The axis label
    };
    Q_ENUM(SelectablePart)
    Q_DECLARE_FLAGS(SelectableParts, SelectablePart)
    Q_FLAG(SelectableParts)

    enum LabelMode
    {
        lmUpright ///<
        ,
        lmRotated ///<
    };
    Q_ENUM(LabelMode)

    template <typename T>
    using OpsFor = QCPPolarAxisRadialOps<T>;

    explicit QCPPolarAxisRadial(QCPPolarAxisAngular* parent);
    virtual ~QCPPolarAxisRadial();

    // getters:
    bool rangeDrag() const { return mRangeDrag; }

    bool rangeZoom() const { return mRangeZoom; }

    double rangeZoomFactor() const { return mRangeZoomFactor; }

    QCPPolarAxisAngular* angularAxis() const { return mAngularAxis; }

    ScaleType scaleType() const { return mScaleType; }

    const QCPRange range() const { return mRange; }

    bool rangeReversed() const { return mRangeReversed; }

    double angle() const { return mAngle; }

    AngleReference angleReference() const { return mAngleReference; }

    QSharedPointer<QCPAxisTicker> ticker() const { return mTicker; }

    bool ticks() const { return mTicks; }

    bool tickLabels() const { return mTickLabels; }

    int tickLabelPadding() const { return mLabelPainter.padding(); }

    QFont tickLabelFont() const { return mTickLabelFont; }

    QColor tickLabelColor() const { return mTickLabelColor; }

    double tickLabelRotation() const { return mLabelPainter.rotation(); }

    LabelMode tickLabelMode() const;
    QString numberFormat() const;

    int numberPrecision() const { return mNumberPrecision; }

    QVector<double> tickVector() const { return mTickVector; }

    QVector<double> subTickVector() const { return mSubTickVector; }

    QVector<QString> tickVectorLabels() const { return mTickVectorLabels; }

    int tickLengthIn() const;
    int tickLengthOut() const;

    bool subTicks() const { return mSubTicks; }

    int subTickLengthIn() const;
    int subTickLengthOut() const;

    QPen basePen() const { return mBasePen; }

    QPen tickPen() const { return mTickPen; }

    QPen subTickPen() const { return mSubTickPen; }

    QFont labelFont() const { return mLabelFont; }

    QColor labelColor() const { return mLabelColor; }

    QString label() const { return mLabel; }

    int labelPadding() const;

    SelectableParts selectedParts() const { return mSelectedParts; }

    SelectableParts selectableParts() const { return mSelectableParts; }

    bool selected() const { return mSelectedParts != spNone; }

    QCP::SelectionType selectable() const
    {
        return mSelectableParts != spNone ? QCP::stWhole : QCP::stNone;
    }

    QFont selectedTickLabelFont() const { return mSelectedTickLabelFont; }

    QFont selectedLabelFont() const { return mSelectedLabelFont; }

    QColor selectedTickLabelColor() const { return mSelectedTickLabelColor; }

    QColor selectedLabelColor() const { return mSelectedLabelColor; }

    QPen selectedBasePen() const { return mSelectedBasePen; }

    QPen selectedTickPen() const { return mSelectedTickPen; }

    QPen selectedSubTickPen() const { return mSelectedSubTickPen; }

    // setters:
    void setRangeDrag(bool enabled);
    void setRangeZoom(bool enabled);
    void setRangeZoomFactor(double factor);

    Q_SLOT void setScaleType(QCPPolarAxisRadial::ScaleType type);
    Q_SLOT void setRange(const QCPRange& range);
    void setRange(double lower, double upper);
    void setRange(double position, double size, Qt::AlignmentFlag alignment);
    void setRangeLower(double lower);
    void setRangeUpper(double upper);
    void setRangeReversed(bool reversed);
    void setAngle(double degrees);
    void setAngleReference(AngleReference reference);
    void setTicker(QSharedPointer<QCPAxisTicker> ticker);
    void setTicks(bool show);
    void setTickLabels(bool show);
    void setTickLabelPadding(int padding);
    void setTickLabelFont(const QFont& font);
    void setTickLabelColor(const QColor& color);
    void setTickLabelRotation(double degrees);
    void setTickLabelMode(LabelMode mode);
    void setNumberFormat(const QString& formatCode);
    void setNumberPrecision(int precision);
    void setTickLength(int inside, int outside = 0);
    void setTickLengthIn(int inside);
    void setTickLengthOut(int outside);
    void setSubTicks(bool show);
    void setSubTickLength(int inside, int outside = 0);
    void setSubTickLengthIn(int inside);
    void setSubTickLengthOut(int outside);
    void setBasePen(const QPen& pen);
    void setTickPen(const QPen& pen);
    void setSubTickPen(const QPen& pen);
    void setLabelFont(const QFont& font);
    void setLabelColor(const QColor& color);
    void setLabel(const QString& str);
    void setLabelPadding(int padding);
    void setSelectedTickLabelFont(const QFont& font);
    void setSelectedLabelFont(const QFont& font);
    void setSelectedTickLabelColor(const QColor& color);
    void setSelectedLabelColor(const QColor& color);
    void setSelectedBasePen(const QPen& pen);
    void setSelectedTickPen(const QPen& pen);
    void setSelectedSubTickPen(const QPen& pen);
    Q_SLOT void setSelectableParts(const QCPPolarAxisRadial::SelectableParts& selectableParts);
    Q_SLOT void setSelectedParts(const QCPPolarAxisRadial::SelectableParts& selectedParts);

    // reimplemented virtual methods:
    virtual double selectTest(const QPointF& pos, bool onlySelectable,
                              QVariant* details = 0) const override;

    // non-property methods:
    void moveRange(double diff);
    void scaleRange(double factor);
    void scaleRange(double factor, double center);
    void rescale(bool onlyVisiblePlottables = false);
    void pixelToCoord(QPointF pixelPos, double& angleCoord, double& radiusCoord) const;
    QPointF coordToPixel(double angleCoord, double radiusCoord) const;
    double coordToRadius(double coord) const;
    double radiusToCoord(double radius) const;
    SelectablePart getPartAt(const QPointF& pos) const;

signals:
    void rangeChanged(const QCPRange& newRange, const QCPRange& oldRange);
    void scaleTypeChanged(QCPPolarAxisRadial::ScaleType scaleType);
    void selectionChanged();
    void selectableChanged();

protected:
    // property members:
    bool mRangeDrag;
    bool mRangeZoom;
    double mRangeZoomFactor;

    // axis base:
    QCPPolarAxisAngular* mAngularAxis;
    double mAngle;
    AngleReference mAngleReference;
    SelectableParts mSelectableParts, mSelectedParts;
    QPen mBasePen, mSelectedBasePen;
    // axis label:
    int mLabelPadding;
    QString mLabel;
    QFont mLabelFont, mSelectedLabelFont;
    QColor mLabelColor, mSelectedLabelColor;
    // tick labels:
    // int mTickLabelPadding; in label painter
    bool mTickLabels;
    // double mTickLabelRotation; in label painter
    QFont mTickLabelFont, mSelectedTickLabelFont;
    QColor mTickLabelColor, mSelectedTickLabelColor;
    int mNumberPrecision;
    QLatin1Char mNumberFormatChar;
    bool mNumberBeautifulPowers;
    bool mNumberMultiplyCross;
    // ticks and subticks:
    bool mTicks;
    bool mSubTicks;
    int mTickLengthIn, mTickLengthOut, mSubTickLengthIn, mSubTickLengthOut;
    QPen mTickPen, mSelectedTickPen;
    QPen mSubTickPen, mSelectedSubTickPen;
    // scale and range:
    QCPRange mRange;
    bool mRangeReversed;
    ScaleType mScaleType;

    // non-property members:
    QPointF mCenter;
    double mRadius;
    QSharedPointer<QCPAxisTicker> mTicker;
    QVector<double> mTickVector;
    QVector<QString> mTickVectorLabels;
    QVector<double> mSubTickVector;
    bool mDragging;
    QCPRange mDragStartRange;
    QCP::AntialiasedElements mAADragBackup, mNotAADragBackup;
    QCPLabelPainterPrivate mLabelPainter;

    // reimplemented virtual methods:
    virtual void applyDefaultAntialiasingHint(QCPPainter* painter) const override;
    virtual void draw(QCPPainter* painter) override;
    virtual QCP::Interaction selectionCategory() const override;
    // events:
    virtual void selectEvent(QMouseEvent* event, bool additive, const QVariant& details,
                             bool* selectionStateChanged) override;
    virtual void deselectEvent(bool* selectionStateChanged) override;
    // mouse events:
    virtual void mousePressEvent(QMouseEvent* event, const QVariant& details) override;
    virtual void mouseMoveEvent(QMouseEvent* event, const QPointF& startPos) override;
    virtual void mouseReleaseEvent(QMouseEvent* event, const QPointF& startPos) override;
    virtual void wheelEvent(QWheelEvent* event) override;

    // non-virtual methods:
    void updateGeometry(const QPointF& center, double radius);
    void setupTickVectors();
    QPen getBasePen() const;
    QPen getTickPen() const;
    QPen getSubTickPen() const;
    QFont getTickLabelFont() const;
    QFont getLabelFont() const;
    QColor getTickLabelColor() const;
    QColor getLabelColor() const;

private:
    Q_DISABLE_COPY(QCPPolarAxisRadial)
    void emitRangeChanged(const QCPRange& oldRange);

    friend class QCustomPlot;
    friend class QCPPolarAxisAngular;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QCPPolarAxisRadial::SelectableParts)
Q_DECLARE_METATYPE(QCPPolarAxisRadial::AngleReference)
Q_DECLARE_METATYPE(QCPPolarAxisRadial::ScaleType)
Q_DECLARE_METATYPE(QCPPolarAxisRadial::SelectablePart)


// The handle of QCPPolarAxisRadial: what add<T>() returns for it.
template <typename T, typename Self>
class QCPPolarAxisRadialOps : public QCPLayerableOps<T, qcp::OpsRet<Self, QCPPolarAxisRadialOps<T>>>
{
    using Base = QCPLayerableOps<T, qcp::OpsRet<Self, QCPPolarAxisRadialOps<T>>>;

public:
    using typename Base::Ret;
    using Base::Base;

    Ret setScaleType(QCPPolarAxisRadial::ScaleType type)
    {
        return this->apply([&](T& o) { o.setScaleType(type); });
    }
    Ret setAngleReference(QCPPolarAxisRadial::AngleReference reference)
    {
        return this->apply([&](T& o) { o.setAngleReference(reference); });
    }
    Ret setRange(double lower, double upper)
    {
        return this->apply([&](T& o) { o.setRange(lower, upper); });
    }
    Ret setRangeLower(double lower)
    {
        return this->apply([&](T& o) { o.setRangeLower(lower); });
    }
    Ret setRangeUpper(double upper)
    {
        return this->apply([&](T& o) { o.setRangeUpper(upper); });
    }
    Ret setRangeReversed(bool reversed)
    {
        return this->apply([&](T& o) { o.setRangeReversed(reversed); });
    }
    Ret setAngle(double degrees)
    {
        return this->apply([&](T& o) { o.setAngle(degrees); });
    }
    Ret setTicks(bool show)
    {
        return this->apply([&](T& o) { o.setTicks(show); });
    }
    Ret setTickLabels(bool show)
    {
        return this->apply([&](T& o) { o.setTickLabels(show); });
    }
    Ret setTickLabelPadding(int padding)
    {
        return this->apply([&](T& o) { o.setTickLabelPadding(padding); });
    }
    Ret setTickLabelFont(const QFont& font)
    {
        return this->apply([&](T& o) { o.setTickLabelFont(font); });
    }
    Ret setTickLabelColor(const QColor& color)
    {
        return this->apply([&](T& o) { o.setTickLabelColor(color); });
    }
    Ret setTickLabelRotation(double degrees)
    {
        return this->apply([&](T& o) { o.setTickLabelRotation(degrees); });
    }
    Ret setRangeZoom(bool enabled)
    {
        return this->apply([&](T& o) { o.setRangeZoom(enabled); });
    }
    Ret setRangeZoomFactor(double factor)
    {
        return this->apply([&](T& o) { o.setRangeZoomFactor(factor); });
    }
    Ret setNumberFormat(const QString& formatCode)
    {
        return this->apply([&](T& o) { o.setNumberFormat(formatCode); });
    }
    Ret setNumberPrecision(int precision)
    {
        return this->apply([&](T& o) { o.setNumberPrecision(precision); });
    }
    Ret setTickLength(int inside, int outside = 0)
    {
        return this->apply([&](T& o) { o.setTickLength(inside, outside); });
    }
    Ret setSubTicks(bool show)
    {
        return this->apply([&](T& o) { o.setSubTicks(show); });
    }
    Ret setSubTickLength(int inside, int outside = 0)
    {
        return this->apply([&](T& o) { o.setSubTickLength(inside, outside); });
    }
    Ret setBasePen(const QPen& pen)
    {
        return this->apply([&](T& o) { o.setBasePen(pen); });
    }
    Ret setTickPen(const QPen& pen)
    {
        return this->apply([&](T& o) { o.setTickPen(pen); });
    }
    Ret setSubTickPen(const QPen& pen)
    {
        return this->apply([&](T& o) { o.setSubTickPen(pen); });
    }
    Ret setLabelFont(const QFont& font)
    {
        return this->apply([&](T& o) { o.setLabelFont(font); });
    }
    Ret setLabelColor(const QColor& color)
    {
        return this->apply([&](T& o) { o.setLabelColor(color); });
    }
    Ret setLabel(const QString& str)
    {
        return this->apply([&](T& o) { o.setLabel(str); });
    }
    Ret setLabelPadding(int padding)
    {
        return this->apply([&](T& o) { o.setLabelPadding(padding); });
    }
};

#endif // QCP_RADIALAXIS_H
