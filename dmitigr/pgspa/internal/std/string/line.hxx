// -*- C++ -*-
// Copyright (C) Dmitry Igrishin
// For conditions of distribution and use, see file LICENSE.txt

#ifndef DMITIGR_PGSPA_INTERNAL_STD_STRING_LINE_HXX
#define DMITIGR_PGSPA_INTERNAL_STD_STRING_LINE_HXX

#include "dmitigr/pgspa/internal/debug.hxx"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

namespace dmitigr::pgspa::internal {

/**
 * @internal
 *
 * @returns Line number by the given absolute position. (Line numbers starts at 1.)
 */
std::size_t line_number_by_position(const std::string& str, const std::size_t pos)
{
  DMITIGR_PGSPA_INTERNAL_ASSERT(pos < str.size());
  return std::count(cbegin(str), cbegin(str) + pos, '\n') + 1;
}

/**
 * @internal
 *
 * @returns Line and column numbers by the given absolute position. (Both numbers starts at 1.)
 */
std::pair<std::size_t, std::size_t> line_column_numbers_by_position(const std::string& str, const std::size_t pos)
{
  DMITIGR_PGSPA_INTERNAL_ASSERT(pos < str.size());
  std::size_t line{}, column{};
  for (std::size_t i = 0; i < pos; ++i) {
    ++column;
    if (str[i] == '\n') {
      ++line;
      column = 0;
    }
  }
  return std::make_pair(line + 1, column + 1);
}

} // namespace dmitigr::pgspa::internal

#endif  // DMITIGR_PGSPA_INTERNAL_STD_STRING_LINE_HXX
