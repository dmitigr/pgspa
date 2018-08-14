// -*- C++ -*-
// Copyright (C) Dmitry Igrishin
// For conditions of distribution and use, see files LICENSE.txt

#ifdef _WIN32
#define NOMINMAX
#endif

#include "dmitigr/pgspa/internal/console.hxx"
#include "dmitigr/pgspa/internal/debug.hxx"
#include "dmitigr/pgspa/internal/simple_config.hxx"
#include "dmitigr/pgspa/internal/os/user.hxx"
#include "dmitigr/pgspa/internal/std/filesystem/read_to_string.hxx"
#include "dmitigr/pgspa/internal/std/filesystem/read_lines_to_vector.hxx"
#include "dmitigr/pgspa/internal/std/filesystem/relative_root_path.hxx"
#include "dmitigr/pgspa/internal/std/string/line.hxx"
#include "dmitigr/pgspa/internal/std/string/substring.hxx"

#include <dmitigr/pgfe.hpp>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#define ASSERT(a) DMITIGR_PGSPA_INTERNAL_ASSERT(a)
#define ASSERT_ALWAYS(a) DMITIGR_PGSPA_INTERNAL_ASSERT_ALWAYS(a)

namespace dmitigr::pgspa {

namespace fs = std::filesystem;

/**
 * @returns The general usage info.
 */
std::string usage()
{
  return std::string("pgspa - The SQL Programming Assistant for PostgreSQL\n\n")
    .append("Usage: pgspa <command>\n\n")
    .append("Commands:\n")
    .append("  help\n")
    .append("  version\n")
    .append("\n")
    .append("  init\n")
    .append("  exec");
}

/**
 * @brief Represents a dummy exception thrown after the real exception was handled.
 *
 * @remarks The handlers of the exceptions of this type should not have side any effects.
 */
class Handled_exception{};

/**
 * @brief Represents the pgspa command.
 */
class Command : public internal::Console_command {
public:
  /**
   * @brief This factory function makes the command object from the textual type identifier.
   */
  template<typename ... Types>
  static std::unique_ptr<Command> make(const std::string& cmd, Types&& ... opts);

  /**
   * @brief Runs the current command.
   *
   * If the "--help" option was specified, this functions just prints the
   * usage info to the standard output.
   */
  void go()
  {
    if (is_help_requested())
      std::cout << usage() << "\n";
    else
      run();
  }

protected:
  /**
   * @returns The string with the options specific to this command.
   */
  static std::string options()
  {
    static std::string result{
      "  --help - print the usage info and exit. (Cancels effect of all other arguments.)"};
    return result;
  }

  Command() = default;

  /**
   * @brief Sets the values of the options specific to this command.
   */
  void parse_option(const std::string& option)
  {
    ASSERT(option.find("--") == 0);
    if (option == "--help")
      help_ = true;
    else
      throw_invalid_usage("invalid option \"" + option + "\"");
  };

  // ---------------------------------------------------------------------------

  /**
   * @returns `true` if "--help" options was specified.
   */
  bool is_help_requested() const
  {
    return help_;
  }

  /**
   * @returns The general usage info of this command.
   */
  std::string usage() const override
  {
    return std::string{}
      .append("Usage: pgspa " + name() + "\n\n")
      .append("Options:\n")
      .append(options());
  }

  /**
   * @returns `true` if the invariant is okay, or `false` otherwise.
   */
  bool is_invariant_ok() const
  {
    return true;
  }

  // ---------------------------------------------------------------------------

  /**
   * @returns The root path of the project.
   */
  static fs::path root_path()
  {
    return internal::relative_root_path(".pgspa");
  }

  /**
   * @returns The vector of paths to SQL files of the specified ref.
   */
  static std::vector<fs::path> sql_paths(const fs::path& ref)
  {
    return sql_paths(ref, {ref});
  }

private:
  /**
   * @brief Run the current command.
   *
   * @remarks The API function `go()` wraps this function.
   *
   * @see go().
   */
  virtual void run() override = 0;

