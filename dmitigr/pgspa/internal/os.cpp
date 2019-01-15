// -*- C++ -*-
// Copyright (C) Dmitry Igrishin
// For conditions of distribution and use, see file LICENSE.txt

#include "dmitigr/pgspa/internal/debug.hxx"
#include "dmitigr/pgspa/internal/os.hxx"

#include <cstdlib>
#include <memory>
#include <system_error>

#ifdef _WIN32

#include <Windows.h>
#include <Winnls.h>
#include <Lmcons.h>

// IO headers
#include <io.h>
#include <Stdio.h>

#include <direct.h> // _getcwd()

#else

#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>

#endif

namespace os = dmitigr::pgspa::internal::os;
namespace io = os::io;

namespace {

inline char* getcwd__(char* buffer, std::size_t size)
{
#ifdef _WIN32
  return _getcwd(buffer, int(size));
#else
  return getcwd(buffer, size);
#endif
}

char* dmint_os_cwd()
{
  constexpr std::size_t max_path_size = 128 * 128;
  std::size_t sz = 16;
  auto* buf = static_cast<char*>(std::calloc(1, sz));
  while (!getcwd__(buf, sz)) {
    if (errno == ERANGE) {
      sz *= 2;
      if (sz > max_path_size) {
        std::free(buf);
        return nullptr;
      } else {
        if (auto* new_buf = static_cast<char*>(std::realloc(buf, sz)))
          buf = new_buf;
        else {
          std::free(buf);
          return nullptr;
        }
      }
    } else { // ENOMEM
      DMITIGR_PGSPA_INTERNAL_DOUT_ASSERT(!buf);
      return nullptr;
    }
  }

  return buf;
}

} // namespace

std::string os::cwd()
{
  std::unique_ptr<char[], void (*)(void*)> guarded{dmint_os_cwd(), &std::free};
  return guarded ? guarded.get() : std::string{};
}

std::string os::current_username()
{
  std::string result;
#ifdef _WIN32
  constexpr DWORD max_size = UNLEN + 1;
  result.resize(max_size);
  DWORD sz{max_size};
  if (::GetUserName(result.data(), &sz) != 0)
    result.resize(sz - 1);
  else
    throw std::system_error{int(::GetLastError()), std::system_category(), "dmitigr::pgspa::internal::os::current_username()"};
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

// -----------------------------------------------------------------------------

std::size_t io::seek(const int fd, long const offset, const Origin whence)
{
#ifdef _WIN32
  const auto result = ::_lseek(fd, offset, int(whence));
#else
  const auto result = ::lseek(fd, offset, int(whence));
#endif

  if (result < 0) {
    const int err = errno;
    throw std::system_error{err, std::system_category(), "dmitigr::http::internal::io::seek()"};
  } else
    return std::size_t(result);
}

std::size_t io::read(const int fd, void* const buffer, const unsigned int count)
{
  DMITIGR_PGSPA_INTERNAL_ASSERT(buffer);

#ifdef _WIN32
  const auto result = ::_read(fd, buffer, count);
#else
  const auto result = ::read(fd, buffer, count);
#endif

  if (result < 0) {
    const int err = errno;
    throw std::system_error{err, std::system_category(), "dmitigr::http::internal::io::read()"};
  } else
    return std::size_t(result);
}
