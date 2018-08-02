// -*- C++ -*-
// Copyright (C) Dmitry Igrishin
// For conditions of distribution and use, see file LICENSE.txt

#ifndef DMITIGR_PGSPA_INTERNAL_OS_USER_HXX
#define DMITIGR_PGSPA_INTERNAL_OS_USER_HXX

#include <string>
#include <system_error>

#ifdef _WIN32
#include <Windows.h>
#include <Winnls.h>
#include <Lmcons.h>
#else
#include <memory>
#include <stdexcept>

#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>
#endif

namespace dmitigr::pgspa::internal::os {

/**
 * @internal
 *
 * @returns The string with the current username.
 */
inline std::string current_username()
{
  std::string result;
#ifdef _WIN32
  constexpr DWORD max_size = UNLEN + 1;
  result.resize(max_size);
  DWORD sz{max_size};
  if (::GetUserName(result.data(), &sz) != 0)
    result.resize(sz - 1);
  else
    throw std::system_error(::GetLastError(), std::system_category(), "dmitigr::pgspa::internal::os::current_username()");
#else
  struct passwd pwd;
  struct passwd *pwd_ptr{};
  const uid_t uid = geteuid();
  const std::size_t bufsz = []()
  {
    auto result = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (result == -1)
      result = 16384;
    return result;
  }();
  const std::unique_ptr<char[]> buf{new char[bufsz]};
  const int s = getpwuid_r(uid, &pwd, buf.get(), bufsz, &pwd_ptr);
  if (pwd_ptr == nullptr) {
    if (s == 0)
      throw std::logic_error{"current username is unavailable (possible something wrong with the OS)"};
    else
      throw std::system_error{s, std::system_category(), "dmitigr::pgspa::internal::os::current_username()"};
  } else
    result = pwd.pw_name;
#endif
  return result;
}

} // namespace dmitigr::pgspa::internal::os

#endif  // DMITIGR_PGSPA_INTERNAL_OS_USER_HXX
