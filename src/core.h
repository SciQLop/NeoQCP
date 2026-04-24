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

#ifndef QCP_CORE_H
#define QCP_CORE_H

#include "axis/axis.h"
#include "axis/range.h"
#include "global.h"
#include "painting/paintbuffer.h"
#include "plottables/plottable.h"

#include <QPointer>
#include <QRhiWidget>
#include <functional>

class QCPPainter;
class QCPLayer;
class QCPAbstractItem;
class QCPAxis;
class QCPGraph;
class QCPLegend;
class QCPAbstractLegendItem;
class QCPSelectionRect;
class QRhi;
class QRhiCommandBuffer;
class QRhiGraphicsPipeline;
class QRhiResourceUpdateBatch;
class QRhiSampler;
class QRhiBuffer;
class QRhiShaderResourceBindings;
class QCPPlottableRhiLayer;
class QCPColormapRhiLayer;
class QCPSpanRhiLayer;
class QCPGridRhiLayer;
class QCPScatterRhiLayer;
class QCPTheme;
class QCPPipelineScheduler;
class QCPOverlay;
class QCPItemCreationState;
class QCustomPlot;

using ItemCreator = std::function<QCPAbstractItem*(QCustomPlot* plot, QCPAxis* keyAxis, QCPAxis* valueAxis)>;
using ItemPositioner = std::function<void(QCPAbstractItem* item, double anchorKey, double anchorValue, double currentKey, double currentValue)>;

class QCP_LIB_DECL QCustomPlot : public QRhiWidget
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(QRect viewport READ viewport WRITE setViewport)
    Q_PROPERTY(QPixmap background READ background WRITE setBackground)
    Q_PROPERTY(bool backgroundScaled READ backgroundScaled WRITE setBackgroundScaled)
    Q_PROPERTY(Qt::AspectRatioMode backgroundScaledMode READ backgroundScaledMode WRITE
                   setBackgroundScaledMode)
    Q_PROPERTY(QCPLayoutGrid* plotLayout READ plotLayout)
    Q_PROPERTY(bool autoAddPlottableToLegend READ autoAddPlottableToLegend WRITE
                   setAutoAddPlottableToLegend)
    Q_PROPERTY(int selectionTolerance READ selectionTolerance WRITE setSelectionTolerance)
    Q_PROPERTY(bool noAntialiasingOnDrag READ noAntialiasingOnDrag WRITE setNoAntialiasingOnDrag)
    Q_PROPERTY(Qt::KeyboardModifier multiSelectModifier READ multiSelectModifier WRITE
                   setMultiSelectModifier)
    Q_PROPERTY(QColor themeBackground READ themeBackground WRITE setThemeBackground)
    Q_PROPERTY(QColor themeForeground READ themeForeground WRITE setThemeForeground)
    Q_PROPERTY(QColor themeGrid READ themeGrid WRITE setThemeGrid)
    Q_PROPERTY(QColor themeSubGrid READ themeSubGrid WRITE setThemeSubGrid)
    Q_PROPERTY(QColor themeSelection READ themeSelection WRITE setThemeSelection)
    Q_PROPERTY(QColor themeLegendBackground READ themeLegendBackground WRITE setThemeLegendBackground)
    Q_PROPERTY(QColor themeLegendBorder READ themeLegendBorder WRITE setThemeLegendBorder)
    /// \endcond

