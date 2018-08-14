// -*- C++ -*-
// Copyright (C) Dmitry Igrishin
// For conditions of distribution and use, see file LICENSE.txt

#ifndef DMITIGR_PGSPA_INTERNAL_CONSOLE_HXX
#define DMITIGR_PGSPA_INTERNAL_CONSOLE_HXX

#include "dmitigr/pgspa/internal/debug.hxx"

#include <functional>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace dmitigr::pgspa::internal {

/**
 * @internal
 *
 * @brief Represents a console command to run.
 */
class Console_command {
public:
  /** The destructor */
  virtual ~Console_command() = default;

  /** @returns The name of the command. */
  virtual std::string name() const = 0;

  /** @return The usage string of the command. */
  virtual std::string usage() const = 0;

  /** Runs the command. */
  virtual void run() = 0;

protected:
  /** Represents a vector of command options. */
  using Option_vector = std::vector<std::string>;

  /** Represents an iterator of command options. */
  using Option_iterator = typename Option_vector::const_iterator;

  /** Represents a setter of command options. */
  using Option_parser = std::function<void (const std::string&)>;

  /**
   * @throws An instance of std::logic_error.
   *
   * @param details - The details to be included into the error message.
   */
  [[noreturn]] void throw_invalid_usage(std::string details = {}) const
  {
    std::string message = "invalid usage of the \"" + name() + "\" command\n";
    if (!details.empty())
      message.append("  details: " + std::move(details) + "\n");
    throw std::logic_error{std::move(message.append(usage()))};
  }

  /**
   * @returns The argument that follows the option.
   *
   * @param value - The value the option.
   * @param is_optional - The indicator of an optional option argument.
   */
  std::optional<std::string> option_argument(const std::string& value, const bool is_optional = false) const
  {
    if (const auto pos = value.find('='); pos != std::string::npos)
      return pos < value.size() ? value.substr(pos + 1) : std::string{};
    else if (is_optional)
      return std::nullopt;
    else
      throw_invalid_usage("no argument for the \"" + value.substr(0, pos) + "\" option specified");
  }

  void check_no_option_argument(const std::string& value) const
  {
    DMITIGR_PGSPA_INTERNAL_ASSERT(value.find("--") == 0);
    if (const auto pos = value.find('='); pos != std::string::npos)
      throw_invalid_usage("no argument for the option \"" + value.substr(0, pos) + "\" can be specified");
  }

  /**
   * @brief Parses the options.
   *
   * @param i - The starting iterator of the options to parse.
   * @param e - The ending iterator of the options to parse.
   * @param parse_option - The callback that will be called for each option.
   * The parser must accepts one argument: the string of the option to parse.
   */
  Option_iterator parse_options(Option_iterator i, const Option_iterator e, Option_parser parse_option)
  {
    for (; i != e && *i != "--" && (i->find("--") == 0); ++i)
      parse_option(*i);
    return i;
  }
};

/**
 * @returns The command ID and options.
 *
 * The command ID - is an identifier specified as the 1st argument.
 * For example, the "exec" is the command ID here:
 *   pgspa exec --strong foo bar baz
 */
std::pair<std::string, std::vector<std::string>> command_and_options(const int argc, const char* const* argv)
{
  DMITIGR_PGSPA_INTERNAL_ASSERT(argc > 1);
  std::string result1{argv[1]};
  std::vector<std::string> result2;
  for (int i = 2; i < argc; ++i)
    result2.push_back(argv[i]);
  return std::make_pair(result1, result2);
};

} // namespace dmitigr::pgspa::internal

#endif // DMITIGR_PGSPA_INTERNAL_CONSOLE_HXX
