#ifndef QCP_POLAR_ANGULARAXIS_H
#define QCP_POLAR_ANGULARAXIS_H

#include "../axis/axis-base.h"
#include "polar-area.h"
#include "../axis/axistickerfixed.h"
#include "../axis/labelpainter.h"
#include "../global.h"
#include "../handle-ops.h"
#include "../layer.h"

class QCPPainter;
class QCustomPlot;
class QCPPolarArea;
class QCPPolarAxisRadial;
class QCPPolarGrid;

template <typename T, typename Self = void>
class QCPPolarAxisAngularOps;

/*!
  The angular axis of a polar plot: its range, ticks, labels and drawing, and the angle at which
  its zero coordinate points. It is a child of the \ref QCPPolarArea that lays it out and that owns
  its radial axes and graphs. The range model is the one of \ref QCPAxisBase.
*/
class QCP_LIB_DECL QCPPolarAxisAngular : public QCPAxisBase
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES

    /// \endcond

public:
    /*!
      Defines how the tick labels are turned around the circle.
    */
    enum LabelMode
    {
        lmUpright ///< The labels stay upright
        ,
        lmRotated ///< The labels follow the tick direction
    };
    Q_ENUM(LabelMode)

    template <typename T>
    using OpsFor = QCPPolarAxisAngularOps<T>;

    explicit QCPPolarAxisAngular(QCPPolarArea* area);
    // Out of line: it unlists from the area, whose definition needs this class complete.
    virtual ~QCPPolarAxisAngular() override;

    // getters:
    // Out of line: the covariant return needs QCPPolarArea complete.
    QCPPolarArea* area() const override;

    double angle() const { return mAngle; }

    bool ticks() const { return mTicks; }

    bool tickLabels() const { return mTickLabels; }

    int tickLabelPadding() const { return mLabelPainter.padding(); }

    QFont tickLabelFont() const { return mTickLabelFont; }

    QColor tickLabelColor() const { return mTickLabelColor; }

    double tickLabelRotation() const { return mLabelPainter.rotation(); }

    LabelMode tickLabelMode() const;
    QString numberFormat() const;

    int numberPrecision() const { return mNumberPrecision; }

    int tickLengthIn() const { return mTickLengthIn; }

    int tickLengthOut() const { return mTickLengthOut; }

    bool subTicks() const { return mSubTicks; }

    int subTickLengthIn() const { return mSubTickLengthIn; }

    int subTickLengthOut() const { return mSubTickLengthOut; }

    QPen basePen() const { return mBasePen; }

    QPen tickPen() const { return mTickPen; }

    QPen subTickPen() const { return mSubTickPen; }

    QFont labelFont() const { return mLabelFont; }

    QColor labelColor() const { return mLabelColor; }

    QString label() const { return mLabel; }

    int labelPadding() const { return mLabelPadding; }

    QFont selectedTickLabelFont() const { return mSelectedTickLabelFont; }

    QFont selectedLabelFont() const { return mSelectedLabelFont; }

    QColor selectedTickLabelColor() const { return mSelectedTickLabelColor; }

    QColor selectedLabelColor() const { return mSelectedLabelColor; }

    QPen selectedBasePen() const { return mSelectedBasePen; }

    QPen selectedTickPen() const { return mSelectedTickPen; }

    QPen selectedSubTickPen() const { return mSelectedSubTickPen; }

    // setters:
    void setAngle(double degrees);
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
    void setLabelPosition(Qt::AlignmentFlag position);
    void setSelectedTickLabelFont(const QFont& font);
    void setSelectedLabelFont(const QFont& font);
    void setSelectedTickLabelColor(const QColor& color);
    void setSelectedLabelColor(const QColor& color);
    void setSelectedBasePen(const QPen& pen);
    void setSelectedTickPen(const QPen& pen);
    void setSelectedSubTickPen(const QPen& pen);

    // non-property methods:
    // The turn an angular axis shows is the whole circle: rescaling would cut it, so it keeps its
    // range.
    void rescale(bool onlyVisiblePlottables = false) override;

    double coordToAngleRad(double coord) const
    {
        return mAngleRad
            + (coord - mRange.lower) / mRange.size() * (mRangeReversed ? -2.0 * M_PI : 2.0 * M_PI);
    } // mention in doc that return doesn't wrap

    double angleRadToCoord(double angleRad) const
    {
        return mRange.lower
            + (angleRad - mAngleRad) / (mRangeReversed ? -2.0 * M_PI : 2.0 * M_PI) * mRange.size();
    }

    void pixelToCoord(QPointF pixelPos, double& angleCoord, double& radiusCoord) const;
    QPointF coordToPixel(double angleCoord, double radiusCoord) const;

    // The part of this axis at pos, spNone where there is none. Public as in 2.x.
    SelectablePart getPartAt(const QPointF& pos) const override;

