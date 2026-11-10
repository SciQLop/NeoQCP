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

#ifndef QCP_AXIS_AXISBASE_H
#define QCP_AXIS_AXISBASE_H

#include "../global.h"
#include "../layer.h"
#include "axisticker.h"
#include "range.h"
#include <QList>
#include <QPointer>
#include <QSharedPointer>
#include <functional>
#include <memory>

class QCPAbstractPlottable;
class QCPArea;
class QCPAxisBase;
struct QCPAxisLink;

//! The mapping a link applies to the source axis' coordinates.
using QCPAxisFunction = std::function<double(double)>;

/*! \brief The range model, the ticks and the selectable parts an axis of any kind shares.

  A cartesian axis, the angular axis of a polar plot and its radial axes all show a range, pick tick
  positions with a ticker, carry selectable parts and may follow another axis through a link. That
  is what this class holds; each subclass adds the geometry of its own kind (a side of an axis rect,
  a circle, a spoke) and its own label formatting.

  The one place a range is written is applyRange, reached through commitRange: a range change
  dirties the layers that depend on the axis, refreshes the axes linked to it, then emits \ref
  rangeChanged. A subclass hooks its own dirty marking in by overriding rangeDidChange.
*/
class QCP_LIB_DECL QCPAxisBase : public QCPLayerable
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(ScaleType scaleType READ scaleType WRITE setScaleType NOTIFY scaleTypeChanged)
    Q_PROPERTY(QCPRange range READ range WRITE setRange NOTIFY rangeChanged)
    Q_PROPERTY(bool rangeReversed READ rangeReversed WRITE setRangeReversed)
    Q_PROPERTY(QSharedPointer<QCPAxisTicker> ticker READ ticker WRITE setTicker)
    Q_PROPERTY(QVector<double> tickVector READ tickVector)
    Q_PROPERTY(QVector<QString> tickVectorLabels READ tickVectorLabels)
    Q_PROPERTY(SelectableParts selectedParts READ selectedParts WRITE setSelectedParts NOTIFY
                   selectionChanged)
    Q_PROPERTY(SelectableParts selectableParts READ selectableParts WRITE setSelectableParts NOTIFY
                   selectableChanged)
    /// \endcond

