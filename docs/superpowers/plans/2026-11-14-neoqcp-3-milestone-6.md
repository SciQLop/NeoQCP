# NeoQCP 3.0 milestone 6, stage 1 — the target programs Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** the eleven target programs of milestone 6, written in the API the maintainer wants, with Python sketches, a
shared list of every name they assume, and a page for the maintainer to read them. No library change.

**Architecture:** the targets are plain `.cpp` files that do not compile yet. They are the fixed point stage 2 builds
the library towards. One vocabulary file keeps them consistent: every API name a target uses that does not exist
today is listed once, with its signature, and every target uses that same name. Stage 2 (the library changes) and
stage 3 (the matplotlib comparison) get their own plan after the maintainer's gate (Task 4).

**Tech Stack:** C++20, Qt 6.12; Python sketches in PySide style; no build.

**Spec:** `docs/specs/2026-11-14-neoqcp-3-milestone-6-design.md` (goals 1 and 4, "The API the maintainer wants",
maintainer answers, controller calls).

## Global Constraints

- Looks like an official Qt module, but nicer, modern and safer where it can: `QCP` prefix, Qt naming (`color()` /
  `setColor()`), Qt value types (`QColor`, `QPointF`, `QString`, `QPen`), signals outside an object, virtual event
  handlers in derived classes.
- Simple names, little code per plot. No `<>` at the call site unless nothing nicer exists; `add<T>(args...)` is the
  one general entry for any type, and built-in types also get short named calls (`ax->line(...)`) with named options.
- C++20 and later: concepts, `auto`, designated initializers. Any range of numbers goes in.
- The drawing API is used only inside derived classes (a plot type, an extension of one, an annotation). No
  draw callback on the axis rect.
- Annotations: any composition of drawing commands, each in data or pixel coordinates, mixed in one annotation; click,
  move and resize hooks the user implements.
- A target is never bent to fit today's API. Where today's API is already the nice form, use it as it is.
- Python is not a constraint (PySide later); the sketches are informative.
- Commit with explicit pathspecs; never `subprojects/.wraplock`, `subprojects/jkqtmathtext`, `qt.ini`, `.superpowers`.
  No build is needed in this stage. Push only when asked.

## Review Focus

1. A target that quietly uses today's awkward form (NaN helper columns, `QSharedPointer` tickers, hand-kept sync
   lambdas) where the spec wants a nicer one: each task's reviewer checks every call against the survey's "awkward"
   reason for that case.
2. The same idea named two ways in two targets: Task 3 and Task 4 check every new name against `m6-vocabulary.md`.
3. A target that dodges its hard part (Asteroids text drawn on a separate layer, the callout without resize, the inset
   without its connectors): the "must show" list of each target is checked line by line.
4. Names a Qt developer would not recognise, or `<>` where a named call would do: checked per vocabulary row.
5. Game logic swamping the API under test: each game keeps its rules in a small plain struct, so the reader sees the
   NeoQCP calls.

---

### Task 1: Commit the surveys

**Files:**
- Create: `docs/plans/neoqcp-3/m6-mpl-survey.md` (copy of `.superpowers/mpl-challenge/selection.md`)
- Create: `docs/plans/neoqcp-3/m6-extension-survey.md` (copy of `.superpowers/extension-challenge/selection.md`)

- [ ] **Step 1:** Copy both files unchanged, then add one line under each title: "Survey of 3.0 at 42956b6, milestone
  6 evidence (spec `2026-11-14-neoqcp-3-milestone-6-design.md`)."
- [ ] **Step 2:** Fix the one known factual error: the extension survey says `docs/writing-a-plot-type.md` names
  `QCPDrawn<Base>`; check the guide today and correct the survey row if the guide changed.
- [ ] **Step 3: Commit** `docs(plans): milestone 6 surveys`.

### Task 2: The five drawing-side targets and the vocabulary

**Files:**
- Create: `examples/user-code/README.md` (one line per target: what it shows, the survey row it answers)
- Create: `examples/user-code/lollipop/main.cpp`, `lollipop.py`
- Create: `examples/user-code/asteroids/main.cpp`, `asteroids.py`
- Create: `examples/user-code/brick-breaker/main.cpp`, `brick_breaker.py`
- Create: `examples/user-code/callout/main.cpp`, `callout.py`
- Create: `examples/user-code/peak-fit/main.cpp`, `peak_fit.py`
- Create: `docs/plans/neoqcp-3/m6-vocabulary.md`
- Do not touch `examples/meson.build`: the targets are not built in stage 1.

**Interfaces:**
- Produces: `m6-vocabulary.md`, one table, one row per API name a target uses that does not exist today:
  `| Name | Signature | Kind (new / reshaped / exists) | Used by | Replaces (today's form) |`. Rows for existing names
  only where a target relies on behaviour that differs from today (say what differs).

Each program is complete (a `main()` that shows a window), short, and reads top to bottom. "Must show" per target:

