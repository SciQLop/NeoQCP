# NeoQCP use-case catalogue (evidence for NeoQCP 3.0)

Evidence only, no design. Two sources:

- `SQP/` = `/home/jeandet/Documents/prog/SciQLopPlots` (`src/`, `include/SciQLopPlots/`): the main client.
- `NQ/` = `SQP/subprojects/NeoQCP` (`docs/snippets/*.cpp`, `docs/*.md`, `examples/`): NeoQCP's own docs and examples.

Counts come from grep. They are rough. "docs" means `NQ/docs/snippets`, "examples" means `NQ/examples/**/*.cpp`, "SQP" means `SQP/src` + `SQP/include`.

Headline counts in SciQLopPlots:

- 152 `QPointer<...>` members or locals. 19 of them point at a NeoQCP type.
- 85 lines of "am I still alive?" guards (`QPointer self` / `if (!self)`) after a NeoQCP call or emit.
- 62 `qptr_apply` / `qptr_apply_or` calls: "do this only if the NeoQCP object still exists".
- 168 `replot(` calls, 54 of them `rpQueuedReplot`.
- 205 `SCIQLOP_ON_OWNER_THREAD` hops to the GUI thread. They exist because NeoQCP objects are GUI-thread only.
- 12 `QOverload<const QCPRange&>::of(...)`-style casts, needed to connect an overloaded signal.

---

## A. Plot and axes

### 1. Create a plot and configure its axes (labels, ranges, log scale, tickers)

**Frequency.** Every doc snippet (about 60). Every example. In SQP: one constructor (`SciQLopPlot.cpp:98-158`) and the axis wrapper (`SciQLopPlotAxis.cpp`, about 30 setters). 17 `setTicker` in docs, 12 in examples.

**Excerpt.** `NQ/docs/snippets/getting-started.cpp:14-20`
```cpp
    customPlot->addGraph();
    customPlot->graph(0)->setData(x, y);
    customPlot->xAxis->setLabel("x");
    customPlot->yAxis->setLabel("y");
    customPlot->xAxis->setRange(-1, 1);
    customPlot->yAxis->setRange(0, 1);
    customPlot->replot();
```
A log axis takes two calls that must agree. `NQ/docs/snippets/export.cpp:40-43`
```cpp
    customPlot->xAxis->setScaleType(QCPAxis::stLogarithmic);
    customPlot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    customPlot->xAxis->setTicker(QSharedPointer<QCPAxisTickerLog>(new QCPAxisTickerLog));
    customPlot->yAxis->setTicker(QSharedPointer<QCPAxisTickerLog>(new QCPAxisTickerLog));
```
SQP has to do the same change with signals blocked. `SQP/src/SciQLopPlotAxis.cpp:132-141`
```cpp
    if (!m_axis.isNull() && (m_axis->scaleType() == QCPAxis::stLogarithmic) != log)
    {
        {
            // Block signals during scale type + ticker change to prevent
            // rangeChanged from triggering margin recomputation with stale tick vectors
            const QSignalBlocker blocker(m_axis);
            m_axis->setScaleType(log ? QCPAxis::stLogarithmic : QCPAxis::stLinear);
            m_axis->setTicker(make_ticker(log));
        }
        m_axis->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
```

**Boilerplate.**
- A manual `replot()` closes every snippet (96 `replot()` in docs).
- A log axis needs a scale type and a matching ticker, set separately. One `QSharedPointer<...>::create()` per axis.
- SQP keeps its own ticker factory, so the ticker follows the time / log / text state (`SciQLopPlotAxis.cpp:615-632`).
- SQP calls `m_axis->parentPlot()->replot(QCustomPlot::rpQueuedReplot)` after every single setter, for example `SciQLopPlotAxis.cpp:105,118,141,152,184,216,227`.

**Unsafe / error-prone.**
- Setting a log scale without a log ticker gives wrong ticks silently. The two calls can disagree.
- Setting the scale type emits `rangeChanged` while the ticker is still stale. SQP must block signals around it (`SciQLopPlotAxis.cpp:137`).
- `xAxis`, `yAxis`, `xAxis2` and `yAxis2` are raw public members. They become `nullptr` when the first axis rect is deleted (`NQ/docs/concepts.md:53-54`, `NQ/docs/layout.md:128`).
- SQP holds the axis as `QPointer<QCPAxis> m_axis` (`SQP/include/SciQLopPlots/SciQLopPlotAxis.hpp:325`) and null-checks it in every setter.

### 2. Add extra value axes (second y axis, stacked left axes)

**Frequency.** docs: 3 (`axes.cpp:60-96`, `interactions.cpp:39-56`, `axes_tickers`). examples: axis-tags, interactions. SQP: the four default axes are each wrapped once (`SciQLopPlot.cpp:150-154`).

**Excerpt.** `NQ/docs/snippets/axes.cpp:76-90`
```cpp
    customPlot->yAxis2->setVisible(true);                                   // right axis
    QCPAxis* humidityAxis = customPlot->axisRect()->addAxis(QCPAxis::atLeft);   // second left axis
    humidityAxis->grid()->setVisible(false);
    ...
    for (const auto& [axis, values, color, label] : series)
    {
        QCPGraph* graph = customPlot->addGraph(customPlot->xAxis, axis);
        graph->setData(hour, values);
        graph->setPen(QPen(color, 2));
        axis->setLabel(label);
        axis->setLabelColor(color);
        axis->setTickLabelColor(color);
    }
```

**Boilerplate.**
- The key axis is passed again for every graph, even when it is always `xAxis`.
- Colouring an axis like its graph means three setters (label colour, tick label colour, label).

**Unsafe / error-prone.**
- An extra axis is reached by index: `axisRect()->axis(QCPAxis::atRight, 1)` (`NQ/examples/axis-tags/mainwindow.cpp:46-51`). The index shifts when axes are added or removed.
- Drag and zoom do not follow a new axis on their own. They need `setRangeDragAxes` / `setRangeZoomAxes` lists (`interactions.cpp:52-55`).

---

## B. Data

### 3. Add a classic graph and give it data

**Frequency.** `addGraph`: 73 in docs, 39 in examples. SQP: 1 (`QCPAbstractPlottableWrapper.hpp`, `newComponent<QCPGraph>`).

**Excerpt.** `NQ/docs/snippets/layout.cpp:37-42`
```cpp
    customPlot->addGraph()->setData(x, s);
    customPlot->graph(0)->setName("sin(x)");
    customPlot->graph(0)->setPen(QPen(QColor(31, 119, 180), 2));
    customPlot->addGraph()->setData(x, c);
    customPlot->graph(1)->setName("cos(x)");
    customPlot->graph(1)->setPen(QPen(QColor(255, 127, 14), 2));
```
SQP has to pick one of two creation styles by type. `SQP/include/SciQLopPlots/Plotables/QCPAbstractPlottableWrapper.hpp` (`newComponent`)
```cpp
        if constexpr (std::is_same_v<T, QCPGraph>)
        {
            component = new SciQLopGraphComponent(plot->addGraph(keyAxis, valueAxis), this);
        }
        else if constexpr (std::is_base_of_v<QCPCurve, T>)
        {
            component = new SciQLopGraphComponent(new T(keyAxis, valueAxis), this);
        }
```

**Boilerplate.**
- There are two ways to create a plottable. `addGraph()` returns it. Every other type uses `new T(keyAxis, valueAxis)`, and the plot takes it through the axis's parent.
- The data is copied into `QVector<double>` first.

**Unsafe / error-prone.**
- `graph(i)` is index-based. Removing a graph shifts every later index. Used in `NQ/examples/plots/mainwindow.cpp:1433-1434` and `NQ/examples/scrollbar-axis-range-control/mainwindow.cpp:75-76`.
- `new QCPCurve(...)` looks like a leak but is not. The ownership is implicit (see `NQ/docs/concepts.md:33-35`, "never delete them yourself").

