# M4a — Signal emission inventory (`src/`)

Read-only inventory of every signal emission statement in `src/` (`emit` and its `Q_EMIT`
spelling — `Q_EMIT` is not matched by `grep -rn 'emit '` and is listed here so the inventory is
complete). Comment-only mentions of "emit" are excluded.

## Count summary

| Metric | Count |
|---|---|
| Emission statements total | **120** |
| — spelled `emit` | 104 |
| — spelled `Q_EMIT` | 16 |
| Files containing emissions | 28 |
| Classes emitting | 29 |
| **Already inside a `QCustomPlot::deliver(...)` lambda** | **17** |
| — of which `Q_EMIT` | 0 |
| — transitively inside one (slot of a delivered signal) | 1 (`QCPPlottableGroup::doubleClicked`) |
| Not wrapped in `deliver(...)` | 103 |
| Emitters with **no `QCustomPlot` reachable** | 22 sites / 3 classes (`QCPTheme` 16, `QCPLaneLayout` 1, `QCPAsyncPipelineBase` 5) |
| Sites **callable from a non-GUI thread** | 3 (`async-pipeline.cpp:52`, `:77`, `:146`) |
| `selectionChanged` / `selectableChanged` **declarations** in headers | 20 |

`QCustomPlot::deliver(QObject* sender, std::function<void()> emission)` — `src/core.cpp:2801`:

```cpp
void QCustomPlot::deliver(QObject* sender, std::function<void()> emission)
{
    if (!mEmissions.isOpen())
        return emission();
    mEmissions.push(sender, std::move(emission));
}
```

Deferred emission is queued on `sender` and flushed by the emission scope; outside a scope (or
with no plot) it runs inline.

Legend for the **Plot** column: `yes` = the emitter holds or reaches a `QCustomPlot` (so `deliver`
is reachable); `no` = no plot pointer in the class at all.

Legend for the **Thread** column: `GUI` = GUI thread only by contract; `any` = may run on the
caller's thread (see notes).

---

## QCPAxis — `src/axis/axis.cpp`

| file:line | emitter | signal (args) | deliver? | Plot | Thread |
|---|---|---|---|---|---|
| axis.cpp:639 | `QCPAxis::setScaleType` | `scaleTypeChanged(QCPAxis::ScaleType)` | no | yes | GUI |
| axis.cpp:663 | `QCPAxis::rangeDidChange` | `rangeChanged(const QCPRange& newRange)` | no | yes | GUI |
| axis.cpp:665 | `QCPAxis::rangeDidChange` | `rangeChanged(const QCPRange& newRange, const QCPRange& oldRange)` | no | yes | GUI |
| axis.cpp:727 | `QCPAxis::setSelectableParts` | `selectableChanged(const QCPAxis::SelectableParts&)` | no | yes | GUI |
| axis.cpp:755 | `QCPAxis::setSelectedParts` | `selectionChanged(const QCPAxis::SelectableParts&)` | no | yes | GUI |

All five guard with `QPointer` where a slot may delete the axis (663/755); none is delivered.

## QCPPolarAxisAngular — `src/polar/layoutelement-angularaxis.cpp`

| file:line | emitter | signal (args) | deliver? | Plot | Thread |
|---|---|---|---|---|---|
| layoutelement-angularaxis.cpp:860 | `QCPPolarAxisAngular::setSelectableParts` | `selectableChanged(const QCPPolarAxisAngular::SelectableParts&)` | no | yes | GUI |
| layoutelement-angularaxis.cpp:888 | `QCPPolarAxisAngular::setSelectedParts` | `selectionChanged(const QCPPolarAxisAngular::SelectableParts&)` | no | yes | GUI |
| layoutelement-angularaxis.cpp:1892 | `QCPPolarAxisAngular::emitRangeChanged` | `rangeChanged(const QCPRange& newRange)` | no | yes | GUI |
| layoutelement-angularaxis.cpp:1894 | `QCPPolarAxisAngular::emitRangeChanged` | `rangeChanged(const QCPRange& newRange, const QCPRange& oldRange)` | no | yes | GUI |

## QCPPolarAxisRadial — `src/polar/radialaxis.cpp`

