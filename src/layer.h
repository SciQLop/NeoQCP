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

#ifndef QCP_LAYER_H
#define QCP_LAYER_H

#include <QSet>
#include "spatial-index.h"

#include <cstdint>
#include <optional>
#include <vector>
#include <QPointer>

#include "global.h"
#include "handle-ops.h"
Q_MOC_INCLUDE("core.h") // Qt6 needs this if using forward declared types in Q_PROPERTY
#include "painting/paintbuffer.h"

class QCPPainter;
class QCustomPlot;
class QCPLayerable;
class QCPLayoutElement;
class QCPLayout;

class QCP_LIB_DECL QCPLayer : public QObject
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(QCustomPlot* parentPlot READ parentPlot)
    Q_PROPERTY(QString name READ name)
    Q_PROPERTY(int index READ index)
    Q_PROPERTY(QList<QCPLayerable*> children READ children)
    Q_PROPERTY(bool visible READ visible WRITE setVisible)
    Q_PROPERTY(LayerMode mode READ mode WRITE setMode)
    /// \endcond

public:
    /*!
      Defines the different rendering modes of a layer. Depending on the mode, certain layers can be
      replotted individually, without the need to replot (possibly complex) layerables on other
      layers.

      \see setMode
    */
    enum LayerMode
    {
        lmLogical ///< Layer is used only for rendering order, and shares paint buffer with all
                  ///< other adjacent logical layers.
        ,
        lmBuffered ///< Layer has its own paint buffer and may be replotted individually (see \ref
                   ///< replot).
    };
    Q_ENUM(LayerMode)

    QCPLayer(QCustomPlot* parentPlot, const QString& layerName);
    virtual ~QCPLayer();

    // getters:
    [[nodiscard]] QCustomPlot* parentPlot() const { return mParentPlot; }

    [[nodiscard]] QString name() const { return mName; }

    [[nodiscard]] int index() const { return mIndex; }

    [[nodiscard]] QList<QCPLayerable*> children() const { return mChildren; }

    // One flag per child: 0 means the child is indexed and lies outside \a pixelArea on screen, so
    // it need not be drawn or hit tested; 1 means "look at it" (unindexed, on the side list, or a
    // hit). Const because a lookup may rebuild the index.
    const std::vector<std::uint8_t>& itemCandidates(const QRectF& pixelArea) const;

    [[nodiscard]] bool visible() const { return mVisible; }

    [[nodiscard]] LayerMode mode() const { return mMode; }

    [[nodiscard]] QPointF pixelOffset() const;
    [[nodiscard]] bool canSkipRepaintForTranslation() const;
    [[nodiscard]] bool canTranslateInsteadOfRepaint() const;
    void invalidatePaintBuffer();

    // setters:
    void setVisible(bool visible);
    void setMode(LayerMode mode);

    // non-virtual methods:
    void replot();
    // Marks the layer for the next turn's repaint; a pan may still shift its old frame. GUI thread
    // only.
    void markDirty();

protected:
    // property members:
    QCustomPlot* mParentPlot;
    QString mName;
    int mIndex;
    QList<QCPLayerable*> mChildren;
    bool mVisible;
    LayerMode mMode;

    // non-property members:
    QWeakPointer<QCPAbstractPaintBuffer> mPaintBuffer;
    quint64 mDirtyGeneration = 0;
    quint64 mDrawnGeneration = 0;

    // non-virtual methods:
    void noteDirtied();
    [[nodiscard]] bool dirtiedAfterDraw() const { return mDirtyGeneration > mDrawnGeneration; }
    void draw(QCPPainter* painter);
    void drawToPaintBuffer();
    void addChild(QCPLayerable* layerable, bool prepend);
    void removeChild(QCPLayerable* layerable);
    void removeChildren(const QSet<QCPLayerable*>& dying);

private:
    // The per-child item index: one packed R-tree per (key axis, value axis) pair, in data
    // coordinates so that it survives range changes. Kinds are 0 unindexed, 1 indexed, 2 side
    // (extent changed since the build: tested one by one).
    void rebuildItemIndex() const;
    mutable std::vector<std::uint8_t> mItemKind;
    mutable std::vector<int> mItemGroup;
    mutable std::vector<std::uint8_t> mItemCandidates;
    struct ItemIndexGroup
    {
        // QPointers: removeAxis() deletes an axis and this group outlives it until the next rebuild.
        // A dead axis makes the group unusable, and its items are then simply not culled.
        QPointer<QCPAxis> keyAxis;
        QPointer<QCPAxis> valueAxis;
        QCPSpatialIndex index;
        double maxPad = 0.0;
    };
    mutable std::vector<ItemIndexGroup> mItemGroups;
    mutable std::vector<int> mItemSide;
    mutable bool mItemIndexDirty = true;
    mutable int mItemIndexBuilds = 0;

    void invalidateItemIndex();
    void noteItemExtentChanged(QCPAbstractItem* item);

    friend class QCPAbstractItem;
    friend class QCPAxis; // a scale type change invalidates the item index
    friend class TestItemIndex;

    Q_DISABLE_COPY(QCPLayer)

    friend class QCustomPlot;
    friend class QCPLayerable;
    friend class TestPaintBuffer;
    friend class TestBusyIndicator;
    friend class TestQCPLegend;
    friend class TestPlottableGroup;
    friend class TestOverlay;
    friend class TestHistogramPlottable;
    friend class TestPipeline;
    friend class TestRepaint;
};
Q_DECLARE_METATYPE(QCPLayer::LayerMode)