  /**
   * @returns The vector of paths to SQL files by the specified `reference`.
   */
  static std::vector<fs::path> sql_paths(const fs::path& reference, std::vector<fs::path> trace)
  {
    std::vector<fs::path> result;

    if (const auto name = reference.filename(); name.empty() || name.string().front() == '.')
      throw std::logic_error{"the reference name cannot be empty or starts with the dot (\".\")"};

    static const auto sql_file_path = [](const fs::path& reference)
    {
      auto file = reference;
      return file.replace_extension(".sql");
    };

    if (is_regular_file(reference) && reference.extension() == ".sql") {
      result.push_back(reference);
    } else if (is_regular_file(reference) && reference.extension().empty()) {
      static const auto is_nor_empty_nor_commented = [](const std::string& line) { return (!line.empty() && line.front() != '#'); };
      const auto parent = reference.parent_path();
      const auto paths = internal::read_lines_to_vector_if(reference, is_nor_empty_nor_commented);

      const auto is_in_trace = [&](const fs::path& p)
      {
        const auto b = cbegin(trace), e = cend(trace);
        return std::find(b, e, p) != e;
      };

      for (const auto& path : paths) {
        const auto full_path = parent / path;
        if (!is_in_trace(full_path)) {
          auto trace_copy = trace;
          trace_copy.push_back(full_path);
          push_back(result, sql_paths(full_path, std::move(trace_copy)));
        } else {
          std::string graph;
          for (const auto& r : trace)
            graph.append(r.string()).append(" -> ");
          graph.append(full_path.string());
          throw std::runtime_error{"reference cyclicity detected: \"" + graph + "\""};
        }
      }
    } else if (is_directory(reference)) {
      if (const auto config = reference / ".pgspa"; is_regular_file(config)) {
        if (auto params = parsed_config(config); params.boolean_parameter("explicit").value_or(false)) {
          throw std::runtime_error{"the references of the directory \"" + reference.string() +
            "\" are allowed to be used only explicitly"};
        }
      }

      if (auto heading_file = sql_file_path(reference); is_regular_file(heading_file))
        result.emplace_back(std::move(heading_file));

      const auto refs_of_dir = [&reference]()
      {
        auto result = refs_of_directory(reference);
        std::sort(begin(result), end(result));
        return result;
      }();

      for (auto r : refs_of_dir) {
        r = reference / r;

        r.replace_extension(".sql");
        if (is_regular_file(r))
          result.push_back(r);

        r.replace_extension();
        if (is_directory(r))
          push_back(result, sql_paths(r, std::move(trace)));
      }
    } else if (auto file = sql_file_path(reference); is_regular_file(file)) {
      result.emplace_back(std::move(file));
    } else
      throw std::runtime_error{"invalid reference \"" + reference.string() + "\" specified"};
    return result;
  }

  /**
   * @brief Appends the `appendix` to the `result`.
   */
  static void push_back(std::vector<fs::path>& result, std::vector<fs::path>&& appendix)
  {
    result.insert(cend(result), std::move_iterator(begin(appendix)), std::move_iterator(end(appendix)));
  }

  /**
   * @brief Appends the `path` to the `result` if it's not already there.
   */
  static void push_back_if_not_exists(std::vector<fs::path>& result, fs::path path)
  {
    if (std::none_of(cbegin(result), cend(result), [&](const auto& elem) { return elem == path; }))
      result.push_back(std::move(path));
  }

  /**
   * @brief Appends the `e` to the `result` if `e` represents the SQL file or the directory.
   */
  static void push_back_if_sql_file_or_directory(std::vector<fs::path>& result, const fs::directory_entry& e)
  {
    if (e.is_regular_file() && e.path().extension() == ".sql")
      push_back_if_not_exists(result, e.path().stem());
    else if (e.is_directory())
      push_back_if_not_exists(result, e.path().filename());
  }

