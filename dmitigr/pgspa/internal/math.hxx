// -*- C++ -*-
// Copyright (C) Dmitry Igrishin
// For conditions of distribution and use, see file LICENSE.txt

#ifndef DMITIGR_PGSPA_INTERNAL_MATH_HXX
#define DMITIGR_PGSPA_INTERNAL_MATH_HXX

#include <cstdlib>

namespace dmitigr::pgspa::internal::math {

/**
 * @internal
 *
 * @return The random number.
 *
 * @remarks From TC++PL 3rd, 22.7.
 */
template<typename T>
inline T rand_cpp_pl_3rd(const T& num)
{
  return T(double(std::rand()) / RAND_MAX) * num;
}

} // namespace dmitigr::pgspa::internal::math

#endif  // DMITIGR_PGSPA_INTERNAL_MATH_HXX