- **lollipop**: a `QCPLollipop` plot type a user writes, deriving from an existing type or the 1D base (pick the one
  that needs least code and say why in the README); stem + disc per point recorded in `draw`; click selects one
  lollipop and it is drawn thicker; legend icon is one small lollipop; created with `ax->add<QCPLollipop>(...)`.
- **asteroids**: ship, rocks and bullets as annotations the user derives (rotated polygons in data units); held keys
  (turn while thrusting) reach the plot without a click first; positions wrap at the edges; score label on top of
  everything; game-over banner; a frame tick (a timer is fine if the target shows why nothing nicer is needed).
- **brick-breaker**: 60+ static bricks, one ball, a paddle dragged with the mouse (the user's drag hook); collisions
  computed in data units from the shapes' geometry the library exposes; a hit brick disappears without re-recording
  the others (the target only shows the call; the cost is stage 2's concern).
- **callout**: an annotation whose pointer tip sits on a data point and whose rounded text box is placed in pixels
  from it; the user drags the box (tip stays), resizes it from a corner handle, and other annotations can attach to
  its `tip` and `box` anchors.
- **peak-fit**: a noisy spectrum with three Gaussian peaks; each peak is an annotation the user derives (centre
  dragged, width resized from two side handles); on every change a plain function refits and the model curve and a
  residual panel below (sharing x) update; no user lambda keeps the panels in sync.

- [ ] **Step 1:** Write the five programs. Each game's rules live in one small plain struct (no NeoQCP types) so the
  NeoQCP calls stand out.
- [ ] **Step 2:** Write the vocabulary: every name in the five programs that is not in today's headers gets a row;
  grep `src/` for each name to fill the Kind column.
- [ ] **Step 3:** Write each Python sketch: the same program as PySide code would read, same names in snake_case
  only where PySide would rename (it does not; keep Qt names).
- [ ] **Step 4:** Check: every "must show" line above appears in its program (list them in the report with line
  numbers); no `<>` at a call site except `add<T>`; no draw callback outside a derived class.
- [ ] **Step 5: Commit** `docs(user-code): the five drawing-side targets and their vocabulary`.

### Task 3: The six matplotlib targets

**Files:**
- Create: `examples/user-code/{shared-x,symlog,inset-zoom,fill-between,spectrogram,annotations}/main.cpp` and one
  `.py` sketch each (`shared_x.py`, ...)
- Modify: `examples/user-code/README.md`, `docs/plans/neoqcp-3/m6-vocabulary.md` (add rows; reuse existing rows'
  names for the same idea)

**Interfaces:**
- Consumes: `m6-vocabulary.md` from Task 2. Same idea, same name.

Each target reproduces its matplotlib gallery example (URL in `m6-mpl-survey.md`, cases 4, 3, 9, 20, 25, 10) with the
same data and the same picture. "Must show": the survey's "awkward" or "missing" reason for that case is gone from
the code:

- **shared-x** (#4): N panels share x in one call; inner tick labels hidden by that call, not one by one.
- **symlog** (#3): a symlog axis scale with a linear threshold, set in one call.
- **inset-zoom** (#9): an inset axes on a sub-range; the indicator box and connectors are created by one call and
  follow range changes without user lambdas.
- **fill-between** (#20): two lines and a fill between them, green above and red below, no helper columns; the fill
  follows when one line's data changes.
- **spectrogram** (#25): a spectrogram panel above a time-series panel sharing time; log frequency axis; colour bar
  beside the spectrogram; no `QSharedPointer` ticker and no `qobject_cast`.
- **annotations** (#10): text with an arrow to a point in one annotation; arc and elbow connectors; the arrow stops
  short of the text and the point.

- [ ] **Step 1:** Write the six programs, each with a one-line comment giving its gallery URL.
- [ ] **Step 2:** Add their new names to the vocabulary; where a name from Task 2 fits, use it.
- [ ] **Step 3:** Write the six Python sketches.
- [ ] **Step 4:** Check as Task 2 Step 4, against this task's "must show" lines; add line counts per program to the
  README beside the matplotlib example's line count.
- [ ] **Step 5: Commit** `docs(user-code): the six matplotlib targets`.

### Task 4: The maintainer's gate

**Files:**
- Create: `docs/plans/neoqcp-3/m6-gaps.md`: the vocabulary rows grouped by theme (multi-panel plumbing, scales,
  drawing API in derived classes, annotations and hooks, call-site friction, other), each with the targets that need
  it and a size S/M/L. This is stage 2's input.

- [ ] **Step 1:** Write `m6-gaps.md`.
- [ ] **Step 2:** The controller publishes one page for the maintainer: per target, the program, its line count, and
  (for the matplotlib six) the gallery picture link; then the gap themes.
- [ ] **Step 3: Gate.** The maintainer reads. Their changes go into the targets and the vocabulary (one commit per
  round, `docs(user-code): maintainer review round N`). Stage 2's plan is written only after the maintainer approves.