| file:line | emitter | signal (args) | deliver? | Plot | Thread |
|---|---|---|---|---|---|
| radialaxis.cpp:289 | `QCPPolarAxisRadial::setScaleType` | `scaleTypeChanged(QCPPolarAxisRadial::ScaleType)` | no | yes | GUI |
| radialaxis.cpp:336 | `QCPPolarAxisRadial::setSelectableParts` | `selectableChanged(const QCPPolarAxisRadial::SelectableParts&)` | no | yes | GUI |
| radialaxis.cpp:364 | `QCPPolarAxisRadial::setSelectedParts` | `selectionChanged(const QCPPolarAxisRadial::SelectableParts&)` | no | yes | GUI |
| radialaxis.cpp:1631 | `QCPPolarAxisRadial::emitRangeChanged` | `rangeChanged(const QCPRange& newRange)` | no | yes | GUI |
| radialaxis.cpp:1633 | `QCPPolarAxisRadial::emitRangeChanged` | `rangeChanged(const QCPRange& newRange, const QCPRange& oldRange)` | no | yes | GUI |

## QCPAbstractPlottable — `src/plottables/plottable.cpp`

| file:line | emitter | signal (args) | deliver? | Plot | Thread |
|---|---|---|---|---|---|
| plottable.cpp:679 | `QCPAbstractPlottable::emitSelectionChanged` | `selectionChanged(bool selected)` | no | yes | GUI |
| plottable.cpp:681 | `QCPAbstractPlottable::emitSelectionChanged` | `selectionChanged(const QCPDataSelection& selection)` | no | yes | GUI |
| plottable.cpp:729 | `QCPAbstractPlottable::setSelectable` | `selectableChanged(QCP::SelectionType)` | no | yes | GUI |
| plottable.cpp:1217 | `QCPAbstractPlottable::updateEffectiveBusy` | `busyChanged(bool busy)` | no | yes | GUI |
| plottable.cpp:1231 | `QCPAbstractPlottable::onDebounceTimeout` | `visuallyBusyChanged(bool visuallyBusy)` | no | yes | GUI |

The two busy signals fire from the plottable's own debounce timer / effect update; the pipeline
signals that drive them are auto-connected across threads, so the slot (and therefore these
emits) runs on the plottable's thread.

## QCPPolarGraph — `src/polar/polargraph.cpp`

| file:line | emitter | signal (args) | deliver? | Plot | Thread |
|---|---|---|---|---|---|
| polargraph.cpp:332 | `QCPPolarGraph::setSelectable` | `selectableChanged(QCP::SelectionType)` | no | yes | GUI |
| polargraph.cpp:375 | `QCPPolarGraph::emitSelectionChanged` | `selectionChanged(bool selected)` | no | yes | GUI |
| polargraph.cpp:377 | `QCPPolarGraph::emitSelectionChanged` | `selectionChanged(const QCPDataSelection& selection)` | no | yes | GUI |

## QCPAbstractItem — `src/items/item.cpp`

| file:line | emitter | signal (args) | deliver? | Plot | Thread |
|---|---|---|---|---|---|
| item.cpp:1347 | `QCPAbstractItem::setSelectable` | `selectableChanged(bool selectable)` | no | yes | GUI |
| item.cpp:1372 | `QCPAbstractItem::setSelected` | `selectionChanged(bool selected)` | no | yes | GUI |

## QCPItemHSpan — `src/items/item-hspan.cpp`

| file:line | emitter | signal (args) | deliver? | Plot | Thread |
|---|---|---|---|---|---|
| item-hspan.cpp:48 | `QCPItemHSpan::setRange` | `rangeChanged(const QCPRange& newRange)` | no | yes | GUI |
| item-hspan.cpp:176 | `QCPItemHSpan::mouseMoveEvent` | `rangeChanged(const QCPRange& newRange)` | no | yes | GUI |

## QCPItemVSpan — `src/items/item-vspan.cpp`

| file:line | emitter | signal (args) | deliver? | Plot | Thread |
|---|---|---|---|---|---|
| item-vspan.cpp:48 | `QCPItemVSpan::setRange` | `rangeChanged(const QCPRange& newRange)` | no | yes | GUI |
| item-vspan.cpp:176 | `QCPItemVSpan::mouseMoveEvent` | `rangeChanged(const QCPRange& newRange)` | no | yes | GUI |