protected:
    // The area as the polar one it always is: the base stores it as a plain QCPArea.
    QCPPolarArea* polarArea() const { return static_cast<QCPPolarArea*>(mArea); }

    // property members:
    double mAngle, mAngleRad;
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

    // non-property members:
    QVector<QPointF> mTickVectorCosSin;
    QVector<QPointF> mSubTickVectorCosSin;
    // The boxes of the tick labels last drawn: what a click on a label lands in.
    QList<QRectF> mTickLabelBoxes;
    QCPLabelPainterPrivate mLabelPainter;

    // reimplemented virtual methods:
    virtual void applyDefaultAntialiasingHint(QCPPainter* painter) const override;
    virtual void draw(QCPPainter* painter) override;
    void markAppearanceDirty() override;
    bool scaleTypeWillChange(ScaleType type) override;
    void rangeDidChange(const QCPRange& oldRange) override;
    bool checkLinkSource(const QCPAxisBase* source) const override;

    // non-virtual methods:
    void setupTickVectors();
    QPen getBasePen() const;
    QPen getTickPen() const;
    QPen getSubTickPen() const;
    QFont getTickLabelFont() const;
    QFont getLabelFont() const;
    QColor getTickLabelColor() const;
    QColor getLabelColor() const;

private:
    Q_DISABLE_COPY(QCPPolarAxisAngular)

    friend class QCustomPlot;
    friend class QCPPolarArea;
    friend class QCPPolarGrid;
};

// The handle of QCPPolarAxisAngular: what add<T>() returns for it.
template <typename T, typename Self>
class QCPPolarAxisAngularOps
        : public QCPLayerableOps<T, qcp::OpsRet<Self, QCPPolarAxisAngularOps<T>>>
{
    using Base = QCPLayerableOps<T, qcp::OpsRet<Self, QCPPolarAxisAngularOps<T>>>;

public:
    using Base::Base;
    using typename Base::Ret;

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

    Ret setNumberFormat(const QString& formatCode)
    {
        return this->apply([&](T& o) { o.setNumberFormat(formatCode); });
    }

    Ret setNumberPrecision(int precision)
    {
        return this->apply([&](T& o) { o.setNumberPrecision(precision); });
    }

    Ret setTickLength(int inside, int outside)
    {
        return this->apply([&](T& o) { o.setTickLength(inside, outside); });
    }

    Ret setSubTicks(bool show)
    {
        return this->apply([&](T& o) { o.setSubTicks(show); });
    }

    Ret setSubTickLength(int inside, int outside)
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

    Ret setRangeDraggable(bool draggable)
    {
        return this->apply([&](T& o) { o.setRangeDraggable(draggable); });
    }

    Ret setRangeZoomable(bool zoomable)
    {
        return this->apply([&](T& o) { o.setRangeZoomable(zoomable); });
    }
};

// The handle is complete with only this header included: the area's angularAxis() handle is usable
// without polargraph.h (a graph needs the polar area's own header).
static_assert(
    requires(qcp::HandleFor<QCPPolarAxisAngular> handle) { handle.setRange(0.0, 1.0); },
    "the angular axis handle must be complete in this header");

#endif // QCP_POLAR_ANGULARAXIS_H
