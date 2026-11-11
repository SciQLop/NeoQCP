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

#include "alignment-group.h"
#include "core.h"
#include <memory>
#include <utility>
#include "misuse.h"
#include "plottables/plottable-bars.h"
#include "plottables/plottable-group.h"
#include "item-creation-state.h"
#include "overlay.h"

#include "Profiling.hpp"
#include "axis/axis.h"
#include "coordinate-system.h"
#include "items/item.h"
#include "items/item-text.h"
#include "items/item-tracer.h"
#include "polar/angularaxis.h"
#include "polar/polargraph.h"
#include "layer.h"
#include "layoutelements/area.h"
#include "layoutelements/layoutelement-axisrect.h"
#include "layoutelements/layoutelement-legend.h"
#include "painting/painter.h"
#include "painting/paintbuffer.h"
#include "painting/paintbuffer-pixmap.h"
#include "painting/paintbuffer-rhi.h"
#include "painting/plottable-rhi-layer.h"
#include "painting/grid-rhi-layer.h"
#include "painting/scatter-rhi-layer.h"
#include <QSet>
#include <rhi/qrhi.h>
#include "embedded_shaders.h"
#include "painting/rhi-utils.h"
#include "plottables/plottable.h"
#include "plottables/plottable-bars.h"
#include "plottables/plottable-colormap.h"
#include "plottables/plottable-colormap2.h"
#include "plottables/plottable-graph.h"
#include "plottables/plottable-histogram2d.h"
#include "selectionrect.h"
#include "theme.h"
#include "layoutelements/layoutelement-textelement.h"
#include "layoutelements/layoutelement-colorscale.h"

#include <QTimer>
#include "datasource/pipeline-scheduler.h"

#include <atomic>

quint64 qcp::nextFrameEpoch()
{
    // One counter for the process: a plot's address may be reused after it is deleted, an epoch may
    // not. See QCustomPlot::frameEpoch().
    static std::atomic<quint64> counter { 0 };
    return ++counter;
}

// Selection and input handlers call into layerables whose signals reach user slots, and a slot may
// delete any layerable: these loops walk guarded snapshots instead of the live lists.
namespace
{
using qcp::guarded;

// The layerables a deselect pass must visit, guarded because a selectionChanged slot may delete
// any of them. Only those that may be selected: guarding all of them was most of a click with
// thousands of items.
// simplify: still one virtual call per child, ~15 us per click at 10 000 items; a plot-wide set of
// selected layerables is the upgrade if clicks over 100 000 items matter.
QList<QPointer<QCPLayerable>> guardedSelectedLayerables(const QList<QCPLayer*>& layers,
                                                       const QCPLayerable* kept = nullptr)
{
    QList<QPointer<QCPLayerable>> result;
    for (auto* layer : layers)
        for (QCPLayerable* layerable : layer->children())
            if (layerable != kept && layerable->mayBeSelected())
                result.append(layerable);
    return result;
}
} // namespace


////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCustomPlot
////////////////////////////////////////////////////////////////////////////////////////////////////

/*! \class QCustomPlot

  \brief The central class of the library. This is the QWidget which displays the plot and
  interacts with the user.

  For tutorials on how to use QCustomPlot, see the website\n
  https://www.qcustomplot.com/
*/

/* start of documentation of inline functions */

/*! \fn QCPSelectionRect *QCustomPlot::selectionRect() const

  Allows access to the currently used QCPSelectionRect instance (or subclass thereof), that is used
  to handle and draw selection rect interactions (see \ref setSelectionRectMode).

  Do not keep the returned pointer across a signal: a slot may delete or replace the rect. Read it
  again instead; it is null once the rect is gone.

  \see setSelectionRect
*/

/* end of documentation of inline functions */
/* start of documentation of signals */

/*! \fn void QCustomPlot::mouseDoubleClick(QMouseEvent *event)

  This signal is emitted when the QCustomPlot receives a mouse double click event.

  Like every input signal, it is delivered when QCustomPlot has handled the event, before Qt goes on
  with it, so a slot may delete the plot; \a event is the event itself.
*/

/*! \fn void QCustomPlot::mousePress(QMouseEvent *event)

  This signal is emitted when the QCustomPlot receives a mouse press event.

  Like every input signal, it is delivered when QCustomPlot has handled the event, before Qt goes on
  with it, so a slot may delete the plot; \a event is the event itself. Unlike in 2.x, a slot can
  no longer change how this press drags (\ref QCPAxisRect::setRangeDragAxes applies from the next
  press); an event filter on the plot sees the press first.
*/

/*! \fn void QCustomPlot::mouseMove(QMouseEvent *event)

  This signal is emitted when the QCustomPlot receives a mouse move event.

  Like every input signal, it is delivered when QCustomPlot has handled the event, before Qt goes on
  with it, so a slot may delete the plot; \a event is the event itself.
*/

/*! \fn void QCustomPlot::mouseRelease(QMouseEvent *event)

  This signal is emitted when the QCustomPlot receives a mouse release event.

  Like every input signal, it is delivered when QCustomPlot has handled the event, before Qt goes on
  with it, so a slot may delete the plot; \a event is the event itself.
*/

/*! \fn void QCustomPlot::mouseWheel(QMouseEvent *event)

  This signal is emitted when the QCustomPlot receives a mouse wheel event.

  Like every input signal, it is delivered when QCustomPlot has handled the event, before Qt goes on
  with it, so a slot may delete the plot; \a event is the event itself. Unlike in 2.x, a slot can
  no longer change how this wheel step zooms, but \c event->ignore() now passes the wheel on to
  the parent widget (once Qt::WA_NoMousePropagation, which QCustomPlot sets, is cleared).
*/

/*! \fn void QCustomPlot::plottableClick(QCPAbstractPlottable *plottable, int dataIndex, QMouseEvent
  *event)

  This signal is emitted when a plottable is clicked.

  \a event is the mouse event that caused the click and \a plottable is the plottable that received
  the click. The parameter \a dataIndex indicates the data point that was closest to the click
  position.

  \see plottableDoubleClick
*/

/*! \fn void QCustomPlot::plottableDoubleClick(QCPAbstractPlottable *plottable, int dataIndex,
  QMouseEvent *event)

  This signal is emitted when a plottable is double clicked.

  \a event is the mouse event that caused the click and \a plottable is the plottable that received
  the click. The parameter \a dataIndex indicates the data point that was closest to the click
  position.

  \see plottableClick
*/

/*! \fn void QCustomPlot::itemClick(QCPAbstractItem *item, QMouseEvent *event)

  This signal is emitted when an item is clicked.

  \a event is the mouse event that caused the click and \a item is the item that received the
  click.

  \see itemDoubleClick
*/

/*! \fn void QCustomPlot::itemDoubleClick(QCPAbstractItem *item, QMouseEvent *event)

  This signal is emitted when an item is double clicked.

  \a event is the mouse event that caused the click and \a item is the item that received the
  click.

  \see itemClick
*/

/*! \fn void QCustomPlot::axisClick(QCPAxis *axis, QCPAxis::SelectablePart part, QMouseEvent *event)

  This signal is emitted when an axis is clicked.

  \a event is the mouse event that caused the click, \a axis is the axis that received the click and
  \a part indicates the part of the axis that was clicked.

  \see axisDoubleClick
*/

/*! \fn void QCustomPlot::axisDoubleClick(QCPAxis *axis, QCPAxis::SelectablePart part, QMouseEvent
  *event)

  This signal is emitted when an axis is double clicked.

  \a event is the mouse event that caused the click, \a axis is the axis that received the click and
  \a part indicates the part of the axis that was clicked.

  \see axisClick
*/

/*! \fn void QCustomPlot::legendClick(QCPLegend *legend, QCPAbstractLegendItem *item, QMouseEvent
  *event)

  This signal is emitted when a legend (item) is clicked.

  \a event is the mouse event that caused the click, \a legend is the legend that received the
  click and \a item is the legend item that received the click. If only the legend and no item is
  clicked, \a item is \c nullptr. This happens for a click inside the legend padding or the space
  between two items.

  \see legendDoubleClick
*/

/*! \fn void QCustomPlot::legendDoubleClick(QCPLegend *legend,  QCPAbstractLegendItem *item,
  QMouseEvent *event)

  This signal is emitted when a legend (item) is double clicked.

  \a event is the mouse event that caused the click, \a legend is the legend that received the
  click and \a item is the legend item that received the click. If only the legend and no item is
  clicked, \a item is \c nullptr. This happens for a click inside the legend padding or the space
  between two items.

  \see legendClick
*/

/*! \fn void QCustomPlot::selectionChangedByUser()

  This signal is emitted after the user has changed the selection in the QCustomPlot, e.g. by
  clicking. It is not emitted when the selection state of an object has changed programmatically by
  a direct call to <tt>setSelected()</tt>/<tt>setSelection()</tt> on an object or by calling \ref
  deselectAll.

  In addition to this signal, selectable objects also provide individual signals, for example \ref
  QCPAxis::selectionChanged or \ref QCPAbstractPlottable::selectionChanged. Note that those signals
  are emitted even if the selection state is changed programmatically.

  See the documentation of \ref setInteractions for details about the selection mechanism.

  \see selectedPlottables, selectedGraphs, selectedItems, selectedAxes, selectedLegends
*/

/*! \fn void QCustomPlot::beforeReplot()

  This signal is emitted immediately before a replot takes place (caused by a call to the slot \ref
  replot). A slot may delete the plot; a \ref replot called from it does nothing.

  It is safe to mutually connect the replot slot with this signal on two QCustomPlots to make them
  replot synchronously, it won't cause an infinite recursion.

  \see replot, afterReplot, afterLayout
*/

/*! \fn void QCustomPlot::afterLayout()

  This signal is emitted immediately after the layout step has been completed, which occurs right
  before drawing the plot. This is typically during a call to \ref replot, and in such cases this
  signal is emitted in between the signals \ref beforeReplot and \ref afterReplot. Unlike those
  signals however, this signal is also emitted during off-screen painting, such as when calling
  \ref toPixmap or \ref savePdf.

  The layout step queries all layouts and layout elements in the plot for their proposed size and
  arranges the objects accordingly as preparation for the subsequent drawing step. Through this
  signal, you have the opportunity to update certain things in your plot that depend crucially on
  the exact dimensions/positioning of layout elements such as axes and axis rects.

  A slot may delete the plot; a \ref replot called from it does nothing.

  \warning However, changing any parameters of this QCustomPlot instance which would normally
  affect the layouting (e.g. axis range order of magnitudes, tick label sizes, etc.) will not issue
  a second run of the layout step. It will propagate directly to the draw step and may cause
  graphical inconsistencies such as overlapping objects, if sizes or positions have changed.

  \see updateLayout, beforeReplot, afterReplot
*/

/*! \fn void QCustomPlot::afterReplot()

  This signal is emitted after a replot has taken place (caused by a call to the slot \ref
  replot).

  It is delivered when the outermost NeoQCP call returns, after the plot is drawn, so a slot may
  delete the plot; what a slot changes shows in the next replot. A \ref replot called from its slot
  does nothing.

  It is safe to mutually connect the replot slot with this signal on two QCustomPlots to make them
  replot synchronously, it won't cause an infinite recursion.

  \see replot, beforeReplot, afterLayout
*/

/* end of documentation of signals */
/* start of documentation of public members */

/*! \var QCPAxis *QCustomPlot::xAxis

  A pointer to the primary x Axis (bottom) of the main axis rect of the plot.

  QCustomPlot offers convenient pointers to the axes (\ref xAxis, \ref yAxis, \ref xAxis2, \ref
  yAxis2) and the \ref legend. They make it very easy working with plots that only have a single
  axis rect and at most one axis at each axis rect side. If you use \link thelayoutsystem the
  layout system\endlink to add multiple axis rects or multiple axes to one side, use the \ref
  QCPAxisRect::axis interface to access the new axes. If one of the four default axes or the
  default legend is removed due to manipulation of the layout system (e.g. by removing the main
  axis rect), the corresponding pointers become \c nullptr.

  If an axis convenience pointer is currently \c nullptr and a new axis rect or a corresponding
  axis is added in the place of the main axis rect, QCustomPlot resets the convenience pointers to
  the according new axes. Similarly the \ref legend convenience pointer will be reset if a legend
  is added after the main legend was removed before.
*/

/*! \var QCPAxis *QCustomPlot::yAxis

  A pointer to the primary y Axis (left) of the main axis rect of the plot.

  QCustomPlot offers convenient pointers to the axes (\ref xAxis, \ref yAxis, \ref xAxis2, \ref
  yAxis2) and the \ref legend. They make it very easy working with plots that only have a single
  axis rect and at most one axis at each axis rect side. If you use \link thelayoutsystem the
  layout system\endlink to add multiple axis rects or multiple axes to one side, use the \ref
  QCPAxisRect::axis interface to access the new axes. If one of the four default axes or the
  default legend is removed due to manipulation of the layout system (e.g. by removing the main
  axis rect), the corresponding pointers become \c nullptr.

  If an axis convenience pointer is currently \c nullptr and a new axis rect or a corresponding
  axis is added in the place of the main axis rect, QCustomPlot resets the convenience pointers to
  the according new axes. Similarly the \ref legend convenience pointer will be reset if a legend
  is added after the main legend was removed before.
*/

/*! \var QCPAxis *QCustomPlot::xAxis2

  A pointer to the secondary x Axis (top) of the main axis rect of the plot. Secondary axes are
  invisible by default. Use QCPAxis::setVisible to change this (or use \ref
  QCPAxisRect::setupFullAxesBox).

  QCustomPlot offers convenient pointers to the axes (\ref xAxis, \ref yAxis, \ref xAxis2, \ref
  yAxis2) and the \ref legend. They make it very easy working with plots that only have a single
  axis rect and at most one axis at each axis rect side. If you use \link thelayoutsystem the
  layout system\endlink to add multiple axis rects or multiple axes to one side, use the \ref
  QCPAxisRect::axis interface to access the new axes. If one of the four default axes or the
  default legend is removed due to manipulation of the layout system (e.g. by removing the main
  axis rect), the corresponding pointers become \c nullptr.

  If an axis convenience pointer is currently \c nullptr and a new axis rect or a corresponding
  axis is added in the place of the main axis rect, QCustomPlot resets the convenience pointers to
  the according new axes. Similarly the \ref legend convenience pointer will be reset if a legend
  is added after the main legend was removed before.
*/

/*! \var QCPAxis *QCustomPlot::yAxis2

  A pointer to the secondary y Axis (right) of the main axis rect of the plot. Secondary axes are
  invisible by default. Use QCPAxis::setVisible to change this (or use \ref
  QCPAxisRect::setupFullAxesBox).

  QCustomPlot offers convenient pointers to the axes (\ref xAxis, \ref yAxis, \ref xAxis2, \ref
  yAxis2) and the \ref legend. They make it very easy working with plots that only have a single
  axis rect and at most one axis at each axis rect side. If you use \link thelayoutsystem the
  layout system\endlink to add multiple axis rects or multiple axes to one side, use the \ref
  QCPAxisRect::axis interface to access the new axes. If one of the four default axes or the
  default legend is removed due to manipulation of the layout system (e.g. by removing the main
  axis rect), the corresponding pointers become \c nullptr.

  If an axis convenience pointer is currently \c nullptr and a new axis rect or a corresponding
  axis is added in the place of the main axis rect, QCustomPlot resets the convenience pointers to
  the according new axes. Similarly the \ref legend convenience pointer will be reset if a legend
  is added after the main legend was removed before.
*/

/*! \var QCPLegend *QCustomPlot::legend

  A pointer to the default legend of the main axis rect. The legend is invisible by default. Use
  QCPLegend::setVisible to change this.

  QCustomPlot offers convenient pointers to the axes (\ref xAxis, \ref yAxis, \ref xAxis2, \ref
  yAxis2) and the \ref legend. They make it very easy working with plots that only have a single
  axis rect and at most one axis at each axis rect side. If you use \link thelayoutsystem the
  layout system\endlink to add multiple legends to the plot, use the layout system interface to
  access the new legend. For example, legends can be placed inside an axis rect's \ref
  QCPAxisRect::insetLayout "inset layout", and must then also be accessed via the inset layout. If
  the default legend is removed due to manipulation of the layout system (e.g. by removing the main
  axis rect), the corresponding pointer becomes \c nullptr.

  If an axis convenience pointer is currently \c nullptr and a new axis rect or a corresponding
  axis is added in the place of the main axis rect, QCustomPlot resets the convenience pointers to
  the according new axes. Similarly the \ref legend convenience pointer will be reset if a legend
  is added after the main legend was removed before.
*/

/* end of documentation of public members */

/*!
  Constructs a QCustomPlot and sets reasonable default values.
*/
QCustomPlot::QCustomPlot(QWidget* parent)
        : QRhiWidget(parent)
        , xAxis(nullptr)
        , yAxis(nullptr)
        , xAxis2(nullptr)
        , yAxis2(nullptr)
        , legend(nullptr)
        , mBufferDevicePixelRatio(1.0)
        , // will be adapted to true value below
        mPlotLayout(nullptr)
        , mAutoAddPlottableToLegend(true)
        , mAntialiasedElements(QCP::aeLegendItems | QCP::aePlottables | QCP::aeItems | QCP::aeAxes
                               | QCP::aeSubGrid | QCP::aeLegend)
        , mNotAntialiasedElements(QCP::aeNone)
        , mInteractions(QCP::iNone)
        , mSelectionTolerance(8)
        , mNoAntialiasingOnDrag(true)
        , mBackgroundBrush(Qt::white, Qt::SolidPattern)
        , mBackgroundScaled(true)
        , mBackgroundScaledMode(Qt::KeepAspectRatioByExpanding)
        , mCurrentLayer(nullptr)
        , mPlottingHints(QCP::phImmediateRefresh)
        , mMultiSelectModifier(Qt::ControlModifier)
        , mSelectionRectMode(QCP::srmNone)
        , mSelectionRect(nullptr)
        , mOwnedTheme(nullptr)
        , mThemeDirty(false)
        , mMouseHasMoved(false)
        , mMouseEventLayerable(nullptr)
        , mMouseSignalLayerable(nullptr)
        , mReplotting(false)
        , mReplotQueued(false)
        , mReplotTime(0)
        , mReplotTimeAverage(0)
{
    setAttribute(Qt::WA_NoMousePropagation);
    // Allow forcing a specific RHI backend via environment variable.
    // NEOQCP_RHI_BACKEND=opengl|metal|vulkan|d3d11|d3d12
    if (const QByteArray backend = qgetenv("NEOQCP_RHI_BACKEND").toLower(); !backend.isEmpty()) {
        if (backend == "opengl")       setApi(Api::OpenGL);
        else if (backend == "vulkan")  setApi(Api::Vulkan);
        else if (backend == "metal")   setApi(Api::Metal);
        else if (backend == "d3d11")   setApi(Api::Direct3D11);
        else if (backend == "d3d12")   setApi(Api::Direct3D12);
    }
    // HiDPI displays (DPR >= 2) already provide sub-pixel smoothness, so MSAA=1 saves
    // bandwidth. Non-HiDPI gets 4x MSAA for proper antialiasing of GPU-rendered lines.
    // Users can override via setSampleCount() after construction.
    setSampleCount(devicePixelRatioF() >= 2.0 ? 1 : 4);
    setFocusPolicy(Qt::ClickFocus);
    setMouseTracking(true);
    QLocale currentLocale = locale();
    currentLocale.setNumberOptions(QLocale::OmitGroupSeparator);
    setLocale(currentLocale);
    setBufferDevicePixelRatio(QWidget::devicePixelRatioF());

    // create initial layers:
    mLayers.append(new QCPLayer(this, QLatin1String("background")));
    mLayers.append(new QCPLayer(this, QLatin1String("grid")));
    mLayers.append(new QCPLayer(this, QLatin1String("main")));
    mLayers.append(new QCPLayer(this, QLatin1String("annotations")));
    mLayers.append(new QCPLayer(this, QLatin1String("axes")));
    mLayers.append(new QCPLayer(this, QLatin1String("legend")));
    mLayers.append(new QCPLayer(this, QLatin1String("overlay")));
    updateLayerIndices();
    setCurrentLayer(QLatin1String("main"));
    layer(QLatin1String("overlay"))->setMode(QCPLayer::lmBuffered);
    layer(QLatin1String("main"))->setMode(QCPLayer::lmBuffered);
    layer(QLatin1String("annotations"))->setMode(QCPLayer::lmBuffered);

    // create initial layout, axis rect and legend:
    createPlotLayout();
    QCPAxisRect* defaultAxisRect = new QCPAxisRect(this, true);
    mPlotLayout->addElement(0, 0, defaultAxisRect);
    xAxis = defaultAxisRect->axis(QCPAxis::atBottom);
    yAxis = defaultAxisRect->axis(QCPAxis::atLeft);
    xAxis2 = defaultAxisRect->axis(QCPAxis::atTop);
    yAxis2 = defaultAxisRect->axis(QCPAxis::atRight);
    legend = new QCPLegend;
    legend->setVisible(false);
    defaultAxisRect->insetLayout()->addElement(legend, Qt::AlignRight | Qt::AlignTop);
    defaultAxisRect->insetLayout()->setMargins(QMargins(12, 12, 12, 12));

    defaultAxisRect->setLayer(QLatin1String("background"));
    xAxis->setLayer(QLatin1String("axes"));
    yAxis->setLayer(QLatin1String("axes"));
    xAxis2->setLayer(QLatin1String("axes"));
    yAxis2->setLayer(QLatin1String("axes"));
    xAxis->grid()->setLayer(QLatin1String("grid"));
    yAxis->grid()->setLayer(QLatin1String("grid"));
    xAxis2->grid()->setLayer(QLatin1String("grid"));
    yAxis2->grid()->setLayer(QLatin1String("grid"));
    legend->setLayer(QLatin1String("legend"));

    // create selection rect instance:
    mSelectionRect = new QCPSelectionRect(this);
    mSelectionRect->setLayer(QLatin1String("overlay"));

    adoptWidgetViewport(); // needs to be called after mPlotLayout has been created

    mTheme = ownedTheme();
    connectThemeSignal();

#ifdef Q_OS_WASM
    mPipelineScheduler = new QCPPipelineScheduler(QCPPipelineScheduler::browserPool(), 0, this);
#else
    mPipelineScheduler = new QCPPipelineScheduler(0, this);
#endif

    mDataSwapTimer.setSingleShot(true);
    connect(&mDataSwapTimer, &QTimer::timeout, this, &QCustomPlot::commitPendingData);

    // No replot here — initialize() + resizeEvent() will handle the first replot once
    // the RHI backend is ready, avoiding throwaway pixmap buffer creation.
}

QCustomPlot::~QCustomPlot()
{
    mBeingDestroyed = true;
    if (mEmissions.insideWidgetEvent())
        qcp::reportMisuse(Q_FUNC_INFO, "QCustomPlot deleted inside its own resize, show or "
                                       "paint event (a replot signal's slot?): use deleteLater()");
    mEmissions.discard(); // a plot being destroyed delivers nothing it held back
    // Release paint buffer GPU resources before QRhiWidget tears down the RHI
    qDeleteAll(std::exchange(mPlottableRhiLayers, {}));
    qDeleteAll(std::exchange(mScatterRhiLayers, {}));
    mPaintBuffers.clear();

    // Before the plottables and the legend: a group's destructor touches both. As QObject
    // children the groups would otherwise be destroyed after them.
    while (!mPlottableGroups.isEmpty())
        delete mPlottableGroups.last();
    // While the layout stands: a destroyed() slot of these still sees a whole plot.
    while (!mPlottables.isEmpty())
        deletePlottable(mPlottables.last());
    deleteAllItems();

    delete mPlotLayout.data();

    mCurrentLayer = nullptr;
    // Unlisted first: a layer still listed would move its children to its neighbour as it dies.
    qDeleteAll(std::exchange(mLayers, {}));
    // Last: the spans and axes deleted above unregister from the span and grid GPU layers.
    releaseCompositeResources();
}

/*! \internal

  Makes an empty root layout: in the constructor, and when the user deleted the previous one.
*/
void QCustomPlot::createPlotLayout()
{
    mPlotLayout = new QCPLayoutGrid;
    mPlotLayout->initializeParentPlot(this);
    mPlotLayout->setParent(
        this); // important because if parent is QWidget, QCPLayout::sizeConstraintsChanged will
               // call QWidget::updateGeometry
    if (layer(QLatin1String("main")))
        mPlotLayout->setLayer(QLatin1String("main"));
    mPlotLayout->setOuterRect(mViewport);
}

