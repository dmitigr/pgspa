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
  using Option_setter = std::function<void (const Option_iterator&, const Option_iterator&)>;

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
   * @param iterator - The iterator that denotes the option.
   * @param end - The iterator that denotes the end of options.
   * @param is_optional - The indicator of an optional option argument.
   */
  std::optional<std::string> read_option_argument(const Option_iterator& iterator, const Option_iterator end,
    const bool is_optional = false) const
  {
    DMITIGR_PGSPA_INTERNAL_ASSERT(iterator < end);
    const auto& value = *iterator;
    if (const auto pos = value.find('='); pos != std::string::npos)
      return pos < value.size() ? value.substr(pos + 1) : std::string{};
    else if (is_optional)
      return std::nullopt;
    else
      throw_invalid_usage("no argument for the \"" + value + "\" option specified");
  }

  /**
   * @brief Parses the options.
   *
   * @param opts - The options to parse.
   * @param set_option - The callback setter that will be called for each option.
   * The setter must accepts two arguments:
   *   - the iterator of the first option to parse;
   *   - the iterator of the after-last option to parse;
   */
  Option_iterator parse_options(Option_iterator i, const Option_iterator e, Option_setter set_option)
  {
    for (; i != e && *i != "--" && (i->find("--") == 0); ++i)
      set_option(i, e);
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
