# The user-guide snippets as a live gallery, built for the browser (Qt for WebAssembly).
# CMake, not meson: Qt's CMake does the wasm link steps (platform plugin, loader, html).
#
# From the repository root:
#   source ~/emsdk/emsdk_env.sh                      # Emscripten 4.0.7 for Qt 6.11
#   ~/Qt/6.11.2/wasm_multithread/bin/qt-cmake -S examples/wasm-gallery -B build-wasm -G Ninja \
#       -DCMAKE_BUILD_TYPE=Release -DQT_HOST_PATH=$HOME/Qt/6.11.2/gcc_64 -DQSB=$HOME/Qt/6.11.2/gcc_64/bin/qsb
#   cmake --build build-wasm && examples/wasm-gallery/build_site.sh build-wasm _site
#   (cd _site && python3 ../examples/wasm-gallery/serve.py 8765)   # http://127.0.0.1:8765/?plot=gallery_orbit
# CI (.github/workflows/pages.yml) does the same and publishes _site to GitHub Pages.
cmake_minimum_required(VERSION 3.21)
project(neoqcp_gallery LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg PrintSupport GuiPrivate WidgetsPrivate)

set(NEOQCP ${CMAKE_CURRENT_SOURCE_DIR}/../..)
file(GLOB_RECURSE NEOQCP_SOURCES ${NEOQCP}/src/*.cpp ${NEOQCP}/src/*.h)
list(FILTER NEOQCP_SOURCES EXCLUDE REGEX "latexlabelrenderer")

set(NEOQCP_VERSION_MAJOR 2)
set(NEOQCP_VERSION_MINOR 1)
set(NEOQCP_VERSION_PATCH 1)
file(WRITE ${CMAKE_BINARY_DIR}/gen/neoqcp_config.h
"#pragma once
#define NEOQCP_VERSION_MAJOR 2
#define NEOQCP_VERSION_MINOR 1
#define NEOQCP_VERSION_PATCH 1
#define NEOQCP_VERSION \"2.1.1\"
#define NEOQCP_WITH_LATEX 0
")

set(SHADERS composite.vert composite.frag plottable.vert plottable.frag span.vert
            contour_line.vert contour_line.frag scatter.vert scatter.frag
            scatter_colored.vert scatter_colored.frag)
set(QSB_FILES "")
set(EMBED_ARGS "")
foreach(shader ${SHADERS})
    set(out ${CMAKE_BINARY_DIR}/gen/${shader}.qsb)
    add_custom_command(OUTPUT ${out}
        COMMAND ${QSB} --qt6 -o ${out} ${NEOQCP}/src/painting/shaders/${shader}
        DEPENDS ${NEOQCP}/src/painting/shaders/${shader})
    string(REPLACE "." "_" var ${shader})
    list(APPEND QSB_FILES ${out})
    list(APPEND EMBED_ARGS "${var}_qsb_data:${out}")
endforeach()
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/gen/embedded_shaders.h
    COMMAND python3 ${NEOQCP}/src/painting/shaders/embed_shaders.py
            ${CMAKE_BINARY_DIR}/gen/embedded_shaders.h ${EMBED_ARGS}
    DEPENDS ${QSB_FILES})

file(GLOB SNIPPETS ${NEOQCP}/docs/snippets/*.cpp)
list(FILTER SNIPPETS EXCLUDE REGEX "/main\\.cpp$")

qt_add_executable(neoqcp_gallery gallery.cpp ${SNIPPETS} ${NEOQCP_SOURCES}
                  ${CMAKE_BINARY_DIR}/gen/embedded_shaders.h)
target_include_directories(neoqcp_gallery PRIVATE ${NEOQCP}/src ${NEOQCP}
                           ${NEOQCP}/docs/snippets ${CMAKE_BINARY_DIR}/gen)
target_link_libraries(neoqcp_gallery PRIVATE Qt6::Core Qt6::Gui Qt6::GuiPrivate Qt6::WidgetsPrivate Qt6::Widgets
                      Qt6::Svg Qt6::PrintSupport)
set_target_properties(neoqcp_gallery PROPERTIES QT_WASM_INITIAL_MEMORY "256MB")