/*!
  Returns the top level layout of this QCustomPlot instance. It is a \ref QCPLayoutGrid, initially
  containing just one cell with the main QCPAxisRect inside.

  It is never \c nullptr: if the user deleted it, an empty layout takes its place. The exception
  is a plot being destroyed (\ref isBeingDestroyed): then nothing is made.
*/
QCPLayoutGrid* QCustomPlot::plotLayout() const
{
    if (!mPlotLayout && !mBeingDestroyed)
        const_cast<QCustomPlot*>(this)->createPlotLayout();
    return mPlotLayout;
}

/*! \internal

  The plot's own theme, used when no other is set. Made again if the user deleted it.
*/
QCPTheme* QCustomPlot::ownedTheme()
{
    if (!mOwnedTheme && !mBeingDestroyed)
        mOwnedTheme = new QCPTheme(this);
    return mOwnedTheme;
}

QCPTheme* QCustomPlot::theme() const
{
    return mTheme;
}

void QCustomPlot::setTheme(QCPTheme* theme)
{
    QCP_CHECK_OR_RETURN(!mBeingDestroyed, "the plot is being destroyed");
    QCPTheme* resolved = theme ? theme : ownedTheme();
    if (mTheme == resolved)
        return;
    if (mTheme)
        disconnect(mTheme, &QCPTheme::changed, this, nullptr);
    mTheme = resolved;
    connectThemeSignal();
    applyTheme();
}

void QCustomPlot::connectThemeSignal()
{
    connect(mTheme, &QCPTheme::changed, this, [this]() {
        if (!mThemeDirty) {
            mThemeDirty = true;
            QTimer::singleShot(0, this, [this]() {
                if (!mThemeDirty) // settle() already applied it
                    return;
                mThemeDirty = false;
                applyTheme();
            });
        }
    });
}

void QCustomPlot::applyThemeToAxis(QCPAxis* axis)
{
    const QColor fg = mTheme->foreground();
    const QColor sel = mTheme->selection();
    auto recoloredPen = [](QPen pen, const QColor& c) { pen.setColor(c); return pen; };

    axis->setBasePen(recoloredPen(axis->basePen(), fg));
    axis->setTickPen(recoloredPen(axis->tickPen(), fg));
    axis->setSubTickPen(recoloredPen(axis->subTickPen(), fg));
    axis->setLabelColor(fg);
    axis->setTickLabelColor(fg);

    axis->setSelectedBasePen(recoloredPen(axis->selectedBasePen(), sel));
    axis->setSelectedTickPen(recoloredPen(axis->selectedTickPen(), sel));
    axis->setSelectedSubTickPen(recoloredPen(axis->selectedSubTickPen(), sel));
    axis->setSelectedLabelColor(sel);
    axis->setSelectedTickLabelColor(sel);

    auto* grid = axis->grid();
    grid->setPen(recoloredPen(grid->pen(), mTheme->grid()));
    grid->setSubGridPen(recoloredPen(grid->subGridPen(), mTheme->subGrid()));
    grid->setZeroLinePen(recoloredPen(grid->zeroLinePen(), mTheme->grid()));
}

void QCustomPlot::applyTheme()
{
    if (!mTheme || mBeingDestroyed)
        return;

    ++mViewEpoch;

    auto recoloredPen = [](QPen pen, const QColor& c) { pen.setColor(c); return pen; };

    mBackgroundBrush.setColor(mTheme->background());

    const QColor fg = mTheme->foreground();
    const QColor sel = mTheme->selection();

    for (auto* rect : axisRects())
    {
        for (auto* axis : rect->axes())
            applyThemeToAxis(axis);
        // The rect's own scale is outside the layout tree: theme its axis here.
        if (rect->mColorScale)
            applyThemeToAxis(rect->mColorScale->axis());
    }

    if (legend) {
        legend->setBrush(QBrush(mTheme->legendBackground()));
        legend->setBorderPen(recoloredPen(legend->borderPen(), mTheme->legendBorder()));
        legend->setTextColor(fg);
        legend->setSelectedTextColor(sel);
        legend->setSelectedBorderPen(recoloredPen(legend->selectedBorderPen(), sel));
    }

    std::function<void(QCPLayoutElement*)> walkLayout = [&](QCPLayoutElement* el) {
        if (!el) return;
        if (auto* te = qobject_cast<QCPTextElement*>(el)) {
            te->setTextColor(fg);
            te->setSelectedTextColor(sel);
        }
        if (auto* cs = qobject_cast<QCPColorScale*>(el))
            applyThemeToAxis(cs->axis());
        if (auto* layout = qobject_cast<QCPLayout*>(el))
            for (int i = 0; i < layout->elementCount(); ++i)
                walkLayout(layout->elementAt(i));
    };
    walkLayout(plotLayout());

    for (auto* item : mItems) {
        if (auto* textItem = qobject_cast<QCPItemText*>(item)) {
            textItem->setColor(fg);
            textItem->setSelectedColor(sel);
        }
    }

    if (mSelectionRect)
        mSelectionRect->setPen(recoloredPen(mSelectionRect->pen(), sel));

    // The setters above marked what they touch; the background brush is drawn outside every layer,
    // so invalidate them all and let the queued repaint do the work.
    // simplify: invalidates every layer even for a theme change that touches nothing, because
    // working out which layer a theme field reaches costs more than one repaint.
    for (QCPLayer* layer : mLayers)
        layer->invalidatePaintBuffer();
    requestRepaint();
}

QColor QCustomPlot::themeBackground() const { return mTheme ? mTheme->background() : QColor(); }
void QCustomPlot::setThemeBackground(const QColor& c) { if (mTheme) mTheme->setBackground(c); }

QColor QCustomPlot::themeForeground() const { return mTheme ? mTheme->foreground() : QColor(); }
void QCustomPlot::setThemeForeground(const QColor& c) { if (mTheme) mTheme->setForeground(c); }

QColor QCustomPlot::themeGrid() const { return mTheme ? mTheme->grid() : QColor(); }
void QCustomPlot::setThemeGrid(const QColor& c) { if (mTheme) mTheme->setGrid(c); }

QColor QCustomPlot::themeSubGrid() const { return mTheme ? mTheme->subGrid() : QColor(); }
void QCustomPlot::setThemeSubGrid(const QColor& c) { if (mTheme) mTheme->setSubGrid(c); }

QColor QCustomPlot::themeSelection() const { return mTheme ? mTheme->selection() : QColor(); }
void QCustomPlot::setThemeSelection(const QColor& c) { if (mTheme) mTheme->setSelection(c); }

QColor QCustomPlot::themeLegendBackground() const { return mTheme ? mTheme->legendBackground() : QColor(); }
void QCustomPlot::setThemeLegendBackground(const QColor& c) { if (mTheme) mTheme->setLegendBackground(c); }

QColor QCustomPlot::themeLegendBorder() const { return mTheme ? mTheme->legendBorder() : QColor(); }
void QCustomPlot::setThemeLegendBorder(const QColor& c) { if (mTheme) mTheme->setLegendBorder(c); }

void QCustomPlot::setMaxPipelineThreads(int count)
{
    mPipelineScheduler->setMaxThreads(count);
}

QCPOverlay* QCustomPlot::overlay()
{
    if (!mOverlay && !mBeingDestroyed) {
        auto* notifLayer = layer(QLatin1String("notification"));
        if (!notifLayer) {
            (void)addLayer(QLatin1String("notification"));
            notifLayer = layer(QLatin1String("notification"));
        }
        notifLayer->setMode(QCPLayer::lmBuffered);
        setupPaintBuffers();

        mOverlay = new QCPOverlay(this);
        mOverlay->setLayer(notifLayer);
    }
    return mOverlay;
}

QCPGridRhiLayer* QCustomPlot::gridRhiLayer()
{
    if (!mGridRhiLayer && mRhi && !mBeingDestroyed)
        mGridRhiLayer = new QCPGridRhiLayer(mRhi);
    return mGridRhiLayer;
}

QCPGridRhiLayer* QCustomPlot::gridRhiLayerIfExists() const
{
    return mGridRhiLayer;
}

void QCustomPlot::setSkipReplotsWhenHidden(bool skip)
{
    mSkipReplotsWhenHidden = skip;
}

bool QCustomPlot::skipReplotsWhenHidden() const
{
    return mSkipReplotsWhenHidden;
}

void QCustomPlot::setItemIndexing(bool on)
{
    if (mItemIndexing == on)
        return;
    mItemIndexing = on;
    for (QCPLayer* layer : std::as_const(mLayers))
        layer->invalidateItemIndex();
    requestRepaint();
}

/*!
  Sets which elements are forcibly drawn antialiased as an \a or combination of
  QCP::AntialiasedElement.

  This overrides the antialiasing settings for whole element groups, normally controlled with the
  \a setAntialiasing function on the individual elements. If an element is neither specified in
  \ref setAntialiasedElements nor in \ref setNotAntialiasedElements, the antialiasing setting on
  each individual element instance is used.

  For example, if \a antialiasedElements contains \ref QCP::aePlottables, all plottables will be
  drawn antialiased, no matter what the specific QCPAbstractPlottable::setAntialiased value was set
  to.

  if an element in \a antialiasedElements is already set in \ref setNotAntialiasedElements, it is
  removed from there.

  \see setNotAntialiasedElements
*/
void QCustomPlot::setAntialiasedElements(const QCP::AntialiasedElements& antialiasedElements)
{
    mAntialiasedElements = antialiasedElements;

    // make sure elements aren't in mNotAntialiasedElements and mAntialiasedElements simultaneously:
    if ((mNotAntialiasedElements & mAntialiasedElements) != 0)
        mNotAntialiasedElements |= ~mAntialiasedElements;
}

/*!
  Sets whether the specified \a antialiasedElement is forcibly drawn antialiased.

  See \ref setAntialiasedElements for details.

  \see setNotAntialiasedElement
*/
void QCustomPlot::setAntialiasedElement(QCP::AntialiasedElement antialiasedElement, bool enabled)
{
    if (!enabled && mAntialiasedElements.testFlag(antialiasedElement))
        mAntialiasedElements &= ~antialiasedElement;
    else if (enabled && !mAntialiasedElements.testFlag(antialiasedElement))
        mAntialiasedElements |= antialiasedElement;

    // make sure elements aren't in mNotAntialiasedElements and mAntialiasedElements simultaneously:
    if ((mNotAntialiasedElements & mAntialiasedElements) != 0)
        mNotAntialiasedElements |= ~mAntialiasedElements;
}

/*!
  Sets which elements are forcibly drawn not antialiased as an \a or combination of
  QCP::AntialiasedElement.

  This overrides the antialiasing settings for whole element groups, normally controlled with the
  \a setAntialiasing function on the individual elements. If an element is neither specified in
  \ref setAntialiasedElements nor in \ref setNotAntialiasedElements, the antialiasing setting on
  each individual element instance is used.

  For example, if \a notAntialiasedElements contains \ref QCP::aePlottables, no plottables will be
  drawn antialiased, no matter what the specific QCPAbstractPlottable::setAntialiased value was set
  to.

  if an element in \a notAntialiasedElements is already set in \ref setAntialiasedElements, it is
  removed from there.

  \see setAntialiasedElements
*/
void QCustomPlot::setNotAntialiasedElements(const QCP::AntialiasedElements& notAntialiasedElements)
{
    mNotAntialiasedElements = notAntialiasedElements;

    // make sure elements aren't in mNotAntialiasedElements and mAntialiasedElements simultaneously:
    if ((mNotAntialiasedElements & mAntialiasedElements) != 0)
        mAntialiasedElements |= ~mNotAntialiasedElements;
}

/*!
  Sets whether the specified \a notAntialiasedElement is forcibly drawn not antialiased.

  See \ref setNotAntialiasedElements for details.

  \see setAntialiasedElement
*/
void QCustomPlot::setNotAntialiasedElement(QCP::AntialiasedElement notAntialiasedElement,
                                           bool enabled)
{
    if (!enabled && mNotAntialiasedElements.testFlag(notAntialiasedElement))
        mNotAntialiasedElements &= ~notAntialiasedElement;
    else if (enabled && !mNotAntialiasedElements.testFlag(notAntialiasedElement))
        mNotAntialiasedElements |= notAntialiasedElement;

    // make sure elements aren't in mNotAntialiasedElements and mAntialiasedElements simultaneously:
    if ((mNotAntialiasedElements & mAntialiasedElements) != 0)
        mAntialiasedElements |= ~mNotAntialiasedElements;
}

/*!
  If set to true, adding a plottable (e.g. a graph) to the QCustomPlot automatically also adds the
  plottable to the legend (QCustomPlot::legend).

  \see addGraph, QCPLegend::addItem
*/
void QCustomPlot::setAutoAddPlottableToLegend(bool on)
{
    mAutoAddPlottableToLegend = on;
}

/*!
  Sets the possible interactions of this QCustomPlot as an or-combination of \ref QCP::Interaction
  enums. There are the following types of interactions:

  <b>Axis range manipulation</b> is controlled via \ref QCP::iRangeDrag and \ref QCP::iRangeZoom.
  When the respective interaction is enabled, the user may drag axes ranges and zoom with the mouse
  wheel. For details how to control which axes the user may drag/zoom and in what orientations, see
  \ref QCPAxisRect::setRangeDrag, \ref QCPAxisRect::setRangeZoom, \ref
  QCPAxisRect::setRangeDragAxes,
  \ref QCPAxisRect::setRangeZoomAxes.

  <b>Plottable data selection</b> is controlled by \ref QCP::iSelectPlottables. If \ref
  QCP::iSelectPlottables is set, the user may select plottables (graphs, curves, bars,...) and
  their data by clicking on them or in their vicinity (\ref setSelectionTolerance). Whether the
  user can actually select a plottable and its data can further be restricted with the \ref
  QCPAbstractPlottable::setSelectable method on the specific plottable. For details, see the
  special page about the \ref dataselection "data selection mechanism". To retrieve a list of all
  currently selected plottables, call \ref selectedPlottables. If you're only interested in
  QCPGraphs, you may use the convenience function \ref selectedGraphs.

  <b>Item selection</b> is controlled by \ref QCP::iSelectItems. If \ref QCP::iSelectItems is set,
  the user may select items (QCPItemLine, QCPItemText,...) by clicking on them or in their vicinity.
  To find out whether a specific item is selected, call QCPAbstractItem::selected(). To retrieve a
  list of all currently selected items, call \ref selectedItems.

  <b>Axis selection</b> is controlled with \ref QCP::iSelectAxes. If \ref QCP::iSelectAxes is set,
  the user may select parts of the axes by clicking on them. What parts exactly (e.g. Axis base
  line, tick labels, axis label) are selectable can be controlled via \ref
  QCPAxis::setSelectableParts for each axis. To retrieve a list of all axes that currently contain
  selected parts, call \ref selectedAxes. Which parts of an axis are selected, can be retrieved with
  QCPAxis::selectedParts().

  <b>Legend selection</b> is controlled with \ref QCP::iSelectLegend. If this is set, the user may
  select the legend itself or individual items by clicking on them. What parts exactly are
  selectable can be controlled via \ref QCPLegend::setSelectableParts. To find out whether the
  legend or any of its child items are selected, check the value of QCPLegend::selectedParts. To
  find out which child items are selected, call \ref QCPLegend::selectedItems.

  <b>All other selectable elements</b> The selection of all other selectable objects (e.g.
  QCPTextElement, or your own layerable subclasses) is controlled with \ref QCP::iSelectOther. If
  set, the user may select those objects by clicking on them. To find out which are currently
  selected, you need to check their selected state explicitly.

  If the selection state has changed by user interaction, the \ref selectionChangedByUser signal is
  emitted. Each selectable object additionally emits an individual selectionChanged signal whenever
  their selection state has changed, i.e. not only by user interaction.

  To allow multiple objects to be selected by holding the selection modifier (\ref
  setMultiSelectModifier), set the flag \ref QCP::iMultiSelect.

  \note In addition to the selection mechanism presented here, QCustomPlot always emits
  corresponding signals, when an object is clicked or double clicked. see \ref plottableClick and
  \ref plottableDoubleClick for example.

  \see setInteraction, setSelectionTolerance
*/
void QCustomPlot::setInteractions(const QCP::Interactions& interactions)
{
    mInteractions = interactions;
    updateGestureInput();
}

/*!
  Sets the single \a interaction of this QCustomPlot to \a enabled.

  For details about the interaction system, see \ref setInteractions.

  \see setInteractions
*/
void QCustomPlot::setInteraction(const QCP::Interaction& interaction, bool enabled)
{
    if (!enabled && mInteractions.testFlag(interaction))
        mInteractions &= ~interaction;
    else if (enabled && !mInteractions.testFlag(interaction))
        mInteractions |= interaction;
    updateGestureInput();
}

/*! \internal

  Touch input is only taken with \ref QCP::iGestures (see touchEvent).
*/
void QCustomPlot::updateGestureInput()
{
    setAttribute(Qt::WA_AcceptTouchEvents, mInteractions.testFlag(QCP::iGestures));
}

/*!
  Sets the tolerance that is used to decide whether a click selects an object (e.g. a plottable) or
  not.

  If the user clicks in the vicinity of the line of e.g. a QCPGraph, it's only regarded as a
  potential selection when the minimum distance between the click position and the graph line is
  smaller than \a pixels. Objects that are defined by an area (e.g. QCPBars) only react to clicks
  directly inside the area and ignore this selection tolerance. In other words, it only has meaning
  for parts of objects that are too thin to exactly hit with a click and thus need such a
  tolerance.

  \see setInteractions, QCPLayerable::selectTest
*/
void QCustomPlot::setSelectionTolerance(int pixels)
{
    mSelectionTolerance = pixels;
}

/*!
  Sets whether antialiasing is disabled for this QCustomPlot while the user is dragging axes
  ranges. If many objects, especially plottables, are drawn antialiased, this greatly improves
  performance during dragging. Thus it creates a more responsive user experience. As soon as the
  user stops dragging, the last replot is done with normal antialiasing, to restore high image
  quality.

  \see setAntialiasedElements, setNotAntialiasedElements
*/
void QCustomPlot::setNoAntialiasingOnDrag(bool enabled)
{
    mNoAntialiasingOnDrag = enabled;
}

/*!
  Sets the plotting hints for this QCustomPlot instance as an \a or combination of
  QCP::PlottingHint.

  \see setPlottingHint
*/
void QCustomPlot::setPlottingHints(const QCP::PlottingHints& hints)
{
    mPlottingHints = hints;
}

/*!
  Sets the specified plotting \a hint to \a enabled.

  \see setPlottingHints
*/
void QCustomPlot::setPlottingHint(QCP::PlottingHint hint, bool enabled)
{
    QCP::PlottingHints newHints = mPlottingHints;
    if (!enabled)
        newHints &= ~hint;
    else
        newHints |= hint;

    if (newHints != mPlottingHints)
        setPlottingHints(newHints);
}

/*!
  Sets the keyboard modifier that will be recognized as multi-select-modifier.

  If \ref QCP::iMultiSelect is specified in \ref setInteractions, the user may select multiple
  objects (or data points) by clicking on them one after the other while holding down \a modifier.

  By default the multi-select-modifier is set to Qt::ControlModifier.

  \see setInteractions
*/
void QCustomPlot::setMultiSelectModifier(Qt::KeyboardModifier modifier)
{
    mMultiSelectModifier = modifier;
}

/*!
  Sets how QCustomPlot processes mouse click-and-drag interactions by the user.

  If \a mode is \ref QCP::srmNone, the mouse drag is forwarded to the underlying objects. For
  example, QCPAxisRect may process a mouse drag by dragging axis ranges, see \ref
  QCPAxisRect::setRangeDrag. If \a mode is not \ref QCP::srmNone, the current selection rect (\ref
  selectionRect) becomes activated and allows e.g. rect zooming and data point selection.

  If you wish to provide your user both with axis range dragging and data selection/range zooming,
  use this method to switch between the modes just before the interaction is processed, e.g. in
  reaction to the \ref mousePress or \ref mouseMove signals. For example you could check whether
  the user is holding a certain keyboard modifier, and then decide which \a mode shall be set.

  If a selection rect interaction is currently active, and \a mode is set to \ref QCP::srmNone, the
  interaction is canceled (\ref QCPSelectionRect::cancel). Switching between any of the other modes
  will keep the selection rect active. Upon completion of the interaction, the behaviour is as
  defined by the currently set \a mode, not the mode that was set when the interaction started.

  \see setInteractions, setSelectionRect, QCPSelectionRect
*/
void QCustomPlot::setSelectionRectMode(QCP::SelectionRectMode mode)
{
    // When switching to none, abort a potentially active selection rect. A canceled slot may
    // delete or replace it, or delete the plot.
    const QPointer<QCustomPlot> alive(this);
    if (mSelectionRect && mode == QCP::srmNone)
        mSelectionRect->cancel();
    if (!alive)
        return;
    if (mSelectionRect)
    {
        // disconnect old connections:
        if (mSelectionRectMode == QCP::srmSelect)
            disconnect(mSelectionRect, &QCPSelectionRect::accepted, this, &QCustomPlot::processRectSelection);
        else if (mSelectionRectMode == QCP::srmZoom)
            disconnect(mSelectionRect, &QCPSelectionRect::accepted, this, &QCustomPlot::processRectZoom);

        // establish new ones:
        if (mode == QCP::srmSelect)
            connect(mSelectionRect, &QCPSelectionRect::accepted, this, &QCustomPlot::processRectSelection);
        else if (mode == QCP::srmZoom)
            connect(mSelectionRect, &QCPSelectionRect::accepted, this, &QCustomPlot::processRectZoom);
    }

    mSelectionRectMode = mode;
}

/*!
  Sets the \ref QCPSelectionRect instance that QCustomPlot will use if \a mode is not \ref
  QCP::srmNone and the user performs a click-and-drag interaction. QCustomPlot takes ownership of
  the passed \a selectionRect. It can be accessed later via \ref selectionRect.

  This method is useful if you wish to replace the default QCPSelectionRect instance with an
  instance of a QCPSelectionRect subclass, to introduce custom behaviour of the selection rect.

  If the previous rect is being dragged, it is canceled before it is deleted. A slot connected to
  its \ref QCPSelectionRect::canceled signal that installs a rect of its own wins: the rect passed
  here is then deleted. Passing the current rect does nothing.

  \see setSelectionRectMode
*/
void QCustomPlot::setSelectionRect(QCPSelectionRect* selectionRect)
{
    if (selectionRect == mSelectionRect)
        return;
    const QPointer<QCPSelectionRect> previous = mSelectionRect;
    mSelectionRect = selectionRect;

    // Unique: a canceled slot below may reinstall a rect that is still connected.
    if (mSelectionRect)
    {
        if (mSelectionRectMode == QCP::srmSelect)
            connect(mSelectionRect, &QCPSelectionRect::accepted, this,
                    &QCustomPlot::processRectSelection, Qt::UniqueConnection);
        else if (mSelectionRectMode == QCP::srmZoom)
            connect(mSelectionRect, &QCPSelectionRect::accepted, this, &QCustomPlot::processRectZoom,
                    Qt::UniqueConnection);
    }
    // Last: a canceled slot may install another rect, delete this one, reinstall it, or delete
    // the plot.
    const QPointer<QCustomPlot> alive(this);
    if (previous)
        previous->cancel();
    if (alive && previous && previous != mSelectionRect)
        delete previous.data();
}

void QCustomPlot::setItemCreator(ItemCreator creator)
{
    if (!mCreationState) {
        mCreationState = new QCPItemCreationState(this);
        connect(mCreationState, &QCPItemCreationState::itemCreated,
                this, &QCustomPlot::itemCreated);
        connect(mCreationState, &QCPItemCreationState::itemCanceled,
                this, &QCustomPlot::itemCanceled);
    }
    mItemCreator = std::move(creator);
    if (!mItemCreator && mCreationState->state() == QCPItemCreationState::Drawing)
        mCreationState->cancel();
}

void QCustomPlot::setCreationModeEnabled(bool enabled)
{
    if (mCreationModeEnabled == enabled) return;
    mCreationModeEnabled = enabled;
    if (enabled)
        setCursor(Qt::CrossCursor);
    else {
        setCursor(Qt::ArrowCursor);
        if (mCreationState)
            mCreationState->cancel();
    }
    replot(rpQueuedReplot);
}