### 4. Plot large data with zero-copy or owning setters (QCPGraph2, QCPMultiGraph, QCPColorMap2)

**Frequency.** docs: 18 `new QCPGraph2/QCPMultiGraph`, 12 `new QCPColorMap*`, 9 `setDataSource/viewData`. examples: 0. SQP: every line graph, curve, colour map and histogram (5 wrapper classes).

**Excerpt (owning).** `NQ/docs/snippets/graphs.cpp:250-255`
```cpp
    auto* b = new QCPMultiGraph(customPlot->xAxis, customPlot->yAxis);
    b->setData(std::move(time), std::move(field));   // one column per component
    b->setName("B (nT)");
    b->setComponentNames({ "Bx", "By", "Bz" });
    b->setComponentPens({ QPen(QColor(31, 119, 180), 1.5), QPen(QColor(44, 160, 44), 1.5),
                          QPen(QColor(214, 39, 40), 1.5) });
```
**Excerpt (view).** `NQ/docs/snippets/graphs.cpp:159-174`
```cpp
    // The graph reads these buffers at every replot: they must outlive it.
    static std::vector<double> time(1000);
    static std::vector<double> level(1000);
    ...
    auto* graph = new QCPGraph2(customPlot->xAxis, customPlot->yAxis);
    graph->viewData(time.data(), level.data(), static_cast<int>(time.size()));
    graph->setPen(QPen(QColor(44, 160, 44), 2));

    // Later, after writing into the buffers in place:
    for (int i = 600; i < 700; ++i)
        level[i] = 0;
    graph->dataChanged();
```

**Boilerplate.**
- A raw `new` with two axes, every time.
- After writing in place, the user must call `dataChanged()` by hand.

**Unsafe / error-prone.**
- `viewData` keeps raw pointers and a count. The doc snippet makes the buffers `static` just to survive (`graphs.cpp:160-161`).
- The rule "must outlive, must not move, must keep its size" is in prose only (`NQ/docs/graphs.md:113-117`, `NQ/docs/colormaps.md:61`).
- Resampling runs on a worker thread, so a buffer freed after `viewData` is read later from another thread (`NQ/docs/graphs.md:129-132`).
- `int` counts. SQP rejects sizes above INT_MAX itself (`SQP/src/SciQLopColorMap.cpp:110-119`). It also rejects a 1-column colour map that NeoQCP would render blank with no error (`SciQLopColorMap.cpp:121-131`).

### 5. Replace data repeatedly from foreign (Python) buffers, with a lifetime guard

**Frequency.** SQP: every `set_data` path. `setDataSource` is called 9 times across `SciQLopMultiGraphBase.cpp`, `SciQLopColorMap.cpp`, `SciQLopHistogram.cpp`, `SciQLopHistogram2D.cpp`. docs: described in `graphs.md:129-132`, no snippet.

**Excerpt.** `SQP/src/SciQLopMultiGraphBase.cpp:62-100` (abridged; the three branches are verbatim in the file)
```cpp
    struct Buffers { SciQLopPyBuffer x, y; };
    auto guard = std::make_shared<Buffers>(Buffers{x, y});

    dispatch_dtype(y.format_code(), [&](auto tag) {
        using V = typename decltype(tag)::type;
        const auto* values = static_cast<const V*>(y.raw_data());

        if (y.ndim() == 1)
        {
            std::vector<std::span<const V>> columns{std::span<const V>(values, n)};
            auto source = std::make_shared<
                QCPSoAMultiDataSource<std::span<const double>, std::span<const V>>>(
                std::span<const double>(keys, n), std::move(columns), guard);
            _multiGraph->setDataSource(std::move(source));
        }
        else ... QCPRowMajorMultiDataSource<double, V>(..., guard) ...
        else ... QCPSoAMultiDataSource over per-column spans ...
```
The colour map uses an aliasing `shared_ptr` instead. `SQP/src/SciQLopColorMap.cpp:183-195`
```cpp
            auto source = std::make_shared<QCPSoADataSource2D<
                std::span<const double>, std::span<const Y>, std::span<const Z>>>(
                std::span<const double>(x_ptr, nx),
                std::span<const Y>(y_ptr, ny),
                std::span<const Z>(z_ptr, nz));

            _dataHolder = std::make_shared<DataSourceWithBuffers>(
                DataSourceWithBuffers{x, y, z, source});
            auto aliased = std::shared_ptr<QCPAbstractDataSource2D>(_dataHolder, source.get());
            _cmap->setDataSource(std::move(aliased));
```

**Boilerplate.**
- The user picks a source class by memory layout: SoA, row-major, or per-column spans.
- The user picks the numeric type through a dtype dispatch.
- The user builds a guard struct.
- The 1-D and 2-D sources keep the guard in two different ways: a third argument, or an aliasing `shared_ptr`.

**Unsafe / error-prone.**
- The guard is optional. Forget it and a worker-thread resample reads freed memory (`SciQLopMultiGraphBase.cpp:59-61`, comment).
- Each source class has a different constructor. One takes `(keys, values, n, cols, stride, guard)` with three ints in a row (`SciQLopMultiGraphBase.cpp:85-87`).
- An empty input must not reach `QCPSoADataSource2D`, which asserts. SQP clears with an empty `shared_ptr` instead (`SciQLopColorMap.cpp:150-166`).
- `setDataSource` can emit `busyChanged`, and the slot may delete the wrapper. So a `QPointer self` check follows (`SciQLopColorMap.cpp:160-166, 175, 198-201`).

### 6. Stream data (append and scroll)

**Frequency.** examples: 2 (`plots` realtime demo, `axis-tags`). docs: none for the new plottables (`addData()` exists on `QCPGraph` only, `performance.md:189`). SQP: none. It replaces whole buffers instead (use case 5).

**Excerpt.** `NQ/examples/plots/mainwindow.cpp:1433-1442`
```cpp
    ui->customPlot->graph(0)->addData(key, qSin(key)+std::rand()/(double)RAND_MAX*1*qSin(key/0.3843));
    ui->customPlot->graph(1)->addData(key, qCos(key)+std::rand()/(double)RAND_MAX*0.5*qSin(key/0.4364));
    ...
  ui->customPlot->xAxis->setRange(key, 8, Qt::AlignRight);
  ui->customPlot->replot();
```

**Boilerplate.**
- The user scrolls the axis by hand.
- The user calls `replot()` by hand on a timer.
- `rescaleValueAxis(false, true)` is called per graph (`axis-tags/mainwindow.cpp:77-80`).

**Unsafe / error-prone.**
- Graphs are reached by index (`graph(0)`).
- There is no append path on the zero-copy plottables, so streaming means re-sending the whole buffer.

### 7. React when asynchronous data is ready (busy state, first-data rescale)

**Frequency.** docs: `performance.cpp:139-156`, `interactions.cpp:290-297`. SQP: 6 `busyChanged` relays (`SciQLopColorMap.cpp:46`, `SciQLopMultiGraphBase.cpp:34`, `SciQLopHistogram.cpp:122`, `SciQLopHistogram2D.cpp:43`, `SciQLopCurve.cpp:278,295`), plus `check_first_data` (`SciQLopGraphInterface.cpp:119-134`).

**Excerpt.** `NQ/docs/snippets/performance.cpp:146-154`
```cpp
    graph->setData(std::move(keys), std::move(values));
    QObject::connect(graph, &QCPAbstractPlottable::busyChanged, customPlot,
                     [customPlot](bool busy) {
                         if (!busy)
                         {
                             customPlot->rescaleAxes();
                             customPlot->replot();
                         }
                     });
```

