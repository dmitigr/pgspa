// -*- C++ -*-
// Copyright (C) Dmitry Igrishin
// For conditions of distribution and use, see file LICENSE.txt

#ifndef DMITIGR_PGSPA_INTERNAL_STREAM_HXX
#define DMITIGR_PGSPA_INTERNAL_STREAM_HXX

#include <iosfwd>
#include <string>
#include <system_error>

// Exceptions and error codes
namespace dmitigr::pgspa::internal::stream {

/**
 * @brief Represents an exception that may be thrown by `read_*()` functions.
 */
class Read_exception : public std::system_error {
public:
  explicit Read_exception(std::error_condition condition);

  Read_exception(std::error_condition condition, std::string&& incomplete_result);

  const std::string& incomplete_result() const;

  const char* what() const noexcept override;

private:
  std::string incomplete_result_;
};

/**
 * @internal
 *
 * @brief Represents a read error code.
 */
enum class Read_errc { success = 0, stream_error, invalid_input };

/**
 * @internal
 *
 * @brief Represents a type derived from class `std::error_category` to support category
 * of dmitigr::pgspa::internal::parse runtime errors.
 */
class Error_category : public std::error_category {
public:
  const char* name() const noexcept override;

  std::string message(const int ev) const override;
};

/**
 * @internal
 *
 * @returns A reference to an object of a type derived from class `std::error_category`.
 *
 * @remarks The object's name() function returns a pointer to the string "dmitigr_internal_stream_error".
 */
const Error_category& error_category() noexcept;

/**
 * @internal
 *
 * @returns `std::error_code(int(errc), parse_error_category())`
 */
std::error_code make_error_code(Read_errc errc) noexcept;

/**
 * @internal
 *
 * @returns `std::error_condition(int(errc), parse_error_category())`
 */
std::error_condition make_error_condition(Read_errc errc) noexcept;

} // namespace dmitigr::pgspa::internal::stream

// Integration with the std::system_error framework
namespace std {

template<> struct is_error_condition_enum<dmitigr::pgspa::internal::stream::Read_errc> : true_type {};

} // namespace std

namespace dmitigr::pgspa::internal::stream {

/**
 * @internal
 *
 * @brief Reads a whole stream to a string.
 *
 * @returns The string with the content read from the stream.
 */
std::string read_to_string(std::istream& input);

/**
 * @internal
 *
 * @brief Reads the (next) "simple phrase" from the `input`.
 *
 * Whitespaces (i.e. space, tab or newline) or the quote (i.e. '"') that follows
 * after the phrase are preserved in the `input`.
 *
 * @returns The string with the "simple phrase".
 *
 * @throws Simple_phrase_error with the appropriate code and incomplete result
 * of parsing.
 *
 * @remarks the "simple phrase" - an unquoted expression without spaces, or
 * quoted expression (which can include any characters).
 */
std::string read_simple_phrase_to_string(std::istream& input);

} // namespace dmitigr::pgspa::internal::stream

#endif  // DMITIGR_PGSPA_INTERNAL_STREAM_HXX