void QCustomPlot::setCreationModifier(Qt::KeyboardModifier mod)
{
    mCreationModifier = mod;
}

void QCustomPlot::setItemPositioner(ItemPositioner positioner)
{
    mItemPositioner = std::move(positioner);
}

/*!
  Sets the viewport of this QCustomPlot. Usually users of QCustomPlot don't need to change the
  viewport manually.

  The viewport is the area in which the plot is drawn. All mechanisms, e.g. margin calculation take
  the viewport to be the outer border of the plot. The viewport normally is the rect() of the
  QCustomPlot widget, i.e. a rect with top left (0, 0) and size of the QCustomPlot widget.

  Don't confuse the viewport with the axis rect (QCustomPlot::axisRect). An axis rect is typically
  an area enclosed by four axes, where the graphs/plottables are drawn in. The viewport is larger
  and contains also the axes themselves, their tick numbers, their labels, or even additional axis
  rects, color scales and other layout elements.

  This function is used to allow arbitrary size exports with \ref toPixmap, \ref savePng, \ref
  savePdf, etc. by temporarily changing the viewport size.
*/
void QCustomPlot::adoptWidgetViewport()
{
    mViewportWidgetSize = size();
    setViewport(rect());
}

// Qt holds back a hidden widget's resize event until it is shown, so a plot resized while hidden
// would keep laying out at its old size: coordToPixel(), hit tests and the screen layout would all
// disagree with width() and height(). A size change no resize event reported is applied here.
void QCustomPlot::syncViewportToWidgetSize()
{
    if (size() != mViewportWidgetSize)
        adoptWidgetViewport();
}

void QCustomPlot::setViewport(const QRect& rect)
{
    ++mViewEpoch;
    mViewport = rect;
    if (mPlotLayout)
        mPlotLayout->setOuterRect(mViewport);
}

/*!
  Sets the device pixel ratio used by the paint buffers of this QCustomPlot instance.

  Normally, this doesn't need to be set manually, because it is initialized with the regular \a
  QWidget::devicePixelRatio which is configured by Qt to fit the display device (e.g. 1 for normal
  displays, 2 for High-DPI displays).

  Device pixel ratios are supported by Qt only for Qt versions since 5.4. If this method is called
  when QCustomPlot is being used with older Qt versions, outputs an according qDebug message and
  leaves the internal buffer device pixel ratio at 1.0.
*/
void QCustomPlot::setBufferDevicePixelRatio(double ratio)
{
    if (!qFuzzyCompare(ratio, mBufferDevicePixelRatio))
    {
        mBufferDevicePixelRatio = ratio;
        ++mViewEpoch;
        for (auto& buffer : mPaintBuffers)
        {
            buffer->setDevicePixelRatio(mBufferDevicePixelRatio);
        }
    }
}

QCPPlottableRhiLayer* QCustomPlot::plottableRhiLayer(QCPLayer* layer)
{
    if (!mRhi || mBeingDestroyed)
        return nullptr;
    if (!mPlottableRhiLayers.contains(layer))
    {
        auto* prl = new QCPPlottableRhiLayer(mRhi);
        mPlottableRhiLayers[layer] = prl;
    }
    return mPlottableRhiLayers[layer];
}

QCPScatterRhiLayer* QCustomPlot::scatterRhiLayer(QCPLayer* layer)
{
    if (!mRhi || mBeingDestroyed)
        return nullptr;
    if (!mScatterRhiLayers.contains(layer))
    {
        auto* srl = new QCPScatterRhiLayer(mRhi);
        mScatterRhiLayers[layer] = srl;
    }
    return mScatterRhiLayers[layer];
}

QSize QCustomPlot::rhiOutputSize() const
{
    return renderTarget() ? renderTarget()->pixelSize() : QSize();
}

/*!
  Sets \a pm as the viewport background pixmap (see \ref setViewport). The pixmap is always drawn
  below all other objects in the plot.

  For cases where the provided pixmap doesn't have the same size as the viewport, scaling can be
  enabled with \ref setBackgroundScaled and the scaling mode (whether and how the aspect ratio is
  preserved) can be set with \ref setBackgroundScaledMode. To set all these options in one call,
  consider using the overloaded version of this function.

  If a background brush was set with \ref setBackground(const QBrush &brush), the viewport will
  first be filled with that brush, before drawing the background pixmap. This can be useful for
  background pixmaps with translucent areas.

  \see setBackgroundScaled, setBackgroundScaledMode
*/
void QCustomPlot::setBackground(const QPixmap& pm)
{
    mBackgroundPixmap = pm;
    mScaledBackgroundPixmap = QPixmap();
}

/*!
  Sets the background brush of the viewport (see \ref setViewport).

  Before drawing everything else, the background is filled with \a brush. If a background pixmap
  was set with \ref setBackground(const QPixmap &pm), this brush will be used to fill the viewport
  before the background pixmap is drawn. This can be useful for background pixmaps with translucent
  areas.

  Set \a brush to Qt::NoBrush or Qt::Transparent to leave background transparent. This can be
  useful for exporting to image formats which support transparency, e.g. \ref savePng.

  \see setBackgroundScaled, setBackgroundScaledMode
*/
void QCustomPlot::setBackground(const QBrush& brush)
{
    mBackgroundBrush = brush;
}

/*! \overload

  Allows setting the background pixmap of the viewport, whether it shall be scaled and how it
  shall be scaled in one call.

  \see setBackground(const QPixmap &pm), setBackgroundScaled, setBackgroundScaledMode
*/
void QCustomPlot::setBackground(const QPixmap& pm, bool scaled, Qt::AspectRatioMode mode)
{
    mBackgroundPixmap = pm;
    mScaledBackgroundPixmap = QPixmap();
    mBackgroundScaled = scaled;
    mBackgroundScaledMode = mode;
}

/*!
  Sets whether the viewport background pixmap shall be scaled to fit the viewport. If \a scaled is
  set to true, control whether and how the aspect ratio of the original pixmap is preserved with
  \ref setBackgroundScaledMode.

  Note that the scaled version of the original pixmap is buffered, so there is no performance
  penalty on replots. (Except when the viewport dimensions are changed continuously.)

  \see setBackground, setBackgroundScaledMode
*/
void QCustomPlot::setBackgroundScaled(bool scaled)
{
    mBackgroundScaled = scaled;
}

/*!
  If scaling of the viewport background pixmap is enabled (\ref setBackgroundScaled), use this
  function to define whether and how the aspect ratio of the original pixmap is preserved.

  \see setBackground, setBackgroundScaled
*/
void QCustomPlot::setBackgroundScaledMode(Qt::AspectRatioMode mode)
{
    mBackgroundScaledMode = mode;
}

/*!
  Returns the plottable with \a index. If the index is invalid, returns \c nullptr.

  There is an overloaded version of this function with no parameter which returns the last added
  plottable, see QCustomPlot::plottable()

  \see plottableCount
*/
QCPAbstractPlottable* QCustomPlot::plottable(int index)
{
    if (index >= 0 && index < mPlottables.size())
    {
        return mPlottables.at(index);
    }
    else
    {
        qDebug() << Q_FUNC_INFO << "index out of bounds:" << index;
        return nullptr;
    }
}

/*! \overload

  Returns the last plottable that was added to the plot. If there are no plottables in the plot,
  returns \c nullptr.

  \see plottableCount
*/
QCPAbstractPlottable* QCustomPlot::plottable()
{
    if (!mPlottables.isEmpty())
    {
        return mPlottables.last();
    }
    else
        return nullptr;
}

/*!
  Removes the specified plottable from the plot and deletes it. If necessary, the corresponding
  legend item is also removed from the default legend (QCustomPlot::legend).

  Returns true on success.

  \see clearPlottables
*/
bool QCustomPlot::removePlottable(QCPAbstractPlottable* plottable)
{
    QCP_CHECK_OR_RETURN(!mBeingDestroyed, "the plot is being destroyed", false);
    if (!mPlottables.contains(plottable))
    {
        qDebug() << Q_FUNC_INFO
                 << "plottable not in list:" << reinterpret_cast<quintptr>(plottable);
        return false;
    }
    deletePlottable(plottable);
    return true;
}

namespace
{
// What still points at a plottable that leaves without being destroyed: the colour scale it follows,
// its bars group and its stack. Destruction undoes these by itself.
void leaveSiblings(QCPAbstractPlottable* plottable)
{
    if (auto* mapped = qobject_cast<QCPAbstractColorMapped*>(plottable))
        mapped->setColorScale(nullptr);
    else if (auto* bars = qobject_cast<QCPBars*>(plottable))
    {
        bars->setBarsGroup(nullptr);
        bars->moveBelow(nullptr);
    }
}
} // namespace

/*! \internal

  QCustomPlot::take for a plottable: everything the plot and its parts hold of it is let go, and the
  object is left whole, in the state of a plottable built without axes (inert, no parent).
*/
bool QCustomPlot::detach(QCPAbstractPlottable* plottable)
{
    QCP_CHECK_OR_RETURN(!mBeingDestroyed, "the plot is being destroyed", false);
    QCP_CHECK_OR_RETURN(mPlottables.contains(plottable), "the object is not in this plot", false);
    leaveSiblings(plottable);
    plottable->setPipelineScheduler(nullptr);
    for (QCPLegend* legend : findChildren<QCPLegend*>())
        plottable->removeFromLegend(legend);
    // A tracer keeps the graph it follows as a QPointer: the taken graph outlives the take, so the
    // pointer would stay valid while the graph is no longer in this plot.
    for (QCPAbstractItem* item : std::as_const(mItems))
        if (auto* tracer = qobject_cast<QCPItemTracer*>(item); tracer && tracer->graph() == plottable)
            tracer->setGraph(nullptr);
    const QPointer<QCPPlottableGroup> group = unlistPlottable(plottable);
    plottable->moveToLayer(nullptr, false);
    plottable->setParentLayerable(nullptr);
    plottable->mSystem = nullptr;
    plottable->mKeyAxis = nullptr;
    plottable->mValueAxis = nullptr;
    plottable->mReportedLostAxis = true;
    plottable->mRegisteredPlot = nullptr;
    plottable->mParentPlot = nullptr;
    plottable->setParent(nullptr);
    if (group)
    {
        group->updateLegendRow();
        qcpEmit(group.data(), this, &QCPPlottableGroup::membersChanged);
    }
    return true;
}

/*! \internal

  QCustomPlot::add(std::unique_ptr) for a plottable: the inverse of detach, onto the default axes.
*/
bool QCustomPlot::adopt(QCPAbstractPlottable* plottable)
{
    QCP_CHECK_OR_RETURN(!mBeingDestroyed, "the plot is being destroyed", false);
    QCP_CHECK_OR_RETURN(!plottable->parentPlot(), "the object already belongs to a plot: take it first",
                        false);
    // A polar graph draws through a polar area's frame: the default axes cannot carry it.
    QCP_CHECK_OR_RETURN(!qobject_cast<QCPPolarGraph*>(plottable),
                        "a polar graph is readded through a polar area, not on the default axes",
                        false);
    const auto [keyAxis, valueAxis] = axesForAdd();
    if (!keyAxis)
        return false;
    return adoptPlottable(plottable, keyAxis->axisRect()->system(keyAxis, valueAxis),
                          keyAxis->axisRect());
}

/*! \internal

  The part of adopt() that is the same for every area of the plot: the plottable joins this plot in
  the given system, as the child of that system's area. An area reads its own objects back in with
  it (see QCPPolarArea::add(std::unique_ptr<T>)).
*/
bool QCustomPlot::adoptPlottable(QCPAbstractPlottable* plottable, QCPCoordinateSystem* system,
                                 QCPArea* area)
{
    QCP_CHECK_OR_RETURN(system && area, "the object has no coordinate system to join", false);
    plottable->setParent(this);
    plottable->initializeParentPlot(this);
    // It joins the frame of the area it arrives in, like a plottable built there, through setSystem
    // so it rewires to the new axes: a graph2 would keep following the old ones' signals.
    plottable->setSystem(system);
    plottable->setPipelineScheduler(mPipelineScheduler);
    plottable->setParentLayerable(area);
    if (!registerPlottable(plottable))
        return false;
    plottable->mRegisteredPlot = this;
    if (auto* graph = qobject_cast<QCPGraph*>(plottable))
        registerGraph(graph);
    // The scale it followed belonged to the plot it left: join the new rect's, as a fresh one does.
    if (auto* mapped = qobject_cast<QCPAbstractColorMapped*>(plottable);
        mapped && mapped->joinsOnArrival())
        mapped->followKeyAxisRectScale();
    return true;
}

/*! \internal

  Unlinks every parent anchor between \a item and the rest of the plot, both directions, keeping the
  pixel positions (the plot is still there to compute them).
*/
// A position keeps the pixel it had when its anchor is cut, which is what a user expects for a
// chain; a self-anchor only keeps the relative offset, so the item collapses onto (0, 0).
void QCustomPlot::cutAnchorLinks(QCPAbstractItem* item)
{
    const auto elsewhere = [item](const QCPItemAnchor* anchor)
    { return anchor && anchor->mParentItem != item; };
    for (QCPItemPosition* position : item->positions())
    {
        if (elsewhere(position->parentAnchorX()))
            position->setParentAnchorX(nullptr, true);
        if (elsewhere(position->parentAnchorY()))
            position->setParentAnchorY(nullptr, true);
    }
    for (QCPItemAnchor* anchor : item->anchors())
    {
        for (QCPItemPosition* child : anchor->mChildrenX.values())
            if (elsewhere(child))
                child->setParentAnchorX(nullptr, true);
        for (QCPItemPosition* child : anchor->mChildrenY.values())
            if (elsewhere(child))
                child->setParentAnchorY(nullptr, true);
    }
}

/*! \internal

  QCustomPlot::take for an item. Its positions forget their axes and axis rect, and the anchors the
  plot, so nothing of the old plot is reachable from it. Parent anchors between items are kept.
*/
bool QCustomPlot::detach(QCPAbstractItem* item)
{
    QCP_CHECK_OR_RETURN(!mBeingDestroyed, "the plot is being destroyed", false);
    QCP_CHECK_OR_RETURN(mItems.contains(item), "the object is not in this plot", false);
    cutAnchorLinks(item);
    mItems.removeOne(item);
    item->mRegisteredPlot = nullptr;
    item->setClipAxisRect(nullptr);
    item->moveToLayer(nullptr, false);
    item->setParentLayerable(nullptr);
    for (QCPItemPosition* position : item->mPositions)
    {
        position->setSystem(nullptr);
        position->setArea(nullptr);
    }
    for (QCPItemAnchor* anchor : item->mAnchors)
        anchor->mParentPlot = nullptr;
    // The tracer caches the graph it follows: that graph belongs to the plot it is leaving.
    if (auto* tracer = qobject_cast<QCPItemTracer*>(item))
        tracer->setGraph(nullptr);
    item->mParentPlot = nullptr;
    item->setParent(nullptr);
    return true;
}

/*! \internal

  QCustomPlot::add(std::unique_ptr) for an item: positions get the plot's default axes and axis rect
  as in the item constructor.
*/
bool QCustomPlot::adopt(QCPAbstractItem* item)
{
    QCP_CHECK_OR_RETURN(!mBeingDestroyed, "the plot is being destroyed", false);
    QCP_CHECK_OR_RETURN(!item->parentPlot(), "the object already belongs to a plot: take it first",
                        false);
    QCP_CHECK_OR_RETURN(axisRect(), "the plot has no axis rect", false);
    item->setParent(this);
    item->initializeParentPlot(this);
    for (QCPItemAnchor* anchor : item->mAnchors)
        anchor->mParentPlot = this;
    for (QCPItemPosition* position : item->mPositions)
    {
        position->setAxes(xAxis, yAxis);
        position->setAxisRect(axisRect());
    }
    if (!registerItem(item))
        return false;
    item->mRegisteredPlot = this;
    if (QCPAxisRect* rect = axisRect())
        item->setClipAxisRect(rect);
    return true;
}

/*! \internal

  Unlists and deletes \a plottable, one of this plot's.
*/
void QCustomPlot::deletePlottable(QCPAbstractPlottable* plottable)
{
    // Unlisted while still whole, so a subclass's own legend row goes too; the destructor skips it.
    QPointer<QCPPlottableGroup> group = unlistPlottable(plottable);
    plottable->mRegisteredPlot = nullptr;
    delete plottable;
    // Only now: a slot may call removePlottable again, and must find this one gone.
    if (group)
    {
        group->updateLegendRow();
        qcpEmit(group.data(), this, &QCPPlottableGroup::membersChanged);
    }
}

/*! \internal

  Takes \a plottable out of its group, the plot's legend and the plottable and graph lists, and
  returns the group it left (null if none). Safe to call twice: the second call finds nothing.

  Called from the plottable base destructor too, when the subclass parts are already gone: virtual
  calls reach the base versions and qobject_cast sees a QCPAbstractPlottable, so neither is relied
  on here. A subclass's own legend row leaves by itself (QCPAbstractLegendItem::leaveLegendWhenDestroyed).
*/
QPointer<QCPPlottableGroup> QCustomPlot::unlistPlottable(QCPAbstractPlottable* plottable)
{
    QPointer<QCPPlottableGroup> group = plottable->group();
    if (group)
        group->detach(plottable, false);
    if (legend)
        plottable->removeFromLegend(legend);
    mGraphs.removeIf([plottable](const QCPGraph* graph) { return graph == plottable; });
    mPlottables.removeOne(plottable);
    return group;
}

/*! \internal

  Called by the destructor of \a plottable: a plain delete (or deleteLater) leaves no dead pointer
  in the plot, its group or its legend row.
*/
void QCustomPlot::plottableDestroyed(QCPAbstractPlottable* plottable)
{
    QPointer<QCPPlottableGroup> group = unlistPlottable(plottable);
    if (group)
    {
        group->updateLegendRow();
        qcpEmit(group.data(), this, &QCPPlottableGroup::membersChanged);
    }
}

/*! \overload

  Removes and deletes the plottable by its \a index.
*/
bool QCustomPlot::removePlottable(int index)
{
    QCP_CHECK_OR_RETURN(!mBeingDestroyed, "the plot is being destroyed", false);
    if (index >= 0 && index < mPlottables.size())
        return removePlottable(mPlottables[index]);
    else
    {
        qDebug() << Q_FUNC_INFO << "index out of bounds:" << index;
        return false;
    }
}

/*!
  Removes all plottables from the plot and deletes them. Corresponding legend items are also
  removed from the default legend (QCustomPlot::legend).

  Returns the number of plottables removed.

  \see removePlottable
*/
/*!
  Creates an empty \ref QCPPlottableGroup named \a name. The plot owns it.

  \see removePlottableGroup
*/
QCPPlottableGroup* QCustomPlot::addPlottableGroup(const QString& name)
{
    QCP_CHECK_OR_RETURN(!mBeingDestroyed, "the plot is being destroyed", nullptr);
    auto* group = new QCPPlottableGroup(this, name);
    mPlottableGroups.append(group);
    return group;
}

/*!
  Deletes \a group. Its members stay, and get their own legend rows back.

  Returns false if \a group isn't one of this plot's groups.
*/
bool QCustomPlot::removePlottableGroup(QCPPlottableGroup* group)
{
    QCP_CHECK_OR_RETURN(!mBeingDestroyed, "the plot is being destroyed", false);
    if (!mPlottableGroups.contains(group))
        return false;
    delete group;
    return true;
}

int QCustomPlot::clearPlottables()
{
    QCP_CHECK_OR_RETURN(!mBeingDestroyed, "the plot is being destroyed", 0);
    int c = mPlottables.size();
    while (!mPlottables.isEmpty())
        deletePlottable(mPlottables.last());
    return c;
}

/*!
  Returns the number of currently existing plottables in the plot

  \see plottable
*/
int QCustomPlot::plottableCount() const
{
    return mPlottables.size();
}

/*!
  Returns a list of the selected plottables. If no plottables are currently selected, the list is
  empty.

  There is a convenience function if you're only interested in selected graphs, see \ref
  selectedGraphs.

  \see setInteractions, QCPAbstractPlottable::setSelectable, QCPAbstractPlottable::setSelection
*/
QList<QCPAbstractPlottable*> QCustomPlot::selectedPlottables() const
{
    QList<QCPAbstractPlottable*> result;
    for (QCPAbstractPlottable* plottable : mPlottables)
    {
        if (plottable->selected())
            result.append(plottable);
    }
    return result;
}

/*!
  Returns any plottable at the pixel position \a pos. Since it can capture all plottables, the
  return type is the abstract base class of all plottables, QCPAbstractPlottable.

  For details, and if you wish to specify a certain plottable type (e.g. QCPGraph), see the
  template method plottableAt<PlottableType>()

  \see plottableAt<PlottableType>(), itemAt, layoutElementAt
*/
QCPAbstractPlottable* QCustomPlot::plottableAt(const QPointF& pos, bool onlySelectable,
                                               int* dataIndex) const
{
    return plottableAt<QCPAbstractPlottable>(pos, onlySelectable, dataIndex);
}

/*!
  Returns whether this QCustomPlot instance contains the \a plottable.
*/
bool QCustomPlot::hasPlottable(QCPAbstractPlottable* plottable) const
{
    return mPlottables.contains(plottable);
}

/*!
  Returns the graph with \a index. If the index is invalid, returns \c nullptr.

  There is an overloaded version of this function with no parameter which returns the last created
  graph, see QCustomPlot::graph()

  \see graphCount, addGraph
*/
QCPGraph* QCustomPlot::graph(int index) const
{
    if (index >= 0 && index < mGraphs.size())
    {
        return mGraphs.at(index);
    }
    else
    {
        qDebug() << Q_FUNC_INFO << "index out of bounds:" << index;
        return nullptr;
    }
}

/*! \overload

  Returns the last graph, that was created with \ref addGraph. If there are no graphs in the plot,
  returns \c nullptr.

  \see graphCount, addGraph
*/
QCPGraph* QCustomPlot::graph() const
{
    if (!mGraphs.isEmpty())
    {
        return mGraphs.last();
    }
    else
        return nullptr;
}

/*!
  Creates a new graph inside the plot. If \a keyAxis and \a valueAxis are left unspecified (0), the
  bottom (xAxis) is used as key and the left (yAxis) is used as value axis. If specified, \a
  keyAxis and \a valueAxis must reside in this QCustomPlot.

  \a keyAxis will be used as key axis (typically "x") and \a valueAxis as value axis (typically
  "y") for the graph.

  Returns a pointer to the newly created graph, or \c nullptr if adding the graph failed.

  \see graph, graphCount, removeGraph, clearGraphs
*/
std::pair<QCPAxis*, QCPAxis*> QCustomPlot::axesForAdd() const
{
    QCP_CHECK_OR_RETURN(!mBeingDestroyed, "the plot is being destroyed", {});
    const QList<QCPAxisRect*> rects = axisRects();
    QCP_CHECK_OR_RETURN(!rects.isEmpty(), "the plot has no axis rect", {});
    const QList<QCPAxis*> keyAxes = rects.first()->axes(QCPAxis::atBottom);
    const QList<QCPAxis*> valueAxes = rects.first()->axes(QCPAxis::atLeft);
    QCP_CHECK_OR_RETURN(!keyAxes.isEmpty() && !valueAxes.isEmpty(),
                        "the default axis rect has no bottom or left axis", {});
    return {keyAxes.first(), valueAxes.first()};
}

QCPGraph* QCustomPlot::addGraph(QCPAxis* keyAxis, QCPAxis* valueAxis)
{
    QCP_CHECK_OR_RETURN(!mBeingDestroyed, "the plot is being destroyed", nullptr);
    if (!keyAxis)
        keyAxis = xAxis;
    if (!valueAxis)
        valueAxis = yAxis;
    if (!keyAxis || !valueAxis)
    {
        qDebug() << Q_FUNC_INFO
                 << "can't use default QCustomPlot xAxis or yAxis, because at least one is invalid "
                    "(has been deleted)";
        return nullptr;
    }
    if (keyAxis->parentPlot() != this || valueAxis->parentPlot() != this)
    {
        qDebug() << Q_FUNC_INFO
                 << "passed keyAxis or valueAxis doesn't have this QCustomPlot as parent";
        return nullptr;
    }

    QCPGraph* newGraph = new QCPGraph(keyAxis, valueAxis);
    newGraph->setName(QLatin1String("Graph ") + QString::number(mGraphs.size()));
    return newGraph;
}

