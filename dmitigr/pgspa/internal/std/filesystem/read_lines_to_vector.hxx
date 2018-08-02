// -*- C++ -*-
// Copyright (C) Dmitry Igrishin
// For conditions of distribution and use, see file LICENSE.txt

#ifndef DMITIGR_PGSPA_INTERNAL_STD_FILESYSTEM_READ_LINES_TO_VECTOR_HXX
#define DMITIGR_PGSPA_INTERNAL_STD_FILESYSTEM_READ_LINES_TO_VECTOR_HXX

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace dmitigr::pgspa::internal {

/**
 * @internal
 *
 * @brief Reads lines of the given file into the string vector.
 */
template<typename Pred>
inline std::vector<std::string> read_lines_to_vector_if(const std::filesystem::path& path, Pred pred)
{
  std::vector<std::string> result;
  std::string line;
  std::ifstream lines{path};
  while (getline(lines, line)) {
    if (pred(line))
      result.push_back(line);
  }
  return result;
}

/**
 * @internal
 *
 * @overload
 */
inline std::vector<std::string> read_lines_to_vector(const std::filesystem::path& path)
{
  return read_lines_to_vector_if(path, [](const auto&) { return true; });
}

} // namespace dmitigr::pgspa::internal

#endif  // DMITIGR_PGSPA_INTERNAL_STD_FILESYSTEM_READ_LINES_TO_VECTOR_HXX
