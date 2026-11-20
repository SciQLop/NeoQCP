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

#ifndef QCP_ITEM_ZOOM_INDICATOR_H
#define QCP_ITEM_ZOOM_INDICATOR_H

#include "../global.h"
#include "../layoutelements/layoutelement-axisrect.h"
#include "item.h"

#include <array>

class QCustomPlot;

namespace qcp
{
/*!
  Which of the four connectors of a zoom indicator to draw, for a \a box and the \a inset it points
  at, both in pixels with y pointing down.

  Index 0 is the connector between the bottom-left corners of the two rects, 1 the top-left, 2 the
  bottom-right, 3 the top-right. This is the rule matplotlib's zoom inset uses: a connector is drawn
  when the inset does not already sit beyond the box on both sides that corner looks at, so an inset
  to the upper right of the box is reached from the box's top-left and bottom-right corners.
*/
[[nodiscard]] inline std::array<bool, 4> zoomConnectorVisibility(const QRectF& box,
                                                               const QRectF& inset)
{
    const bool x0 = box.left() < inset.left(), x1 = box.right() < inset.right();
    const bool y0 = box.bottom() > inset.bottom(), y1 = box.top() > inset.top();
    return { x0 != y0, x0 == y1, x1 == y0, x1 != y1 };
}
} // namespace qcp

/*!
  \brief The box an inset's ranges cover in its parent, and the connectors that join it to the inset.

  Makes one with QCPAxisRect::zoomIndicator() on the parent of an inset, which is where the box and
  the connectors belong:

  \snippet layout.cpp layout_zoom_indicator

  The box runs from the inset's bottom axis range to its left axis range, in this rect's bottom and
  left axes. Each of the four corners may carry a connector to the same corner of the inset's data
  rect, chosen by qcp::zoomConnectorVisibility().

  The box and the connectors are remade whenever the inset's ranges change, and whenever the frame
  moves under them, so panning the inset, panning this rect, or resizing the widget all keep the
  picture true. Nothing is drawn while the inset is hidden or after it has been deleted.
*/
class QCP_LIB_DECL QCPItemZoomIndicator : public QCPItem
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(QCPItemZoomIndicator)
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(QPen pen READ pen WRITE setPen NOTIFY penChanged)
    Q_PROPERTY(QBrush brush READ brush WRITE setBrush NOTIFY brushChanged)
    Q_PROPERTY(QCPAxisRect* inset READ inset WRITE setInset NOTIFY insetChanged)
    /// \endcond

public:
    QCPItemZoomIndicator();
    virtual ~QCPItemZoomIndicator() override;

    // getters:
    QPen pen() const { return mPen; }

    QBrush brush() const { return mBrush; }

    QCPAxisRect* inset() const { return mInset.data(); }

    // setters:
    void setPen(const QPen& pen);
    void setBrush(const QBrush& brush);
    void setInset(QCPAxisRect* inset);

    // reimplemented virtual methods:
    void draw(QCPDrawContext& context) const override;

signals:
    void penChanged(const QPen& pen);
    void brushChanged(const QBrush& brush);
    void insetChanged(QCPAxisRect* inset);

protected:
    // The connectors end on the inset's pixel rect, which a layout can move without moving this
    // rect's frame: the recording is remade when it does.
    [[nodiscard]] quint64 dependencyRevision() const override;

    // property members:
    QPen mPen;
    QBrush mBrush;

    QPointer<QCPAxisRect> mInset;
    //! The inset's two axes and its visibility, each one a reason to remake the recording.
    QList<QMetaObject::Connection> mInsetLinks;

    friend class TestZoomIndicator; // reads the recording's count
};

#endif // QCP_ITEM_ZOOM_INDICATOR_H
