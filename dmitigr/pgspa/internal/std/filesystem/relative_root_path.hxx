// -*- C++ -*-
// Copyright (C) Dmitry Igrishin
// For conditions of distribution and use, see file LICENSE.txt

#ifndef DMITIGR_PGSPA_INTERNAL_STD_FILESYSTEM_RELATIVE_ROOT_PATH_HXX
#define DMITIGR_PGSPA_INTERNAL_STD_FILESYSTEM_RELATIVE_ROOT_PATH_HXX

#include <filesystem>
#include <stdexcept>

namespace dmitigr::pgspa::internal {

/**
 * @internal
 *
 * @brief Searches for `indicator` in the current directory and in the parent directories.
 *
 * @returns The path to the `indicator`.
 */
std::filesystem::path relative_root_path(const std::filesystem::path& indicator)
{
  auto path = std::filesystem::current_path();
  while (true) {
    if (is_directory(path / indicator))
      return path;
    else if (path.has_relative_path())
      path = path.parent_path();
    else
      throw std::runtime_error("no " + indicator.string() + " directory found");
  }
}

} // namespace dmitigr::pgspa::internal

#endif  // DMITIGR_PGSPA_INTERNAL_STD_FILESYSTEM_RELATIVE_ROOT_PATH_HXX
