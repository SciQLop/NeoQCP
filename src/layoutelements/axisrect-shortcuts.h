/*!
  The one-call annotations of an axis rect: a line of data, a colour map, a text and an ellipse,
  each in the space it is given. They are add<T>() with the styling a caller would apply right
  after, so a call site reads as the picture it makes. Included by qcp.h after construct.h, where
  add<T>() lives.
*/

#ifndef QCP_LAYOUTELEMENT_AXISRECT_SHORTCUTS_H
#define QCP_LAYOUTELEMENT_AXISRECT_SHORTCUTS_H

#include "../items/item-ellipse.h"
#include "../items/item-text.h"
#include "../plottables/plottable-graph.h"
#include "../plottables/plottable-colormap2.h"
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
  A colour map of \a z over the grid of \a x and \a y, styled by \a options. The ranges go through
  qcp::toDoubles into vectors the map takes over, and flat \a z is x-major, as QCPColorMap2::setData
  reads it.

  The map joins the rect's colour scale at the add, which is what makes the bar appear; the options
  then go through the map's own setters, which forward to that scale, so the gradient, the data scale
  and the range are the ones asked for. A map with \a options.colorBar false is detached from the
  scale first: it must not restyle a bar it does not show.

  Returns an empty handle when the rect has no such axis pair or the map refuses the shapes, as
  add<QCPColorMap2>() does.
*/
template <QCPNumberRange X, QCPNumberRange Y, QCPNumbersOnly Z>
QCPHandle<QCPColorMap2> QCPAxisRect::colorMap(const X& x, const Y& y, const Z& z,
                                              const QCPColorMapOptions& options)
{
    const QCPHandle<QCPColorMap2> map
        = add<QCPColorMap2>(qcp::toDoubles(x), qcp::toDoubles(y), qcp::toDoubles(z));
    if (!map)
        return map;
    if (!options.colorBar)
        map->setColorScale(nullptr);
    map->setGradient(options.gradient);
    map->setDataScale(options.dataScale);
    map->setName(options.name);
    if (options.dataRange)
        map->setDataRange(*options.dataRange);
    else
        map->rescaleDataRange(true);
    return map;
}

/*!
  The columns form of colorMap(): one column of \a z per \a x value, each of y.size() values,
  flattened x-major so that a column is a column of the map. Ragged columns and a column count other
  than x.size() are misuse, reported once, and give an empty handle: the flattening is the only place
  that knows which of the two shapes is wrong.
*/
template <QCPNumberRange X, QCPNumberRange Y, QCPColumns Z>
QCPHandle<QCPColorMap2> QCPAxisRect::colorMap(const X& x, const Y& y, const Z& z,
                                              const QCPColorMapOptions& options)
{
    const std::vector<double> keys = qcp::toDoubles(x);
    const std::vector<double> values = qcp::toDoubles(y);
    std::vector<double> flat;
    std::size_t rows = 0;
    bool first = true;
    for (const auto& column : qcp::columns(z))
    {
        const std::vector<double> numbers = qcp::toDoubles(column);
        QCP_CHECK_OR_RETURN(first || numbers.size() == rows,
                            "the columns of z are ragged: every one needs the same number of "
                            "values",
                            QCPHandle<QCPColorMap2> { });
        rows = first ? numbers.size() : rows;
        first = false;
        flat.insert(flat.end(), numbers.begin(), numbers.end());
    }
    QCP_CHECK_OR_RETURN(flat.size() == keys.size() * rows,
                        "the columns of z are one per x value", QCPHandle<QCPColorMap2> { });
    return colorMap(keys, values, flat, options);
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