/*!
  Removes the specified \a graph from the plot and deletes it. If necessary, the corresponding
  legend item is also removed from the default legend (QCustomPlot::legend). If any other graphs in
  the plot have a channel fill set towards the removed graph, the channel fill property of those
  graphs is reset to \c nullptr (no channel fill).

  Returns true on success.

  \see clearGraphs
*/
bool QCustomPlot::removeGraph(QCPGraph* graph)
{
    return removePlottable(graph);
}

/*! \overload

  Removes and deletes the graph by its \a index.
*/
bool QCustomPlot::removeGraph(int index)
{
    QCP_CHECK_OR_RETURN(!mBeingDestroyed, "the plot is being destroyed", false);
    if (index >= 0 && index < mGraphs.size())
        return removeGraph(mGraphs[index]);
    else
        return false;
}

/*!
  Removes all graphs from the plot and deletes them. Corresponding legend items are also removed
  from the default legend (QCustomPlot::legend).

  Returns the number of graphs removed.

  \see removeGraph
*/
int QCustomPlot::clearGraphs()
{
    QCP_CHECK_OR_RETURN(!mBeingDestroyed, "the plot is being destroyed", 0);
    int c = mGraphs.size();
    while (!mGraphs.isEmpty())
        (void)removeGraph(mGraphs.last());
    return c;
}

/*!
  Returns the number of currently existing graphs in the plot

  \see graph, addGraph
*/
int QCustomPlot::graphCount() const
{
    return mGraphs.size();
}

/*!
  Returns a list of the selected graphs. If no graphs are currently selected, the list is empty.

  If you are not only interested in selected graphs but other plottables like QCPCurve, QCPBars,
  etc., use \ref selectedPlottables.

  \see setInteractions, selectedPlottables, QCPAbstractPlottable::setSelectable,
  QCPAbstractPlottable::setSelection
*/
QList<QCPGraph*> QCustomPlot::selectedGraphs() const
{
    QList<QCPGraph*> result;
    for (QCPGraph* graph : mGraphs)
    {
        if (graph->selected())
            result.append(graph);
    }
    return result;
}

/*!
  Returns the item with \a index. If the index is invalid, returns \c nullptr.

  There is an overloaded version of this function with no parameter which returns the last added
  item, see QCustomPlot::item()

  \see itemCount
*/
QCPAbstractItem* QCustomPlot::item(int index) const
{
    if (index >= 0 && index < mItems.size())
    {
        return mItems.at(index);
    }
    else
    {
        qDebug() << Q_FUNC_INFO << "index out of bounds:" << index;
        return nullptr;
    }
}

/*! \overload

  Returns the last item that was added to this plot. If there are no items in the plot,
  returns \c nullptr.

  \see itemCount
*/
QCPAbstractItem* QCustomPlot::item() const
{
    if (!mItems.isEmpty())
    {
        return mItems.last();
    }
    else
        return nullptr;
}

/*!
  Removes the specified item from the plot and deletes it.

  Returns true on success.

  \see clearItems
*/
bool QCustomPlot::removeItem(QCPAbstractItem* item)
{
    QCP_CHECK_OR_RETURN(!mBeingDestroyed, "the plot is being destroyed", false);
    // One search, not contains() then removeOne(); it stays a search (not item->mRegisteredPlot)
    // because the caller's pointer may already be dangling.
    if (const qsizetype index = mItems.indexOf(item); index >= 0)
    {
        // Unlisted first: a slot on destroyed() that calls removeItem or clearItems must find it gone.
        mItems.removeAt(index);
        item->mRegisteredPlot = nullptr;
        delete item;
        ++mItemsEpoch;
        return true;
    }
    else
    {
        qDebug() << Q_FUNC_INFO << "item not in list:" << reinterpret_cast<quintptr>(item);
        return false;
    }
}

/*! \overload

  Removes and deletes the item by its \a index.
*/
bool QCustomPlot::removeItem(int index)
{
    QCP_CHECK_OR_RETURN(!mBeingDestroyed, "the plot is being destroyed", false);
    if (index >= 0 && index < mItems.size())
        return removeItem(mItems[index]);
    else
    {
        qDebug() << Q_FUNC_INFO << "index out of bounds:" << index;
        return false;
    }
}

/*!
  Removes all items from the plot and deletes them.

  Returns the number of items removed.

  \see removeItem
*/
int QCustomPlot::clearItems()
{
    QCP_CHECK_OR_RETURN(!mBeingDestroyed, "the plot is being destroyed", 0);
    return deleteAllItems();
}

int QCustomPlot::deleteAllItems()
{
    // All unlisted before the first delete, as in removeItem: a slot on destroyed() finds none of
    // them. The guarded snapshot skips any that such a slot deletes itself.
    const auto items = guarded(mItems);
    for (QCPAbstractItem* item : std::as_const(mItems))
        item->mRegisteredPlot = nullptr;
    QSet<QCPLayerable*> dying;
    for (const auto& item : items)
        if (item)
            dying.insert(item.data());
    mItems.clear();
    // One pass per layer instead of one removeChild() per item, and mLayer is cleared so the
    // destructors do not search the layer again.
    for (QCPLayer* layer : std::as_const(mLayers))
        layer->removeChildren(dying);
    // Oldest first: Qt's children indexOf finds each at the front, where erase is a pointer bump.
    for (const auto& item : items)
        delete item.data();
    if (!items.isEmpty())
        ++mItemsEpoch;
    return items.size();
}

/*!
  Returns the number of currently existing items in the plot

  \see item
*/
int QCustomPlot::itemCount() const
{
    return mItems.size();
}

/*!
  Returns a list of the selected items. If no items are currently selected, the list is empty.

  \see setInteractions, QCPAbstractItem::setSelectable, QCPAbstractItem::setSelected
*/
QList<QCPAbstractItem*> QCustomPlot::selectedItems() const
{
    QList<QCPAbstractItem*> result;
    for (QCPAbstractItem* item : mItems)
    {
        if (item->selected())
            result.append(item);
    }
    return result;
}

/*!
  Returns the item at the pixel position \a pos. Since it can capture all items, the
  return type is the abstract base class of all items, QCPAbstractItem.

  For details, and if you wish to specify a certain item type (e.g. QCPItemLine), see the
  template method itemAt<ItemType>()

  \see itemAt<ItemType>(), plottableAt, layoutElementAt
*/
QCPAbstractItem* QCustomPlot::itemAt(const QPointF& pos, bool onlySelectable) const
{
    return itemAt<QCPAbstractItem>(pos, onlySelectable);
}

/*!
  Returns whether this QCustomPlot contains the \a item.

  \see item
*/
bool QCustomPlot::hasItem(QCPAbstractItem* item) const
{
    return mItems.contains(item);
}

/*!
  Returns the layer with the specified \a name. If there is no layer with the specified name, \c
  nullptr is returned.

  Layer names are case-sensitive.

  \see addLayer, moveLayer, removeLayer
*/
QCPLayer* QCustomPlot::layer(const QString& name) const
{
    for (QCPLayer* layer : mLayers)
    {
        if (layer->name() == name)
            return layer;
    }
    return nullptr;
}

/*! \overload

  Returns the layer by \a index. If the index is invalid, \c nullptr is returned.

  \see addLayer, moveLayer, removeLayer
*/
QCPLayer* QCustomPlot::layer(int index) const
{
    if (index >= 0 && index < mLayers.size())
    {
        return mLayers.at(index);
    }
    else
    {
        qDebug() << Q_FUNC_INFO << "index out of bounds:" << index;
        return nullptr;
    }
}

/*!
  Returns the layer that is set as current layer (see \ref setCurrentLayer).
*/
QCPLayer* QCustomPlot::currentLayer() const
{
    return mCurrentLayer;
}

/*!
  Sets the layer with the specified \a name to be the current layer. All layerables (\ref
  QCPLayerable), e.g. plottables and items, are created on the current layer, except items: they
  default to the "annotations" layer (see \ref QCPAbstractItem).

  Returns true on success, i.e. if there is a layer with the specified \a name in the QCustomPlot.

  Layer names are case-sensitive.

  \see addLayer, moveLayer, removeLayer, QCPLayerable::setLayer
*/
bool QCustomPlot::setCurrentLayer(const QString& name)
{
    QCP_CHECK_OR_RETURN(!mBeingDestroyed, "the plot is being destroyed", false);
    if (QCPLayer* newCurrentLayer = layer(name))
    {
        return setCurrentLayer(newCurrentLayer);
    }
    else
    {
        qDebug() << Q_FUNC_INFO << "layer with name doesn't exist:" << name;
        return false;
    }
}

/*! \overload

  Sets the provided \a layer to be the current layer.

  Returns true on success, i.e. when \a layer is a valid layer in the QCustomPlot.

  \see addLayer, moveLayer, removeLayer
*/
bool QCustomPlot::setCurrentLayer(QCPLayer* layer)
{
    QCP_CHECK_OR_RETURN(!mBeingDestroyed, "the plot is being destroyed", false);
    if (!mLayers.contains(layer))
    {
        qDebug() << Q_FUNC_INFO
                 << "layer not a layer of this QCustomPlot:" << reinterpret_cast<quintptr>(layer);
        return false;
    }

    mCurrentLayer = layer;
    return true;
}

/*!
  Returns the number of currently existing layers in the plot

  \see layer, addLayer
*/
int QCustomPlot::layerCount() const
{
    return mLayers.size();
}

/*!
  Adds a new layer to this QCustomPlot instance. The new layer will have the name \a name, which
  must be unique. Depending on \a insertMode, it is positioned either below or above \a otherLayer.

  Returns true on success, i.e. if there is no other layer named \a name and \a otherLayer is a
  valid layer inside this QCustomPlot.

  If \a otherLayer is 0, the highest layer in the QCustomPlot will be used.

  For an explanation of what layers are in QCustomPlot, see the documentation of \ref QCPLayer.

  \see layer, moveLayer, removeLayer
*/
bool QCustomPlot::addLayer(const QString& name, QCPLayer* otherLayer,
                           QCustomPlot::LayerInsertMode insertMode)
{
    QCP_CHECK_OR_RETURN(!mBeingDestroyed, "the plot is being destroyed", false);
    // A plain delete can remove every layer (removeLayer keeps one): the new one is then the only one.
    const bool onlyLayer = mLayers.isEmpty();
    if (!otherLayer && !onlyLayer)
        otherLayer = mLayers.last();
    if (!onlyLayer && !mLayers.contains(otherLayer))
    {
        qDebug() << Q_FUNC_INFO << "otherLayer not a layer of this QCustomPlot:"
                 << reinterpret_cast<quintptr>(otherLayer);
        return false;
    }
    if (layer(name))
    {
        qDebug() << Q_FUNC_INFO << "A layer exists already with the name" << name;
        return false;
    }

    QCPLayer* newLayer = new QCPLayer(this, name);
    mLayers.insert(onlyLayer ? 0 : otherLayer->index() + (insertMode == limAbove ? 1 : 0), newLayer);
    updateLayerIndices();
    setupPaintBuffers(); // associates new layer with the appropriate paint buffer
    // Buffers are matched to layers by index, so inserting a layer shifts later layers onto
    // buffers that still hold another layer's pixels: repaint them all instead of compositing
    // stale content.
    for (auto& buffer : std::as_const(mPaintBuffers))
        buffer->setInvalidated();
    return true;
}

/*!
  Removes the specified \a layer and returns true on success.

  All layerables (e.g. plottables and items) on the removed layer will be moved to the layer below
  \a layer. If \a layer is the bottom layer, the layerables are moved to the layer above. In both
  cases, the total rendering order of all layerables in the QCustomPlot is preserved.

  If \a layer is the current layer (\ref setCurrentLayer), the layer below (or above, if bottom
  layer) becomes the new current layer.

  It is not possible to remove the last layer of the plot.

  \see layer, addLayer, moveLayer
*/
bool QCustomPlot::removeLayer(QCPLayer* layer)
{
    const QCPEmissionScope scope(this);
    QCP_CHECK_OR_RETURN(!mBeingDestroyed, "the plot is being destroyed", false);
    QCP_CHECK_OR_RETURN(mLayers.contains(layer), "the layer is not a layer of this plot", false);
    QCP_CHECK_OR_RETURN(mLayers.size() >= 2, "can't remove the plot's last layer", false);

    unlistLayer(layer);
    delete layer;
    return true;
}

void QCustomPlot::unlistLayer(QCPLayer* layer)
{
    // Children go to the layer below (above, for the bottom layer), keeping the total rendering
    // order. The last layer has no neighbour: ~QCPLayer detaches its children.
    const int removedIndex = layer->index();
    const bool isFirstLayer = removedIndex == 0;
    QCPLayer* targetLayer = mLayers.size() < 2
        ? nullptr
        : mLayers.at(isFirstLayer ? removedIndex + 1 : removedIndex - 1);
    if (targetLayer)
    {
        QList<QCPLayerable*> children = layer->children();
        if (isFirstLayer) // prepend in reverse order (such that relative order stays the same)
            std::reverse(children.begin(), children.end());
        for (QCPLayerable* child : children)
            child->moveToLayer(targetLayer, isFirstLayer);
    }

    if (layer == mCurrentLayer)
        mCurrentLayer = targetLayer;

    if (QSharedPointer<QCPAbstractPaintBuffer> pb = layer->mPaintBuffer.toStrongRef())
        pb->setInvalidated();

    // Later replots and renders must never dereference the stale QCPLayer* keys.
    delete mPlottableRhiLayers.take(layer);
    delete mScatterRhiLayers.take(layer);
    mLayers.removeOne(layer);
    updateLayerIndices();
}

/*!
  Moves the specified \a layer either above or below \a otherLayer. Whether it's placed above or
  below is controlled with \a insertMode.

  Returns true on success, i.e. when both \a layer and \a otherLayer are valid layers in the
  QCustomPlot.

  \see layer, addLayer, moveLayer
*/
bool QCustomPlot::moveLayer(QCPLayer* layer, QCPLayer* otherLayer,
                            QCustomPlot::LayerInsertMode insertMode)
{
    QCP_CHECK_OR_RETURN(!mBeingDestroyed, "the plot is being destroyed", false);
    if (!mLayers.contains(layer))
    {
        qDebug() << Q_FUNC_INFO
                 << "layer not a layer of this QCustomPlot:" << reinterpret_cast<quintptr>(layer);
        return false;
    }
    if (!mLayers.contains(otherLayer))
    {
        qDebug() << Q_FUNC_INFO << "otherLayer not a layer of this QCustomPlot:"
                 << reinterpret_cast<quintptr>(otherLayer);
        return false;
    }

    if (layer->index() > otherLayer->index())
        mLayers.move(layer->index(), otherLayer->index() + (insertMode == limAbove ? 1 : 0));
    else if (layer->index() < otherLayer->index())
        mLayers.move(layer->index(), otherLayer->index() + (insertMode == limAbove ? 0 : -1));

    // invalidate the paint buffers that are responsible for the layers:
    if (QSharedPointer<QCPAbstractPaintBuffer> pb = layer->mPaintBuffer.toStrongRef())
        pb->setInvalidated();
    if (QSharedPointer<QCPAbstractPaintBuffer> pb = otherLayer->mPaintBuffer.toStrongRef())
        pb->setInvalidated();

    updateLayerIndices();
    return true;
}

/*!
  Returns the number of axis rects in the plot.

  All axis rects can be accessed via QCustomPlot::axisRect().

  Initially, only one axis rect exists in the plot.

  \see axisRect, axisRects
*/
int QCustomPlot::axisRectCount() const
{
    return axisRects().size();
}

/*!
  Returns the axis rect with \a index.

  Initially, only one axis rect (with index 0) exists in the plot. If multiple axis rects were
  added, all of them may be accessed with this function in a linear fashion (even when they are
  nested in a layout hierarchy or inside other axis rects via QCPAxisRect::insetLayout).

  The order of the axis rects is given by the fill order of the \ref QCPLayout that is holding
  them. For example, if the axis rects are in the top level grid layout (accessible via \ref
  QCustomPlot::plotLayout), they are ordered from left to right, top to bottom, if the layout's
  default \ref QCPLayoutGrid::setFillOrder "setFillOrder" of \ref QCPLayoutGrid::foColumnsFirst
  "foColumnsFirst" wasn't changed.

  If you want to access axis rects by their row and column index, use the layout interface. For
  example, use \ref QCPLayoutGrid::element of the top level grid layout, and \c qobject_cast the
  returned layout element to \ref QCPAxisRect. (See also \ref thelayoutsystem.)

  \see axisRectCount, axisRects, QCPLayoutGrid::setFillOrder
*/
QCPAxisRect* QCustomPlot::firstAxisRect() const
{
    // Same depth-first order as axisRects(), stopping at the first rect: allocating the full list
    // per item dominated the add benchmark.
    QVarLengthArray<QCPLayoutElement*, 16> stack;
    if (mPlotLayout)
        stack.append(mPlotLayout);
    while (!stack.isEmpty())
    {
        QCPLayoutElement* element = stack.back();
        stack.pop_back();
        for (QCPLayoutElement* child : element->elements(false))
        {
            if (!child)
                continue;
            if (auto* rect = qobject_cast<QCPAxisRect*>(child))
                return rect;
            stack.append(child);
        }
    }
    return nullptr;
}

QCPAxisRect* QCustomPlot::axisRect(int index) const
{
    const QList<QCPAxisRect*> rectList = axisRects();
    if (index >= 0 && index < rectList.size())
    {
        return rectList.at(index);
    }
    else
    {
        qDebug() << Q_FUNC_INFO << "invalid axis rect index" << index;
        return nullptr;
    }
}

/*!
  Returns all axis rects in the plot.

  The order of the axis rects is given by the fill order of the \ref QCPLayout that is holding
  them. For example, if the axis rects are in the top level grid layout (accessible via \ref
  QCustomPlot::plotLayout), they are ordered from left to right, top to bottom, if the layout's
  default \ref QCPLayoutGrid::setFillOrder "setFillOrder" of \ref QCPLayoutGrid::foColumnsFirst
  "foColumnsFirst" wasn't changed.

  \see axisRectCount, axisRect, QCPLayoutGrid::setFillOrder
*/
QList<QCPAxisRect*> QCustomPlot::axisRects() const
{
    QList<QCPAxisRect*> result;
    for (QCPArea* area : areas())
        if (QCPAxisRect* rect = qobject_cast<QCPAxisRect*>(area))
            result.append(rect);
    return result;
}

QList<QCPArea*> QCustomPlot::areas() const
{
    QList<QCPArea*> result;
    QStack<QCPLayoutElement*> elementStack;
    if (mPlotLayout)
        elementStack.push(mPlotLayout);

    while (!elementStack.isEmpty())
    {
        for (QCPLayoutElement* element : elementStack.pop()->elements(false))
        {
            if (element)
            {
                elementStack.push(element);
                if (QCPArea* area = qobject_cast<QCPArea*>(element))
                    result.append(area);
            }
        }
    }

    return result;
}

/*!
  Returns the area \a pos falls in: the deepest area among the layout elements at that pixel, so a
  rect inside a grid of the plot layout answers, not the layout. Null where no area is.

  \see layoutElementAt, axisRectAt
*/
QCPArea* QCustomPlot::areaAt(const QPointF& pos) const
{
    for (QCPLayoutElement* element = layoutElementAt(pos); element; element = element->layout())
        if (QCPArea* area = qobject_cast<QCPArea*>(element))
            return area;
    return nullptr;
}

/*!
  Returns the layout element at pixel position \a pos. If there is no element at that position,
  returns \c nullptr.

  Only visible elements are used. If \ref QCPLayoutElement::setVisible on the element itself or on
  any of its parent elements is set to false, it will not be considered.

  \see itemAt, plottableAt
*/
QCPLayoutElement* QCustomPlot::layoutElementAt(const QPointF& pos) const
{
    QCPLayoutElement* currentElement = mPlotLayout;
    bool searchSubElements = true;
    while (searchSubElements && currentElement)
    {
        searchSubElements = false;
        for (QCPLayoutElement* subElement : currentElement->elements(false))
        {
            if (subElement && subElement->realVisibility()
                && subElement->selectTest(pos, false) >= 0)
            {
                currentElement = subElement;
                searchSubElements = true;
                break;
            }
        }
    }
    return currentElement;
}

/*!
  Returns the layout element of type \ref QCPAxisRect at pixel position \a pos. This method ignores
  other layout elements even if they are visually in front of the axis rect (e.g. a \ref
  QCPLegend). If there is no axis rect at that position, returns \c nullptr.

  Only visible axis rects are used. If \ref QCPLayoutElement::setVisible on the axis rect itself or
  on any of its parent elements is set to false, it will not be considered.

  \see layoutElementAt
*/
QCPAxisRect* QCustomPlot::axisRectAt(const QPointF& pos) const
{
    for (QCPLayoutElement* el = layoutElementAt(pos); el; el = el->layout())
    {
        if (auto* ar = qobject_cast<QCPAxisRect*>(el))
            return ar;
    }
    return nullptr;
}

/*!
  Returns the axes that currently have selected parts, i.e. whose selection state is not \ref
  QCPAxis::spNone.

  \see selectedPlottables, selectedLegends, setInteractions, QCPAxis::setSelectedParts,
  QCPAxis::setSelectableParts
*/
QList<QCPAxis*> QCustomPlot::selectedAxes() const
{
    QList<QCPAxis*> result;
    for (QCPAxisRect* rect : axisRects())
        for (QCPAxis* axis : rect->axes())
            if (axis->selectedParts() != QCPAxis::spNone)
                result.append(axis);
    return result;
}

/*!
  Returns the legends that currently have selected parts, i.e. whose selection state is not \ref
  QCPLegend::spNone.

  \see selectedPlottables, selectedAxes, setInteractions, QCPLegend::setSelectedParts,
  QCPLegend::setSelectableParts, QCPLegend::selectedItems
*/
QList<QCPLegend*> QCustomPlot::selectedLegends() const
{
    QList<QCPLegend*> result;

    QStack<QCPLayoutElement*> elementStack;
    if (mPlotLayout)
        elementStack.push(mPlotLayout);

    while (!elementStack.isEmpty())
    {
        for (QCPLayoutElement* subElement : elementStack.pop()->elements(false))
        {
            if (subElement)
            {
                elementStack.push(subElement);
                if (QCPLegend* leg = qobject_cast<QCPLegend*>(subElement))
                {
                    if (leg->selectedParts() != QCPLegend::spNone)
                        result.append(leg);
                }
            }
        }
    }

    return result;
}

/*!
  Deselects all layerables (plottables, items, axes, legends,...) of the QCustomPlot.

  Since calling this function is not a user interaction, this does not emit the \ref
  selectionChangedByUser signal. The individual selectionChanged signals are emitted though, if the
  objects were previously selected.

  \see setInteractions, selectedPlottables, selectedItems, selectedAxes, selectedLegends
*/
void QCustomPlot::deselectAll()
{
    for (const auto& layerable : guardedSelectedLayerables(mLayers))
        if (layerable)
            layerable->deselectEvent(nullptr);
}

/*! \internal

  Sends a deselectEvent to every layerable but \a kept whose selection category is enabled in the
  interactions. Returns whether any selection changed.
*/
bool QCustomPlot::deselectAllExcept(const QCPLayerable* kept)
{
    bool changed = false;
    for (const auto& layerable : guardedSelectedLayerables(mLayers, kept))
    {
        if (layerable
            && mInteractions.testFlag(layerable->selectionCategory()))
        {
            bool selChanged = false;
            layerable->deselectEvent(&selChanged);
            changed |= selChanged;
        }
    }
    return changed;
}

/*!
  Causes a complete replot into the internal paint buffer(s). Finally, the widget surface is
  refreshed with the new buffer contents. This is the method that must be called to make changes to
  the plot, e.g. on the axis ranges or data points of graphs, visible.

  The parameter \a refreshPriority can be used to fine-tune the timing of the replot. For example
  if your application calls \ref replot very quickly in succession (e.g. multiple independent
  functions change some aspects of the plot and each wants to make sure the change gets replotted),
  it is advisable to set \a refreshPriority to \ref QCustomPlot::rpQueuedReplot. This way, the
  actual replotting is deferred to the next event loop iteration. Multiple successive calls of \ref
  replot with this priority will only cause a single replot, avoiding redundant replots and
  improving performance.

  Under a few circumstances, QCustomPlot causes a replot by itself. Those are resize events of the
  QCustomPlot widget and user interactions (object selection and range dragging/zooming).

  Before the replot happens, the signal \ref beforeReplot is emitted. After the replot, \ref
  afterReplot is delivered when the outermost NeoQCP call returns (\ref deliver). It is safe to mutually connect the replot slot
  with any of those signals on two QCustomPlots to make them replot synchronously, it won't cause
  an infinite recursion.

  If a layer is in mode \ref QCPLayer::lmBuffered (\ref QCPLayer::setMode), it is also possible to
  replot only that specific layer via \ref QCPLayer::replot. See the documentation there for
  details.

  \see replotTime
*/
void QCustomPlot::requestDataSwap()
{
    // Leading-edge window: later requests ride along instead of restarting the
    // timer, so the swap latency is bounded by one window under streaming data.
    if (!mDataSwapTimer.isActive())
    {
        mDataSwapTimer.start(mDataSwapDebounceMs);
        mDataSwapRequested.start();
    }
}

