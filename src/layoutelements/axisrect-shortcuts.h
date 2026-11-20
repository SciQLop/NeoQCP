/*!
  The one-call annotations of an axis rect: a line of data, a colour map, a text and an ellipse,
  each in the space it is given. They are add<T>() with the styling a caller would apply right
  after, so a call site reads as the picture it makes. Included by qcp.h after construct.h, where
  add<T>() lives.
*/

#ifndef QCP_LAYOUTELEMENT_AXISRECT_SHORTCUTS_H
#define QCP_LAYOUTELEMENT_AXISRECT_SHORTCUTS_H

#include <optional>
#include <utility>
#include <vector>

#include "../items/item-ellipse.h"
#include "../items/item-text.h"
#include "../items/item-zoom-indicator.h"
#include "../plottables/graph-fill.h"
#include "../plottables/plottable-graph.h"
#include "../plottables/plottable-fillbetween.h"
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

inline std::optional<QCPAxisRect::FillFrame>
QCPAxisRect::fillFrameOf(const QCPGraph& line) const
{
    QCPCoordinateSystem* system = line.system().get();
    if (!system || system->area().get() != this)
        return std::nullopt;
    QCPAxis* keyAxis = line.keyAxis().get();
    QCPAxis* valueAxis = line.valueAxis().get();
    if (!keyAxis || !valueAxis)
    {
        // A user system keeps no key or value axis of its own: it maps through the pair it was made
        // with, and a system without a pair of QCPAxis has no fill to draw in.
        keyAxis = qobject_cast<QCPAxis*>(system->keyAxis().get());
        valueAxis = qobject_cast<QCPAxis*>(system->valueAxis().get());
    }
    const QList<QCPAxis*> own = axesRaw();
    if (!keyAxis || !valueAxis || !own.contains(keyAxis) || !own.contains(valueAxis))
        return std::nullopt;
    return FillFrame { system, keyAxis, valueAxis };
}

inline void QCPAxisRect::placeBelowTheLowerLine(QCPFillBetween& fill, const QCPGraph& first,
                                               const QCPGraph& second) const
{
    const QCPHandle<QCPLayer> layer = first.layer();
    if (!layer)
        return;
    // The fill stays on the first line's layer: below both is below the first, unless the second
    // draws on that layer before it.
    const QCPGraph* lower = &first;
    if (second.layer() && second.layer().get() == layer.get())
        for (const QCPHandle<QCPLayerable>& child : layer->children())
            if (child.get() == &first || child.get() == &second)
            {
                lower = child.get() == &first ? &first : &second;
                break;
            }
    fill.moveBelow(lower);
}

/*!
  The area between \a first and \a second on this rect, styled by \a options.

  The fill draws under the lower of its two lines: the fill goes just below whichever of the two
  draws first, on that line's layer. For two lines on one key axis, the key axis decides the frame,
  and a fill across two value axes reads both.

  Both lines must be in this rect and must fill to each other: the same coordinate system, which
  may be a user system (the fill then draws in that frame, as the lines do), or 2.x's channel
  between y and y2 (one key axis, two value axes). A fill of a line with itself needs two different
  components, and a component a line does not have is refused too. Anything else is misuse,
  reported once, and gives an empty handle.
*/
inline QCPHandle<QCPFillBetween> QCPAxisRect::fillBetween(const QCPHandle<QCPGraph>& first,
                                                          const QCPHandle<QCPGraph>& second,
                                                          const QCPFillOptions& options)
{
    QCP_CHECK_OR_RETURN(first && second, "a fill needs two lines", { });
    QCP_CHECK_OR_RETURN(first.get() != second.get()
                            || options.firstComponent != options.secondComponent,
                        "a fill of a line with itself needs two components of it", { });
    QCP_CHECK_OR_RETURN(first.get() == second.get() || qcp::fill::canFillTo(*first, *second),
                        "the two lines of a fill must fill to each other", { });
    const std::optional<FillFrame> frame = fillFrameOf(*first);
    const std::optional<FillFrame> otherFrame
        = first.get() == second.get() ? frame : fillFrameOf(*second);
    QCP_CHECK_OR_RETURN(frame && otherFrame, "the two lines of a fill must be on this rect", { });
    const auto namesAColumn = [](const QCPGraph& line, int component)
    {
        const QCPMultiDataSource* source = line.dataSource();
        return !source || source->empty()
            || (component >= 0 && component < source->columnCount()
                && component < line.componentCount());
    };
    QCP_CHECK_OR_RETURN(namesAColumn(*first, options.firstComponent)
                            && namesAColumn(*second, options.secondComponent),
                        "the fill's component does not name a column of its line", { });

    const QCPHandle<QCPFillBetween> fill
        = add<QCPFillBetween>(frame->keyAxis, frame->valueAxis);
    if (!fill)
        return fill;
    fill->setSystem(frame->system); // the lines' own system, whatever pair its axes are on
    fill->setLines(first, options.firstComponent, second, options.secondComponent);
    fill->setBrush(options.brush);
    fill->setBelowBrush(options.belowBrush);
    fill->setName(options.name);
    if (first.get() == second.get())
        fill->moveBelow(first.get()); // the only line draws first, so below it is where a fill goes
    else
        placeBelowTheLowerLine(*fill, *first, *second);
    return fill;
}