public:
    /*!
      Defines how a layer should be inserted relative to an other layer.

      \see addLayer, moveLayer
    */
    enum LayerInsertMode
    {
        limBelow ///< Layer is inserted below other layer
        ,
        limAbove ///< Layer is inserted above other layer
    };
    Q_ENUMS(LayerInsertMode)

    /*!
      Defines with what timing the QCustomPlot surface is refreshed after a replot.

      \see replot
    */
    enum RefreshPriority
    {
        rpImmediateRefresh ///< Replots immediately. Under QRhiWidget, the
                           ///< widget repaint is always queued via update().
        ,
        rpQueuedRefresh [[deprecated("Use rpImmediateRefresh — identical under QRhiWidget")]]
            = rpImmediateRefresh ///< Deprecated. Identical to rpImmediateRefresh.
        ,
        rpRefreshHint [[deprecated("Use rpImmediateRefresh — identical under QRhiWidget")]]
            = rpImmediateRefresh ///< Deprecated. Identical to rpImmediateRefresh.
        ,
        rpQueuedReplot = 3 ///< Queues the entire replot for the next event loop iteration. This way
                           ///< multiple redundant replots can be avoided.
                           ///< (Value 3 preserves ABI; 1–2 were the former rpQueuedRefresh/rpRefreshHint.)
    };
    Q_ENUMS(RefreshPriority)

    explicit QCustomPlot(QWidget* parent = nullptr);
    virtual ~QCustomPlot() override;

    // getters:
    [[nodiscard]] QRect viewport() const { return mViewport; }

    [[nodiscard]] double bufferDevicePixelRatio() const { return mBufferDevicePixelRatio; }

    [[nodiscard]] QRhi* rhi() const { return mRhi; }
    [[nodiscard]] QCPPlottableRhiLayer* plottableRhiLayer(QCPLayer* layer);
    [[nodiscard]] QCPScatterRhiLayer* scatterRhiLayer(QCPLayer* layer);
    void registerColormapRhiLayer(QCPColormapRhiLayer* layer);
    void unregisterColormapRhiLayer(QCPColormapRhiLayer* layer);
    [[nodiscard]] QSize rhiOutputSize() const;

    [[nodiscard]] QPixmap background() const { return mBackgroundPixmap; }
    [[nodiscard]] QBrush backgroundBrush() const { return mBackgroundBrush; }

    [[nodiscard]] bool backgroundScaled() const { return mBackgroundScaled; }

    [[nodiscard]] Qt::AspectRatioMode backgroundScaledMode() const { return mBackgroundScaledMode; }

    [[nodiscard]] QCPLayoutGrid* plotLayout() const { return mPlotLayout; }

    [[nodiscard]] QCP::AntialiasedElements antialiasedElements() const { return mAntialiasedElements; }

    [[nodiscard]] QCP::AntialiasedElements notAntialiasedElements() const { return mNotAntialiasedElements; }

    [[nodiscard]] bool autoAddPlottableToLegend() const { return mAutoAddPlottableToLegend; }

    [[nodiscard]] const QCP::Interactions interactions() const { return mInteractions; }

    [[nodiscard]] int selectionTolerance() const { return mSelectionTolerance; }

    [[nodiscard]] bool noAntialiasingOnDrag() const { return mNoAntialiasingOnDrag; }

    [[nodiscard]] QCP::PlottingHints plottingHints() const { return mPlottingHints; }

    [[nodiscard]] Qt::KeyboardModifier multiSelectModifier() const { return mMultiSelectModifier; }

    [[nodiscard]] QCP::SelectionRectMode selectionRectMode() const { return mSelectionRectMode; }

    [[nodiscard]] QCPSelectionRect* selectionRect() const { return mSelectionRect; }

    // Item creation mode
    void setItemCreator(ItemCreator creator);
    [[nodiscard]] const ItemCreator& itemCreator() const { return mItemCreator; }
    void setItemPositioner(ItemPositioner positioner);
    [[nodiscard]] const ItemPositioner& itemPositioner() const { return mItemPositioner; }
    void setCreationModeEnabled(bool enabled);
    [[nodiscard]] bool creationModeEnabled() const { return mCreationModeEnabled; }
    void setCreationModifier(Qt::KeyboardModifier mod);
    [[nodiscard]] Qt::KeyboardModifier creationModifier() const { return mCreationModifier; }

    // setters:
    void setViewport(const QRect& rect);
    void setBufferDevicePixelRatio(double ratio);
    void setBackground(const QPixmap& pm);
    void setBackground(const QPixmap& pm, bool scaled,
                       Qt::AspectRatioMode mode = Qt::KeepAspectRatioByExpanding);
    void setBackground(const QBrush& brush);
    void setBackgroundScaled(bool scaled);
    void setBackgroundScaledMode(Qt::AspectRatioMode mode);
    void setAntialiasedElements(const QCP::AntialiasedElements& antialiasedElements);
    void setAntialiasedElement(QCP::AntialiasedElement antialiasedElement, bool enabled = true);
    void setNotAntialiasedElements(const QCP::AntialiasedElements& notAntialiasedElements);
    void setNotAntialiasedElement(QCP::AntialiasedElement notAntialiasedElement,
                                  bool enabled = true);
    void setAutoAddPlottableToLegend(bool on);
    void setInteractions(const QCP::Interactions& interactions);
    void setInteraction(const QCP::Interaction& interaction, bool enabled = true);
    void setSelectionTolerance(int pixels);
    void setNoAntialiasingOnDrag(bool enabled);
    void setPlottingHints(const QCP::PlottingHints& hints);
    void setPlottingHint(QCP::PlottingHint hint, bool enabled = true);
    void setMultiSelectModifier(Qt::KeyboardModifier modifier);
    void setSelectionRectMode(QCP::SelectionRectMode mode);
    void setSelectionRect(QCPSelectionRect* selectionRect);
    // theme:
    [[nodiscard]] QCPTheme* theme() const;
    void setTheme(QCPTheme* theme);
    void applyTheme();
    [[nodiscard]] QColor themeBackground() const;
    void setThemeBackground(const QColor& color);
    [[nodiscard]] QColor themeForeground() const;
    void setThemeForeground(const QColor& color);
    [[nodiscard]] QColor themeGrid() const;
    void setThemeGrid(const QColor& color);
    [[nodiscard]] QColor themeSubGrid() const;
    void setThemeSubGrid(const QColor& color);
    [[nodiscard]] QColor themeSelection() const;
    void setThemeSelection(const QColor& color);
    [[nodiscard]] QColor themeLegendBackground() const;
    void setThemeLegendBackground(const QColor& color);
    [[nodiscard]] QColor themeLegendBorder() const;
    void setThemeLegendBorder(const QColor& color);
    // overlay:
    QCPOverlay* overlay();
    // span GPU layer:
    QCPSpanRhiLayer* spanRhiLayer();
    // grid GPU layer:
    QCPGridRhiLayer* gridRhiLayer();
    // pipeline:
    [[nodiscard]] QCPPipelineScheduler* pipelineScheduler() const { return mPipelineScheduler; }
    void setMaxPipelineThreads(int count);

    // non-property methods:
    // plottable interface:
    [[nodiscard]] QCPAbstractPlottable* plottable(int index);
    [[nodiscard]] QCPAbstractPlottable* plottable();
    [[nodiscard]] bool removePlottable(QCPAbstractPlottable* plottable);
    [[nodiscard]] bool removePlottable(int index);
    int clearPlottables();
    [[nodiscard]] int plottableCount() const;
    [[nodiscard]] QList<QCPAbstractPlottable*> selectedPlottables() const;
    template <class PlottableType>
    [[nodiscard]] PlottableType* plottableAt(const QPointF& pos, bool onlySelectable = false,
                               int* dataIndex = nullptr) const;
    [[nodiscard]] QCPAbstractPlottable* plottableAt(const QPointF& pos, bool onlySelectable = false,
                                      int* dataIndex = nullptr) const;
    [[nodiscard]] bool hasPlottable(QCPAbstractPlottable* plottable) const;

    // specialized interface for QCPGraph:
    [[nodiscard]] QCPGraph* graph(int index) const;
    [[nodiscard]] QCPGraph* graph() const;
    QCPGraph* addGraph(QCPAxis* keyAxis = nullptr, QCPAxis* valueAxis = nullptr);
    [[nodiscard]] bool removeGraph(QCPGraph* graph);
    [[nodiscard]] bool removeGraph(int index);
    int clearGraphs();
    [[nodiscard]] int graphCount() const;
    [[nodiscard]] QList<QCPGraph*> selectedGraphs() const;

    // item interface:
    [[nodiscard]] QCPAbstractItem* item(int index) const;
    [[nodiscard]] QCPAbstractItem* item() const;
    [[nodiscard]] bool removeItem(QCPAbstractItem* item);
    [[nodiscard]] bool removeItem(int index);
    int clearItems();
    [[nodiscard]] int itemCount() const;
    [[nodiscard]] QList<QCPAbstractItem*> selectedItems() const;
    template <class ItemType>
    [[nodiscard]] ItemType* itemAt(const QPointF& pos, bool onlySelectable = false) const;
    [[nodiscard]] QCPAbstractItem* itemAt(const QPointF& pos, bool onlySelectable = false) const;
    [[nodiscard]] bool hasItem(QCPAbstractItem* item) const;

    // layer interface:
    [[nodiscard]] QCPLayer* layer(const QString& name) const;
    [[nodiscard]] QCPLayer* layer(int index) const;
    [[nodiscard]] QCPLayer* currentLayer() const;
    bool setCurrentLayer(const QString& name);
    bool setCurrentLayer(QCPLayer* layer);
    [[nodiscard]] int layerCount() const;
    [[nodiscard]] bool addLayer(const QString& name, QCPLayer* otherLayer = nullptr,
                  LayerInsertMode insertMode = limAbove);
    [[nodiscard]] bool removeLayer(QCPLayer* layer);
    [[nodiscard]] bool moveLayer(QCPLayer* layer, QCPLayer* otherLayer, LayerInsertMode insertMode = limAbove);

    // axis rect/layout interface:
    [[nodiscard]] int axisRectCount() const;
    [[nodiscard]] QCPAxisRect* axisRect(int index = 0) const;
    [[nodiscard]] QList<QCPAxisRect*> axisRects() const;
    [[nodiscard]] QCPLayoutElement* layoutElementAt(const QPointF& pos) const;
    [[nodiscard]] QCPAxisRect* axisRectAt(const QPointF& pos) const;
    Q_SLOT void rescaleAxes(bool onlyVisiblePlottables = false);

    [[nodiscard]] QList<QCPAxis*> selectedAxes() const;
    [[nodiscard]] QList<QCPLegend*> selectedLegends() const;
    Q_SLOT void deselectAll();

    [[nodiscard]] bool savePdf(const QString& fileName, int width = 0, int height = 0,
                 QCP::ExportPen exportPen = QCP::epAllowCosmetic,
                 const QString& pdfCreator = QString(), const QString& pdfTitle = QString());
    [[nodiscard]] bool savePng(const QString& fileName, int width = 0, int height = 0, double scale = 1.0,
                 int quality = -1, int resolution = 96,
                 QCP::ResolutionUnit resolutionUnit = QCP::ruDotsPerInch);
    [[nodiscard]] bool saveJpg(const QString& fileName, int width = 0, int height = 0, double scale = 1.0,
                 int quality = -1, int resolution = 96,
                 QCP::ResolutionUnit resolutionUnit = QCP::ruDotsPerInch);
    [[nodiscard]] bool saveBmp(const QString& fileName, int width = 0, int height = 0, double scale = 1.0,
                 int resolution = 96, QCP::ResolutionUnit resolutionUnit = QCP::ruDotsPerInch);
    [[nodiscard]] bool saveRastered(const QString& fileName, int width, int height, double scale,
                      const char* format, int quality = -1, int resolution = 96,
                      QCP::ResolutionUnit resolutionUnit = QCP::ruDotsPerInch);
    [[nodiscard]] QPixmap toPixmap(int width = 0, int height = 0, double scale = 1.0);
    void toPainter(QCPPainter* painter, int width = 0, int height = 0);
    Q_SLOT void replot(QCustomPlot::RefreshPriority refreshPriority = QCustomPlot::rpImmediateRefresh);
    [[nodiscard]] double replotTime(bool average = false) const;

    QCPAxis *xAxis, *yAxis, *xAxis2, *yAxis2;
    QCPLegend* legend;