**Boilerplate.**
- "Rescale once the data has landed" is a hand-written connection.
- The connection is made after `setData`, and it stays connected for every later update too.

**Unsafe / error-prone.**
- After `setData`, the graph still reports the old data for up to 200 ms (the deferred swap, `NQ/docs/migrating.md:99`). `rescaleAxes()` called right away fits the old data.
- SQP had to make the first-data fit wait for the swap and skip off-view batches (`SciQLopGraphInterface.cpp:119-134`).
- Thread: `check_first_data` and `set_data_range` re-post themselves to the GUI thread (`SciQLopGraphInterface.cpp:110-125`). The axes may only be read there.

---

## C. Styling

### 8. Style a plottable (pen, brush, scatter, per-component style, fills, error bars)

**Frequency.** docs: most snippets. SQP: `setPen` 29, `setBrush` 11. `SciQLopGraphComponent::set_pen` and siblings wrap them.

**Excerpt.** `NQ/docs/snippets/graphs.cpp:333-340`
```cpp
    auto* measured = new QCPMultiGraph(customPlot->xAxis, customPlot->yAxis);
    measured->setData(std::move(x), std::move(columns));
    measured->setComponentNames({ "measured", "lower", "upper" });
    measured->setComponentLineStyle(0, QCPMultiGraph::lsNone);
    measured->component(0).scatterStyle = QCPScatterStyle(QCPScatterStyle::ssDisc, 6);
    measured->setComponentErrorBars(0, 1, 2);   // also takes "lower" and "upper" out of the legend
    measured->setComponentVisible(1, false);     // draw the bounds as bars only, not as lines
    measured->setComponentVisible(2, false);
```

**Boilerplate.**
- Components are addressed by integer index in every setter.
- A band or error bar needs three calls: hide the edges, remove them from the legend, then the error-bar or fill call.

**Unsafe / error-prone.**
- `component(0).scatterStyle = ...` writes a field directly, with no setter. So no change signal fires and no replot is queued (`graphs.cpp:337`).
- Component indexes go stale when `setData` changes the column count. SQP keeps `QPointer` component wrappers and detaches them first, because "a wrapper left listed would read past its end" (`QCPAbstractPlottableWrapper.hpp:47-58`).
- `setSelectionDecorator(new ...)`: "the graph owns it now" is stated only in a comment (`interactions.cpp:104-112`).

### 9. Apply a theme

**Frequency.** docs: 2 (`styling.cpp:28, 49-57`). examples: theme-demo. SQP: `SciQLopTheme` wrapper + `SciQLopPlot.cpp:1405-1430`, teardown at 1037-1043, panel at `SciQLopMultiPlotPanel.cpp:980`.

**Excerpt.** `NQ/docs/snippets/styling.cpp:49-57`
```cpp
    auto* theme = new QCPTheme(customPlot);
    theme->setBackground(QColor("#fdf6e3"));
    theme->setForeground(QColor("#586e75"));
    theme->setGrid(QColor("#e4dcc4"));
    theme->setSubGrid(QColor("#eee8d5"));
    theme->setSelection(QColor("#d33682"));
    theme->setLegendBackground(QColor("#eee8d5"));
    theme->setLegendBorder(QColor("#93a1a1"));
    customPlot->setTheme(theme);
```
SQP has to track the theme's death. `SQP/src/SciQLopPlot.cpp:1420-1424`
```cpp
        connect(theme, &QObject::destroyed, this, [this]() {
            m_impl->setTheme(nullptr);
            apply_selection_style();
            Q_EMIT theme_changed(nullptr);
        });
```

**Boilerplate.**
- The theme gets a parent through `new QCPTheme(parent)`, then is passed to `setTheme` separately.
- SQP re-applies the theme to its own items (the crosshair) on `QCPTheme::changed` (`SciQLopPlot.cpp:1413-1417`).
- A new colour scale or title must be themed again by hand: `applyTheme()` at `SciQLopPlot.cpp:688, 724`.

**Unsafe / error-prone.**
- `setTheme()` does not take ownership (`NQ/docs/styling.md:25`). The plot keeps a raw pointer to an object someone else may delete.
- SQP has to disconnect from the theme in the plot's destructor. Otherwise the theme, as a child, dies after the inner widget and its `destroyed` handler calls a deleted widget (`SciQLopPlot.cpp:1034-1043`).

---

## D. Colour maps

### 10. Add a colour map with a colour scale, aligned by a margin group

**Frequency.** docs: 6 `new QCPColorScale`, 11 `QCPMarginGroup(`. examples: 1 + 3. SQP: one scale per plot, created hidden (`SciQLopPlot.cpp:136-147`). It is shown or hidden by moving it in and out of the layout (`SciQLopPlot.cpp:682-736`). Two `_ensure_colorscale_is_visible` overloads link maps to it (`SciQLopPlot.cpp:738-748`). 36 `QCPColorScale` mentions in SQP.

**Excerpt.** `NQ/docs/snippets/colormaps.cpp:24-34`
```cpp
    auto* scale = new QCPColorScale(customPlot);
    customPlot->plotLayout()->addElement(0, 1, scale);   // right of the axis rect
    scale->setLabel("amplitude");
    map->setColorScale(scale);
    map->setGradient(QCPColorGradient::gpViridis);
    map->rescaleDataRange();

    // Keep the colour bar exactly as tall as the axis rect.
    auto* margins = new QCPMarginGroup(customPlot);
    customPlot->axisRect()->setMarginGroup(QCP::msTop | QCP::msBottom, margins);
    scale->setMarginGroup(QCP::msTop | QCP::msBottom, margins);
```
Showing it later means finding the axis rect's row. `SQP/src/SciQLopPlot.cpp:682-701`
```cpp
void _impl::SciQLopPlot::show_color_scale()
{
    if (m_color_scale->visible())
        return;
    m_color_scale->setVisible(true);
    plotLayout()->addElement(axis_rect_row(), 1, m_color_scale);
    applyTheme();
}

int _impl::SciQLopPlot::axis_rect_row() const
{
    for (int i = 0; i < plotLayout()->elementCount(); ++i)
        if (plotLayout()->elementAt(i) == axisRect())
        {
            int row = 0, column = 0;
            plotLayout()->indexToRowCol(i, row, column);
            return row;
        }
    return 0;
}
```

**Boilerplate.**
- Five objects to wire: map, scale, layout cell, margin group, axis rect.
- The margin sides are passed twice and must match.
- The user chooses the scale's grid cell by hand, by row and column.
- Hiding it means `take()` + `simplify()` + `replot()` (`SciQLopPlot.cpp:728-736`).
- The scale's log setting and its ticker are again two calls (`colormaps.cpp:93-96`).

**Unsafe / error-prone.**
- The margin group's default 6 px minimum margins leak to every plot in the group. SQP zeroes them (`SciQLopPlot.cpp:137-140`). A memory note records two regressions from this.
- The scale and map lifetimes are linked silently: "Deleting a QCPColorScale detaches the colour maps that use it, without a signal" (`NQ/docs/migrating.md:88`).
- SQP keeps `QPointer<QCPColorScale>` in three places: `SciQLopPlotAxis.hpp:412`, `SciQLopLineGraph.hpp:45`, `SciQLopTimeColoredCurve.hpp:51`.
- `take()` versus `remove()`: one deletes and the other does not (`NQ/docs/layout.md:123-124`). Mixing them up is a crash or a leak.

---

## E. Layout

### 11. Layout: extra axis rects, titles, insets

**Frequency.** docs: 15 `new QCPAxisRect`, 31 `addElement`. examples: 4 + 11. SQP: the title (`SciQLopPlot.cpp:703-726`), the lane layout for timelines (`SciQLopPlot.cpp:1151-1156`), and `plotLayout()` 15 times.

