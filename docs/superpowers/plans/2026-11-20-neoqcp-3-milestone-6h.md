# NeoQCP 3.0 milestone 6h (annotations) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The annotations target (`examples/user-code/annotations`, matplotlib's connectorstyle demo) compiles
unchanged, runs, and has a reference picture: one item, `QCPItemAnnotation`, draws a text and a connector from the
text's box to a target point, with a head, a shrink at both ends and five connector shapes (`QCPConnector`). This is
the last stage-2 item of milestone 6: after it, all eleven targets compile.

**Architecture:** `QCPConnector` is a value type (a `std::variant` of five small shape structs) whose `path(start,
end)` builds the connector in pixels with matplotlib's formulas (`ConnectionStyle.Arc3/Angle3/Angle/Arc/Bar`).
`qcp::trimConnector` cuts that path where it leaves the text box and again `shrink` pixels from each end, as
matplotlib's `_Base.__call__` does. `QCPItemAnnotation` reads both positions in pixels at draw time (so it reads the
transform and re-records when the view changes), records the trimmed connector, the head at its end, then the text
through the same `QCPDrawContext::text` call `QCPItemText` uses. `QCPAxisRect::annotation` makes one in a call.

**Tech Stack:** C++23, Qt 6.12 (`QPainterPath`), meson; QtTest auto-tests; reference pictures in
`tests/auto/test-reference-m4e`.

**Spec:** `docs/specs/2026-11-14-neoqcp-3-milestone-6-design.md` (target 11, "annotations with connectors"); gaps
`docs/plans/neoqcp-3/m6-gaps.md` (section 4 row "One annotation item", decision 15 on connector names); vocabulary
`docs/plans/neoqcp-3/m6-vocabulary.md` rows `QCPAxisRect::annotation`, `QCPAnnotationOptions`, `QCPConnector`.
matplotlib source used for the formulas: `matplotlib/patches.py` 3.11 (`ConnectionStyle`), `matplotlib/text.py`
(`Annotation.update_positions`).

## Global Constraints

- The target compiles **unchanged** from its committed text, except removing "Does not compile yet." from its first
  comment line.
- Names exactly as the vocabulary: `QCPAxisRect::annotation`, `QCPItemAnnotation`, `QCPAnnotationOptions`,
  `QCPConnector` with `arc`, `curve`, `elbow`, `arms`, `bracket` (decision 15: `arc` is matplotlib's `arc3`, `arms`
  is its `arc`, `elbow` its `angle`, `curve` its `angle3`, `bracket` its `bar`).
- No interim designs: no forwarders, no second way beside the new one. Additive milestone: nothing is deleted.
- Every rule from earlier milestones stands: signals through `qcpEmit`, a slot may delete the object or the plot,
  handles for plot-owned objects in and out of the API, misuse through `QCP_CHECK_OR_RETURN` (`src/misuse.h`)
  reported once at creation, small functions, `simplify:` notes on deliberate shortcuts.
- Reference scenes never use `std::*_distribution` (the 6d CI lesson).
- Failing test first for every change. Whole suite green locally under Xvfb and on Coder (`build` with examples,
  `build-asan`, `build-fco`); reference pictures drawn and checked locally (fonts). clang `-fsyntax-only` clean.
- `-j 4` locally, one build per build directory at a time, never `meson setup` in the local worktree, no build trees
  in /tmp. Explicit pathspecs; never commit `subprojects/.wraplock`, `subprojects/jkqtmathtext`, other untracked
  `subprojects/*`, `.superpowers`, `images/`, `plot.png`, `x/`, `qt.ini`. Never push.

## Pre-flight decisions (for the maintainer)

- **P-1 (where the connector starts):** at the centre of the text's box, cut where the path leaves the box (matplotlib:
  `relpos` (0.5, 0.5) and `patchA` = the text box). The box is the one the text draws, padding included. Empty text:
  the connector starts at `at`, uncut.
