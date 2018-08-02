// -*- C++ -*-
// Copyright (C) Dmitry Igrishin
// For conditions of distribution and use, see file LICENSE.txt

#ifndef DMITIGR_PGSPA_INTERNAL_STD_STRING_PREDICATES_HXX
#define DMITIGR_PGSPA_INTERNAL_STD_STRING_PREDICATES_HXX

#include <locale>
#include <string>

namespace dmitigr::pgspa::internal {

/**
 * @internal
 *
 * @returns `true` if `c` is a valid non-space character.
 */
inline bool is_non_space_character(const char c)
{
  return !std::isspace(c, std::locale{});
}

/**
 * @internal
 *
 * @returns `true` if `c` is a valid simple identifier character.
 */
inline bool is_simple_identifier_character(const char c)
{
  return std::isalnum(c, std::locale{}) || c == '_';
};

} // namespace dmitigr::pgspa::internal

#endif  // DMITIGR_PGSPA_INTERNAL_STD_STRING_PREDICATES_HXX
