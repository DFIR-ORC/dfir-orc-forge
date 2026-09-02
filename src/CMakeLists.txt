set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(ENABLE_STATIC_RUNTIME)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(QT_COMPONENTS Core Widgets)
find_package(Qt6 COMPONENTS ${QT_COMPONENTS} REQUIRED)

find_package(7zip REQUIRED)
find_package(unofficial-libyara REQUIRED)

set(GENERATED_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/src")
configure_file(versionres.h.in "${GENERATED_SRC_DIR}/versionres.h" @ONLY)

qt_standard_project_setup()

set(SOURCE_FILES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    version.h
    version.cpp
    7z/archiveextractcallback.h
    7z/archiveextractcallback.cpp
    7z/archiveupdatecallback.h
    7z/archiveupdatecallback.cpp
    7z/compress.h
    7z/compress.cpp
    7z/extract.h
    7z/extract.cpp
    7z/inmemstream.h
    7z/inmemstream.cpp
    7z/lib7z.h
    7z/outmemstream.h
    7z/outmemstream.cpp
    editor/xmlconfigeditor.h
    editor/xmlconfigeditor.cpp
    editor/xmlcompleter.h
    editor/xmlcompleter.cpp
    editor/editorhub.h
    editor/editorhub.cpp
    editor/editorinterface.h
    editor/editorinterface.cpp
    editor/resourceeditor.h
    editor/resourceeditor.cpp
    editor/xmlhighlighter.h
    editor/xmlhighlighter.cpp
    editor/yaraeditor.h
    editor/yaraeditor.cpp
    editor/yarahighlighter.h
    editor/yarahighlighter.cpp
    formatter/formatter.h
    formatter/qstring.h
    formatter/std_expected.h
    formatter/std_optional.h
    formatter/xmlerror.h
    resource/resource.h
    resource/resource.cpp
    resource/resourceinfo.h
    resource/resourceinfo.cpp
    resource/resourcedirectory.h
    resource/resourcedirectory.cpp
    textedit/gotolinewidget.h
    textedit/gotolinewidget.cpp
    textedit/searchwidget.h
    textedit/searchwidget.cpp
    textedit/statuslabel.h
    textedit/statuslabel.cpp
    textedit/texteditwidget.h
    textedit/texteditwidget.cpp
    toolembed/embedconfiguration.h
    toolembed/embedconfiguration.cpp
    toolembed/embedfiles.h
    toolembed/embedfiles.cpp
    toolembed/processoutputview.h
    toolembed/processoutputview.cpp
    toolembed/toolembedsetupview.h
    toolembed/toolembedsetupview.cpp
    toolembed/toolembedview.h
    toolembed/toolembedview.cpp
    utils/guard.h
    utils/hresult.h
    utils/hresult.cpp
    utils/log.h
    utils/peinfo.h
    utils/peinfo.cpp
    utils/qtfileerror.h
    utils/win32.h
    utils/win32.cpp
    utils/xmlerror.h
    utils/xmlerror.cpp
    xmldom.h
    xmldom.cpp
    xsdstore.h
    xsdstore.cpp
)

set(ASSETS ${CMAKE_SOURCE_DIR}/assets/app.rc)

qt_add_executable(${PROJECT_NAME} WIN32 ${SOURCE_FILES} ${ASSETS})

source_group(TREE "${CMAKE_SOURCE_DIR}/src" FILES ${SOURCE_FILES})

set_target_properties(${PROJECT_NAME} PROPERTIES
    WIN32_EXECUTABLE TRUE
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    UNICODE
    NOMINMAX
)

target_compile_options(${PROJECT_NAME} PRIVATE /Zi /EHsc $<$<COMPILE_LANGUAGE:C,CXX>:/guard:cf>)
target_link_options(${PROJECT_NAME} PRIVATE
    /DEBUG
    $<$<NOT:$<CONFIG:Debug>>:/OPT:REF>
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${GENERATED_SRC_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    msxml6.lib
    shell32.lib
    shlwapi.lib
    version.lib
    winhttp.lib
    7zip::7zip
    7zip::extras
    unofficial::libyara::libyara
)

include(${DOF_ROOT}/cmake/DownloadFile.cmake)

# Report a resolved asset as '-- <label> [CACHED]: <value>', the '[CACHED]' tag
# telling that nothing was downloaded or cloned again.
function(message_asset label cached value)
    set(_tag "")
    if (cached)
        set(_tag " [CACHED]")
    endif()
    message(STATUS "${label}${_tag}: ${value}")
endfunction()

set(DFIRORC_REPO_URL "https://github.com/DFIR-ORC/dfir-orc")
set(DFIRORC_CONFIG_REPO_URL "https://github.com/DFIR-ORC/dfir-orc-config")
set(DFIRORC_DOC_REPO_URL "https://github.com/DFIR-ORC/dfir-orc.github.io")

if (DFIRORC_DOC)
    # Archive the documentation at configure time (entries are expected under
    # 'docs/'); the embedded commit is empty when unknown.
    fetch_repo_archive(
        SPEC "${DFIRORC_DOC}"
        REPO_URL "${DFIRORC_DOC_REPO_URL}"
        ARCHIVE "${CMAKE_BINARY_DIR}/dfir-orc.github.io.zip"
        # The rendered site (with its 'docs' directory) lives on gh-pages, the
        # default branch holds the sphinx sources.
        REF gh-pages
        DIRECTORY docs
        OUTPUT_ARCHIVE DFIRORC_DOC_ARCHIVE
        OUTPUT_COMMIT DFIRORC_DOC_COMMIT
        OUTPUT_CACHED DFIRORC_DOC_CACHED)

    set(_doc "${DFIRORC_DOC}")
    if (DFIRORC_DOC_COMMIT)
        string(APPEND _doc " (${DFIRORC_DOC_COMMIT})")
    endif()
    message_asset("DFIR-ORC documentation" "${DFIRORC_DOC_CACHED}" "${_doc}")

    add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Embedding DFIR-ORC documentation ${DFIRORC_DOC}"
        COMMAND ${RCEDIT_EXECUTABLE}
            set --type "RT_RCDATA"
                --name "DFIRORC_DOC"
                --value-path ${DFIRORC_DOC_ARCHIVE}
                $<TARGET_FILE:${PROJECT_NAME}>
        VERBATIM
    )

    if (DFIRORC_DOC_COMMIT)
        add_custom_command(
            TARGET ${PROJECT_NAME}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo
                "Embedding DFIR-ORC documentation commit ${DFIRORC_DOC_COMMIT}"
            COMMAND ${RCEDIT_EXECUTABLE}
                set --type "RT_RCDATA"
                    --name "DFIRORC_DOC_COMMIT"
                    --value "${DFIRORC_DOC_COMMIT}"
                    $<TARGET_FILE:${PROJECT_NAME}>
            VERBATIM
        )
    endif()
endif()

if (DFIRORC_CONFIG)
    # Archive the configuration at configure time; the embedded commit is
    # empty when unknown.
    fetch_repo_archive(
        SPEC "${DFIRORC_CONFIG}"
        REPO_URL "${DFIRORC_CONFIG_REPO_URL}"
        ARCHIVE "${CMAKE_BINARY_DIR}/dfir-orc-config.zip"
        REF fabienfl/10.3.0
        OUTPUT_ARCHIVE DFIRORC_CONFIG_ARCHIVE
        OUTPUT_COMMIT DFIRORC_CONFIG_COMMIT
        OUTPUT_CACHED DFIRORC_CONFIG_CACHED)

    set(_config "${DFIRORC_CONFIG}")
    if (DFIRORC_CONFIG_COMMIT)
        string(APPEND _config " (${DFIRORC_CONFIG_COMMIT})")
    endif()
    message_asset("DFIR-ORC configuration" "${DFIRORC_CONFIG_CACHED}" "${_config}")

    add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Embedding DFIR-ORC configuration ${DFIRORC_CONFIG}"
        COMMAND ${RCEDIT_EXECUTABLE}
            set --type "RT_RCDATA"
                --name "DFIRORC_CONFIG"
                --value-path ${DFIRORC_CONFIG_ARCHIVE}
                $<TARGET_FILE:${PROJECT_NAME}>
        VERBATIM
    )

    if (DFIRORC_CONFIG_COMMIT)
        add_custom_command(
            TARGET ${PROJECT_NAME}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo
                "Embedding DFIR-ORC configuration commit ${DFIRORC_CONFIG_COMMIT}"
            COMMAND ${RCEDIT_EXECUTABLE}
                set --type "RT_RCDATA"
                    --name "DFIRORC_CONFIG_COMMIT"
                    --value "${DFIRORC_CONFIG_COMMIT}"
                    $<TARGET_FILE:${PROJECT_NAME}>
            VERBATIM
        )
    endif()