  /**
   * @returns The vector of database object names from the specified `path`.
   */
  static std::vector<fs::path> refs_of_directory(const fs::path& path)
  {
    if (!is_directory(path))
      throw std::runtime_error{"directory \"" + path.string() + "\" does not exists"};
    std::vector<fs::path> result;
    for (const auto& e : fs::directory_iterator{path})
      push_back_if_sql_file_or_directory(result, e);
    return result;
  }

  /**
   * @returns The map of per-directory configuration parameters.
   */
  static internal::Simple_config parsed_config(const fs::path& path)
  {
    internal::Simple_config result{path};
    for (const auto& pair : result.data()) {
      if (pair.first != "explicit")
        throw std::logic_error{"unknown parameter \"" + pair.first + "\" specified in \"" + path.string() + "\""};
    }
    return result;
  }

  bool help_{};
};

// -----------------------------------------------------------------------------

/**
 * @brief Reprensents the "help" command.
 */
class Help final : public Command {
public:
  explicit Help(const std::vector<std::string>& opts)
  {
    const auto parse_option = [this](const std::string& o) { Command::parse_option(o); };
    auto i = parse_options(cbegin(opts), cend(opts), parse_option);
    if (i != cend(opts)) {
      if (*i != "help")
        command_ = make(*i, std::vector<std::string>{"--help"});

      if (++i != cend(opts))
        throw_invalid_usage("only one command can be specified");
    }
  }

  std::string usage() const override
  {
    return std::string{}
      .append("Usage: pgspa " + name() + " [options] [<command>]\n\n")
      .append("Options:\n")
      .append(Command::options());
  }

  std::string name() const override
  {
    return "help";
  }

  void run() override
  {
    if (!command_)
      std::cout << dmitigr::pgspa::usage() << "\n";
    else
      command_->go();
  }

private:
  std::unique_ptr<Command> command_;
};

// -----------------------------------------------------------------------------

/**
 * @brief Reprensents the "version" command.
 */
class Version final : public Command {
public:
  explicit Version(const std::vector<std::string>& opts)
  {
    const auto parse_option = [this](const std::string& o) { Command::parse_option(o); };
    const auto i = parse_options(cbegin(opts), cend(opts), parse_option);
    if (i != cend(opts))
      throw_invalid_usage();
  }

  std::string name() const override
  {
    return "version";
  }

  void run() override
  {
    constexpr int major_version = PGSPA_VERSION_PART1;
    constexpr int minor_version = PGSPA_VERSION_PART2;
    std::cout << major_version << "." << minor_version << "\n";
  }
};

// -----------------------------------------------------------------------------

/**
 * @brief Reprensents the "init" command.
 */
class Init final : public Command {
public:
  explicit Init(const std::vector<std::string>& opts)
  {
    const auto parse_option = [this](const std::string& o) { return Command::parse_option(o); };
    const auto i = parse_options(cbegin(opts), cend(opts), parse_option);
    if (i != cend(opts))
      throw_invalid_usage();
  }

  std::string name() const override
  {
    return "init";
  }

  void run() override
  {
    const auto p = fs::perms::owner_all |
      fs::perms::group_read |  fs::perms::group_exec |
      fs::perms::others_read | fs::perms::others_exec;
    const fs::path pgspa{".pgspa"};
    fs::create_directory(pgspa);
    fs::permissions(pgspa, p);
  }
};

// -----------------------------------------------------------------------------

namespace detail {

/**
 * @brief Reprensents the base for all "online" commands.
 *
 * The "online" command is command that requires the interaction with the server to run.
 */
class Online : public Command {
protected:
  /**
   * @returns The string with the options specific to this command.
   */
  static std::string options()
  {
    static std::string result{
      "  --host=<name> - the host name of the PostgreSQL server (\"localhost\" by default).\n"
      "  --address=<IP address> - the IP address of the PostgreSQL server (unset by default).\n"
      "  --port=<number> - the port number of the PostgreSQL server to operate (\"5432\" by default).\n"
      "  --user=<name> - the name of the user to operate (current username by default).\n"
      "  --password=<password> - the password (be aware, it may appear in the system logs!)\n"
      "  --database=<name> - the name of the database to operate (value of --user by default).\n"
      "  --client_encoding=<name> - the name of the client encoding to operate.\n"
      "  --connect_timeout=<seconds> - the connect timeout in seconds (\"8\" by default)."};
    return result;
  }

