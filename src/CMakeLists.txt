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
    utils/guard.h
    utils/hresult.h
    utils/hresult.cpp
    utils/log.h
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
    7zip::7zip
    7zip::extras
    unofficial::libyara::libyara
)

if (ENABLE_DOCUMENTATION)
    set(DFIRORC_DOC "${CMAKE_BINARY_DIR}/dfir-orc.github.io.zip")

    add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        WORKING_DIRECTORY "${DOF_ROOT}/external/dfir-orc.github.io"
        COMMAND ${CMAKE_COMMAND} -E tar cf ${DFIRORC_DOC} --format=zip "docs"
        COMMAND ${CMAKE_COMMAND} -E echo "Embedding DFIR-ORC documentation ${DFIRORC_DOC}"
        COMMAND ${RCEDIT_EXECUTABLE}
            set --type "RT_RCDATA"
                --name "DOC"
                --value-path ${DFIRORC_DOC}
                $<TARGET_FILE:${PROJECT_NAME}>
        VERBATIM
    )
endif()