**Excerpt.** `NQ/docs/snippets/performance.cpp:77-87`
```cpp
    auto* detailRect = new QCPAxisRect(customPlot);
    customPlot->plotLayout()->addElement(1, 0, detailRect);
    // A new axis rect lands on the "main" layer: move it, so a pan can translate "main".
    detailRect->setLayer("background");
    for (QCPAxis* axis : detailRect->axes())
    {
        axis->setLayer("axes");
        axis->grid()->setLayer("grid");
    }
    auto* detail = new QCPGraph2(detailRect->axis(QCPAxis::atBottom),
                                 detailRect->axis(QCPAxis::atLeft));
```
A title in SQP. `SQP/src/SciQLopPlot.cpp:705-724`
```cpp
    if (title.isEmpty())
    {
        if (m_title)
        {
            plotLayout()->remove(m_title);
            plotLayout()->simplify();
        }
    }
    else if (m_title)
        m_title->setText(title);
    else
    {
        plotLayout()->insertRow(0);
        QFont font = this->font();
        font.setBold(true);
        font.setPointSizeF(font.pointSizeF() * 1.2);
        m_title = new QCPTextElement(this, title, font);
        plotLayout()->addElement(0, 0, m_title);
        applyTheme();
    }
```

**Boilerplate.**
- Create the element with `new X(plot)`, then insert it into the layout. These are two steps and both name the plot.
- `insertRow(0)` before adding a title shifts every existing row index.
- Every new axis rect must be moved off the "main" layer by hand. Otherwise pan caching breaks (`performance.cpp:79-85`, repeated at `performance.cpp:161-167`).
- A title toggle needs remove + simplify + a QPointer to remember it (`SciQLopPlot.hpp:57`).

**Unsafe / error-prone.**
- Hard-coded row and column numbers break as soon as rows are inserted.
- The layer fix-up is a silent performance and correctness trap. Nothing warns when it is missing.
- `remove()` deletes the element. `m_title` survives only because it is a `QPointer` (`SciQLopPlot.hpp:57`).

### 12. Customise and place the legend (incl. legend groups)

**Frequency.** `legend->`: 35 in docs, 32 in examples. SQP: 217 mentions of "legend". `SciQLopPlotLegend.cpp` reaches the inset slot 4 times.

**Excerpt.** `NQ/docs/snippets/layout.cpp:46-52`
```cpp
    customPlot->legend->setVisible(true);
    customPlot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignBottom | Qt::AlignLeft);

    customPlot->legend->insertRow(0);
    auto* legendTitle = new QCPTextElement(customPlot, "Signals", QFont("sans", 9, QFont::Bold));
    legendTitle->setLayer(customPlot->legend->layer()); // else the legend background hides it
    customPlot->legend->addElement(0, 0, legendTitle);
```
SQP searches for the legend's slot every time. `SQP/src/SciQLopPlotLegend.cpp:48-56, 72-81`
```cpp
std::pair<QCPLayoutInset*, int> inset_slot(QCPLegend* legend)
{
    auto* inset = qobject_cast<QCPLayoutInset*>(legend->layout());
    if (inset)
        for (int i = 0; i < inset->elementCount(); ++i)
            if (inset->elementAt(i) == legend)
                return { inset, i };
    return { nullptr, -1 };
}
...
    inset->setInsetPlacement(index, QCPLayoutInset::ipFree);
    // A zero size: the inset layout grows it to the legend's minimum, its natural size.
    inset->setInsetRect(index, QRectF(position, QSizeF(0., 0.)));
    m_legend->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
```

**Boilerplate.**
- Placement lives on the inset layout, addressed by the legend's index (`setInsetAlignment(0, ...)`), not on the legend itself.
- A legend title needs its layer copied from the legend by hand.
- Moving the legend outside the rect takes a new grid + `addElement` + a stretch factor of 0.001 (`layout.cpp:77-82`).

**Unsafe / error-prone.**
- Index 0 is assumed to be the legend. Any other inset element breaks that.
- `customPlot->legend` is a raw public pointer. It dies with the first axis rect (`NQ/docs/layout.md:129`).
- SQP stores it as a raw `QCPLegend* m_legend` (`SQP/include/SciQLopPlots/SciQLopPlotLegend.hpp:32`), with no guard.
- Legend items are re-created when a plottable's legend flag changes. SQP must re-connect to the new items each time (`SciQLopGraphInterface.cpp:66-92`, `SciQLopGraphComponent.cpp:67-78`).

---

## F. Items

### 13. Annotate with items and anchor them (text, line, tracer, bracket)

**Frequency.** `new QCPItem`: 40 in docs, 16 in examples. SQP: the crosshair (3 items, `SciQLopCrosshair.cpp:94-124`), projection markers (`SciQLopNDProjectionPlot.cpp:477`), item wrappers (Text, Pixmap, Shapes, StraightLines, Spans). `setCoords` 33, `setType*` 25.

**Excerpt.** `NQ/docs/snippets/items.cpp:59-71`
```cpp
    auto* tracer = new QCPItemTracer(customPlot);
    tracer->setGraph(graph);
    tracer->setGraphKey(12.5);
    tracer->setInterpolating(true);
    tracer->setStyle(QCPItemTracer::tsCircle);
    tracer->setPen(QPen(QColor(214, 39, 40), 2));
    tracer->setBrush(Qt::white);
    tracer->setSize(8);
    auto* value = new QCPItemText(customPlot);
    value->position->setParentAnchor(tracer->position);
    value->position->setCoords(10, -6); // 10 px right, 6 px up
    value->setPositionAlignment(Qt::AlignLeft | Qt::AlignBottom);
    value->setText("tracer at x = 12.5");
```
The axis-tags example builds one compound item from three, with an invisible tracer as anchor. `NQ/examples/axis-tags/axistag.cpp:34-40, 67-75`
```cpp
  mDummyTracer = new QCPItemTracer(mAxis->parentPlot());
  mDummyTracer->setVisible(false);
  mDummyTracer->position->setTypeX(QCPItemPosition::ptAxisRectRatio);
  mDummyTracer->position->setTypeY(QCPItemPosition::ptPlotCoords);
  mDummyTracer->position->setAxisRect(mAxis->axisRect());
  mDummyTracer->position->setAxes(0, mAxis);
  mDummyTracer->position->setCoords(1, 0);
...
AxisTag::~AxisTag()
{
  if (mDummyTracer)
    mDummyTracer->parentPlot()->removeItem(mDummyTracer);
  if (mArrow)
    mArrow->parentPlot()->removeItem(mArrow);
  if (mLabel)
    mLabel->parentPlot()->removeItem(mLabel);
}
```

**Boilerplate.**
- One setter per property, after a bare `new`.
- Every crosshair item repeats the same four "decorative" calls: layer, not selectable, mouse-transparent, hidden (`SciQLopCrosshair.cpp:97-122`).
- A position's meaning (data, ratio or pixels) is set by `setType` / `setTypeX` / `setTypeY` and the axes, separately from its coordinates.
- A straight vertical line needs two points (`point1`, `point2`) with an arbitrary y (`items.cpp:172-174`, `performance.cpp:117-118`).

**Unsafe / error-prone.**
- Calling `setType` and `setCoords` in the wrong order moves the item. `setType` keeps the old pixel position (`SQP/src/SpanTargets.cpp:169-171`, comment).
- The `position` anchors are raw public members (`label->position->...`).
- The axis-tags example keeps three `QPointer`s and removes each item by hand (`axistag.h:54-56`).
- SQP keeps the crosshair items as raw pointers (`SQP/include/SciQLopPlots/Items/SciQLopCrosshair.hpp:31-34`) and the projection markers as `QList<QCPItemTracer*>` (`SciQLopNDProjectionPlot.hpp:77`). They are safe only while nobody calls `clearItems()`.
- Items have no tooltip. SQP multiply-inherits a tooltip mixin into each item and `dynamic_cast`s on hover (`SpanTargets.cpp:36-45`, `SciQLopPlot.cpp:615-628`).

