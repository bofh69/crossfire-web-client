cmake_minimum_required(VERSION 2.8)
project(crossfire-client C)

# Set project version information.
set(VERSION 1.71.0)

# Set configuration options.
option(LUA "Lua scripting" OFF)
option(METASERVER2 "Metaserver2 support" ON)
option(OPENGL "OpenGL rendering" ON)
option(SDL "SDL rendering" ON)
option(SOUND "Sound support" ON)

# Set compiler to use C99 standard.
set(CMAKE_C_FLAGS "-std=c99")

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
    find_package(Lua51)
    include_directories(${LUA_INCLUDE_DIR})
endif()

if(METASERVER2)
    find_package(CURL)
    include_directories(${CURL_INCLUDE_DIRS})
endif()

if(OPENGL)
    find_package(OpenGL)
    include_directories(${OPENGL_INCLUDE_DIR})
endif()

if(SDL)
    find_package(SDL)
    include_directories(${SDL_INCLUDE_DIR})
endif()

if(SOUND)
    find_package(SDL_mixer)
    include_directories(${SDL_MIXER_INCLUDE_DIRS})
endif()

# Set compatibility definitions for Autotools.
set(HAVE_CURL_CURL_H ${CURL_FOUND})
set(HAVE_OPENGL ${OPENGL_FOUND})
set(HAVE_SDL ${SDL_FOUND})

include(CheckIncludeFiles)
check_include_files(fcntl.h HAVE_FCNTL_H)
check_include_files(pthread.h HAVE_PTHREAD_H)
check_include_files(sys/time.h HAVE_SYS_TIME_H)
check_include_files(unistd.h HAVE_UNISTD_H)

include(CheckFunctionExists)
check_function_exists(getaddrinfo HAVE_GETADDRINFO)
check_function_exists(sysconf HAVE_SYSCONF)

# Write configuration to header file.
configure_file(
    "${PROJECT_SOURCE_DIR}/config.h.in"
    "${PROJECT_BINARY_DIR}/config.h"
)

# Tell the compiler where to find the header files.
include_directories(
    ${PROJECT_BINARY_DIR}
    ${PROJECT_BINARY_DIR}/common
    ${GTK2_INCLUDE_DIRS}
    ${PNG_INCLUDE_DIRS}
    ${X11_INCLUDE_DIR}
)

# Add temporary hacks to pick up angled brackets.
include_directories(${PROJECT_SOURCE_DIR}/common)

# Build in subdirectories.
add_subdirectory(common)
add_subdirectory(gtk-v2)

if(SOUND)
    add_subdirectory(sound-src)
endif()
