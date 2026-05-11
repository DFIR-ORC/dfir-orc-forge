cmake_minimum_required(VERSION 3.29)

option(ENABLE_STATIC_RUNTIME "Enable static runtime build" ON)
option(ENABLE_STATIC_QT "Enable static link for Qt" ON)
option(BUILD_QT_BASE "Build Qt from minimal sources" OFF)

# DFIRORC_EXECUTABLE accepts:
#   LATEST                       release asset of https://github.com/DFIR-ORC/dfir-orc
#   <version>                    a release tag (e.g. v10.3.4) of the default
#                                repository above
#   <repo-url>[,<version>]       release asset of a GitHub or GitLab repository,
#                                latest release unless a tag is appended with
#                                ',' or ';' (e.g. ...,v10.3.4); ssh remotes
#                                (git@github.com:DFIR-ORC/dfir-orc) identify
#                                the repository too, assets stay over https
#   <url>                        direct URL of the file (may name an archive
#                                member: .../DFIR-ORC-binaries.zip/OrcCapsule.exe)
#   <path>                       local file

# Optional: DFIR-ORC executable to embed as RT_RCDATA/"DFIR-ORC"
# Examples:
#   -DDFIRORC_EXECUTABLE=LATEST
#   -DDFIRORC_EXECUTABLE=v10.3.4
#   -DDFIRORC_EXECUTABLE=https://github.com/fork/dfir-orc,v10.3.4
#   -DDFIRORC_EXECUTABLE=https://github.com/DFIR-ORC/dfir-orc/releases/download/v10.3.0/DFIR-ORC.exe
#   -DDFIRORC_EXECUTABLE=C:/path/to/DFIR-ORC.exe
set(DFIRORC_EXECUTABLE "" CACHE STRING "DFIR-ORC executable to embed into dfir-orc-forge: OFF, LATEST, <repo-url>[,<version>], <url> or <path>")

option(DFIRORC_CONFIGURATION "Embed DFIR-ORC configuration" ON)
option(DFIRORC_DOCUMENTATION "Embed DFIR-ORC documentation" ON)

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

# Submodule commit hashes (empty string when not present)
git_info(
    REPO_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/external/dfir-orc-config
    COMMIT DFIRORC_CONFIG_COMMIT
)
git_info(
    REPO_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/external/dfir-orc.github.io
    COMMIT DFIRORC_DOC_COMMIT
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
