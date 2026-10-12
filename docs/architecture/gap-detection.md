# Gap Detection

Scientific data often has gaps — missing timestamps, instrument outages, or non-contiguous data blocks. NeoQCP detects these gaps so they appear as blank regions rather than being interpolated across.

## The Problem

When data has gaps, naive rendering produces visual artifacts:

```
Source data:   ●  ●  ●  ●  ●              ●  ●  ●  ●  ●
                                    ↑
                              Data gap here

Without gap detection:   ●──●──●──●──●──────────●──●──●──●──●
                                        ↑
                              Line drawn across gap (wrong)

With gap detection:      ●──●──●──●──●          ●──●──●──●──●
                                        ↑
                              Blank region (correct)
```

For colormaps, the issue is similar: without gap detection, source cells on either side of a gap would bleed their fill into the gap region.

Gap detection is done by <a href="../api/classQCPGraph2.html">QCPGraph2</a>, <a href="../api/classQCPMultiGraph.html">QCPMultiGraph</a> (and so `QCPWaterfallGraph`) and <a href="../api/classQCPColorMap2.html">QCPColorMap2</a>. The classic plottables don't do it: `QCPGraph` breaks its line only at NaN values, as upstream QCustomPlot does.

## Algorithm: Local Neighbor-Based Detection

NeoQCP uses a **local reference** algorithm. For each pair of consecutive X values, the spacing (`dx`) is compared to the spacing of its immediate neighbors. A gap is declared when `dx` is significantly larger than the local reference.

```mermaid
flowchart TD
    A["For each consecutive pair<br/>i, i+1 in the source data"] --> B["Compute dx = x[i+1] - x[i]"]
    B --> C["Compute reference dx:<br/>min of left neighbor dx and right neighbor dx"]
    C --> D{"dx > threshold × refDx ?"}
    D -->|Yes| E["Mark as GAP"]
    D -->|No| F["Mark as NORMAL"]
```

### Reference Computation

For the pair `(i, i+1)`:

```
refDx = min(
    x[i] - x[i-1],       // left neighbor spacing (if i > 0)
    x[i+2] - x[i+1]      // right neighbor spacing (if i+2 < N)
)
```

At the edges of the data (first or last pair), only the available neighbor is used.

### Why Local Reference?

An earlier implementation used a **sequential** algorithm where the previous `dx` was carried forward as the reference. This caused a critical bug: when the viewport included a gap, the large gap distance contaminated the reference value for subsequent pairs, causing gaps to appear and disappear depending on the viewport position.

```
Sequential (broken):
    dx values:   1  1  1  100  1  1  1  100  1  1
    prevDx:      1  1  1   1  100  1  1   1  100  1
    gap?:        -  -  -   ✓   -  ✓  -   ✓   -  ✓   ← false positives!

Local neighbor (correct):
    dx values:   1  1  1  100  1  1  1  100  1  1
    refDx:       1  1  1   1   1  1  1   1   1  1
    gap?:        -  -  -   ✓   -  -  -   ✓   -  -   ← correct
```

The local algorithm only compares to immediate neighbors, so a gap's large distance never pollutes the reference for non-gap pairs.

## Implementation

### Colormap Gap Detection

In `qcp::algo2d::resample()`, the source range is extended by 1 column on each side as *context* for gap detection and fill spacing. This ensures that when zoomed in at a gap boundary (where only 2 visible columns straddle the gap), there are still neighbors available for the local-reference comparison.

```
// Extend range for gap detection context
int ctxBegin = max(0, xBegin - 1);
int ctxEnd = min(xSize, xEnd + 1);

// Gap detection uses the wider context range
std::vector<bool> gapBetween(ctxCount, false);
for (int i = 0; i < ctxCount - 1; ++i)
{
    double dx = src.xAt(ctxBegin + i + 1) - src.xAt(ctxBegin + i);
    double refDx = MAX;
    if (i > 0)
        refDx = min(refDx, src.xAt(ctxBegin + i) - src.xAt(ctxBegin + i - 1));
    if (i + 2 < ctxCount)
        refDx = min(refDx, src.xAt(ctxBegin + i + 2) - src.xAt(ctxBegin + i + 1));
    if (refDx < MAX && dx > gapThreshold * refDx)
        gapBetween[i] = true;
}

// Binning loop iterates only over the original [xBegin, xEnd) range,
// but uses context indices for gap lookups and neighbor distances.
```

Without the context extension, zooming in at a gap boundary produces only 2 visible columns with no neighbors. The gap goes undetected, the gap distance is treated as normal spacing, and data fills the entire viewport:

```
Zoomed in at gap boundary (no context):

  Visible columns:  ●(x=4)                          ●(x=10)
  Gap detection:    srcCount=2, no neighbors → no gap detected
  Fill:             ████████████████████████████████████████████████
                    x=4 fills left half    x=10 fills right half
                    → entire viewport colored (wrong!)

With context columns (x=3, x=11):

  Context:          ●(x=3) ●(x=4)          ●(x=10) ●(x=11)
  Gap detection:    dx(4→10)=6, refDx=min(1,1)=1, 6>1.5 → GAP
  Fill:                 ████                    ████
                    x=4 fills [3.5,4.5]    x=10 fills [9.5,10.5]
                    → gap region is transparent (correct!)
```

