# The library only, for CMake projects. Tests, examples, the docs and the WebAssembly gallery
# build with Meson (meson.build), which stays the reference build.
cmake_minimum_required(VERSION 3.21)

project(NeoQCP VERSION 2.1.1 LANGUAGES CXX)

option(NEOQCP_WITH_LATEX "Typeset $...$ spans in labels with JKQTMathText" ON)
option(NEOQCP_TRACY "Instrument for the Tracy profiler" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

set(QT_NO_PRIVATE_MODULE_WARNING ON)  # NeoQCP renders through QRhi, which Qt ships as a private API
find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui GuiPrivate Widgets Svg PrintSupport ShaderTools)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

include(cmake/sources.cmake)
include(cmake/shaders.cmake)

neoqcp_embed_shaders(${CMAKE_CURRENT_BINARY_DIR}/embedded_shaders.h ${NEOQCP_SHADERS})

set(NEOQCP_VERSION ${PROJECT_VERSION})
set(NEOQCP_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(NEOQCP_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(NEOQCP_VERSION_PATCH ${PROJECT_VERSION_PATCH})
if(NEOQCP_WITH_LATEX)
    set(NEOQCP_WITH_LATEX_VALUE 1)
else()
    set(NEOQCP_WITH_LATEX_VALUE 0)
endif()
configure_file(cmake/neoqcp_config.h.in neoqcp_config.h @ONLY)

add_library(NeoQCP STATIC
    ${NEOQCP_SOURCES}
    ${NEOQCP_MOC_HEADERS}
    ${CMAKE_CURRENT_BINARY_DIR}/embedded_shaders.h)
add_library(NeoQCP::NeoQCP ALIAS NeoQCP)

target_include_directories(NeoQCP PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<INSTALL_INTERFACE:include/NeoQCP>)
target_link_libraries(NeoQCP PUBLIC
    Qt6::Core Qt6::Gui Qt6::GuiPrivate Qt6::Widgets Qt6::Svg Qt6::PrintSupport)

if(NEOQCP_WITH_LATEX)
    include(cmake/jkqtmathtext.cmake)
    target_sources(NeoQCP PRIVATE src/axis/latexlabelrenderer.cpp)
    neoqcp_link_jkqtmathtext(NeoQCP)
endif()

if(NEOQCP_TRACY)
    include(FetchContent)
    FetchContent_Declare(tracy GIT_REPOSITORY https://github.com/wolfpld/tracy.git GIT_TAG master GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(tracy)
    target_link_libraries(NeoQCP PUBLIC TracyClient)
endif()

include(cmake/install.cmake)
