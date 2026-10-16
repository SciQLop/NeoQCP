# A project that uses NeoQCP the way a CMake user would, built by CI (.github/workflows/cmake.yml).
#   -DNEOQCP_SOURCE_DIR=<checkout>  FetchContent from that checkout
#   otherwise                       find_package(NeoQCP), from an installed NeoQCP
cmake_minimum_required(VERSION 3.21)
project(neoqcp_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

if(NEOQCP_SOURCE_DIR)
    include(FetchContent)
    FetchContent_Declare(NeoQCP SOURCE_DIR ${NEOQCP_SOURCE_DIR})
    FetchContent_MakeAvailable(NeoQCP)
else()
    find_package(NeoQCP REQUIRED)
endif()

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE NeoQCP::NeoQCP Qt6::Widgets)
