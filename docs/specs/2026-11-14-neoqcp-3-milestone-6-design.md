# NeoQCP 3.0 — milestone 6: the user's code first

The authorities before this one are the object-model spec (`2026-11-02-...`), the drawing-API spec (`2026-11-04-...`),
the coordinate-systems design (`2026-11-09-...`) and the milestone-5 design (`2026-11-11-...`). Milestones 1-5 rebuilt
the object model, the drawing path and the meta-object surface. This milestone looks at the result from the user's
side of the screen.

## Intent

The goal is a fresh, well-designed NeoQCP with a clean, elegant and powerful API; SciQLopPlots is rebuilt on it later
(maintainer, 2026-11-14). Two benchmarks judge it:

- hard cases from the matplotlib gallery, built with a simpler and better API ("being able to build them with a
  simple and elegant C++ code means Python will even be simpler");
- user-written plot types and annotations that torture the drawing API ("should feel like something you want to use
  and play with"), Asteroids and a brick breaker among them (Asteroids replaced Snake, maintainer 2026-11-14:
  Snake only draws grid squares).

Two read-only surveys already ran against 3.0 as it is (`.superpowers/mpl-challenge/selection.md`, 25 gallery cases:
4 clean, 15 awkward, 6 missing; `.superpowers/extension-challenge/selection.md`, 10 extension programs: 3 easy, 6
awkward, 1 blocked). They are the evidence base; both are git-ignored and get committed under
`docs/plans/neoqcp-3/` as the first step.

Method: write the code a user should write **first**, have the maintainer read it, then change the library until that
code compiles, runs and draws the right picture. The target code is fixed before the API moves; the API is what bends.

## The API the maintainer wants (2026-11-14)

- **Looks like an official Qt module, but better where it can be: nicer, modern, safer.** `QCP` prefix, Qt naming
  (`color()` / `setColor()`), Qt value types (`QColor`, `QPointF`, `QString`, `QPen`), signals for code outside an
  object, virtual event handlers for derived classes (as `QWidget::mousePressEvent`). No new parallel API with other
  names; the `QCP` classes themselves are reshaped.
- **Simple names, little code per plot.**
- **No `<>` at the call site unless nothing nicer exists** (`ax->line(x, y)` rather than `add<QCPGraph>()`). C++20
  and later: concepts, `auto`, designated initializers, as much as they help. Any range of numbers goes in.
- **Two call styles he liked, to combine:** short calls with named options
  (`ax->line(x, y, {.pen = ..., .name = "..."})`), and handles that are kept and stay linked (a fill between two lines
  follows when one line's data changes).
- **The drawing API is used from inside derived classes**: a new plot type, an extension of an existing one, or an
  annotation. The axis rect is not a free canvas, and there is no draw-whatever-you-want callback on it.
- **Annotations of any shape stay**: a composition of drawing commands, each in data or pixel coordinates (mixed in
  one annotation), with click, move and resize hooks the user implements.
- Both building on an existing plot type and writing a new one from scratch should be easy.

## Goals

1. Ten target programs, written in the API above, read and approved by the maintainer before the library changes:
   - **Plot types and annotations** (the drawing API):
     1. lollipop chart: a new plot type; click selects one lollipop; selected look; legend icon;
     2. Asteroids: rotated polygons, many moving shapes, held keys (thrust while turning) without a click first,
        wrap-around at the edges, a score label on top, a game-over banner;
     3. brick breaker: many static shapes, a paddle dragged with the mouse, collisions in data space;
     4. a callout annotation the user can drag and resize, pointer tip on a data point, text box in pixels.
   - **Matplotlib hard cases** (gallery numbers from the survey):
     5. shared-x panels (#4);
     6. symlog axis (#3);
     7. inset zoom with its indicator box and connectors (#9);
     8. conditional fill between two curves, coloured by which is on top (#20);
     9. spectrogram above a time series (#25);
     10. annotations with connectors (#10).
2. The library changed until all ten compile, run, and draw a picture that matches the target, each with tests.
3. A side-by-side page: each matplotlib case next to the NeoQCP picture and code, with line counts.
4. For each target, a short Python sketch of the same program, on paper. Informative only: Python will come through
   PySide, so binding concerns do not shape the C++ API in this milestone (maintainer, 2026-11-14).
5. The colour-bar look checked against matplotlib in that comparison: today the automatic bar sits flush against the
   data and is shorter than the axis rect. Fixed if it looks wrong next to matplotlib.

## Non-goals

- Missing plot families (survey pattern 3). They are in the backlog, "New plot types", to be written later with the
  same public drawing API a user has, as further torture tests.
- The data model and threading (project 3).
- Python bindings; the SciQLopPlots port; a release.
- New game features in the library. The games use only generic API.

## Anti-goals

- Bending a target program to fit today's API. If the target cannot compile, the library changes, or the maintainer
  rewrites the target.
- A second API beside the `QCP` classes, or deprecated forwarders kept "for now" (no interim designs, a standing
  maintainer rule).
- Names or patterns a Qt developer would not recognise.

## Constraints

- Branch `neoqcp-3.0`, worktree `~/Documents/prog/NeoQCP-3.0`. Qt 6.12, C++20, meson + `cmake/sources.cmake`.
- Breaking changes are allowed; each goes into `docs/migrating.md` and, lightly, the SciQLopPlots migration notes.
- Failing test first for every change. Builds and suites on the Coder workspace `neoqcp`; reference pictures locally.
- Every rule from earlier milestones stands: deferred signals (4a), emission scopes and delete-in-slot safety (5e),
  handles for plot-owned objects (5b).
- Push only when asked.

## Order of work

1. Commit the two surveys. Write the ten targets as plain `.cpp` files that do not compile yet, plus the Python
   sketches. **Gate: the maintainer reads them.** Their feedback changes the targets, not the plan.
2. Group the library changes the targets need by theme (the survey patterns are a starting split: multi-panel
   plumbing, scales, the drawing API in derived classes, annotations and their hooks, call-site friction). One
   sub-milestone per theme, each ending with its targets compiling and their pictures checked.
3. The side-by-side page and the colour-bar check.

## Maintainer answers (2026-11-14, after the builder check)

- **Custom types keep one general entry, `add<T>(args...)`**, for built-in and user types alike: there `<>` is the
  nice form, since the type is what is chosen. Built-in types also get short named calls (`ax->line(...)`,
  `ax->bars(...)`, ...) with named options, as thin shortcuts over it. Nothing is deleted to make room for them.
- **Python is not a constraint now** (PySide later): no plain-list overloads beside concept entry points, and no
  replacement of `std::variant` / `std::span` / `shared_ptr` in the drawing API for binding's sake.

## Calls made by the controller (not the maintainer's; check them)

- **A reshape a target needs applies across the whole library** (named options on every built-in type, a new scale
  model on every axis and colour scale), never to one corner: the no-interim-designs rule.
- **Done, per target:** it compiles unchanged from the approved text, runs, and has a reference-picture test (checked
  locally, fonts). The matplotlib comparison is a page for the eye, not a pixel test. Each sub-milestone ends with a
  page of its targets' pictures and code for the maintainer to look at.
- **Call order is screen order inside one recording.** Text recorded after shapes is drawn over them. Today it is
  drawn under (extension survey, Snake), and "use a higher layer" is a workaround the anti-goal rules out.
- **The other drawing-side gaps the extension survey found are library work when a target hits them:** keys reaching
  a plot without a click, hover events for annotations, per-datum hit test and selected look from the recording,
  re-recording only what changed (brick breaker).

## Left to the builder

Exact names, option structs and signatures, as long as the targets the maintainer approved compile unchanged; where
the target programs live (an `examples/` subfolder built with the examples is the obvious place); the split of
sub-milestones; how keys reach a plot without a click first.