endif()

# download_asset() replaces the spec with a local path: keep the spec to tell
# later whether both assets were seeded from the same DFIRORC_REPO.
set(_dfirorc_exe_spec "${DFIRORC_EXECUTABLE}")

if (DFIRORC_EXECUTABLE)
    download_asset(
        SPEC "${DFIRORC_EXECUTABLE}"
        REPO_URL "${DFIRORC_REPO_URL}"
        ASSET_REGEX "DFIR-ORC\\.exe"
        OUTPUT DFIRORC_EXECUTABLE
        OUTPUT_VERSION DFIRORC_RELEASE_VERSION
        OUTPUT_CACHED DFIRORC_EXECUTABLE_CACHED)

    if (NOT EXISTS "${DFIRORC_EXECUTABLE}")
        message(FATAL_ERROR "DFIRORC_EXECUTABLE does not exist: ${DFIRORC_EXECUTABLE}")
    endif()

    message_asset("DFIR-ORC executable" "${DFIRORC_EXECUTABLE_CACHED}" "${DFIRORC_EXECUTABLE}")

    add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Embedding DFIR-ORC executable ${DFIRORC_EXECUTABLE}"
        COMMAND ${RCEDIT_EXECUTABLE}
            set --type "RT_RCDATA"
                --name "DFIRORC_EXE"
                --value-path "${DFIRORC_EXECUTABLE}"
                $<TARGET_FILE:${PROJECT_NAME}>
        VERBATIM
    )
