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

#ifndef QCP_AXIS_H
#define QCP_AXIS_H

#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include "../global.h"
#include "labelrenderer.h"
Q_MOC_INCLUDE(
    "layoutelements/layoutelement-axisrect.h") // Qt6 needs this if using forward declared types in
                                               // Q_PROPERTY; path is relative to .pro
#include "../layer.h"
#include "../layout.h"
#include "../lineending.h"
#include "../vector2d.h"
#include "axis-base.h"
#include "axisticker.h"
#include "range.h"

class QCPPainter;
class QCustomPlot;
class QCPAxis;
class QCPAxisRect;
class QCPAxisPainterPrivate;
class QCPAbstractPlottable;
class QCPGraph;
class QCPAbstractItem;

template <typename T, typename Self = void>
class QCPAxisOps;

struct QCPAxisLink;

class QCP_LIB_DECL QCPGrid : public QCPLayerable
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(bool subGridVisible READ subGridVisible WRITE setSubGridVisible)
    Q_PROPERTY(bool antialiasedSubGrid READ antialiasedSubGrid WRITE setAntialiasedSubGrid)
    Q_PROPERTY(bool antialiasedZeroLine READ antialiasedZeroLine WRITE setAntialiasedZeroLine)
    Q_PROPERTY(QPen pen READ pen WRITE setPen)
    Q_PROPERTY(QPen subGridPen READ subGridPen WRITE setSubGridPen)
    Q_PROPERTY(QPen zeroLinePen READ zeroLinePen WRITE setZeroLinePen)
    /// \endcond

public:
    explicit QCPGrid(QCPAxis* parentAxis);

    // getters:
    bool subGridVisible() const { return mSubGridVisible; }

    bool antialiasedSubGrid() const { return mAntialiasedSubGrid; }

    bool antialiasedZeroLine() const { return mAntialiasedZeroLine; }

    QPen pen() const { return mPen; }

    QPen subGridPen() const { return mSubGridPen; }

    QPen zeroLinePen() const { return mZeroLinePen; }

    // setters:
    void setSubGridVisible(bool visible);
    void setAntialiasedSubGrid(bool enabled);
    void setAntialiasedZeroLine(bool enabled);
    void setPen(const QPen& pen);
    void setSubGridPen(const QPen& pen);
    void setZeroLinePen(const QPen& pen);

protected:
    // property members:
    bool mSubGridVisible;
    bool mAntialiasedSubGrid, mAntialiasedZeroLine;
    QPen mPen, mSubGridPen, mZeroLinePen;

    // non-property members:
    QCPAxis* mParentAxis;

    // reimplemented virtual methods:
    virtual void applyDefaultAntialiasingHint(QCPPainter* painter) const override;
    virtual void draw(QCPPainter* painter) override;

    // non-virtual methods:
    void drawGridLines(QCPPainter* painter) const;
    void drawSubGridLines(QCPPainter* painter) const;

    friend class QCPAxis;
};

