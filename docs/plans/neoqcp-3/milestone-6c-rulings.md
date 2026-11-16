# NeoQCP 3.0 — milestone 6c rulings and deferred items

Milestone 6c: the drawing context. A plot type or annotation draws with short, unit-explicit calls (`line`,
`circle`, `polygon`, `rect`, `roundedRect`, boxed `text`, `5_px`, `Data + PixelOffset`), and what it records later
is drawn over what it recorded earlier, text included, on screen and in an export. `QCPItemText` takes math text
on or off. Branch `neoqcp-3.0`, `5f53d34..` the commit that adds this file (not pushed). Plan:
`docs/superpowers/plans/2026-11-15-neoqcp-3-milestone-6c.md` (pre-flight rulings P-1..P-8). Implementer: opencode
(DeepSeek V4.1 Flash, auto mode); task, re-review and final reviews by Claude subagents.

## Results

- Offscreen 4195 passed, 0 failed, 153 skipped (Coder, at 72bef43). Whole auto-tests under Xvfb 4348/0/0. Whole
  ASan + UBSan suite 4195/0/153, no sanitizer report (leak checks off). It caught one use-after-free in the export's
  deferred markers (a deleted object's recording); the deferral now copies its marker commands.
- Local Qt 6.12 picture suite (Xvfb): every test binary OK, no reference picture moved after the five m4e pictures
  re-baselined in Task 3.
- clang `-fsyntax-only` clean on the 25 changed `.cpp` files; examples build; `check-sources.py`: the file lists
  match; guide snippets pass (`doc-snippets`).

## Rulings

- R-1 (P-4): call order by painter slices. Painter-only content recorded after GPU content on a layer goes into an
  output-sized image (a slice) composited above that GPU content, not one image per command. Plain layerables are
  ordered the same way. Cost: one output-sized upload per slice per repaint.
- R-2 (P-7): `QCPItemText::mathText` defaults to true (every existing text item keeps its look); `ax->text()` copies
  `QCPTextStyle::mathText`, whose default is false, so a default style gives a plain item.
- R-3: a `QCPItemSpan` or `QCPItemRSpan` edge given a `DataOffset` is refused through the misuse tier (one report
  per item, edge unchanged): a span edge is an axis value, a pixel nudge has no meaning there.
- R-4: `qcpResolvePosition` is guarded by a `static_assert` on the `QCP::Position` variant size, not
  `Q_UNREACHABLE`: a new alternative fails the build instead of being undefined behaviour.
- R-5: the Task 1 pan test asserts at `QCPRecordingCache::reuse`, not by reading back the framebuffer: Xvfb leaves
  the stall-shifted buffer unpainted.
- R-6: scatter markers are not "GPU content before" a slice; at most 8 slices per layer per frame (`kMaxSlices`);
  a slice texture's key is `-(1 + index)`; one slice is open at a time; a plain layerable opens one.
- R-7 (amendments A-D to the Task 3 design): call-order runs inside one recording; "GPU content before" counts only
  layers that share the paint buffer; the pan-reuse veto applies only to recordings that used a slice; only a GPU
  submission closes a slice.
- R-8: the intervals' lane-label layer is kept: `QCPIntervals::labelLayer()` is public API and carries lane names,
  selection and preview.
- R-9: no per-recording reuse flag: a recording that used a slice never keeps geometry, and one that goes back to
  the fast path builds and reuses again.
- R-10: an export never opens a slice and never reads the screen's GPU state; a layer knows whether it paints its
  own buffer.
- R-11: a slice takes the painter state of each child that draws into it (opacity, antialiasing, pmNonCosmetic,
  transform, then the clip last, in the transform's coordinates), inside one save/restore per child.
- R-12: the item index pads an item's box by the largest pixel offset among its positions, rather than leaving it
  unbounded: culling still works.
- R-13: a recording that would open more slices than the layer has left is replayed as one GPU submission followed
  by all its painter content in one slice: the object stays whole and on top, its inner interleaving is lost. With
  no slice left at all it paints whole into the buffer, with one warning per layer.
- R-14: an export draws a layer's markers after the rest of that layer's content, with each child's painter state,
  as the screen's scatter pass does.
- R-15: a pass trims its slice images to what it used; a pure `QCPLayout` container never opens a slice.

## Deferred

- Opening a slice lazily on a child's first painter call (a plain layerable that paints nothing still costs one
  slice upload per frame): needs the painter to report its first call; `simplify:` comment in `src/layer.cpp`.
- A GPU shape is not antialiased at a device pixel ratio of 2 (docs/limits.md); a recording interleaving GPU and
  painter content keeps no pan geometry (docs/performance.md).
- The `ADL` data adapters and C++23 (n11) come before 6f.
