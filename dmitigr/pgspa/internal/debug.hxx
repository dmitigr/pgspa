// -*- C++ -*-
// Copyright (C) Dmitry Igrishin
// For conditions of distribution and use, see file LICENSE.txt

#ifndef DMITIGR_PGSPA_INTERNAL_DEBUG_HXX
#define DMITIGR_PGSPA_INTERNAL_DEBUG_HXX

#include "dmitigr/pgspa/internal/macros.hxx"

#include <cstdio>
#include <stdexcept>
#include <string>

namespace dmitigr::pgspa::internal {

#ifdef NDEBUG
constexpr bool is_debug_enabled = false;
#else
constexpr bool is_debug_enabled = true;
#endif

} // namespace dmitigr::pgspa::internal

#define DMITIGR_PGSPA_INTERNAL_DOUT__(...) {                      \
    std::fprintf(stderr, "Debug output from " __FILE__ ":"  \
      DMITIGR_PGSPA_INTERNAL_XSTR__(__LINE__) ": " __VA_ARGS__);  \
  }

#define DMITIGR_PGSPA_INTERNAL_ASSERT__(a, t) {                               \
    if (!(a)) {                                                         \
      DMITIGR_PGSPA_INTERNAL_DOUT__("assertion '%s' failed\n", #a)            \
        if constexpr (t) {                                              \
          throw std::logic_error(std::string("assertion '" #a "' failed at " __FILE__ ":") \
            .append(std::to_string(int(__LINE__))));                    \
        }                                                               \
    }                                                                   \
  }

#define DMITIGR_PGSPA_INTERNAL_DOUT_ALWAYS(...)      DMITIGR_PGSPA_INTERNAL_DOUT__(__VA_ARGS__)
#define DMITIGR_PGSPA_INTERNAL_DOUT_ASSERT_ALWAYS(a) DMITIGR_PGSPA_INTERNAL_ASSERT__(a, false)
#define DMITIGR_PGSPA_INTERNAL_ASSERT_ALWAYS(a)      DMITIGR_PGSPA_INTERNAL_ASSERT__(a, true)

#define DMITIGR_PGSPA_INTERNAL_IF_DEBUG__(code) if constexpr (dmitigr::pgspa::internal::is_debug_enabled) { code }

#define DMITIGR_PGSPA_INTERNAL_DOUT(...)      { DMITIGR_PGSPA_INTERNAL_IF_DEBUG__(DMITIGR_PGSPA_INTERNAL_DOUT_ALWAYS(__VA_ARGS__)) }
#define DMITIGR_PGSPA_INTERNAL_DOUT_ASSERT(a) { DMITIGR_PGSPA_INTERNAL_IF_DEBUG__(DMITIGR_PGSPA_INTERNAL_DOUT_ASSERT_ALWAYS(a)) }
#define DMITIGR_PGSPA_INTERNAL_ASSERT(a)      { DMITIGR_PGSPA_INTERNAL_IF_DEBUG__(DMITIGR_PGSPA_INTERNAL_ASSERT_ALWAYS(a)) }

// -----------------------------------------------------------------------------

#define DMITIGR_PGSPA_INTERNAL_REQUIRE__(r) {                                 \
    if (!(r)) {                                                         \
      std::string message{"API requirement '" #r "' violated"};         \
      if constexpr (dmitigr::pgspa::internal::is_debug_enabled) {              \
        message.append(" at " __FILE__ ":").append(std::to_string(int(__LINE__))); \
      }                                                                 \
      throw std::logic_error(message);                                  \
    }                                                                   \
  }

#define DMITIGR_PGSPA_INTERNAL_REQUIRE(r) DMITIGR_PGSPA_INTERNAL_REQUIRE__(r)

#endif  // DMITIGR_PGSPA_INTERNAL_DEBUG_HXX
