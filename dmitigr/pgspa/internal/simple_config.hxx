// -*- C++ -*-
// Copyright (C) Dmitry Igrishin
// For conditions of distribution and use, see file LICENSE.txt

#ifndef DMITIGR_PGSPA_INTERNAL_SIMPLE_CONFIG_HXX
#define DMITIGR_PGSPA_INTERNAL_SIMPLE_CONFIG_HXX

#include "dmitigr/pgspa/internal/debug.hxx"
#include "dmitigr/pgspa/internal/std/filesystem/read_lines_to_vector.hxx"
#include "dmitigr/pgspa/internal/std/string/substring.hxx"

#include <locale>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

namespace dmitigr::pgspa::internal {

class Simple_config {
public:
  Simple_config(const std::filesystem::path& path)
    : data_{parsed_config(path)}
  {
    DMITIGR_PGSPA_INTERNAL_ASSERT(is_invariant_ok());
  }

  const std::optional<std::string>& string_parameter(const std::string& name) const
  {
    if (const auto e = cend(data_), i = data_.find(name); i != e)
      return i->second;
    else
      return null_string_parameter();
  }

  std::optional<bool> boolean_parameter(const std::string& name) const
  {
    if (const auto& str_param = string_parameter(name)) {
      const auto& str = *str_param;
      if (str == "y" || str == "yes" || str == "t" || str == "true" || str == "1")
        return true;
      else if (str == "n" || str == "no" || str == "f" || str == "false" || str == "0")
        return false;
      else
        throw std::logic_error{"invalid value \"" + str + "\" of the boolean parameter \"" + name + "\""};
    } else
      return std::nullopt;
  }

  const std::map<std::string, std::optional<std::string>>& data() const
  {
    return data_;
  }

private:
  /**
   * @returns Parsed config entry. The format of `line` can be:
   *   - "param=one";
   *   - "param='one two  three';
   *   - "param='one \'two three\' four'.
   */
  std::pair<std::string, std::string> parsed_config_entry(const std::string& line)
  {
    std::string param;
    std::string value;
    std::string::size_type pos = position_of_non_space(line, 0);
    DMITIGR_PGSPA_INTERNAL_ASSERT(pos < line.size());

    /*
     * @returns The position of the first character of a parameter value.
     */
    static const auto position_of_value = [](const std::string& str, std::string::size_type pos)
    {
      pos = position_of_non_space(str, pos);
      if (pos < str.size()) {
        if (str[pos] == '=')
          return position_of_non_space(str, ++pos);
        else
          throw std::runtime_error{"no value assignment"};
      } else
        throw std::runtime_error{"no value assignment"};

      return pos;
    };

    // Reading the parameter name.
    std::tie(param, pos) = substring_if_simple_identifier(line, pos);
    if (pos < line.size()) {
      if (param.empty() || (!std::isspace(line[pos], std::locale{}) && line[pos] != '='))
        throw std::runtime_error{"invalid parameter name"};
    } else
      throw std::runtime_error{"invalid configuration entry"};

    // Reading the parameter value.
    if (pos = position_of_value(line, pos); pos < line.size()) {
      std::tie(value, pos) = unquoted_substring(line, pos);
      DMITIGR_PGSPA_INTERNAL_ASSERT(!value.empty());
      if (pos < line.size()) {
        if (pos = position_of_non_space(line, pos); pos < line.size())
          throw std::runtime_error{"junk in the config entry"};
      }
    } // else the value is empty

    return {std::move(param), std::move(value)};
  }

  /**
   * @returns Parsed config file. The format of `line` can be:
   *   - "param=one";
   *   - "param='one two  three';
   *   - "param='one \'two three\' four'.
   */
  std::map<std::string, std::optional<std::string>> parsed_config(const std::filesystem::path& path)
  {
    std::map<std::string, std::optional<std::string>> result;
    static const auto is_nor_empty_nor_commented = [](const std::string& line)
    {
      if (!line.empty())
        if (const auto pos = position_of_non_space(line, 0); pos < line.size())
          return line[pos] != '#';
      return false;
    };
    const auto lines = read_lines_to_vector_if(path, is_nor_empty_nor_commented);
    for (std::size_t i = 0; i < lines.size(); ++i) {
      try {
        result.insert(parsed_config_entry(lines[i]));
      } catch (const std::exception& e) {
        throw std::runtime_error{std::string{e.what()} +" (line " + std::to_string(i + 1) + ")"};
      }
    }
    return result;
  }

  constexpr bool is_invariant_ok() const
  {
    return true;
  }

  static const std::optional<std::string>& null_string_parameter()
  {
    static const std::optional<std::string> result;
    return result;
  }

  std::map<std::string, std::optional<std::string>> data_;
};

} // namespace dmitigr::pgspa::internal

#endif  // DMITIGR_PGSPA_INTERNAL_SIMPLE_CONFIG