class QCP_LIB_DECL QCPAxis : public QCPAxisBase
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(AxisType axisType READ axisType)
    Q_PROPERTY(QCPAxisRect* axisRect READ axisRect)
    Q_PROPERTY(bool ticks READ ticks WRITE setTicks)
    Q_PROPERTY(bool tickLabels READ tickLabels WRITE setTickLabels)
    Q_PROPERTY(int tickLabelPadding READ tickLabelPadding WRITE setTickLabelPadding)
    Q_PROPERTY(QFont tickLabelFont READ tickLabelFont WRITE setTickLabelFont)
    Q_PROPERTY(QColor tickLabelColor READ tickLabelColor WRITE setTickLabelColor)
    Q_PROPERTY(double tickLabelRotation READ tickLabelRotation WRITE setTickLabelRotation)
    Q_PROPERTY(LabelSide tickLabelSide READ tickLabelSide WRITE setTickLabelSide)
    Q_PROPERTY(QString numberFormat READ numberFormat WRITE setNumberFormat)
    Q_PROPERTY(int numberPrecision READ numberPrecision WRITE setNumberPrecision)
    Q_PROPERTY(int tickLengthIn READ tickLengthIn WRITE setTickLengthIn)
    Q_PROPERTY(int tickLengthOut READ tickLengthOut WRITE setTickLengthOut)
    Q_PROPERTY(bool subTicks READ subTicks WRITE setSubTicks)
    Q_PROPERTY(int subTickLengthIn READ subTickLengthIn WRITE setSubTickLengthIn)
    Q_PROPERTY(int subTickLengthOut READ subTickLengthOut WRITE setSubTickLengthOut)
    Q_PROPERTY(QPen basePen READ basePen WRITE setBasePen)
    Q_PROPERTY(QPen tickPen READ tickPen WRITE setTickPen)
    Q_PROPERTY(QPen subTickPen READ subTickPen WRITE setSubTickPen)
    Q_PROPERTY(QFont labelFont READ labelFont WRITE setLabelFont)
    Q_PROPERTY(QColor labelColor READ labelColor WRITE setLabelColor)
    Q_PROPERTY(QString label READ label WRITE setLabel)
    Q_PROPERTY(int labelPadding READ labelPadding WRITE setLabelPadding)
    Q_PROPERTY(int padding READ padding WRITE setPadding)
    Q_PROPERTY(int offset READ offset WRITE setOffset)
    Q_PROPERTY(
        QFont selectedTickLabelFont READ selectedTickLabelFont WRITE setSelectedTickLabelFont)
    Q_PROPERTY(QFont selectedLabelFont READ selectedLabelFont WRITE setSelectedLabelFont)
    Q_PROPERTY(
        QColor selectedTickLabelColor READ selectedTickLabelColor WRITE setSelectedTickLabelColor)
    Q_PROPERTY(QColor selectedLabelColor READ selectedLabelColor WRITE setSelectedLabelColor)
    Q_PROPERTY(QPen selectedBasePen READ selectedBasePen WRITE setSelectedBasePen)
    Q_PROPERTY(QPen selectedTickPen READ selectedTickPen WRITE setSelectedTickPen)
    Q_PROPERTY(QPen selectedSubTickPen READ selectedSubTickPen WRITE setSelectedSubTickPen)
    Q_PROPERTY(QCPLineEnding lowerEnding READ lowerEnding WRITE setLowerEnding)
    Q_PROPERTY(QCPLineEnding upperEnding READ upperEnding WRITE setUpperEnding)
    Q_PROPERTY(QCPGrid* grid READ grid)
    /// \endcond