The `gapBetween` array is then used during binning to clamp fill regions:

```mermaid
flowchart LR
    subgraph "Normal (no gaps)"
        A1["Column i"] --- B1["Fill extends<br/>half neighbor distance<br/>in each direction"] --- C1["Column i+1"]
    end

    subgraph "Gap boundary"
        A2["Column i"] --- B2["Fill clamped<br/>at gap boundary"] -.-x C2["Gap"] -.-x D2["Fill clamped<br/>at gap boundary"] --- E2["Column j"]
    end
```

At a gap boundary:
- The column before the gap does not extend its fill rightward
- The column after the gap does not extend its fill leftward
- If a column has a gap on one side but not the other, its fill mirrors the available spacing

### 1D Graph Gap Detection

Line graphs use the same local-reference rule, in `qcp::algo::detectKeyGaps()`. It runs over the key range being drawn and marks each point that starts a new segment:

```
For each consecutive pair (i, i+1) in [begin, end):
    refDx = min(step(i-1), step(i+1))      // only the neighbours that exist
    if refDx exists and step(i) > threshold × refDx:
        gapBefore[i+1] = true
```

A few details differ from the colormap version:

- A repeated key (a zero step) draws a vertical edge. It says nothing about the sampling, so it is never used as a reference.
- Fewer than 3 points in range: no gap is detected.
- The result is kept in a small cache on the data source, keyed by the range and the threshold, so a redraw of the same range does not scan again.

Both pixel conversions honour the marks:

- `linesToPixels()` emits a NaN point before each marked point.
- `optimizedLineData()` closes the current pixel interval at a marked point, emits a NaN point, and starts a new interval there.

The NaN point breaks the polyline: the line extruder and `QPainter` both start a new segment after it.

NaN values in the data break the line too, when you could see the hole. `linesToPixels()`, used below 2 samples per pixel, breaks at every NaN. `optimizedLineData()` breaks when a NaN run is wider than a pixel. A narrower run cannot be seen, so the line joins the samples on either side.

### Resampled lines

For large data, lines are drawn from the min/max bins built by [adaptive sampling](adaptive-sampling.md). The bin keys say nothing about gaps. A bin narrower than the sampling step can hold no sample, and empty bins are dropped. On perfectly uniform data that leaves key steps of 3 to 5 times the normal one. Run on the bin keys, the detector broke such lines into dots.

So each bin carries the step ratio of the raw rows it came from: the same `dx / refDx` measure, from `keyStepRatio()`. The ratio follows the data from L1 to L2 (`gapRatio` in `BinResult` and `MultiColumnBinResult`). A dropped bin passes its ratio on to the next bin that is drawn. The line breaks before a bin whose ratio passes the threshold, whatever the bin layout.

A dropped bin that held only NaN rows is marked `+inf`. A run of them breaks the line when it is at least a pixel wide (`DroppedBins`, 4 L2 bins), the NaN rule of `optimizedLineData()`. With the threshold at 0 that NaN rule still applies.

## Threshold

The gap threshold is a multiplier, set per plottable with `setGapThreshold()`. Default: `1.5`.

- `gapThreshold = 1.5` → a gap is declared when `dx > 1.5 × refDx`
- `gapThreshold = 0` → gap detection disabled (no gaps)
- Higher values → more tolerant of irregular spacing

For uniformly spaced data, any value > 1.0 works. For mildly non-uniform data (e.g., timestamps with jitter), 1.5–2.0 is typical.

```cpp
colorMap->setGapThreshold(2.0);  // more tolerant
graph->setGapThreshold(0.0);     // disable gap detection
```

A graph applies its threshold to whichever source it draws, at draw time, so it survives data replacement. A colormap re-runs its resample when the threshold changes.

## Visual Effect

```
Source data with gap (threshold = 1.5):

  x:  0.0  1.0  2.0  3.0  4.0     10.0  11.0  12.0  13.0
  dx:   1    1    1    1    6        1     1     1

  refDx at gap:  min(1, 1) = 1
  dx / refDx = 6 / 1 = 6 > 1.5  →  GAP

  Colormap output:
  ┌──────────────────┐   ┌───────────────────┐
  │  data region 1   │   │  data region 2    │
  │  (filled bins)   │   │  (filled bins)    │
  └──────────────────┘   └───────────────────┘
                  gap (NaN / transparent)
```

## Key Files

| File | Role |
|---|---|
| `src/datasource/resample.cpp` | Gap detection in `resample()` function |
| `src/datasource/algorithms.h` | `detectKeyGaps()`, `keyStepRatio()`, `gapsFromStepRatios()`, `GapCache`, and their use in `linesToPixels()` / `optimizedLineData()` |
| `src/datasource/graph-resampler.h`, `resampled-multi-datasource.h` | The gap ratio of each bin, through L1 and L2; `DroppedBins` |
| `src/plottables/plottable-colormap2.h` | `setGapThreshold()` / `gapThreshold()` API |
| `src/plottables/plottable-graph2.h`, `plottable-multigraph.h` | `setGapThreshold()` / `gapThreshold()` API for lines |
