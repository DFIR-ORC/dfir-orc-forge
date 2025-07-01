set(PORT_DEBUG ON)

vcpkg_download_distfile(
    ARCHIVE
    URLS "https://download.qt.io/official_releases/qt/6.8/6.8.3/submodules/qtbase-everywhere-src-6.8.3.zip"
    FILENAME "qtbase-everywhere-src-6.8.3.zip"
    SHA512 6403fc9d608504cf272c8b5d8babc913282eb33b105973286b1d91b6a1d5abc47c7c3bd6401cea348b96671a7b4622231eab056b441d86260766a2a6b5190e81
)

vcpkg_extract_source_archive_ex(
    OUT_SOURCE_PATH SOURCE_PATH
    ARCHIVE "${ARCHIVE}"
)

# vcpkg_check_features(
#     OUT_FEATURE_OPTIONS FEATURE_OPTIONS
#     FEATURES
#         static FEATURE_static
#         shared FEATURE_shared
# )

# TODO
# - file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")
# - license or VCPKG_POLICY_SKIP_COPYRIGHT_CHECK enabled or vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.md")
# - remove executables or use vcpkg_copy_tools
# - warning: ${CURRENT_PACKAGES_DIR}/debug/bin exists but should not in a static build. To suppress this message, add set(VCPKG_POLICY_DLLS_IN_STATIC_LIBRARY enabled)
# - warning: ${CURRENT_PACKAGES_DIR}/bin exists but should not in a static build. To suppress this message, add set(VCPKG_POLICY_DLLS_IN_STATIC_LIBRARY enabled)
# - warning: There should be no installed empty directories. Empty directories are not representable to several binary cache providers, git repositories, and are not considered semantic build outputs. You should either create a regular file inside each empty directory, or delete them with the following CMake. To suppress this message, add set(VCPKG_POLICY_ALLOW_EMPTY_FOLDERS enabled)
# - warning: There should be no absolute paths, such as the following, in an installed package. To suppress this message, add set(VCPKG_POLICY_SKIP_ABSOLUTE_PATHS_CHECK enabled)

# Building qtbase[brotli,concurrent,core,dbus,dnslookup,doubleconversion,freetype,gui,harfbuzz,icu,jpeg,network,opengl,openssl,pcre2,png,sql,sql-psql,sql-sqlite,testlib,thread,widgets,zstd]:x64-windows@6.8.3...

# if(VCPKG_TARGET_TRIPLET MATCHES "static")
#     list(APPEND OPTIONS -DOF_ENABLE_STATIC_QT=ON -DOF_ENABLE_STATIC_RUNTIME=ON)
# endif()

        # abstractbutton
        # abstractslider
        # accessibility
        # action
        # androiddeployqt
        # animation
        # appstore-compliant
        # batch_test_support
        # brotli
        # buttongroup
        # calendarwidget
        # cborstreamreader
        # cborstreamwriter
        # checkbox
        # clipboard
        # colordialog
        # colornames
        # columnview
        # combobox
        # commandlineparser
        # commandlinkbutton
        # completer
        # concatenatetablesproxymodel
        # concurrent
        # contextmenu
        # cpp-winrt
        # cssparser
        # cups
        # cursor
        # datawidgetmapper
        # datestring
        # datetimeedit
        # datetimeparser
        # desktopservices
        # dial
        # dialog
        # dialogbuttonbox
        # dnslookup
        # dockwidget
        # dom
        # draganddrop
        # dtls
        # easingcurve
        # effects
        # errormessage
        # filedialog
        # filesystemiterator
        # filesystemmodel
        # filesystemwatcher
        # fontcombobox
        # fontdialog
        # formlayout
        # freetype
        # fscompleter
        # future
        # gestures
        # graphicseffect
        # graphicsframecapture
        # graphicsview
        # groupbox
        # gssapi
        # highdpiscaling
        # hijricalendar
        # http
        # identityproxymodel
        # im
        # image_heuristic_mask
        # image_text
        # imageformat_bmp
        # imageformat_jpeg
        # imageformat_png
        # imageformat_ppm
        # imageformat_xbm
        # imageformat_xpm
        # imageformatplugin
        # inputdialog
        # islamiccivilcalendar
        # itemmodel
        # itemmodeltester
        # itemviews
        # jalalicalendar
        # keysequenceedit
        # label
        # lcdnumber
        # library
        # lineedit
        # listview
        # listwidget
        # localserver
        # macdeployqt
        # mainwindow
        # mdiarea
        # menu
        # menubar
        # messagebox
        # mimetype
        # movie
        # multiprocess
        # network
        # networkdiskcache
        # networkinterface
        # networklistmanager
        # networkproxy
        # ocsp
        # openssl-hash
        # pdf
        # permissions
        # picture
        # poll-exit-on-error
        # printdialog
        # printer
        # printpreviewdialog
        # printpreviewwidget
        # printsupport
        # process
        # processenvironment
        # progressbar
        # progressdialog
        # proxymodel
        # pushbutton
        # qmake
        # radiobutton
        # raster-64bit
        # raster-fp
        # regularexpression
        # relocatable
        # resizehandler
        # rubberband
        # sanitize_fuzzer_no_link
        # scrollarea
        # scrollbar
        # scroller
        # sessionmanager
        # settings
        # sha3-fast
        # sharedmemory
        # shortcut
        # sizegrip
        # slider
        # socks5
        # sortfilterproxymodel
        # spinbox
        # splashscreen
        # splitter
        # sql
        # sqlmodel
        # sspi
        # stackedwidget
        # standarditemmodel
        # statusbar
        # statustip
        # stringlistmodel
        # style-stylesheet
        # syntaxhighlighter
        # systemsemaphore
        # systemtrayicon
        # tabbar
        # tabletevent
        # tableview
        # tablewidget
        # tabwidget
        # temporaryfile
        # testlib
        # testlib_selfcover
        # textbrowser
        # textdate
        # textedit
        # texthtmlparser
        # textmarkdownreader
        # textmarkdownwriter
        # textodfwriter
        # thread
        # timezone
        # timezone_locale
        # toolbar
        # toolbox
        # toolbutton
        # tooltip
        # topleveldomain
        # translation
        # transposeproxymodel
        # treeview
        # treewidget
        # tuiotouch
        # udpsocket
        # undocommand
        # undogroup
        # undostack
        # undoview
        # valgrind
        # validator
        # wasm-exceptions
        # wasm-simd128
        # whatsthis
        # wheelevent
        # widgettextcontrol
        # windeployqt
        # wizard
        # xml
        # xmlstream
        # xmlstreamreader
        # xmlstreamwriter