public:
    template <typename T>
    using OpsFor = QCPAxisOps<T>;

    /*!
      Defines at which side of the axis rect the axis will appear. This also affects how the tick
      marks are drawn, on which side the labels are placed etc.
    */
    enum AxisType
    {
        atLeft = 0x01 ///< <tt>0x01</tt> Axis is vertical and on the left side of the axis rect
        ,
        atRight = 0x02 ///< <tt>0x02</tt> Axis is vertical and on the right side of the axis rect
        ,
        atTop = 0x04 ///< <tt>0x04</tt> Axis is horizontal and on the top side of the axis rect
        ,
        atBottom
        = 0x08 ///< <tt>0x08</tt> Axis is horizontal and on the bottom side of the axis rect
    };
    Q_ENUM(AxisType)
    Q_DECLARE_FLAGS(AxisTypes, AxisType)
    Q_FLAG(AxisTypes)

    /*!
      Defines on which side of the axis the tick labels (numbers) shall appear.

      \see setTickLabelSide
    */
    enum LabelSide
    {
        lsInside ///< Tick labels will be displayed inside the axis rect and clipped to the inner
                 ///< axis rect
        ,
        lsOutside ///< Tick labels will be displayed outside the axis rect
    };
    Q_ENUM(LabelSide)

    explicit QCPAxis(QCPAxisRect* parent, AxisType type);
    virtual ~QCPAxis() override;

    // getters:
    AxisType axisType() const { return mAxisType; }

    // Out of line: it reads the base's area pointer, and the rect type is complete in the .cpp.
    QCPAxisRect* axisRect() const;

    QCPArea* area() const override;

    bool ticks() const { return mTicks; }

    bool tickLabels() const { return mTickLabels; }

    int tickLabelPadding() const;

    QFont tickLabelFont() const { return mTickLabelFont; }

    QColor tickLabelColor() const { return mTickLabelColor; }

    double tickLabelRotation() const;
    LabelSide tickLabelSide() const;
    QString numberFormat() const;

    int numberPrecision() const { return mNumberPrecision; }

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

    QCPLabelRenderer* labelRenderer() const { return mLabelRenderer; }

    int labelPadding() const;

    int padding() const { return mPadding; }

    int offset() const;

    QFont selectedTickLabelFont() const { return mSelectedTickLabelFont; }

    QFont selectedLabelFont() const { return mSelectedLabelFont; }

    QColor selectedTickLabelColor() const { return mSelectedTickLabelColor; }

    QColor selectedLabelColor() const { return mSelectedLabelColor; }

    QPen selectedBasePen() const { return mSelectedBasePen; }

    QPen selectedTickPen() const { return mSelectedTickPen; }

    QPen selectedSubTickPen() const { return mSelectedSubTickPen; }

    QCPLineEnding lowerEnding() const;
    QCPLineEnding upperEnding() const;

    QCPGrid* grid() const;

    // Never creates a grid: null once the user deleted it. For internal draw and dirty-marking
    // paths, which must not bring back a deleted grid.
    QCPGrid* gridIfExists() const { return mGrid; }

    // setters:
    void setTicks(bool show);
    void setTickLabels(bool show);
    void setTickLabelPadding(int padding);
    void setTickLabelFont(const QFont& font);
    void setTickLabelColor(const QColor& color);
    void setTickLabelRotation(double degrees);
    void setTickLabelSide(LabelSide side);
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
    //! The colour of the label, the tick labels and of the base, tick and sub-tick pens.
    void setColor(const QColor& color);
    void setLabel(const QString& str);
    /*! Substitute the label typesetter; \a renderer is not owned and must
        outlive the axis. nullptr restores plain QPainter::drawText. */
    void setLabelRenderer(QCPLabelRenderer* renderer);
    void setLabelPadding(int padding);
    void setPadding(int padding);
    void setOffset(int offset);
    void setSelectedTickLabelFont(const QFont& font);
    void setSelectedLabelFont(const QFont& font);
    void setSelectedTickLabelColor(const QColor& color);
    void setSelectedLabelColor(const QColor& color);
    void setSelectedBasePen(const QPen& pen);
    void setSelectedTickPen(const QPen& pen);
    void setSelectedSubTickPen(const QPen& pen);
    void setLowerEnding(const QCPLineEnding& ending);
    void setUpperEnding(const QCPLineEnding& ending);

    // non-property methods:
    Qt::Orientation orientation() const { return mOrientation; }

    int pixelOrientation() const
    {
        return rangeReversed() != (orientation() == Qt::Vertical) ? -1 : 1;
    }

    void setScaleRatio(const QCPAxis* otherAxis, double ratio = 1.0);
    void rescale(bool onlyVisiblePlottables = false) override;
    double pixelToCoord(double value) const;
    double coordToPixel(double value) const;

    // linked axes:
    QCPAxis* linkSource() const override;

    /*! \internal The axis a range move of this one acts on: the link's source when linked. */
    QCPAxis* moveTarget() const override;
    /*! \internal \a pixel seen on the move target's rect, at the same fraction of the axis. */
    double pixelOnMoveTarget(double pixel) const;
    QList<QCPAbstractPlottable*> plottables() const override;
    QList<QCPGraph*> graphs() const;
    QList<QCPAbstractItem*> items() const;

    static AxisType marginSideToAxisType(QCP::MarginSide side);

    static Qt::Orientation orientation(AxisType type)
    {
        return type == atBottom || type == atTop ? Qt::Horizontal : Qt::Vertical;
    }

    static AxisType opposite(AxisType type);

    // The part of this axis at pos, spNone where there is none. Public as in 2.x.
    SelectablePart getPartAt(const QPointF& pos) const override;