### 14. Spans and draggable items, with change signals

**Frequency.** docs: `items.cpp:200-237`. SQP: `SpanTargets.cpp` (V/H/R spans), `LineTargets.cpp`, `MultiPlotsVSpan.cpp`, and `SciQLopPlotItem<T>` (a base for movable items with its own mouse handling, `SciQLopPlotItem.hpp:60-131`).

**Excerpt.** `NQ/docs/snippets/items.cpp:213-234`
```cpp
    auto* event = new QCPItemVSpan(customPlot);
    event->setRange(QCPRange(20, 35));
    event->setPen(Qt::NoPen);
    event->setBrush(QColor(255, 127, 14, 50));
    event->setBorderPen(QPen(QColor(255, 127, 14), 2));
    ...
    QObject::connect(event, &QCPItemVSpan::rangeChanged, customPlot,
                     [](const QCPRange& range) { qDebug() << "event:" << range.lower << range.upper; });
```
Three span types, three signal names, one `qobject_cast` chain. `SQP/src/SpanTargets.cpp:184-192`
```cpp
    if (auto* v = qobject_cast<QCPItemVSpan*>(m_item.data()))
        QObject::connect(v, &QCPItemVSpan::rangeChanged, span, notify(SpanAxis::Key));
    else if (auto* h = qobject_cast<QCPItemHSpan*>(m_item.data()))
        QObject::connect(h, &QCPItemHSpan::rangeChanged, span, notify(SpanAxis::Value));
    else if (auto* r = qobject_cast<QCPItemRSpan*>(m_item.data()))
    {
        QObject::connect(r, &QCPItemRSpan::keyRangeChanged, span, notify(SpanAxis::Key));
        QObject::connect(r, &QCPItemRSpan::valueRangeChanged, span, notify(SpanAxis::Value));
    }
```

**Boilerplate.**
- The abstract span base has no common range API. A second `qobject_cast` chain sets the range (`SpanTargets.cpp:139-153`).
- SQP's movable-item base re-implements press and move, and coalesces replots with its own `QTimer::singleShot(0, ...)` flag (`SciQLopPlotItem.hpp:80-99`).

**Unsafe / error-prone.**
- A signal from a drag may reach a slot that deletes the item mid-handler. So SQP checks `QPointer self` after `move()` (`SciQLopPlotItem.hpp:109-121`).
- The span wrapper keeps `SpanBase* m_base; // m_item seen as a SpanBase: use it only while m_item is alive` (`SpanTargets.hpp:85`): a raw pointer paired with the `QPointer`.
- Delete and Backspace only emit `deleteRequested`, and nothing is deleted (`NQ/docs/migrating.md:83`). Every client must wire the actual removal.

### 15. Let the user draw a new item (creation mode)

**Frequency.** docs: `items.cpp:239-265`. SQP: `SciQLopMultiPlotPanel.cpp:816-930`, which draws the span on one plot and previews it on every other plot in the panel.

**Excerpt.** `NQ/docs/snippets/items.cpp:245-258`
```cpp
    customPlot->setItemCreator([](QCustomPlot* plot, QCPAxis*, QCPAxis*) -> QCPAbstractItem*
    {
        auto* span = new QCPItemVSpan(plot);
        span->setPen(Qt::NoPen);
        span->setBrush(QColor(255, 127, 14, 50));
        span->setBorderPen(QPen(QColor(255, 127, 14), 2));
        return span;
    });
    customPlot->setItemPositioner([](QCPAbstractItem* item, double anchorKey, double,
                                     double currentKey, double)
    {
        static_cast<QCPItemVSpan*>(item)->setRange(QCPRange(anchorKey, currentKey));
    });
    customPlot->setCreationModeEnabled(true); // or hold Shift on the first click
```
SQP throws the created item away and builds its own. `SQP/src/SciQLopMultiPlotPanel.cpp:906-918`
```cpp
void SciQLopMultiPlotPanel::_on_item_created(QCustomPlot* qcp, QCPAbstractItem* item)
{
    auto* vspan = qobject_cast<QCPItemVSpan*>(item);
    if (!vspan)
        return;

    auto qcp_range = vspan->range();
    SciQLopPlotRange range(qcp_range.lower, qcp_range.upper);

    (void)qcp->removeItem(vspan);
    _clear_preview_spans();

    auto* mpvspan
        = new MultiPlotsVerticalSpan(this, range, m_span_creation_color, false, true, "");
```

**Boilerplate.**
- Two callbacks plus a signal for one gesture.
- The positioner gets a base pointer and has to `static_cast` it back.
- SQP keeps `QList<QPointer<QCPItemVSpan>> preview_spans` (`SciQLopMultiPlotPanel.hpp:70`) and a set of plots to replot (`SciQLopMultiPlotPanel.cpp:852-862, 889-904`).
- SQP stores per-plot connections so it can uninstall them (`SciQLopMultiPlotPanel.cpp:866-886`).

**Unsafe / error-prone.**
- `static_cast<QCPItemVSpan*>(item)` in the positioner is unchecked (`items.cpp:256`).
- The plot creates the item, SQP removes it, and SQP re-creates its own. A slot connected to `itemCreated` that keeps the pointer would dangle.
- Preview items live on other plots. Each must be null-checked before use (`SciQLopMultiPlotPanel.cpp:855, 893`).

### 16. Layers (custom layer for a fast cursor, ordering, buffered mode)

**Frequency.** docs: 9 `setLayer`, `addLayer` in `concepts.cpp:70`, `performance.cpp:111`. SQP: 3 `addLayer`, 9 `setLayer`, 27 `layer(`.

**Excerpt.** `NQ/docs/snippets/performance.cpp:111-125`
```cpp
    if (customPlot->addLayer("cursor"))
        customPlot->layer("cursor")->setMode(QCPLayer::lmBuffered);

    auto* cursor = new QCPItemStraightLine(customPlot);
    cursor->setLayer("cursor");
    ...
                         customPlot->layer("cursor")->replot(); // only this layer is redrawn
```
SQP, at plot construction. `SQP/src/SciQLopPlot.cpp:102-107`
```cpp
    (void)this->addLayer(LayersNames::Spans, this->layer(LayersNames::Main), QCustomPlot::limAbove);
    this->layer(LayersNames::Spans)->setMode(QCPLayer::lmBuffered);
    this->layer(LayersNames::Spans)->setVisible(true);
    (void)this->addLayer(LayersNames::ColorMap, this->layer(LayersNames::Main), QCustomPlot::limBelow);
    this->layer(LayersNames::ColorMap)->setMode(QCPLayer::lmBuffered);
    this->layer(LayersNames::ColorMap)->setVisible(true);
```

**Boilerplate.**
- Add, then look up by name again, then set the mode. Three steps per layer.
- `(void)` casts are needed on `[[nodiscard]]` calls whose result nobody needs.
- The projection plot repeats the same dance per pane (`SciQLopNDProjectionPlot.cpp:471-473`).

**Unsafe / error-prone.**
- Layers are addressed by string. A typo gives `nullptr` and then a crash on `->setMode`.
- `addLayer` returns false if the layer exists. The docs guard it, SQP ignores it.
- Items and axis rects default to whatever layer is current. A forgotten `setLayer` breaks pan caching (use case 11).

---

## G. Interaction

### 17. Configure drag, zoom and wheel behaviour

