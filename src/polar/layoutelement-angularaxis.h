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

#ifndef QCP_LAYOUTELEMENT_ANGULARAXIS_H
#define QCP_LAYOUTELEMENT_ANGULARAXIS_H

#include "../axis/axistickerfixed.h"
#include "../axis/labelpainter.h"
#include "../axis/range.h"
#include "../global.h"
#include "../handle-ops.h"
#include "../layout.h"

class QCPPainter;
class QCustomPlot;
class QCPPolarAxisRadial;
class QCPPolarAxisAngular;
class QCPPolarGrid;
class QCPPolarGraph;

template <typename T, typename Self = void>
class QCPPolarAxisAngularOps;

class QCP_LIB_DECL QCPPolarAxisAngular : public QCPLayoutElement
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES

    /// \endcond

public:
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

    /*!
      TODO
    */
    enum LabelMode
    {
        lmUpright ///<
        ,
        lmRotated ///<
    };
    Q_ENUM(LabelMode)

    template <typename T>
    using OpsFor = QCPPolarAxisAngularOps<T>;

    explicit QCPPolarAxisAngular(QCustomPlot* parentPlot);
    virtual ~QCPPolarAxisAngular();

    // getters:
    QPixmap background() const { return mBackgroundPixmap; }

    QBrush backgroundBrush() const { return mBackgroundBrush; }

    bool backgroundScaled() const { return mBackgroundScaled; }

    Qt::AspectRatioMode backgroundScaledMode() const { return mBackgroundScaledMode; }

    bool rangeDrag() const { return mRangeDrag; }

    bool rangeZoom() const { return mRangeZoom; }

    double rangeZoomFactor() const { return mRangeZoomFactor; }

    const QCPRange range() const { return mRange; }

    bool rangeReversed() const { return mRangeReversed; }

    double angle() const { return mAngle; }

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

    QVector<QString> tickVectorLabels() const { return mTickVectorLabels; }

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

    SelectableParts selectedParts() const { return mSelectedParts; }

    SelectableParts selectableParts() const { return mSelectableParts; }

    QFont selectedTickLabelFont() const { return mSelectedTickLabelFont; }

    QFont selectedLabelFont() const { return mSelectedLabelFont; }

    QColor selectedTickLabelColor() const { return mSelectedTickLabelColor; }

    QColor selectedLabelColor() const { return mSelectedLabelColor; }

    QPen selectedBasePen() const { return mSelectedBasePen; }

    QPen selectedTickPen() const { return mSelectedTickPen; }

    QPen selectedSubTickPen() const { return mSelectedSubTickPen; }

    QCPPolarGrid* grid() const { return mGrid; }

    // setters:
    void setBackground(const QPixmap& pm);
    void setBackground(const QPixmap& pm, bool scaled,
                       Qt::AspectRatioMode mode = Qt::KeepAspectRatioByExpanding);
    void setBackground(const QBrush& brush);
    void setBackgroundScaled(bool scaled);
    void setBackgroundScaledMode(Qt::AspectRatioMode mode);
    void setRangeDrag(bool enabled);
    void setRangeZoom(bool enabled);
    void setRangeZoomFactor(double factor);

    Q_SLOT void setRange(const QCPRange& range);
    void setRange(double lower, double upper);
    void setRange(double position, double size, Qt::AlignmentFlag alignment);
    void setRangeLower(double lower);
    void setRangeUpper(double upper);
    void setRangeReversed(bool reversed);
    void setAngle(double degrees);
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
    void setLabelPosition(Qt::AlignmentFlag position);
    void setSelectedTickLabelFont(const QFont& font);
    void setSelectedLabelFont(const QFont& font);
    void setSelectedTickLabelColor(const QColor& color);
    void setSelectedLabelColor(const QColor& color);
    void setSelectedBasePen(const QPen& pen);
    void setSelectedTickPen(const QPen& pen);
    void setSelectedSubTickPen(const QPen& pen);
    Q_SLOT void setSelectableParts(const QCPPolarAxisAngular::SelectableParts& selectableParts);
    Q_SLOT void setSelectedParts(const QCPPolarAxisAngular::SelectableParts& selectedParts);

    // reimplemented virtual methods:
    virtual double selectTest(const QPointF& pos, bool onlySelectable,
                              QVariant* details = 0) const override;
    virtual void update(UpdatePhase phase) override;
    virtual QList<QCPLayoutElement*> elements(bool recursive) const override;

    // non-property methods:
    bool removeGraph(QCPPolarGraph* graph);
    int radialAxisCount() const;
    QCPPolarAxisRadial* radialAxis(int index = 0) const;
    QList<QCPPolarAxisRadial*> radialAxes() const;
    QCPPolarAxisRadial* addRadialAxis(QCPPolarAxisRadial* axis = 0);
    bool removeRadialAxis(QCPPolarAxisRadial* axis);

    QCPLayoutInset* insetLayout() const { return mInsetLayout; }

    // The handle form of the graph constructor: the polar graph belongs to this axis and draws on
    // its first radial axis. args go to the handle's setData.
    template <std::same_as<QCPPolarGraph> T, typename... Args>
    qcp::HandleFor<T> add(Args&&... args)
    {
        using Handle = qcp::HandleFor<T>;
        static_assert(requires(Handle h) { h.setData(std::declval<Args>()...); },
                      "add<T>(args...): the graph's handle has no setData taking these arguments");
        QCP_CHECK_OR_RETURN(mParentPlot && !mParentPlot->isBeingDestroyed(),
                            "the plot is being destroyed", Handle{});
        QCP_CHECK_OR_RETURN(radialAxis(0), "the angular axis has no radial axis", Handle{});
        Handle handle = qcp::makeHandle<Handle>(new T(this, radialAxis(0)));
        handle.get()->markDirty();
        try
        {
            handle.setData(std::forward<Args>(args)...);
        }
        catch (...)
        {
            handle.remove(); // the graph unregisters itself from this axis
            throw;
        }
        return handle;
    }


    QRegion exactClipRegion() const;

    void moveRange(double diff);
    void scaleRange(double factor);
    void scaleRange(double factor, double center);
    void rescale(bool onlyVisiblePlottables = false);

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
    SelectablePart getPartAt(const QPointF& pos) const;

    // read-only interface imitating a QRect:
    int left() const { return mRect.left(); }

    int right() const { return mRect.right(); }

    int top() const { return mRect.top(); }

    int bottom() const { return mRect.bottom(); }

    int width() const { return mRect.width(); }

    int height() const { return mRect.height(); }

    QSize size() const { return mRect.size(); }

    QPoint topLeft() const { return mRect.topLeft(); }

    QPoint topRight() const { return mRect.topRight(); }

    QPoint bottomLeft() const { return mRect.bottomLeft(); }

    QPoint bottomRight() const { return mRect.bottomRight(); }

    QPointF center() const { return mCenter; }

    double radius() const { return mRadius; }