  /**
   * @returns The string with the all options of this command.
   */
  static std::string all_options()
  {
    static std::string result{Command::options().append("\n").append(options())};
    return result;
  }

  explicit Online(Online* const delegate)
    : delegate_{delegate}
  {
    ASSERT(is_invariant_ok());
  }

  explicit Online(std::string name)
    : data_{Data{}}
  {
    data_->name_ = std::move(name);
    data_->host_ = "localhost";
    data_->port_ = "5432";
    data_->username_ = internal::os::current_username();
    data_->connect_timeout_ = std::chrono::seconds{8};
    ASSERT(is_invariant_ok());
  }

  /**
   * @brief Parses the options specific to this command.
   */
  void parse_option(const std::string& option)
  {
    ASSERT(delegate() || data_);
    if (auto* const d = delegate()) {
      d->parse_option(option);
    } else {
      if (option.find("--host") == 0)
        data_->host_ = option_argument(option).value();
      else if (option.find("--address") == 0)
        data_->address_ = option_argument(option).value();
      else if (option.find("--port") == 0)
        data_->port_ = option_argument(option).value();
      else if (option.find("--database") == 0)
        data_->database_ = option_argument(option).value();
      else if (option.find("--user") == 0)
        data_->username_ = option_argument(option).value();
      else if (option.find("--password") == 0)
        data_->password_ = option_argument(option).value();
      else if (option.find("--client_encoding") == 0)
        data_->encoding_ = option_argument(option).value();
      else if (option.find("--connect_timeout") == 0)
        data_->connect_timeout_ = std::chrono::seconds{std::stoul(option_argument(option).value())};
      else
        Command::parse_option(option);
    }
    ASSERT(is_invariant_ok());
  };

  // ---------------------------------------------------------------------------

  std::string name() const override final
  {
    ASSERT(delegate() || data_);
    if (auto* const d = delegate())
      return d->name();
    else
      return data_->name_;
  }

  std::string usage() const override
  {
    return std::string()
      .append("Usage: pgspa " + name() + " [<options>]\n\n")
      .append("Options:\n")
      .append(all_options());
  }

  /**
   * @returns The delegate of this command.
   *
   * The main goal of using delegates is to combine different commands within a
   * single transaction. So, the Foo command can call the Bar command, providing
   * itself as a delegate. Then the Bar command will use the `pgfe::Connection`
   * object of the Foo command.
   */
  Online* delegate()
  {
    return const_cast<Online*>(static_cast<const Online*>(this)->delegate());
  }

  /**
   * @overload
   */
  const Online* delegate() const
  {
    return delegate_;
  }

  const std::string& database() const
  {
    ASSERT(delegate() || data_);
    if (auto* const d = delegate())
      return d->database();
    else {
      if (data_->database_.empty())
        data_->database_ = data_->username_;
      return data_->database_;
    }
  }

  const std::string& host_name() const
  {
    ASSERT(delegate() || data_);
    if (auto* const d = delegate())
      return d->host_name();
    else
      return data_->host_;
  }

  const std::optional<std::string>& host_address() const
  {
    ASSERT(delegate() || data_);
    if (auto* const d = delegate())
      return d->host_address();
    else
      return data_->address_;
  }

  const std::string& host_port() const
  {
    ASSERT(delegate() || data_);
    if (auto* const d = delegate())
      return d->host_port();
    else
      return data_->port_;
  }

  const std::string& username() const
  {
    ASSERT(delegate() || data_);
    if (auto* const d = delegate())
      return d->username();
    else
      return data_->username_;
  }

  const std::optional<std::string>& password() const
  {
    ASSERT(delegate() || data_);
    if (auto* const d = delegate())
      return d->password();
    else
      return data_->password_;
  }

  std::chrono::seconds connect_timeout() const
  {
    ASSERT(delegate() || data_);
    if (auto* const d = delegate())
      return d->connect_timeout();
    else
      return data_->connect_timeout_;
  }