signals:
    void mouseDoubleClick(QMouseEvent* event);
    void mousePress(QMouseEvent* event);
    void mouseMove(QMouseEvent* event);
    void mouseRelease(QMouseEvent* event);
    void mouseWheel(QWheelEvent* event);

    void plottableClick(QCPAbstractPlottable* plottable, int dataIndex, QMouseEvent* event);
    void plottableDoubleClick(QCPAbstractPlottable* plottable, int dataIndex, QMouseEvent* event);
    void itemClick(QCPAbstractItem* item, QMouseEvent* event);
    void itemDoubleClick(QCPAbstractItem* item, QMouseEvent* event);
    void axisClick(QCPAxis* axis, QCPAxis::SelectablePart part, QMouseEvent* event);
    void axisDoubleClick(QCPAxis* axis, QCPAxis::SelectablePart part, QMouseEvent* event);
    void legendClick(QCPLegend* legend, QCPAbstractLegendItem* item, QMouseEvent* event);
    void legendDoubleClick(QCPLegend* legend, QCPAbstractLegendItem* item, QMouseEvent* event);

    void selectionChangedByUser();
    void beforeReplot();
    void afterLayout();
    void afterReplot();

    void itemCreated(QCPAbstractItem* item);
    void itemCanceled();

protected:
    // property members:
    QRect mViewport;
    double mBufferDevicePixelRatio;
    QCPLayoutGrid* mPlotLayout;
    bool mAutoAddPlottableToLegend;
    QList<QCPAbstractPlottable*> mPlottables;
    QList<QCPGraph*>
        mGraphs; // extra list of plottables also in mPlottables that are of type QCPGraph
    QList<QCPAbstractItem*> mItems;
    QList<QCPLayer*> mLayers;
    QCP::AntialiasedElements mAntialiasedElements, mNotAntialiasedElements;
    QCP::Interactions mInteractions;
    int mSelectionTolerance;
    bool mNoAntialiasingOnDrag;
    QBrush mBackgroundBrush;
    QPixmap mBackgroundPixmap;
    QPixmap mScaledBackgroundPixmap;
    bool mBackgroundScaled;
    Qt::AspectRatioMode mBackgroundScaledMode;
    QCPLayer* mCurrentLayer;
    QCP::PlottingHints mPlottingHints;
    Qt::KeyboardModifier mMultiSelectModifier;
    QCP::SelectionRectMode mSelectionRectMode;
    QCPSelectionRect* mSelectionRect;
    QCPTheme* mOwnedTheme;
    QPointer<QCPTheme> mTheme;
    bool mThemeDirty;
    // non-property members:
    QList<QSharedPointer<QCPAbstractPaintBuffer>> mPaintBuffers;
    QPoint mMousePressPos;
    bool mMouseHasMoved;
    QPointer<QCPLayerable> mMouseEventLayerable;
    QPointer<QCPLayerable> mMouseSignalLayerable;
    QVariant mMouseEventLayerableDetails;
    QVariant mMouseSignalLayerableDetails;
    bool mReplotting;
    bool mReplotQueued;
    double mReplotTime, mReplotTimeAverage;
    // RHI compositing resources (mRhi cached from rhi() in initialize(); Qt docs only guarantee
    // rhi() during initialize/render/releaseResources, but the pointer is stable in practice):
    QRhi* mRhi = nullptr;
    QRhiGraphicsPipeline* mCompositePipeline = nullptr;
    QRhiShaderResourceBindings* mLayoutSrb = nullptr;
    QRhiSampler* mSampler = nullptr;
    QRhiBuffer* mQuadVertexBuffer = nullptr;
    QRhiBuffer* mQuadIndexBuffer = nullptr;
    QRhiBuffer* mCompositeUbo = nullptr;
    bool mRhiInitialized = false;
    QMap<QCPLayer*, QCPPlottableRhiLayer*> mPlottableRhiLayers;
    QMap<QCPLayer*, QCPScatterRhiLayer*> mScatterRhiLayers;
    QSet<QCPColormapRhiLayer*> mColormapRhiLayers;
    QCPSpanRhiLayer* mSpanRhiLayer = nullptr;
    QCPGridRhiLayer* mGridRhiLayer = nullptr;
    QCPPipelineScheduler* mPipelineScheduler = nullptr;
    QCPOverlay* mOverlay = nullptr;
    ItemCreator mItemCreator;
    ItemPositioner mItemPositioner;
    bool mCreationModeEnabled = false;
    Qt::KeyboardModifier mCreationModifier = Qt::ShiftModifier;
    QCPItemCreationState* mCreationState = nullptr;
    // reimplemented virtual methods:
    virtual QSize minimumSizeHint() const override;
    virtual QSize sizeHint() const override;
    void initialize(QRhiCommandBuffer* cb) override;
    void render(QRhiCommandBuffer* cb) override;
    void releaseResources() override;
    void ensureCompositePipeline();
    void uploadLayerTextures(QRhiResourceUpdateBatch* updates, const QSize& outputSize);
    void executeRenderPass(QRhiCommandBuffer* cb, QRhiResourceUpdateBatch* updates,
                           const QSize& outputSize);
    virtual void resizeEvent(QResizeEvent* event) override;
    virtual void mouseDoubleClickEvent(QMouseEvent* event) override;
    virtual void mousePressEvent(QMouseEvent* event) override;
    virtual void mouseMoveEvent(QMouseEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;
    virtual void wheelEvent(QWheelEvent* event) override;
    virtual void keyPressEvent(QKeyEvent* event) override;

    // introduced virtual methods:
    virtual void draw(QCPPainter* painter);
    virtual void updateLayout();
    virtual void axisRemoved(QCPAxis* axis);
    virtual void legendRemoved(QCPLegend* legend);
    Q_SLOT virtual void processRectSelection(QRect rect, QMouseEvent* event);
    Q_SLOT virtual void processRectZoom(QRect rect, QMouseEvent* event);
    Q_SLOT virtual void processPointSelection(QMouseEvent* event);

    // non-virtual methods:
    bool registerPlottable(QCPAbstractPlottable* plottable);
    bool registerGraph(QCPGraph* graph);
    bool registerItem(QCPAbstractItem* item);
    void updateLayerIndices() const;
    QCPLayerable* layerableAt(const QPointF& pos, bool onlySelectable,
                              QVariant* selectionDetails = nullptr) const;
    QList<QCPLayerable*> layerableListAt(const QPointF& pos, bool onlySelectable,
                                         QList<QVariant>* selectionDetails = nullptr) const;
    void drawBackground(QCPPainter* painter);
    void connectThemeSignal();
    void setupPaintBuffers();
    QCPAbstractPaintBuffer* createPaintBuffer(const QString& layerName);
    bool hasInvalidatedPaintBuffers();
    void ensureAtLeastOneBufferDirty();
    friend class QCPLegend;
    friend class QCPAxis;
    friend class QCPLayer;
    friend class TestPaintBuffer;
    friend class QCPAxisRect;
    friend class QCPAbstractPlottable;
    friend class QCPGraph;
    friend class QCPAbstractItem;
};
Q_DECLARE_METATYPE(QCustomPlot::LayerInsertMode)
Q_DECLARE_METATYPE(QCustomPlot::RefreshPriority)

