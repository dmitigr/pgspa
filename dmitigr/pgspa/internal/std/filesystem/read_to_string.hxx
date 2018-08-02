// -*- C++ -*-
// Copyright (C) Dmitry Igrishin
// For conditions of distribution and use, see file LICENSE.txt

#ifndef DMITIGR_PGSPA_INTERNAL_STD_FILESYSTEM_READ_TO_STRING_HXX
#define DMITIGR_PGSPA_INTERNAL_STD_FILESYSTEM_READ_TO_STRING_HXX

#include "dmitigr/pgspa/internal/std/stream/read_to_string.hxx"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace dmitigr::pgspa::internal {

/**
 * @internal
 *
 * @brief Reads an entire file.
 *
 * @returns The string with the content read from the file denoted by the given `path`.
 */
inline std::string read_to_string(const std::filesystem::path& path)
{
  std::ifstream stream(path, std::ios_base::in | std::ios_base::binary);
  if (stream)
    return read_to_string(stream);
  else
    throw std::runtime_error("unable to open file \"" + path.generic_string() + "\"");
}

} // namespace dmitigr::pgspa::internal

#endif  // DMITIGR_PGSPA_INTERNAL_STD_FILESYSTEM_READ_TO_STRING_HXX