signals:
    void rangeChanged(const QCPRange& newRange, const QCPRange& oldRange);
    void selectionChanged(const QCPPolarAxisAngular::SelectableParts& parts);
    void selectableChanged(const QCPPolarAxisAngular::SelectableParts& parts);

protected:
    // property members:
    QBrush mBackgroundBrush;
    QPixmap mBackgroundPixmap;
    QPixmap mScaledBackgroundPixmap;
    bool mBackgroundScaled;
    Qt::AspectRatioMode mBackgroundScaledMode;
    QCPLayoutInset* mInsetLayout;
    bool mRangeDrag;
    bool mRangeZoom;
    double mRangeZoomFactor;

    // axis base:
    double mAngle, mAngleRad;
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

    // non-property members:
    QPointF mCenter;
    double mRadius;
    QList<QCPPolarAxisRadial*> mRadialAxes;
    QCPPolarGrid* mGrid;
    QList<QCPPolarGraph*> mGraphs;
    QSharedPointer<QCPAxisTicker> mTicker;
    QVector<double> mTickVector;
    QVector<QString> mTickVectorLabels;
    QVector<QPointF> mTickVectorCosSin;
    QVector<double> mSubTickVector;
    QVector<QPointF> mSubTickVectorCosSin;
    bool mDragging;
    QCPRange mDragAngularStart;
    QList<QPair<QPointer<QCPPolarAxisRadial>, QCPRange>> mDragRadialStart;
    QCP::AntialiasedElements mAADragBackup, mNotAADragBackup;
    QCPLabelPainterPrivate mLabelPainter;

    // reimplemented virtual methods:
    virtual void applyDefaultAntialiasingHint(QCPPainter* painter) const override;
    virtual void draw(QCPPainter* painter) override;
    virtual QCP::Interaction selectionCategory() const override;
    // events:
    virtual void mousePressEvent(QMouseEvent* event, const QVariant& details) override;
    virtual void mouseMoveEvent(QMouseEvent* event, const QPointF& startPos) override;
    virtual void mouseReleaseEvent(QMouseEvent* event, const QPointF& startPos) override;
    virtual void wheelEvent(QWheelEvent* event) override;

    // non-virtual methods:
    bool registerPolarGraph(QCPPolarGraph* graph);
    void drawBackground(QCPPainter* painter, const QPointF& center, double radius);
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
    void emitRangeChanged(const QCPRange& oldRange);

    friend class QCustomPlot;
    friend class QCPPolarGrid;
    friend class QCPPolarGraph;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QCPPolarAxisAngular::SelectableParts)
