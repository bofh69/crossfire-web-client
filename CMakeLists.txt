cmake_minimum_required(VERSION 2.8)
project(crossfire-client C)
set(VERSION 1.71.0)

# Set configuration options.
option(LUA "Lua scripting" OFF)
option(METASERVER2 "Metaserver2 support" ON)
option(OPENGL "OpenGL rendering" ON)
option(SDL "SDL rendering" ON)
option(SOUND "Sound support" ON)

# Set compiler to use C99 standard.
add_definitions(-std=c99)

# If Linux or other Unix-like, get gio to work by defining _BSD_SOURCE
if (UNIX)
    add_definitions(-D_BSD_SOURCE)
endif()

# Set standard installation paths.
include(GNUInstallDirs)
set(CMAKE_INSTALL_DATADIR ${CMAKE_INSTALL_DATAROOTDIR}/crossfire-client)

# Look for required dependencies.
find_package(GTK2 REQUIRED)
find_package(PNG REQUIRED)
find_package(Perl REQUIRED)
find_package(X11 REQUIRED)

add_definitions(
    ${GTK2_DEFINITIONS}
    ${PNG_DEFINITIONS}
)

# Look for optional dependencies that are enabled using options.
if(LUA)
    find_package(Lua51 REQUIRED)
    include_directories(${LUA_INCLUDE_DIR})
endif()

if(METASERVER2)
    find_package(CURL REQUIRED)
    include_directories(${CURL_INCLUDE_DIRS})
endif()

if(OPENGL)
    find_package(OpenGL REQUIRED)
    include_directories(${OPENGL_INCLUDE_DIR})
endif()

if(SDL)
    find_package(SDL REQUIRED)
    find_package(SDL_image REQUIRED)
    include_directories(${SDL_INCLUDE_DIR})
endif()

if(SOUND)
    find_package(SDL_mixer REQUIRED)
    include_directories(${SDL_MIXER_INCLUDE_DIRS})
endif()

# Set compatibility definitions for Autotools.
set(HAVE_CURL_CURL_H ${CURL_FOUND})
set(HAVE_OPENGL ${OPENGL_FOUND})
set(HAVE_SDL ${SDL_FOUND})

include(CheckIncludeFiles)
set(CMAKE_REQUIRED_INCLUDES ${GTK2_INCLUDE_DIRS})
check_include_files(gio/gnetworking.h HAVE_GIO_GNETWORKING_H)

include(CheckFunctionExists)
check_function_exists(sysconf HAVE_SYSCONF)

# Add extra platform definitions for systems that need it.
if(WIN32)
    add_definitions(-DWIN32)
    find_library(WSOCK32_LIBRARY wsock32)
    find_library(WINMM_LIBRARY winmm)
endif()

# Write configuration to header file.
configure_file(
    "${PROJECT_SOURCE_DIR}/config.h.in"
    "${PROJECT_BINARY_DIR}/config.h"
)

# Tell the compiler where to find the header files.
include_directories(
    ${PROJECT_BINARY_DIR}
    ${PROJECT_BINARY_DIR}/common
    ${PROJECT_SOURCE_DIR}/common
    ${GTK2_INCLUDE_DIRS}
    ${PNG_INCLUDE_DIRS}
    ${X11_INCLUDE_DIR}
)

# Build in subdirectories.
add_subdirectory(common)
add_subdirectory(gtk-v2)

if(SOUND)
    add_subdirectory(sound-src)
endif()

# Build an installation package.
include(InstallRequiredSystemLibraries)
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_VERSION "${VERSION}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
set(CPACK_SOURCE_GENERATOR "TBZ2")
include(CPack)