# list(APPEND DISABLED_FEATURES
#     concurrent
#     dbus
#     network
#     sql
#     xml
#     png
#     jpeg
#     harfbuzz
#     freetype
#     testlib
#     printsupport
#     calendarwidget
#     cborstreamreader
#     cborstreamwriter
#     movie
#     multiprocess
#     process
#     testlib
# )

# fb or fbsupport
# skip: device_discovery ? or devicediscoverysupport
# exampleicons
# opengl
list(APPEND DISABLED_FEATURES
    concurrent
    dbus
    network
    sql
    xml
    png
    jpeg
    harfbuzz
    freetype
    testlib
    printsupport
)

set(FEATURE_STRINGS "")

foreach(FEATURE ${DISABLED_FEATURES})
    list(APPEND DISABLED_FEATURE_STRINGS "-DQT_FEATURE_${FEATURE}=OFF")
endforeach()

vcpkg_cmake_configure(
    SOURCE_PATH ${SOURCE_PATH}
    OPTIONS ${DISABLED_FEATURE_STRINGS}
)

vcpkg_cmake_install()

message(STATUS "### CURRENT_PACKAGES_DIR=${CURRENT_PACKAGES_DIR}")
message(STATUS "### VCPKG_BUILD_DIR=${VCPKG_BUILD_DIR}")

get_cmake_property(_variableNames VARIABLES)
list(SORT _variableNames)

foreach (_variableName ${_variableNames})
    message(STATUS "${_variableName}=${${_variableName}}")
endforeach()

# file(COPY
#     "{CURRENT_PACKAGES_DIR}/lib/cmake/"
#     DESTINATION
#         "${CURRENT_PACKAGES_DIR}/share/${PORT}"
#     )

# TODO: use TOOL_PATH

#file(COPY ${CURRENT_BUILDTREES_DIR}/lib/cmake ${CURRENT_BUILDTREES_DIR}/lib/cmake/)

vcpkg_cmake_config_fixup(
    PACKAGE_NAME qtbase
    CONFIG_PATH lib/cmake
)

# Get all items in the share/Qt6/ directory
message(STATUS "Moving and patching Qt cmake scripts")

# 'E:/dev/vcpkg/packages/qtbase_x64-windows-static/debug/lib/cmake/Qt6BuildInternals' does not exist.
#file(GLOB QT6_FILES "${CURRENT_PACKAGES_DIR}/lib/cmake/*")
#foreach(QT6_FILE ${QT6_FILES})
#    message(STATUS "Fixup \"${QT6_FILE}\"")
#    file(RELATIVE_PATH RELATIVE_FILE_PATH "${CURRENT_PACKAGES_DIR}" "${QT6_FILE}")
#    message(STATUS "    relative path: \"${RELATIVE_FILE_PATH}\"")
#    vcpkg_cmake_config_fixup(CONFIG_PATH ${RELATIVE_FILE_PATH})
#endforeach()


