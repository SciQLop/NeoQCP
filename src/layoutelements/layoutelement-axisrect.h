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

#ifndef QCP_LAYOUTELEMENT_AXISRECT_H
#define QCP_LAYOUTELEMENT_AXISRECT_H

#include "../axis/axis.h"
#include "../global.h"
#include "area.h"
#include <concepts>

class QCPPainter;
class QCustomPlot;
class QCPAxis;
class QCPAbstractPlottable;
class QCPGraph;
class QCPAbstractItem;
class QCPColorScale;

template <typename T, typename Self = void>
class QCPColorScaleOps;

template <typename T, typename Self = void>
class QCPAxisRectOps;

class QCP_LIB_DECL QCPAxisRect : public QCPArea
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(Qt::Orientations rangeDrag READ rangeDrag WRITE setRangeDrag)
    Q_PROPERTY(Qt::Orientations rangeZoom READ rangeZoom WRITE setRangeZoom)
    /// \endcond

public:
    template <typename T>
    using OpsFor = QCPAxisRectOps<T>;

    explicit QCPAxisRect(QCustomPlot* parentPlot, bool setupDefaultAxes = true);
    virtual ~QCPAxisRect() override;

    // getters:
    Qt::Orientations rangeDrag() const { return mRangeDrag; }

    Qt::Orientations rangeZoom() const { return mRangeZoom; }

    QCPAxis* rangeDragAxis(Qt::Orientation orientation);
    QCPAxis* rangeZoomAxis(Qt::Orientation orientation);
    QList<QCPAxis*> rangeDragAxes(Qt::Orientation orientation);
    QList<QCPAxis*> rangeZoomAxes(Qt::Orientation orientation);
    double rangeZoomFactor(Qt::Orientation orientation);

    // setters:
    void setRangeDrag(Qt::Orientations orientations);
    void setRangeZoom(Qt::Orientations orientations);
    void setRangeDragAxes(QCPAxis* horizontal, QCPAxis* vertical);
    void setRangeDragAxes(QList<QCPAxis*> axes);
    void setRangeDragAxes(QList<QCPAxis*> horizontal, QList<QCPAxis*> vertical);
    void setRangeZoomAxes(QCPAxis* horizontal, QCPAxis* vertical);
    void setRangeZoomAxes(QList<QCPAxis*> axes);
    void setRangeZoomAxes(QList<QCPAxis*> horizontal, QList<QCPAxis*> vertical);
    // Back to dragging/zooming every axis of the rect that allows it.
    void resetRangeDragAxes();
    void resetRangeZoomAxes();
    void setRangeZoomFactor(double horizontalFactor, double verticalFactor);
    void setRangeZoomFactor(double factor);

    // non-property methods:
    int axisCount(QCPAxis::AxisType type) const;
    QCPAxis* axis(QCPAxis::AxisType type, int index = 0) const;
    QList<QCPAxis*> axes(QCPAxis::AxisTypes types) const;
    QList<QCPAxis*> axes() const;
    QList<QCPAxisBase*> allAxes() const override;
    QCPRatioTransform ratioTransform() const override;

    // The handle form of addAxis(type): the rect owns the new axis.
    template <std::same_as<QCPAxis> T>
    qcp::HandleFor<T> add(QCPAxis::AxisType type)
    {
        using Handle = qcp::HandleFor<T>;
        QCPAxis* axis = addAxis(type);
        return axis ? qcp::makeHandle<Handle>(axis) : Handle { };
    }

    // The handle form of a plottable constructor: the plottable belongs to this rect and is drawn
    // in the system of its bottom and left axes, or of \a key and \a value; args go to the handle's
    // setData. Defined in construct.h, where the construction rules live.
    template <typename T, typename... Args>
        requires std::derived_from<T, QCPAbstractPlottable>
    qcp::HandleFor<T> add(Args&&... args);

    template <typename T, typename... Args>
        requires std::derived_from<T, QCPAbstractPlottable>
    qcp::HandleFor<T> add(QCPAxis* key, QCPAxis* value, Args&&... args);

    QCPAxis* addAxis(QCPAxis::AxisType type, QCPAxis* axis = nullptr);
    QList<QCPAxis*> addAxes(QCPAxis::AxisTypes types);
    bool removeAxis(QCPAxis* axis);

    void zoom(const QRectF& pixelRect);
    void zoom(const QRectF& pixelRect, const QList<QCPAxis*>& affectedAxes);
    // The data follows a pointer moved by delta pixels, on the range drag axes and orientations.
    void panByPixels(const QPointF& delta);
    // Scales the range zoom axes by factor (< 1 zooms in) around the pixel pos.
    void zoomAt(const QPointF& pos, double factor);
    void setupFullAxesBox(bool connectRanges = false);
    QList<QCPAbstractPlottable*> plottables() const;
    QList<QCPGraph*> graphs() const;
    QList<QCPAbstractItem*> items() const;
    //! The colour scale this rect owns and places in its margin, made on first call. It is visible
    //! exactly while a plottable uses it.
    QCPColorScaleOps<QCPColorScale> colorScale();

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

    QPoint center() const { return mRect.center(); }

    // reimplemented virtual methods:
    virtual void update(UpdatePhase phase) override;
    virtual int calculateAutoMargin(QCP::MarginSide side) override;

