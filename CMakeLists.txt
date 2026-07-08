cmake_minimum_required(VERSION 3.20)
project(mybarrier VERSION 0.2.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(mybarrier
    src/main.cpp
    src/cli.cpp
    src/config.cpp
    src/daemon.cpp
    src/net.cpp
    src/protocol.cpp
    src/util.cpp
    src/platform.cpp
)

target_include_directories(mybarrier PRIVATE src)

if(MSVC)
    target_compile_options(mybarrier PRIVATE /W4)
else()
    target_compile_options(mybarrier PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    target_link_libraries(mybarrier PRIVATE ws2_32)
else()
    target_link_libraries(mybarrier PRIVATE pthread)
endif()

install(TARGETS mybarrier RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES README.md docs/build-artifacts.md docs/implementation.md docs/barrier-gap-analysis.md
        DESTINATION ${CMAKE_INSTALL_DOCDIR})

set(CPACK_PACKAGE_NAME "mybarrier")
set(CPACK_PACKAGE_VENDOR "mybarrier")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Cross-platform LAN keyboard, clipboard, and file sharing prototype")
set(CPACK_PACKAGE_FILE_NAME "mybarrier-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_PACKAGE_CONTACT "mybarrier maintainers")

if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_DISPLAY_NAME "mybarrier")
    set(CPACK_NSIS_PACKAGE_NAME "mybarrier")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop;TGZ;ZIP")
    set(CPACK_DMG_VOLUME_NAME "mybarrier")
else()
    set(CPACK_GENERATOR "DEB;TGZ;ZIP")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "mybarrier maintainers")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libstdc++6")
endif()

include(CPack)