public:
    /*!
      Defines the scale types an axis can show its range with.
    */
    enum ScaleType
    {
        stLinear ///< Linear scale
        ,
        stLogarithmic ///< Logarithmic scale
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

    // The area the axis is drawn in, which is also its layerable parent, and the direction a
    // cartesian axis maps its range along (the polar axes leave it horizontal and unused).
    explicit QCPAxisBase(QCPArea* area, Qt::Orientation orientation = Qt::Horizontal);
    virtual ~QCPAxisBase() override;

    // getters:
    ScaleType scaleType() const { return mScaleType; }

    const QCPRange range() const { return mRange; }

    bool rangeReversed() const { return mRangeReversed; }

    //! Whether a drag in the area moves this axis, when the area allows range dragging.
    bool rangeDraggable() const { return mRangeDraggable; }

    //! Whether a wheel zoom in the area scales this axis, when the area allows range zooming.
    bool rangeZoomable() const { return mRangeZoomable; }

    QSharedPointer<QCPAxisTicker> ticker() const { return mTicker; }

    QVector<double> tickVector() const { return mTickVector; }

    QVector<double> subTickVector() const { return mSubTickVector; }

    QVector<QString> tickVectorLabels() const { return mTickVectorLabels; }

    SelectableParts selectedParts() const { return mSelectedParts; }

    SelectableParts selectableParts() const { return mSelectableParts; }

    bool selected() const { return mSelectedParts != spNone; }

    QCP::SelectionType selectable() const
    {
        return mSelectableParts != spNone ? QCP::stWhole : QCP::stNone;
    }

    // setters:
    Q_SLOT void setScaleType(QCPAxisBase::ScaleType type);
    Q_SLOT void setRange(const QCPRange& range);
    void setRange(double lower, double upper);
    void setRange(double position, double size, Qt::AlignmentFlag alignment);
    void setRangeLower(double lower);
    void setRangeUpper(double upper);
    void setRangeReversed(bool reversed);
    void setRangeDraggable(bool draggable);
    void setRangeZoomable(bool zoomable);
    void setTicker(QSharedPointer<QCPAxisTicker> ticker);
    Q_SLOT void setSelectableParts(const QCPAxisBase::SelectableParts& selectableParts);
    Q_SLOT void setSelectedParts(const QCPAxisBase::SelectableParts& selectedParts);

    // reimplemented virtual methods:
    virtual double selectTest(const QPointF& pos, bool onlySelectable,
                              QVariant* details = nullptr) const override;
    virtual QCP::Interaction selectionCategory() const override;

    // non-property methods:
    void moveRange(double diff);
    void scaleRange(double factor);
    void scaleRange(double factor, double center);

    // Changes the range so what the plottables of the systems using this axis draw is visible: their
    // drawn data ranges, in this axis' own dimension. A cartesian axis refines this with the data
    // ranges it always used, and the angular axis keeps its full turn.
    virtual void rescale(bool onlyVisiblePlottables = false);
    //! The plottables of every system that maps through this axis, in the systems' creation order.
    virtual QList<QCPAbstractPlottable*> plottables() const;

    // A link makes this axis follow source: its range is forward(source's range), and a range set
    // here is written back through the inverse. Only cartesian axes may take part.
    void linkTo(QCPAxisBase* source, QCPAxisFunction forward = { }, QCPAxisFunction inverse = { });
    void unlink();
    virtual QCPAxisBase* linkSource() const;

    std::shared_ptr<const QCPAxisLink> link() const { return mLink; }

    virtual QCPAxisBase* moveTarget() const;

    // The area this axis is drawn in.
    virtual QCPArea* area() const { return mArea; }

    // The part of this axis at pos, spNone where there is none. Public as in 2.x, where hover
    // cursors and context menus call it directly.
    virtual SelectablePart getPartAt(const QPointF& pos) const = 0;

signals:
    void rangeChanged(const QCPRange& newRange, const QCPRange& oldRange);
    void scaleTypeChanged(QCPAxisBase::ScaleType scaleType);
    void selectionChanged();
    void selectableChanged();

protected:
    // The hot members sit together, first among this class's: a coordToPixel call reads where the
    // axis draws, its range and direction, the link flag and the scale type. For a cartesian axis
    // the area is its rect, whose geometry turns the fraction into a pixel, so both live here
    // rather than in QCPAxis; the polar area answers for the polar axes. GCC 16 puts mArea at
    // +0x40, mRange at +0x48, mRangeReversed at +0x58, mLinked at +0x59, mScaleType at +0x5c and
    // mOrientation at +0x60: 36 bytes, one cache line of the four a call used to touch.
    QCPArea* mArea;
    QCPRange mRange;
    bool mRangeReversed;
    // Set by a link: the flag is what the hot mapping path reads, the shared_ptr lives further
    // down.
    bool mLinked = false;
    bool mRangeDraggable = true;
    bool mRangeZoomable = true;
    ScaleType mScaleType;
    Qt::Orientation mOrientation;

    // property members:
    SelectableParts mSelectableParts, mSelectedParts;

    // non-property members:
    QSharedPointer<QCPAxisTicker> mTicker;
    QVector<double> mTickVector;
    QVector<QString> mTickVectorLabels;
    QVector<double> mSubTickVector;
    //! How many connections listen to rangeChanged: asked on every range set of a drag or a pan.
    int mRangeListeners = 0;
    std::shared_ptr<const QCPAxisLink> mLink;
    QList<QPointer<QCPAxisBase>> mLinkedAxes;

    // reimplemented virtual methods:
    virtual void selectEvent(QMouseEvent* event, bool additive, const QVariant& details,
                             bool* selectionStateChanged) override;
    virtual void deselectEvent(bool* selectionStateChanged) override;

    // Every range setter ends here, after the range was stored: a subclass marks the layers that
    // draw the axis dirty, then calls the base implementation, which refreshes the linked axes and
    // emits rangeChanged.
    virtual void rangeDidChange(const QCPRange& oldRange);

    // The hooks around the scale type change: a cartesian axis drops its cached margin and item
    // trees before, and redraws itself and the axes it drives after.
    //! Asked before the scale type changes: false refuses it, for a subclass whose geometry only
    //! knows one scale.
    virtual bool scaleTypeWillChange([[maybe_unused]] ScaleType type) { return true; }

    virtual void scaleTypeDidChange();
    // Dirties what a look change moves. The default dirties the axis alone; a cartesian axis also
    // dirties the rect's layers.
    virtual void markAppearanceDirty();
    // The checks a subclass adds before it lets a link be made. The default accepts.
    virtual bool checkLinkSource(const QCPAxisBase* source) const;

    //! Whether anything listens to what a mapping change tells: the linked axes or a system.
    [[nodiscard]] bool defersEmissions() const;
    void connectNotify(const QMetaMethod& signal) override;
    void disconnectNotify(const QMetaMethod& signal) override;

    // Drops every link, this axis's own and the ones following it. Idempotent: a subclass calls it
    // while it still exists, the destructor as a fallback.
    void detachLinks();

    // The tail of rescale(): stores \a range, or, when it is not usable (a constant plottable has a
    // zero-size range), recentres the current range on it. Shared with subclasses that rescale.
    void rescaleTo(const QCPRange& range, bool haveRange);

    // Takes the source's range, its direction included, through the link's forward function.
    void refreshFromLink();
    void refreshLinkedAxes();

private:
    Q_DISABLE_COPY(QCPAxisBase)

    // Tells the area its axis maps differently: the systems that use it are stale from now on.
    void notifyMappingChanged();
    // The one place that writes a range: every mutator ends here.
    void applyRange(const QCPRange& range);
    // applyRange, or the inverse function of a link and back through the source.
    void commitRange(const QCPRange& range);
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QCPAxisBase::SelectableParts)

/*! \brief The source an axis follows and the functions that connect the two ranges.

  The struct is immutable once the link is made, and the snapshots of QCPTransform share it.
*/
struct QCP_LIB_DECL QCPAxisLink
{
    QPointer<QCPAxisBase> source;
    QCPAxisFunction forward;
    QCPAxisFunction inverse;
    bool decreasing = false;
};

Q_DECLARE_METATYPE(QCPAxisBase::ScaleType)
Q_DECLARE_METATYPE(QCPAxisBase::SelectablePart)

#endif // QCP_AXIS_AXISBASE_H