else()
    message(STATUS
        "DFIRORC_EXECUTABLE is not set. "
        "A DFIR-ORC executable can be embedded into dfir-orc-forge so that "
        "\"File > New DFIR-ORC\" automatically uses it as the pre-loaded resource. "
        "Set -DDFIRORC_EXECUTABLE=<path|url>, or -DDFIRORC_REPO=<version> to take "
        "it from the same release as the capsule, to enable this feature."
    )
endif()

if (DFIRORC_CAPSULE)
    # Identical specs mean DFIRORC_REPO seeded both: reuse the tag the executable
    # resolved to instead of asking for the latest release a second time, which a
    # release published in between would answer differently -- pairing forge.exe
    # with a capsule from another release.
    set(_dfirorc_pin "")
    if (DFIRORC_RELEASE_VERSION AND "${DFIRORC_CAPSULE}" STREQUAL "${_dfirorc_exe_spec}")
        set(_dfirorc_pin "${DFIRORC_RELEASE_VERSION}")
        message(STATUS
            "Pinning the DFIR-ORC capsule to ${DFIRORC_RELEASE_VERSION}, "
            "the release the executable resolved to")
    endif()

    download_asset(
        SPEC "${DFIRORC_CAPSULE}"
        REPO_URL "${DFIRORC_REPO_URL}"
        ASSET_REGEX "DFIR-ORC-binaries\\.zip/OrcCapsule.exe"
        VERSION "${_dfirorc_pin}"
        OUTPUT DFIRORC_CAPSULE
        OUTPUT_CACHED DFIRORC_CAPSULE_CACHED)

    if (NOT EXISTS "${DFIRORC_CAPSULE}")
        message(FATAL_ERROR "DFIRORC_CAPSULE does not exist: ${DFIRORC_CAPSULE}")
    endif()

    message_asset("DFIR-ORC capsule" "${DFIRORC_CAPSULE_CACHED}" "${DFIRORC_CAPSULE}")
    message(STATUS "${PROJECT_NAME} will be wrapped into OrcCapsule as DFIR-ORC-Forge.exe")

    set(DFIRORC_CAPSULE_OUTPUT "$<TARGET_FILE_DIR:${PROJECT_NAME}>/DFIR-ORC-Forge.exe")
    add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo
            "Wrapping $<TARGET_FILE_NAME:${PROJECT_NAME}> into OrcCapsule -> ${DFIRORC_CAPSULE_OUTPUT}"
        COMMAND "${DFIRORC_CAPSULE}" capsule add "$<TARGET_FILE:${PROJECT_NAME}>" --output "${DFIRORC_CAPSULE_OUTPUT}" --force
        VERBATIM
    )
else()
    # The configure-time warning scrolls away and the cache makes the opt-out
    # permanent: keep it in the build log of every unwrapped binary too.
    add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo
            "warning: DFIRORC_CAPSULE=OFF: $<TARGET_FILE_NAME:${PROJECT_NAME}> is not wrapped into OrcCapsule and is exposed to DLL side-loading"
        VERBATIM
    )
endif()

# The resources are embedded by POST_BUILD commands, which an up-to-date
# target skips: compile the identity of the embedded content so that changing
# any of it relinks the executable and re-runs the embedding.
string(SHA256 DFIRORC_EMBEDDED_ID
    "${DFIRORC_EXECUTABLE}|${DFIRORC_CAPSULE}|${DFIRORC_CONFIG}|${DFIRORC_CONFIG_COMMIT}|${DFIRORC_DOC}|${DFIRORC_DOC_COMMIT}")
file(CONFIGURE
    OUTPUT "${GENERATED_SRC_DIR}/embeddedid.cpp"
    CONTENT "extern const char* const kDfirOrcEmbeddedId = \"${DFIRORC_EMBEDDED_ID}\";\n"
)
target_sources(${PROJECT_NAME} PRIVATE "${GENERATED_SRC_DIR}/embeddedid.cpp")