**Frequency.** docs: 7 `setInteractions`. examples: 15. SQP: 1 `setInteractions` (`SciQLopPlot.cpp:118-120`), but `wheelEvent` is fully re-implemented (`SciQLopPlot.cpp:450-501`), along with pinch and native gestures (`SciQLopPlot.cpp:390-416`).

**Excerpt.** `NQ/docs/snippets/interactions.cpp:20-24`
```cpp
    customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    QCPAxisRect* rect = customPlot->axisRect();
    rect->setRangeDrag(Qt::Horizontal); // pan along x only
    rect->setRangeZoom(Qt::Horizontal); // zoom along x only
    rect->setRangeZoomFactor(0.7);      // stronger zoom per wheel step than the default 0.85
```
SQP replaced the wheel handling for speed. `SQP/src/SciQLopPlot.cpp:456-468`
```cpp
    // Check axes directly — avoids layerableListAt() which triggers expensive
    // selectTest() on every plottable (O(N) per data point in QCPGraph2).
    for (auto axisType : { QCPAxis::atBottom, QCPAxis::atLeft, QCPAxis::atTop, QCPAxis::atRight })
    {
        auto* ax = ar->axis(axisType);
        if (ax && ax->selectTest(pos, false) >= 0)
        {
            this->_wheel_zoom(ax, wheelSteps, pos);
            event->accept();
            this->replot(rpQueuedReplot);
            return;
        }
    }
```

**Boilerplate.**
- Interactions are set on the plot, but drag and zoom directions are set on the axis rect.
- Per-axis wheel zoom, modifier-based pan versus zoom, and colour-scale zoom are all hand-written (`SciQLopPlot.cpp:470-498`).
- Mode switching on a modifier is done in a `mousePress` slot (`interactions.cpp:235-238`).

**Unsafe / error-prone.**
- The default event path is O(points) per wheel event. That forced the override above.
- SQP throttles hover handling to 60 Hz itself (`SciQLopPlot.cpp:353-357`).
- The example's "only drag the selected axis" logic runs inside `mousePress`/`mouseWheel` slots that read selection state (`NQ/examples/interactions/mainwindow.cpp:39-40, 154-170`).

### 18. Selection, click signals and hit testing (incl. legend–plottable sync)

**Frequency.** docs: `interactions.cpp:60-187, 247-263`. examples: interactions (whole example). SQP: 18 `selectionChanged`, Escape-to-deselect (`SciQLopPlot.cpp:503-531`), `itemAt` / `plottableAt` for the cursor and tooltip (`SciQLopPlot.cpp:597-628`), and legend selection relays (`SciQLopGraphInterface.cpp:76-84`, `SciQLopGraphComponent.cpp:67-78`).

**Excerpt.** `NQ/examples/interactions/mainwindow.cpp:142-152`
```cpp
  // synchronize selection of graphs with selection of corresponding legend items:
  for (int i=0; i<ui->customPlot->graphCount(); ++i)
  {
    QCPGraph *graph = ui->customPlot->graph(i);
    QCPPlottableLegendItem *item = ui->customPlot->legend->itemWithPlottable(graph);
    if (item->selected() || graph->selected())
    {
      item->setSelected(true);
      graph->setSelection(QCPDataSelection(graph->data()->dataRange()));
    }
  }
```
"Deselect everything" in SQP. `SQP/src/SciQLopPlot.cpp:505-529`
```cpp
        auto items = selectedItems();
        for (auto item : items)
        {
            item->setSelected(false);
        }
        for (auto g : selectedGraphs())
        {
            g->setSelection(QCPDataSelection());
        }
        for (auto l : selectedLegends())
        {
            l->setSelectedParts(QCPLegend::SelectablePart::spNone);
        }
        for (auto a : selectedPlottables())
        {
            a->setSelection(QCPDataSelection());
        }
        for (auto ax : selectedAxes())
        {
            ax->setSelectedParts(QCPAxis::spNone);
        }
```

**Boilerplate.**
- Selection is spread over five object kinds, each with its own API (`setSelected`, `setSelection`, `setSelectedParts` x2).
- A plottable and its legend row are selected separately and must be synced by hand.
- The selection signal is overloaded (`bool` and `QCPDataSelection`), so every connect needs `QOverload<bool>::of(...)` (`SciQLopGraphComponent.cpp:41, 56`; `SciQLopTimeline.cpp:131`).

**Unsafe / error-prone.**
- `itemWithPlottable` returns a raw legend item. That item is re-created when the legend rebuilds, which forces SQP to reconnect after every `set_in_legend` (`SciQLopGraphInterface.cpp:66-75`).
- `item->selected()` in the example dereferences without a null check (`mainwindow.cpp:147`).
- Hover hit-testing runs `itemAt` + `plottableAt` + `dynamic_cast` to SQP mixins on every mouse move (`SciQLopPlot.cpp:597-611`).

---

## H. Several plots

### 19. Synchronise axes and margins across plots

**Frequency.** docs: `axes.cpp:177-213`, `layout.cpp:121-149`. examples: interactions (`xAxis` mirrored to `xAxis2`), axis-tags, scrollbar. SQP: `AxisSynchronizer`, `XAxisSynchronizer`, `TimeAxisSynchronizer`, and `VPlotsAlign` (manual margin alignment, `VPlotsAlign.cpp:29-85`).

**Excerpt.** `NQ/docs/snippets/axes.cpp:186-194`
```cpp
    QObject::connect(upperX, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
                     lowerX, QOverload<const QCPRange&>::of(&QCPAxis::setRange));
    QObject::connect(lowerX, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
                     upperX, QOverload<const QCPRange&>::of(&QCPAxis::setRange));

    // Line the two axis rects up on the left and right.
    auto* margins = new QCPMarginGroup(customPlot);
    customPlot->axisRect(0)->setMarginGroup(QCP::msLeft | QCP::msRight, margins);
    lowerRect->setMarginGroup(QCP::msLeft | QCP::msRight, margins);
```
Across separate widgets a margin group cannot be used, so SQP computes margins itself. `SQP/src/VPlotsAlign.cpp:70-76`
```cpp
            auto new_margins
                = QMargins(max_left_margin, ar->calculateAutoMargin(QCP::MarginSide::msTop),
                           new_right_margin, ar->calculateAutoMargin(QCP::MarginSide::msBottom));
            if (ar->minimumMargins() != new_margins)
            {
                ar->setMinimumMargins(new_margins);
                any_changed = true;
            }
```

**Boilerplate.**
- Two-way linking takes two connects, with the overloaded-signal cast on both ends.
- The scrollbar example guards against a feedback loop with a 0.01 tolerance and comments "we don't want to replot twice" (`NQ/examples/scrollbar-axis-range-control/mainwindow.cpp:83-95`).
- SQP's `VPlotsAlign` runs a 16 ms debounce timer, then two passes over all plots, then replots them all (`VPlotsAlign.cpp:29-96`).

**Unsafe / error-prone.**
- Two-way connects only stop looping because `setRange` ignores an equal range. That is an implicit rule.
- A margin group only works inside one `QCustomPlot`. Stacked separate widgets need hand-rolled margin math, which depends on the colour scale's `outerRect()` and on column spacing (`VPlotsAlign.cpp:42-68`).
- SQP adds a `m_suppress_range_signals` flag to the plot (`SciQLopPlot.cpp:121-133`) and to the axis (`SciQLopPlotAxis.cpp:70`) to break re-emission.

---

## I. Export

### 20. Export to PNG, PDF, a pixmap or a painter

**Frequency.** docs: 11. examples: 1 (text-document-integration, `qcpdocumentobject.cpp:67-69`). SQP: `savePdf` / `savePng` (`SciQLopPlot.cpp:1751, 1762`), `toPainter` / `toPixmap` (`SciQLopPlot.cpp:1831-1845`), and its own multi-plot exporter (`SQP/src/Export/SciQLopExportable.cpp`).

