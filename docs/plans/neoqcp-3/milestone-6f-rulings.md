# NeoQCP 3.0 — milestone 6f rulings and deferred items

Milestone 6f: scales. One value type, `QCPScale` (linear, log, symlog), replaces the scale enum on axes, colour
scales and colour-mapped plottables; symlog maps, pans, zooms, resamples, colours and ticks in its own space;
`setScale` installs its scale's ticker; `ax->colorMap(x, y, z, options)` makes a colour map in one call. Branch
`neoqcp-3.0`, `e62f0db..` the commit that adds this file (not pushed). Plan:
`docs/superpowers/plans/2026-11-16-neoqcp-3-milestone-6f.md` (rev. 2, pre-flight C1..C14, M1..M9). Spec:
`docs/specs/2026-11-14-neoqcp-3-milestone-6-design.md`, gaps `m6-gaps.md` theme 2. Implementer: opencode (DeepSeek
V4.1 Flash, auto mode); task, re-review and final reviews by Claude subagents. The range also carries the 6c CI fix
`59c626a` (boxed text clipped to its box, not its rounded inner rect: macOS Intel inked 2 px outside).

## Results

- Coder (GCC 16): whole suite 4321 passed, 0 failed, 162 skipped (the skips need a QRhi). Whole ASan + UBSan
  suite 4321/0/162, no sanitizer report (leak checks off under ptrace). `build-fco` (UBSan float-cast-overflow,
  default abort) 4321/0/162, no report.
- Local Qt 6.12 under Xvfb: whole `auto-tests` 4471/0/0, no reference picture moved except the three re-baselined
  on purpose (R-14); offscreen 4309/0/162. clang `-fsyntax-only` on the 87 changed files: no new diagnostic.
  Examples build (seven); `check-sources.py`: the lists match; `doc-snippets` and the doc binaries pass.
- Benchmarks (Coder, -O3, A/B against the base tree): coordinate mapping and colour rows equal or faster (symlog
  mapping -10 %, `QCPScale_ToScale1M` -53 %, colour-map pan sequence -5..8 %); see Deferred for the L1 build row.
- Reviews: per-task gates found 1 Critical (log zoom replayed as a pan), 1 Critical (unbounded symlog ticks for a
  legal base) and 14 Important, all fixed with failing tests first; the final whole-branch review found 4
  Important and 8 Minor, all fixed (13 + 2 commits).

## Rulings

- R-1 (B1): the enum is replaced in one task (Task 2) with log arithmetic unchanged; symlog arithmetic lands in
  later tasks. No forwarders (decision 2): `ScaleType`, `setScaleType`, `scaleType`, `scaleTypeChanged`,
  `setDataScaleType`, `dataScaleType`, and the hooks `scaleTypeWillChange`/`scaleTypeDidChange` (now
  `scaleWillChange(const QCPScale&)`/`scaleDidChange()`) are gone.
- R-2: the axis hot block keeps a one-byte `mScaleKind` at the old slot; the full `mScale` lives outside it;
  `setScale` is the only writer of both.
- R-3 (C13): a log scale is defined on |v|; the sign rule and the off-range rule stay in the axis and the snapshot.
  `QCPScale::operator==` and `qHash` compare the four parameters only, never the derived caches.
- R-4 (C5): log pans reuse kept GPU geometry (`isTranslationOf` compares the log slope over the log span; linear and
  symlog the scale-space slope). A reused frame must equal a fresh replot; tests pin both.
- R-5: `moveRange(diff)` is an offset in data units on linear, in `toScale` units on symlog, a factor on log.
- R-6: flat-range expansion and `sanitized` on symlog use the linear rule, on axes, plottables and colour scales
  alike (final review M3).
- R-7 (C2, M6, M7): thinning, L1/L2 bins, the line-cache window, colour-map rows and columns, histograms and
  automatic contour levels are equal steps of scale space on symlog; log keeps its arithmetic bit for bit (its
  `toScale` is one ulp off `log10`). The L1 cache is keyed on the binning space (symlog with its parameters, else
  linear), so log <-> linear keeps the bins.
- R-8: `QCPColorMap2` samples its key axis in the key scale's space too (log and symlog), by the same mechanism as
  rows; a non-positive range falls back to linear; log ends and gap edges are full width (mirrored in log space).
- R-9: a key gap is a step wide in key units AND in the shown space (a sampling even in one space is uneven in the
  other, so neither alone can decide).
- R-10 (C12): the colour lookup dispatches its value space once per call (template), so the linear and log loops
  contain no call they did not have; a non-finite or out-of-range position takes an end level.
- R-11: the legacy `QCPColorMap` keeps its documented contract (cells equally spaced on screen between its
  corners) on log and symlog axes; its half-cell rule follows the scale.
- R-12 (C7): symlog major ticks follow matplotlib's `SymmetricalLogLocator` (literals from matplotlib 3.11.2);
  thinning uses the axis tick count (`setTickCount`), not matplotlib's fixed 15; sub ticks are integer mantissas for
  bases 3..10 over at most 20 decades, else a linear subdivision; inside the band a decade's sub grid; ±threshold
  marked; fewer than two visible decades fall back to even steps. All work is bounded for any legal scale.
- R-13 (C8): `setScale` installs `defaultTicker(scale)` on every accepted call, an equal scale included (no
  `scaleChanged` then); a refused scale installs nothing; a later `setTicker` wins. `defaultTicker` is virtual: the
  polar angular axis keeps its 30° ticker. `QCPColorScale::setDataScale` re-installs the bar ticker only when the
  scale changes.
- R-14 (C9): three reference pictures moved on purpose (log axes now get a log ticker): `errorbars-log`,
  `financial-log`, `statbox-log`. New: `symlog-single-rect`, `spectrogram-single-rect`. Its flat lowest row is real
  DC leakage of the test noise through the Hann window, as matplotlib shows.
- R-15 (C14): polar radial axes take symlog; the angular axis refuses a non-linear scale through the misuse tier.
- R-16 (C10, C11): `colorMap` copies through `qcp::toDoubles`, applies the options after the add through the map's
  setters, fits the range (`rescaleDataRange(true)`) unless one is given; a grid that does not fit (empty x or y,
  flat z of the wrong size, ragged columns, a column length other than y.size(), a count other than x.size()) is
  misuse with an empty handle. A second map on the same rect restyles the shared bar (documented).
- R-17: a scale change never replots synchronously; a small graph only marks its lines dirty.
- R-18: GCC's `-fsanitize=undefined` does not include float-cast-overflow; a `build-fco` tree on Coder runs the
  suite with it. It found one 6f cast and two older ones (`QCPVector2D::toPoint`), all clamped now.

## Deferred

- `QCPGraph_BuildL1Cache1M` (1M rows, -O3) measures a median 4.9 ms against 3.6-4.4 ms on the pre-6f tree, in
  noisy interleaved runs: the gap rule now reads the key scale per row (R-9). Measure again with more rounds and
  look at `foldRowStepRatios` if it holds.
- GPU pan speed before/after 6f is unmeasured: Coder has no QRhi and no BASE tree may be set up locally. The pan
  behaviour is pinned by recording-count and frame-equality tests.
- A negative log key range always re-records its window (slower, never wrong).
- `examples/user-code/*` (11 targets) do not compile yet: three need 6e's `setAxisRectGrid`, eight need other m6
  vocabulary (`QCPMouseEvent`, `QCPConnector`, `fillBetween`, `addInset`, key events, ...).
- A data scale chosen before the add keeps its scale while the joined bar shows its own (the same rule as a chosen gradient).
- `linkTo` does not copy the source's scale (only `setupFullAxesBox` does).
