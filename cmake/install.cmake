# Installs the library, its headers and the files find_package(NeoQCP) reads.
# Off by default when NeoQCP is a subproject (FetchContent, add_subdirectory) of another build.
option(NEOQCP_INSTALL "Install NeoQCP" ${PROJECT_IS_TOP_LEVEL})

# simplify: a Tracy build is for profiling NeoQCP itself, so it is not installable (TracyClient
# would have to be exported too). Add that if someone needs an installed, instrumented NeoQCP.
if(NOT NEOQCP_INSTALL OR NEOQCP_TRACY)
    return()
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(NEOQCP_CMAKE_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/NeoQCP)

install(TARGETS NeoQCP EXPORT NeoQCPTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY src/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/NeoQCP
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/neoqcp_config.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/NeoQCP)
install(EXPORT NeoQCPTargets NAMESPACE NeoQCP:: DESTINATION ${NEOQCP_CMAKE_DIR})

configure_package_config_file(cmake/NeoQCPConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/NeoQCPConfig.cmake
    INSTALL_DESTINATION ${NEOQCP_CMAKE_DIR})
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/NeoQCPConfigVersion.cmake
    COMPATIBILITY SameMajorVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/NeoQCPConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/NeoQCPConfigVersion.cmake
    DESTINATION ${NEOQCP_CMAKE_DIR})