protected:
    // The pair the rect maps, refused as misuse when both axes grow the same way.
    QCPCoordinateSystem* createSystem(QCPAxisBase* key, QCPAxisBase* value) override;

    // property members:
    Qt::Orientations mRangeDrag, mRangeZoom;
    QList<QPointer<QCPAxis>> mRangeDragHorzAxis, mRangeDragVertAxis;
    QList<QPointer<QCPAxis>> mRangeZoomHorzAxis, mRangeZoomVertAxis;
    // A list set by the user wins whole; otherwise every axis of the rect that allows it is moved.
    bool mRangeDragAxesSet = false, mRangeZoomAxesSet = false;
    double mRangeZoomFactorHorz, mRangeZoomFactorVert;

    // non-property members:
    // The axes a drag in progress moves, one per source, snapshotted at the press.
    QList<QPointer<QCPAxis>> mDragHorzAxes, mDragVertAxes;
    QList<QCPRange> mDragStartHorzRange, mDragStartVertRange;
    QCP::AntialiasedElements mAADragBackup, mNotAADragBackup;
    QCP::AntialiasedElements mAAWheelBackup, mNotAAWheelBackup;
    bool mDragging;
    bool mWheelZooming = false;
    QTimer mWheelZoomRestoreTimer;
    QHash<QCPAxis::AxisType, QList<QCPAxis*>> mAxes;
    QPointer<QCPColorScale> mColorScale;

    // reimplemented virtual methods:
    virtual void layoutChanged() override;

    // A rect inside a QCPColorScale must not make a scale of its own.
    virtual bool mayOwnColorScale() const { return true; }

    // events:
    virtual void mousePressEvent(QMouseEvent* event, const QVariant& details) override;
    virtual void mouseMoveEvent(QMouseEvent* event, const QPointF& startPos) override;
    virtual void mouseReleaseEvent(QMouseEvent* event, const QPointF& startPos) override;
    virtual void wheelEvent(QWheelEvent* event) override;

    // non-property methods:
    void updateAxesOffset(QCPAxis::AxisType type);
    void zoomAxesAt(const QPointF& pos, double horizontalFactor, double verticalFactor);
    void markAffectedLayersDirty(bool invalidate = false) override;
    // A margin or size change moves the inner rect, so every axis, grid and plottable in it draws
    // at new pixels.
    void markLayoutAppearanceDirty() override;

private:
    Q_DISABLE_COPY(QCPAxisRect)

    // The rect's own scale while it has at least one user, otherwise null.
    QCPColorScale* usedColorScale() const;
    // The width a vertical scale takes in the margin, or the height of a horizontal one.
    int colorScaleExtent() const;
    void placeColorScale();
    // The scale gained or lost a user, or changed the side or width it needs: the margin moves.
    void colorScaleUsersChanged();
    // The axes of that orientation a drag (or zoom) moves: the explicit set, or the default one.
    QList<QCPAxis*> effectiveRangeAxes(bool drag, Qt::Orientation orientation) const;
    // One axis per source: a listed source moves itself, otherwise the first listed axis linked
    // to it moves it through its own pixel motion.
    QList<QCPAxis*> moveRepresentatives(const QList<QCPAxis*>& axes) const;

    // Takes axis out of mAxes and the plot's shortcuts, without deleting it. False if not listed.
    bool unlistAxis(QCPAxis* axis);
    // Afterwards the plottables' and items' axis pointers are null and nothing can find them.
    void markLayersUsingAxisDirty(const QCPAxis* axis);
    // Only the item layers: markAffectedLayersDirty() already covers plottables.
    void markItemsUsingAxisDirty(const QCPAxis* axis);
    void detachFromOwners();

    friend class QCPAxis; // rangeDidChange() -> markAffectedLayersDirty(), ~QCPAxis -> unlistAxis()
    friend class QCPColorScale; // it reports user and size changes that move this rect's margin
    friend class QCPAbstractColorMapped; // it seeds the scale it causes this rect to make

    friend class QCustomPlot;
    friend class TestPaintBuffer;
};

// The handle of an axis rect: created in the layout by QCustomPlot::add<QCPAxisRect>(row, column).
template <typename T, typename Self>
class QCPAxisRectOps : public QCPLayerableOps<T, qcp::OpsRet<Self, QCPAxisRectOps<T>>>
{
    using Base = QCPLayerableOps<T, qcp::OpsRet<Self, QCPAxisRectOps<T>>>;

public:
    using Base::Base;
    using typename Base::Ret;

    Ret setBackground(const QBrush& brush)
    {
        return this->apply([&](T& p) { p.setBackground(brush); });
    }

    Ret setBackground(const QPixmap& pixmap)
    {
        return this->apply([&](T& p) { p.setBackground(pixmap); });
    }

    Ret setRangeDrag(Qt::Orientations orientations)
    {
        return this->apply([&](T& p) { p.setRangeDrag(orientations); });
    }

    Ret setRangeZoom(Qt::Orientations orientations)
    {
        return this->apply([&](T& p) { p.setRangeZoom(orientations); });
    }

    Ret setRangeZoomFactor(double factor)
    {
        return this->apply([&](T& p) { p.setRangeZoomFactor(factor); });
    }
};

#endif // QCP_LAYOUTELEMENT_AXISRECT_H