  // ---------------------------------------------------------------------------

  /**
   * @brief Represents the batch of SQL commands of a file.
   */
  class Sql_batch final {
  public:
    explicit Sql_batch(const fs::path& path)
      : path_{path}
    {
      vec_ = pgfe::Sql_vector::make(internal::read_to_string(*path_));
      ASSERT(is_invariant_ok());
    }

    explicit Sql_batch(std::unique_ptr<pgfe::Sql_vector>&& vec)
      : vec_{std::move(vec)}
    {
      ASSERT(is_invariant_ok());
    }

    const std::optional<fs::path>& path() const
    {
      return path_;
    }

    pgfe::Sql_vector* sql_vector()
    {
      return const_cast<pgfe::Sql_vector*>(static_cast<const Sql_batch*>(this)->sql_vector());
    }

    const pgfe::Sql_vector* sql_vector() const
    {
      return vec_.get();
    }

  private:
    bool is_invariant_ok() const
    {
      const bool vec_ok = bool(vec_);
      return vec_ok;
    }

    std::unique_ptr<pgfe::Sql_vector> vec_;
    std::optional<fs::path> path_;
  };

  /**
   * @brief Represents the transaction guard.
   */
  class Tx_guard final {
  public:
    Tx_guard(const Tx_guard&) = delete;
    Tx_guard& operator=(const Tx_guard&) = delete;
    Tx_guard(Tx_guard&&) = delete;
    Tx_guard& operator=(Tx_guard&&) = delete;

    static void begin(pgfe::Connection* const conn)
    {
      ASSERT(conn);
      if (!conn->is_transaction_block_uncommitted())
        conn->perform("begin");
    }

    static void commit(pgfe::Connection* const conn)
    {
      ASSERT(conn);
      if (conn->is_transaction_block_uncommitted())
        conn->perform("commit");
    }

    static void rollback(pgfe::Connection* const conn)
    {
      ASSERT(conn);
      if (conn->is_transaction_block_uncommitted())
        conn->perform("rollback");
    }

    // -------------------------------------------------------------------------

    ~Tx_guard()
    {
      // No care about exceptions here: let it crash.
      rollback(conn_);
    }

    Tx_guard(pgfe::Connection* const conn)
      : conn_{conn}
    {
      ASSERT(conn_);
      begin(conn_);
    }

    void commit()
    {
      commit(conn_);
    }

  private:
    pgfe::Connection* conn_;
  };

  // ---------------------------------------------------------------------------

  /**
   * @returns The opened connection to the PostgreSQL server.
   */
  pgfe::Connection* conn()
  {
    ASSERT(delegate() || data_);
    namespace pgfe = dmitigr::pgfe;
    if (auto* const d = delegate()) {
      return d->conn();
    } else {
      auto& conn = data_->conn_;

      if (!conn) {
        conn = pgfe::Connection_options::make()->
          set_tcp_host_name(host_name())->
          set_tcp_host_address(host_address())->
          set_tcp_host_port(std::stoi(host_port()))->
          set_database(database())->
          set_username(username())->
          set_password(password())->
          make_connection();
      }

      if (!conn->is_connected()) {
        conn->connect(data_->connect_timeout_);
        if (!data_->encoding_.empty())
          conn->perform("set client_encoding to " + conn->to_quoted_identifier(data_->encoding_));
      }

      return conn.get();
    }
  }

  /**
   * @brief Executes the SQL commands of the specified files in a same transaction.
   */
  static std::size_t execute(pgfe::Connection* const conn, const std::vector<fs::path>& paths)
  {
    return execute(conn, make_batches(paths));
  }

  // ---------------------------------------------------------------------------

