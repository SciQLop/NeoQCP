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
#ifndef QCP_GLOBAL_H
#define QCP_GLOBAL_H

#include "neoqcp_config.h"

// amalgamation: include begin
#include <QtCore/qglobal.h>

// some Qt version/configuration dependent macros to include or exclude certain code paths:

#include <QtCore/QCache>
#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QFlags>
#include <QtCore/QMargins>
#include <QtCore/QMultiMap>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QSharedPointer>
#include <QtCore/QStack>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtCore/QVector>
#include <QtGui/QMouseEvent>
#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPixmap>
#include <QtGui/QWheelEvent>
#include <algorithm>
#include <limits>
#include <qmath.h>

#ifdef QCP_OPENGL_FBO
#include <QOpenGLFramebufferObject>
#include <QOpenGLPaintDevice>
#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLContext>
#endif

#include <QtNumeric>
#include <QtPrintSupport/QtPrintSupport>
#include <QtWidgets/QWidget>

#include <QtCore/QElapsedTimer>
#include <QtCore/QTimeZone>

// amalgamation: include end

// decl definitions for shared library compilation/usage:
#if defined(QT_STATIC_BUILD)
#define QCP_LIB_DECL
#elif defined(QCUSTOMPLOT_COMPILE_LIBRARY)
#define QCP_LIB_DECL Q_DECL_EXPORT
#elif defined(QCUSTOMPLOT_USE_LIBRARY)
#define QCP_LIB_DECL Q_DECL_IMPORT
#else
#define QCP_LIB_DECL
#endif

// define empty macro for Q_DECL_OVERRIDE if it doesn't exist (Qt < 5)
#ifndef Q_DECL_OVERRIDE
#define Q_DECL_OVERRIDE
#endif

/*!
  The QCP Namespace contains general enums, QFlags and functions used throughout the QCustomPlot
  library.

  It provides QMetaObject-based reflection of its enums and flags via \a QCP::staticMetaObject.
*/

// Qt version < 6.2.0: to get metatypes Q_GADGET/Q_ENUMS/Q_FLAGS in namespace we have to make it
// look like a class during moc-run
namespace QCP
{
Q_NAMESPACE // this is how to add the staticMetaObject to namespaces in newer Qt versions


