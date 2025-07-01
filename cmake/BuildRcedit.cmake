# Builds rcedit as an independent, fully static tool and sets
# RCEDIT_EXECUTABLE to the result.
#
# rcedit links the static CRT, and its codecs link 7-Zip and zstd statically.
# Those come from vcpkg and therefore have to be built for x64-windows-static,
# which is not this project's triplet: dfir-orc-forge builds against
# x64-windows-static-md (/MD) for its own dependencies and for Qt. One vcpkg
# triplet cannot serve both, so add_subdirectory() would force rcedit's codecs
# off. Configure and build it as a separate CMake project instead, with its
# own triplet and its own vcpkg_installed tree, the way cmake/BuildQt.cmake
# builds a static Qt out of line. Nothing links the two together -- rcedit is
# only ever run as a POST_BUILD command -- so the two runtimes never meet.
#
# This runs at configure time, so the executable exists before the POST_BUILD
# commands that use it are generated. Both steps are idempotent and re-run on
# every configure: the sub-configure is a no-op once its cache is warm, and
# the sub-build is a no-op once rcedit is up to date, which is what makes a
# change to the options below, or a submodule bump, actually take effect.
# Unlike Qt, rcedit is small enough that this costs nothing worth guarding.

set(RCEDIT_SOURCE_DIR "${DOF_ROOT}/external/rcedit")

# The tool is a build-time dependency, so it is always built MinSizeRel: it
# does not have to follow the configuration dfir-orc-forge is built in, and on
# a multi-config generator that saves building it once per configuration.
set(RCEDIT_BUILD_CONFIG "MinSizeRel")
set(RCEDIT_BUILD_DIR "${CMAKE_BINARY_DIR}/rcedit-build")

# rcedit sets CMAKE_RUNTIME_OUTPUT_DIRECTORY to <build>/$<CONFIG>.
set(RCEDIT_EXE "${RCEDIT_BUILD_DIR}/${RCEDIT_BUILD_CONFIG}/rcedit.exe")

if(NOT EXISTS "${RCEDIT_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "${RCEDIT_SOURCE_DIR} is empty: run "
        "'git submodule update --init external/rcedit'")
endif()

set(RCEDIT_CMAKE_ARGS
    -S "${RCEDIT_SOURCE_DIR}"
    -B "${RCEDIT_BUILD_DIR}"
    -G "${CMAKE_GENERATOR}"
    # Handing it this project's vcpkg toolchain means rcedit's own vcpkg
    # submodule is never used, so building dfir-orc-forge does not need a
    # recursive submodule clone. (VCPKG_ROOT is not passed: rcedit only reads
    # it to locate a toolchain of its own, and passing it would just draw an
    # unused-variable warning on every configure.)
    "-DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
    -DVCPKG_TARGET_TRIPLET=x64-windows-static
    # A tree of its own: the ports here are built against the static CRT
    # and must not land in, or be taken from, this project's.
    "-DVCPKG_INSTALLED_DIR=${RCEDIT_BUILD_DIR}/vcpkg_installed"
    -DRCEDIT_ENABLE_7Z=${RCEDIT_ENABLE_7Z}
    -DRCEDIT_ENABLE_ZSTD=${RCEDIT_ENABLE_ZSTD}
    # rcedit's suite tests rcedit, not dfir-orc-forge; it is run from
    # rcedit's own presets.
    -DRCEDIT_BUILD_TESTS=OFF)

# VCPKG_OVERLAY_PORTS and VCPKG_OVERLAY_TRIPLETS are deliberately not
# passed: rcedit sets both to its own external/ overlays before project(),
# so the 7-Zip port and the static triplet it builds with are its own.

if(CMAKE_GENERATOR_PLATFORM)
    list(APPEND RCEDIT_CMAKE_ARGS -A "${CMAKE_GENERATOR_PLATFORM}")
endif()

if(CMAKE_GENERATOR_TOOLSET)
    list(APPEND RCEDIT_CMAKE_ARGS -T "${CMAKE_GENERATOR_TOOLSET}")
endif()

message(STATUS "Configure rcedit from ${RCEDIT_SOURCE_DIR}")
execute_process(
    COMMAND ${CMAKE_COMMAND} ${RCEDIT_CMAKE_ARGS}
    COMMAND_ERROR_IS_FATAL ANY
)

message(STATUS "Build rcedit in ${RCEDIT_BUILD_DIR}")
execute_process(
    COMMAND ${CMAKE_COMMAND}
        --build "${RCEDIT_BUILD_DIR}"
        --config ${RCEDIT_BUILD_CONFIG}
        --target rcedit
    COMMAND_ERROR_IS_FATAL ANY
)

if(NOT EXISTS "${RCEDIT_EXE}")
    message(FATAL_ERROR "rcedit build produced no ${RCEDIT_EXE}")
endif()

set(RCEDIT_EXECUTABLE "${RCEDIT_EXE}")
