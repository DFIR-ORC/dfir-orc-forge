cmake_minimum_required(VERSION 3.29)

option(ENABLE_STATIC_RUNTIME "Enable static runtime build" ON)
option(ENABLE_STATIC_QT "Enable static link for Qt" ON)
option(ENABLE_DOCUMENTATION "Embed DFIR-ORC documentation" ON)
option(BUILD_QT_BASE "Build Qt from minimal sources" OFF)

set(DOF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

if(NOT VCPKG_ROOT)
    set(VCPKG_ROOT ${DOF_ROOT}/external/vcpkg)
    set(ENV{VCPKG_ROOT} ${VCPKG_ROOT})
endif()

message(STATUS "Using VCPKG from ${VCPKG_ROOT}")

set(VCPKG_OVERLAY_PORTS "${DOF_ROOT}/external/vcpkg_overlay_ports")
set(VCPKG_OVERLAY_TRIPLETS "${DOF_ROOT}/external/vcpkg_overlay_triplets")

# Setup repository related variables and force refresh on new commits
include(cmake/GitInfo.cmake)
git_monitor_status()
git_info(
    REPO_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    VERSION_FROM_SEMVER_TAG DOF_VERSION
    BRANCH DOF_BRANCH
    COMMIT DOF_COMMIT
)

project(dfir-orc-forge)

if (BUILD_QT_BASE)
    include(cmake/BuildQt.cmake)  # set QT_INSTALL_DIR_*
    set(QTDIR ${QT_INSTALL_DIR_MinSizeRel} CACHE STRING "Qt directory")
endif()

if (DEFINED QTDIR)
    set(ENV{QTDIR} ${QTDIR})
endif()

if (DEFINED ENV{QTDIR})
    file(GLOB cmake_dirs LIST_DIRECTORIES true "$ENV{QTDIR}/lib/cmake/*")
    foreach(dir ${cmake_dirs})
        list(INSERT CMAKE_PREFIX_PATH 0 "${dir}")
    endforeach()
    message(STATUS "Using Qt from $ENV{QTDIR}")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

include(cmake/BuildRcedit.cmake)  # sets RCEDIT_EXECUTABLE

add_subdirectory(src)
