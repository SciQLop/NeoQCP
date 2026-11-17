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
#include "../colorgradient.h"
#include "../coords.h"
#include "../global.h"
#include "../painting/draw-context.h"
#include "../scatterstyle.h"
#include "area.h"
#include <concepts>
#include <optional>
#include <tuple>

namespace qcp
{
// The first argument of QCPAxisRect::add<T>() is an axis pair when it converts to QCPAxis*: the
// two-axis overload must take that call, not the data pack (a handle converts, H-8).
template <typename... Args>
concept FirstArgumentIsNotAnAxis
    = sizeof...(Args) == 0
      || !std::convertible_to<std::tuple_element_t<0, std::tuple<Args...>>, QCPAxis*>;
} // namespace qcp

class QCPPainter;
class QCustomPlot;
class QCPAxis;
class QCPPlottable;
class QCPGraph;
class QCPItem;
class QCPItemEllipse;
class QCPItemText;
class QCPColorScale;
class QCPTextElement;
class QCPColorMap2;

template <typename T, typename Self = void>
class QCPColorScaleOps;

template <typename T, typename Self = void>
class QCPAxisRectOps;

/*!
  How QCPAxisRect::line() styles the graph it makes: the three graph setters a one-call line needs.
  Each field is named after the setter it feeds, so a designated initialiser list reads as the
  setters it replaces. \a pen with \c Qt::NoPen draws the markers of \a scatterStyle only.
*/
struct QCPLineOptions
{
    QPen pen;
    QString name;
    QCPScatterStyle scatterStyle;
};

/*!
  How QCPAxisRect::ellipse() styles the item it makes: its outline pen and its fill brush.
*/
struct QCPShapeOptions
{
    QPen pen;
    QBrush brush;
};

/*!
  How QCPAxisRect::colorMap() styles the map it makes: its gradient and data scale, whether it takes
  the rect's colour bar, the range there, and its name. Each field is named after the setter it
  feeds, so a designated initialiser list reads as the setters it replaces.

  \a colorBar is \c true for the rect's shared colour scale, which the add joins by itself, and
  \c false for no colour scale at all: the map is then coloured by its own gradient and data scale,
  and an unused bar takes no room. \a dataRange unset fits the range on the data given, as
  rescaleDataRange() does.
*/
struct QCPColorMapOptions
{
    QCPColorGradient gradient = QCPColorGradient::gpViridis;
    QCPScale dataScale = QCPScale::linear();
    bool colorBar = true;
    std::optional<QCPRange> dataRange = std::nullopt;
    QString name = {};
};