- **P-2 (angles and bends):** every formula is matplotlib's, in its y-up frame: pixels have y negated before the
  shape is built and after. So `curve(90, 0)` leaves upwards on screen, a positive `arc` bend bows to the left of the
  start→end direction, as in matplotlib's picture. Angles in degrees, counter-clockwise from +x (as `QLineF::angle`).
- **P-3 (shrink):** after the box cut, the path is cut again where it leaves a circle of radius `shrink` around each
  end (matplotlib's `shrinkA`/`shrinkB`, one value for both, as the vocabulary has it). Default `{}` = 0 px. A path
  shorter than the cuts draws no connector (the text still draws).
- **P-4 (shapes with no answer):** `curve` and `elbow` need the two direction lines to meet; parallel lines (matplotlib
  raises) give a straight connector. Start == end gives no connector. A non-finite position draws nothing.
- **P-5 (head):** the head sits at the target end, turned along the path's tangent there (as `QCPItemCurve`); its pen
  and brush come from `QCPLineEnding::penFor/brushFor(pen)`. No tail option (YAGNI; the vocabulary has none).
- **P-6 (draw order and look):** connector first, then the text over it (matplotlib draws the arrow patch, then the
  text). Text through `QCPTextStyle` exactly as `ax->text()` draws it. Selected look as the other built-in lines:
  `setSelectedPen` (default blue, 2 px) for the connector; the text keeps its style.
- **P-7 (clicks):** the item is selectable like other built-in items; its hit test is the nearer of the text box and
  the drawn connector polyline.

## Review Focus

1. A text box that already contains the target (label on top of its point): the box cut leaves nothing, so no
   connector draws and nothing crashes — `aTargetInsideTheBoxDrawsNoConnector` (Task 2).
2. A connector whose path leaves and re-enters the box (a strong `arc` bend, `arms` pointing back): the cut is at
   the first exit, as matplotlib's `split_path_inout` — `theCutIsAtTheFirstExit` (Task 2).
3. A target in data space and a label in pixels (mixed spaces), then a pan: the tip follows the data and the text stays
   put — `aPanMovesTheTipNotTheText` (Task 2).
4. A huge zoom that puts the target far outside the rect (1e12 px): no hang, no overflow, clipped like
   `QCPItemCurve` — `aFarTargetDrawsInBoundedTime` (Task 2).
5. `curve`/`elbow` with parallel directions, zero-length connectors — `parallelDirectionsGiveAStraightPath`,
   `aZeroLengthConnectorIsEmpty` (Task 1).

---

### Task 1: QCPConnector, the five shapes

**Files:**
- Create: `src/items/connector.h`, `src/items/connector.cpp`
- Modify: `meson.build`, `cmake/sources.cmake`, `src/qcp.h` (include the header)
- Test: `tests/auto/test-connector/test-connector.{h,cpp}`, register in `tests/auto/meson.build` and
  `tests/auto/autotest.cpp`

**Interfaces:**
- Produces:
  ```cpp
  class QCP_LIB_DECL QCPConnector {
  public:
      static QCPConnector arc(double bend = 0);
      static QCPConnector curve(double startAngle, double endAngle);
      static QCPConnector elbow(double startAngle, double endAngle, QCP::PixelLength radius = { });
      static QCPConnector arms(double startAngle, QCP::PixelLength startArm, double endAngle,
                               QCP::PixelLength endArm, QCP::PixelLength radius = { });
      static QCPConnector bracket(double fraction, std::optional<double> angle = { });
      QCPConnector() = default;                       // arc(0): straight
      // The connector from start (text end) to end (target end), in pixels (y down).
      [[nodiscard]] QPainterPath path(QPointF start, QPointF end) const;
      bool operator==(const QCPConnector&) const = default;
  private:
      struct Arc { double bend = 0; bool operator==(const Arc&) const = default; };
      struct Curve { double startAngle, endAngle; ... };
      struct Elbow { double startAngle, endAngle, radius; ... };
      struct Arms { double startAngle, startArm, endAngle, endArm, radius; ... };
      struct Bracket { double fraction; std::optional<double> angle; ... };
      std::variant<Arc, Curve, Elbow, Arms, Bracket> mShape;
  };
  ```
  `path` dispatches with `std::visit` to one small function per shape (one level of abstraction each).

- [ ] **Step 1: Write the failing tests** in `TestConnector`.

`connectorPathsMatchMatplotlib` (data-driven). Start `(200, 100)`, end `(50, 300)` in pixels. Each row's expected
path is built with `moveTo/lineTo/quadTo` from these numbers, computed with matplotlib 3.11 (`ConnectionStyle.connect`
on y-negated points, results negated back). `Q(c) Q(e)` is one `quadTo(c, e)`:

| connector | expected |
|---|---|
| `arc(0.3)` | M(200,100) Q(65,155)(50,300) |
| `arc(-0.3)` | M(200,100) Q(185,245)(50,300) |
| `curve(90, 0)` | M(200,100) Q(200,300)(50,300) |
| `elbow(-90, 180)` | M(200,100) L(200,300) L(50,300) |
| `elbow(-90, 180, 5_px)` | M(200,100) L(200,295) Q(200,300)(195,300) L(50,300) |
| `arms(-90, 30_px, 0, 30_px)` | M(200,100) L(200,130) Q(200,130)(200,130) L(80,300) Q(80,300)(80,300) L(50,300) |
| `arms(-90, 30_px, 0, 30_px, 5_px)` | M(200,100) L(200,125) Q(200,130)(197.1,134.1) L(82.88,295.9) Q(80,300)(75,300) L(50,300) |
| `arms(-90, 0_px, 0, 40_px)` | M(200,100) L(90,300) Q(90,300)(90,300) L(50,300) |
| `bracket(0.3)` | M(200,100) L(140,55) L(-10,255) L(50,300) |
| `bracket(-0.2, 180)` | M(200,100) L(200,150) L(50,150) L(50,300) |

Compare element count, then each element's type and point with a 0.05 px tolerance (`QPainterPath::elementAt`).
Plus:
- `arcZeroIsStraight`: `arc(0).path(a, b)` drawn points all lie on segment ab (check `pointAtPercent(0.5)`).
- `parallelDirectionsGiveAStraightPath`: `curve(90, 90)` and `elbow(90, 90)` from (0,0) to (100,0) give exactly
  M(0,0) L(100,0).
- `aZeroLengthConnectorIsEmpty`: every shape with start == end returns an empty path.
- `connectorsCompareEqual`: `arc(0.3) == arc(0.3)`, `arc(0.3) != arc(-0.3)`, `QCPConnector{} == arc(0)`,
  `bracket(0.3) != bracket(0.3, 0)`.

- [ ] **Step 2: Run, expect the build to fail** (no `QCPConnector`):
  `PATH=/home/jeandet/Qt/6.12.0/gcc_64/bin:$PATH ninja -C build-qt612 -j 4 tests/auto/auto-tests`.

- [ ] **Step 3: Implement** `src/items/connector.{h,cpp}`. Port each shape's `connect()` from matplotlib's
  `ConnectionStyle` (link the class in a comment: the one allowed comment kind), working in the y-up frame of P-2:
  negate y of both points, build, negate y of every path point. `CURVE3` pairs become `quadTo`. `Angle3`/`Angle`
  intersections that do not exist (parallel lines) fall back to `lineTo(end)` (P-4).

- [ ] **Step 4: Run `QCP_TEST_CLASS=TestConnector`** (rules' xvfb command), expect all pass; then the whole suite.

- [ ] **Step 5: Commit** `feat(api): QCPConnector, the five connector shapes of matplotlib's annotate`.

### Task 2: QCPItemAnnotation, `ax->annotation`, the target

**Files:**
- Create: `src/items/item-annotation.{h,cpp}`
- Modify: `src/items/connector.{h,cpp}` (add `trimConnector`), `src/layoutelements/layoutelement-axisrect.h`
  (declaration + `QCPAnnotationOptions`), `src/layoutelements/axisrect-shortcuts.h` (definition), `meson.build`,
  `cmake/sources.cmake`, `src/qcp.h`, `examples/user-code/meson.build` (add `'annotations'`, drop the "waits for 6h"
  comment), `examples/user-code/README.md`, `examples/user-code/annotations/main.cpp` (first comment line only),
  `docs/items.md` (a section "Annotations" with a compiled snippet in `docs/snippets/items.cpp` and its doc-shot),
  `docs/plans/neoqcp-3/m6-gaps.md` (row and order item 8 marked done)
- Test: `tests/auto/test-annotation/test-annotation.{h,cpp}` (registered), `tests/auto/test-reference-m4e/` scene
  `annotationsTarget` and `reference/annotations-target.png`

**Interfaces:**
- Consumes: `QCPConnector` (Task 1); `qcpMeasureText`, `qcpTextLayout` (`src/painting/draw-commands.h`);
  `QCPDrawContext::path/text`; `QCPLineEnding::path/penFor/brushFor`.
- Produces:
  ```cpp
  struct QCPAnnotationOptions {
      QCPConnector connector;
      QPen pen;                      // default-constructed QPen: black 1 px, as the vocabulary row
      QCPLineEnding head;
      QCP::PixelLength shrink;
      QCPTextStyle textStyle;
  };
  QCPHandle<QCPItemAnnotation> QCPAxisRect::annotation(QCP::Position target, QCP::Position at,
                                                       const QString& text,
                                                       const QCPAnnotationOptions& options = { });

  namespace qcp {
  struct TrimmedConnector { QPolygonF line; double endAngleDegrees = 0; };   // empty line: draw nothing
  // Cut path where it first leaves startBox (an empty box cuts nothing), then shrink px from each end.
  [[nodiscard]] QCP_LIB_DECL TrimmedConnector trimConnector(const QPainterPath& path, const QRectF& startBox,
                                                            double shrink);
  }

  class QCP_LIB_DECL QCPItemAnnotation : public QCPItem {   // Q_OBJECT, properties for each setter
  public:
      QCPItemPosition* const target;
      QCPItemPosition* const position;
      void setText(const QString&);       QString text() const;
      void setTextStyle(const QCPTextStyle&); QCPTextStyle textStyle() const;
      void setConnector(const QCPConnector&); QCPConnector connector() const;
      void setPen(const QPen&);           QPen pen() const;
      void setSelectedPen(const QPen&);   QPen selectedPen() const;
      void setHead(const QCPLineEnding&); QCPLineEnding head() const;
      void setShrink(QCP::PixelLength);   QCP::PixelLength shrink() const;
  };
  ```
  Setters call `update()` first, as the project's setter rule says, and skip equal values.

- [ ] **Step 1: Write the failing tests.**

`TestAnnotation`, on a 400x300 plot, axes 0..1 both ways, `ax->annotation(QCP::Data{0.3, 0.2}, QCP::Data{0.8, 0.6},
"label", { .head = QCPLineEnding::esLineArrow })` unless the test says otherwise. Pixel checks read the recorded
commands or `pixelPosition()`, never font-dependent pixel counts:
- `trimCutsAtTheBoxAndShrinks` (pure): straight path (0,0)→(100,0), box (-10,-10,20,20), shrink 5 → line from (15,0)
  to (95,0), `endAngleDegrees` 0.
- `theCutIsAtTheFirstExit` (pure): a path that leaves the box, re-enters, leaves again → the line starts at the first
  exit point.
- `aTargetInsideTheBoxDrawsNoConnector`: target inside the text box → no connector command recorded, text recorded.
- `emptyTextStartsAtThePosition`: text "" → the line starts at `at`'s pixel (+ shrink 0), no box cut.
- `theTipEndsShrinkPixelsFromTheTarget`: `.shrink = 5_px` → the line's last point is 5 px (±0.5) from the target pixel.
- `theHeadPointsAlongThePath`: `curve(90, 0)` → head direction equals the path's tangent at the cut end (±1°).
- `aPanMovesTheTipNotTheText`: target in data, `at` as `QCP::PixelOffset`/pixels → after `xAxis()->setRange(0.1,
  1.1)` and a replot, the tip pixel moved by the pan, the text pixel did not.
- `aFarTargetDrawsInBoundedTime`: target at data 1e12 → replot returns (QTRY within 1 s), nothing recorded beyond the
  clip.
- `theConnectorDrawsUnderTheText`: connector commands come before the text command in the recording.
- `aClickOnTheConnectorSelects`: `setSelectable(true)`; a click 1 px off the drawn line selects; the selected pen is
  recorded after.
- `theShortcutAppliesEveryOption`: connector, pen, head, shrink, textStyle read back from the item equal the options.
- `settersRecordAgain`: each setter re-records once; an equal value does not.

`TestReferenceM4e::annotationsTarget`: the target's scene at 700x630, reference `annotations-target.png`
(`QCP_WRITE_REFERENCE=1` once, locally). The controller compares it with matplotlib's connectorstyle demo.

- [ ] **Step 2: Run, expect the build to fail** (no `QCPItemAnnotation`).

- [ ] **Step 3: Implement `qcp::trimConnector`** in `connector.cpp`: find the first exit from `startBox`, and the
  shrink circles, by bisection on `QPainterPath::percentAtLength` (matplotlib's `split_path_inout` bisects the same
  way); sample the kept part into a polyline at ≤ 1 px steps; the end angle is the original path's
  `angleAtPercent` at the end cut. `simplify:` note: the kept part is a sampled polyline, not exact Bézier pieces;
  upgrade path = split the quads with de Casteljau if a thick pen shows facets.

- [ ] **Step 4: Implement `QCPItemAnnotation`** (`item-annotation.{h,cpp}`): `draw` reads both positions in pixels
  (reading the transform, as `QCPItemZoomIndicator` and `QCPItemCurve` do, so a view change re-records), lays out the
  text box with `qcpTextLayout(style, qcpMeasureText(style, text))` at `position` (empty text: no box), builds
  `connector().path(boxCentreOrAt, targetPixel)`, trims it, records the polyline with `mainPen()`, the head with
  `QCPLineEnding::path` at the line's end, then `context.text(position, text, style, glyphSize)`. Clip against
  `clipRect()` enlarged by the pen width, as `QCPItemCurve::draw` does. `hitTest`: nearer of the box and the
  polyline. Cache the measured glyph size like `QCPItemText::glyphSize`.

- [ ] **Step 5: Implement `QCPAxisRect::annotation`** in `axisrect-shortcuts.h`: `add<QCPItemAnnotation>()`, set both
  positions, text and every option, return the handle. No misuse case exists (every input draws something or
  nothing, P-4).

- [ ] **Step 6: The target.** Add `'annotations'` to `examples/user-code/meson.build`, remove "Does not compile yet."
  from `main.cpp`'s first line, update the README. Write the reference scene and picture. Docs: the "Annotations"
  section in `docs/items.md` (what `at`/`target` mean, P-1 box start, the five shapes in one table with matplotlib's
  names, shrink, head) with a compiled snippet and its doc-shot. Mark the gaps row and order item 8 done.

- [ ] **Step 7: Run** `QCP_TEST_CLASS=TestAnnotation`, `TestReferenceM4e`, then the whole local suite; doc-shots; clang
  check; then Coder `build` (the log must show `user-code-annotations` compiled and linked), `build-asan`, `build-fco`
  (`ASAN_OPTIONS=detect_leaks=0`). List every written PNG with one line on what it shows.

- [ ] **Step 8: Commit** `feat(api): an annotation item with five connectors; the annotations target builds`.
