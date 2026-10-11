# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

NeoQCP is a fork of QCustomPlot (v2.1.1), a Qt-based C++20 plotting library. It is split into individual source files (the original QCustomPlot ships as a single amalgamated .h/.cpp pair). Licensed under GPL3.

## Build System

Uses Meson with Qt6. Requires `meson` and `ninja`.

```bash
# Configure (default: release, tests on)
meson setup build

# Build
meson compile -C build

# Run all tests
meson test -C build

# Run tests with error output
meson test --print-errorlogs -C build

# Run benchmarks
meson test --benchmark -C build
```

### Meson Options

| Option | Default | Description |
|---|---|---|
| `tracy_enable` | false | Enable Tracy profiling (use with `--buildtype=debugoptimized`) |
| `with_examples` | false | Build example applications |
| `with_tests` | true | Build and enable tests |

Example: `meson setup -Dtracy_enable=true --buildtype=debugoptimized build`

## Architecture

### Source Layout (`src/`)

The library builds as a static library (`libNeoQCP.a`). The public include header is `src/qcustomplot.h` which includes `src/qcp.h` (the umbrella header).

- **`core.h/cpp`** — `QCustomPlot` widget class (the main entry point). Inherits `QRhiWidget`. Owns layers, plottables, axes, layout system. Implements `initialize()`/`render()`/`releaseResources()` for RHI compositing.
- **`global.h`** — Macros, QCP namespace enums/flags, Qt includes. All other headers include this.
- **`axis/`** — `QCPAxis`, `QCPRange`, axis tickers (datetime, log, pi, text, time, fixed), label painter
- **`plottables/`** — Graph, curve, bars, colormap, financial, errorbar, statistical box. Base class `QCPAbstractPlottable` in `plottable.h`, 1D plottable template in `plottable1d.h`
- **`items/`** — Overlay items (text, line, rect, ellipse, pixmap, bracket, curve, tracer, straight-line)
- **`layoutelements/`** — Axis rect, legend, color scale, text element
- **`painting/`** — Paint buffer hierarchy (`QCPAbstractPaintBuffer` → `QCPPaintBufferPixmap`, `QCPPaintBufferRhi`), `QCPPainter`, compositing shaders in `shaders/`
- **`polar/`** — Polar plot support (angular axis, radial axis, polar graph, polar grid)
- **`Profiling.hpp`** — Tracy profiling macros, compiles to no-ops when `TRACY_ENABLE` is not defined

### Rendering Architecture

NeoQCP uses QRhiWidget (Qt 6.7+) for hardware-accelerated rendering:

1. **Replot phase** (`replot()`): Layers draw via QPainter into CPU-side `QImage` staging buffers (`QCPPaintBufferRhi`)
2. **Render phase** (`render()`): Staging images are uploaded as GPU textures and composited via fullscreen quads with premultiplied alpha blending
3. **Export paths** (`savePdf()`, `toPixmap()`, etc.): Use QPainter directly, bypassing the RHI pipeline

### Key NeoQCP Additions (vs upstream QCustomPlot)

- **QRhi rendering backend** (`QCPPaintBufferRhi` in `paintbuffer-rhi.h`): Replaces the OpenGL FBO path. Each layer paints into a QImage staging buffer; at render time, textures are uploaded and composited on the GPU with no readback.
- **Tracy profiling integration** via `Profiling.hpp` macros.
- **Split source files** instead of single amalgamated file.

### Tests (`tests/`)

- **`auto/`** — Qt Test-based unit tests, compiled into a single `auto-tests` executable. Tests for graph, curve, bars, financial, layout, axis rect, colormap, data container, legend, and QCustomPlot core.
- **`benchmark/`** — Performance benchmarks using Qt Test benchmarking.
- **`manual/`** — Manual/visual test application.
- **`big_data_for_tracy/`** — Tracy profiling test with large datasets.

### Documentation (Read the Docs)

- `docs/*.md` is the user guide (Sphinx + MyST, `docs/conf.py`), `docs/architecture/` is its
  "Under the hood" section. `docs/plans`, `docs/specs`, `docs/superpowers` are internal, not published.
- Every guide example is a `DOC_SNIPPET` in `docs/snippets/<page>.cpp`, included by marker
  (`// [name]` … `// [/name]`). The `doc-snippets` meson test compiles and runs them all.
- Images: `build/docs/snippets/doc-shots docs/images [name ...]` redraws the guide figures
  (offscreen, waits for async plottables); `build/documentation/doc-image-generator/doc-image-generator
  documentation/images` redraws the API reference images. Both are committed: Read the Docs
  cannot build NeoQCP.
- API reference: `doxygen docs/Doxyfile` writes `docs/_api/api/`, published at `<site>/api/`.
- Local build: `pip install -r docs/requirements.txt`, then `sphinx-build -b html docs docs/_build/html`.

### CI

GitHub Actions runs on Linux (x86_64 + aarch64), Windows, and macOS (Intel + Apple Silicon) with Qt 6. Build uses `--buildtype debugoptimized`.
