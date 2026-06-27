# NeoQCP, a QCustomPlot Modernized Fork
[![Tests](https://github.com/SciQLop/NeoQCP/actions/workflows/tests.yml/badge.svg)](https://github.com/SciQLop/NeoQCP/actions/workflows/tests.yml)

[http://www.qcustomplot.com/images/logo.png](http://www.qcustomplot.com/images/logo.png)

This fork aims at modernizing the excellent [QCustomPlot](https://www.qcustomplot.com/) library with:

## ✨ Key Improvements

- **Qt6 Exclusive Support**
  Dropped legacy Qt4/Qt5 support - focused on Qt6 only to reduce complexity and leverage modern Qt features

- **QRhi Rendering Backend**
  Replaced the QWidget + OpenGL FBO rendering path with QRhiWidget. Qt's Rendering Hardware Interface maps to the native graphics API on each platform (Metal on macOS, Vulkan on Linux, D3D on Windows). Layer textures are composited directly on the GPU with no GPU-to-CPU readback.

- **GitHub Actions CI**
  Integrated GitHub Actions for continuous integration, ensuring code quality and build stability across platforms

- **Modern Build System**
  Replaced qmake with [Meson](https://mesonbuild.com/) for faster, more reliable builds

- **[Tracy](https://github.com/wolfpld/tracy) Profiler Integration**
  Integrated Tracy for real-time performance profiling, allowing you to visualize and optimize your plotting code and
  NeoQCP itself.

- **Zero-Copy Data Source (QCPGraph2)**
  New plottable that plots directly from user-owned containers without copying or converting data. Supports `std::vector`, `QVector`, `std::span`, and raw pointers with any numeric type (`double`, `float`, `int`). Uses C++20 concepts and SoA layout — algorithms run in the container's native type and only cast to `double` at the final pixel-coordinate step. Supports all line styles from QCPGraph (line, step-left/right/center, impulse), scatter symbols, and scatter skip.

  ```cpp
  // Owning — move your data in, no copy
  std::vector<double> keys = /* ... */;
  std::vector<float> values = /* ... */;
  graph->setData(std::move(keys), std::move(values));

  // Zero-copy view — plot directly from your buffer
  graph->viewData(keyPtr, valuePtr, count);

  // Shared — multiple graphs, one data source
  auto src = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<float>>>(
      std::move(keys), std::move(values));
  graph1->setDataSource(src);
  graph2->setDataSource(src);

  // Line styles and scatter symbols
  graph->setLineStyle(QCPGraph2::lsStepCenter);
  graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 8));
  graph->setScatterSkip(2);            // draw every 3rd scatter point
  graph->setScatterMaxPoints(50000);   // stratified subsampling cap for huge datasets

  // Per-point color axis — color scatters by a third quantity
  graph->setScatterColorValues(std::move(colorValues));
  graph->setScatterColorGradient(QCPColorGradient::gpJet);
  ```

  Scatters render on the GPU via instanced SDF sprites (see GPU Plottable Rendering), and large series are decimated with adaptive min/max sampling so panning and zooming stay interactive.

  **QCPGraph2 vs QCPGraph:**

  | Feature | QCPGraph | QCPGraph2 |
  |---------|----------|-----------|
  | Data source | `QCPDataContainer` (copies data) | Zero-copy pluggable `QCPAbstractDataSource` |
  | Container types | `double` only | Any numeric type (`float`, `int`, etc.) |
  | Line styles | All 6 | All 6 |
  | Scatter symbols | All 17+ shapes | All 17+ shapes |
  | Fill under graph | Yes | No |
  | Channel fill | Yes | No |
  | `addData()` incremental | Yes | Not yet |
  | Selection decoration | Full (pen + scatter) | Basic |

- **GPU Plottable Rendering**
  QCPGraph and QCPCurve solid-line strokes and baseline fills rendered via QRhi shaders with polyline extrusion and 4x MSAA antialiasing. Scatter symbols are drawn as GPU-instanced SDF sprites, and the grid, tick marks, and span items are emitted directly to the GPU as well — keeping the whole hot path off the CPU.

- **Smooth GPU-Translated Panning**
  Panning shifts cached pixel-space geometry on the GPU via per-draw offsets instead of re-extruding and re-uploading every frame, and the compositor translates unchanged layer textures rather than repainting them. This makes dragging large datasets fluid without touching the CPU data path.

- **Zero-Copy Colormap (QCPColorMap2)**
  New colormap plottable with async resampling and zero-copy data sources. Data is resampled on a background thread with request coalescing — only the latest viewport matters, so rapid zooming/panning stays fluid.

  ```cpp
  auto* cm = new QCPColorMap2(xAxis, yAxis);

  // Owning — move your data in
  cm->setData(std::move(xVec), std::move(yVec), std::move(zVec));

  // Zero-copy view — plot directly from your buffer
  cm->viewData(xSpan, ySpan, zSpan);
  ```

  Data gaps (missing acquisition periods) are detected and left blank instead of being interpolated across, logarithmic Y axes are supported, and an optional GPU contour-line overlay can be drawn on top of the colorized cells.

- **Multi-Component Graph (QCPMultiGraph)**
  Plots many value columns that share a single key axis from one zero-copy SoA source — ideal for multi-channel time series. Each component has its own pen/selection, components can be colored in bulk, and the same two-level async resampling as QCPGraph2 keeps millions of points per column interactive. Accepts column-major, row-major, and `std::span` layouts.

  ```cpp
  auto* mg = new QCPMultiGraph(xAxis, yAxis);
  mg->setData(std::move(keys), std::move(valueColumns));   // one vector per component
  mg->setComponentColors({Qt::red, Qt::green, Qt::blue});
  ```

- **Waterfall (QCPWaterfall)**
  Built on QCPMultiGraph, stacks normalized spectra/traces into a waterfall display. Normalization is recomputed automatically as data changes.

- **2D Histogram (QCPHistogram2D)**
  Bins `(key, value)` point clouds into a colormap on a background thread, with per-axis configurable bin counts and logarithmic bin spacing that follows the axis scale type.

  ```cpp
  auto* h = new QCPHistogram2D(xAxis, yAxis);
  h->setBins(256, 256);
  h->viewData(xSpan, ySpan);
  ```

- **Span Items (QCPVSpan / QCPHSpan / QCPRSpan)**
  GPU-rendered shaded regions spanning a key range (vertical), value range (horizontal), or both (rectangular) — useful for highlighting time intervals, catalog events, or thresholds. Coordinates are converted to pixels in double precision on the CPU to avoid float32 cancellation with large timestamps.

  ```cpp
  auto* span = new QCPVSpan(plot);
  span->setRange(QCPRange(t0, t1));
  span->setBrush(QColor(0, 120, 215, 60));
  ```

- **Interactive Item Creation Mode**
  Drive click-and-drag item creation from user callbacks instead of bespoke event handling. An `ItemCreator` builds the item on press; an `ItemPositioner` updates its geometry as the mouse drags. Creation only fires inside the axis rect's data area.

  ```cpp
  plot->setItemCreator([](QCustomPlot* p, QCPAxis* kx, QCPAxis* vx) {
      return new QCPVSpan(p);
  });
  plot->setItemPositioner([](QCPAbstractItem* it, double k0, double, double k1, double) {
      static_cast<QCPVSpan*>(it)->setRange(QCPRange(k0, k1));
  });
  ```

- **Theming**
  `QCPTheme` with named color roles (background, foreground, grid, sub-grid, selection, legend). Ships with `QCPTheme::light()` and `QCPTheme::dark()` factories. A shared theme can be applied to multiple plots and live-updated — all axes, grids, legends, ticks, labels, and items react automatically. Theme colors are exposed as Qt properties for QSS integration.

  ```cpp
  auto* theme = QCPTheme::dark();
  plot->setTheme(theme);  // applies and connects for live updates
  ```

  Theme colors are exposed as Qt properties on `QCustomPlot`, so you can drive them entirely from QSS:

  ```css
  /* Dark theme via stylesheet */
  QCustomPlot {
      qproperty-themeBackground:       #1e1e1e;
      qproperty-themeForeground:       #d4d4d4;
      qproperty-themeGrid:             #3c3c3c;
      qproperty-themeSubGrid:          #2a2a2a;
      qproperty-themeSelection:        #264f78;
      qproperty-themeLegendBackground: #252526;
      qproperty-themeLegendBorder:     #3c3c3c;
  }

  /* Light theme via stylesheet */
  QCustomPlot[objectName="lightPlot"] {
      qproperty-themeBackground:       #ffffff;
      qproperty-themeForeground:       #1e1e1e;
      qproperty-themeGrid:             #d0d0d0;
      qproperty-themeSubGrid:          #e8e8e8;
      qproperty-themeSelection:        #0078d4;
      qproperty-themeLegendBackground: #f5f5f5;
      qproperty-themeLegendBorder:     #d0d0d0;
  }
  ```

  This lets you switch plot themes along with your application stylesheet — no C++ theme code needed.

- **Upstream QCustomPlot Bugs Fixed**
  Latent bugs in QCustomPlot's own code, patched in this fork:
    - DPI scaling issues on macOS and Windows
    - Crash on Qt 6.10+ from an out-of-range `tickStep` reaching `qRound` in the datetime axis ticker (`qCheckedFPConversionToInteger` assert)
    - `QCustomPlot::removeLayer()` use-after-delete — the layer was deleted before being removed from the layer list (latent UB)

- **NeoQCP Rendering-Path Fixes**
  Hardening of NeoQCP's own new GPU / async / zero-copy machinery:
    - Data gaps and NaN values break polylines instead of drawing fan lines across them (regression in the zero-copy `QCPGraph2`/`QCPMultiGraph` pixel mapping — upstream `QCPGraph` already handled this)
    - Non-owning data sources guard against use-after-free when the caller frees the buffer mid-resample (`dataGuard`)
    - Stale results stay visible during background rebuilds — no blank frames on data changes or pans
    - Async jobs are safe against plottable destruction and viewport mismatches
    - macOS/Metal correctness: scissor-rect Y-flip and vertex/SRB binding order

- **Planned Features**
    - Incremental `addData()` for QCPGraph2
    - Fill support for QCPGraph2 (under graph / channel fill)

## 📥 Installation

### Prerequisites

- Qt 6.7+ (Core, Gui, Widgets, Svg, PrintSupport modules)
- Meson 1.1.0+
- Ninja build system
- C++20 compatible compiler

### Build Steps

```bash
git clone https://github.com/jeandet/NeoQCP.git
cd NeoQCP
meson setup build --buildtype=release
meson compile -C build
```

### Integration in Your Meson Project

Create a `subprojects/NeoQCP.wrap` file with the following content:

```meson
[wrap-git]
url = https://github.com/SciQLop/NeoQCP.git
revision = HEAD
depth = 1

[provide]
NeoQCP = NeoQCP_dep

```

Then, in your `meson.build` file, add:

```meson
project('my-plot-app', 'cpp',
        default_options: ['cpp_std=c++20'])

qtmod = import('qt6')
qtdeps = dependency('qt6', modules : ['Core','Widgets','Gui','Svg','PrintSupport'], method:'qmake')

neoqcp = dependency('NeoQCP')

executable('myapp',
           'main.cpp',
           dependencies: [qtdeps, neoqcp],
           gui_app: true)
```

## 🆚 Migration from QCustomPlot

TBD

## 🧪 Testing

Run test suite with:

```bash
meson setup builddir
meson test -C builddir
```

## 🤝 Contributing

We welcome contributions! Please follow these guidelines:

1. Fork the repository
2. Create a new branch for your feature or bug fix
3. Write tests for new functionality or bug fixes
4. Submit pull request with description of changes

## 📄 License

GNU GPL v3 (same as upstream QCustomPlot)

---

**Report Issues**: [GitHub Issues](https://github.com/SciQLop/NeoQCP/issues)