class QCP_LIB_DECL QCPAxisRect : public QCPArea
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(Qt::Orientations rangeDrag READ rangeDrag WRITE setRangeDrag NOTIFY rangeDragChanged)
    Q_PROPERTY(Qt::Orientations rangeZoom READ rangeZoom WRITE setRangeZoom NOTIFY rangeZoomChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    /// \endcond

public:
    template <typename T>
    using OpsFor = QCPAxisRectOps<T>;

    explicit QCPAxisRect(QCustomPlot* parentPlot, bool setupDefaultAxes = true);
    virtual ~QCPAxisRect() override;

    // getters:
    Qt::Orientations rangeDrag() const { return mRangeDrag; }

    Qt::Orientations rangeZoom() const { return mRangeZoom; }

    QCPHandle<QCPAxis> rangeDragAxis(Qt::Orientation orientation);
    QCPHandle<QCPAxis> rangeZoomAxis(Qt::Orientation orientation);
    QList<QCPHandle<QCPAxis>> rangeDragAxes(Qt::Orientation orientation);
    QList<QCPHandle<QCPAxis>> rangeZoomAxes(Qt::Orientation orientation);
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
    QCPHandle<QCPAxis> axis(QCPAxis::AxisType type, int index = 0) const;
    // The rect's first bottom and left axes: the pair add<T>() binds a plottable to. Empty when the
    // rect has no axis of that side (removed, or not made yet): asking is not misuse.
    QCPHandle<QCPAxis> xAxis() const;
    QCPHandle<QCPAxis> yAxis() const;
    QList<QCPHandle<QCPAxis>> axes(QCPAxis::AxisTypes types) const;
    QList<QCPHandle<QCPAxis>> axes() const;
    QList<QCPHandle<QCPAxisBase>> allAxes() const override;
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
    // in the system of its bottom and left axes, or of \a key and \a value; the args are the
    // plottable's constructor arguments (its data, for the built-in types). Defined in construct.h,
    // where the construction rules live.
    template <typename T, typename... Args>
        requires std::derived_from<T, QCPPlottable>
              && qcp::FirstArgumentIsNotAnAxis<Args...>
    qcp::HandleFor<T> add(Args&&... args);

    template <typename T, typename... Args>
        requires std::derived_from<T, QCPPlottable>
    qcp::HandleFor<T> add(QCPAxis* key, QCPAxis* value, Args&&... args);

    // The handle form of an item constructor: built with the item's own arguments, adopted into
    // this rect, clipped to it and bound to its first bottom and left axes. Defined in construct.h.
    template <typename T, typename... Args>
        requires std::derived_from<T, QCPItem>
    qcp::HandleFor<T> add(Args&&... args);

    // A graph of \a keys and \a values on this rect's axes, in one call: the data goes to the
    // graph's data constructor, and \a options feeds its pen, name and scatter style. Defined in
    // axisrect-shortcuts.h, after the graph and the items exist.
    template <QCPNumberRange K, QCPNumbersOnly V>
    QCPHandle<QCPGraph> line(const K& keys, const V& values,
                             const QCPLineOptions& options = { });
    // A text item at \a at, in whatever space \a at names, carrying \a text and \a style.
    QCPHandle<QCPItemText> text(QCP::Position at, const QString& text,
                                const QCPTextStyle& style = { });
    // The built-in ellipse item between the corners \a a and \a b, each in its own space.
    QCPHandle<QCPItemEllipse> ellipse(QCP::Position a, QCP::Position b,
                                      const QCPShapeOptions& options = { });

    // A colour map of \a z over the grid of \a x and \a y, in one call: the ranges go through
    // qcp::toDoubles into the map's own storage, and \a options feeds its gradient, data scale,
    // name, colour bar and data range. Flat \a z is x-major: z[i * y.size() + j] belongs to
    // x[i], y[j] (x.size() * y.size() values), which is the layout QCPColorMap2::setData takes.
    // x and y need a value each and z exactly that many, or the call is misuse: one report and an
    // empty handle. Defined in axisrect-shortcuts.h, after the map exists.
    template <QCPNumberRange X, QCPNumberRange Y, QCPNumbersOnly Z>
    QCPHandle<QCPColorMap2> colorMap(const X& x, const Y& y, const Z& z,
                                     const QCPColorMapOptions& options = { });
    // The columns form of colorMap(): one column of \a z per \a x value, each of y.size() values,
    // flattened x-major. Ragged columns, a column length other than y.size(), or a count other than
    // x.size(), are misuse: one report and an empty handle.
    template <QCPNumberRange X, QCPNumberRange Y, QCPColumns Z>
    QCPHandle<QCPColorMap2> colorMap(const X& x, const Y& y, const Z& z,
                                     const QCPColorMapOptions& options = { });

    QCPHandle<QCPAxis> addAxis(QCPAxis::AxisType type, QCPAxis* axis = nullptr);
    QList<QCPHandle<QCPAxis>> addAxes(QCPAxis::AxisTypes types);
    bool removeAxis(QCPAxis* axis);

    void zoom(const QRectF& pixelRect);
    void zoom(const QRectF& pixelRect, const QList<QCPAxis*>& affectedAxes);
    // The data follows a pointer moved by delta pixels, on the range drag axes and orientations.
    void panByPixels(const QPointF& delta);
    // Scales the range zoom axes by factor (< 1 zooms in) around the pixel pos.
    void zoomAt(const QPointF& pos, double factor);
    void setupFullAxesBox(bool connectRanges = false);
    QList<QCPHandle<QCPPlottable>> plottables() const;
    QList<QCPHandle<QCPGraph>> graphs() const;
    QList<QCPHandle<QCPItem>> items() const;
    //! The colour scale this rect owns and places in its margin, made on first call. It is visible
    //! exactly while a plottable colours through it. A plottable given its own scale, or none,
    //! with setColorScale() never joins it.
    QCPColorScaleOps<QCPColorScale> colorScale();
    //! A line of text centred above the rect, in its top margin, as QGroupBox::setTitle. An empty
    //! title takes no room.
    void setTitle(const QString& title);
    [[nodiscard]] QString title() const;

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
    virtual void updateLayout(UpdatePhase phase) override;
    virtual int calculateAutoMargin(QCP::MarginSide side) override;

signals:
    void rangeDragChanged(Qt::Orientations rangeDrag);
    void rangeZoomChanged(Qt::Orientations rangeZoom);
    void titleChanged(const QString& title);

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
    QPointer<QCPTextElement> mTitle;

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
    QList<QCPAxisBase*> allAxesRaw() const override;
    void updateAxesOffset(QCPAxis::AxisType type);
    void zoomAxesAt(const QPointF& pos, double horizontalFactor, double verticalFactor);
    void markAffectedLayersDirty(bool invalidate = false) override;
    // A margin or size change moves the inner rect, so every axis, grid and plottable in it draws
    // at new pixels.
    void markLayoutAppearanceDirty() override;

private:
    Q_DISABLE_COPY_MOVE(QCPAxisRect)

    // The rect's own scale while at least one user colours through it, otherwise null.
    QCPColorScale* usedColorScale() const;
    // The width a vertical scale takes in the margin, or the height of a horizontal one.
    int colorScaleExtent() const;
    void placeColorScale();
    // The height the title takes in the top margin, its own margins included; 0 without one.
    int titleExtent() const;
    void styleTitle();
    void placeTitle();
    // The scale gained or lost a user, or changed the side or width it needs: the margin moves.
    void colorScaleUsersChanged();
    // The axes raw, for the rect's own layout and range passes (K8): the public lists hand out
    // handles.
    QList<QCPAxis*> axesRaw(QCPAxis::AxisTypes types) const;
    QList<QCPAxis*> axesRaw() const;
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
    friend class QCPColorMapped; // it seeds the scale it causes this rect to make

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