void QCustomPlot::setDataSwapDebounceMs(int ms)
{
    mDataSwapDebounceMs = qMax(0, ms);
}

void QCustomPlot::setDataSwapMaxWaitMs(int ms)
{
    mDataSwapMaxWaitMs = qMax(0, ms);
}

void QCustomPlot::commitPendingData()
{
    // Swapping mid-pan costs one full repaint in the middle of a drag; wait for
    // the view to be still for one window, but never longer than the cap so an
    // auto-scrolling plot still gets its data.
    const bool panning = mLastTranslation.isValid()
        && mLastTranslation.elapsed() < mDataSwapDebounceMs;
    const bool capped = mDataSwapRequested.elapsed() >= mDataSwapMaxWaitMs;
    if (panning && !capped)
    {
        mDataSwapTimer.start(mDataSwapDebounceMs);
        return;
    }

    if (commitEveryPendingSource())
        replot(QCustomPlot::rpQueuedReplot);
}

bool QCustomPlot::commitEveryPendingSource()
{
    // A commit's busyChanged slot may remove plottables: walk a snapshot, skip the gone ones.
    bool committed = false;
    const QList<QPointer<QCPAbstractPlottable>> plottables(mPlottables.cbegin(), mPlottables.cend());
    for (const auto& plottable : plottables)
        if (plottable)
            committed |= plottable->commitPendingData();
    return committed;
}

void QCustomPlot::requestRepaint()
{
    if (mBeingDestroyed)
        return;
    if (mReplotting)
        mRepaintAskedDuringReplot = true;
    else
        scheduleRepaint();
}

// Refuses during a replot: the end of that replot reschedules what was marked after its draw.
void QCustomPlot::scheduleRepaint()
{
    if (mBeingDestroyed || mReplotting || mReplotQueued)
        return;
    if (replotDeferredWhileHidden())
    {
        mReplotSkippedWhileHidden = true;
        return;
    }
    mReplotQueued = true;
    // A synchronous replot() in between already drew it, and a later request posts its own timer:
    // only the latest request's timer replots.
    const quint64 ticket = ++mRepaintTicket;
    QTimer::singleShot(0, this, [this, ticket] {
        if (mReplotQueued && ticket == mRepaintTicket)
            replot();
    });
}

// Exports draw the model directly (draw() lays out first), so the screen's queued repaint is not
// needed: it would draw a second time, at the widget's size. The screen catches up next turn.
void QCustomPlot::settle()
{
    const QCPEmissionScope scope(this);
    if (mBeingDestroyed || mReplotting)
        return;
    if (mThemeDirty) // a theme change waits for the event loop; settling means the new picture
    {
        mThemeDirty = false;
        applyTheme();
    }
    keepNotificationLayerOnTop();
    if (mDataSwapTimer.isActive())
    {
        mDataSwapTimer.stop();
        if (commitEveryPendingSource())
            scheduleRepaint();
    }
}

void QCustomPlot::keepNotificationLayerOnTop()
{
    if (!mOverlay)
        return;
    if (auto* notifLayer = layer(QLatin1String("notification"));
        notifLayer && notifLayer != mLayers.last())
    {
        mLayers.removeOne(notifLayer);
        mLayers.append(notifLayer);
        updateLayerIndices();
    }
}

bool QCustomPlot::replotDeferredWhileHidden() const
{
    return mSkipReplotsWhenHidden && mWasShown && !isVisible();
}

// Flags are cleared at the end of a replot, so a layer marked after it was drawn is marked again.
bool QCustomPlot::remarkLayersDirtiedAfterDraw()
{
    bool any = false;
    for (auto* layer : std::as_const(mLayers))
    {
        if (!layer->dirtiedAfterDraw())
            continue;
        if (auto pb = layer->mPaintBuffer.toStrongRef())
            pb->setInvalidated();
        any = true;
    }
    return any;
}

void QCustomPlot::deliver(QObject* sender, std::function<void()> emission)
{
    if (!mEmissions.isOpen())
        return emission();
    mEmissions.push(sender, std::move(emission));
}

void QCustomPlot::replot(QCustomPlot::RefreshPriority refreshPriority)
{
    PROFILE_HERE;
    const QCPEmissionScope scope(this);
    if (mBeingDestroyed)
        return;
    if (refreshPriority == QCustomPlot::rpQueuedReplot)
        return requestRepaint();

    if (mReplotting || mDeliveringReplotSignal) // a replot signal's slot calls replot
        return;

    if (replotDeferredWhileHidden())
    {
        // Defer all painting to the next show: showEvent() replots, since a show at the same
        // size (a tab switch, a sleep) does not run initialize(). Buffer dirty/invalidated
        // flags are intentionally left untouched. Never-shown widgets fall through: classic
        // behavior, required for offscreen rendering.
        mReplotQueued = false;
        mReplotSkippedWhileHidden = true;
        return;
    }

    mReplotQueued = false;
    syncViewportToWidgetSize();
    if (!emitReplotSignal(&QCustomPlot::beforeReplot))
        return;
    mReplotting = true;
    mRepaintAskedDuringReplot = false;
    keepNotificationLayerOnTop();

    QElapsedTimer replotTimer;
    replotTimer.start();

    const QPointer<QCustomPlot> alive(this);
    updateLayout();
    if (!alive)
        return;
    ensureAtLeastOneBufferDirty();
    // draw all layered objects (grid, axes, plottables, items, legend,...) into their buffers:
    setupPaintBuffers();
    for (auto it = mPlottableRhiLayers.begin(); it != mPlottableRhiLayers.end(); ++it)
    {
        QCPLayer* layer = it.key();
        auto pb = layer->mPaintBuffer.toStrongRef();
        if (!pb || !pb->contentDirty())
            continue;
        if (layer->canSkipRepaintForTranslation())
        {
            // Paint buffer skipped (compositor shifts old texture).
            // PRL geometry is unchanged — update per-draw offsets
            // to match the current pixel offset.
            QPointF offset = layer->pixelOffset();
            it.value()->setAllOffsets(static_cast<float>(offset.x()),
                                      static_cast<float>(offset.y()));
        }
        else
        {
            it.value()->clear();
        }
    }
    for (auto it = mScatterRhiLayers.begin(); it != mScatterRhiLayers.end(); ++it)
    {
        QCPLayer* layer = it.key();
        auto pb = layer->mPaintBuffer.toStrongRef();
        if (!pb || !pb->contentDirty())
            continue;
        if (layer->canSkipRepaintForTranslation())
        {
            QPointF offset = layer->pixelOffset();
            it.value()->setAllOffsets(static_cast<float>(offset.x()),
                                      static_cast<float>(offset.y()));
        }
        else
        {
            it.value()->clear();
        }
    }
    // One frame for the layer paints below: an everyFrame() recording is rebuilt once for it, and
    // the queries made while painting it read what it built.
    mFrameEpoch = qcp::nextFrameEpoch();
    bool translated = false;
    for (auto& layer : mLayers)
    {
        // Drawn, translated or kept as it is: this replot has dealt with every mark made so far.
        layer->mDrawnGeneration = mPaintGeneration;
        if (QSharedPointer<QCPAbstractPaintBuffer> pb = layer->mPaintBuffer.toStrongRef();
            pb && pb->contentDirty())
        {
            if (layer->canSkipRepaintForTranslation())
                translated = true;
            else
                layer->drawToPaintBuffer();
        }
    }
    if (translated)
        mLastTranslation.start();
    for (auto& buffer : mPaintBuffers)
    {
        buffer->setInvalidated(false);
        buffer->setContentDirty(false);
    }

    // QRhiWidget rendering is compositor-driven; always use update() to schedule the next frame.
    // repaint() would attempt a synchronous paint which is not compatible with QRhiWidget.
    update();

    mReplotTime = replotTimer.nsecsElapsed() * 1e-6;

    if (!qFuzzyIsNull(mReplotTimeAverage))
        mReplotTimeAverage = mReplotTimeAverage * 0.9
            + mReplotTime
                * 0.1; // exponential moving average with a time constant of 10 last replots
    else
        mReplotTimeAverage
            = mReplotTime; // no previous replots to average with, so initialize with replot time

    const bool asked = std::exchange(mRepaintAskedDuringReplot, false);
    const bool dirtied = remarkLayersDirtiedAfterDraw();
    deliver(this, [this] { emitReplotSignal(&QCustomPlot::afterReplot); });
    mReplotting = false;
    if (asked || dirtied)
        scheduleRepaint();
}

// Returns false when a slot deleted the plot.
bool QCustomPlot::emitReplotSignal(void (QCustomPlot::*signal)())
{
    const QPointer<QCustomPlot> alive(this);
    const bool outer = std::exchange(mDeliveringReplotSignal, true);
    (this->*signal)();
    if (!alive)
        return false;
    mDeliveringReplotSignal = outer;
    return true;
}

/*!
  Returns the time in milliseconds that the last replot took. If \a average is set to true, an
  exponential moving average over the last couple of replots is returned.

  \see replot
*/
double QCustomPlot::replotTime(bool average) const
{
    return average ? mReplotTimeAverage : mReplotTime;
}

/*!
  Rescales the axes such that all plottables (like graphs) in the plot are fully visible.

  if \a onlyVisiblePlottables is set to true, only the plottables that have their visibility set to
  true (QCPLayerable::setVisible), will be used to rescale the axes.

  \see QCPAbstractPlottable::rescaleAxes, QCPAxis::rescale
*/
void QCustomPlot::rescaleAxes(bool onlyVisiblePlottables)
{
    const QCPEmissionScope scope(this);
    QList<QPointer<QCPAxisBase>> allAxes;
    for (QCPArea* area : areas())
        allAxes << guarded(area->allAxes());

    for (const auto& axis : allAxes)
        if (axis) // a rangeChanged slot may delete the others
            axis->rescale(onlyVisiblePlottables);
}

/*!
  Saves a PDF with the vectorized plot to the file \a fileName. The axis ratio as well as the scale
  of texts and lines will be derived from the specified \a width and \a height. This means, the
  output will look like the normal on-screen output of a QCustomPlot widget with the corresponding
  pixel width and height. If either \a width or \a height is zero, the exported image will have the
  same dimensions as the QCustomPlot widget currently has.

  Setting \a exportPen to \ref QCP::epNoCosmetic allows to disable the use of cosmetic pens when
  drawing to the PDF file. Cosmetic pens are pens with numerical width 0, which are always drawn as
  a one pixel wide line, no matter what zoom factor is set in the PDF-Viewer. For more information
  about cosmetic pens, see the QPainter and QPen documentation.

  The objects of the plot will appear in the current selection state. If you don't want any
  selected objects to be painted in their selected look, deselect everything with \ref deselectAll
  before calling this function.

  Returns true on success.

  \warning
  \li If you plan on editing the exported PDF file with a vector graphics editor like Inkscape, it
  is advised to set \a exportPen to \ref QCP::epNoCosmetic to avoid losing those cosmetic lines
  (which might be quite many, because cosmetic pens are the default for e.g. axes and tick marks).
  \li If calling this function inside the constructor of the parent of the QCustomPlot widget
  (i.e. the MainWindow constructor, if QCustomPlot is inside the MainWindow), always provide
  explicit non-zero widths and heights. If you leave \a width or \a height as 0 (default), this
  function uses the current width and height of the QCustomPlot widget. However, in Qt, these
  aren't defined yet inside the constructor, so you would get an image that has strange
  widths/heights.

  \a pdfCreator and \a pdfTitle may be used to set the according metadata fields in the resulting
  PDF file.

  \note On Android systems, this method does nothing and issues an according qDebug warning
  message. This is also the case if for other reasons the define flag \c QT_NO_PRINTER is set.

  \see savePng, saveBmp, saveJpg, saveRastered
*/
bool QCustomPlot::savePdf([[maybe_unused]] const QString& fileName, [[maybe_unused]] int width,
                          [[maybe_unused]] int height, [[maybe_unused]] QCP::ExportPen exportPen,
                          [[maybe_unused]] const QString& pdfCreator,
                          [[maybe_unused]] const QString& pdfTitle)
{
    bool success = false;
#ifdef QT_NO_PRINTER
    qDebug() << Q_FUNC_INFO
             << "Qt was built without printer support (QT_NO_PRINTER). PDF not created.";
#else
    const QCPEmissionScope scope(this);
    const QPointer<QCustomPlot> alive(this); // an afterLayout slot may delete the plot
    settle();
    if (!alive)
        return false;
    int newWidth, newHeight;
    if (width == 0 || height == 0)
    {
        newWidth = this->width();
        newHeight = this->height();
    }
    else
    {
        newWidth = width;
        newHeight = height;
    }

    QPrinter printer(QPrinter::ScreenResolution);
    printer.setOutputFileName(fileName);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setColorMode(QPrinter::Color);
    printer.printEngine()->setProperty(QPrintEngine::PPK_Creator, pdfCreator);
    printer.printEngine()->setProperty(QPrintEngine::PPK_DocumentName, pdfTitle);
    syncViewportToWidgetSize();
    QRect oldViewport = viewport();
    setViewport(QRect(0, 0, newWidth, newHeight));

    QPageLayout pageLayout;
    pageLayout.setMode(QPageLayout::FullPageMode);
    pageLayout.setOrientation(QPageLayout::Portrait);
    pageLayout.setMargins(QMarginsF(0, 0, 0, 0));
    pageLayout.setPageSize(
        QPageSize(viewport().size(), QPageSize::Point, QString(), QPageSize::ExactMatch));
    printer.setPageLayout(pageLayout);

    QCPPainter printpainter;
    if (printpainter.begin(&printer))
    {
        printpainter.setMode(QCPPainter::pmVectorized);
        printpainter.setMode(QCPPainter::pmNoCaching);
        printpainter.setMode(QCPPainter::pmNonCosmetic, exportPen == QCP::epNoCosmetic);
        printpainter.setWindow(mViewport);
        if (mBackgroundBrush.style() != Qt::NoBrush && mBackgroundBrush.color() != Qt::white
            && mBackgroundBrush.color() != Qt::transparent
            && mBackgroundBrush.color().alpha()
                > 0) // draw pdf background color if not white/transparent
            printpainter.fillRect(viewport(), mBackgroundBrush);
        draw(&printpainter);
        printpainter.end();
        success = alive;
    }
    if (alive)
        restoreViewportAfterExport(oldViewport);
#endif // QT_NO_PRINTER
    return success;
}

/*!
  Saves a PNG image file to \a fileName on disc. The output plot will have the dimensions \a width
  and \a height in pixels, multiplied by \a scale. If either \a width or \a height is zero, the
  current width and height of the QCustomPlot widget is used instead. Line widths and texts etc.
  are not scaled up when larger widths/heights are used. If you want that effect, use the \a scale
  parameter.

  For example, if you set both \a width and \a height to 100 and \a scale to 2, you will end up with
  an image file of size 200*200 in which all graphical elements are scaled up by factor 2 (line
  widths, texts, etc.). This scaling is not done by stretching a 100*100 image, the result will have
  full 200*200 pixel resolution.

  If you use a high scaling factor, it is recommended to enable antialiasing for all elements by
  temporarily setting \ref QCustomPlot::setAntialiasedElements to \ref QCP::aeAll as this allows
  QCustomPlot to place objects with sub-pixel accuracy.

  image compression can be controlled with the \a quality parameter which must be between 0 and 100
  or -1 to use the default setting.

  The \a resolution will be written to the image file header and has no direct consequence for the
  quality or the pixel size. However, if opening the image with a tool which respects the metadata,
  it will be able to scale the image to match either a given size in real units of length (inch,
  centimeters, etc.), or the target display DPI. You can specify in which units \a resolution is
  given, by setting \a resolutionUnit. The \a resolution is converted to the format's expected
  resolution unit internally.

  Returns true on success. If this function fails, most likely the PNG format isn't supported by
  the system, see Qt docs about QImageWriter::supportedImageFormats().

  The objects of the plot will appear in the current selection state. If you don't want any selected
  objects to be painted in their selected look, deselect everything with \ref deselectAll before
  calling this function.

  If you want the PNG to have a transparent background, call \ref setBackground(const QBrush &brush)
  with no brush (Qt::NoBrush) or a transparent color (Qt::transparent), before saving.

  \warning If calling this function inside the constructor of the parent of the QCustomPlot widget
  (i.e. the MainWindow constructor, if QCustomPlot is inside the MainWindow), always provide
  explicit non-zero widths and heights. If you leave \a width or \a height as 0 (default), this
  function uses the current width and height of the QCustomPlot widget. However, in Qt, these
  aren't defined yet inside the constructor, so you would get an image that has strange
  widths/heights.

  \see savePdf, saveBmp, saveJpg, saveRastered
*/
bool QCustomPlot::savePng(const QString& fileName, int width, int height, double scale, int quality,
                          int resolution, QCP::ResolutionUnit resolutionUnit)
{
    return saveRastered(fileName, width, height, scale, "PNG", quality, resolution, resolutionUnit);
}

/*!
  Saves a JPEG image file to \a fileName on disc. The output plot will have the dimensions \a width
  and \a height in pixels, multiplied by \a scale. If either \a width or \a height is zero, the
  current width and height of the QCustomPlot widget is used instead. Line widths and texts etc.
  are not scaled up when larger widths/heights are used. If you want that effect, use the \a scale
  parameter.

  For example, if you set both \a width and \a height to 100 and \a scale to 2, you will end up with
  an image file of size 200*200 in which all graphical elements are scaled up by factor 2 (line
  widths, texts, etc.). This scaling is not done by stretching a 100*100 image, the result will have
  full 200*200 pixel resolution.

  If you use a high scaling factor, it is recommended to enable antialiasing for all elements by
  temporarily setting \ref QCustomPlot::setAntialiasedElements to \ref QCP::aeAll as this allows
  QCustomPlot to place objects with sub-pixel accuracy.

  image compression can be controlled with the \a quality parameter which must be between 0 and 100
  or -1 to use the default setting.

  The \a resolution will be written to the image file header and has no direct consequence for the
  quality or the pixel size. However, if opening the image with a tool which respects the metadata,
  it will be able to scale the image to match either a given size in real units of length (inch,
  centimeters, etc.), or the target display DPI. You can specify in which units \a resolution is
  given, by setting \a resolutionUnit. The \a resolution is converted to the format's expected
  resolution unit internally.

  Returns true on success. If this function fails, most likely the JPEG format isn't supported by
  the system, see Qt docs about QImageWriter::supportedImageFormats().

  The objects of the plot will appear in the current selection state. If you don't want any selected
  objects to be painted in their selected look, deselect everything with \ref deselectAll before
  calling this function.

  \warning If calling this function inside the constructor of the parent of the QCustomPlot widget
  (i.e. the MainWindow constructor, if QCustomPlot is inside the MainWindow), always provide
  explicit non-zero widths and heights. If you leave \a width or \a height as 0 (default), this
  function uses the current width and height of the QCustomPlot widget. However, in Qt, these
  aren't defined yet inside the constructor, so you would get an image that has strange
  widths/heights.

  \see savePdf, savePng, saveBmp, saveRastered
*/
bool QCustomPlot::saveJpg(const QString& fileName, int width, int height, double scale, int quality,
                          int resolution, QCP::ResolutionUnit resolutionUnit)
{
    return saveRastered(fileName, width, height, scale, "JPG", quality, resolution, resolutionUnit);
}

/*!
  Saves a BMP image file to \a fileName on disc. The output plot will have the dimensions \a width
  and \a height in pixels, multiplied by \a scale. If either \a width or \a height is zero, the
  current width and height of the QCustomPlot widget is used instead. Line widths and texts etc.
  are not scaled up when larger widths/heights are used. If you want that effect, use the \a scale
  parameter.

  For example, if you set both \a width and \a height to 100 and \a scale to 2, you will end up with
  an image file of size 200*200 in which all graphical elements are scaled up by factor 2 (line
  widths, texts, etc.). This scaling is not done by stretching a 100*100 image, the result will have
  full 200*200 pixel resolution.

  If you use a high scaling factor, it is recommended to enable antialiasing for all elements by
  temporarily setting \ref QCustomPlot::setAntialiasedElements to \ref QCP::aeAll as this allows
  QCustomPlot to place objects with sub-pixel accuracy.

  The \a resolution will be written to the image file header and has no direct consequence for the
  quality or the pixel size. However, if opening the image with a tool which respects the metadata,
  it will be able to scale the image to match either a given size in real units of length (inch,
  centimeters, etc.), or the target display DPI. You can specify in which units \a resolution is
  given, by setting \a resolutionUnit. The \a resolution is converted to the format's expected
  resolution unit internally.

  Returns true on success. If this function fails, most likely the BMP format isn't supported by
  the system, see Qt docs about QImageWriter::supportedImageFormats().

  The objects of the plot will appear in the current selection state. If you don't want any selected
  objects to be painted in their selected look, deselect everything with \ref deselectAll before
  calling this function.

  \warning If calling this function inside the constructor of the parent of the QCustomPlot widget
  (i.e. the MainWindow constructor, if QCustomPlot is inside the MainWindow), always provide
  explicit non-zero widths and heights. If you leave \a width or \a height as 0 (default), this
  function uses the current width and height of the QCustomPlot widget. However, in Qt, these
  aren't defined yet inside the constructor, so you would get an image that has strange
  widths/heights.

  \see savePdf, savePng, saveJpg, saveRastered
*/
bool QCustomPlot::saveBmp(const QString& fileName, int width, int height, double scale,
                          int resolution, QCP::ResolutionUnit resolutionUnit)
{
    return saveRastered(fileName, width, height, scale, "BMP", -1, resolution, resolutionUnit);
}

/*! \internal

  Returns a minimum size hint that corresponds to the minimum size of the top level layout
  (\ref plotLayout). To prevent QCustomPlot from being collapsed to size/width zero, set a minimum
  size (setMinimumSize) either on the whole QCustomPlot or on any layout elements inside the plot.
  This is especially important, when placed in a QLayout where other components try to take in as
  much space as possible (e.g. QMdiArea).
*/
QSize QCustomPlot::minimumSizeHint() const
{
    const QCPLayoutGrid* layout = plotLayout();
    return layout ? layout->minimumOuterSizeHint() : QSize();
}

/*! \internal

  Returns a size hint that is the same as \ref minimumSizeHint.

*/
QSize QCustomPlot::sizeHint() const
{
    return QCustomPlot::minimumSizeHint();
}