/*!
  A colour map of \a z over the grid of \a x and \a y, styled by \a options. The ranges go through
  qcp::toDoubles into vectors the map takes over, and flat \a z is x-major, as QCPColorMap2::setData
  reads it: z[i * y.size() + j] belongs to x[i], y[j].

  The shapes are checked here rather than in the map, which would drop a grid it cannot read with a
  bare warning and leave an empty map behind: \a x and \a y need a value each, and flat \a z needs
  x.size() * y.size() values. Anything else is misuse, reported once, and gives an empty handle.

  An empty handle also comes from add<QCPColorMap2>() itself, when the rect has no axis pair to draw
  on.

  The map joins the rect's colour scale at the add, which is what makes the bar appear; the options
  then go through the map's own setters, which forward to that scale, so the gradient, the data scale
  and the range are the ones asked for. A map with \a options.colorBar false is detached from the
  scale first: it must not restyle a bar it does not show.
*/
template <QCPNumberRange X, QCPNumberRange Y, QCPNumbersOnly Z>
QCPHandle<QCPColorMap2> QCPAxisRect::colorMap(const X& x, const Y& y, const Z& z,
                                              const QCPColorMapOptions& options)
{
    std::vector<double> keys = qcp::toDoubles(x);
    std::vector<double> values = qcp::toDoubles(y);
    std::vector<double> numbers = qcp::toDoubles(z);
    QCP_CHECK_OR_RETURN(!keys.empty() && !values.empty(),
                        "x and y need at least one value each: a colour map is a grid",
                        QCPHandle<QCPColorMap2> { });
    QCP_CHECK_OR_RETURN(numbers.size() == keys.size() * values.size(),
                        "z needs one value per grid point: x.size() * y.size() values",
                        QCPHandle<QCPColorMap2> { });
    const QCPHandle<QCPColorMap2> map
        = add<QCPColorMap2>(std::move(keys), std::move(values), std::move(numbers));
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
  flattened x-major so that a column is a column of the map. The flattening is the only place that
  knows which shape is wrong, so each mismatch is named on its own: ragged columns, a column length
  other than y.size(), a column count other than x.size(). Each is misuse, reported once, and gives
  an empty handle.
*/
template <QCPNumberRange X, QCPNumberRange Y, QCPColumns Z>
QCPHandle<QCPColorMap2> QCPAxisRect::colorMap(const X& x, const Y& y, const Z& z,
                                              const QCPColorMapOptions& options)
{
    std::vector<double> keys = qcp::toDoubles(x);
    std::vector<double> values = qcp::toDoubles(y);
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
        QCP_CHECK_OR_RETURN(numbers.size() == values.size(),
                            "each column of z needs one value per y value",
                            QCPHandle<QCPColorMap2> { });
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

/*!
  The zoom indicator of \a inset: the box its bottom and left axis ranges cover in this rect, and
  the connectors that join the box to the inset's corners, styled by \a options.

  An empty handle, and a rect that is not an inset of this one (a layout cell, an inset of another
  rect or plot), are misuse: one report and an empty handle. The report for the rect comes from
  setInset(), which is the rule's only home. An item may be given no inset later with setInset(nullptr).
*/
inline QCPHandle<QCPItemZoomIndicator>
QCPAxisRect::zoomIndicator(const QCPHandle<QCPAxisRect>& inset, const QCPShapeOptions& options)
{
    QCP_CHECK_OR_RETURN(inset, "a zoom indicator needs an inset", { });
    const QCPHandle<QCPItemZoomIndicator> item = add<QCPItemZoomIndicator>();
    if (!item)
        return item;
    item->setPen(options.pen);
    item->setBrush(options.brush);
    item->setInset(inset); // a rect that is not an inset of this one is reported there
    if (!item->inset())
    {
        item.remove();
        return QCPHandle<QCPItemZoomIndicator>();
    }
    return item;
}

#endif // QCP_LAYOUTELEMENT_AXISRECT_SHORTCUTS_H
