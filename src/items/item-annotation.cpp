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

#include "item-annotation.h"

#include "../core.h"
#include "../layoutelements/layoutelement-axisrect.h"
#include "../painting/draw-commands.h"
#include "../painting/draw-context.h"

#include <cmath>
#include <utility>
#include <vector>

namespace
{
//! The unit direction a head at \a degrees points along, in widget pixels: \a degrees is
//! QPainterPath::angleAtPercent's angle, counter-clockwise with y up the screen.
QCPVector2D headDirection(double degrees)
{
    const double radians = qDegreesToRadians(degrees);
    return { std::cos(radians), -std::sin(radians) };
}
} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPItemAnnotation
////////////////////////////////////////////////////////////////////////////////////////////////////

/*! \class QCPItemAnnotation
  \brief A text and the line from it to a target: matplotlib's `annotate`.

  One item instead of a text and an arrow that must be kept together: \ref position is the text,
  \ref target is what it points at, and each names its own space, so a label pinned in pixels with a
  tip that follows the data is one annotation.

  The connector starts at the centre of the text box, leaves the box where its path crosses the
  box's edge, and ends at the target; \ref shrink cuts it that many pixels short of both ends. The
  text draws over the connector. A target inside the box leaves no line to draw, and the text still
  draws; so does a connector whose shape has no answer there (see QCPConnector).

  The connector and its head are recorded in pixels, so panning the target's axes moves the tip and
  never the text, and a view change remakes the recording.
*/

/*! \fn void QCPItemAnnotation::textChanged(const QString& text)

  This signal is emitted when \ref text changes, after the plot delivers its deferred signals.
*/

/*! \fn void QCPItemAnnotation::textStyleChanged(const QCPTextStyle& textStyle)

  This signal is emitted when \ref textStyle changes, after the plot delivers its deferred signals.
*/

/*! \fn void QCPItemAnnotation::connectorChanged(const QCPConnector& connector)

  This signal is emitted when \ref connector changes, after the plot delivers its deferred signals.
*/

/*! \fn void QCPItemAnnotation::penChanged(const QPen& pen)

  This signal is emitted when \ref pen changes, after the plot delivers its deferred signals.
*/

/*! \fn void QCPItemAnnotation::selectedPenChanged(const QPen& selectedPen)

  This signal is emitted when \ref selectedPen changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPItemAnnotation::headChanged(const QCPLineEnding& head)

  This signal is emitted when \ref head changes, after the plot delivers its deferred signals.
*/

/*! \fn void QCPItemAnnotation::shrinkChanged(QCP::PixelLength shrink)

  This signal is emitted when \ref shrink changes, after the plot delivers its deferred signals.
*/

/*!
  Creates an annotation with no text and both ends at the origin, a straight connector in a plain
  black line, and a selected look of a blue line two pixels wide. The text takes the drawing
  context's defaults: no box, the plot's own font and colour.
*/
QCPItemAnnotation::QCPItemAnnotation()
        : QCPItem()
        , target(createPosition(QLatin1String("target")))
        , position(createPosition(QLatin1String("position")))
{
    setPen(QPen(Qt::black));
    setSelectedPen(QPen(Qt::blue, 2));
    // An annotation is not clipped to its rect, as matplotlib's annotate is not: a label may hang
    // past the edge, so the recording is clipped to the viewport instead.
    setClipToAxisRect(false);
}

QCPItemAnnotation::~QCPItemAnnotation() { }

/*!
  Sets the text drawn at \ref position. An empty text draws no box, and the connector then starts at
  the position itself.
*/
void QCPItemAnnotation::setText(const QString& text)
{
    if (text == mText)
        return;
    update();
    mText = text;
    qcpEmit(this, mParentPlot, &QCPItemAnnotation::textChanged, this->text());
}

/*!
  Sets the font, colour, box and alignment of the text: the same QCPTextStyle the drawing context
  takes. \ref QCPTextStyle::padding is inside the box the connector leaves.
*/
void QCPItemAnnotation::setTextStyle(const QCPTextStyle& textStyle)
{
    if (textStyle == mTextStyle)
        return;
    update();
    mTextStyle = textStyle;
    qcpEmit(this, mParentPlot, &QCPItemAnnotation::textStyleChanged, this->textStyle());
}

/*!
  Sets the shape of the line from the text to the target: an arc, a curve, an elbow, arms or a
  bracket (see QCPConnector).
*/
void QCPItemAnnotation::setConnector(const QCPConnector& connector)
{
    if (connector == mConnector)
        return;
    update();
    mConnector = connector;
    qcpEmit(this, mParentPlot, &QCPItemAnnotation::connectorChanged, this->connector());
}

/*!
  Sets the pen of the connector and of its head; the default is a plain black line.
*/
void QCPItemAnnotation::setPen(const QPen& pen)
{
    if (pen == mPen)
        return;
    update();
    mPen = pen;
    qcpEmit(this, mParentPlot, &QCPItemAnnotation::penChanged, this->pen());
}