# file(GLOB_RECURSE QT6_FILES "${CURRENT_PACKAGES_DIR}/share/qtbase/*")

# foreach(QT6_FILE ${QT6_FILES})
#     if(IS_DIRECTORY "${QT6_FILE}")
#         continue()
#     endif()

#     set(QT6_DIR "${CURRENT_PACKAGES_DIR}/share/qtbase")
#     file(RELATIVE_PATH RELATIVE_FILE_PATH "${QT6_DIR}" "${QT6_FILE}")
#     set(NEW_FILE_PATH "${CURRENT_PACKAGES_DIR}/share/${RELATIVE_FILE_PATH}")

#     get_filename_component(NEW_FILE_DIR "${NEW_FILE_PATH}" DIRECTORY)
#     file(MAKE_DIRECTORY "${NEW_FILE_DIR}")

#     message(STATUS "Rename: ${QT6_FILE} to ${NEW_FILE_PATH}")
#     file(RENAME "${QT6_FILE}" "${NEW_FILE_PATH}")

#     # patch
#     file(READ "${NEW_FILE_PATH}" CONFIG_FILE_CONTENT)
#     string(REPLACE "/share/qtbase/" "/share/" MODIFIED_CONFIG_FILE_CONTENT "${CONFIG_FILE_CONTENT}")
#     file(WRITE "${NEW_FILE_PATH}" "${MODIFIED_CONFIG_FILE_CONTENT}")
# endforeach()


# file(GLOB_RECURSE QT6_FILES "${CURRENT_PACKAGES_DIR}/debug/share/qtbase/*")

# foreach(QT6_FILE ${QT6_FILES})
#     if(IS_DIRECTORY "${QT6_FILE}")
#         continue()
#     endif()

#     set(QT6_DIR "${CURRENT_PACKAGES_DIR}/debug/share/qtbase")
#     file(RELATIVE_PATH RELATIVE_FILE_PATH "${QT6_DIR}" "${QT6_FILE}")
#     set(NEW_FILE_PATH "${CURRENT_PACKAGES_DIR}/debug/share/${RELATIVE_FILE_PATH}")

#     get_filename_component(NEW_FILE_DIR "${NEW_FILE_PATH}" DIRECTORY)
#     file(MAKE_DIRECTORY "${NEW_FILE_DIR}")

#     message(STATUS "Rename: ${QT6_FILE} to ${NEW_FILE_PATH}")
#     file(RENAME "${QT6_FILE}" "${NEW_FILE_PATH}")

#     # patch
#     file(READ "${NEW_FILE_PATH}" CONFIG_FILE_CONTENT)
#     string(REPLACE "/share/qtbase/" "/share/" MODIFIED_CONFIG_FILE_CONTENT "${CONFIG_FILE_CONTENT}")
#     file(WRITE "${NEW_FILE_PATH}" "${MODIFIED_CONFIG_FILE_CONTENT}")
# endforeach()

foreach(_config debug release)
    if(_config MATCHES "debug")
        set(path_suffix debug/)
    else()
        set(path_suffix)
    endif()
    if(NOT EXISTS "${CURRENT_PACKAGES_DIR}/${path_suffix}bin")
        continue()
    endif()
    file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/tools/Qt6/bin/${path_suffix}")
    foreach(other_file IN LISTS other_files)
        if(EXISTS "${CURRENT_PACKAGES_DIR}/${path_suffix}bin/${other_file}")
            set(target_file "${CURRENT_PACKAGES_DIR}/tools/Qt6/bin/${path_suffix}${other_file}")
            file(RENAME "${CURRENT_PACKAGES_DIR}/${path_suffix}bin/${other_file}" "${target_file}")
            file(READ "${target_file}" _contents)
            if(_config MATCHES "debug")
                string(REPLACE "..\\share\\" "..\\..\\..\\..\\share\\" _contents "${_contents}")
                string(REPLACE "../share/" "../../../../share/" _contents "${_contents}")
            else()
                string(REPLACE "..\\share\\" "..\\..\\..\\share\\" _contents "${_contents}")
                string(REPLACE "../share/" "../../../share/" _contents "${_contents}")
            endif()
            string(REGEX REPLACE "set cmake_path=[^\n]+\n" "set cmake_path=cmake\n" _contents "${_contents}")
            string(REGEX REPLACE "original_cmake_path=[^\n]+\n" "original_cmake_path=does-not-exist\n" _contents "${_contents}")
            file(WRITE "${target_file}" "${_contents}")
        endif()
    endforeach()
