// -*- C++ -*-
// Copyright (C) Dmitry Igrishin
// For conditions of distribution and use, see file LICENSE.txt

#ifndef DMITIGR_PGSPA_INTERNAL_OS_HXX
#define DMITIGR_PGSPA_INTERNAL_OS_HXX

#include <cstddef>
#include <string>

namespace dmitigr::pgspa::internal::os {

/**
 * @internal
 *
 * @returns The current working directory.
 */
std::string cwd();

/**
 * @internal
 *
 * @returns The string with the current username.
 */
std::string current_username();

// -----------------------------------------------------------------------------

namespace io {

enum Origin {
  seek_set = SEEK_SET,
  seek_cur = SEEK_CUR,
  seek_end = SEEK_END,
#ifndef _WIN32
  seek_data = SEEK_DATA,
  seek_hole = SEEK_HOLE
#endif
};

std::size_t seek(int fd, long offset, Origin whence);

std::size_t read(int fd, void* buffer, unsigned int count);

} // namespace io

} // namespace dmitigr::pgspa::internal::os

#endif  // DMITIGR_PGSPA_INTERNAL_OS_HXX