**Excerpt.** `NQ/docs/snippets/export.cpp:113-120`
```cpp
    QImage figure(1200, 360, QImage::Format_ARGB32_Premultiplied);
    figure.fill(Qt::white);
    QCPPainter painter(&figure);
    customPlot->toPainter(&painter, 600, 360); // left panel: everything
    painter.translate(600, 0);
    customPlot->xAxis->setRange(4, 6);
    customPlot->toPainter(&painter, 600, 360); // right panel: a zoom
    painter.end();
```
SQP. `SQP/src/SciQLopPlot.cpp:1840-1843`
```cpp
    if (kind == SciQLopExportTarget::Vector)
        qcp->toPainter(static_cast<QCPPainter*>(painter), target.width(), target.height());
    else
        painter->drawPixmap(0, 0, qcp->toPixmap(target.width(), target.height()));
```

**Boilerplate.**
- Save functions return `bool` with no reason given. SQP wraps each one in `save_or_throw` (`SciQLopPlot.cpp:1751, 1762`).
- Long positional argument lists, for example `savePng(file, 600, 360, 1.0, -1, 300)` (`export.cpp:74`).
- `toPainter` requires a `QCPPainter`, not a `QPainter`.

**Unsafe / error-prone.**
- `static_cast<QCPPainter*>(painter)` on a `QPainter*` parameter (`SciQLopPlot.cpp:1841`). It is correct only because every caller happens to build a `QCPPainter` (`SciQLopExportable.cpp:122, 152, 178`). A plain `QPainter` from any other caller is undefined behaviour.
- Positional `int`/`double` arguments (width, height, scale, quality, resolution) are easy to swap.

---

## J. Removal and teardown

### 21. Remove a plottable or an item

**Frequency.** docs: rule stated in `concepts.md:33-35`, `items.md:9`. examples: 5 (`removeItem` in axis-tags, `removeGraph` in interactions). SQP: 7 `removePlottable`, 4 `removeItem`, 5 `hasPlottable`, plus `qcp_item_remove` used by every item wrapper.

**Excerpt.** `SQP/src/SciQLopColorMap.cpp:33-36, 45, 60-69`
```cpp
void SciQLopColorMap::_cmap_got_destroyed()
{
    _cmap = nullptr;
}
...
    connect(_cmap, &QCPColorMap2::destroyed, this, &SciQLopColorMap::_cmap_got_destroyed);
...
SciQLopColorMap::~SciQLopColorMap()
{
    if (_cmap)
    {
        auto* plot = _plot();
        auto* cmap = _cmap.data();
        _cmap = nullptr;
        if (plot && plot->hasPlottable(cmap))
            (void)plot->removePlottable(cmap);
    }
}
```
The shared item helper. `SQP/include/SciQLopPlots/helpers.hpp:51-67`
```cpp
/* Detach a QCP item from its plot, for an item wrapper's destructor.
   Without this the wrapper goes away and leaves its item painted on the plot,
   unreachable -- so deleteLater() could not remove it. A queued replot takes the
   item off the screen at once. Null-safe both ways: the QPointer clears if the
   plot destroyed the item first. */
inline auto qcp_item_remove(auto&& ptr) -> void
{
    qptr_apply(ptr,
               [](auto&& item)
               {
                   if (auto* plot = item->parentPlot())
                   {
                       (void)plot->removeItem(item);
                       plot->replot(QCustomPlot::rpQueuedReplot);
                   }
               });
}
```

**Boilerplate.**
- Every wrapper repeats the same five steps: QPointer member, a `destroyed` connection or null check, `parentPlot()`, `hasPlottable`, `removePlottable`. Repeated at `SciQLopMultiGraphBase.cpp:40-50`, `SciQLopGraphComponent.cpp:88-104`, `SciQLopHistogram.cpp:140`, `SciQLopHistogram2D.cpp:118`, `SciQLopTimeline.cpp:182-189`, `SciQLopPlotGroup.cpp:61-66`.
- The caller must queue a replot after removal, or the item stays on screen.
- `(void)` on every `[[nodiscard]]` remove.

**Unsafe / error-prone.**
- There are three ways to remove, and the rules differ:
  - "never delete them yourself" (`concepts.md:34`);
  - `removePlottable`, which deletes;
  - layout `remove`, which deletes, versus `take`, which does not.
- SQP itself breaks rule one: `delete plottable(0)` (`SciQLopPlot.cpp:1052`) and `delete hist` (`SciQLopPlot.cpp:87`).
- `parentPlot()` returns a raw pointer. SQP calls `_intervals->parentPlot()->removePlottable(...)` with no plot null check (`SciQLopTimeline.cpp:188`). The same happens in `SciQLopPlotGroup.cpp:65`.
- `SciQLopGraphComponent.cpp:98` needs `dynamic_cast<QCustomPlot*>(m_plottable->parentPlot())` to get a plot it can remove from.

### 22. Tear down a plot

**Frequency.** SQP: `SciQLopPlot.cpp:166-174` (the inner widget) and `SciQLopPlot.cpp:1032-1054` (the outer widget). Also `QCPAbstractPlottableWrapper.hpp:84` and `SciQLopTimeline.cpp:182-189`. docs and examples: nothing (the widget deletes its children).

**Excerpt.** `SQP/src/SciQLopPlot.cpp:1032-1054`
```cpp
SciQLopPlot::~SciQLopPlot()
{
    // A theme parented to this plot dies with its children, after m_impl: its destroyed()
    // handler (set_theme) would then call into a deleted widget. Qt only drops our
    // connections in ~QObject, too late for that.
    if (m_theme)
    {
        disconnect(m_theme, nullptr, this, nullptr);
        if (m_theme->qcp_theme())
            disconnect(m_theme->qcp_theme(), nullptr, this, nullptr);
        m_theme->detach(this);
    }
    // Deleting a timeline changes the lane layout; reacting to it here would report a natural
    // height to a container that may itself be mid-destruction.
    if (m_lane_layout)
        disconnect(m_lane_layout, nullptr, this, nullptr);
    disconnect(m_impl, &QCustomPlot::afterLayout, this, &SciQLopPlot::_update_timeline_geometry);
    m_curve_scale->quiesce();
    while (plottables().size() > 0)
    {
        delete plottable(0);
    }
}
```

**Boilerplate.**
- Disconnect from every NeoQCP signal source before the children die.
- Delete plottables one by one in a loop.

**Unsafe / error-prone.**
- NeoQCP emits during destruction. For example, removing intervals emits the lane layout's `changed` (`SciQLopTimeline.cpp:184-186`). Any connected slot then runs on a half-destroyed object.
- SQP adds a `m_dying` flag set from `destroyed`, plus a `quiesce()` call, just to stay out of half-destroyed children (`ColorScaleController.cpp:19-22`, `SciQLopPlot.cpp:1049`).
- The wrapper must detach components before deleting any, because a multigraph shrinks its own array mid-loop (`QCPAbstractPlottableWrapper.hpp:47-58`).

---

## K. Unusual patterns in SciQLopPlots

### 23. Wrap NeoQCP objects in long-lived handles (QPointer + "apply if alive")

**Frequency.** SQP: 19 `QPointer<QCP...>` members, 62 `qptr_apply*` calls, 46 `destroyed` mentions, and about 15 wrapper classes (axis, legend, overlay, theme, text/pixmap/shape/line/span items, colour map, histograms, timeline, components).

