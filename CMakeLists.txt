cmake_minimum_required(VERSION 2.8)
project(crossfire-client C)
set(VERSION 1.72.0)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

option(LUA "Lua scripting" OFF)
option(METASERVER2 "Metaserver2 support" ON)
option(OPENGL "OpenGL rendering" ON)
option(SDL "SDL rendering" ON)
option(SOUND "Sound support" ON)

if(UNIX)
    # If Linux or other Unix-like, get gio to work by defining _BSD_SOURCE
    add_definitions(-D_BSD_SOURCE)
elseif(MINGW OR WIN32)
    add_definitions(-DWIN32)
    set(CMAKE_INSTALL_PREFIX ".")
endif()

include(GNUInstallDirs)
set(CMAKE_INSTALL_DATADIR ${CMAKE_INSTALL_DATAROOTDIR}/crossfire-client)

# Look for required dependencies.
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK2 gtk+-2.0 gio-2.0 REQUIRED)
find_package(PNG REQUIRED)
find_package(Perl REQUIRED)
find_package(Vala REQUIRED)
find_package(X11 REQUIRED)
add_definitions(
    ${GTK2_CFLAGS_OTHER}
    ${PNG_DEFINITIONS}
)

include(${VALA_USE_FILE})

# Look for optional dependencies that are enabled using options.
if(LUA)
    find_package(Lua51 REQUIRED)
endif()

if(METASERVER2)
    pkg_check_modules(CURL libcurl REQUIRED)
    set(HAVE_CURL_CURL_H ${CURL_FOUND})
endif()

if(OPENGL)
    find_package(OpenGL REQUIRED)
    set(HAVE_OPENGL ${OPENGL_FOUND})
endif()

if(SDL)
    pkg_check_modules(SDL SDL_image REQUIRED)
    set(HAVE_SDL ${SDL_FOUND})
endif()

if(SOUND)
    pkg_check_modules(SDLMIXER SDL_mixer REQUIRED)
    add_subdirectory(sound-src)
endif()

include(CheckIncludeFiles)
set(CMAKE_REQUIRED_INCLUDES ${GTK2_INCLUDE_DIRS})
check_include_files(gio/gnetworking.h HAVE_GIO_GNETWORKING_H)

include(CheckFunctionExists)
check_function_exists(sysconf HAVE_SYSCONF)

configure_file(
    "${PROJECT_SOURCE_DIR}/config.h.in"
    "${PROJECT_BINARY_DIR}/config.h"
)

add_subdirectory(common)
add_subdirectory(gtk-v2)

enable_testing()

# Build an installation package.
include(InstallRequiredSystemLibraries)
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_VERSION "${VERSION}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
set(CPACK_SOURCE_GENERATOR "TBZ2")
include(CPack)
