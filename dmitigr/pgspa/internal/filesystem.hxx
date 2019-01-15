// -*- C++ -*-
// Copyright (C) Dmitry Igrishin
// For conditions of distribution and use, see file LICENSE.txt

#ifndef DMITIGR_PGSPA_INTERNAL_FILESYSTEM_HXX
#define DMITIGR_PGSPA_INTERNAL_FILESYSTEM_HXX

#include "dmitigr/pgspa/internal/filesystem_experimental.hxx"

#include <fstream>
#include <string>
#include <vector>

namespace dmitigr::pgspa::internal::filesystem {

/**
 * @internal
 *
 * @returns The vector of the paths.
 *
 * @param root - The search root.
 * @param extension - The extension of files to be included into the result.
 * @param recursive - if `true` then do the recursive search.
 * @param include_heading - if `true` then include the "heading file" into the result.
 * The "heading file" - is a regular file with the given `extension` which has the same
 * parent directory as the `root`.
 */
std::vector<std::filesystem::path> files_by_extension(const std::filesystem::path& root,
  const std::filesystem::path& extension, bool recursive, bool include_heading = false);

// -----------------------------------------------------------------------------

/**
 * @internal
 *
 * @brief Reads lines of the given file into the string vector.
 */
template<typename Pred>
std::vector<std::string> read_lines_to_vector_if(const std::filesystem::path& path, Pred pred)
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
 * @brief Similar to read_lines_to_vector_if(path, always_true).
 */
inline std::vector<std::string> read_lines_to_vector(const std::filesystem::path& path)
{
  return read_lines_to_vector_if(path, [](const auto&) { return true; });
}

// -----------------------------------------------------------------------------

/**
 * @internal
 *
 * @brief Reads an entire file.
 *
 * @returns The string with the content read from the file denoted by the given `path`.
 */
std::string read_to_string(const std::filesystem::path& path);

// -----------------------------------------------------------------------------

/**
 * @internal
 *
 * @brief Searches for `indicator` directory in the current directory and in the parent directories.
 *
 * @returns The path to the `indicator` directory.
 */
std::filesystem::path relative_root_path(const std::filesystem::path& indicator);

} // namespace dmitigr::pgspa::internal::filesystem

#endif  // DMITIGR_PGSPA_INTERNAL_FILESYSTEM_HXX
