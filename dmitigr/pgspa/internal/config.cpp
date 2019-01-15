// -*- C++ -*-
// Copyright (C) Dmitry Igrishin
// For conditions of distribution and use, see file LICENSE.txt

#include "dmitigr/pgspa/internal/config.hxx"
#include "dmitigr/pgspa/internal/debug.hxx"
#include "dmitigr/pgspa/internal/string.hxx"

#include <locale>
#include <stdexcept>
#include <tuple>

namespace dmitigr::pgspa::internal::config {

Flat::Flat(const std::filesystem::path& path)
  : parameters_{parsed_config(path)}
{
  DMITIGR_PGSPA_INTERNAL_ASSERT(is_invariant_ok());
}

const std::optional<std::string>& Flat::string_parameter(const std::string& name) const
{
  if (const auto e = cend(parameters_), i = parameters_.find(name); i != e)
    return i->second;
  else
    return null_string_parameter();
}

std::optional<bool> Flat::boolean_parameter(const std::string& name) const
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

const std::map<std::string, std::optional<std::string>>& Flat::parameters() const
{
  return parameters_;
}

std::pair<std::string, std::string> Flat::parsed_config_entry(const std::string& line)
{
  std::string param;
  std::string value;
  std::string::size_type pos = string::position_of_non_space(line, 0);
  DMITIGR_PGSPA_INTERNAL_ASSERT(pos < line.size());

  /*
   * @returns The position of the first character of a parameter value.
   */
  static const auto position_of_value = [](const std::string& str, std::string::size_type pos)
  {
    pos = string::position_of_non_space(str, pos);
    if (pos < str.size()) {
      if (str[pos] == '=')
        return string::position_of_non_space(str, ++pos);
      else
        throw std::runtime_error{"no value assignment"};
    } else
      throw std::runtime_error{"no value assignment"};

    return pos;
  };

  // Reading the parameter name.
  std::tie(param, pos) = string::substring_if_simple_identifier(line, pos);
  if (pos < line.size()) {
    if (param.empty() || (!std::isspace(line[pos], std::locale{}) && line[pos] != '='))
      throw std::runtime_error{"invalid parameter name"};
  } else
    throw std::runtime_error{"invalid configuration entry"};

  // Reading the parameter value.
  if (pos = position_of_value(line, pos); pos < line.size()) {
    std::tie(value, pos) = string::unquoted_substring(line, pos);
    DMITIGR_PGSPA_INTERNAL_ASSERT(!value.empty());
    if (pos < line.size()) {
      if (pos = string::position_of_non_space(line, pos); pos < line.size())
        throw std::runtime_error{"junk in the config entry"};
    }
  } // else the value is empty

  return {std::move(param), std::move(value)};
}

std::map<std::string, std::optional<std::string>> Flat::parsed_config(const std::filesystem::path& path)
{
  std::map<std::string, std::optional<std::string>> result;
  static const auto is_nor_empty_nor_commented = [](const std::string& line)
  {
    if (!line.empty())
      if (const auto pos = string::position_of_non_space(line, 0); pos < line.size())
        return line[pos] != '#';
    return false;
  };
  const auto lines = filesystem::read_lines_to_vector_if(path, is_nor_empty_nor_commented);
  for (std::size_t i = 0; i < lines.size(); ++i) {
    try {
      result.insert(parsed_config_entry(lines[i]));
    } catch (const std::exception& e) {
      throw std::runtime_error{std::string{e.what()} +" (line " + std::to_string(i + 1) + ")"};
    }
  }
  return result;
}

bool Flat::is_invariant_ok() const
{
  return true;
}

const std::optional<std::string>& Flat::null_string_parameter()
{
  static const std::optional<std::string> result;
  return result;
}

} // namespace dmitigr::pgspa::internal::config
