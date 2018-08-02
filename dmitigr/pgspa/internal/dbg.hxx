// -*- C++ -*-
// Copyright (C) Dmitry Igrishin
// For conditions of distribution and use, see file LICENSE.txt

#ifndef DMITIGR_PGSPA_INTERNAL_DBG_HXX
#define DMITIGR_PGSPA_INTERNAL_DBG_HXX

#include <iostream>
#include <string>
#include <type_traits>

namespace dmitigr::pgspa::internal::dbg {

/**
 * @internal
 *
 * @brief Prints one-dimensional container to the standard error.
 *
 * @todo Generalize for multidimensional containers.
 */
template<class Container, class ToStr>
void print_container(const Container& cont, ToStr to_str)
{
  std::string output{"{"};
  for (const auto& e : cont)
    output.append(to_str(e)).append(",");
  if (!cont.empty())
    output.pop_back();
  output.append("}");
  std::cerr << output << "\n";
}

template<typename T, template<class, class> class Container, template<class> class Allocator>
std::enable_if_t<std::is_same_v<T, std::string>> print_container(const Container<T, Allocator<T>>& cont)
{
  print_container(cont, [](const std::string& e)->const std::string& { return e; });
}

} // namespace

#endif  // DMITIGR_PGSPA_INTERNAL_DBG_HXX
