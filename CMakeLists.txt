# -*- cmake -*-
# Copyright (C) Dmitry Igrishin
# For conditions of distribution and use, see files LICENSE.txt

project(dmitigr_pgspa)

cmake_minimum_required(VERSION 3.13)

if (NOT (UNIX OR WIN32))
  message(FATAL_ERROR "unsupported target platform")
endif()

list(APPEND CMAKE_MODULE_PATH ${dmitigr_pgspa_SOURCE_DIR}/cmake)
include(dmitigr)

set(dmitigr_pgspa_version_major 0)
set(dmitigr_pgspa_version_minor 2)
add_definitions(-DPGSPA_VERSION_MAJOR=${dmitigr_pgspa_version_major})
add_definitions(-DPGSPA_VERSION_MINOR=${dmitigr_pgspa_version_minor})

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

enable_language(CXX)
set(CMAKE_CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED ON)

include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR})

# ------------------------------------------------------------------------------

set(suff "")
if (DMITIGR_CEFEIKA_HEADER_ONLY)
  set(suff "_interface")
endif()

find_package(dmitigr_cefeika REQUIRED COMPONENTS app base cfg fs os pgfe${suff} str)

# ------------------------------------------------------------------------------

add_executable(pgspa pgspa.cpp)
dmitigr_target_compile_options(pgspa)
target_link_libraries(pgspa PRIVATE dmitigr::app dmitigr::base
  dmitigr::cfg dmitigr::fs dmitigr::os dmitigr::pgfe dmitigr::str)
if (WIN32)
  target_link_libraries(pgspa PRIVATE Advapi32.lib)
endif()

# ------------------------------------------------------------------------------

install(TARGETS pgspa
  RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/${DMITIGR_PGSPA_BIN_INSTALL_DIR}")

if (NOT ("${DMITIGR_PGSPA_PG_SHAREDIR}" STREQUAL ""))
  install(FILES
    sql/dmitigr_spa--0.2.sql
    sql/dmitigr_spa.control

    DESTINATION "${DMITIGR_PGSPA_PG_SHAREDIR}/extension")
endif()