endforeach()

# file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/share/qtbase")

vcpkg_copy_tools(
    TOOL_NAMES
        androiddeployqt
        androidtestrunner
        cmake_automoc_parser
        moc
        qlalr
        qmake
        qtpaths
        qvkgen
        rcc
        syncqt
        tracegen
        tracepointgen
        uic
        windeployqt
    DESTINATION ${CURRENT_PACKAGES_DIR}/tools/qtbase
    AUTO_CLEAN
)

# vcpkg_configure_cmake(
#     SOURCE_PATH ${SOURCE_PATH}
#     PREFER_NINJA
#     OPTIONS
#         -DQT_BUILD_EXAMPLES=OFF
#         -DQT_BUILD_TESTS=OFF
#         ${OPTIONS}
# )

#vcpkg_install_cmake()

#vcpkg_fixup_cmake_targets(CONFIG_PATH lib/cmake/Qt6)


# -- CURRENT_BUILDTREES_DIR=E:/dev/dfir-orc-forge/external/vcpkg/buildtrees/qtbase
# -- CURRENT_HOST_INSTALLED_DIR=E:/dev/dfir-orc-forge/build-static/forge-static/vcpkg_installed/x64-windows
# -- CURRENT_INSTALLED_DIR=E:/dev/dfir-orc-forge/build-static/forge-static/vcpkg_installed/x64-windows-static
# -- CURRENT_PACKAGES_DIR=E:/dev/dfir-orc-forge/external/vcpkg/packages/qtbase_x64-windows-static
# -- CURRENT_PORT_DIR=E:/dev/dfir-orc-forge/external/vcpkg_overlay_ports/qtbase
# -- CURRENT_PORT_DIR=E:/dev/dfir-orc-forge/external/vcpkg_overlay_ports/qtbase

# e:\dev\dfir-orc-forge\external\vcpkg\buildtrees\qtbase\x64-windows-static-dbg\lib\cmake\

# PWD: E:/dev/dfir-orc-forge/external/vcpkg/

# file(COPY
#     "{CMAKE_CONFIG_DIR}/lib/cmake/"
#     DESTINATION
#         "${CURRENT_INSTALLED_DIR}/share/${PORT}"
#     )

# file(COPY
#     "${CURRENT_BUILDTREES_DIR}/lib/cmake/"
#     DESTINATION
#         "${CURRENT_INSTALLED_DIR}/share/${PORT}"
#     )

# Install release configuration files
# file(INSTALL
#     "${SOURCE_PATH}/cmake/Qt6Config.cmake"
#     DESTINATION "${CURRENT_PACKAGES_DIR}/share/Qt6"
# )

# Install debug configuration files
# file(INSTALL
#     "${SOURCE_PATH}/lib/cmake/Qt6/Qt6Config.cmake"
#     DESTINATION "${CURRENT_PACKAGES_DIR}/debug/share/Qt6"
# )

file(REMOVE_RECURSE ${CURRENT_PACKAGES_DIR}/debug/include)

vcpkg_copy_pdbs()

#file(INSTALL ${SOURCE_PATH}/LICENSES/ DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)

# Fix CMake targets
# vcpkg_fixup_cmake_targets(
#     CONFIG_PATH lib/cmake/foobar
# )

# vcpkg_cmake_configure(
#     SOURCE_PATH "${SOURCE_PATH}"
#     OPTIONS
#         -DCMAKE_BUILD_TYPE=Release
#

# if(NOT BUILD_SHARED_LIBS)
#     message(STATUS "Configuring specific options for static build")
#     vcpkg_cmake_configure(
#         SOURCE_PATH "${SOURCE_PATH}"
#         OPTIONS
#             -DCMAKE_BUILD_TYPE=${BUILD_TYPE}
#             -DENABLE_STATIC_QT=ON
#             -DENABLE_STATIC_RUNTIME=ON)
# else()
#     message(STATUS "Configuring for shared library build")
#     vcpkg_cmake_configure(
#         SOURCE_PATH "${SOURCE_PATH}"
#         OPTIONS
#             -DCMAKE_BUILD_TYPE=${BUILD_TYPE}
#             -DENABLE_STATIC_QT=OFF
#             -DENABLE_STATIC_RUNTIME=OFF)
# endif()

# vcpkg_cmake_install()

# vcpkg_cmake_config_fixup()

#file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
