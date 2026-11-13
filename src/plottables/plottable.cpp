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

#include "plottable.h"
#include "../coordinate-system.h"
#include "../layoutelements/area.h"

#include "axis/axis.h"
#include <QSet>
#include "core.h"
#include "layoutelements/layoutelement-axisrect.h"
#include "layoutelements/layoutelement-legend.h"
#include "plottable1d.h"
#include "misuse.h"
#include "painting/painter.h"
#include "theme.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPSelectionDecorator
////////////////////////////////////////////////////////////////////////////////////////////////////

/*! \class QCPSelectionDecorator
  \brief Controls how a plottable's data selection is drawn

  Each \ref QCPAbstractPlottable instance has one \ref QCPSelectionDecorator (accessible via \ref
  QCPAbstractPlottable::selectionDecorator) and uses it when drawing selected segments of its data.

  The selection decorator controls both pen (\ref setPen) and brush (\ref setBrush), as well as the
  scatter style (\ref setScatterStyle) if the plottable draws scatters. Since a \ref
  QCPScatterStyle is itself composed of different properties such as color shape and size, the
  decorator allows specifying exactly which of those properties shall be used for the selected data
  point, via \ref setUsedScatterProperties.

  A \ref QCPSelectionDecorator subclass instance can be passed to a plottable via \ref
  QCPAbstractPlottable::setSelectionDecorator, allowing greater customizability of the appearance
  of selected segments.

  Use \ref copyFrom to easily transfer the settings of one decorator to another one. This is
  especially useful since plottables take ownership of the passed selection decorator, and thus the
  same decorator instance can not be passed to multiple plottables.

  Selection decorators can also themselves perform drawing operations by reimplementing \ref
  drawDecoration, which is called by the plottable's draw method. The base class \ref
  QCPSelectionDecorator does not make use of this however. For example, \ref
  QCPSelectionDecoratorBracket draws brackets around selected data segments.
*/

/*!
  Creates a new QCPSelectionDecorator instance with default values
*/
QCPSelectionDecorator::QCPSelectionDecorator()
        : mPen(QColor(80, 80, 255), 2.5)
        , mBrush(Qt::NoBrush)
        , mUsedScatterProperties(QCPScatterStyle::spNone)
        , mPlottable(nullptr)
{
}

QCPSelectionDecorator::~QCPSelectionDecorator() { }

/*!
  Sets the pen that will be used by the parent plottable to draw selected data segments.
*/
void QCPSelectionDecorator::setPen(const QPen& pen)
{
    mPen = pen;
    if (mPlottable)
        mPlottable->markDirty(); // the decorator paints through the plottable, which records its look
}

/*!
  Sets the brush that will be used by the parent plottable to draw selected data segments.
*/
void QCPSelectionDecorator::setBrush(const QBrush& brush)
{
    mBrush = brush;
    if (mPlottable)
        mPlottable->markDirty();
}

/*!
  Sets the scatter style that will be used by the parent plottable to draw scatters in selected
  data segments.

  \a usedProperties specifies which parts of the passed \a scatterStyle will be used by the
  plottable. The used properties can also be changed via \ref setUsedScatterProperties.
*/
void QCPSelectionDecorator::setScatterStyle(const QCPScatterStyle& scatterStyle,
                                            QCPScatterStyle::ScatterProperties usedProperties)
{
    mScatterStyle = scatterStyle;
    setUsedScatterProperties(usedProperties);
    if (mPlottable)
        mPlottable->markDirty(); // the decorator paints through the plottable, which records its look
}

/*!
  Use this method to define which properties of the scatter style (set via \ref setScatterStyle)
  will be used for selected data segments. All properties of the scatter style that are not
  specified in \a properties will remain as specified in the plottable's original scatter style.

  \see QCPScatterStyle::ScatterProperty
*/
void QCPSelectionDecorator::setUsedScatterProperties(
    const QCPScatterStyle::ScatterProperties& properties)
{
    mUsedScatterProperties = properties;
    if (mPlottable)
        mPlottable->markDirty(); // the decorator paints through the plottable, which records its look
}

/*!
  Returns the scatter style that the parent plottable shall use for selected scatter points. The
  plottable's original (unselected) scatter style must be passed as \a unselectedStyle. Depending
  on the setting of \ref setUsedScatterProperties, the returned scatter style is a mixture of this
  selecion decorator's scatter style (\ref setScatterStyle), and \a unselectedStyle.

  \see setScatterStyle
*/
QCPScatterStyle
QCPSelectionDecorator::getFinalScatterStyle(const QCPScatterStyle& unselectedStyle) const
{
    QCPScatterStyle result(unselectedStyle);
    result.setFromOther(mScatterStyle, mUsedScatterProperties);

    // if style shall inherit pen from plottable (has no own pen defined), give it the selected
    // plottable pen explicitly, so it doesn't use the unselected plottable pen when used in the
    // plottable:
    if (!result.isPenDefined())
        result.setPen(mPen);

    return result;
}

/*!
  Copies all properties (e.g. color, fill, scatter style) of the \a other selection decorator to
  this selection decorator.
*/
void QCPSelectionDecorator::copyFrom(const QCPSelectionDecorator* other)
{
    setPen(other->pen());
    setBrush(other->brush());
    setScatterStyle(other->scatterStyle(), other->usedScatterProperties());
}

/*!
  This method is called by all plottables' draw methods to allow custom selection decorations to be
  drawn. Use the passed \a painter to perform the drawing operations. \a selection carries the data
  selection for which the decoration shall be drawn.

  The default base class implementation of \ref QCPSelectionDecorator has no special decoration, so
  this method does nothing.
*/
void QCPSelectionDecorator::drawDecoration([[maybe_unused]] QCPPainter* painter,
                                           [[maybe_unused]] QCPDataSelection selection)
{
}

