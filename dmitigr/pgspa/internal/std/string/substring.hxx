// -*- C++ -*-
// Copyright (C) Dmitry Igrishin
// For conditions of distribution and use, see file LICENSE.txt

#ifndef DMITIGR_PGSPA_INTERNAL_STD_STRING_SUBSTRING_HXX
#define DMITIGR_PGSPA_INTERNAL_STD_STRING_SUBSTRING_HXX

#include "dmitigr/pgspa/internal/debug.hxx"
#include "dmitigr/pgspa/internal/std/string/predicates.hxx"

#include <algorithm>
#include <string>
#include <utility>

namespace dmitigr::pgspa::internal {

/**
 * @returns The position of the first non-space character of `str` in range [pos, str.size()).
 */
inline std::string::size_type position_of_non_space(const std::string& str, std::string::size_type pos)
{
  DMITIGR_PGSPA_INTERNAL_ASSERT(pos <= str.size());
  const auto b = cbegin(str);
  return std::find_if(b + pos, cend(str), is_non_space_character) - b;
}

/**
 * @returns The substring of `str` from position of `pos` until the position
 * of the character "c" that `pred(c) == false` as the first element, and the
 * position of the character followed "c" as the second element.
 */
template<typename Pred>
std::pair<std::string, std::string::size_type> substring_if(const std::string& str, Pred pred, std::string::size_type pos)
{
  DMITIGR_PGSPA_INTERNAL_ASSERT(pos <= str.size());
  std::pair<std::string, std::string::size_type> result;
  const auto input_size = str.size();
  for (; pos < input_size; ++pos) {
    if (pred(str[pos]))
      result.first += str[pos];
    else
      break;
  }
  result.second = pos;
  return result;
}

/**
 * @returns The substring of `str` with the "simple identifier" from position of `pos`
 * as the first element, and the position of the next character in `str`.
 */
inline std::pair<std::string, std::string::size_type> substring_if_simple_identifier(const std::string& str,
  std::string::size_type pos)
{
  DMITIGR_PGSPA_INTERNAL_ASSERT(pos <= str.size());
  return std::isalpha(str[pos], std::locale{}) ? substring_if(str, is_simple_identifier_character, pos) :
    std::make_pair(std::string{}, pos);
}

/**
 * @returns The substring of `str` with no spaces from position of `pos`
 * as the first element, and the position of the next character in `str`.
 */
inline std::pair<std::string, std::string::size_type> substring_if_no_spaces(const std::string& str, std::string::size_type pos)
{
  return substring_if(str, is_non_space_character, pos);
}

/**
 * @returns The unquoted substring of `str` if `str[pos] == '\''` or the substring
 * with no spaces from the position of `pos` as the first element, and the position
 * of the next character in `str`.
 */
inline std::pair<std::string, std::string::size_type> unquoted_substring(const std::string& str, std::string::size_type pos)
{
  DMITIGR_PGSPA_INTERNAL_ASSERT(pos <= str.size());
  if (pos == str.size())
    return {std::string{}, pos};

  std::pair<std::string, std::string::size_type> result;
  constexpr char quote_char = '\'';
  constexpr char escape_char = '\\';
  if (str[pos] == quote_char) {
    // Trying to reach the trailing quote character.
    const auto input_size = str.size();
    enum { normal, escape } state = normal;
    for (++pos; pos < input_size; ++pos) {
      const auto ch = str[pos];
      switch (state) {
      case normal:
        if (ch == quote_char)
          goto finish;
        else if (ch == escape_char)
          state = escape;
        else
          result.first += ch;
        break;
      case escape:
        if (ch != quote_char)
          result.first += escape_char; // it's not escape, so preserve
        result.first += ch;
        state = normal;
        break;
      }
    }

  finish:
    if (pos == input_size && str.back() != quote_char || pos < input_size && str[pos] != quote_char)
      throw std::runtime_error{"no trailing quote found"};
    else
      result.second = pos + 1; // discarding the trailing quote
  } else
    result = substring_if_no_spaces(str, pos);
  return result;
}

} // namespace dmitigr::pgspa::internal

#endif  // DMITIGR_PGSPA_INTERNAL_STD_STRING_SUBSTRING_HXX
