// -*- C++ -*-
// Copyright (C) Dmitry Igrishin
// For conditions of distribution and use, see file LICENSE.txt

#ifndef DMITIGR_PGSPA_INTERNAL_STD_STREAM_READ_TO_STRING_HXX
#define DMITIGR_PGSPA_INTERNAL_STD_STREAM_READ_TO_STRING_HXX

#include <istream>
#include <string>

namespace dmitigr::pgspa::internal {

/**
 * @internal
 *
 * @brief Reads an entire stream.
 *
 * @returns The string with the content read from the stream.
 */
inline std::string read_to_string(std::istream& stream)
{
  constexpr std::size_t buffer_size{512};
  std::string result;
  char buffer[buffer_size];
  while (stream.read(buffer, buffer_size))
    result.append(buffer, buffer_size);
  result.append(buffer, stream.gcount());
  return result;
}

} // namespace dmitigr::pgspa::internal

#endif  // DMITIGR_PGSPA_INTERNAL_STD_STREAM_READ_TO_STRING_HXX
