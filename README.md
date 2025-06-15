# NeoQCP, a QCustomPlot Modernized Fork
[![Tests](https://github.com/SciQLop/NeoQCP/actions/workflows/tests.yml/badge.svg)](https://github.com/SciQLop/NeoQCP/actions/workflows/tests.yml)

[http://www.qcustomplot.com/images/logo.png](http://www.qcustomplot.com/images/logo.png)

This fork aims at modernizing the excellent [QCustomPlot](https://www.qcustomplot.com/) library with:

## ✨ Key Improvements

- **Qt6 Exclusive Support**  
  Dropped legacy Qt4/Qt5 support - focussed for Qt6 only to reduce complexity and leverage modern Qt features

- **GitHub Actions CI**  
  Integrated GitHub Actions for continuous integration, ensuring code quality and build stability across platforms

- **Modern Build System**  
  Replaced qmake with [Meson](https://mesonbuild.com/) for faster, more reliable builds

- **[Tracy](https://github.com/wolfpld/tracy) Profiler Integration**
  Integrated Tracy for real-time performance profiling, allowing you to visualize and optimize your plotting code and
  NeoQCP itself.

- **Bug Fixes**  
  Patched critical issues from upstream:
    - Fixed OpenGL context handling with multiple OpenGL widgets
    - Fixed DPI scaling issues with OpenGL on MacOS and Windows

- **Planned Features**
    - Support more data containers like `std::vector`, `std::array`, and `std::span`
    - More customization points
    - C++ modernization
    - Experimental improvements:
        - Support for fp32 with OpenGL
        - Threaded OpenGL rendering

## 📥 Installation

### Prerequisites

- Qt6 Core, Gui, and Widgets modules
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
qtdeps = dependency('qt6', modules : ['Core','Widgets','Gui','Svg','PrintSupport', 'OpenGL'], method:'qmake')

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
meson setup -Dtests=true builddir
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