// implementation of template functions:

/*!
  Returns the plottable at the pixel position \a pos. The plottable type (a QCPAbstractPlottable
  subclass) that shall be taken into consideration can be specified via the template parameter.

  Plottables that only consist of single lines (like graphs) have a tolerance band around them, see
  \ref setSelectionTolerance. If multiple plottables come into consideration, the one closest to \a
  pos is returned.

  If \a onlySelectable is true, only plottables that are selectable
  (QCPAbstractPlottable::setSelectable) are considered.

  if \a dataIndex is non-null, it is set to the index of the plottable's data point that is closest
  to \a pos.

  If there is no plottable of the specified type at \a pos, returns \c nullptr.

  \see itemAt, layoutElementAt
*/
template <class PlottableType>
PlottableType* QCustomPlot::plottableAt(const QPointF& pos, bool onlySelectable,
                                        int* dataIndex) const
{
    PlottableType* resultPlottable = nullptr;
    QVariant resultDetails;
    double resultDistance
        = mSelectionTolerance; // only regard clicks with distances smaller than mSelectionTolerance
                               // as selections, so initialize with that value

    for (QCPAbstractPlottable* plottable : mPlottables)
    {
        PlottableType* currentPlottable = qobject_cast<PlottableType*>(plottable);
        if (!currentPlottable
            || (onlySelectable
                && !currentPlottable->selectable())) // we could have also passed onlySelectable to
                                                     // the selectTest function, but checking here
                                                     // is faster, because we have access to
                                                     // QCPAbstractPlottable::selectable
            continue;
        if (currentPlottable->clipRect().contains(
                pos.toPoint())) // only consider clicks where the plottable is actually visible
        {
            QVariant details;
            double currentDistance
                = currentPlottable->selectTest(pos, false, dataIndex ? &details : nullptr);
            if (currentDistance >= 0 && currentDistance < resultDistance)
            {
                resultPlottable = currentPlottable;
                resultDetails = details;
                resultDistance = currentDistance;
            }
        }
    }

    if (resultPlottable && dataIndex)
    {
        QCPDataSelection sel = resultDetails.value<QCPDataSelection>();
        if (!sel.isEmpty())
            *dataIndex = sel.dataRange(0).begin();
    }
    return resultPlottable;
}