/*! \internal

  Called by QRhiWidget when the RHI instance is ready. Creates GPU resources needed for
  compositing paint buffer textures onto the widget surface.
*/
void QCustomPlot::initialize(QRhiCommandBuffer* cb)
{
    // Delivers at its own return, also under a method scope (grabFramebuffer() from an export):
    // the paint event's scope does not cover that path.
    const QCPEmissionScope scope(this, QCPEmissionScope::Kind::WidgetEvent);
    PROFILE_HERE_N("QCustomPlot::initialize");

    // QRhiWidget calls initialize() on resize (when the backing texture is recreated).
    // Most RHI resources (sampler, buffers) are still valid, but the pipeline depends on the
    // render pass descriptor which may change. Invalidate it for lazy recreation in render().
    if (mRhiInitialized)
    {
        delete mCompositePipeline;
        mCompositePipeline = nullptr;
        delete mLayoutSrb;
        mLayoutSrb = nullptr;
        delete mCompositeUbo;
        mCompositeUbo = nullptr;
        for (const auto& buffer : mPaintBuffers)
        {
            if (auto* rhiBuffer = dynamic_cast<QCPPaintBufferRhi*>(buffer.data()))
                rhiBuffer->setSrb(nullptr, nullptr);
        }
        for (auto* prl : mPlottableRhiLayers)
            prl->invalidatePipeline();
        for (auto* srl : mScatterRhiLayers)
            srl->invalidatePipeline();
        if (mGridRhiLayer)
            mGridRhiLayer->invalidatePipeline();
        // QRhiWidget calls initialize() on resize BEFORE resizeEvent() fires.
        // Regenerate geometry now so render() has fresh data for the new size.
        adoptWidgetViewport();
        replot(rpImmediateRefresh);
        return;
    }

    mRhi = QRhiWidget::rhi();
    if (!mRhi)
    {
        qDebug() << Q_FUNC_INFO << "No QRhi instance available";
        return;
    }

    // Create sampler for texture compositing
    mSampler = mRhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                 QRhiSampler::None,
                                 QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    mSampler->create();

    // Fullscreen quad: position (x,y) + texcoord (u,v)
    // QImage origin is top-left. In Y-up NDC (OpenGL), (-1,-1) is bottom-left so V must be
    // flipped. In Y-down NDC (Metal/D3D), (-1,-1) is top-left so V maps directly.
    const float v0 = mRhi->isYUpInNDC() ? 1.0f : 0.0f; // V at NDC bottom
    const float v1 = mRhi->isYUpInNDC() ? 0.0f : 1.0f; // V at NDC top
    const float quadVertices[] = {
        -1.0f, -1.0f,  0.0f, v0,
         1.0f, -1.0f,  1.0f, v0,
         1.0f,  1.0f,  1.0f, v1,
        -1.0f,  1.0f,  0.0f, v1,
    };
    mQuadVertexBuffer = mRhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer,
                                         sizeof(quadVertices));
    mQuadVertexBuffer->create();

    static const quint16 quadIndices[] = { 0, 1, 2, 0, 2, 3 };
    mQuadIndexBuffer = mRhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::IndexBuffer,
                                        sizeof(quadIndices));
    mQuadIndexBuffer->create();

    // Upload vertex/index data
    QRhiResourceUpdateBatch* updates = mRhi->nextResourceUpdateBatch();
    updates->uploadStaticBuffer(mQuadVertexBuffer, quadVertices);
    updates->uploadStaticBuffer(mQuadIndexBuffer, quadIndices);
    cb->resourceUpdate(updates);

    // Uniform buffer for per-layer composite translation (5 floats, padded to 32 for std140)
    mCompositeUbo = mRhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 32);
    mCompositeUbo->create();

    // Recreate paint buffers now that we have an RHI instance
    mPaintBuffers.clear();
    setupPaintBuffers();

    mRhiInitialized = true;

    // Draw content into the fresh buffers so the first frame is not blank.
    // This handles widgets that are initially hidden (e.g. in a QTabWidget).
    adoptWidgetViewport();
    replot(rpImmediateRefresh);
}

/*! \internal

  Called by QRhiWidget each frame. Uploads paint buffer staging images to GPU textures
  and composites them onto the widget render target.
*/
void QCustomPlot::ensureCompositePipeline()
{
    if (mCompositePipeline)
        return;

    auto vertShader = qcp::rhi::loadEmbeddedShader(composite_vert_qsb_data, composite_vert_qsb_data_len);
    auto fragShader = qcp::rhi::loadEmbeddedShader(composite_frag_qsb_data, composite_frag_qsb_data_len);

    if (!vertShader.isValid() || !fragShader.isValid())
    {
        qDebug() << Q_FUNC_INFO << "Failed to deserialize compositing shaders";
        return;
    }

    mCompositePipeline = mRhi->newGraphicsPipeline();
    mCompositePipeline->setShaderStages({
        { QRhiShaderStage::Vertex, vertShader },
        { QRhiShaderStage::Fragment, fragShader }
    });

    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({ { 4 * sizeof(float) } });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
        { 0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float) }
    });
    mCompositePipeline->setVertexInputLayout(inputLayout);

    mCompositePipeline->setTargetBlends({qcp::rhi::premultipliedAlphaBlend()});

    mCompositePipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
    delete mLayoutSrb;
    mLayoutSrb = mRhi->newShaderResourceBindings();
    mLayoutSrb->setBindings({
        QRhiShaderResourceBinding::sampledTexture(
            0, QRhiShaderResourceBinding::FragmentStage,
            nullptr, mSampler),
        QRhiShaderResourceBinding::uniformBuffer(
            1, QRhiShaderResourceBinding::VertexStage,
            mCompositeUbo)
    });
    mLayoutSrb->create();
    mCompositePipeline->setShaderResourceBindings(mLayoutSrb);
    mCompositePipeline->setSampleCount(sampleCount());
    mCompositePipeline->setFlags(mCompositePipeline->flags()
                                 | QRhiGraphicsPipeline::UsesScissor);
    mCompositePipeline->create();
}

/*! \internal

  Refreshes the GPU translation offsets of layers that are skipping their repaint
  via translation, using the freshest axis ranges. replot() sets these offsets too,
  but replots are coalesced (rpQueuedReplot): a range change landing between the
  last replot and this frame would otherwise leave plottable/scatter/colormap
  geometry trailing render-time-updated layers (spans, grid) by up to a frame.

  The canSkipRepaintForTranslation() guard makes this a no-op whenever the offset
  is invalid (zoom, pan beyond one viewport, invalidated buffer) — those frames are
  owned by the replot's full-repaint path and behave exactly as before.
*/
void QCustomPlot::refreshLayerTranslationOffsets()
{
    for (auto it = mPlottableRhiLayers.begin(); it != mPlottableRhiLayers.end(); ++it)
    {
        if (it.key()->canSkipRepaintForTranslation())
        {
            const QPointF offset = it.key()->pixelOffset();
            it.value()->setAllOffsets(static_cast<float>(offset.x()),
                                      static_cast<float>(offset.y()));
        }
    }
    for (auto it = mScatterRhiLayers.begin(); it != mScatterRhiLayers.end(); ++it)
    {
        if (it.key()->canSkipRepaintForTranslation())
        {
            const QPointF offset = it.key()->pixelOffset();
            it.value()->setAllOffsets(static_cast<float>(offset.x()),
                                      static_cast<float>(offset.y()));
        }
    }
}

void QCustomPlot::uploadLayerTextures(QRhiResourceUpdateBatch* updates, const QSize& outputSize)
{
    // Freshen translation offsets first — the PRL/SRL/colormap uploadResources()
    // calls below upload per-draw uniforms every frame.
    refreshLayerTranslationOffsets();

    for (const auto& buffer : mPaintBuffers)
    {
        auto* rhiBuffer = static_cast<QCPPaintBufferRhi*>(buffer.data());
        if (rhiBuffer->texture() && rhiBuffer->needsUpload())
        {
            QRhiTextureSubresourceUploadDescription subDesc(rhiBuffer->stagingImage());
            QRhiTextureUploadDescription uploadDesc(
                QRhiTextureUploadEntry(0, 0, subDesc));
            updates->uploadTexture(rhiBuffer->texture(), uploadDesc);
            rhiBuffer->setUploaded();
        }
    }

    for (auto* prl : mPlottableRhiLayers)
    {
        prl->ensurePipeline(renderTarget()->renderPassDescriptor(), sampleCount());
        prl->uploadResources(updates, outputSize, mBufferDevicePixelRatio,
                              mRhi->isYUpInNDC());
    }

    for (auto* srl : mScatterRhiLayers)
    {
        srl->ensurePipeline(renderTarget()->renderPassDescriptor(), sampleCount());
        srl->uploadResources(updates, outputSize, mBufferDevicePixelRatio,
                              mRhi->isYUpInNDC());
    }

    if (!mCompositeUbo)
    {
        mCompositeUbo = mRhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 32);
        mCompositeUbo->create();
    }

    // Register all axes with the grid RHI layer
    if (mGridRhiLayer || mRhi)
    {
        auto* grl = gridRhiLayer();
        if (grl)
        {
            for (auto* ar : axisRects())
            {
                for (auto* axis : ar->axes())
                    grl->registerAxis(axis);
            }
        }
    }

    // Upload grid RHI resources (must run before hasContent() check in render loop,
    // since uploadResources() triggers the first geometry build after axis registration)
    if (mGridRhiLayer)
    {
        mGridRhiLayer->ensurePipeline(renderTarget()->renderPassDescriptor(), sampleCount());
        mGridRhiLayer->uploadResources(updates, outputSize, mBufferDevicePixelRatio,
                                        mRhi->isYUpInNDC());
    }
}

void QCustomPlot::executeRenderPass(QRhiCommandBuffer* cb, QRhiResourceUpdateBatch* updates,
                                     const QSize& outputSize)
{
    QColor clearColor = (mBackgroundBrush.style() == Qt::SolidPattern)
        ? mBackgroundBrush.color() : Qt::white;

    cb->beginPass(renderTarget(), clearColor, { 1.0f, 0 }, updates);

    const QRhiCommandBuffer::VertexInput vbufBinding(mQuadVertexBuffer, 0);

    // Iterate over layers (not paint buffers) — mPaintBuffers and mLayers are NOT 1:1.
    // Multiple logical layers share a single paint buffer. Track which buffers we've
    // already composited to avoid double-drawing.
    QSet<QCPAbstractPaintBuffer*> compositedBuffers;

    for (QCPLayer* layer : mLayers)
    {
        // Composite the layer's paint buffer texture (if not already done)
        if (auto pb = layer->mPaintBuffer.toStrongRef())
        {
            if (!compositedBuffers.contains(pb.data()))
            {
                compositedBuffers.insert(pb.data());
                auto* rhiBuffer = static_cast<QCPPaintBufferRhi*>(pb.data());
                if (rhiBuffer->texture())
                {
                    if (!rhiBuffer->srb() || !rhiBuffer->srbMatchesTexture())
                    {
                        auto* srb = mRhi->newShaderResourceBindings();
                        srb->setBindings({
                            QRhiShaderResourceBinding::sampledTexture(
                                0, QRhiShaderResourceBinding::FragmentStage,
                                rhiBuffer->texture(), mSampler),
                            QRhiShaderResourceBinding::uniformBuffer(
                                1, QRhiShaderResourceBinding::VertexStage,
                                mCompositeUbo)
                        });
                        srb->create();
                        rhiBuffer->setSrb(srb, rhiBuffer->texture());
                    }
                    QPointF layerOffset = layer->canTranslateInsteadOfRepaint()
                        ? layer->pixelOffset() : QPointF();
                    struct {
                        float translateX, translateY, viewportW, viewportH, yFlip;
                    } compositeParams = {
                        float(layerOffset.x()),
                        float(layerOffset.y()),
                        float(outputSize.width()),
                        float(outputSize.height()),
                        mRhi->isYUpInNDC() ? -1.0f : 1.0f
                    };
                    QRhiResourceUpdateBatch* uboUpdates = mRhi->nextResourceUpdateBatch();
                    uboUpdates->updateDynamicBuffer(mCompositeUbo, 0, sizeof(compositeParams), &compositeParams);
                    cb->resourceUpdate(uboUpdates);

                    bool needsScissor = !layerOffset.isNull();
                    if (needsScissor)
                    {
                        QRect clipRect;
                        for (QCPAbstractPlottable* plottable : layer->plottableChildren())
                            clipRect = clipRect.isNull() ? plottable->clipRect()
                                                         : clipRect.united(plottable->clipRect());
                        if (!clipRect.isNull())
                        {
                            double dpr = bufferDevicePixelRatio();
                            int sx = static_cast<int>(clipRect.x() * dpr);
                            int sy = static_cast<int>(clipRect.y() * dpr);
                            int sw = static_cast<int>(clipRect.width() * dpr);
                            int sh = static_cast<int>(clipRect.height() * dpr);
                            if (mRhi->isYUpInFramebuffer())
                                sy = outputSize.height() - sy - sh;
                            cb->setScissor({sx, sy, sw, sh});
                        }
                        else
                        {
                            needsScissor = false;
                        }
                    }

                    cb->setGraphicsPipeline(mCompositePipeline);
                    cb->setViewport({0, 0, float(outputSize.width()), float(outputSize.height())});
                    if (!needsScissor)
                        cb->setScissor({0, 0, outputSize.width(), outputSize.height()});
                    cb->setShaderResources(rhiBuffer->srb());
                    cb->setVertexInput(0, 1, &vbufBinding, mQuadIndexBuffer, 0,
                                       QRhiCommandBuffer::IndexUInt16);
                    cb->drawIndexed(6);
                }
            }
        }

        // Draw GPU plottable geometry for this layer (after its paint buffer)
        if (auto* prl = mPlottableRhiLayers.value(layer, nullptr))
        {
            if (prl->hasGeometry())
                prl->render(cb, outputSize);
        }

        // Draw GPU scatter geometry for this layer (after lines)
        if (auto* srl = mScatterRhiLayers.value(layer, nullptr))
        {
            if (srl->hasGeometry())
                srl->render(cb, outputSize);
        }

        // Draw GPU grid lines on the "grid" layer
        if (layer == this->layer(QLatin1String("grid"))
            && mGridRhiLayer && mGridRhiLayer->hasContent())
            mGridRhiLayer->renderGridLines(cb, outputSize);

        // Draw GPU tick marks on the "axes" layer
        if (layer == this->layer(QLatin1String("axes"))
            && mGridRhiLayer && mGridRhiLayer->hasContent())
            mGridRhiLayer->renderTickMarks(cb, outputSize);
    }

    cb->endPass();
}

void QCustomPlot::render(QRhiCommandBuffer* cb)
{
    // Delivers at its own return, also under a method scope (grabFramebuffer() from an export):
    // the paint event's scope does not cover that path.
    const QCPEmissionScope scope(this, QCPEmissionScope::Kind::WidgetEvent);
    PROFILE_FRAME_MARK;
    PROFILE_HERE_N("QCustomPlot::render");

    if (!mRhiInitialized || !mRhi)
        return;

    // Detect DPR changes (e.g. window moved between Retina and non-Retina displays)
    if (const auto newDpr = devicePixelRatioF(); !qFuzzyCompare(mBufferDevicePixelRatio, newDpr))
    {
        const int autoSample = mBufferDevicePixelRatio >= 2.0 ? 1 : 4;
        const bool userOverrode = sampleCount() != autoSample;
        setBufferDevicePixelRatio(newDpr);
        if (!userOverrode)
            setSampleCount(newDpr >= 2.0 ? 1 : 4);
        replot(QCustomPlot::rpImmediateRefresh);
        return;
    }

    const QSize outputSize = renderTarget()->pixelSize();
    QRhiResourceUpdateBatch* updates = mRhi->nextResourceUpdateBatch();

    uploadLayerTextures(updates, outputSize);
    ensureCompositePipeline();
    if (!mCompositePipeline)
        return;
    executeRenderPass(cb, updates, outputSize);
}

/*! \internal

  Called by QRhiWidget when GPU resources should be released.
*/
void QCustomPlot::releaseResources()
{
    PROFILE_HERE_N("QCustomPlot::releaseResources");
    // Release paint buffer GPU resources while the RHI is still valid
    qDeleteAll(mPlottableRhiLayers);
    mPlottableRhiLayers.clear();
    qDeleteAll(mScatterRhiLayers);
    mScatterRhiLayers.clear();
    mPaintBuffers.clear();
    releaseCompositeResources();
    mRhi = nullptr;
    mRhiInitialized = false;
}

/*! \internal

  Frees the grid GPU layer and the compositing resources. Safe to call twice: by
  releaseResources, then by the destructor.
*/
void QCustomPlot::releaseCompositeResources()
{
    delete std::exchange(mGridRhiLayer, nullptr);
    delete std::exchange(mCompositePipeline, nullptr);
    delete std::exchange(mLayoutSrb, nullptr);
    delete std::exchange(mCompositeUbo, nullptr);
    delete std::exchange(mSampler, nullptr);
    delete std::exchange(mQuadVertexBuffer, nullptr);
    delete std::exchange(mQuadIndexBuffer, nullptr);
}

/*! \internal

  Event handler for a resize of the QCustomPlot widget. The viewport (which becomes the outer rect
  of mPlotLayout) is resized appropriately. Finally a \ref replot is performed.
*/
void QCustomPlot::resizeEvent([[maybe_unused]] QResizeEvent* event)
{
    // resize and repaint the buffer:
    adoptWidgetViewport();
    if (mGridRhiLayer)
        mGridRhiLayer->markGeometryDirty();
    notifyAlignmentGroups();
    replot(rpImmediateRefresh);
}

/*! \internal

  Event handler for a move of the QCustomPlot widget. The elements' groups compare their edges in
  window coordinates, so a move of the widget moves one member relative to its peers: this plot
  relays out, and its group marks the peers only if the shared edge actually changed.
*/
void QCustomPlot::moveEvent(QMoveEvent* event)
{
    notifyAlignmentGroups();
    QRhiWidget::moveEvent(event);
}

/*! \internal

  Relays out this plot's grouped elements, which recompute the shared edges from their new
  geometry. Peers are left alone unless a shared edge changed, so moving or resizing one plot of a
  splitter does not repaint the whole stack.
*/
void QCustomPlot::notifyAlignmentGroups()
{
    for (QCPLayoutElement* element : plotLayout()->elements(true))
        if (element && element->alignmentGroup())
            element->markLayoutAppearanceDirty();
}

/*! \internal

  Relays out every member of this plot's alignment groups, so they pick up a member that was just
  hidden or shown: a hidden plot does not lay out, so its peers must be told instead.
*/
void QCustomPlot::relayoutAlignmentGroups()
{
    QSet<QCPAlignmentGroup*> groups;
    for (QCPLayoutElement* element : plotLayout()->elements(true))
        if (element && element->alignmentGroup())
            groups.insert(element->alignmentGroup().data());
    for (QCPAlignmentGroup* group : groups)
        group->relayoutMembers();
}

/*! \internal

  Event handler for when the widget is shown. Records that the widget has been
  shown at least once (mWasShown), which the skipReplotsWhenHidden guard uses to
  only skip replots for widgets that are hidden after a real show — never-shown
  widgets (offscreen rendering, tests) keep classic replot behavior.
*/
void QCustomPlot::showEvent(QShowEvent* event)
{
    QRhiWidget::showEvent(event);
    mWasShown = true;
    if (std::exchange(mReplotSkippedWhileHidden, false))
        replot(rpQueuedReplot);
    relayoutAlignmentGroups();
}

/*! \internal

  Event handler for when the widget is hidden. The peers of the alignment groups this plot's
  elements belong to relay out, so a hidden member stops widening their shared edge.
*/
void QCustomPlot::hideEvent(QHideEvent* event)
{
    relayoutAlignmentGroups();
    QRhiWidget::hideEvent(event);
}

/*! \internal

 Event handler for when a double click occurs. Emits the \ref mouseDoubleClick signal, then
 determines the layerable under the cursor and forwards the event to it. Finally, emits the
 specialized signals when certain objecs are clicked (e.g. \ref plottableDoubleClick, \ref
 axisDoubleClick, etc.).

 \see mousePressEvent, mouseReleaseEvent
*/
void QCustomPlot::mouseDoubleClickEvent(QMouseEvent* event)
{
    const QPointer<QCustomPlot> alive(this); // a layerable's slot may delete the plot
    deliver(this, [this, event] { emit mouseDoubleClick(event); });
    mMouseHasMoved = false;
    mMousePressPos = event->pos();

    // determine layerable under the cursor (this event is called instead of the second press event
    // in a double-click):
    QList<QVariant> details;
    const auto candidates = guarded(layerableListAt(mMousePressPos, false, &details));
    for (int i = 0; i < candidates.size(); ++i)
    {
        if (!candidates.at(i))
            continue;
        event->accept(); // default impl of QCPLayerable's mouse events ignore the event, in that
                         // case propagate to next candidate in list
        candidates.at(i)->mouseDoubleClickEvent(event, details.at(i));
        if (!alive)
            return;
        if (event->isAccepted())
        {
            mMouseEventLayerable = candidates.at(i);
            mMouseEventLayerableDetails = details.at(i);
            break;
        }
    }

    if (!candidates.isEmpty() && candidates.first())
        emitDoubleClickSignal(candidates.first(), details.first(), event);

    event->accept(); // in case QCPLayerable reimplementation manipulates event accepted state. In
                     // QWidget event system, QCustomPlot wants to accept the event.
}

/*! \internal

  Event handler for when a mouse button is pressed. Emits the mousePress signal.

  If the current \ref setSelectionRectMode is not \ref QCP::srmNone, passes the event to the
  selection rect. Otherwise determines the layerable under the cursor and forwards the event to it.

  \see mouseMoveEvent, mouseReleaseEvent
*/
void QCustomPlot::mousePressEvent(QMouseEvent* event)
{
    const QPointer<QCustomPlot> alive(this); // a layerable's slot may delete the plot
    deliver(this, [this, event] { emit mousePress(event); });
    // save some state to tell in releaseEvent whether it was a click:
    mMouseHasMoved = false;
    mMousePressPos = event->pos();

    // Right-click cancels in-progress creation
    if (event->button() == Qt::RightButton && mCreationState
        && mCreationState->state() == QCPItemCreationState::Drawing)
    {
        mCreationState->cancel();
        event->accept();
        return;
    }

    // Item creation intercept (highest priority)
    if (mCreationState)
    {
        bool creationActive = (mCreationState->state() == QCPItemCreationState::Drawing)
            || (mCreationModeEnabled && mItemCreator)
            || ((event->modifiers() & mCreationModifier) && mItemCreator);
        if (creationActive && mCreationState->handleMousePress(event))
        {
            event->accept();
            return;
        }
    }

    if (mSelectionRect && mSelectionRectMode != QCP::srmNone)
    {
        if (mSelectionRectMode != QCP::srmZoom
            || qobject_cast<QCPAxisRect*>(axisRectAt(
                mMousePressPos))) // in zoom mode only activate selection rect if on an axis rect
            mSelectionRect->startSelection(event);
    }
    else
    {
        // no selection rect interaction, prepare for click signal emission and forward event to
        // layerable under the cursor:
        QList<QVariant> details;
        const auto candidates = guarded(layerableListAt(mMousePressPos, false, &details));
        if (!candidates.isEmpty())
        {
            mMouseSignalLayerable
                = candidates.first(); // candidate for signal emission is always topmost hit
                                      // layerable (signal emitted in release event)
            mMouseSignalLayerableDetails = details.first();
        }
        // forward event to topmost candidate which accepts the event:
        for (int i = 0; i < candidates.size(); ++i)
        {
            if (!candidates.at(i))
                continue;
            event->accept(); // default impl of QCPLayerable's mouse events call ignore() on the
                             // event, in that case propagate to next candidate in list
            candidates.at(i)->mousePressEvent(event, details.at(i));
            if (!alive)
                return;
            if (event->isAccepted())
            {
                mMouseEventLayerable = candidates.at(i);
                mMouseEventLayerableDetails = details.at(i);
                break;
            }
        }
    }

    event->accept(); // in case QCPLayerable reimplementation manipulates event accepted state. In
                     // QWidget event system, QCustomPlot wants to accept the event.
}

/*! \internal

  Event handler for when the cursor is moved. Emits the \ref mouseMove signal.

  If the selection rect (\ref setSelectionRect) is currently active, the event is forwarded to it
  in order to update the rect geometry.

  Otherwise, if a layout element has mouse capture focus (a mousePressEvent happened on top of the
  layout element before), the mouseMoveEvent is forwarded to that element.

  \see mousePressEvent, mouseReleaseEvent
*/
void QCustomPlot::mouseMoveEvent(QMouseEvent* event)
{
    deliver(this, [this, event] { emit mouseMove(event); });

    if (!mMouseHasMoved && (mMousePressPos - event->pos()).manhattanLength() > 3)
        mMouseHasMoved = true; // moved too far from mouse press position, don't handle as click on
                               // mouse release

    if (mCreationState && mCreationState->handleMouseMove(event))
    {
        event->accept();
        return;
    }

    if (mSelectionRect && mSelectionRect->isActive())
        mSelectionRect->moveSelection(event);
    else if (mMouseEventLayerable) // call event of affected layerable:
        mMouseEventLayerable->mouseMoveEvent(event, mMousePressPos);

    event->accept(); // in case QCPLayerable reimplementation manipulates event accepted state. In
                     // QWidget event system, QCustomPlot wants to accept the event.
}