**Excerpt.** `SQP/include/SciQLopPlots/Items/SciQLopTextItem.hpp:89-120`
```cpp
    QPointer<impl::TextItem> m_item;

public:
    SciQLopTextItem(SciQLopPlot* plot, const QString& text, const QPointF& position,
                    bool movable = false, Coordinates coordinates = Coordinates::Pixels)
            : SciQLopBoundingRectItemInterface { SciQLopPlots::item_plot(plot, "SciQLopTextItem") }
    {
        m_item = new impl::TextItem(plot->qcp_plot(), text, position, movable, coordinates);
        _follow_selection(m_item);
    }

    virtual ~SciQLopTextItem() { qcp_item_remove(m_item); }

    [[nodiscard]] inline QString tool_tip() const noexcept override
    {
        return qptr_apply_or(m_item, [](auto&& item) { return item->tooltip(); });
    }

    inline void set_tool_tip(const QString& tooltip) noexcept override
    {
        SCIQLOP_ON_OWNER_THREAD(set_tool_tip(tooltip));
        qptr_apply(m_item, [tooltip](auto&& item) { item->setToolTip(tooltip); });
    }
```

**Boilerplate.**
- Every getter and setter is a lambda wrapped in `qptr_apply`.
- Every mutator starts with a thread hop.
- Every setter ends with a replot request.
- The NeoQCP item itself is subclassed (`impl::TextItem : SciQLopPlotItem<QCPItemText>`) to add movability, a tooltip and replot coalescing (`SciQLopTextItem.hpp:38-79`, `SciQLopPlotItem.hpp:60-131`).

**Unsafe / error-prone.**
- The handle and the NeoQCP object have separate lifetimes. The plot can delete the item (`clearItems`, layout `clear`, plot destruction) while the handle lives on. Every call must then become a silent no-op.
- NeoQCP objects are GUI-thread-only, with no check. SQP's 205 `SCIQLOP_ON_OWNER_THREAD` hops are the only thing that stops worker-thread calls (`SQP/include/SciQLopPlots/ThreadGuard.hpp`).
- A slot may delete its sender (allowed since NeoQCP, `migrating.md:85`). So any NeoQCP setter that emits is followed by an aliveness check: 85 sites (see the guard list below).

---

## Top 5: most boilerplate-heavy or unsafe today

1. **Remove a plottable or item (21).** There are three removal rules (never delete / `removePlottable` deletes / `take` keeps). Each wrapper needs a QPointer, a `destroyed` relay, a `hasPlottable` check, `(void)removePlottable` and a queued replot, repeated in 7+ classes. `parentPlot()` is used unchecked.
2. **Replace data from foreign buffers (5).** The user picks one of several source classes by memory layout and dtype. The lifetime guard is optional. Two incompatible guard styles exist (an extra argument versus an aliasing `shared_ptr`). Forgetting the guard means a worker thread reads freed memory.
3. **Long-lived wrappers / handles (23).** Every NeoQCP object reference is a `QPointer` plus `qptr_apply` (62 sites), plus a thread hop (205 sites), plus an aliveness check after any emitting call (85 sites). This is the client defending itself against implicit ownership and re-entrant signals.
4. **Colour map + colour scale + margin group (10).** Five objects, margin sides passed twice, manual layout cells. Showing or hiding is a row search + `addElement` / `take` + `simplify`. The scale-map link breaks silently on delete, and the margin group's default minimums leak to every plot.
5. **Interactive creation and spans (15 + 14).** Two callbacks plus a signal. Unchecked `static_cast` in the positioner. Create, then remove, then re-create in SQP. Each span type has its own range API and signal names behind a `qobject_cast` chain. Preview items need QPointer lists and per-plot replot sets.

Runner-up: the teardown ordering in plot destruction (22), and legend–plottable selection sync (12, 18).

## Guard patterns SciQLopPlots added around NeoQCP

Each one marks a place where the API made the client defend itself.

1. **QPointer to every NeoQCP object, with "apply if alive" helpers.**
   - Members: `SciQLopColorMap.hpp:39`, `SciQLopPlotAxis.hpp:325, 412`, `SciQLopPlotItem.hpp:149`, `SpanTargets.hpp:84`, `SciQLopTimeline.hpp:45-46`, `SciQLopHistogram.hpp:38`, `SciQLopOverlay.hpp:35`, `SciQLopTheme.hpp:12`.
   - Helpers: `helpers.hpp:28-50`, with 62 uses.
2. **`destroyed` → null our pointer.** `SciQLopColorMap.cpp:33-36, 45`. Also `QCPAbstractPlottableWrapper.cpp:65`, `SciQLopPlotGroup.cpp:116`, `SciQLopPlot.cpp:665, 1420`.
3. **`hasPlottable` before `removePlottable`.** `SciQLopColorMap.cpp:66`, `SciQLopMultiGraphBase.cpp:46`, `SciQLopGraphComponent.cpp:98-102`.
4. **Remove + queued replot helper for item wrappers.** `helpers.hpp:56-67` (`qcp_item_remove`), used e.g. at `SciQLopTextItem.hpp:100` and `SpanTargets.cpp:197`.
5. **"Am I still alive?" checks after any emitting NeoQCP call (85 sites).**
   - `SciQLopColorMap.cpp:158-166, 175, 198-201`.
   - `SciQLopPlotItem.hpp:111-118`.
   - `survives()` in `SciQLopPlotAxis.hpp:338-344` (used at `SciQLopPlotAxis.cpp:103, 240`).
   - `_survives()` in `SciQLopTimeline.cpp:174-179, 201`.
   - `QCPAbstractPlottableWrapper.hpp:139-140`.
6. **Disconnect before teardown, because NeoQCP emits while being destroyed.** `SciQLopTimeline.cpp:182-189`, `SciQLopPlot.cpp:1037-1049`, `SciQLopPlot.cpp:166-174`, `QCPAbstractPlottableWrapper.hpp:47-58`.
7. **Dying flag / `quiesce()`.** `ColorScaleController.cpp:19-22`, `ColorScaleController.hpp:80`, `SciQLopPlot.cpp:1049`.
8. **Snapshot a list before a loop whose body may emit.** `QCPAbstractPlottableWrapper.hpp:139-140` (`const auto parts = m_components;`) and `_drop_components_from` (`QCPAbstractPlottableWrapper.hpp:49-58`).
9. **Signal blocking around compound changes that emit mid-way.** `SciQLopPlotAxis.cpp:137` (scale type + ticker), `SpanTargets.cpp:166` (type + coordinates), `SciQLopPlot.cpp:1826`.
10. **Re-emission suppression flags.** `m_suppress_range_signals` in `SciQLopPlot.cpp:121-133` and `SciQLopPlotAxis.cpp:70`.
11. **Own replot coalescing per item.** `SciQLopPlotItem.hpp:80-99` (a `QTimer::singleShot(0)` flag), on top of 54 `rpQueuedReplot` calls.
12. **Re-wire legend items after NeoQCP re-creates them.** `SciQLopGraphInterface.cpp:66-92`, `SciQLopGraphComponent.cpp:67-78`.
13. **Thread hops to the GUI thread (205 sites).** `SCIQLOP_ON_OWNER_THREAD`, e.g. `SciQLopColorMap.cpp:145`. Also deferred `QMetaObject::invokeMethod` at `SciQLopGraphInterface.cpp:110-125`.
14. **Input validation NeoQCP lacks.** INT_MAX sizes, a 1-column colour map rendering blank, and an empty 2-D source asserting: `SciQLopColorMap.cpp:110-131, 150-166`.
15. **Delete a half-built object on failure.** `SciQLopPlot.cpp:79-90` (`set_bins_log_or_delete`).
16. **Event-handler overrides to dodge O(points) hit-testing.** `SciQLopPlot.cpp:450-501` (wheel) and `SciQLopPlot.cpp:353-357` (hover throttle).
17. **Margin-group side effect undone by hand.** `SciQLopPlot.cpp:137-140` (zeroed minimum margins).