/*!
  Returns the item at the pixel position \a pos. The item type (a QCPAbstractItem subclass) that
  shall be taken into consideration can be specified via the template parameter. Items that only
  consist of single lines (e.g. \ref QCPItemLine or \ref QCPItemCurve) have a tolerance band around
  them, see \ref setSelectionTolerance. If multiple items come into consideration, the one closest
  to \a pos is returned.

  If \a onlySelectable is true, only items that are selectable (QCPAbstractItem::setSelectable) are
  considered.

  If there is no item at \a pos, returns \c nullptr.

  \see plottableAt, layoutElementAt
*/
template <class ItemType>
ItemType* QCustomPlot::itemAt(const QPointF& pos, bool onlySelectable) const
{
    ItemType* resultItem = nullptr;
    double resultDistance
        = mSelectionTolerance; // only regard clicks with distances smaller than mSelectionTolerance
                               // as selections, so initialize with that value

    for (QCPAbstractItem* item : mItems)
    {
        ItemType* currentItem = qobject_cast<ItemType*>(item);
        if (!currentItem
            || (onlySelectable
                && !currentItem
                        ->selectable())) // we could have also passed onlySelectable to the
                                         // selectTest function, but checking here is faster,
                                         // because we have access to QCPAbstractItem::selectable
            continue;
        if (!currentItem->clipToAxisRect()
            || currentItem->clipRect().contains(
                pos.toPoint())) // only consider clicks inside axis cliprect of the item if actually
                                // clipped to it
        {
            double currentDistance = currentItem->selectTest(pos, false);
            if (currentDistance >= 0 && currentDistance < resultDistance)
            {
                resultItem = currentItem;
                resultDistance = currentDistance;
            }
        }
    }

    return resultItem;
}


#endif // QCP_CORE_H