class QCP_LIB_DECL QCPLayerable : public QObject
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(bool visible READ visible WRITE setVisible)
    Q_PROPERTY(QCustomPlot* parentPlot READ parentPlot)
    Q_PROPERTY(QCPLayerable* parentLayerable READ parentLayerable)
    Q_PROPERTY(QCPLayer* layer READ layer WRITE setLayer NOTIFY layerChanged)
    Q_PROPERTY(bool antialiased READ antialiased WRITE setAntialiased)
    Q_PROPERTY(bool mouseTransparent READ mouseTransparent WRITE setMouseTransparent)
    /// \endcond

public:
    // The handle of a layerable that declares neither Handle nor its own OpsFor (see QCPObjectOps).
    template <typename T>
    using OpsFor = QCPLayerableOps<T>;

    QCPLayerable(QCustomPlot* plot, QString targetLayer = QString(),
                 QCPLayerable* parentLayerable = nullptr);
    virtual ~QCPLayerable();

    // getters:
    [[nodiscard]] bool visible() const { return mVisible; }

    [[nodiscard]] QCustomPlot* parentPlot() const { return mParentPlot; }

    [[nodiscard]] QCPLayerable* parentLayerable() const { return mParentLayerable.data(); }

    [[nodiscard]] QCPLayer* layer() const { return mLayer; }

    [[nodiscard]] bool antialiased() const { return mAntialiased; }

    // When true, this layerable is skipped by hit-testing (layerableListAt), so
    // it never captures mouse clicks/hover nor shadows interactive elements
    // beneath it. Use for purely decorative overlays (e.g. a crosshair) that
    // would otherwise steal clicks from the legend/plottables below.
    [[nodiscard]] bool mouseTransparent() const { return mMouseTransparent; }

    // setters:
    void setVisible(bool on);
    Q_SLOT bool setLayer(QCPLayer* layer);
    bool setLayer(const QString& layerName);
    void setAntialiased(bool enabled);
    void setMouseTransparent(bool transparent) { mMouseTransparent = transparent; }

    // introduced virtual methods:
    virtual double selectTest(const QPointF& pos, bool onlySelectable,
                              QVariant* details = nullptr) const;

    // non-property methods:
    [[nodiscard]] bool realVisibility() const;
    // Schedules a full repaint of this layerable's layer next turn; a no-op without a plot or a
    // layer. GUI thread only. Virtual so an item can tell its layer that its extent may have
    // changed: an item that moves leaves the index and is tested one by one until the next rebuild.
    virtual void markDirty();

signals:
    void layerChanged(QCPLayer* newLayer);
    void visibleChanged(bool visible);

protected:
    // property members:
    bool mVisible;
    QCustomPlot* mParentPlot;
    QPointer<QCPLayerable> mParentLayerable;
    QCPLayer* mLayer;
    bool mAntialiased;
    bool mMouseTransparent = false;

    // introduced virtual methods:
    virtual void parentPlotInitialized(QCustomPlot* parentPlot);
    virtual QCP::Interaction selectionCategory() const;
    virtual QRect clipRect() const;
    virtual void applyDefaultAntialiasingHint(QCPPainter* painter) const = 0;
    virtual void draw(QCPPainter* painter) = 0;
    // selection events:
    virtual void selectEvent(QMouseEvent* event, bool additive, const QVariant& details,
                             bool* selectionStateChanged);
    virtual void deselectEvent(bool* selectionStateChanged);
    // low-level mouse events:
    virtual void mousePressEvent(QMouseEvent* event, const QVariant& details);
    virtual void mouseMoveEvent(QMouseEvent* event, const QPointF& startPos);
    virtual void mouseReleaseEvent(QMouseEvent* event, const QPointF& startPos);
    virtual void mouseDoubleClickEvent(QMouseEvent* event, const QVariant& details);
    virtual void wheelEvent(QWheelEvent* event);

    // non-property methods:
    void initializeParentPlot(QCustomPlot* parentPlot);
    void setParentLayerable(QCPLayerable* parentLayerable);
    bool moveToLayer(QCPLayer* layer, bool prepend);
    void emitLayerChanged();
    void applyAntialiasingHint(QCPPainter* painter, bool localAntialiased,
                               QCP::AntialiasedElement overrideElement) const;

private:
    Q_DISABLE_COPY(QCPLayerable)

    friend class QCustomPlot;
    friend class QCPLayer;
    friend class QCPAxisRect;
};

#endif // QCP_LAYER_H