  bool is_invariant_ok() const
  {
    return (data_ && !delegate() && Command::is_invariant_ok()) ||
      (!data_ && delegate() && delegate()->is_invariant_ok());
  }

private:
  /**
   * @returns The count of non-empty SQL query strings in `vec`.
   */
  static std::size_t non_empty_count(const pgfe::Sql_vector* const vec)
  {
    ASSERT(vec);
    std::size_t result{};
    const auto count = vec->sql_string_count();
    using Counter = std::remove_const_t<decltype (count)>;
    for (Counter i = 0; i < count; ++i)
      if (!vec->sql_string(i)->is_query_empty())
        ++result;
    return result;
  }

  /**
   * @returns The starting string position of the SQL string at position `pos` of `vec`.
   */
  static std::size_t sql_string_position(const pgfe::Sql_vector* const vec, const std::size_t pos)
  {
    ASSERT(vec);
    ASSERT(pos < vec->sql_string_count());
    std::size_t result{};
    using Counter = std::remove_const_t<decltype (pos)>;
    for (Counter i = 0; i < pos; ++i)
      result += vec->sql_string(i)->to_string().size() + 1;
    return result;
  };

  /**
   * @brief Prints the Emacs-friendly information about an error to the standard error.
   */
  static void report_file_error(const fs::path& path, const std::size_t lnum, const std::size_t cnum, const std::string& msg)
  {
    /*
     * Use GNU style for reporting error messages:
     * foo.sql:3:1:Error: End of file during parsing
     * (See etc/compilation.txt of Emacs installation.)
     */
    std::cerr << absolute(path).string() << ":" << lnum << ":" << cnum << ":Error: " << msg << "\n";
  }