/*! \internal

  Event handler for when a mouse button is released. Emits the \ref mouseRelease signal.

  If the mouse was moved less than a certain threshold in any direction since the \ref
  mousePressEvent, it is considered a click which causes the selection mechanism (if activated via
  \ref setInteractions) to possibly change selection states accordingly. Further, specialized mouse
  click signals are emitted (e.g. \ref plottableClick, \ref axisClick, etc.)

  If a layerable is the mouse capturer (a \ref mousePressEvent happened on top of the layerable
  before), the \ref mouseReleaseEvent is forwarded to that element.

  \see mousePressEvent, mouseMoveEvent
*/
void QCustomPlot::mouseReleaseEvent(QMouseEvent* event)
{
    // Every step below can reach a slot that deletes the plot.
    const QPointer<QCustomPlot> alive(this);
    deliver(this, [this, event] { emit mouseRelease(event); });
    if (alive && !mMouseHasMoved) // mouse hasn't moved (much) between press and release
        releaseAsClick(event);
    if (alive)
        releaseDrag(event);
    if (alive && noAntialiasingOnDrag())
        replot(rpQueuedReplot);

    event->accept(); // in case QCPLayerable reimplementation manipulates event accepted state. In
                     // QWidget event system, QCustomPlot wants to accept the event.
}

// A click: cancels a selection rect, selects, and emits the specialized click signal.
void QCustomPlot::releaseAsClick(QMouseEvent* event)
{
    const QPointer<QCustomPlot> alive(this);
    // a simple click shouldn't successfully finish a selection rect, so cancel it here
    if (mSelectionRect && mSelectionRect->isActive())
        mSelectionRect->cancel();
    if (alive && event->button() == Qt::LeftButton)
        processPointSelection(event);
    if (alive)
        emitClickSignal(event);
}

namespace
{
int clickedDataIndex(const QVariant& details)
{
    const auto selection = details.value<QCPDataSelection>();
    return selection.isEmpty() ? 0 : selection.dataRange().begin();
}
} // namespace

// Each is delivered only while the clicked object lives.
void QCustomPlot::emitClickSignal(QMouseEvent* event)
{
    QCPLayerable* clicked = std::exchange(mMouseSignalLayerable, nullptr);
    const QVariant details = mMouseSignalLayerableDetails;
    if (auto* ap = qobject_cast<QCPAbstractPlottable*>(clicked))
        deliver(ap, [this, ap, i = clickedDataIndex(details), event] {
            emit plottableClick(ap, i, event);
        });
    else if (auto* ax = qobject_cast<QCPAxis*>(clicked))
        deliver(ax, [this, ax, part = details.value<QCPAxis::SelectablePart>(), event] {
            emit axisClick(ax, part, event);
        });
    else if (auto* ai = qobject_cast<QCPAbstractItem*>(clicked))
        deliver(ai, [this, ai, event] { emit itemClick(ai, event); });
    else if (auto* lg = qobject_cast<QCPLegend*>(clicked))
        deliver(lg, [this, lg, event] { emit legendClick(lg, nullptr, event); });
    else if (auto* li = qobject_cast<QCPAbstractLegendItem*>(clicked))
        deliver(li, [this, li, event] { emit legendClick(li->parentLegend(), li, event); });
}

void QCustomPlot::emitDoubleClickSignal(QCPLayerable* clicked, const QVariant& details,
                                        QMouseEvent* event)
{
    if (auto* ap = qobject_cast<QCPAbstractPlottable*>(clicked))
        deliver(ap, [this, ap, i = clickedDataIndex(details), event] {
            emit plottableDoubleClick(ap, i, event);
        });
    else if (auto* ax = qobject_cast<QCPAxis*>(clicked))
        deliver(ax, [this, ax, part = details.value<QCPAxis::SelectablePart>(), event] {
            emit axisDoubleClick(ax, part, event);
        });
    else if (auto* ai = qobject_cast<QCPAbstractItem*>(clicked))
        deliver(ai, [this, ai, event] { emit itemDoubleClick(ai, event); });
    else if (auto* lg = qobject_cast<QCPLegend*>(clicked))
        deliver(lg, [this, lg, event] { emit legendDoubleClick(lg, nullptr, event); });
    else if (auto* li = qobject_cast<QCPAbstractLegendItem*>(clicked))
        deliver(li, [this, li, event] { emit legendDoubleClick(li->parentLegend(), li, event); });
}

// Finishes the selection rect (the action runs from its accepted signal), or the captured drag.
void QCustomPlot::releaseDrag(QMouseEvent* event)
{
    if (mSelectionRect && mSelectionRect->isActive()) // a click above already canceled it
        mSelectionRect->endSelection(event);
    else if (const QPointer<QCPLayerable> target = std::exchange(mMouseEventLayerable, nullptr))
        target->mouseReleaseEvent(event, QPointF(mMousePressPos));
}

namespace
{
QCPEmissionScope::Kind scopeKindOf(QEvent::Type type)
{
    switch (type)
    {
        case QEvent::Resize:
        case QEvent::Show:
        case QEvent::Paint:
            return QCPEmissionScope::Kind::WidgetEvent;
        default:
            return QCPEmissionScope::Kind::Event;
    }
}
} // namespace

/*! \internal

  The signals the handlers queue are delivered as this returns: after QCustomPlot handled the
  event, before Qt goes on with it. Their slots get the event itself, so \c ignore() in a slot
  lets the event go on to the parent widget (once Qt::WA_NoMousePropagation is cleared).
*/
bool QCustomPlot::event(QEvent* event)
{
    // A deleteLater() teardown delivers like a plain delete: no scope to hold its signals back.
    if (event->type() == QEvent::DeferredDelete)
        return handleEvent(event);
    const QPointer<QCustomPlot> alive(this);
    const bool handled = handleInScope(event);
    if (alive)
        return handled;
    // QApplication passes an ignored mouse or wheel event on through this widget, now deleted.
    event->accept();
    return true;
}

bool QCustomPlot::handleInScope(QEvent* event)
{
    const QCPEmissionScope scope(this, scopeKindOf(event->type()));
    return handleEvent(event);
}

bool QCustomPlot::handleEvent(QEvent* event)
{
    if (mInteractions.testFlag(QCP::iGestures))
    {
        switch (event->type())
        {
            case QEvent::TouchBegin:
            case QEvent::TouchUpdate:
            case QEvent::TouchEnd:
            case QEvent::TouchCancel:
                return touchEvent(static_cast<QTouchEvent*>(event));
            default:
                break;
        }
        if (event->type() == QEvent::NativeGesture
            && nativeGestureEvent(static_cast<QNativeGestureEvent*>(event)))
            return true;
    }
    return QRhiWidget::event(event);
}

/*! \internal

  Touch with \ref QCP::iGestures. One finger acts as the mouse: drag, click. Two fingers pinch:
  the data under their midpoint is scaled by the change of their distance, then follows the
  midpoint. Each finger's last position is kept here, by id: in the browser every finger moves in
  its own event, and the other one carries a stale lastPosition(). Qt's QPinchGesture counted
  those steps twice.
*/
bool QCustomPlot::touchEvent(QTouchEvent* event)
{
    event->accept();
    const QPointer<QCustomPlot> alive(this); // the mouse and range slots may delete the plot
    const auto& points = event->points();
    const bool ending = event->type() == QEvent::TouchEnd || event->type() == QEvent::TouchCancel;
    if (event->type() == QEvent::TouchBegin && points.size() == 1)
        mTouch = {};   // a browser sends a second TouchBegin when the second finger lands
    if (!mTouch.pinching && points.size() >= 2 && !ending)
    {
        mTouch.pinching = true;
        if (mTouch.mouseDown)   // a second finger ends the drag, without a click
        {
            mMouseHasMoved = true;
            forwardTouchAsMouse(QEvent::MouseButtonRelease, points.first(), event);
            if (!alive)
                return true;
        }
    }
    if (mTouch.pinching && points.size() >= 2 && !ending)
        pinchStep(points.at(0), points.at(1));
    else if (!mTouch.pinching && points.size() == 1)
    {
        const QEventPoint& point = points.first();
        if (point.state() == QEventPoint::Pressed)
            forwardTouchAsMouse(QEvent::MouseButtonPress, point, event);
        else if (point.state() == QEventPoint::Released)
            forwardTouchAsMouse(QEvent::MouseButtonRelease, point, event);
        else if (mTouch.mouseDown)
            forwardTouchAsMouse(QEvent::MouseMove, point, event);
    }
    if (!alive)
        return true;
    for (const QEventPoint& point : points)
        mTouch.positions[point.id()] = touchPosition(point);
    if (ending)
    {
        const bool mouseDown = mTouch.mouseDown;
        mTouch = {}; // first: the release may delete the plot
        if (mouseDown)
            forwardTouchAsMouse(QEvent::MouseButtonRelease, points.first(), event);
    }
    return true;
}

void QCustomPlot::forwardTouchAsMouse(QEvent::Type type, const QEventPoint& point,
                                      const QTouchEvent* touch)
{
    // On the mouse, not the touch screen: an event shares its point's state with its device, so a
    // mouse event on touch point 0 would end the touch sequence.
    const bool release = type == QEvent::MouseButtonRelease;
    QMouseEvent mouse(type, point.position(), point.scenePosition(), point.globalPosition(),
                      Qt::LeftButton, release ? Qt::NoButton : Qt::LeftButton, touch->modifiers(),
                      QPointingDevice::primaryPointingDevice());
    mTouch.mouseDown = !release; // first: the mouse handlers may delete the plot
    // Delivers before the event on this stack dies.
    const QCPEmissionScope scope(this, QCPEmissionScope::Kind::Event);
    if (type == QEvent::MouseButtonPress)
        mousePressEvent(&mouse);
    else if (release)
        mouseReleaseEvent(&mouse);
    else
        mouseMoveEvent(&mouse);
}

// A finger that did not move is where it was: some platforms report no position for it.
QPointF QCustomPlot::touchPosition(const QEventPoint& point) const
{
    return point.state() == QEventPoint::Stationary
        ? mTouch.positions.value(point.id(), point.position())
        : point.position();
}

void QCustomPlot::pinchStep(const QEventPoint& a, const QEventPoint& b)
{
    const QPointF posA = touchPosition(a), posB = touchPosition(b);
    const QPointF lastA = mTouch.positions.value(a.id(), posA);
    const QPointF lastB = mTouch.positions.value(b.id(), posB);
    const QPointF lastMid = (lastA + lastB) / 2;
    const QPointF mid = (posA + posB) / 2;
    const double lastSpan = QLineF(lastA, lastB).length();
    const double span = QLineF(posA, posB).length();
    // A rangeChanged slot may delete the axis rect, or the plot with it.
    const QPointer<QCustomPlot> alive(this);
    const QPointer<QCPAxisRect> rect = axisRectAt(lastMid);
    if (!rect)
        return;
    if (mInteractions.testFlag(QCP::iRangeZoom) && lastSpan > 0 && span > 0)
        rect->zoomAt(lastMid, lastSpan / span);
    if (rect && mInteractions.testFlag(QCP::iRangeDrag))
        rect->panByPixels(mid - lastMid);
    if (alive)
        replot(rpQueuedReplot);
}

/*! \internal

  A native trackpad pinch (macOS): value() is the change in magnification.
*/
bool QCustomPlot::nativeGestureEvent(QNativeGestureEvent* event)
{
    if (event->gestureType() != Qt::ZoomNativeGesture)
        return false;
    QCPAxisRect* rect = axisRectAt(event->position());
    if (!rect || !mInteractions.testFlag(QCP::iRangeZoom) || event->value() <= -1)
        return false; // left ignored: the parent widget gets the pinch
    // Before the zoom: QApplication ignores the event first, and an ignored one goes on to the
    // widget's parent through the widget, which a rangeChanged slot may have deleted.
    event->accept();
    const QPointer<QCustomPlot> alive(this); // a rangeChanged slot may delete the plot
    rect->zoomAt(event->position(), 1.0 / (1.0 + event->value()));
    if (alive)
        replot(rpQueuedReplot);
    return true;
}

/*! \internal

  Event handler for mouse wheel events. First, the \ref mouseWheel signal is emitted. Then
  determines the affected layerable and forwards the event to it.
*/
void QCustomPlot::wheelEvent(QWheelEvent* event)
{
    deliver(this, [this, event] { emit mouseWheel(event); });

    // forward event to layerable under cursor:
    for (const auto& candidate : guarded(layerableListAt(event->position(), false)))
    {
        if (!candidate)
            continue;
        event->accept(); // default impl of QCPLayerable's mouse events ignore the event, in that
                         // case propagate to next candidate in list
        candidate->wheelEvent(event);
        if (event->isAccepted())
            break;
    }
    event->accept(); // in case QCPLayerable reimplementation manipulates event accepted state. In
                     // QWidget event system, QCustomPlot wants to accept the event.
}