protected:
    // property members:
    // axis base:
    AxisType mAxisType;
    // int mOffset; // in QCPAxisPainter
    int mPadding;
    QPen mBasePen, mSelectedBasePen;
    // QCPLineEnding mLowerEnding, mUpperEnding; // in QCPAxisPainter
    //  axis label:
    // int mLabelPadding; // in QCPAxisPainter
    QString mLabel;
    QCPLabelRenderer* mLabelRenderer = nullptr;
    QFont mLabelFont, mSelectedLabelFont;
    QColor mLabelColor, mSelectedLabelColor;
    // tick labels:
    // int mTickLabelPadding; // in QCPAxisPainter
    bool mTickLabels;
    // double mTickLabelRotation; // in QCPAxisPainter
    QFont mTickLabelFont, mSelectedTickLabelFont;
    QColor mTickLabelColor, mSelectedTickLabelColor;
    int mNumberPrecision;
    QLatin1Char mNumberFormatChar;
    bool mNumberBeautifulPowers;
    // bool mNumberMultiplyCross; // QCPAxisPainter
    //  ticks and subticks:
    bool mTicks;
    bool mSubTicks;
    // int mTickLengthIn, mTickLengthOut, mSubTickLengthIn, mSubTickLengthOut; // QCPAxisPainter
    QPen mTickPen, mSelectedTickPen;
    QPen mSubTickPen, mSelectedSubTickPen;

    // non-property members:
    mutable QPointer<QCPGrid> mGrid;
    QCPAxisPainterPrivate* mAxisPainter;
    bool mCachedMarginValid;
    int mCachedMargin;
    bool mDragging;
    QCPRange mDragStartRange;
    // The axis mDragStartRange belongs to: the link's source when the strip drags a linked axis.
    QPointer<QCPAxis> mDragTarget;
    QCP::AntialiasedElements mAADragBackup, mNotAADragBackup;

    // introduced virtual methods:
    virtual int calculateMargin();
    void applyTickLabelFormat();
    // A scale type change stales the margin and every item tree of the plot, then moves the axes
    // that follow this one.
    virtual bool scaleTypeWillChange(ScaleType type) override;
    // A look change moves every axis, grid and plottable of the rect.
    virtual void markAppearanceDirty() override;
    virtual bool checkLinkSource(const QCPAxisBase* source) const override;
    virtual void rangeDidChange(const QCPRange& oldRange) override;

    // reimplemented virtual methods:
    virtual void applyDefaultAntialiasingHint(QCPPainter* painter) const override;
    virtual void draw(QCPPainter* painter) override;
    // events:
    virtual void mousePressEvent(QMouseEvent* event, const QVariant& details) override;
    virtual void mouseMoveEvent(QMouseEvent* event, const QPointF& startPos) override;
    virtual void mouseReleaseEvent(QMouseEvent* event, const QPointF& startPos) override;
    virtual void wheelEvent(QWheelEvent* event) override;

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
    Q_DISABLE_COPY(QCPAxis)
    double linkedCoordToPixel(double value) const;
    double linkedPixelToCoord(double value) const;

    friend class QCustomPlot;
    friend class QCPGrid;
    friend class QCPAxisRect;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QCPAxis::AxisTypes)
Q_DECLARE_METATYPE(QCPAxis::AxisType)
Q_DECLARE_METATYPE(QCPAxis::LabelSide)

class QCPAxisPainterPrivate
{
public:
    explicit QCPAxisPainterPrivate(QCustomPlot* parentPlot);
    virtual ~QCPAxisPainterPrivate();

    virtual void draw(QCPPainter* painter);
    virtual int size();
    void clearCache();

    QRect axisSelectionBox() const { return mAxisSelectionBox; }

    QRect tickLabelsSelectionBox() const { return mTickLabelsSelectionBox; }

    QRect labelSelectionBox() const { return mLabelSelectionBox; }

    // public property members:
    QCPAxis::AxisType type;
    QPen basePen;
    QCPLineEnding lowerEnding, upperEnding; // directly accessed by QCPAxis setters/getters
    int labelPadding; // directly accessed by QCPAxis setters/getters
    QFont labelFont;
    QColor labelColor;
    QString label;
    QCPLabelRenderer* labelRenderer = nullptr;
    int tickLabelPadding; // directly accessed by QCPAxis setters/getters
    double tickLabelRotation; // directly accessed by QCPAxis setters/getters
    QCPAxis::LabelSide tickLabelSide; // directly accessed by QCPAxis setters/getters
    bool substituteExponent;
    bool numberMultiplyCross; // directly accessed by QCPAxis setters/getters
    int tickLengthIn, tickLengthOut, subTickLengthIn,
        subTickLengthOut; // directly accessed by QCPAxis setters/getters
    QPen tickPen, subTickPen;
    QFont tickLabelFont;
    QColor tickLabelColor;
    QRect axisRect, viewportRect;
    int offset; // directly accessed by QCPAxis setters/getters
    bool abbreviateDecimalPowers;
    bool reversedEndings;

