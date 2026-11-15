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
  The data goes straight into the graph's data constructor: the ranges are read once, into the
  array the graph stores, and no setData() pass follows.

  Returns an empty handle when the rect has no such axis pair or the system refuses the graph, as
  add<QCPGraph>() does.
*/
template <QCPNumberRange K, QCPNumberRange V>
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

  A style field left at its default leaves the item's own: its alignment, and the colour the plot
  themes, so text(at, QStringLiteral("x")) reads as a plain annotation.
*/
inline QCPHandle<QCPItemText> QCPAxisRect::text(QCP::Position at, const QString& text,
                                                const QCPTextStyle& style)
{
    const QCPHandle<QCPItemText> item = add<QCPItemText>();
    if (!item)
        return item;
    item->setPosition(at);
    item->setText(text);

    const QCPTextStyle defaults;
    if (style.font != defaults.font)
        item->setFont(style.font);
    if (style.color != defaults.color)
        item->setColor(style.color);
    if (style.pen != defaults.pen)
        item->setPen(style.pen);
    if (style.brush != defaults.brush)
        item->setBrush(style.brush);
    if (style.alignment != defaults.alignment)
        item->setPositionAlignment(style.alignment);
    if (style.textFlags != defaults.textFlags)
        item->setTextAlignment(static_cast<Qt::Alignment>(style.textFlags));
    if (style.rotationDegrees != defaults.rotationDegrees)
        item->setRotation(style.rotationDegrees);
    if (style.padding != defaults.padding)
        item->setPadding(style.padding.toMargins());
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
