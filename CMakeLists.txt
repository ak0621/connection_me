cmake_minimum_required(VERSION 3.20)
project(mybarrier VERSION 0.2.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

function(mybarrier_apply_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(mybarrier_core STATIC
    src/barrier_config.cpp
    src/cli.cpp
    src/config.cpp
    src/daemon.cpp
    src/net.cpp
    src/protocol.cpp
    src/util.cpp
    src/platform.cpp
)

target_include_directories(mybarrier_core PUBLIC src)
mybarrier_apply_warnings(mybarrier_core)

if(WIN32)
    target_compile_definitions(mybarrier_core PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(mybarrier_core PUBLIC ws2_32)
else()
    target_link_libraries(mybarrier_core PUBLIC pthread)
endif()

add_executable(mybarrier src/main.cpp)
target_link_libraries(mybarrier PRIVATE mybarrier_core)
mybarrier_apply_warnings(mybarrier)

set(MYBARRIER_BUILD_GUI "AUTO" CACHE STRING "Build the Qt desktop GUI: AUTO, ON, or OFF")
set_property(CACHE MYBARRIER_BUILD_GUI PROPERTY STRINGS AUTO ON OFF)
string(TOUPPER "${MYBARRIER_BUILD_GUI}" MYBARRIER_BUILD_GUI_UPPER)
if(NOT MYBARRIER_BUILD_GUI_UPPER MATCHES "^(AUTO|ON|OFF)$")
    message(FATAL_ERROR "MYBARRIER_BUILD_GUI must be AUTO, ON, or OFF")
endif()

set(MYBARRIER_GUI_AVAILABLE OFF)
if(NOT MYBARRIER_BUILD_GUI_UPPER STREQUAL "OFF")
    find_package(Qt6 COMPONENTS Widgets QUIET)
    if(Qt6_FOUND)
        set(mybarrier_gui_options)
        if(WIN32)
            list(APPEND mybarrier_gui_options WIN32)
        endif()
        if(APPLE)
            list(APPEND mybarrier_gui_options MACOSX_BUNDLE)
        endif()

        add_executable(mybarrier-gui ${mybarrier_gui_options}
            src/gui/main.cpp
            src/gui/MainWindow.cpp
            src/gui/MainWindow.h
        )
        target_link_libraries(mybarrier-gui PRIVATE mybarrier_core Qt6::Widgets)
        mybarrier_apply_warnings(mybarrier-gui)
        set_target_properties(mybarrier-gui PROPERTIES
            OUTPUT_NAME "MyBarrier"
            MACOSX_BUNDLE_BUNDLE_NAME "MyBarrier"
            MACOSX_BUNDLE_GUI_IDENTIFIER "org.mybarrier.MyBarrier"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        )
        set(MYBARRIER_GUI_AVAILABLE ON)

        if(COMMAND qt_generate_deploy_app_script)
            if(Qt6_VERSION VERSION_LESS "6.5.0")
                qt_generate_deploy_app_script(
                    TARGET mybarrier-gui
                    FILENAME_VARIABLE mybarrier_gui_deploy_script
                    NO_UNSUPPORTED_PLATFORM_ERROR
                )
            else()
                qt_generate_deploy_app_script(
                    TARGET mybarrier-gui
                    OUTPUT_SCRIPT mybarrier_gui_deploy_script
                    NO_UNSUPPORTED_PLATFORM_ERROR
                )
            endif()
        endif()
    elseif(MYBARRIER_BUILD_GUI_UPPER STREQUAL "ON")
        message(FATAL_ERROR "Qt6 Widgets was not found. Install Qt 6 or configure with -DMYBARRIER_BUILD_GUI=OFF.")
    else()
        message(STATUS "Qt6 Widgets was not found; building CLI only.")
    endif()
endif()

set(MYBARRIER_DOC_FILES README.md docs/build-artifacts.md docs/implementation.md docs/barrier-gap-analysis.md)
set(MYBARRIER_APP_ICON packaging/icons/mybarrier.svg)

if(APPLE AND MYBARRIER_GUI_AVAILABLE)
    install(TARGETS mybarrier-gui BUNDLE DESTINATION .)
    if(DEFINED mybarrier_gui_deploy_script)
        install(SCRIPT ${mybarrier_gui_deploy_script})
    endif()
    install(PROGRAMS $<TARGET_FILE:mybarrier> DESTINATION "MyBarrier.app/Contents/MacOS" RENAME mybarrier)
    install(FILES ${MYBARRIER_DOC_FILES} DESTINATION "MyBarrier.app/Contents/Resources/docs")
    install(CODE [[
        find_program(CODESIGN_EXECUTABLE codesign)
        if(CODESIGN_EXECUTABLE AND EXISTS "${CMAKE_INSTALL_PREFIX}/MyBarrier.app")
            execute_process(
                COMMAND "${CODESIGN_EXECUTABLE}" --force --deep --sign - "${CMAKE_INSTALL_PREFIX}/MyBarrier.app"
                RESULT_VARIABLE MYBARRIER_CODESIGN_RESULT
                OUTPUT_VARIABLE MYBARRIER_CODESIGN_OUTPUT
                ERROR_VARIABLE MYBARRIER_CODESIGN_ERROR
            )
            if(NOT MYBARRIER_CODESIGN_RESULT EQUAL 0)
                message(FATAL_ERROR "Ad-hoc codesign failed: ${MYBARRIER_CODESIGN_OUTPUT}\n${MYBARRIER_CODESIGN_ERROR}")
            endif()
        else()
            message(WARNING "codesign was not found; MyBarrier.app will remain unsigned.")
        endif()
    ]])
else()
    install(TARGETS mybarrier RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    if(MYBARRIER_GUI_AVAILABLE)
        install(TARGETS mybarrier-gui
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            BUNDLE DESTINATION .
        )
        if(DEFINED mybarrier_gui_deploy_script)
            install(SCRIPT ${mybarrier_gui_deploy_script})
        endif()
        if(UNIX AND NOT APPLE)
            install(FILES packaging/mybarrier.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
            install(FILES ${MYBARRIER_APP_ICON}
                DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
                RENAME mybarrier.svg
            )
        endif()
    endif()
    install(FILES ${MYBARRIER_DOC_FILES} DESTINATION ${CMAKE_INSTALL_DOCDIR})
endif()

if(WIN32)
    set(MYBARRIER_PACKAGE_OS "windows")
elseif(APPLE)
    set(MYBARRIER_PACKAGE_OS "macos")
else()
    set(MYBARRIER_PACKAGE_OS "linux")
endif()

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" MYBARRIER_PACKAGE_PROCESSOR)
if(MYBARRIER_PACKAGE_PROCESSOR MATCHES "^(x86_64|amd64)$")
    set(MYBARRIER_PACKAGE_ARCH "x86-64")
elseif(MYBARRIER_PACKAGE_PROCESSOR MATCHES "^(aarch64|arm64)$")
    set(MYBARRIER_PACKAGE_ARCH "arm-64")
elseif(MYBARRIER_PACKAGE_PROCESSOR STREQUAL "")
    set(MYBARRIER_PACKAGE_ARCH "unknown")
else()
    string(REPLACE "_" "-" MYBARRIER_PACKAGE_ARCH "${MYBARRIER_PACKAGE_PROCESSOR}")
endif()

set(CPACK_PACKAGE_NAME "mybarrier")
set(CPACK_PACKAGE_VENDOR "mybarrier")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Cross-platform LAN keyboard, clipboard, and file sharing prototype")
set(CPACK_PACKAGE_FILE_NAME "MyBarrier-${PROJECT_VERSION}-${MYBARRIER_PACKAGE_OS}-${MYBARRIER_PACKAGE_ARCH}")
set(CPACK_PACKAGE_CONTACT "mybarrier maintainers")

if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_DISPLAY_NAME "MyBarrier")
    set(CPACK_NSIS_PACKAGE_NAME "MyBarrier")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    if(MYBARRIER_GUI_AVAILABLE)
        set(CPACK_PACKAGE_EXECUTABLES "MyBarrier" "MyBarrier")
        set(CPACK_CREATE_DESKTOP_LINKS "MyBarrier")
    endif()
elseif(APPLE)
    if(MYBARRIER_GUI_AVAILABLE)
        set(CPACK_GENERATOR "DragNDrop;TGZ;ZIP")
        set(CPACK_DMG_VOLUME_NAME "MyBarrier")
    else()
        set(CPACK_GENERATOR "TGZ;ZIP")
    endif()
else()
    set(CPACK_GENERATOR "DEB;TGZ;ZIP")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "mybarrier maintainers")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
endif()

include(CPack)
