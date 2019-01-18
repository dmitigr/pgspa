# -*- cmake -*-
# Copyright (C) Dmitry Igrishin
# For conditions of distribution and use, see files LICENSE.txt

project(dmitigr_pgspa)

cmake_minimum_required(VERSION 3.13)

if (NOT (UNIX OR WIN32))
  message(FATAL_ERROR "unsupported target platform")
endif()

set(PGSPA_VERSION_PART1 0)
set(PGSPA_VERSION_PART2 1)
add_definitions(-DPGSPA_VERSION_PART1=${PGSPA_VERSION_PART1})
add_definitions(-DPGSPA_VERSION_PART2=${PGSPA_VERSION_PART2})

# ------------------------------------------------------------------------------

if (UNIX)
  set(PGSPA_BIN_INSTALL_DIR "bin" CACHE
    STRING "Name of the binary directory relative to ${CMAKE_INSTALL_PREFIX}")
elseif(WIN32)
  set(PGSPA_BIN_INSTALL_DIR "" CACHE
    STRING "Name of the binary directory relative to ${CMAKE_INSTALL_PREFIX}")
endif()

set(PG_SHAREDIR "" CACHE
  PATH "SHAREDIR of the PostgreSQL installation")

if (NOT ("${PG_SHAREDIR}" STREQUAL ""))
  get_filename_component(PG_SHAREDIR "${PG_SHAREDIR}" ABSOLUTE)
  message("The PostgreSQL extensions will be installed to \"${PG_SHAREDIR}/extension\"")
endif()

# ------------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED ON)

# ------------------------------------------------------------------------------

find_package(dmitigr_pgfe REQUIRED)
find_package(dmitigr_internal REQUIRED)

include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(pgspa pgspa.cpp)
set(libraries_to_link dmitigr_pgfe dmitigr_internal)
if (WIN32)
  list(APPEND libraries_to_link Advapi32.lib)
endif()
target_link_libraries(pgspa ${libraries_to_link})

# ------------------------------------------------------------------------------

install(TARGETS pgspa
  RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/${PGSPA_BIN_INSTALL_DIR}")

# ------------------------------------------------------------------------------

add_subdirectory(sql)
