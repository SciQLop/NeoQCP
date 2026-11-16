# xtl 0.7.7 ships a CMake build only, and its project() carries no version, which the xtensor wrap
# needs. This overlay is the header-only equivalent, versioned from the release the wrap pins.
project('xtl', 'cpp', version: '0.7.7', license: 'BSD-3-Clause',
        default_options: ['warning_level=3', 'cpp_std=c++14'])

xtl_dep = declare_dependency(
    include_directories: include_directories('include', is_system: true),
    version: meson.project_version())