## QCPItemRSpan — `src/items/item-rspan.cpp`

| file:line | emitter | signal (args) | deliver? | Plot | Thread |
|---|---|---|---|---|---|
| item-rspan.cpp:63 | `QCPItemRSpan::setKeyRange` | `keyRangeChanged(const QCPRange& newRange)` | no | yes | GUI |
| item-rspan.cpp:76 | `QCPItemRSpan::setValueRange` | `valueRangeChanged(const QCPRange& newRange)` | no | yes | GUI |
| item-rspan.cpp:276 | `QCPItemRSpan::emitDraggedRanges` | `keyRangeChanged(const QCPRange& newRange)` | no | yes | GUI |
| item-rspan.cpp:278 | `QCPItemRSpan::emitDraggedRanges` | `valueRangeChanged(const QCPRange& newRange)` | no | yes | GUI |

276/278 are `QPointer`-guarded ("a slot may delete this span: emit last, and the value range only
while it is alive").

## QCPItemCreationState — `src/item-creation-state.cpp`

| file:line | emitter | signal (args) | deliver? | Plot | Thread |
|---|---|---|---|---|---|
| item-creation-state.cpp:136 | `QCPItemCreationState::commitItem` | `itemCreated(QCPAbstractItem* item)` | no | yes (`mPlot`) | GUI |
| item-creation-state.cpp:151 | `QCPItemCreationState::cancelItem` | `itemCanceled()` | no | yes (`mPlot`) | GUI |

`QCustomPlot` merely forwards these to its own `itemCreated` / `itemCanceled` via `connect`
(`core.cpp:1173-1176`); the plot's own signals are never `emit`-ed directly.

## QCPTextElement — `src/layoutelements/layoutelement-textelement.cpp`

| file:line | emitter | signal (args) | deliver? | Plot | Thread |
|---|---|---|---|---|---|
| layoutelement-textelement.cpp:306 | `QCPTextElement::setSelectable` | `selectableChanged(bool selectable)` | no | yes | GUI |
| layoutelement-textelement.cpp:323 | `QCPTextElement::setSelected` | `selectionChanged(bool selected)` | no | yes | GUI |
| layoutelement-textelement.cpp:438 | `QCPTextElement::mouseReleaseEvent` | `clicked(QMouseEvent* event)` | no | yes | GUI |
| layoutelement-textelement.cpp:449 | `QCPTextElement::mouseDoubleClickEvent` | `doubleClicked(QMouseEvent* event)` | no | yes | GUI |

## QCPColorScale — `src/layoutelements/layoutelement-colorscale.cpp`

| file:line | emitter | signal (args) | deliver? | Plot | Thread |
|---|---|---|---|---|---|
| layoutelement-colorscale.cpp:270 | `QCPColorScale::setDataRange` | `dataRangeChanged(const QCPRange& newRange)` | no | yes | GUI |
| layoutelement-colorscale.cpp:306 | `QCPColorScale::setDataScaleType` | `dataScaleTypeChanged(QCPAxis::ScaleType)` | no | yes | GUI |
| layoutelement-colorscale.cpp:327 | `QCPColorScale::setGradient` | `gradientChanged(const QCPColorGradient& newGradient)` | no | yes | GUI |

## QCPAbstractLegendItem — `src/layoutelements/layoutelement-legend.cpp`

| file:line | emitter | signal (args) | deliver? | Plot | Thread |
|---|---|---|---|---|---|
| layoutelement-legend.cpp:159 | `QCPAbstractLegendItem::setSelectable` | `selectableChanged(bool selectable)` | no | yes | GUI |
| layoutelement-legend.cpp:177 | `QCPAbstractLegendItem::setSelected` | `selectionChanged(bool selected)` | no | yes | GUI |

## QCPLegend — `src/layoutelements/layoutelement-legend.cpp`

| file:line | emitter | signal (args) | deliver? | Plot | Thread |
|---|---|---|---|---|---|
| layoutelement-legend.cpp:664 | `QCPLegend::setSelectableParts` | `selectableChanged(QCPLegend::SelectableParts parts)` | no | yes | GUI |
| layoutelement-legend.cpp:727 | `QCPLegend::setSelectedParts` | `selectionChanged(QCPLegend::SelectableParts parts)` | no | yes | GUI |

## QCPGroupLegendItem — `src/layoutelements/layoutelement-legend-group.cpp`

| file:line | emitter | signal (args) | deliver? | Plot | Thread |
|---|---|---|---|---|---|
| layoutelement-legend-group.cpp:156 | `QCPGroupLegendItem::selectEvent` | `componentClicked(int componentIndex)` | no | yes | GUI |

## QCPOverlay — `src/overlay.cpp`

| file:line | emitter | signal (args) | deliver? | Plot | Thread |
|---|---|---|---|---|---|
| overlay.cpp:21 | `QCPOverlay::showMessage` | `messageChanged(const QString& text, QCPOverlay::Level level)` | no | yes | GUI |
| overlay.cpp:29 | `QCPOverlay::clearMessage` | `messageChanged(const QString& text, QCPOverlay::Level level)` | no | yes | GUI |
| overlay.cpp:40 | `QCPOverlay::setCollapsible` | `collapsedChanged(bool collapsed)` | no | yes | GUI |
| overlay.cpp:50 | `QCPOverlay::setCollapsed` | `collapsedChanged(bool collapsed)` | no | yes | GUI |

## QCPLaneLayout — `src/plottables/lane-layout.cpp`

| file:line | emitter | signal (args) | deliver? | Plot | **no plot** | Thread |
|---|---|---|---|---|---|---|
| lane-layout.cpp:187 | `QCPLaneLayout::flush` | `changed()` | no | **no** | — | GUI |

## QCPPlottableGroup — `src/plottables/plottable-group.cpp`

| file:line | emitter | signal (args) | deliver? | Plot | Thread |
|---|---|---|---|---|---|
| plottable-group.cpp:50 | lambda in ctor, connected to `QCustomPlot::legendDoubleClick` | `doubleClicked()` | **indirect** — runs inside a slot of a signal emitted from a `deliver` lambda | yes | GUI |
| plottable-group.cpp:108 | `QCPPlottableGroup::addMember` | `membersChanged()` | no | yes | GUI |
| plottable-group.cpp:111 | `QCPPlottableGroup::addMember` | `membersChanged()` | no | yes | GUI |
| plottable-group.cpp:121 | `QCPPlottableGroup::removeMember` | `membersChanged()` | no | yes | GUI |

## QCPColorMap2 — `src/plottables/plottable-colormap2.cpp`

| file:line | emitter | signal (args) | deliver? | Plot | Thread |
|---|---|---|---|---|---|
| plottable-colormap2.cpp:182 | `QCPColorMap2::setGradient` | `gradientChanged(const QCPColorGradient& newGradient)` | no | yes | GUI |
| plottable-colormap2.cpp:227 | `QCPColorMap2::setDataRange` | `dataRangeChanged(const QCPRange& newRange)` | no | yes | GUI |
| plottable-colormap2.cpp:246 | `QCPColorMap2::setDataScaleType` | `dataRangeChanged(const QCPRange& newRange)` | no | yes | GUI |
| plottable-colormap2.cpp:248 | `QCPColorMap2::setDataScaleType` | `dataScaleTypeChanged(QCPAxis::ScaleType newType)` | no | yes | GUI |

## QCPHistogram2D — `src/plottables/plottable-histogram2d.cpp`

| file:line | emitter | signal (args) | deliver? | Plot | Thread |
|---|---|---|---|---|---|
| plottable-histogram2d.cpp:174 | `QCPHistogram2D::setGradient` | `gradientChanged(const QCPColorGradient& newGradient)` | no | yes | GUI |
| plottable-histogram2d.cpp:219 | `QCPHistogram2D::setDataRange` | `dataRangeChanged(const QCPRange& newRange)` | no | yes | GUI |
| plottable-histogram2d.cpp:233 | `QCPHistogram2D::setDataScaleType` | `dataRangeChanged(const QCPRange& newRange)` | no | yes | GUI |
| plottable-histogram2d.cpp:235 | `QCPHistogram2D::setDataScaleType` | `dataScaleTypeChanged(QCPAxis::ScaleType newType)` | no | yes | GUI |

## QCPColorMap — `src/plottables/plottable-colormap.cpp`

| file:line | emitter | signal (args) | deliver? | Plot | Thread |
|---|---|---|---|---|---|
| plottable-colormap.cpp:830 | `QCPColorMap::setDataRange` | `dataRangeChanged(const QCPRange& newRange)` | no | yes | GUI |
| plottable-colormap.cpp:846 | `QCPColorMap::setDataScaleType` | `dataScaleTypeChanged(QCPAxis::ScaleType scaleType)` | no | yes | GUI |
| plottable-colormap.cpp:869 | `QCPColorMap::setGradient` | `gradientChanged(const QCPColorGradient& newGradient)` | no | yes | GUI |

## QCPHistogram — `src/plottables/plottable-histogram.cpp`

| file:line | emitter | signal (args) | deliver? | Plot | Thread |
|---|---|---|---|---|---|
| plottable-histogram.cpp:283 | `QCPHistogram::takeResult` | `edgesChanged()` | no | yes | GUI |
| plottable-histogram.cpp:285 | `QCPHistogram::takeResult` | `histogramReady()` | no | yes | GUI |

`takeResult` is called from `onPipelineFinished` — i.e. from the queued delivery of
`QCPAsyncPipelineBase::finished`, so still GUI.

## QCPIntervals — `src/plottables/plottable-intervals.cpp`

| file:line | emitter | signal (args) | deliver? | Plot | Thread |
|---|---|---|---|---|---|
| plottable-intervals.cpp:694 | `QCPIntervals::finishGesture` | `intervalCreated(double start, double stop, int lane, int category)` | no | yes | GUI |
| plottable-intervals.cpp:713 | `QCPIntervals::emitEdits` | `intervalsEdited(const QVector<QCPIntervalEdit>& edits)` | no | yes | GUI |
| plottable-intervals.cpp:735 | `QCPIntervals::keyPress` | `deleteRequested(const QVector<qint64>& ids)` | no | yes | GUI |

## QCPMultiGraph — `src/plottables/plottable-multigraph.cpp`

| file:line | emitter | signal (args) | deliver? | Plot | Thread |
|---|---|---|---|---|---|
| plottable-multigraph.cpp:575 | `QCPMultiGraph::setComponentInLegend` | `componentVisibilityChanged()` | no | yes | GUI |
| plottable-multigraph.cpp:597 | `QCPMultiGraph::setComponentVisible` | `componentVisibilityChanged()` | no | yes | GUI |

## QCPSelectionRect — `src/selectionrect.cpp`

| file:line | emitter | signal (args) | deliver? | Plot | Thread |
|---|---|---|---|---|---|
| selectionrect.cpp:195 | `QCPSelectionRect::cancelWith` | `canceled(const QRect& rect, QInputEvent* event)` | no | yes | GUI |
| selectionrect.cpp:210 | `QCPSelectionRect::startSelection` | `started(QMouseEvent* event)` | no | yes | GUI |
| selectionrect.cpp:223 | `QCPSelectionRect::moveSelection` | `changed(const QRect& rect, QMouseEvent* event)` | no | yes | GUI |
| selectionrect.cpp:238 | `QCPSelectionRect::endSelection` | `accepted(const QRect& rect, QMouseEvent* event)` | no | yes | GUI |

## QCPLayerable — `src/layer.cpp`

| file:line | emitter | signal (args) | deliver? | Plot | Thread |
|---|---|---|---|---|---|
| layer.cpp:652 | `QCPLayerable::setVisible` | `visibleChanged(bool visible)` | no | yes | GUI |
| layer.cpp:858 | `QCPLayerable::emitLayerChanged` | `layerChanged(QCPLayer* newLayer)` | **yes** — `plot->deliver(this, emission)`, inline when no plot | yes (cast from `mParentPlot`) | GUI |

## QCPTheme — `src/theme.cpp` (no plot)

| file:line | emitter | signal (args) | deliver? | Plot | Thread |
|---|---|---|---|---|---|
| theme.cpp:17 | `QCPTheme::setBackground` | `changed()` | no | **no** | GUI |
| theme.cpp:22 | `QCPTheme::setForeground` | `changed()` | no | **no** | GUI |
| theme.cpp:27 | `QCPTheme::setGrid` | `changed()` | no | **no** | GUI |
| theme.cpp:32 | `QCPTheme::setSubGrid` | `changed()` | no | **no** | GUI |
| theme.cpp:37 | `QCPTheme::setSelection` | `changed()` | no | **no** | GUI |
| theme.cpp:42 | `QCPTheme::setLegendBackground` | `changed()` | no | **no** | GUI |
| theme.cpp:47 | `QCPTheme::setLegendBorder` | `changed()` | no | **no** | GUI |
| theme.cpp:52 | `QCPTheme::setTooltipBackground` | `changed()` | no | **no** | GUI |
| theme.cpp:57 | `QCPTheme::setTooltipBorder` | `changed()` | no | **no** | GUI |
| theme.cpp:62 | `QCPTheme::setTooltipText` | `changed()` | no | **no** | GUI |
| theme.cpp:67 | `QCPTheme::setTooltipCornerRadius` | `changed()` | no | **no** | GUI |
| theme.cpp:72 | `QCPTheme::setCrosshairColor` | `changed()` | no | **no** | GUI |
| theme.cpp:77 | `QCPTheme::setBusyIndicatorSymbol` | `changed()` | no | **no** | GUI |
| theme.cpp:82 | `QCPTheme::setBusyFadeAlpha` | `changed()` | no | **no** | GUI |
| theme.cpp:87 | `QCPTheme::setBusyShowDelayMs` | `changed()` | no | **no** | GUI |
| theme.cpp:92 | `QCPTheme::setBusyHideDelayMs` | `changed()` | no | **no** | GUI |

`QCPTheme` is a bare `QObject` (`Q_PROPERTY ... NOTIFY changed`): no `QCustomPlot*` member, no
`parentPlot()`, and nothing in `theme.h`/`theme.cpp` mentions `QCustomPlot`. The 16 setters are the
only path to `changed()`, so a plot that wants a `deliver`-wrapped theme signal has to take the
theme at plot level or proxy it.

## QCPAsyncPipelineBase — `src/datasource/async-pipeline.cpp` (no plot)

| file:line | emitter | signal (args) | deliver? | Plot | **Thread** |
|---|---|---|---|---|---|
| async-pipeline.cpp:52 | `QCPAsyncPipelineBase::emitBusyIfNeeded` | `busyChanged(bool busy)` | no | **no** | **any** — caller of `onDataChanged` / `onViewportChanged` |
| async-pipeline.cpp:77 | `QCPAsyncPipelineBase::startJob` | `busyChanged(bool busy)` | no | **no** | **any** — same callers |
| async-pipeline.cpp:146 | `QCPAsyncPipelineBase::settleIdle` | `busyChanged(bool busy)` | no | **no** | **any** — same callers |
| async-pipeline.cpp:239 | `QCPAsyncPipelineBase::deliverResult` | `finished(uint64_t generation)` | no | **no** | pipeline's thread (queued metacall) |
| async-pipeline.cpp:262 | `QCPAsyncPipelineBase::deliverResult` | `busyChanged(bool busy)` | no | **no** | pipeline's thread (queued metacall) |

Reachability from a non-GUI thread, from `async-pipeline.h`:

- `50-52`: `setSource()` only takes the mutex, then calls `onDataChanged()` on the caller's thread.
- `73-75`: *"With data changed off the owner thread, a queued `busyChanged(true)` can land after
  `busyChanged(false)`: slots re-read `isBusy()` instead of trusting the argument."* — the header
  itself documents off-thread `busyChanged(true)`.
- `239`/`262` run inside `deliverResult`, reached only through `QMetaObject::invokeMethod(...,
  Qt::QueuedConnection)` from `makeJob`, so they run on the pipeline's own thread.

So three sites (52/77/146) can emit on a non-GUI thread, two of the same signal that the two
queued sites emit. All five have no `QCustomPlot` to deliver through; the owner plottable does.

## QCustomPlot — `src/core.cpp`

| file:line | emitter | signal (args) | deliver? | Plot | Thread |
|---|---|---|---|---|---|
| core.cpp:1499 | `QCustomPlot::detach` | `QCPPlottableGroup::membersChanged()` (via `group->`) | no | yes | GUI |
| core.cpp:1667 | `QCustomPlot::deletePlottable` | `QCPPlottableGroup::membersChanged()` (via `group->`) | no | yes | GUI |
| core.cpp:1703 | `QCustomPlot::plottableDestroyed` | `QCPPlottableGroup::membersChanged()` (via `group->`) | no | yes | GUI |
| core.cpp:3810 | `QCustomPlot::mouseDoubleClickEvent` | `mouseDoubleClick(QMouseEvent*)` | **yes** | yes | GUI |
| core.cpp:3854 | `QCustomPlot::mousePressEvent` | `mousePress(QMouseEvent*)` | **yes** | yes | GUI |
| core.cpp:3938 | `QCustomPlot::mouseMoveEvent` | `mouseMove(QMouseEvent*)` | **yes** | yes | GUI |
| core.cpp:3977 | `QCustomPlot::mouseReleaseEvent` | `mouseRelease(QMouseEvent*)` | **yes** | yes | GUI |
| core.cpp:4018 | `QCustomPlot::emitClickSignal` | `plottableClick(QCPAbstractPlottable*, int dataIndex, QMouseEvent*)` | **yes** (`deliver(ap, ...)`) | yes | GUI |
| core.cpp:4022 | `QCustomPlot::emitClickSignal` | `axisClick(QCPAxis*, QCPAxis::SelectablePart, QMouseEvent*)` | **yes** (`deliver(ax, ...)`) | yes | GUI |
| core.cpp:4025 | `QCustomPlot::emitClickSignal` | `itemClick(QCPAbstractItem*, QMouseEvent*)` | **yes** (`deliver(ai, ...)`) | yes | GUI |
| core.cpp:4027 | `QCustomPlot::emitClickSignal` | `legendClick(QCPLegend*, QCPAbstractLegendItem*, QMouseEvent*)` | **yes** (`deliver(lg, ...)`) | yes | GUI |
| core.cpp:4029 | `QCustomPlot::emitClickSignal` | `legendClick(QCPLegend*, QCPAbstractLegendItem*, QMouseEvent*)` | **yes** (`deliver(li, ...)`) | yes | GUI |
| core.cpp:4037 | `QCustomPlot::emitDoubleClickSignal` | `plottableDoubleClick(QCPAbstractPlottable*, int dataIndex, QMouseEvent*)` | **yes** (`deliver(ap, ...)`) | yes | GUI |
| core.cpp:4041 | `QCustomPlot::emitDoubleClickSignal` | `axisDoubleClick(QCPAxis*, QCPAxis::SelectablePart, QMouseEvent*)` | **yes** (`deliver(ax, ...)`) | yes | GUI |
| core.cpp:4044 | `QCustomPlot::emitDoubleClickSignal` | `itemDoubleClick(QCPAbstractItem*, QMouseEvent*)` | **yes** (`deliver(ai, ...)`) | yes | GUI |
| core.cpp:4046 | `QCustomPlot::emitDoubleClickSignal` | `legendDoubleClick(QCPLegend*, QCPAbstractLegendItem*, QMouseEvent*)` | **yes** (`deliver(lg, ...)`) | yes | GUI |
| core.cpp:4048 | `QCustomPlot::emitDoubleClickSignal` | `legendDoubleClick(QCPLegend*, QCPAbstractLegendItem*, QMouseEvent*)` | **yes** (`deliver(li, ...)`) | yes | GUI |
| core.cpp:4254 | `QCustomPlot::wheelEvent` | `mouseWheel(QWheelEvent*)` | **yes** | yes | GUI |
| core.cpp:4310 | `QCustomPlot::keyPressEvent` | `QCPAbstractItem::deleteRequested()` (via `item->`) | **yes** (`deliver(item, ...)`) | yes | GUI |
| core.cpp:4799 | `QCustomPlot::announceUserSelection` | `selectionChangedByUser()` | no | yes | GUI |

Note: `detach` / `deletePlottable` / `plottableDestroyed` emit **another object's** signal
(`group->membersChanged()`), un-delivered, from inside plot teardown paths.

## Related: `selectionChanged` / `selectableChanged` declarations

Every header declaration whose name is exactly `selectionChanged` or `selectableChanged`
(20 total):

| file:line | class | exact signature |
|---|---|---|
| axis.h:402 | `QCPAxis` | `void selectionChanged(const QCPAxis::SelectableParts& parts)` |
| axis.h:403 | `QCPAxis` | `void selectableChanged(const QCPAxis::SelectableParts& parts)` |
| items/item.h:288 | `QCPAbstractItem` | `void selectionChanged(bool selected)` |
| items/item.h:289 | `QCPAbstractItem` | `void selectableChanged(bool selectable)` |
| layoutelements/layoutelement-textelement.h:98 | `QCPTextElement` | `void selectionChanged(bool selected)` |
| layoutelements/layoutelement-textelement.h:99 | `QCPTextElement` | `void selectableChanged(bool selectable)` |
| layoutelements/layoutelement-legend.h:83 | `QCPAbstractLegendItem` | `void selectionChanged(bool selected)` |
| layoutelements/layoutelement-legend.h:84 | `QCPAbstractLegendItem` | `void selectableChanged(bool selectable)` |
| layoutelements/layoutelement-legend.h:255 | `QCPLegend` | `void selectionChanged(QCPLegend::SelectableParts parts)` |
| layoutelements/layoutelement-legend.h:256 | `QCPLegend` | `void selectableChanged(QCPLegend::SelectableParts parts)` |
| plottables/plottable.h:253 | `QCPAbstractPlottable` | `void selectionChanged(bool selected)` |
| plottables/plottable.h:254 | `QCPAbstractPlottable` | `void selectionChanged(const QCPDataSelection& selection)` |
| plottables/plottable.h:255 | `QCPAbstractPlottable` | `void selectableChanged(QCP::SelectionType selectable)` |
| polar/layoutelement-angularaxis.h:325 | `QCPPolarAxisAngular` | `void selectionChanged(const QCPPolarAxisAngular::SelectableParts& parts)` |
| polar/layoutelement-angularaxis.h:326 | `QCPPolarAxisAngular` | `void selectableChanged(const QCPPolarAxisAngular::SelectableParts& parts)` |
| polar/polargraph.h:182 | `QCPPolarGraph` | `void selectionChanged(bool selected)` |
| polar/polargraph.h:183 | `QCPPolarGraph` | `void selectionChanged(const QCPDataSelection& selection)` |
| polar/polargraph.h:184 | `QCPPolarGraph` | `void selectableChanged(QCP::SelectionType selectable)` |
| polar/radialaxis.h:261 | `QCPPolarAxisRadial` | `void selectionChanged(const QCPPolarAxisRadial::SelectableParts& parts)` |
| polar/radialaxis.h:262 | `QCPPolarAxisRadial` | `void selectableChanged(const QCPPolarAxisRadial::SelectableParts& parts)` |

Almost all of them are also `NOTIFY` targets of a `Q_PROPERTY` (every class above except
`QCPPolarGraph`'s `selectionChanged(const QCPDataSelection&)` and the second `selectionChanged`
overloads, which share one property). `QCPAbstractPlottable` overloads `selectionChanged` (bool +
`QCPDataSelection`), and `QCPPolarGraph` mirrors it.

Related but **not** matching (name is different): `core.h:492 void
QCustomPlot::selectionChangedByUser()`; `layoutelement-colorscale.h:61 Q_SLOT void
QCPColorScale::axisSelectableChanged(QCPAxis::SelectableParts selectableParts)`.

Incidental, signal-adjacent: `layoutelement-legend.h:48-49` swaps the two NOTIFY names
(`selectable` → `selectionChanged`, `selected` → `selectableChanged`), unlike
`items/item.h:224-225` and `layoutelement-textelement.h:48-49`, which pair them the other way.
