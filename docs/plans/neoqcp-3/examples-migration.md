# NeoQCP 3.0 — every example and doc snippet in the 3.0 style

Decided by the maintainer on 2026-11-19: to promote the 3.0 API, every example must use it. To run after milestone 6h
and before the Mistral user test (`mistral-user-testing.md`), so that test learns from good examples.

## Why

The classic demos were ported in milestone 5 only far enough to compile: `examples/plots/mainwindow.cpp` still calls
`customPlot->graph(i)->setPen(...)` 107 times, plus `addGraph()`, `new QCPAxisRect`, `new QCPColorScale`. Anyone
who opens them, a person or a model, learns the 2.x way. The doc snippets carry the same idioms in places.

## Scope (counts of 2.x idioms on 2026-11-19: `addGraph`, `->graph(`, `new QCP…`, `addPlottable`, `addItem`)

| Where | Lines | Old idioms |
|---|---|---|
| `examples/plots` | 1746 | 130 |
| `examples/interactions` | 337 | 7 |
| `examples/text-document-integration` | 336 | 8 |
| `examples/scrollbar-axis-range-control` | 182 | 5 |
| `examples/theme-demo`, `examples/wasm-gallery` | 167 | 2 |
| `examples/axis-tags`, `examples/custom-drawable` | 431 | 0 (check the style anyway) |
| `examples/user-code` (the milestone-6 targets) | 1134 | 0, but axes set one setter per line |
| `docs/snippets/*.cpp` used by the guides | — | in `graphs`, `gallery`, `interactions`, `items`, `systems`, `layout`, `axes` |

Some `new QCP…` hits may be the 3.0 way for that type (a ticker made with `new` and handed over): check each one
against the API before changing it.

## The 3.0 style (what "migrated" means)

- Plottables and items through the rect's one-call shortcuts with named options (`ax->line`, `ax->colorMap`,
  `ax->fillBetween`, `ax->ellipse`, `ax->text`, …), or `add<T>(...)` for the rest; never `addGraph` / `graph(i)`.
- Keep the returned handle and use it; no index lookups.
- Chained handle setters where they read better (`ax->xAxis().setRange(0, 100).setVisible(false)`).
- Layout through `setAxisRectGrid`, `addInset`, `setTitle`; colour bars through the rect's own `colorScale()`.
- The milestone-6 targets: the "compile unchanged" rule held until each target compiled; they now get the same polish.

## The examples drive the API

The migration is also an audit of the API. Whatever an example needs that a user should not have to write is an API
finding, fixed in the library, not hidden in the example:

- **No boilerplate:** setup that every example repeats (the same five setters, a helper copied between demos, glue
  code to connect two objects) becomes an option, a shortcut or a default.
- **No unsafe code:** no manual `delete`, no lifetime the user has to track, no cast to reach a type, no index that
  can go stale. If the only way is unsafe, the API is missing something.
- **No raw pointer juggling:** handles everywhere a user holds an object; a raw `QCP…*` in an example is a question
  ("why is there no handle here?").
- **Easy:** a reader who knows matplotlib or Qt should guess the call. A call that needs a comment to explain it, or
  a name the example has to look up, is a finding.

Each finding is written down (example, what it needed, what the API should offer), fixed with a failing test first
like any change, and the example then uses the fix. Findings that are too big for the pass go to the backlog with the
example that needs them.

## How

One task per example directory (plots split by demo function), each: rewrite, build with the examples, run it, and
compare its picture with the old one where the demo has one (a changed picture is explained, not just redrawn). Doc
snippets: rewrite, run `doc-shots`, look at every image that changed. Finish with a grep showing no 2.x idiom left
outside `docs/migrating.md` and the API docs that name the old calls on purpose.