/*!
  Sets the pen the connector and its head are drawn with while the item is selected. The default is
  a blue line two pixels wide; the text keeps its own style.
*/
void QCPItemAnnotation::setSelectedPen(const QPen& selectedPen)
{
    if (selectedPen == mSelectedPen)
        return;
    update();
    mSelectedPen = selectedPen;
    qcpEmit(this, mParentPlot, &QCPItemAnnotation::selectedPenChanged, this->selectedPen());
}

/*!
  Sets the line ending at the target, turned along the path where the connector arrives. The default
  is \ref QCPLineEnding::esNone, no head; there is no tail at the text.
*/
void QCPItemAnnotation::setHead(const QCPLineEnding& head)
{
    if (head == mHead)
        return;
    update();
    mHead = head;
    qcpEmit(this, mParentPlot, &QCPItemAnnotation::headChanged, this->head());
}

/*!
  Sets how many pixels short of the text box and of the target the connector stops, both ends from
  the one value. The default is zero, a line that touches both.
*/
void QCPItemAnnotation::setShrink(QCP::PixelLength shrink)
{
    if (shrink.px == mShrink.px)
        return;
    update();
    mShrink = shrink;
    qcpEmit(this, mParentPlot, &QCPItemAnnotation::shrinkChanged, this->shrink());
}

/*! \internal

  The text's measured glyphs, measured again only when the string, the font or mathText changes: a
  recording asks on every frame, and measuring is the expensive part.
*/
QSize QCPItemAnnotation::glyphSize(const QCPTextStyle& style) const
{
    if (!mGlyphValid || mGlyphText != mText || mGlyphFont != style.font
        || mGlyphMath != style.mathText)
    {
        mGlyphText = mText;
        mGlyphFont = style.font;
        mGlyphMath = style.mathText;
        mGlyphSize = qcpMeasureText(style, mText);
        mGlyphValid = true;
    }
    return mGlyphSize;
}

/*! \internal

  Returns the pen that should be used for drawing the connector.
*/
QPen QCPItemAnnotation::mainPen() const
{
    return mSelected ? mSelectedPen : mPen;
}

/*! \internal

  Whether the whole annotation is hidden: a target in data space that lies outside the rect holding
  it is matplotlib's \c annotation_clip, and it hides the text along with the connector. A target
  given in pixels, ratios or an anchor is never hidden this way.
*/
bool QCPItemAnnotation::hiddenForItsTarget() const
{
    if (target->type() != QCPItemPosition::ptPlotCoords)
        return false;
    const QCPHandle<QCPAxisRect> rect = target->axisRect();
    if (!rect)
        return false;
    return !QRectF(rect->rect()).contains(target->pixelPosition());
}

/* inherits documentation from base class */
void QCPItemAnnotation::draw(QCPDrawContext& context) const
{
    // The connector and the text's box are laid out in pixels, so the recording is remade whenever
    // the view changes, a pan included (P-6).
    (void)context.transform();

    if (hiddenForItsTarget())
        return;

    bool snapshot = false;
    const QCP::Position at = position->asPosition(&snapshot);
    if (snapshot) // the position's space had to become pixels: they go stale without a new read
        context.recordEveryFrame();

    // A style with no colour of its own takes the theme's foreground, as QCPItemText does: left
    // invalid, the replay would take the painter's pen, which the connector has just set.
    QCPTextStyle style = mTextStyle;
    if (!style.color.isValid())
        style.color = context.theme().foreground();

    const bool hasText = !mText.isEmpty();
    const QSize glyphs = hasText ? glyphSize(style) : QSize();
    const QRectF box = hasText ? QRectF(qcpTextLayout(style, glyphs).box) : QRectF();

    // The text's box is axis aligned in the text's own frame, which is turned with the text: the
    // connector is built in screen space, as matplotlib builds it, then mapped into that frame so
    // the cut can be taken against the box.
    QTransform frame;
    const QPointF anchor = position->pixelPosition();
    frame.translate(anchor.x(), anchor.y());
    frame.rotate(style.rotationDegrees);
    const QPointF start = hasText ? frame.map(box.center()) : anchor;
    const QPainterPath path = mConnector.path(start, target->pixelPosition());
    const qcp::TrimmedConnector trimmed
        = qcp::trimConnector(frame.inverted().map(path), box, mShrink.px);

    const QPen pen = mainPen();
    if (trimmed.line.size() > 1)
    {
        const QPolygonF pixels = frame.map(trimmed.line);
        std::vector<QCP::Position> points;
        points.reserve(std::size_t(pixels.size()));
        for (const QPointF& point : pixels)
            points.push_back(QCP::Pixels { point.x(), point.y() });
        context.polyline(points, pen);

        const QPointF tip = pixels.back();
        const QPainterPath head
            = mHead.path(headDirection(trimmed.endAngleDegrees - style.rotationDegrees), pen.widthF());
        if (!head.isEmpty())
            context.path(QCP::Pixels { tip.x(), tip.y() }, head, mHead.penFor(pen),
                         mHead.brushFor(pen));
    }

    if (hasText)
        context.text(at, mText, style, glyphs);
}
