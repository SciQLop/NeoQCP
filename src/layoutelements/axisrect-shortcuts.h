/*!
  The one-call annotations of an axis rect: a line of data, a text and an ellipse, each in the space
  it is given. They are add<T>() with the styling a caller would apply right after, so a call site
  reads as the picture it makes. Included by qcp.h after construct.h, where add<T>() lives.
*/

#ifndef QCP_LAYOUTELEMENT_AXISRECT_SHORTCUTS_H
#define QCP_LAYOUTELEMENT_AXISRECT_SHORTCUTS_H

#include "../items/item-ellipse.h"
#include "../items/item-text.h"
#include "../plottables/plottable-graph.h"
#include "construct.h"
#include "layoutelement-axisrect.h"

/*!
  Draws \a values against \a keys on this rect's first bottom and left axes, styled by \a options.
  The data goes into the graph's data constructor, which is setData() without the call: the same
  copy, one expression.

  Returns an empty handle when the rect has no such axis pair or the system refuses the graph, as
  add<QCPGraph>() does.
*/
template <QCPNumberRange K, QCPNumbersOnly V>
QCPHandle<QCPGraph> QCPAxisRect::line(const K& keys, const V& values, const QCPLineOptions& options)
{
    const QCPHandle<QCPGraph> graph = add<QCPGraph>(keys, values);
    if (!graph)
        return graph;
    graph->setPen(options.pen);
    graph->setName(options.name);
    graph->setScatterStyle(options.scatterStyle);
    return graph;
}

/*!
  A text item at \a at, carrying \a text. \a at names its own space (data coordinates, pixels, a
  ratio of this rect, ...), and \a style is the same QCPTextStyle the drawing context takes.

  Every style field is applied, whatever its value: the defaults of QCPTextStyle are a drawing
  context's, not an item's, so a default style gives an item whose \a mathText is off, while an
  item built on its own measures and draws through the label renderer. A style colour that is not
  valid leaves the item's own, which is the colour the plot's theme gives a plain annotation.
*/
inline QCPHandle<QCPItemText> QCPAxisRect::text(QCP::Position at, const QString& text,
                                                const QCPTextStyle& style)
{
    const QCPHandle<QCPItemText> item = add<QCPItemText>();
    if (!item)
        return item;
    item->setPosition(at);
    item->setText(text);

    item->setFont(style.font);
    if (style.color.isValid())
        item->setColor(style.color);
    item->setPen(style.pen);
    item->setBrush(style.brush);
    item->setPositionAlignment(style.alignment);
    item->setTextAlignment(static_cast<Qt::Alignment>(style.textFlags));
    item->setRotation(style.rotationDegrees);
    item->setPadding(style.padding.toMargins());
    item->setMathText(style.mathText);
    return item;
}

/*!
  The built-in ellipse item between the corners \a a and \a b, each in its own space: a data corner
  and a pixel corner is a shape that follows the data on one side and the widget on the other. An
  anchored corner (QCPItemAnchor plus a pixel offset) keeps following its anchor.
*/
inline QCPHandle<QCPItemEllipse> QCPAxisRect::ellipse(QCP::Position a, QCP::Position b,
                                                      const QCPShapeOptions& options)
{
    const QCPHandle<QCPItemEllipse> item = add<QCPItemEllipse>();
    if (!item)
        return item;
    item->topLeft->set(a);
    item->bottomRight->set(b);
    item->setPen(options.pen);
    item->setBrush(options.brush);
    return item;
}

#endif // QCP_LAYOUTELEMENT_AXISRECT_SHORTCUTS_H