/*! \internal

  This method is called as soon as a selection decorator is associated with a plottable, by a call
  to \ref QCPAbstractPlottable::setSelectionDecorator. This way the selection decorator can obtain a
  pointer to the plottable that uses it (e.g. to access data points via the \ref
  QCPAbstractPlottable::interface1D interface).

  If the selection decorator was already added to a different plottable before, this method aborts
  the registration and returns false.
*/
bool QCPSelectionDecorator::registerWithPlottable(QCPAbstractPlottable* plottable)
{
    if (!mPlottable)
    {
        mPlottable = plottable;
        return true;
    }
    else
    {
        qDebug() << Q_FUNC_INFO << "This selection decorator is already registered with plottable:"
                 << reinterpret_cast<quintptr>(mPlottable);
        return false;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPAbstractPlottable
////////////////////////////////////////////////////////////////////////////////////////////////////

/*! \class QCPAbstractPlottable
  \brief The abstract base class for all data representing objects in a plot.

  It defines a very basic interface like name, pen, brush, visibility etc. Since this class is
  abstract, it can't be instantiated. Use one of the subclasses or create a subclass yourself to
  create new ways of displaying data (see "Creating own plottables" below). Plottables that display
  one-dimensional data (i.e. data points have a single key dimension and one or multiple values at
  each key) are based off of the template subclass \ref QCPAbstractPlottable1D, see details
  there.

  All further specifics are in the subclasses, for example:
  \li A normal graph with possibly a line and/or scatter points \ref QCPGraph
  (typically created with \ref QCustomPlot::addGraph)
  \li A parametric curve: \ref QCPCurve
  \li A bar chart: \ref QCPBars
  \li A statistical box plot: \ref QCPStatisticalBox
  \li A color encoded two-dimensional map: \ref QCPColorMap
  \li An OHLC/Candlestick chart: \ref QCPFinancial

  \section plottables-subclassing Creating own plottables

  Subclassing directly from QCPAbstractPlottable is only recommended if you wish to display
  two-dimensional data like \ref QCPColorMap, i.e. two logical key dimensions and one (or more)
  data dimensions. If you want to display data with only one logical key dimension, you should
  rather derive from \ref QCPAbstractPlottable1D.

  If subclassing QCPAbstractPlottable directly, these are the virtual functions you implement:
  \li \ref draw(QCPDrawContext&), which records what the plottable draws
  \li \ref drawLegendIcon(QCPDrawContext&), optionally, to record an icon of its own
  \li \ref selectTest, optionally: the base answers a click from the recording
  \li \ref getKeyRange and \ref getValueRange, optionally: the base answers from the recording

  See the documentation of those functions for what they need to do, and
  docs/writing-a-plot-type.md for a worked example.

  Record positions as \c QCP::Data: the frame maps them to pixels, a vertical key axis included.

  Here are some important members you inherit from QCPAbstractPlottable:
  <table>
  <tr>
    <td>QCustomPlot *\b mParentPlot</td>
    <td>A pointer to the parent QCustomPlot instance. The parent plot is inferred from the axes that
  are passed in the constructor.</td>
  </tr><tr>
    <td>QString \b mName</td>
    <td>The name of the plottable.</td>
  </tr><tr>
    <td>QPen \b mPen</td>
    <td>The generic pen of the plottable. You should use this pen for the most prominent data
  representing lines in the plottable (e.g QCPGraph uses this pen for its graph lines and
  scatters)</td>
  </tr><tr>
    <td>QBrush \b mBrush</td>
    <td>The generic brush of the plottable. You should use this brush for the most prominent
  fillable structures in the plottable (e.g. QCPGraph uses this brush to control filling under the
  graph)</td>
  </tr><tr>
    <td>\ref QCPAxis* \b keyAxisRaw(), \b valueAxisRaw()</td>
    <td>The key and value axes this plottable draws through, raw: the cartesian system's
  projection, null in any other system (use \ref system() there). Call their QCPAxis::coordToPixel
  functions to translate coordinates to pixels in either the key or value dimension. Make sure to
  check whether the pointer is \c nullptr before using it. If one of the axes is null, don't draw
  the plottable.</td>
  </tr><tr>
    <td>\ref QCPSelectionDecorator \b mSelectionDecorator</td>
    <td>The currently set selection decorator which specifies how selected data of the plottable
  shall be drawn and decorated. When drawing your data, you must consult this decorator for the
  appropriate pen/brush before drawing unselected/selected data segments. Finally, you should call
  its \ref QCPSelectionDecorator::drawDecoration method at the end of your \ref draw
  implementation.</td>
  </tr><tr>
    <td>\ref QCP::SelectionType \b mSelectable</td>
    <td>In which composition, if at all, this plottable's data may be selected. Enforcing this
  setting on the data selection is done by QCPAbstractPlottable automatically.</td>
  </tr><tr>
    <td>\ref QCPDataSelection \b mSelection</td>
    <td>Holds the current selection state of the plottable's data, i.e. the selected data ranges
  (\ref QCPDataRange).</td>
  </tr>
  </table>
*/

/* start of documentation of inline functions */

/*! \fn QCPSelectionDecorator *QCPAbstractPlottable::selectionDecorator() const

  Provides access to the selection decorator of this plottable. The selection decorator controls
  how selected data ranges are drawn (e.g. their pen color and fill), see \ref
  QCPSelectionDecorator for details.

  If you wish to use an own \ref QCPSelectionDecorator subclass, pass an instance of it to \ref
  setSelectionDecorator.
*/

/*! \fn bool QCPAbstractPlottable::selected() const

  Returns true if there are any data points of the plottable currently selected. Use \ref selection
  to retrieve the current \ref QCPDataSelection.
*/

/*! \fn QCPDataSelection QCPAbstractPlottable::selection() const

  Returns a \ref QCPDataSelection encompassing all the data points that are currently selected on
  this plottable.

  \see selected, setSelection, setSelectable
*/

/*! \fn virtual QCPPlottableInterface1D *QCPAbstractPlottable::interface1D()

  If this plottable is a one-dimensional plottable, i.e. it implements the \ref
  QCPPlottableInterface1D, returns the \a this pointer with that type. Otherwise (e.g. in the case
  of a \ref QCPColorMap) returns zero.

  You can use this method to gain read access to data coordinates while holding a pointer to the
  abstract base class only.
*/

/* end of documentation of inline functions */
/* start of documentation of the drawing virtuals */

/*! \fn void QCPAbstractPlottable::drawLegendIcon(QCPPainter *painter, const QRectF &rect) const
  \internal

  called by QCPLegend::draw (via QCPPlottableLegendItem::draw) to create a graphical representation
  of this plottable inside \a rect, next to the plottable name.

  The passed \a painter has its cliprect set to \a rect, so painting outside of \a rect won't
  appear outside the legend icon border.

  It replays the icon recording, fitted to \a rect: the plottable's drawing by default, or the
  summary a class records through \ref drawLegendIcon(QCPDrawContext&).
*/

/*! \fn QCPRange QCPAbstractPlottable::getKeyRange(bool &foundRange, QCP::SignDomain inSignDomain)
  const

  Returns the coordinate range that all data in this plottable span in the key axis dimension. For
  logarithmic plots, one can set \a inSignDomain to either \ref QCP::sdNegative or \ref
  QCP::sdPositive in order to restrict the returned range to that sign domain. E.g. when only
  negative range is wanted, set \a inSignDomain to \ref QCP::sdNegative and all positive points
  will be ignored for range calculation. For no restriction, just set \a inSignDomain to \ref
  QCP::sdBoth (default). \a foundRange is an output parameter that indicates whether a range could
  be found or not. If this is false, you shouldn't use the returned range (e.g. no points in data).

  The default answers this from the recording's data box, without the log rule: a side that
  reaches zero stays, since the range is data, not an item index.

  Note that \a foundRange is not the same as \ref QCPRange::validRange, since the range returned by
  this function may have size zero (e.g. when there is only one data point). In this case \a
  foundRange would return true, but the returned range is not a valid range in terms of \ref
  QCPRange::validRange.

  \see rescaleAxes, getValueRange
*/

/*! \fn QCPRange QCPAbstractPlottable::getValueRange(bool &foundRange, QCP::SignDomain inSignDomain,
  const QCPRange &inKeyRange) const

  Returns the coordinate range that the data points in the specified key range (\a inKeyRange) span
  in the value axis dimension. For logarithmic plots, one can set \a inSignDomain to either \ref
  QCP::sdNegative or \ref QCP::sdPositive in order to restrict the returned range to that sign
  domain. E.g. when only negative range is wanted, set \a inSignDomain to \ref QCP::sdNegative and
  all positive points will be ignored for range calculation. For no restriction, just set \a
  inSignDomain to \ref QCP::sdBoth (default). \a foundRange is an output parameter that indicates
  whether a range could be found or not. If this is false, you shouldn't use the returned range
  (e.g. no points in data).

  If \a inKeyRange has both lower and upper bound set to zero (is equal to <tt>QCPRange()</tt>),
  all data points are considered, without any restriction on the keys.

  The default answers this from the recording's data box, which keeps no values per key: it ignores
  \a inKeyRange. A subclass that needs the visible-key rule overrides this function, as \ref
  QCPBars does.

  Note that \a foundRange is not the same as \ref QCPRange::validRange, since the range returned by
  this function may have size zero (e.g. when there is only one data point). In this case \a
  foundRange would return true, but the returned range is not a valid range in terms of \ref
  QCPRange::validRange.

  \see rescaleAxes, getKeyRange
*/

/* end of documentation of the drawing virtuals */
/* start of documentation of signals */

/*! \fn void QCPAbstractPlottable::selectionChanged()

  This signal is emitted when the selection state of this plottable has changed, either by user
  interaction or by a direct call to \ref setSelection. The slot reads \ref selected to learn
  whether any points are selected, and \ref selection for the selected data ranges.

  \see setSelection
*/

/*! \fn void QCPAbstractPlottable::selectableChanged()

  This signal is emitted when the selectability of this plottable has changed. The slot reads
  \ref selectable.

  \see setSelectable
*/

/* end of documentation of signals */

/*!
  Constructs an abstract plottable which uses \a keyAxis as its key axis ("x") and \a valueAxis as
  its value axis ("y"). \a keyAxis and \a valueAxis must reside in the same QCustomPlot instance
  and have perpendicular orientations. If either of these restrictions is violated, a corresponding
  message is printed to the debug output (qDebug), the construction is not aborted, though.

  A null \a keyAxis or \a valueAxis is a misuse (see \ref qcp::setMisuseHandler): the plottable is
  then built but inert, in no plot and never drawn, and deleting it is up to the caller.

  Since QCPAbstractPlottable is an abstract class that defines the basic interface to plottables,
  it can't be directly instantiated.

  You probably want one of the subclasses like \ref QCPGraph or \ref QCPCurve instead.
*/
namespace
{
//! Whether \a system hands its plottables the 2.x axis pair: only today's cartesian frame does. A
//! plottable with no system at all keeps the axes it was built from; it is never drawn.
bool projectsOntoAxes(const QCPCoordinateSystem* system)
{
    return !system || system->kind() == QCPTransform::Kind::Cartesian;
}

//! \a system when it offers \a required, else null after a misuse report: the object it belongs
//! to stays inert, exactly like one told to join a frame it cannot draw in.
QCPCoordinateSystem* admits(QCPCoordinateSystem* system, QCP::Capabilities required)
{
    if (!system)
        return nullptr;
    return QCPAbstractPlottable::systemAdmits(system, required) ? system : nullptr;
}
} // namespace

bool QCPAbstractPlottable::systemAdmits(const QCPCoordinateSystem* system,
                                        QCP::Capabilities required)
{
    if (!system)
        return false;
    constexpr QCP::Capabilities axisPair
        = QCP::capRectangular | QCP::capStraightKey | QCP::capLanes | QCP::capColourScale;
    if ((required & axisPair) && system->kind() != QCPTransform::Kind::Cartesian)
    {
        qcp::reportMisuse(Q_FUNC_INFO,
                          "only a cartesian system hands out the axis pair this plottable needs");
        return false;
    }
    if (required & ~system->capabilities())
    {
        qcp::reportMisuse(Q_FUNC_INFO,
                          "the coordinate system lacks a capability this plottable needs");
        return false;
    }
    return true;
}

QCPAbstractPlottable::QCPAbstractPlottable(QCPCoordinateSystem* system, QCP::Capabilities required)
        : QCPLayerable(system && system->area() ? system->area()->parentPlot() : nullptr, QString(),
                       system ? static_cast<QCPLayerable*>(system->area()) : nullptr)
        , mName()
        , mAntialiasedFill(true)
        , mAntialiasedScatters(true)
        , mPen(Qt::black)
        , mBrush(Qt::NoBrush)
        , mSystem(admits(system, required))
        , mRequiredCapabilities(required)
        , mSelectable(QCP::stWhole)
        , mSelectionDecorator(nullptr)
        , mKeyAxis(mSystem && projectsOntoAxes(mSystem) ? qobject_cast<QCPAxis*>(mSystem->keyAxis())
                                                        : nullptr)
        , mValueAxis(mSystem && projectsOntoAxes(mSystem)
                         ? qobject_cast<QCPAxis*>(mSystem->valueAxis())
                         : nullptr)
{
    setSelectionDecorator(new QCPSelectionDecorator);
    mBusyDebounceTimer.setSingleShot(true);
    connect(&mBusyDebounceTimer, &QTimer::timeout, this, &QCPAbstractPlottable::onDebounceTimeout);

    if (!mSystem)
    {
        // Without a system there is no plot to join, and a frame that refused the mask said so:
        // the plottable stays inert, on no layer, registers nowhere, and owes no second report on
        // its first draw. The plot it was built on stays its QObject parent, so nothing leaks.
        setParentLayerable(nullptr);
        moveToLayer(nullptr, false);
        mReportedLostAxis = true;
        return;
    }
    mReportedLostAxis = false;
    if (!mParentPlot)
        return;
    if (mParentPlot->registerPlottable(this))
        mRegisteredPlot = mParentPlot;
}

QCPAbstractPlottable::QCPAbstractPlottable(QCPAxis* keyAxis, QCPAxis* valueAxis,
                                           QCP::Capabilities required)
        : QCPAbstractPlottable(qcpSystemOfAxes(keyAxis, valueAxis), required)
{
    // A frame that refused the pair has already said so and left this object inert: the 2.x axes
    // are not a second chance to join it.
    if (qcpSystemOfAxes(keyAxis, valueAxis) && !mSystem)
        return;
    // The 2.x pair is the cartesian projection of the system it resolves to: a system of the user's
    // own draws through its transform, and hands 2.x pixel code no axis at all.
    if (projectsOntoAxes(mSystem))
    {
        mKeyAxis = keyAxis;
        mValueAxis = valueAxis;
    }
    if (!keyAxis && !valueAxis)
        return;
    QCP_CHECK_OR_RETURN(keyAxis && valueAxis, "a plottable needs a key axis and a value axis");
    if (keyAxis->parentPlot() != valueAxis->parentPlot())
    {
        // Neither plot may end up drawing it: report once and leave the object inert, like the
        // both-null case.
        QCP_CHECK(false, "a plottable needs both axes on the same plot");
        moveToLayer(nullptr, false);
        return;
    }
    if (!system())
    {
        // 2.x drew a cross-rect pair (clipped to both rects) and registered a same-orientation one:
        // neither is one frame now. The plottable stays owned by the plot and inert, and this
        // report is the one diagnostic - never an orphan nobody deletes, never a silent no-show.
        qcp::reportMisuse(Q_FUNC_INFO,
                          "the key and value axes are not two axes of one axis rect: the "
                          "plottable is inert");
        QCustomPlot* plot = keyAxis->parentPlot();
        if (plot)
        {
            setParent(plot);
            initializeParentPlot(plot);
            setParentLayerable(keyAxis->axisRect());
            if (plot->registerPlottable(this))
                mRegisteredPlot = plot;
        }
    }
}

/*! \internal

  dependentLayers() raw: the default converts the public list (K8).
*/
QList<QCPLayer*> QCPAbstractPlottable::dependentLayersRaw() const
{
    return qcp::pointers(dependentLayers());
}

QCPAxisRect* QCPAbstractPlottable::axisRectOf(const QCPAxis* axis)
{
    return axis ? static_cast<QCPAxisRect*>(axis->area()) : nullptr;
}

QCPHandle<QCPArea> QCPAbstractPlottable::area() const
{
    return QCPHandle<QCPArea>(mSystem ? mSystem->area().get() : nullptr);
}

QCPAbstractPlottable::~QCPAbstractPlottable()
{
    if (mRegisteredPlot)
        mRegisteredPlot->plottableDestroyed(this);
    if (mSelectionDecorator)
    {
        delete mSelectionDecorator;
        mSelectionDecorator = nullptr;
    }
}

/*!
  Replays the recording that \ref draw(QCPDrawContext&) made.
*/
void QCPAbstractPlottable::draw(QCPPainter* painter)
{
    mCore.paint(*this, *painter, view());
}

//! What the core reads from this plottable this frame.
QCPDrawnView QCPAbstractPlottable::view() const
{
    return qcpDrawnViewOf(*this, clipRect());
}

//! What the recording occupies in data coordinates.
std::optional<QCPDataBox> QCPAbstractPlottable::drawnDataRange() const
{
    return mCore.dataRange(*this, view());
}

//! The pixel distance from \a at to the recording, or none when it missed.
std::optional<double> QCPAbstractPlottable::hitTest(QCP::Pixels at, QVariant* details) const
{
    return mCore.hitTest(*this, at, details, view());
}

/*!
  A click inside the axis rect hits the recording. \a details then names the whole recording as a
  data selection: the recording keeps no per-point index to hand out, and an empty selection would
  make the click select nothing.
*/
double QCPAbstractPlottable::selectTest(const QPointF& pos, bool onlySelectable,
                                        QVariant* details) const
{
    // Built-in plottables miss outside the axis rect unless the plot opts in: their geometry may
    // still be painted there (an unclipped plottable), but a click does not grab it.
    if (!mSystem || !mSystem->contains(pos))
        return -1;

    const double hit = mCore.selectTest(*this, pos, onlySelectable, details, view());
    if (hit < 0 || !details)
        return hit;
    if (details->isValid() && !details->value<QCPDataSelection>().isEmpty())
        return hit;
    const QCPPlottableInterface1D* iface = const_cast<QCPAbstractPlottable*>(this)->interface1D();
    const int count = iface ? iface->dataCount() : 1;
    *details = QVariant::fromValue(QCPDataSelection(QCPDataRange(0, count)));
    return hit;
}

//! The pixels a pan may shift this plottable's layer by instead of repainting it.
QPointF QCPAbstractPlottable::stallPixelOffset() const
{
    return mCore.stallOffset(*this, view());
}

/*!
  The setter rule: a new content generation, then the layer mark the base needs and the dependents
  that draw this plottable's data. A pan or a zoom never reaches it, so a range change still reuses
  the recording.
*/
void QCPAbstractPlottable::markDirty()
{
    if (!mCore.contentChanged())
        return;
    QCPLayerable::markDirty();
    markDependentsDirty();
}

/*!
  Registers \a dependent, a layerable whose drawing follows this plottable's data, so a content
  change repaints it wherever its own layer is. A null or already registered layerable is ignored.
*/
void QCPAbstractPlottable::addDependent(QCPLayerable* dependent)
{
    if (dependent && !mDependents.contains(dependent))
        mDependents.append(dependent);
}

/*!
  Unregisters \a dependent. A dependent that dies without calling this is forgotten on the next
  content change.
*/
void QCPAbstractPlottable::removeDependent(QCPLayerable* dependent)
{
    mDependents.removeAll(dependent);
}

/*!
  Marks every live dependent dirty. Dependents may depend on each other (two graphs filled to one
  another, ...): a thread-local visit set stops the walk at a plottable already being marked, so a
  cycle marks each object once. A dependent that is not itself a plottable (a tracer, an item) ends
  its own branch.
*/
void QCPAbstractPlottable::markDependentsDirty()
{
    // Nearly every plottable has no dependent, and this runs on every setter: no set work then.
    if (mDependents.isEmpty())
        return;
    static thread_local QSet<const QCPAbstractPlottable*> beingMarked;
    if (beingMarked.contains(this))
        return;
    beingMarked.insert(this);
    struct RemoveOnExit
    {
        QSet<const QCPAbstractPlottable*>& set;
        const QCPAbstractPlottable* self;
        ~RemoveOnExit() { set.remove(self); }
    } removeOnExit { beingMarked, this };

    for (int i = int(mDependents.size()) - 1; i >= 0; --i)
    {
        QCPLayerable* dependent = mDependents.at(i).data();
        if (!dependent)
            mDependents.removeAt(i);
        else
            dependent->markDirty();
    }
}

QCPRange QCPAbstractPlottable::getKeyRange(bool& foundRange, QCP::SignDomain inSignDomain) const
{
    return qcpDrawnRange(mCore.dataRange(*this, view(), false), QCPDrawnSide::Key, foundRange,
                         inSignDomain);
}

QCPRange QCPAbstractPlottable::getValueRange(bool& foundRange, QCP::SignDomain inSignDomain,
                                             const QCPRange& inKeyRange) const
{
    Q_UNUSED(inKeyRange) // simplify: a box keeps no per-key values, so it cannot be narrowed
    return qcpDrawnRange(mCore.dataRange(*this, view(), false), QCPDrawnSide::Value, foundRange,
                         inSignDomain);
}

//! Replays the icon recording, fitted to \a rect, with this plottable's antialiasing: the replay
//! keeps the painter's, which is the legend row's.
void QCPAbstractPlottable::drawLegendIcon(QCPPainter* painter, const QRectF& rect) const
{
    applyDefaultAntialiasingHint(painter);
    mCore.paintIcon(*this, *painter, rect, view());
}

/*!
   The name is the textual representation of this plottable as it is displayed in the legend
   (\ref QCPLegend). It may contain any UTF-8 characters, including newlines.
*/
/*! \internal

  Marks the legend rows that show \a plottable, in every legend the layout contains: the default
  legend sits in an axis rect's inset layout, so the walk descends the whole element tree. Raw, so a
  name change pays no handle per legend row (K8).
*/
void QCPAbstractPlottable::markLegendRows(QCPLayoutElement* element, QCPAbstractPlottable* plottable)
{
    if (!element)
        return;
    if (auto* legend = qobject_cast<QCPLegend*>(element))
    {
        for (int i = 0; i < legend->itemCount(); ++i)
        {
            QCPAbstractLegendItem* item = legend->itemRaw(i);
            auto* row = qobject_cast<QCPPlottableLegendItem*>(item);
            if (!row || row->plottableRaw() != plottable)
                continue;
            item->markDirty();
            legend->markDirty(); // the row may not carry the legend's layer yet
        }
    }
    for (QCPLayoutElement* child : element->elementsRaw(false))
        markLegendRows(child, plottable);
}

void QCPAbstractPlottable::setName(const QString& name)
{
    if (name == mName)
        return;
    if (mParentPlot) // an orphan plottable has no legend to mark, but still has a name
        markLegendRows(mParentPlot->plotLayout(), this);
    mName = name;
}

/*!
  Sets whether fills of this plottable are drawn antialiased or not.

  Note that this setting may be overridden by \ref QCustomPlot::setAntialiasedElements and \ref
  QCustomPlot::setNotAntialiasedElements.
*/
void QCPAbstractPlottable::setAntialiasedFill(bool enabled)
{
    if (mAntialiasedFill == enabled)
        return;
    markDirty();
    mAntialiasedFill = enabled;
}

/*!
  Sets whether the scatter symbols of this plottable are drawn antialiased or not.

  Note that this setting may be overridden by \ref QCustomPlot::setAntialiasedElements and \ref
  QCustomPlot::setNotAntialiasedElements.
*/
void QCPAbstractPlottable::setAntialiasedScatters(bool enabled)
{
    if (mAntialiasedScatters == enabled)
        return;
    markDirty();
    mAntialiasedScatters = enabled;
}

/*!
  The pen is used to draw basic lines that make up the plottable representation in the
  plot.

  For example, the \ref QCPGraph subclass draws its graph lines with this pen.

  \see setBrush
*/
void QCPAbstractPlottable::setPen(const QPen& pen)
{
    if (mPen == pen)
        return;
    markDirty();
    mPen = pen;
}

/*!
  The brush is used to draw basic fills of the plottable representation in the
  plot. The Fill can be a color, gradient or texture, see the usage of QBrush.

  For example, the \ref QCPGraph subclass draws the fill under the graph with this brush, when
  it's not set to Qt::NoBrush.

  \see setPen
*/
void QCPAbstractPlottable::setBrush(const QBrush& brush)
{
    if (mBrush == brush)
        return;
    markDirty();
    mBrush = brush;
}

/*!
  The key axis of a plottable can be set to any axis of a QCustomPlot, as long as it is orthogonal
  to the plottable's value axis. This function performs no checks to make sure this is the case.
  The typical mathematical choice is to use the x-axis (QCustomPlot::xAxis) as key axis and the
  y-axis (QCustomPlot::yAxis) as value axis.

  Normally, the key and value axes are set in the constructor of the plottable (or \ref
  QCustomPlot::addGraph when working with QCPGraphs through the dedicated graph interface).

  \see setValueAxis
*/
void QCPAbstractPlottable::setKeyAxis(QCPAxis* axis)
{
    resolveAxes(axis, valueAxisRaw());
}

void QCPAbstractPlottable::setValueAxis(QCPAxis* axis)
{
    resolveAxes(keyAxisRaw(), axis);
}

//! The 2.x pair resolves to the system of that pair on the axes' area; the pair itself stays the
//! projection a non-cartesian system cannot answer.
void QCPAbstractPlottable::resolveAxes(QCPAxis* key, QCPAxis* value)
{
    QCPCoordinateSystem* resolved = qcpSystemOfAxes(key, value);
    if (resolved == mSystem)
    {
        if (projectsOntoAxes(resolved))
        {
            mKeyAxis = key;
            mValueAxis = value;
        }
        markDirty();
        systemChanged();
        return;
    }
    if (!resolved)
    {
        if (mSystem)
            systemWillChange(nullptr);
        mSystem = nullptr;
        mKeyAxis = key;
        mValueAxis = value;
        markDirty();
        systemChanged();
        return;
    }
    setSystem(resolved);
}

void QCPAbstractPlottable::setSystem(QCPCoordinateSystem* system)
{
    if (system == mSystem)
        return;
    if (system)
    {
        QCP_CHECK_OR_RETURN(system->area() && system->area()->parentPlot() == mParentPlot,
                            "the coordinate system belongs to another plot");
        // One place decides what a system can host: the constructor and every setSystem agree.
        if (!systemAdmits(system, requiredCapabilities()))
            return;
    }
    systemWillChange(system);
    mSystem = system;
    // The area it draws in is the one it hangs on: hiding the area it left must not hide it, and
    // an onlyVisible rescale of the new area must find it.
    setParentLayerable(system ? system->area() : nullptr);
    mKeyAxis = system && projectsOntoAxes(system) ? qobject_cast<QCPAxis*>(system->keyAxis())
                                                  : nullptr;
    mValueAxis = system && projectsOntoAxes(system) ? qobject_cast<QCPAxis*>(system->valueAxis())
                                                    : nullptr;
    // A plottable told to drop its system is inert on purpose: it draws nothing and says nothing.
    // A system that goes away under it is reported once, on the next draw, by dropSystem() below.
    mReportedLostAxis = mSystem == nullptr;
    // A plottable refused at construction registered nowhere. A later system that accepts it joins
    // the plot through the one path adoption uses: lists, legend row, graph list, colour scale.
    if (mSystem && !mRegisteredPlot && mParentPlot)
        mParentPlot->registerJoinedPlottable(this);
    contentChanged(); // the new frame's recording, then whatever follows an axis pair
    systemChanged();
}

/*!
  Sets which data ranges of this plottable are selected. Selected data ranges are drawn differently
  (e.g. color) in the plot. This can be controlled via the selection decorator (see \ref
  selectionDecorator).

  The entire selection mechanism for plottables is handled automatically when \ref
  QCustomPlot::setInteractions contains iSelectPlottables. You only need to call this function when
  you wish to change the selection state programmatically.

  Using \ref setSelectable you can further specify for each plottable whether and to which
  granularity it is selectable. If \a selection is not compatible with the current \ref
  QCP::SelectionType set via \ref setSelectable, the resulting selection will be adjusted
  accordingly (see \ref QCPDataSelection::enforceType).

  emits the \ref selectionChanged signal when \a selected is different from the previous selection
  state.

  \see setSelectable, selectTest
*/
void QCPAbstractPlottable::setSelection(QCPDataSelection selection)
{
    selection.enforceType(mSelectable);
    if (assignSelection(selection))
    {
        markDirty(); // the selection decides what a plottable draws: the next frame must not reuse the last one
        emitSelectionChanged();
    }
}

bool QCPAbstractPlottable::assignSelection(const QCPDataSelection& selection)
{
    if (mSelection == selection)
        return false;
    mSelection = selection;
    return true;
}

/*! \internal

  Emits \ref selectionChanged, at the outermost call: a slot may delete this plottable, and the
  selection is read with \ref selection when the signal is delivered.
*/
void QCPAbstractPlottable::emitSelectionChanged()
{
    qcpEmit(this, mParentPlot, &QCPAbstractPlottable::selectionChanged);
}

/*!
  Use this method to set an own QCPSelectionDecorator (subclass) instance. This allows you to
  customize the visual representation of selected data ranges further than by using the default
  QCPSelectionDecorator.

  The plottable takes ownership of the \a decorator.

  The currently set decorator can be accessed via \ref selectionDecorator.
*/
void QCPAbstractPlottable::setSelectionDecorator(QCPSelectionDecorator* decorator)
{
    if (decorator)
    {
        if (decorator->registerWithPlottable(this))
        {
            markDirty(); // the recording baked the old decorator's look, or a callback to it
            delete mSelectionDecorator; // delete old decorator if necessary
            mSelectionDecorator = decorator;
        }
    }
    else if (mSelectionDecorator) // just clear decorator
    {
        markDirty(); // a recording that still held the decorator's callback would run it
        delete mSelectionDecorator;
        mSelectionDecorator = nullptr;
    }
}

/*!
  Sets whether and to which granularity this plottable can be selected.

  A selection can happen by clicking on the QCustomPlot surface (When \ref
  QCustomPlot::setInteractions contains \ref QCP::iSelectPlottables), by dragging a selection rect
  (When \ref QCustomPlot::setSelectionRectMode is \ref QCP::srmSelect), or programmatically by
  calling \ref setSelection.

  \see setSelection, QCP::SelectionType
*/
void QCPAbstractPlottable::setSelectable(QCP::SelectionType selectable)
{
    if (mSelectable != selectable)
    {
        mSelectable = selectable;
        QCPDataSelection trimmed = mSelection;
        trimmed.enforceType(mSelectable);
        // Through the hook: a subclass that spreads its selection (a graph's components) trims too.
        const bool selectionTrimmed = trimmed != mSelection && assignSelection(trimmed);
        if (selectionTrimmed)
            markDirty();
        const QCPEmissionScope scope(mParentPlot);
        qcpEmit(this, mParentPlot, &QCPAbstractPlottable::selectableChanged);
        if (selectionTrimmed)
            emitSelectionChanged();
    }
}

/*!
  Convenience function for transforming a key/value pair to pixels on the QCustomPlot surface,
  taking the orientations of the axes associated with this plottable into account (e.g. whether key
  represents x or y).

  \a key and \a value are transformed to the coodinates in pixels and are written to \a x and \a y.

  \see pixelsToCoords, QCPAxis::coordToPixel
*/
void QCPAbstractPlottable::coordsToPixels(double key, double value, double& x, double& y) const
{
    QCPAxis* keyAxis = keyAxisRaw();
    QCPAxis* valueAxis = valueAxisRaw();
    if (!keyAxis || !valueAxis)
    {
        if (!mSystem)
        {
            return;
        }
        const QCP::Pixels pixel = mSystem->transform().map(QCP::Data { key, value });
        x = pixel.x;
        y = pixel.y;
        return;
    }

    if (keyAxis->orientation() == Qt::Horizontal)
    {
        x = keyAxis->coordToPixel(key);
        y = valueAxis->coordToPixel(value);
    }
    else
    {
        y = keyAxis->coordToPixel(key);
        x = valueAxis->coordToPixel(value);
    }
}

/*! \overload

  Transforms the given \a key and \a value to pixel coordinates and returns them in a QPointF.
*/
const QPointF QCPAbstractPlottable::coordsToPixels(double key, double value) const
{
    QCPAxis* keyAxis = keyAxisRaw();
    QCPAxis* valueAxis = valueAxisRaw();
    if (!keyAxis || !valueAxis)
    {
        if (!mSystem)
        {
            return QPointF();
        }
        const QCP::Pixels pixel = mSystem->transform().map(QCP::Data { key, value });
        return QPointF(pixel.x, pixel.y);
    }

    if (keyAxis->orientation() == Qt::Horizontal)
        return QPointF(keyAxis->coordToPixel(key), valueAxis->coordToPixel(value));
    else
        return QPointF(valueAxis->coordToPixel(value), keyAxis->coordToPixel(key));
}

/*!
  Convenience function for transforming a x/y pixel pair on the QCustomPlot surface to plot
  coordinates, taking the orientations of the axes associated with this plottable into account (e.g.
  whether key represents x or y).

  \a x and \a y are transformed to the plot coodinates and are written to \a key and \a value.

  \see coordsToPixels, QCPAxis::coordToPixel
*/
void QCPAbstractPlottable::pixelsToCoords(double x, double y, double& key, double& value) const
{
    QCPAxis* keyAxis = keyAxisRaw();
    QCPAxis* valueAxis = valueAxisRaw();
    if (!keyAxis || !valueAxis)
    {
        if (!mSystem)
        {
            return;
        }
        const QCP::Data data = mSystem->transform().inverse(QPointF(x, y));
        key = data.x;
        value = data.y;
        return;
    }

    if (keyAxis->orientation() == Qt::Horizontal)
    {
        key = keyAxis->pixelToCoord(x);
        value = valueAxis->pixelToCoord(y);
    }
    else
    {
        key = keyAxis->pixelToCoord(y);
        value = valueAxis->pixelToCoord(x);
    }
}

/*! \overload

  Returns the pixel input \a pixelPos as plot coordinates \a key and \a value.
*/
void QCPAbstractPlottable::pixelsToCoords(const QPointF& pixelPos, double& key, double& value) const
{
    pixelsToCoords(pixelPos.x(), pixelPos.y(), key, value);
}

/*!
  Rescales the key and value axes associated with this plottable to contain all displayed data, so
  the whole plottable is visible. If the scaling of an axis is logarithmic, rescaleAxes will make
  sure not to rescale to an illegal range i.e. a range containing different signs and/or zero.
  Instead it will stay in the current sign domain and ignore all parts of the plottable that lie
  outside of that domain.

  \a onlyEnlarge makes sure the ranges are only expanded, never reduced. So it's possible to show
  multiple plottables in their entirety by multiple calls to rescaleAxes where the first call has
  \a onlyEnlarge set to false (the default), and all subsequent set to true.

  \see rescaleKeyAxis, rescaleValueAxis, QCustomPlot::rescaleAxes, QCPAxis::rescale
*/
void QCPAbstractPlottable::rescaleAxes(bool onlyEnlarge) const
{
    rescaleKeyAxis(onlyEnlarge);
    rescaleValueAxis(onlyEnlarge);
}

/*!
  Rescales the key axis of the plottable so the whole plottable is visible.

  See \ref rescaleAxes for detailed behaviour.
*/
void QCPAbstractPlottable::rescaleKeyAxis(bool onlyEnlarge) const
{
    // The system's own axis, whatever its kind: a user system on a QCPAxis rect has an ordinary
    // axis to fit. A polar system's axis is not a QCPAxis, so the cast answers null and the area
    // keeps rescaling the whole frame, as before.
    QCPAxis* keyAxis = qobject_cast<QCPAxis*>(keyAxisBase());
    if (!keyAxis)
        return;

    QCP::SignDomain signDomain = QCP::sdBoth;
    if (keyAxis->scaleType() == QCPAxis::stLogarithmic)
        signDomain = (keyAxis->range().upper < 0 ? QCP::sdNegative : QCP::sdPositive);

    bool foundRange;
    QCPRange newRange = getKeyRange(foundRange, signDomain);
    if (foundRange)
    {
        if (onlyEnlarge)
            newRange.expand(keyAxis->range());
        if (!QCPRange::validRange(newRange)) // likely due to range being zero (plottable has only
                                             // constant data in this axis dimension), shift current
                                             // range to at least center the plottable
        {
            double center = (newRange.lower + newRange.upper)
                * 0.5; // upper and lower should be equal anyway, but just to make sure, incase
                       // validRange returned false for other reason
            if (keyAxis->scaleType() == QCPAxis::stLinear)
            {
                newRange.lower = center - keyAxis->range().size() / 2.0;
                newRange.upper = center + keyAxis->range().size() / 2.0;
            }
            else // scaleType() == stLogarithmic
            {
                newRange.lower = center / qSqrt(keyAxis->range().upper / keyAxis->range().lower);
                newRange.upper = center * qSqrt(keyAxis->range().upper / keyAxis->range().lower);
            }
        }
        keyAxis->setRange(newRange);
    }
}

/*!
  Rescales the value axis of the plottable so the whole plottable is visible. If \a inKeyRange is
  set to true, only the data points which are in the currently visible key axis range are
  considered.

  Returns true if the axis was actually scaled. This might not be the case if this plottable has an
  invalid range, e.g. because it has no data points.

  See \ref rescaleAxes for detailed behaviour.
*/
void QCPAbstractPlottable::rescaleValueAxis(bool onlyEnlarge, bool inKeyRange) const
{
    // Same side as rescaleKeyAxis: the system's own axes when they are ordinary QCPAxis objects,
    // null and silent on a polar frame.
    QCPAxis* keyAxis = qobject_cast<QCPAxis*>(keyAxisBase());
    QCPAxis* valueAxis = qobject_cast<QCPAxis*>(valueAxisBase());
    if (!valueAxis)
        return;

    QCP::SignDomain signDomain = QCP::sdBoth;
    if (valueAxis->scaleType() == QCPAxis::stLogarithmic)
        signDomain = (valueAxis->range().upper < 0 ? QCP::sdNegative : QCP::sdPositive);

    bool foundRange;
    QCPRange newRange
        = getValueRange(foundRange, signDomain,
                        inKeyRange && keyAxis ? keyAxis->range() : QCPRange());
    if (foundRange)
    {
        if (onlyEnlarge)
            newRange.expand(valueAxis->range());
        if (!QCPRange::validRange(newRange)) // likely due to range being zero (plottable has only
                                             // constant data in this axis dimension), shift current
                                             // range to at least center the plottable
        {
            double center = (newRange.lower + newRange.upper)
                * 0.5; // upper and lower should be equal anyway, but just to make sure, incase
                       // validRange returned false for other reason
            if (valueAxis->scaleType() == QCPAxis::stLinear)
            {
                newRange.lower = center - valueAxis->range().size() / 2.0;
                newRange.upper = center + valueAxis->range().size() / 2.0;
            }
            else // scaleType() == stLogarithmic
            {
                newRange.lower
                    = center / qSqrt(valueAxis->range().upper / valueAxis->range().lower);
                newRange.upper
                    = center * qSqrt(valueAxis->range().upper / valueAxis->range().lower);
            }
        }
        valueAxis->setRange(newRange);
    }
}

/*! \overload

  Adds this plottable to the specified \a legend.

  Creates a QCPPlottableLegendItem which is inserted into the legend. Returns true on success, i.e.
  when the legend exists and a legend item associated with this plottable isn't already in the
  legend.

  If the plottable needs a more specialized representation in the legend, you can create a
  corresponding subclass of \ref QCPPlottableLegendItem and add it to the legend manually instead
  of calling this method.

  \see removeFromLegend, QCPLegend::addItem
*/
bool QCPAbstractPlottable::addToLegend(QCPLegend* legend)
{
    if (!legendAllowed())
        return false;
    if (!legend)
    {
        qDebug() << Q_FUNC_INFO << "passed legend is null";
        return false;
    }
    if (legend->parentPlot() != mParentPlot)
    {
        qDebug() << Q_FUNC_INFO << "passed legend isn't in the same QCustomPlot as this plottable";
        return false;
    }

    if (!legend->hasItemWithPlottable(this))
    {
        legend->addItem(new QCPPlottableLegendItem(legend, this));
        return true;
    }
    else
        return false;
}

/*!
  Sets whether this plottable wants a legend row. False removes its row and keeps it out of the
  legend, also when it leaves a \ref QCPPlottableGroup. True adds the row back, unless the
  plottable is grouped (its group's row stands for it).
*/
void QCPAbstractPlottable::setWantsLegend(bool wants)
{
    if (mWantsLegend == wants)
        return;
    markDirty();
    mWantsLegend = wants;
    if (mGroup)
        mRowOnLeave = wants;
    else if (wants)
        addToLegend();
    else
        removeFromLegend();
}

/*! \overload

  Adds this plottable to the legend of the parent QCustomPlot (\ref QCustomPlot::legend).

  \see removeFromLegend
*/
bool QCPAbstractPlottable::addToLegend()
{
    if (!mParentPlot || !mParentPlot->legend)
        return false;
    else
        return addToLegend(mParentPlot->legend);
}

/*! \overload

  Removes the plottable from the specifed \a legend. This means the \ref QCPPlottableLegendItem
  that is associated with this plottable is removed.

  Returns true on success, i.e. if the legend exists and a legend item associated with this
  plottable was found and removed.

  \see addToLegend, QCPLegend::removeItem
*/
bool QCPAbstractPlottable::removeFromLegend(QCPLegend* legend) const
{
    if (!legend)
    {
        qDebug() << Q_FUNC_INFO << "passed legend is null";
        return false;
    }

    if (QCPPlottableLegendItem* lip = legend->itemWithPlottable(this))
        return legend->removeItem(lip);
    else
        return false;
}

/*! \overload

  Removes the plottable from the legend of the parent QCustomPlot.

  \see addToLegend
*/
bool QCPAbstractPlottable::removeFromLegend() const
{
    if (!mParentPlot || !mParentPlot->legend)
        return false;
    else
        return removeFromLegend(mParentPlot->legend);
}

/*! \internal
  The system under this plottable went away: its area or one of its axes was deleted. The plottable
  drops it silently like setSystem(nullptr), but re-arms the one report the next draw gives, so the
  user learns why it stopped being drawn.
*/
void QCPAbstractPlottable::dropSystem()
{
    if (mSystem)
        setSystem(nullptr);
    mReportedLostAxis = false;
}

bool QCPAbstractPlottable::drawableAxes() const
{
    if (mSystem)
    {
        mReportedLostAxis = false;
        return true;
    }
    if (!mReportedLostAxis)
    {
        mReportedLostAxis = true;
        qcp::reportMisuse(Q_FUNC_INFO, "it has no coordinate system: it is not drawn");
    }
    return false;
}

/* inherits documentation from base class */
QRect QCPAbstractPlottable::clipRect() const
{
    return mSystem ? mSystem->clipRect() : QRect();
}

/* inherits documentation from base class */
QCP::Interaction QCPAbstractPlottable::selectionCategory() const
{
    return QCP::iSelectPlottables;
}

/*! \internal

  A convenience function to easily set the QPainter::Antialiased hint on the provided \a painter
  before drawing plottable lines.

  This is the antialiasing state the painter passed to the \ref draw method is in by default.

  This function takes into account the local setting of the antialiasing flag as well as the
  overrides set with \ref QCustomPlot::setAntialiasedElements and \ref
  QCustomPlot::setNotAntialiasedElements.

  \seebaseclassmethod

  \see setAntialiased
*/
void QCPAbstractPlottable::applyDefaultAntialiasingHint(QCPPainter* painter) const
{
    applyAntialiasingHint(painter, mAntialiased, QCP::aePlottables);
}

/* inherits documentation from base class */
void QCPAbstractPlottable::selectEvent([[maybe_unused]] QMouseEvent* event, bool additive,
                                       const QVariant& details, bool* selectionStateChanged)
{

    if (mSelectable != QCP::stNone)
    {
        QCPDataSelection newSelection = details.value<QCPDataSelection>();
        QCPDataSelection selectionBefore = mSelection;
        const QPointer<QCPAbstractPlottable> alive(this);
        if (additive)
        {
            if (mSelectable == QCP::stWhole) // in whole selection mode, we toggle to no selection
                                             // even if currently unselected point was hit
            {
                if (selected())
                    setSelection(QCPDataSelection());
                else
                    setSelection(newSelection);
            }
            else // in all other selection modes we toggle selections of homogeneously
                 // selected/unselected segments
            {
                if (mSelection.contains(newSelection)) // if entire newSelection is already
                                                       // selected, toggle selection
                    setSelection(mSelection - newSelection);
                else
                    setSelection(mSelection + newSelection);
            }
        }
        else
            setSelection(newSelection);
        // setSelection only emits on a change, so a plottable a slot deleted did change.
        if (selectionStateChanged)
            *selectionStateChanged = !alive || mSelection != selectionBefore;
    }
}

/* inherits documentation from base class */
void QCPAbstractPlottable::deselectEvent(bool* selectionStateChanged)
{
    if (mSelectable != QCP::stNone)
    {
        QCPDataSelection selectionBefore = mSelection;
        const QPointer<QCPAbstractPlottable> alive(this);
        setSelection(QCPDataSelection());
        if (selectionStateChanged)
            *selectionStateChanged = !alive || mSelection != selectionBefore;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Busy indicator
////////////////////////////////////////////////////////////////////////////////////////////////////

bool QCPAbstractPlottable::busy() const
{
    return mEffectiveBusy;
}

void QCPAbstractPlottable::setBusy(bool busy)
{
    if (mExternalBusy != busy)
    {
        mExternalBusy = busy;
        updateEffectiveBusy();
    }
}

void QCPAbstractPlottable::updateEffectiveBusy()
{
    const bool newBusy = mExternalBusy || pipelineBusy() || hasPendingData();
    if (newBusy == mEffectiveBusy)
        return;

    mEffectiveBusy = newBusy;
    mBusyDebounceTimer.stop();
    const int delay = mEffectiveBusy ? effectiveBusyShowDelayMs() : effectiveBusyHideDelayMs();
    mBusyDebounceTimer.start(delay);
    // Last: a slot may delete this plottable.
    qcpEmit(this, mParentPlot, &QCPAbstractPlottable::busyChanged, mEffectiveBusy);
}

void QCPAbstractPlottable::onDebounceTimeout()
{
    if (mVisuallyBusy == mEffectiveBusy)
        return;

    mVisuallyBusy = mEffectiveBusy;
    // The GPU entries carry the fade alpha; a translating layer would keep the
    // stale ones, so force one real repaint of this layer.
    if (mLayer)
        mLayer->invalidatePaintBuffer(); // invalidating schedules the repaint
    // Last: a slot may delete this plottable.
    qcpEmit(this, mParentPlot, &QCPAbstractPlottable::visuallyBusyChanged, mVisuallyBusy);
}

// per-plottable overrides

void QCPAbstractPlottable::setBusyIndicatorSymbol(const QString& symbol)
{
    mBusyIndicatorSymbol = symbol;
}

void QCPAbstractPlottable::resetBusyIndicatorSymbol()
{
    mBusyIndicatorSymbol.reset();
}

void QCPAbstractPlottable::setBusyFadeAlpha(qreal alpha)
{
    mBusyFadeAlpha = alpha;
}

void QCPAbstractPlottable::resetBusyFadeAlpha()
{
    mBusyFadeAlpha.reset();
}

void QCPAbstractPlottable::setBusyShowDelayMs(int ms)
{
    mBusyShowDelayMs = ms;
}

void QCPAbstractPlottable::resetBusyShowDelayMs()
{
    mBusyShowDelayMs.reset();
}

void QCPAbstractPlottable::setBusyHideDelayMs(int ms)
{
    mBusyHideDelayMs = ms;
}

void QCPAbstractPlottable::resetBusyHideDelayMs()
{
    mBusyHideDelayMs.reset();
}

// effective values: per-plottable override, then theme, then hardcoded fallback

QString QCPAbstractPlottable::effectiveBusyIndicatorSymbol() const
{
    if (mBusyIndicatorSymbol)
        return *mBusyIndicatorSymbol;
    if (mParentPlot && mParentPlot->theme())
        return mParentPlot->theme()->busyIndicatorSymbol();
    return QStringLiteral("\u27F3");
}

qreal QCPAbstractPlottable::effectiveBusyFadeAlpha() const
{
    if (mBusyFadeAlpha)
        return *mBusyFadeAlpha;
    if (mParentPlot && mParentPlot->theme())
        return mParentPlot->theme()->busyFadeAlpha();
    return 0.3;
}

int QCPAbstractPlottable::effectiveBusyShowDelayMs() const
{
    if (mBusyShowDelayMs)
        return *mBusyShowDelayMs;
    if (mParentPlot && mParentPlot->theme())
        return mParentPlot->theme()->busyShowDelayMs();
    return 500;
}

int QCPAbstractPlottable::effectiveBusyHideDelayMs() const
{
    if (mBusyHideDelayMs)
        return *mBusyHideDelayMs;
    if (mParentPlot && mParentPlot->theme())
        return mParentPlot->theme()->busyHideDelayMs();
    return 500;
}

namespace qcp::ops
{
void setLayer(QCPLayerable& layerable, const QString& layerName)
{
    QCustomPlot* plot = layerable.parentPlot();
    QCP_CHECK_OR_RETURN(plot && plot->layer(layerName), "no layer of that name in the plot");
    layerable.setLayer(layerName);
}

void setAxes(QCPAbstractPlottable& plottable, QCPAxis* keyAxis, QCPAxis* valueAxis)
{
    QCP_CHECK_OR_RETURN(keyAxis && valueAxis, "a plottable needs a key axis and a value axis");
    QCP_CHECK_OR_RETURN(keyAxis->parentPlot() == plottable.parentPlot()
                            && valueAxis->parentPlot() == plottable.parentPlot(),
                        "the axes belong to another plot");
    QCP_CHECK_OR_RETURN(keyAxis->orientation() != valueAxis->orientation(),
                        "the key and value axes must be orthogonal");
    plottable.setKeyAxis(keyAxis);
    plottable.setValueAxis(valueAxis);
}

namespace
{
// axis(type) would log on a missing type; a refusal here is already reported as misuse.
QCPAxis* firstAxis(const QCPAxisRect* rect, QCPAxis::AxisType type)
{
    const QList<QCPHandle<QCPAxis>> axes = rect->axes(type);
    return axes.isEmpty() ? nullptr : axes.first().get();
}

void setAxesOfRect(QCPAbstractPlottable& plottable, QCPAxisRect* rect, QCPAxis::AxisType keyType,
                   QCPAxis::AxisType valueType)
{
    QCPAxis* keyAxis = firstAxis(rect, keyType);
    QCPAxis* valueAxis = firstAxis(rect, valueType);
    QCP_CHECK_OR_RETURN(keyAxis && valueAxis, "the axis rect has no axis of that type");
    setAxes(plottable, keyAxis, valueAxis);
}
} // namespace

void setAxes(QCPAbstractPlottable& plottable, QCPAxis::AxisType keyType,
             QCPAxis::AxisType valueType)
{
    QCPAxis* current = plottable.keyAxis();
    QCP_CHECK_OR_RETURN(current, "the plottable has lost its key axis, so it has no axis rect");
    setAxesOfRect(plottable, static_cast<QCPAxisRect*>(current->area()), keyType,
                  valueType);
}

void setAxisRect(QCPAbstractPlottable& plottable, QCPAxisRect* rect)
{
    QCP_CHECK_OR_RETURN(rect && rect->parentPlot() == plottable.parentPlot(),
                        "the axis rect belongs to another plot");
    setAxesOfRect(plottable, rect, QCPAxis::atBottom, QCPAxis::atLeft);
}
} // namespace qcp::ops