/*! \internal

  Event handler for key press events. Emits \ref deleteRequested on all selected items when the
  Delete or Backspace key is pressed.
*/
void QCustomPlot::keyPressEvent(QKeyEvent* event)
{
    // A rubber band being dragged gets the key first: Escape cancels it.
    if (mSelectionRect && mSelectionRect->isActive())
    {
        mSelectionRect->keyPressEvent(event);
        if (!mSelectionRect || !mSelectionRect->isActive())
        {
            event->accept();
            return;
        }
    }

    if (mCreationState && mCreationState->handleKeyPress(event))
    {
        event->accept();
        return;
    }

    bool consumed = false;
    for (const auto& plottable : guarded(mPlottables))
        if (plottable)
            consumed = plottable->keyPress(event) || consumed;
    if (consumed)
    {
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
    {
        for (const auto& item : guarded(mItems))
        {
            if (item && item->selected())
                deliver(item, [item = item.data()] { emit item->deleteRequested(); });
        }
    }
    QRhiWidget::keyPressEvent(event);
}

/*! \internal

  This function draws the entire plot, including background pixmap, with the specified \a painter.
  It does not make use of the paint buffers like \ref replot, so this is the function typically
  used by saving/exporting methods such as \ref savePdf or \ref toPainter.

  Note that it does not fill the background with the background brush (as the user may specify with
  \ref setBackground(const QBrush &brush)), this is up to the respective functions calling this
  method.
*/
void QCustomPlot::draw(QCPPainter* painter)
{
    PROFILE_HERE_N("QCustomPlot::draw");
    const QCPEmissionScope scope(this);
    const QPointer<QCustomPlot> alive(this); // an afterLayout slot may delete the plot
    mFrameEpoch = qcp::nextFrameEpoch(); // an export frame remakes everyFrame() recordings once
    updateLayout();
    if (!alive)
        return;

    // draw viewport background pixmap:
    drawBackground(painter);

    // draw all layered objects (grid, axes, plottables, items, legend,...):
    for (QCPLayer* layer : mLayers)
        layer->draw(painter);

    /* Debug code to draw all layout element rects
    for (QCPLayoutElement *el : findChildren<QCPLayoutElement*>())
    {
      painter->setBrush(Qt::NoBrush);
      painter->setPen(QPen(QColor(0, 0, 0, 100), 0, Qt::DashLine));
      painter->drawRect(el->rect());
      painter->setPen(QPen(QColor(255, 0, 0, 100), 0, Qt::DashLine));
      painter->drawRect(el->outerRect());
    }
    */
}

/*! \internal

  Performs the layout update steps defined by \ref QCPLayoutElement::UpdatePhase, by calling \ref
  QCPLayoutElement::update on the main plot layout.

  Here, the layout elements calculate their positions and margins, and prepare for the following
  draw call.
*/
// An export lays the plot out at its own size. Laying it out again for the screen keeps
// coordToPixel(), axis rects and hit tests describing the widget once the export returns.
void QCustomPlot::restoreViewportAfterExport(const QRect& screenViewport)
{
    setViewport(screenViewport);
    updateLayout();
}

void QCustomPlot::updateLayout()
{
    PROFILE_HERE_N("QCustomPlot::updateLayout");
    // run through layout phases:
    QCPLayoutGrid* layout = plotLayout();
    if (!layout)
        return;
    layout->update(QCPLayoutElement::upPreparation);
    layout->update(QCPLayoutElement::upMargins);
    layout->update(QCPLayoutElement::upLayout);

    emitReplotSignal(&QCustomPlot::afterLayout);
}

/*! \internal

  Draws the viewport background pixmap of the plot.

  If a pixmap was provided via \ref setBackground, this function buffers the scaled version
  depending on \ref setBackgroundScaled and \ref setBackgroundScaledMode and then draws it inside
  the viewport with the provided \a painter. The scaled version is buffered in
  mScaledBackgroundPixmap to prevent expensive rescaling at every redraw. It is only updated, when
  the axis rect has changed in a way that requires a rescale of the background pixmap (this is
  dependent on the \ref setBackgroundScaledMode), or when a differend axis background pixmap was
  set.

  Note that this function does not draw a fill with the background brush
  (\ref setBackground(const QBrush &brush)) beneath the pixmap.

  \see setBackground, setBackgroundScaled, setBackgroundScaledMode
*/
void QCustomPlot::drawBackground(QCPPainter* painter)
{
    // Note: background color is handled in individual replot/save functions

    // draw background pixmap (on top of fill, if brush specified):
    PROFILE_HERE_N("QCustomPlot::drawBackground");
    if (!mBackgroundPixmap.isNull())
    {
        if (mBackgroundScaled)
        {
            // check whether mScaledBackground needs to be updated:
            QSize scaledSize(mBackgroundPixmap.size());
            scaledSize.scale(mViewport.size(), mBackgroundScaledMode);
            if (mScaledBackgroundPixmap.size() != scaledSize)
                mScaledBackgroundPixmap = mBackgroundPixmap.scaled(
                    mViewport.size(), mBackgroundScaledMode, Qt::SmoothTransformation);
            painter->drawPixmap(mViewport.topLeft(), mScaledBackgroundPixmap,
                                QRect(0, 0, mViewport.width(), mViewport.height())
                                    & mScaledBackgroundPixmap.rect());
        }
        else
        {
            painter->drawPixmap(mViewport.topLeft(), mBackgroundPixmap,
                                QRect(0, 0, mViewport.width(), mViewport.height()));
        }
    }
}

/*! \internal

  Goes through the layers and makes sure this QCustomPlot instance holds the correct number of
  paint buffers and that they have the correct configuration (size, pixel ratio, etc.).
  Allocations, reallocations and deletions of paint buffers are performed as necessary. It also
  associates the paint buffers with the layers, so they draw themselves into the right buffer when
  \ref QCPLayer::drawToPaintBuffer is called. This means it associates adjacent \ref
  QCPLayer::lmLogical layers to a mutual paint buffer and creates dedicated paint buffers for
  layers in \ref QCPLayer::lmBuffered mode.

  This method uses \ref createPaintBuffer to create new paint buffers.

  After this method, the paint buffers are empty (filled with \c Qt::transparent) and invalidated
  (so an attempt to replot only a single buffered layer causes a full replot).

  This method is called in every \ref replot call, prior to actually drawing the layers (into their
  associated paint buffer). If the paint buffers don't need changing/reallocating, this method
  basically leaves them alone and thus finishes very fast.
*/
void QCustomPlot::setupPaintBuffers()
{
    PROFILE_HERE;
    int bufferIndex = 0;
    if (mPaintBuffers.isEmpty())
        mPaintBuffers.append(QSharedPointer<QCPAbstractPaintBuffer>(createPaintBuffer("default")));

    for (int layerIndex = 0; layerIndex < mLayers.size(); ++layerIndex)
    {
        QCPLayer* layer = mLayers.at(layerIndex);
        if (layer->mode() == QCPLayer::lmLogical)
        {
            layer->mPaintBuffer = mPaintBuffers.at(bufferIndex).toWeakRef();
        }
        else if (layer->mode() == QCPLayer::lmBuffered)
        {
            ++bufferIndex;
            if (bufferIndex >= mPaintBuffers.size())
                mPaintBuffers.append(
                    QSharedPointer<QCPAbstractPaintBuffer>(createPaintBuffer(layer->name())));
            layer->mPaintBuffer = mPaintBuffers.at(bufferIndex).toWeakRef();
            if (layerIndex < mLayers.size() - 1
                && mLayers.at(layerIndex + 1)->mode()
                    == QCPLayer::lmLogical) // not last layer, and next one is logical, so prepare
                                            // another buffer for next layerables
            {
                ++bufferIndex;
                if (bufferIndex >= mPaintBuffers.size())
                    mPaintBuffers.append(QSharedPointer<QCPAbstractPaintBuffer>(
                        createPaintBuffer(layer->name() + "_logical")));
            }
        }
    }
    // remove unneeded buffers:
    while (mPaintBuffers.size() - 1 > bufferIndex)
        mPaintBuffers.removeLast();
    // resize buffers to viewport size and clear dirty ones:
    for (auto& buffer : mPaintBuffers)
    {
        buffer->setSize(viewport().size()); // may set contentDirty if size changed
        if (buffer->contentDirty())
        {
            // Check if all layers on this buffer can skip repaint via translation.
            // If so, the old content is still valid — the compositor shifts it.
            bool allCanTranslate = true;
            for (auto* layer : std::as_const(mLayers))
            {
                if (layer->mPaintBuffer.toStrongRef() == buffer)
                {
                    if (!layer->canSkipRepaintForTranslation())
                    {
                        allCanTranslate = false;
                        break;
                    }
                }
            }
            if (!allCanTranslate)
            {
                // If the buffer has plottables but none can produce content
                // right now (async pipeline hasn't delivered data), preserve
                // the stale buffer instead of clearing to transparent — avoids
                // a blank-frame blink while the pipeline catches up.
                // Buffers with no plottables (axes, legend) always clear.
                bool hasPlottables = false;
                bool anyCanProduce = false;
                bool hasAnyChildren = false;
                for (auto* layer : std::as_const(mLayers))
                {
                    if (layer->mPaintBuffer.toStrongRef() == buffer)
                    {
                        hasAnyChildren |= !layer->mChildren.isEmpty();
                        for (QCPAbstractPlottable* p : layer->plottableChildren())
                        {
                            hasPlottables = true;
                            if (p->canProduceContent())
                            {
                                anyCanProduce = true;
                                break;
                            }
                        }
                        if (anyCanProduce)
                            break;
                    }
                }
                if (!hasAnyChildren && !buffer->invalidated())
                {
                    buffer->setContentDirty(false);
                }
                else if (!hasPlottables || anyCanProduce)
                {
                    buffer->clear(Qt::transparent);
                    buffer->setInvalidated();
                }
                else
                {
                    // Plottables exist but none can produce content (async
                    // pipeline busy). Skip both clearing and redrawing so
                    // the stale frame stays on screen instead of blinking.
                    buffer->setContentDirty(false);
                }
            }
        }
    }
}

/*! \internal

  This method is used by \ref setupPaintBuffers when it needs to create new paint buffers.

  Creates a new \ref QCPPaintBufferPixmap initialized with the proper size and device pixel ratio,
  and returns it.
*/
QCPAbstractPaintBuffer* QCustomPlot::createPaintBuffer(const QString& layerName)
{
    if (mRhi)
        return new QCPPaintBufferRhi(viewport().size(), mBufferDevicePixelRatio, layerName, mRhi);
    return new QCPPaintBufferPixmap(viewport().size(), mBufferDevicePixelRatio, layerName);
}

/*!
  This method returns whether any of the paint buffers held by this QCustomPlot instance are
  invalidated.

  If any buffer is invalidated, a partial replot (\ref QCPLayer::replot) is not allowed and always
  causes a full replot (\ref QCustomPlot::replot) of all layers. This is the case when for example
  the layer order has changed, new layers were added or removed, layer modes were changed (\ref
  QCPLayer::setMode), or layerables were added or removed.

  \see QCPAbstractPaintBuffer::setInvalidated
*/
bool QCustomPlot::hasInvalidatedPaintBuffers()
{
    for (auto& buffer : mPaintBuffers)
    {
        if (buffer->invalidated())
            return true;
    }
    return false;
}

void QCustomPlot::ensureAtLeastOneBufferDirty()
{
    for (const auto& b : mPaintBuffers)
    {
        if (b->contentDirty())
            return;
    }
    // No buffer is dirty — use markAffectedLayersDirty() to precisely dirty
    // only range-dependent layers (axes, grid, plottables), skipping static
    // buffers (background, empty overlay) to avoid needless memset clears.
    for (auto* ar : axisRects())
        ar->markAffectedLayersDirty();
    // Fallback if nothing got dirtied (e.g. no axis rects)
    for (const auto& b : mPaintBuffers)
    {
        if (b->contentDirty())
            return;
    }
    for (auto& b : mPaintBuffers)
        b->setContentDirty(true);
}

/*! \internal

  This method is used by \ref QCPAxisRect::removeAxis to report removed axes to the QCustomPlot
  so it may clear its QCustomPlot::xAxis, yAxis, xAxis2 and yAxis2 members accordingly.
*/
void QCustomPlot::axisRemoved(QCPAxis* axis)
{
    if (xAxis == axis)
        xAxis = nullptr;
    if (xAxis2 == axis)
        xAxis2 = nullptr;
    if (yAxis == axis)
        yAxis = nullptr;
    if (yAxis2 == axis)
        yAxis2 = nullptr;

    // Note: No need to take care of range drag axes and range zoom axes, because they are stored in
    // smart pointers
}

/*! \internal

  This method is used by the QCPLegend destructor to report legend removal to the QCustomPlot so
  it may clear its QCustomPlot::legend member accordingly.
*/
void QCustomPlot::legendRemoved(QCPLegend* legend)
{
    if (this->legend == legend)
        this->legend = nullptr;
}

/*! \internal

  This slot is connected to the selection rect's \ref QCPSelectionRect::accepted signal when \ref
  setSelectionRectMode is set to \ref QCP::srmSelect.

  First, it determines which axis rect was the origin of the selection rect judging by the starting
  point of the selection. Then it goes through the plottables (\ref QCPAbstractPlottable1D to be
  precise) associated with that axis rect and finds the data points that are in \a rect. It does
  this by querying their \ref QCPAbstractPlottable1D::selectTestRect method.

  Then, the actual selection is done by calling the plottables' \ref
  QCPAbstractPlottable::selectEvent, placing the found selected data points in the \a details
  parameter as <tt>QVariant(\ref QCPDataSelection)</tt>. All plottables that weren't touched by \a
  rect receive a \ref QCPAbstractPlottable::deselectEvent.

  \see processRectZoom
*/
void QCustomPlot::processRectSelection(QRect rect, QMouseEvent* event)
{
    using SelectionCandidate = QPair<QPointer<QCPAbstractPlottable>, QCPDataSelection>;
    using SelectionCandidates = QMultiMap<int, SelectionCandidate>; // map key is number of selected data points, so we have selections sorted by size

    const QPointer<QCustomPlot> alive(this); // a (de)selection slot may delete the plot
    bool selectionStateChanged = false;

    if (mInteractions.testFlag(QCP::iSelectPlottables))
    {
        SelectionCandidates potentialSelections;
        QRectF rectF(rect.normalized());
        if (QCPArea* affectedArea = areaAt(rectF.topLeft()))
        {
            // determine plottables that were hit by the rect and thus are candidates for selection,
            // in the plot's own order as 2.x collected them: going system by system would hand the
            // same plottables over grouped by system instead.
            for (auto plottable : std::as_const(mPlottables))
            {
                if (!plottable->system() || plottable->system()->area() != affectedArea)
                    continue;
                if (QCPPlottableInterface1D* plottableInterface = plottable->interface1D())
                {
                    QCPDataSelection dataSel = plottableInterface->selectTestRect(rectF, true);
                    if (!dataSel.isEmpty())
                        potentialSelections.insert(dataSel.dataPointCount(),
                                                   SelectionCandidate(plottable, dataSel));
                }
            }

            if (!mInteractions.testFlag(QCP::iMultiSelect))
            {
                // only leave plottable with most selected points in map, since we will only select
                // a single plottable:
                if (!potentialSelections.isEmpty())
                {
                    SelectionCandidates::iterator it = potentialSelections.begin();
                    while (it
                           != std::prev(potentialSelections.end())) // erase all except last element
                        it = potentialSelections.erase(it);
                }
            }

            bool additive = event->modifiers().testFlag(mMultiSelectModifier);
            // deselect all other layerables if not additive selection, except the plottable
            // selected afterwards:
            if (!additive)
                selectionStateChanged |= deselectAllExcept(
                    potentialSelections.isEmpty() ? nullptr
                                                  : potentialSelections.constBegin()->first.data());

            // go through selections in reverse (largest selection first) and emit select events:
            SelectionCandidates::const_iterator it = potentialSelections.constEnd();
            while (it != potentialSelections.constBegin())
            {
                --it;
                QCPAbstractPlottable* plottable = it.value().first;
                if (plottable && mInteractions.testFlag(plottable->selectionCategory()))
                {
                    bool selChanged = false;
                    plottable->selectEvent(event, additive, QVariant::fromValue(it.value().second),
                                           &selChanged);
                    selectionStateChanged |= selChanged;
                }
            }
        }
    }

    if (!alive)
        return;
    if (selectionStateChanged)
        announceUserSelection();
    else if (QCPLayer* band = mSelectionRect ? mSelectionRect->layer() : nullptr)
        band->replot();
}

/*! \internal

  This slot is connected to the selection rect's \ref QCPSelectionRect::accepted signal when \ref
  setSelectionRectMode is set to \ref QCP::srmZoom.

  It determines which axis rect was the origin of the selection rect judging by the starting point
  of the selection, and then zooms the axes defined via \ref QCPAxisRect::setRangeZoomAxes to the
  provided \a rect (see \ref QCPAxisRect::zoom).

  \see processRectSelection
*/
void QCustomPlot::processRectZoom(QRect rect, [[maybe_unused]] QMouseEvent* event)
{
    if (QCPAxisRect* axisRect = qobject_cast<QCPAxisRect*>(areaAt(rect.topLeft())))
    {
        QList<QCPAxis*> affectedAxes = QList<QCPAxis*>()
            << axisRect->rangeZoomAxes(Qt::Horizontal) << axisRect->rangeZoomAxes(Qt::Vertical);
        affectedAxes.removeAll(static_cast<QCPAxis*>(nullptr));
        const QPointer<QCustomPlot> alive(this); // a rangeChanged slot may delete the plot
        axisRect->zoom(QRectF(rect), affectedAxes);
        if (!alive)
            return;
    }
    replot(rpQueuedReplot); // always replot to make selection rect disappear
}

/*! \internal

  This method is called when a simple left mouse click was detected on the QCustomPlot surface.

  It first determines the layerable that was hit by the click, and then calls its \ref
  QCPLayerable::selectEvent. All other layerables receive a QCPLayerable::deselectEvent (unless the
  multi-select modifier was pressed, see \ref setMultiSelectModifier).

  In this method the hit layerable is determined a second time using \ref layerableAt (after the
  one in \ref mousePressEvent), because we want \a onlySelectable set to true this time. This
  implies that the mouse event grabber (mMouseEventLayerable) may be a different one from the
  clicked layerable determined here. For example, if a non-selectable layerable is in front of a
  selectable layerable at the click position, the front layerable will receive mouse events but the
  selectable one in the back will receive the \ref QCPLayerable::selectEvent.

  \see processRectSelection, QCPLayerable::selectTest
*/
void QCustomPlot::processPointSelection(QMouseEvent* event)
{
    const QPointer<QCustomPlot> alive(this); // a (de)selection slot may delete the plot
    QVariant details;
    const QPointer<QCPLayerable> clickedLayerable = layerableAt(event->pos(), true, &details);
    bool selectionStateChanged = false;
    bool additive = mInteractions.testFlag(QCP::iMultiSelect)
        && event->modifiers().testFlag(mMultiSelectModifier);
    // deselect all other layerables if not additive selection:
    if (!additive)
        selectionStateChanged |= deselectAllExcept(clickedLayerable);
    if (clickedLayerable && mInteractions.testFlag(clickedLayerable->selectionCategory()))
    {
        // a layerable was actually clicked, call its selectEvent:
        bool selChanged = false;
        clickedLayerable->selectEvent(event, additive, details, &selChanged);
        selectionStateChanged |= selChanged;
    }
    if (alive && selectionStateChanged)
        announceUserSelection();
}

void QCustomPlot::announceUserSelection()
{
    replot(rpQueuedReplot);
    qcpEmit(this, this, &QCustomPlot::selectionChangedByUser); // last: a slot may delete the plot
}

/*! \internal

  Registers the specified plottable with this QCustomPlot and, if \ref setAutoAddPlottableToLegend
  is enabled, adds it to the legend (QCustomPlot::legend). QCustomPlot takes ownership of the
  plottable.

  Returns true on success, i.e. when \a plottable isn't already in this plot and the parent plot of
  \a plottable is this QCustomPlot.

  This method is called automatically in the QCPAbstractPlottable base class constructor.
*/
bool QCustomPlot::registerPlottable(QCPAbstractPlottable* plottable)
{
    QCP_CHECK_OR_RETURN(!mBeingDestroyed, "the plot is being destroyed", false);
    if (mPlottables.contains(plottable))
    {
        qDebug() << Q_FUNC_INFO << "plottable already added to this QCustomPlot:"
                 << reinterpret_cast<quintptr>(plottable);
        return false;
    }
    if (plottable->parentPlot() != this)
    {
        qDebug() << Q_FUNC_INFO << "plottable not created with this QCustomPlot as parent:"
                 << reinterpret_cast<quintptr>(plottable);
        return false;
    }

    mPlottables.append(plottable);
    // possibly add plottable to legend:
    if (mAutoAddPlottableToLegend)
        plottable->addToLegend();
    if (!plottable->layer()) // usually the layer is already set in the constructor of the plottable
                             // (via QCPLayerable constructor)
        plottable->setLayer(currentLayer());
    return true;
}

/*! \internal

  In order to maintain the simplified graph interface of QCustomPlot, this method is called by the
  QCPGraph constructor to register itself with this QCustomPlot's internal graph list. Returns true
  on success, i.e. if \a graph is valid and wasn't already registered with this QCustomPlot.

  This graph specific registration happens in addition to the call to \ref registerPlottable by the
  QCPAbstractPlottable base class.
*/
bool QCustomPlot::registerGraph(QCPGraph* graph)
{
    if (mBeingDestroyed) // registerPlottable reported it already
        return false;
    if (!graph)
    {
        qDebug() << Q_FUNC_INFO << "passed graph is zero";
        return false;
    }
    if (mGraphs.contains(graph))
    {
        qDebug() << Q_FUNC_INFO << "graph already registered with this QCustomPlot";
        return false;
    }

    mGraphs.append(graph);
    return true;
}

/*! \internal

  Called by the destructor of \a item: a plain delete (or deleteLater) leaves no dead pointer in
  the item list.
*/
void QCustomPlot::itemDestroyed(QCPAbstractItem* item)
{
    // simplify: one linear search per deleted item (~1 us at 10k items, the order of Qt's own
    // children search). item(index) needs the order; an index stored on the item or tombstones
    // would make it O(1) if one-by-one deletes of very large sets ever matter.
    mItems.removeOne(item);
    ++mItemsEpoch;
}

/*! \internal

  Registers the specified item with this QCustomPlot. QCustomPlot takes ownership of the item.

  Returns true on success, i.e. when \a item wasn't already in the plot and the parent plot of \a
  item is this QCustomPlot.

  This method is called automatically in the QCPAbstractItem base class constructor.
*/
bool QCustomPlot::registerItem(QCPAbstractItem* item)
{
    QCP_CHECK_OR_RETURN(!mBeingDestroyed, "the plot is being destroyed", false);
    // A live item knows the plot that lists it (registerItem sets it below, detach clears it), so
    // the duplicate test needs no scan.
    if (item->mRegisteredPlot == this)
    {
        qDebug() << Q_FUNC_INFO
                 << "item already added to this QCustomPlot:" << reinterpret_cast<quintptr>(item);
        return false;
    }
    if (item->parentPlot() != this)
    {
        qDebug() << Q_FUNC_INFO << "item not created with this QCustomPlot as parent:"
                 << reinterpret_cast<quintptr>(item);
        return false;
    }

    mItems.append(item);
    // Items annotate the data, so they go to the "annotations" layer whatever the current layer
    // is (a user who removed that layer keeps the current-layer default). An explicit setLayer
    // afterwards still moves them.
    if (QCPLayer* annotations = layer(QLatin1String("annotations")))
        item->setLayer(annotations);
    else if (!item->layer())
        item->setLayer(currentLayer());
    if (mTheme) {
        if (auto* textItem = qobject_cast<QCPItemText*>(item)) {
            textItem->setColor(mTheme->foreground());
            textItem->setSelectedColor(mTheme->selection());
        }
    }
    return true;
}

/*! \internal

  Assigns all layers their index (QCPLayer::mIndex) in the mLayers list. This method is thus called
  after every operation that changes the layer indices, like layer removal, layer creation, layer
  moving.
*/
void QCustomPlot::updateLayerIndices() const
{
    for (int i = 0; i < mLayers.size(); ++i)
        mLayers.at(i)->mIndex = i;
}

/*! \internal

  Returns every layerable hit at pixel position \a pos, topmost first: the one rule behind the
  press, the double-click, the wheel and the selection.

  \a action says what the hit is for: Select skips the layerables that cannot be selected (a
  layerable says so by returning -1 from selectTest() when only selectable ones are asked for),
  Press takes any layerable that is hit, and a wheel asks for Press too. A layerable is hit when its
  selectTest() distance is below the selection tolerance, whatever the action: there is no
  "anywhere in my rectangle" shortcut any more.

  Precedence is the mouse grabber (mMouseEventLayerable, which the event handlers consult before
  this list), then this list. The axis rect lies under the drawables, so it comes last: its own
  drag and its wheel zoom win whenever no drawable above it accepts the event.

  \see layerableAt, layerableListAt
*/
QList<QCPHit> QCustomPlot::hitsAt(const QPointF& pos, QCPHitAction action) const
{
    const bool onlySelectable = action == QCPHitAction::Select;
    const double tol = selectionTolerance();
    QList<QCPHit> result;
    for (int layerIndex = mLayers.size() - 1; layerIndex >= 0; --layerIndex)
    {
        const QCPLayer* layer = mLayers.at(layerIndex);
        const QList<QCPLayerable*> layerables = layer->children();
        // One tree lookup per layer, for the click point inflated by the selection tolerance: an
        // indexed item whose box misses it cannot be hit, so its selectTest() is not called.
        const std::vector<std::uint8_t>* candidates = nullptr;
        if (mItemIndexing && !layerables.isEmpty())
            candidates = &layer->itemCandidates(
                QRectF(pos, QSizeF(0, 0)).adjusted(-tol, -tol, tol, tol));
        for (int i = layerables.size() - 1; i >= 0; --i)
        {
            QCPLayerable* layerable = layerables.at(i);
            if (candidates && !(*candidates)[static_cast<std::size_t>(i)])
                continue;
            if (!layerable->realVisibility())
                continue;
            if (layerable->mouseTransparent())
                continue;
            QVariant details;
            const double dist = layerable->selectTest(pos, onlySelectable, &details);
            if (dist >= 0 && dist < tol)
                result.append({ layerable, details });
        }
    }
    return result;
}

/*! \internal

  Returns the top-most layerable at pixel position \a pos. If \a onlySelectable is set to true,
  only those layerables that are selectable will be considered. (Layerable subclasses communicate
  their selectability via the QCPLayerable::selectTest method, by returning -1.) This is the first
  entry of \ref hitsAt, with action Select when \a onlySelectable is set and Press otherwise.

  \a selectionDetails is an output parameter that contains selection specifics of the affected
  layerable. This is useful if the respective layerable shall be given a subsequent
  QCPLayerable::selectEvent (like in \ref mouseReleaseEvent). \a selectionDetails usually contains
  information about which part of the layerable was hit, in multi-part layerables (e.g.
  QCPAxis::SelectablePart). If the layerable is a plottable, \a selectionDetails contains a \ref
  QCPDataSelection instance with the single data point which is closest to \a pos.

  \see layerableListAt, layoutElementAt, axisRectAt
*/
QCPLayerable* QCustomPlot::layerableAt(const QPointF& pos, bool onlySelectable,
                                       QVariant* selectionDetails) const
{
    const QList<QCPHit> hits
        = hitsAt(pos, onlySelectable ? QCPHitAction::Select : QCPHitAction::Press);
    if (hits.isEmpty())
        return nullptr;
    if (selectionDetails)
        *selectionDetails = hits.first().details;
    return hits.first().layerable;
}

/*! \internal

  Returns the layerables at pixel position \a pos. If \a onlySelectable is set to true, only those
  layerables that are selectable will be considered. (Layerable subclasses communicate their
  selectability via the QCPLayerable::selectTest method, by returning -1.)

  The returned list is sorted by the layerable/drawing order such that the layerable that appears
  on top in the plot is at index 0 of the returned list. If you only need to know the top
  layerable, rather use \ref layerableAt. This is \ref hitsAt with action Select when \a
  onlySelectable is set and Press otherwise.

  \a selectionDetails is an output parameter that contains selection specifics of the affected
  layerable. This is useful if the respective layerable shall be given a subsequent
  QCPLayerable::selectEvent (like in \ref mouseReleaseEvent). \a selectionDetails usually contains
  information about which part of the layerable was hit, in multi-part layerables (e.g.
  QCPAxis::SelectablePart). If the layerable is a plottable, \a selectionDetails contains a \ref
  QCPDataSelection instance with the single data point which is closest to \a pos.

  \see layerableAt, layoutElementAt, axisRectAt
*/
QList<QCPLayerable*> QCustomPlot::layerableListAt(const QPointF& pos, bool onlySelectable,
                                                  QList<QVariant>* selectionDetails) const
{
    QList<QCPLayerable*> result;
    for (const QCPHit& hit :
         hitsAt(pos, onlySelectable ? QCPHitAction::Select : QCPHitAction::Press))
    {
        result.append(hit.layerable);
        if (selectionDetails)
            selectionDetails->append(hit.details);
    }
    return result;
}

/*!
  Saves the plot to a rastered image file \a fileName in the image format \a format. The plot is
  sized to \a width and \a height in pixels and scaled with \a scale. (width 100 and scale 2.0 lead
  to a full resolution file with width 200.) If the \a format supports compression, \a quality may
  be between 0 and 100 to control it.

  Returns true on success. If this function fails, most likely the given \a format isn't supported
  by the system, see Qt docs about QImageWriter::supportedImageFormats().

  The \a resolution will be written to the image file header (if the file format supports this) and
  has no direct consequence for the quality or the pixel size. However, if opening the image with a
  tool which respects the metadata, it will be able to scale the image to match either a given size
  in real units of length (inch, centimeters, etc.), or the target display DPI. You can specify in
  which units \a resolution is given, by setting \a resolutionUnit. The \a resolution is converted
  to the format's expected resolution unit internally.

  \see saveBmp, saveJpg, savePng, savePdf
*/
bool QCustomPlot::saveRastered(const QString& fileName, int width, int height, double scale,
                               const char* format, int quality, int resolution,
                               QCP::ResolutionUnit resolutionUnit)
{
    QImage buffer = toPixmap(width, height, scale).toImage();

    int dotsPerMeter = 0;
    switch (resolutionUnit)
    {
        case QCP::ruDotsPerMeter:
            dotsPerMeter = resolution;
            break;
        case QCP::ruDotsPerCentimeter:
            dotsPerMeter = resolution * 100;
            break;
        case QCP::ruDotsPerInch:
            dotsPerMeter = int(resolution / 0.0254);
            break;
    }
    buffer.setDotsPerMeterX(dotsPerMeter); // this is saved together with some image formats, e.g.
                                           // PNG, and is relevant when opening image in other tools
    buffer.setDotsPerMeterY(dotsPerMeter); // this is saved together with some image formats, e.g.
                                           // PNG, and is relevant when opening image in other tools
    if (!buffer.isNull())
        return buffer.save(fileName, format, quality);
    else
        return false;
}

/*!
  Renders the plot to a pixmap and returns it.

  The plot is sized to \a width and \a height in pixels and scaled with \a scale. (width 100 and
  scale 2.0 lead to a full resolution pixmap with width 200.)

  \see toPainter, saveRastered, saveBmp, savePng, saveJpg, savePdf
*/
QPixmap QCustomPlot::toPixmap(int width, int height, double scale)
{
    const QCPEmissionScope scope(this);
    const QPointer<QCustomPlot> alive(this); // an afterLayout slot may delete the plot
    settle();
    if (!alive)
        return QPixmap();
    // Temporarily mutates mViewport for export. Safe because single-threaded and mReplotting
    // prevents recursive replot. Viewport is restored before returning.
    int newWidth, newHeight;
    if (width == 0 || height == 0)
    {
        newWidth = this->width();
        newHeight = this->height();
    }
    else
    {
        newWidth = width;
        newHeight = height;
    }
    int scaledWidth = qRound(scale * newWidth);
    int scaledHeight = qRound(scale * newHeight);

    QPixmap result(scaledWidth, scaledHeight);
    result.fill(mBackgroundBrush.style() == Qt::SolidPattern
                    ? mBackgroundBrush.color()
                    : Qt::transparent); // if using non-solid pattern, make transparent now and draw
                                        // brush pattern later
    QCPPainter painter;
    painter.begin(&result);
    if (painter.isActive())
    {
        syncViewportToWidgetSize();
        QRect oldViewport = viewport();
        setViewport(QRect(0, 0, newWidth, newHeight));
        painter.setMode(QCPPainter::pmNoCaching);
        if (!qFuzzyCompare(scale, 1.0))
        {
            if (scale > 1.0) // for scale < 1 we always want cosmetic pens where possible, because
                             // else lines might disappear for very small scales
                painter.setMode(QCPPainter::pmNonCosmetic);
            painter.scale(scale, scale);
        }
        if (mBackgroundBrush.style() != Qt::SolidPattern
            && mBackgroundBrush.style()
                != Qt::NoBrush) // solid fills were done a few lines above with QPixmap::fill
            painter.fillRect(mViewport, mBackgroundBrush);
        draw(&painter);
        if (alive)
            restoreViewportAfterExport(oldViewport);
        painter.end();
    }
    else // might happen if pixmap has width or height zero
    {
        qDebug() << Q_FUNC_INFO << "Couldn't activate painter on pixmap";
        return QPixmap();
    }
    return result;
}

/*!
  Renders the plot using the passed \a painter.

  The plot is sized to \a width and \a height in pixels. If the \a painter's scale is not 1.0, the
  resulting plot will appear scaled accordingly.

  \note If you are restricted to using a QPainter (instead of QCPPainter), create a temporary
  QPicture and open a QCPPainter on it. Then call \ref toPainter with this QCPPainter. After ending
  the paint operation on the picture, draw it with the QPainter. This will reproduce the painter
  actions the QCPPainter took, with a QPainter.

  \see toPixmap
*/
void QCustomPlot::toPainter(QCPPainter* painter, int width, int height)
{
    const QCPEmissionScope scope(this);
    const QPointer<QCustomPlot> alive(this); // an afterLayout slot may delete the plot
    settle();
    if (!alive)
        return;
    // this method is somewhat similar to toPixmap. Change something here, and a change in toPixmap
    // might be necessary, too.
    int newWidth, newHeight;
    if (width == 0 || height == 0)
    {
        newWidth = this->width();
        newHeight = this->height();
    }
    else
    {
        newWidth = width;
        newHeight = height;
    }

    if (painter->isActive())
    {
        syncViewportToWidgetSize();
        QRect oldViewport = viewport();
        setViewport(QRect(0, 0, newWidth, newHeight));
        painter->setMode(QCPPainter::pmNoCaching);
        if (mBackgroundBrush.style()
            != Qt::NoBrush) // unlike in toPixmap, we can't do QPixmap::fill for Qt::SolidPattern
                            // brush style, so we also draw solid fills with fillRect here
            painter->fillRect(mViewport, mBackgroundBrush);
        draw(painter);
        if (alive)
            restoreViewportAfterExport(oldViewport);
    }
    else
        qDebug() << Q_FUNC_INFO << "Passed painter is not active";
}

// The owner of each family removes its member (see qcp::detail::removeObject in handle.h).
namespace qcp::detail
{
void removeObject(QCPAbstractPlottable& plottable) { delete &plottable; }
void removeObject(QCPAbstractItem& item) { delete &item; }
void removeObject(QCPBarsGroup& group) { delete &group; }
void removeObject(QCPPlottableGroup& group) { delete &group; }

void removeObject(QCPAxis& axis)
{
    if (QCPAxisRect* rect = axis.axisRect())
        rect->removeAxis(&axis);
    else
        delete &axis;
}

void removeObject(QCPLayer& layer)
{
    if (QCustomPlot* plot = layer.parentPlot())
        plot->removeLayer(&layer);
    else
        delete &layer;
}

void removeObject(QCPLayoutElement& element)
{
    if (QCPLayout* layout = element.layout())
        layout->take(&element);
    delete &element;
}
} // namespace qcp::detail