  /**
   * @brief Executes the SQL batches in the same transaction.
   */
  static std::size_t execute(pgfe::Connection* const conn, const std::vector<Sql_batch>& batches)
  {
    ASSERT(conn);
    ASSERT_ALWAYS(conn->is_transaction_block_uncommitted());

    std::size_t successes_count{};
    std::size_t total_count = [&batches]()
    {
      std::size_t result{};
      for (const auto& b : batches)
        result += non_empty_count(b.sql_vector());
      return result;
    }();

    /*
     * The Execution_status `es` indicates:
     *   - if (es == std::nullopt) then the query was not yet executed;
     *   - if (es == nullptr) then the query was executed successfully;
     *   - if (es != nullptr) then the query was executed with a error.
     */
    using Execution_status = std::optional<std::unique_ptr<pgfe::Error>>;
    auto batches_execution_statuses = [&batches]()
    {
      std::vector<std::vector<Execution_status>> result;
      for (const auto& b : batches)
        result.emplace_back(b.sql_vector()->sql_string_count());
      return result;
    }();

    const auto report_error = [&batches](const std::size_t i, const std::size_t j, const std::string& msg,
      const std::optional<std::size_t> query_offset = {})
    {
      ASSERT(i < batches.size());
      ASSERT(j < batches[i].sql_vector()->sql_string_count());
      ASSERT(!batches[i].sql_vector()->sql_string(j)->is_query_empty());
      const auto* const sv = batches[i].sql_vector();
      if (const auto& path = batches[i].path()) {
        const auto content = sv->to_string();
        const auto sso = sql_string_position(batches[i].sql_vector(), j);
        const auto qpos = query_offset ? sso + *query_offset : sso + internal::position_of_non_space(sv->sql_string(j)->to_string(), 0);
        const auto[lnum, cnum] = internal::line_column_numbers_by_position(content, qpos);
        report_file_error(*path, lnum, cnum, msg);
      } else {
        const auto content = sv->sql_string(j)->to_string();
        const auto qpos = query_offset.value_or(0);
        const auto[lnum, cnum] = internal::line_column_numbers_by_position(content, qpos);
        std::cerr << "pgspa internal query (see below):" << lnum << ":" << cnum << ":Error: " << msg << ":\n"
          << content << "\n";
      }
    };

    const auto query_position = [](const pgfe::Error* const e)
    {
      std::optional<std::size_t> result;
      if (const auto qp = e->query_position())
        result = std::stoul(*qp);
      return result;
    };

    conn->perform("savepoint p1");
    std::size_t iteration_successes_count{};
    const auto batches_size = batches.size();
    do {
      iteration_successes_count = 0;
      ASSERT(batches_size == batches_execution_statuses.size());
      using Counter = std::remove_const_t<decltype (batches_size)>;
      for (Counter i = 0; i < batches_size; ++i) {
        const auto sql_string_count = batches[i].sql_vector()->sql_string_count();
        ASSERT(sql_string_count == batches_execution_statuses[i].size());
        using Counter = std::remove_const_t<decltype (sql_string_count)>;
        for (Counter j = 0; j < sql_string_count; ++j) {
          auto& execution_status = batches_execution_statuses[i][j];
          if (!execution_status || *execution_status) {
            const auto* const sql_string = batches[i].sql_vector()->sql_string(j);
            if (!sql_string->is_query_empty()) {
              try {
                conn->execute(sql_string);
                conn->complete();
                conn->perform("savepoint p1");
                execution_status = nullptr; // done
                ++iteration_successes_count;
              } catch (const pgfe::Server_exception& e) {
                if (e.code() == pgfe::Server_errc::c42_duplicate_table ||
                  e.code() == pgfe::Server_errc::c42_duplicate_function ||
                  e.code() == pgfe::Server_errc::c42_duplicate_object ||
                  e.code() == pgfe::Server_errc::c42_duplicate_schema) {
                  conn->perform("rollback to savepoint p1");
                  execution_status = nullptr; // done
                  ++iteration_successes_count;
                } else if (e.code() == pgfe::Server_errc::c42_undefined_table ||
                  e.code() == pgfe::Server_errc::c42_undefined_function ||
                  e.code() == pgfe::Server_errc::c42_undefined_object ||
                  e.code() == pgfe::Server_errc::c3f_invalid_schema_name ||
                  e.code() == pgfe::Server_errc::c2b_dependent_objects_still_exist) {
                  conn->perform("rollback to savepoint p1");
                  execution_status = e.error()->to_error(); // error (hope for the next iteration)
                } else {
                  report_error(i, j, e.error()->brief(), query_position(e.error()));
                  throw Handled_exception{};
                }
              }
            } else
              execution_status = nullptr; // done (short-circuit an empty query execution)
          }
        }
      }
      successes_count += iteration_successes_count;
    } while (iteration_successes_count > 0);

    /*
     * If there are queries, which has not been executed without errors
     * it is necessary to report about them and to throw an exception.
     */
    if (successes_count < total_count) {
      const auto batches_execution_statuses_size = batches_execution_statuses.size();
      using Counter = std::remove_const_t<decltype (batches_execution_statuses_size)>;
      for (Counter i = 0; i < batches_execution_statuses_size; ++i) {
        const auto batch_execution_statuses_size = batches_execution_statuses[i].size();
        using Counter = std::remove_const_t<decltype (batch_execution_statuses_size)>;
        for (Counter j = 0; j < batch_execution_statuses_size; ++j) {
          const auto& execution_status = batches_execution_statuses[i][j];
          ASSERT(execution_status);
          if (const std::unique_ptr<pgfe::Error>& e = *execution_status)
            report_error(i, j, e->brief(), query_position(e.get()));
        }
      }
      throw Handled_exception{};
    }

    return total_count;
  }

  /**
   * @returns The vector of SQL batches from the vector of file paths.
   */
  static std::vector<Sql_batch> make_batches(const std::vector<fs::path>& paths)
  {
    std::vector<Sql_batch> result;
    result.reserve(paths.size());
    for (const auto& path : paths)
      result.emplace_back(path);
    return result;
  }

  // ---------------------------------------------------------------------------

  struct Data {
    std::string name_;

    std::string host_;
    std::optional<std::string> address_;
    std::string port_;
    std::string database_;
    std::string username_;
    std::optional<std::string> password_;
    std::string encoding_;

    std::chrono::seconds connect_timeout_;