Q_DECLARE_METATYPE(QCPPolarAxisAngular::SelectablePart)


// The handle of QCPPolarAxisAngular: what add<T>() returns for it.
template <typename T, typename Self>
class QCPPolarAxisAngularOps : public QCPLayerableOps<T, qcp::OpsRet<Self, QCPPolarAxisAngularOps<T>>>
{
    using Base = QCPLayerableOps<T, qcp::OpsRet<Self, QCPPolarAxisAngularOps<T>>>;

public:
    using typename Base::Ret;
    using Base::Base;

    Ret setBackground(const QBrush& brush)
    {
        return this->apply([&](T& o) { o.setBackground(brush); });
    }
    Ret setBackground(const QPixmap& pixmap)
    {
        return this->apply([&](T& o) { o.setBackground(pixmap); });
    }
    Ret setBackground(const QPixmap& pixmap, bool scaled, Qt::AspectRatioMode mode)
    {
        return this->apply([&](T& o) { o.setBackground(pixmap, scaled, mode); });
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
    Ret setRangeZoom(bool enabled)
    {
        return this->apply([&](T& o) { o.setRangeZoom(enabled); });
    }
    Ret setRangeZoomFactor(double factor)
    {
        return this->apply([&](T& o) { o.setRangeZoomFactor(factor); });
    }

    // The handle form of addGraph(): the polar graph belongs to this axis and draws on its first
    // radial axis. args go to the graph handle's setData.
    template <std::same_as<QCPPolarGraph> Graph, typename... Args>
    qcp::HandleFor<Graph> add(Args&&... args)
    {
        auto* axis = this->get();
        QCP_CHECK_OR_RETURN(axis, "the handle is dead", qcp::HandleFor<Graph>{});
        return axis->template add<Graph>(std::forward<Args>(args)...);
    }
};

// The handle is complete with only this header included: plot.add<QCPPolarAxisAngular>() is usable
// without polargraph.h (the graph's own handle only needs it when a graph is added).
static_assert(requires(qcp::HandleFor<QCPPolarAxisAngular> handle) { handle.setRange(0.0, 1.0); },
              "the angular axis handle must be complete in this header");

#endif // QCP_LAYOUTELEMENT_ANGULARAXIS_H