    /*!
      Defines the different units in which the image resolution can be specified in the export
      functions.

      \see QCustomPlot::savePng, QCustomPlot::saveJpg, QCustomPlot::saveBmp,
      QCustomPlot::saveRastered
    */
    enum ResolutionUnit {
        ruDotsPerMeter ///< Resolution is given in dots per meter (dpm)
        ,
        ruDotsPerCentimeter ///< Resolution is given in dots per centimeter (dpcm)
        ,
        ruDotsPerInch ///< Resolution is given in dots per inch (DPI/PPI)
    };

/*!
  Defines how cosmetic pens (pens with numerical width 0) are handled during export.

  \see QCustomPlot::savePdf
*/
enum ExportPen
{
    epNoCosmetic ///< Cosmetic pens are converted to pens with pixel width 1 when exporting
    ,
    epAllowCosmetic ///< Cosmetic pens are exported normally (e.g. in PDF exports, cosmetic pens
                    ///< always appear as 1 pixel on screen, independent of viewer zoom level)
};

/*!
  Represents negative and positive sign domain, e.g. for passing to \ref
  QCPAbstractPlottable::getKeyRange and \ref QCPAbstractPlottable::getValueRange.

  This is primarily needed when working with logarithmic axis scales, since only one of the sign
  domains can be visible at a time.
*/
enum SignDomain
{
    sdNegative ///< The negative sign domain, i.e. numbers smaller than zero
    ,
    sdBoth ///< Both sign domains, including zero, i.e. all numbers
    ,
    sdPositive ///< The positive sign domain, i.e. numbers greater than zero
};

/*!
  Defines the sides of a rectangular entity to which margins can be applied.

  \see QCPLayoutElement::setAutoMargins, QCPAxisRect::setAutoMargins
*/
enum MarginSide
{
    msLeft = 0x01 ///< <tt>0x01</tt> left margin
    ,
    msRight = 0x02 ///< <tt>0x02</tt> right margin
    ,
    msTop = 0x04 ///< <tt>0x04</tt> top margin
    ,
    msBottom = 0x08 ///< <tt>0x08</tt> bottom margin
    ,
    msAll = 0xFF ///< <tt>0xFF</tt> all margins
    ,
    msNone = 0x00 ///< <tt>0x00</tt> no margin
};
Q_DECLARE_FLAGS(MarginSides, MarginSide)

/*!
  Defines what objects of a plot can be forcibly drawn antialiased/not antialiased. If an object is
  neither forcibly drawn antialiased nor forcibly drawn not antialiased, it is up to the respective
  element how it is drawn. Typically it provides a \a setAntialiased function for this.

  \c AntialiasedElements is a flag of or-combined elements of this enum type.

  \see QCustomPlot::setAntialiasedElements, QCustomPlot::setNotAntialiasedElements
*/
enum AntialiasedElement
{
    aeAxes = 0x0001 ///< <tt>0x0001</tt> Axis base line and tick marks
    ,
    aeGrid = 0x0002 ///< <tt>0x0002</tt> Grid lines
    ,
    aeSubGrid = 0x0004 ///< <tt>0x0004</tt> Sub grid lines
    ,
    aeLegend = 0x0008 ///< <tt>0x0008</tt> Legend box
    ,
    aeLegendItems = 0x0010 ///< <tt>0x0010</tt> Legend items
    ,
    aePlottables = 0x0020 ///< <tt>0x0020</tt> Main lines of plottables
    ,
    aeItems = 0x0040 ///< <tt>0x0040</tt> Main lines of items
    ,
    aeScatters = 0x0080 ///< <tt>0x0080</tt> Scatter symbols of plottables (excluding scatter
                        ///< symbols of type ssPixmap)
    ,
    aeFills = 0x0100 ///< <tt>0x0100</tt> Borders of fills (e.g. under or between graphs)
    ,
    aeZeroLine = 0x0200 ///< <tt>0x0200</tt> Zero-lines, see \ref QCPGrid::setZeroLinePen
    ,
    aeOther
    = 0x8000 ///< <tt>0x8000</tt> Other elements that don't fit into any of the existing categories
    ,
    aeAll = 0xFFFF ///< <tt>0xFFFF</tt> All elements
    ,
    aeNone = 0x0000 ///< <tt>0x0000</tt> No elements
};
Q_DECLARE_FLAGS(AntialiasedElements, AntialiasedElement)

/*!
  Defines plotting hints that control various aspects of the quality and speed of plotting.

  \see QCustomPlot::setPlottingHints
*/
enum PlottingHint
{
    phNone = 0x000 ///< <tt>0x000</tt> No hints are set
    ,
    phFastPolylines = 0x001 ///< <tt>0x001</tt> Graph/Curve lines are drawn with a faster method.
                            ///< This reduces the quality especially of the line segment
                            ///<                joins, thus is most effective for pen sizes larger
                            ///<                than 1. It is only used for solid line pens.
    ,
    phImmediateRefresh
    = 0x002 ///< <tt>0x002</tt> causes an immediate repaint() instead of a soft update() when
            ///< QCustomPlot::replot() is called with parameter \ref QCustomPlot::rpRefreshHint.
            ///<                This is set by default to prevent the plot from freezing on fast
            ///<                consecutive replots (e.g. user drags ranges with mouse).
    ,
    phCacheLabels = 0x004 ///< <tt>0x004</tt> axis (tick) labels will be cached as pixmaps,
                          ///< increasing replot performance.
};
Q_DECLARE_FLAGS(PlottingHints, PlottingHint)

/*!
  Defines the mouse interactions possible with QCustomPlot.

  \c Interactions is a flag of or-combined elements of this enum type.

  \see QCustomPlot::setInteractions
*/
enum Interaction
{
    iNone = 0x000 ///< <tt>0x000</tt> None of the interactions are possible
    ,
    iRangeDrag = 0x001 ///< <tt>0x001</tt> Axis ranges are draggable (see \ref
                       ///< QCPAxisRect::setRangeDrag, \ref QCPAxisRect::setRangeDragAxes)
    ,
    iRangeZoom = 0x002 ///< <tt>0x002</tt> Axis ranges are zoomable with the mouse wheel (see \ref
                       ///< QCPAxisRect::setRangeZoom, \ref QCPAxisRect::setRangeZoomAxes)
    ,
    iMultiSelect
    = 0x004 ///< <tt>0x004</tt> The user can select multiple objects by holding the modifier set by
            ///< \ref QCustomPlot::setMultiSelectModifier while clicking
    ,
    iSelectPlottables = 0x008 ///< <tt>0x008</tt> Plottables are selectable (e.g. graphs, curves,
                              ///< bars,... see QCPAbstractPlottable)
    ,
    iSelectAxes = 0x010 ///< <tt>0x010</tt> Axes are selectable (or parts of them, see
                        ///< QCPAxis::setSelectableParts)
    ,
    iSelectLegend = 0x020 ///< <tt>0x020</tt> Legends are selectable (or their child items, see
                          ///< QCPLegend::setSelectableParts)
    ,
    iSelectItems = 0x040 ///< <tt>0x040</tt> Items are selectable (Rectangles, Arrows, Textitems,
                         ///< etc. see \ref QCPAbstractItem)
    ,
    iSelectOther = 0x080 ///< <tt>0x080</tt> All other objects are selectable (e.g. your own derived
                         ///< layerables, other layout elements,...)
    ,
    iSelectPlottablesBeyondAxisRect
    = 0x100 ///< <tt>0x100</tt> When performing plottable selection/hit tests, this flag extends the
            ///< sensitive area beyond the axis rect
};
Q_DECLARE_FLAGS(Interactions, Interaction)

/*!
  Defines the behaviour of the selection rect.

  \see QCustomPlot::setSelectionRectMode, QCustomPlot::selectionRect, QCPSelectionRect
*/
enum SelectionRectMode
{
    srmNone ///< The selection rect is disabled, and all mouse events are forwarded to the
            ///< underlying objects, e.g. for axis range dragging
    ,
    srmZoom ///< When dragging the mouse, a selection rect becomes active. Upon releasing, the axes
            ///< that are currently set as range zoom axes (\ref QCPAxisRect::setRangeZoomAxes) will
            ///< have their ranges zoomed accordingly.
    ,
    srmSelect ///< When dragging the mouse, a selection rect becomes active. Upon releasing,
              ///< plottable data points that were within the selection rect are selected, if the
              ///< plottable's selectability setting permits. (See  \ref dataselection "data
              ///< selection mechanism" for details.)
    ,
    srmCustom ///< When dragging the mouse, a selection rect becomes active. It is the programmer's
              ///< responsibility to connect according slots to the selection rect's signals (e.g.
              ///< \ref QCPSelectionRect::accepted) in order to process the user interaction.
};

/*!
  Defines the different ways a plottable can be selected. These images show the effect of the
  different selection types, when the indicated selection rect was dragged:

  <center>
  <table>
  <tr>
    <td>\image html selectiontype-none.png stNone</td>
    <td>\image html selectiontype-whole.png stWhole</td>
    <td>\image html selectiontype-singledata.png stSingleData</td>
    <td>\image html selectiontype-datarange.png stDataRange</td>
    <td>\image html selectiontype-multipledataranges.png stMultipleDataRanges</td>
  </tr>
  </table>
  </center>

  \see QCPAbstractPlottable::setSelectable, QCPDataSelection::enforceType
*/
enum SelectionType
{
    stNone ///< The plottable is not selectable
    ,
    stWhole ///< Selection behaves like \ref stMultipleDataRanges, but if there are any data points
            ///< selected, the entire plottable is drawn as selected.
    ,
    stSingleData ///< One individual data point can be selected at a time
    ,
    stDataRange ///< Multiple contiguous data points (a data range) can be selected
    ,
    stMultipleDataRanges ///< Any combination of data points/ranges can be selected
};

/*! \internal

  Returns whether the specified \a value is considered an invalid data value for plottables (i.e.
  is \e nan or \e +/-inf). This function is used to check data validity upon replots, when the
  compiler flag \c QCUSTOMPLOT_CHECK_DATA is set.
*/
inline bool isInvalidData(double value)
{
    return qIsNaN(value) || qIsInf(value);
}

/*! \internal
  \overload

  Checks two arguments instead of one.
*/
inline bool isInvalidData(double value1, double value2)
{
    return isInvalidData(value1) || isInvalidData(value2);
}

/*! \internal

  Sets the specified \a side of \a margins to \a value

  \see getMarginValue
*/
inline void setMarginValue(QMargins& margins, QCP::MarginSide side, int value)
{
    switch (side)
    {
        case QCP::msLeft:
            margins.setLeft(value);
            break;
        case QCP::msRight:
            margins.setRight(value);
            break;
        case QCP::msTop:
            margins.setTop(value);
            break;
        case QCP::msBottom:
            margins.setBottom(value);
            break;
        case QCP::msAll:
            margins = QMargins(value, value, value, value);
            break;
        default:
            break;
    }
}

/*! \internal

  Returns the value of the specified \a side of \a margins. If \a side is \ref QCP::msNone or
  \ref QCP::msAll, returns 0.

  \see setMarginValue
*/
inline int getMarginValue(const QMargins& margins, QCP::MarginSide side)
{
    switch (side)
    {
        case QCP::msLeft:
            return margins.left();
        case QCP::msRight:
            return margins.right();
        case QCP::msTop:
            return margins.top();
        case QCP::msBottom:
            return margins.bottom();
        default:
            break;
    }
    return 0;
}


Q_ENUM_NS(ExportPen)
Q_ENUM_NS(ResolutionUnit)
Q_ENUM_NS(SignDomain)
Q_ENUM_NS(MarginSide)
Q_ENUM_NS(AntialiasedElement)
Q_ENUM_NS(PlottingHint)
Q_ENUM_NS(Interaction)
Q_ENUM_NS(SelectionRectMode)
Q_ENUM_NS(SelectionType)

Q_FLAG_NS(AntialiasedElements)
Q_FLAG_NS(PlottingHints)
Q_FLAG_NS(MarginSides)
Q_FLAG_NS(Interactions)


} // end of namespace QCP

Q_DECLARE_OPERATORS_FOR_FLAGS(QCP::AntialiasedElements)
Q_DECLARE_OPERATORS_FOR_FLAGS(QCP::PlottingHints)
Q_DECLARE_OPERATORS_FOR_FLAGS(QCP::MarginSides)
Q_DECLARE_OPERATORS_FOR_FLAGS(QCP::Interactions)

#endif // QCP_GLOBAL_H