    std::unique_ptr<pgfe::Connection> conn_;
  };
  mutable std::optional<Data> data_;
  Online* delegate_{};
};

} // namespace detail

// -----------------------------------------------------------------------------

/**
 * @brief Reprensents the `exec` command.
 *
 * The `exec` command executes the bunch of specified SQL queries.
 */
class Exec final : public detail::Online {
public:
  /**
   * @returns The string with the options specific to this command.
   */
  static std::string options()
  {
    static std::string result{""};
    return result;
  }

  /**
   * @returns The string with the all options of this command.
   */
  static std::string all_options()
  {
    static std::string result{Online::all_options().append("\n").append(options())};
    return result;
  }

  explicit Exec(Online* const delegate)
    : Online{delegate}
  {
    ASSERT(is_invariant_ok());
  }

  explicit Exec(const std::vector<std::string>& opts)
    : Online{"exec"}
  {
    const auto parse_option = [this](const std::string& o) { return Online::parse_option(o); };
    const auto b = cbegin(opts), e = cend(opts);
    for (auto i = parse_options(b, e, parse_option); i != e; ++i)
      args_.push_back(*i);
    if (args_.empty())
      throw_invalid_usage("no references specified");
    ASSERT(is_invariant_ok());
  }

  std::string usage() const override
  {
    return std::string()
      .append("Usage: pgspa " + name() + " [<options>] reference ...\n\n")
      .append("Options:\n")
      .append(all_options());
  }

  void run() override
  {
    auto* const cn = conn();
    Tx_guard t{cn};
    run__(cn);
    t.commit();
  }

private:
  bool is_invariant_ok() const
  {
    return !args_.empty();
  }

  void run__(pgfe::Connection* const cn)
  {
    for (const auto& arg : args_) {
      const auto count = execute(cn, sql_paths(root_path() / arg));
      std::cout << "The reference \"" << arg << "\". Executed queries count = " << count << ".\n";
    }
  }

  std::vector<std::string> args_;
};

// -----------------------------------------------------------------------------

template<typename ... Types>
std::unique_ptr<Command> Command::make(const std::string& cmd, Types&& ... opts)
{
  ASSERT(!cmd.empty());
  if (cmd == "help")
    return std::make_unique<Help>(std::forward<Types>(opts)...);
  else if (cmd == "version")
    return std::make_unique<Version>(std::forward<Types>(opts)...);
  else if (cmd == "init")
    return std::make_unique<Init>(std::forward<Types>(opts)...);
  else if (cmd == "exec")
    return std::make_unique<Exec>(std::forward<Types>(opts)...);
  else
    throw std::logic_error{"invalid command \"" + cmd + "\""};
}

} // namespace dmitigr:pgspa

int main(const int argc, const char* const argv[])
{
  namespace pgfe = dmitigr::pgfe;
  namespace spa = dmitigr::pgspa;
  const auto executable_name = argv[0];
  try {
    if (argc > 1) {
      const auto [cmd, opts] = spa::internal::command_and_options(argc, argv);
      const auto command = spa::Command::make(cmd, opts);
      command->go();
    } else {
      std::cerr << spa::usage() << "\n";
      return 1;
    }
  } catch (const spa::Handled_exception&) {
    return 1;
  } catch (const pgfe::Server_exception& e) {
    std::cerr << executable_name << ": server error" << "\n";
    const auto* const error = e.error();
    std::cerr << "  Brief: " << error->brief() << "\n";
    if (error->detail())
      std::cerr << "  Details: " << error->detail().value() << "\n";
    if (error->hint())
      std::cerr << "  Hint: " << error->hint().value() << "\n";
    if (error->query_position())
      std::cerr << "  Query position: " << error->query_position().value() << "\n";
    if (error->internal_query_position())
      std::cerr << "  Internal query position: " << error->internal_query_position().value() << "\n";
    if (error->internal_query())
      std::cerr << "  Internal query: " << error->internal_query().value() << "\n";
    if (error->context())
      std::cerr << "  Context: " << error->context().value() << "\n";
  } catch (const std::exception& e) {
    std::cerr << executable_name << ": " << e.what() << "\n";
    return 1;
  } catch (...) {
    std::cerr << executable_name << ": " << "unknown error" << "\n";
    return 2;
  }
}
