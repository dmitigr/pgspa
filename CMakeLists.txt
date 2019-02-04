# -*- cmake -*-
# Copyright (C) Dmitry Igrishin
# For conditions of distribution and use, see files LICENSE.txt

project(dmitigr_pgspa)

cmake_minimum_required(VERSION 3.13)

if (NOT (UNIX OR WIN32))
  message(FATAL_ERROR "unsupported target platform")
endif()

set(dmitigr_pgspa_version_part1 0)
set(dmitigr_pgspa_version_part2 1)
add_definitions(-DPGSPA_VERSION_PART1=${dmitigr_pgspa_version_part1})
add_definitions(-DPGSPA_VERSION_PART2=${dmitigr_pgspa_version_part2})

# ------------------------------------------------------------------------------

if (UNIX)
  set(DMITIGR_PGSPA_BIN_INSTALL_DIR "bin" CACHE
    STRING "Name of the installation directory for the programs relative to ${CMAKE_INSTALL_PREFIX}")
elseif(WIN32)
  set(DMITIGR_PGSPA_BIN_INSTALL_DIR "" CACHE
    STRING "Name of the installation directory for the programs relative to ${CMAKE_INSTALL_PREFIX}")
endif()

set(DMITIGR_PGSPA_PG_SHAREDIR "" CACHE
  PATH "SHAREDIR of the PostgreSQL installation")

if (NOT ("${DMITIGR_PGSPA_PG_SHAREDIR}" STREQUAL ""))
  get_filename_component(DMITIGR_PGSPA_PG_SHAREDIR "${DMITIGR_PGSPA_PG_SHAREDIR}" ABSOLUTE)
  message("The PostgreSQL extensions will be installed to \"${DMITIGR_PGSPA_PG_SHAREDIR}/extension\"")
endif()

# ------------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED ON)

# ------------------------------------------------------------------------------

set(CMAKE_MODULE_PATH ${dmitigr_pgspa_SOURCE_DIR}/cmake)

set(libraries_to_link)

find_package(Internal REQUIRED)
message("Using ${dmitigr_internal_library} library")
list(APPEND libraries_to_link ${dmitigr_internal_library})

find_package(dmitigr_pgfe REQUIRED)
message("Using dmitigr_pgfe library")
list(APPEND libraries_to_link dmitigr_pgfe)

include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(pgspa pgspa.cpp)
if (WIN32)
  list(APPEND libraries_to_link Advapi32.lib)
endif()
target_link_libraries(pgspa PRIVATE ${libraries_to_link})

# ------------------------------------------------------------------------------

install(TARGETS pgspa
  RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/${DMITIGR_PGSPA_BIN_INSTALL_DIR}")

if (NOT ("${DMITIGR_PGSPA_PG_SHAREDIR}" STREQUAL ""))
  install(FILES
    dmitigr_spa--0.1.sql
    dmitigr_spa.control

    DESTINATION "${DMITIGR_PGSPA_PG_SHAREDIR}/extension")
endif()
