cmake_minimum_required(VERSION 3.29)

option(ENABLE_STATIC_RUNTIME "Enable static runtime build" ON)
option(ENABLE_STATIC_QT "Enable static link for Qt" ON)
option(BUILD_QT_BASE "Build Qt from minimal sources" OFF)

# rcedit embeds the resources below; none of those calls compresses, so its
# codecs are off by default. They are built statically out of line when
# enabled, see cmake/BuildRcedit.cmake.
option(RCEDIT_ENABLE_7Z "Build rcedit with the 7z codec" OFF)
option(RCEDIT_ENABLE_ZSTD "Build rcedit with the zstd codec" OFF)

# DFIRORC_EXECUTABLE and DFIRORC_CAPSULE accept:
#   OFF                          disable embedding
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
# Both are unset by default. DFIRORC_REPO below fills in whichever is left
# unset, so a single release usually answers both.

# Both assets come from the same DFIR-ORC release, so name it once instead of
# repeating it. It seeds DFIRORC_EXECUTABLE and DFIRORC_CAPSULE, and only the
# ones left unset: an explicit value always wins, OFF included.
# Accepts LATEST, <version> or <repo-url>[,<version>] -- not <url> or <path>,
# which name one file and cannot stand for both assets.
# Examples:
#   -DDFIRORC_REPO=LATEST
#   -DDFIRORC_REPO=v10.3.4
#   -DDFIRORC_REPO=https://github.com/fork/dfir-orc,v10.3.4
set(DFIRORC_REPO "" CACHE STRING "DFIR-ORC release providing both the executable and the capsule: LATEST, <version> or <repo-url>[,<version>]")

# Optional: DFIR-ORC executable to embed as RT_RCDATA/"DFIR-ORC"
# Examples:
#   -DDFIRORC_EXECUTABLE=LATEST
#   -DDFIRORC_EXECUTABLE=v10.3.4
#   -DDFIRORC_EXECUTABLE=https://github.com/fork/dfir-orc,v10.3.4
#   -DDFIRORC_EXECUTABLE=https://github.com/DFIR-ORC/dfir-orc/releases/download/v10.3.0/DFIR-ORC.exe
#   -DDFIRORC_EXECUTABLE=C:/path/to/DFIR-ORC.exe
set(DFIRORC_EXECUTABLE "" CACHE STRING "DFIR-ORC executable to embed into dfir-orc-forge: OFF, LATEST, <repo-url>[,<version>], <url> or <path>")

# Required: *empty* OrcCapsule.exe (from DFIR-ORC) used to wrap forge.exe into
# DFIR-ORC-Forge.exe. Unlike the executable above this one has to be answered:
# an unwrapped forge.exe is exposed to DLL side-loading, so building without it
# takes an explicit -DDFIRORC_CAPSULE=OFF.
# Examples:
#   -DDFIRORC_CAPSULE=LATEST
#   -DDFIRORC_CAPSULE=https://github.com/fork/dfir-orc,v10.3.4
#   -DDFIRORC_CAPSULE=C:/path/to/OrcCapsule.exe
set(DFIRORC_CAPSULE "" CACHE STRING "Empty OrcCapsule.exe used to wrap dfir-orc-forge: OFF, LATEST, <repo-url>[,<version>], <url> or <path>")

# Seed whatever DFIRORC_REPO can answer. These are plain variables shadowing the
# cache entries: the cache keeps holding the empty value, so every configure
# re-seeds from the current DFIRORC_REPO instead of freezing the release picked
# by the first one. DFIRORC_REPO is itself cached, as any -D is: -UDFIRORC_REPO
# to drop it.
if(DFIRORC_REPO)
    foreach(_dfirorc_var IN ITEMS DFIRORC_EXECUTABLE DFIRORC_CAPSULE)
        if("${${_dfirorc_var}}" STREQUAL "")
            set(${_dfirorc_var} "${DFIRORC_REPO}")
        endif()
    endforeach()
    unset(_dfirorc_var)
endif()

if("${DFIRORC_CAPSULE}" STREQUAL "")
    message(FATAL_ERROR
        "DFIRORC_CAPSULE is not set.\n"
        "forge.exe is meant to be wrapped into OrcCapsule as DFIR-ORC-Forge.exe: "
        "an unwrapped forge.exe resolves its DLLs from the current directory and "
        "is exposed to side-loading. Pick one:\n"
        "  -DDFIRORC_REPO=v10.3.4     resolve the capsule and the executable from one release\n"
        "  -DDFIRORC_CAPSULE=<spec>   resolve the capsule only\n"
        "  -DDFIRORC_CAPSULE=OFF      build an unwrapped forge.exe anyway (understood risk)")
endif()

if(NOT DFIRORC_CAPSULE)
    # Warn on every configure, not only on the one that opted out: the choice
    # lives in the cache and would otherwise be made once and never seen again.
    message(WARNING
        "DFIRORC_CAPSULE=OFF: forge.exe will not be wrapped into OrcCapsule and "
        "is exposed to DLL side-loading. No DFIR-ORC-Forge.exe will be produced.")
endif()

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