    QVector<double> subTickPositions;
    QVector<double> tickPositions;
    QVector<QString> tickLabels;
    bool skipTickDrawing = false;

protected:
    struct CachedLabel
    {
        QPointF offset;
        QPixmap pixmap;
    };

    struct TickLabelData
    {
        QString basePart, expPart, suffixPart;
        QRect baseBounds, expBounds, suffixBounds, totalBounds, rotatedTotalBounds;
        QFont baseFont, expFont;
    };

    // Laying out the axis label is costly (a QTextLayout, or a LaTeX parse) and it rarely
    // changes, so its size and a picture of it are kept until one of these changes.
    struct AxisLabelKey
    {
        const QCPLabelRenderer* renderer = nullptr;
        QString text;
        QFont font;
        QColor color;
        bool operator==(const AxisLabelKey&) const = default;
    };

    struct CachedAxisLabel
    {
        AxisLabelKey key;
        QSize size;
        QSize box;
        double dpr = 0.;
        QString drawn; // the text in the picture: the label, or its elided form
        QPixmap pixmap;
    };

    QCustomPlot* mParentPlot;
    CachedAxisLabel mAxisLabel;
    QByteArray mLabelParameterHash; // to determine whether mLabelCache needs to be cleared due to
                                    // changed parameters
    QCache<QString, CachedLabel> mLabelCache;
    QRect mAxisSelectionBox, mTickLabelsSelectionBox, mLabelSelectionBox;
    QFont mCachedMetricsFont;
    QFontMetrics mCachedFontMetrics {QFont()};
    // Along the axis, where the tick label drawn last spans; empty before the first of a draw.
    std::optional<std::pair<double, double>> mLastTickLabelSpan;

    QSize axisLabelSize();
    bool overlapsLastTickLabel(double from, double to);
    void drawAxisLabel(QCPPainter* painter, const QRect& rect);
    virtual QByteArray generateLabelParameterHash() const;
    const QFontMetrics& fontMetricsFor(const QFont& font);

    QPointF snappedToDevicePixels(const QPointF& point) const;
    virtual void placeTickLabel(QCPPainter* painter, double position, int distanceToAxis,
                                const QString& text, QSize* tickLabelsSize);
    virtual void drawTickLabel(QCPPainter* painter, double x, double y,
                               const TickLabelData& labelData) const;
    virtual TickLabelData getTickLabelData(const QFont& font, const QString& text);
    virtual QPointF getTickLabelDrawOffset(const TickLabelData& labelData) const;
    virtual void getMaxTickLabelSize(const QFont& font, const QString& text,
                                     QSize* tickLabelsSize);
};

// The handle of an axis: the layerable setters and the axis' own.
template <typename T, typename Self>
class QCPAxisOps : public QCPLayerableOps<T, qcp::OpsRet<Self, QCPAxisOps<T>>>
{
    using Base = QCPLayerableOps<T, qcp::OpsRet<Self, QCPAxisOps<T>>>;

public:
    using typename Base::Ret;
    using Base::Base;

    Ret setLabel(const QString& text)
    {
        return this->apply([&](T& a) { a.setLabel(text); });
    }
    Ret setRange(double lower, double upper)
    {
        return this->apply([&](T& a) { a.setRange(lower, upper); });
    }
    Ret setScaleType(QCPAxis::ScaleType type)
    {
        return this->apply([&](T& a) { a.setScaleType(type); });
    }
    Ret setColor(const QColor& color)
    {
        return this->apply([&](T& a) { a.setColor(color); });
    }
    Ret setTicker(const QSharedPointer<QCPAxisTicker>& ticker)
    {
        return this->apply([&](T& a) { a.setTicker(ticker); });
    }
    Ret setRangeReversed(bool reversed)
    {
        return this->apply([&](T& a) { a.setRangeReversed(reversed); });
    }
    Ret setRangeDraggable(bool draggable)
    {
        return this->apply([&](T& a) { a.setRangeDraggable(draggable); });
    }
    Ret setRangeZoomable(bool zoomable)
    {
        return this->apply([&](T& a) { a.setRangeZoomable(zoomable); });
    }
    Ret linkTo(QCPAxis* source, const QCPAxisFunction& forward = {},
               const QCPAxisFunction& inverse = {})
    {
        return this->apply([&](T& a) { a.linkTo(source, forward, inverse); });
    }
};

#endif // QCP_AXIS_H
