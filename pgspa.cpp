// -*- C++ -*-
// Copyright (C) Dmitry Igrishin
// For conditions of distribution and use, see files LICENSE.txt

#ifdef _WIN32
#define NOMINMAX
#endif

#include <dmitigr/app.hpp>
#include <dmitigr/base.hpp>
#include <dmitigr/cfg.hpp>
#include <dmitigr/fs.hpp>
#include <dmitigr/os.hpp>
#include <dmitigr/pgfe.hpp>
#include <dmitigr/str.hpp>

#include <algorithm>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#define ASSERT(a) DMITIGR_ASSERT(a)
#define ASSERT_ALWAYS(a) DMITIGR_ASSERT_ALWAYS(a)

namespace dmitigr::pgspa {

namespace filesystem = std::filesystem;

/// @returns The general usage info.
inline std::string usage()
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

const filesystem::path root_marker{".pgspa"};
const filesystem::path per_directory_config{".pgspa_config"};

/// @brief Utility functions.
struct Util final {
  /// @returns The root path of the project.
  static filesystem::path root_path()
  {
    if (auto result = fs::parent_directory_path(root_marker))
      return *result;
    else
      throw std::runtime_error{"no directory \"" + root_marker.string() +
          "\" found in parent directory hierarchy"};
  }

  /// @returns The vector of paths to SQL files of the specified `ref`.
  static std::vector<filesystem::path> sql_paths(const filesystem::path& ref)
  {
    return sql_paths(ref, {ref});
  }

  /// @returns The vector of paths to SQL files by the specified `reference`.
  static std::vector<filesystem::path> sql_paths(const filesystem::path& reference,
    std::vector<filesystem::path> trace)
  {
    std::vector<filesystem::path> result;

    if (const auto name = reference.filename(); name.empty() || name.string().front() == '.')
      throw std::logic_error{"the reference name cannot be empty or starts with the dot (\".\")"};

    static const auto sql_file_path = [](const filesystem::path& reference)
    {
      auto file = reference;
      return file.replace_extension(".sql");
    };

    if (is_regular_file(reference) && reference.extension() == ".sql") {
      result.push_back(reference);
    } else if (is_regular_file(reference) && reference.extension().empty()) {
      static const auto is_nor_empty_nor_commented = [](const std::string& line)
      {
        return (!line.empty() && line.front() != '#');
      };
      const auto parent = reference.parent_path();
      const auto paths = str::file_to_strings_if(reference, is_nor_empty_nor_commented);

      const auto is_in_trace = [&trace](const filesystem::path& p)
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
      if (const auto config = reference / per_directory_config; is_regular_file(config)) {
        if (auto params = parsed_config(config); params.boolean_parameter("explicit").value_or(false)) {
          throw std::runtime_error{"the references of the directory \"" + reference.string() +
              "\" are allowed to be used only explicitly"};
        }
      }

      if (auto heading_file = sql_file_path(reference); is_regular_file(heading_file))
        result.emplace_back(std::move(heading_file));

      const auto refs_of_dir = [&reference]
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

  /// @brief Appends the `appendix` to the `result`.
  static void push_back(std::vector<filesystem::path>& result,
    std::vector<filesystem::path>&& appendix)
  {
    result.insert(cend(result), std::move_iterator(begin(appendix)),
      std::move_iterator(end(appendix)));
  }

  /// @brief Appends `path` to `result` if it's not already there.
  static void push_back_if_not_exists(std::vector<filesystem::path>& result,
    filesystem::path path)
  {
    if (std::none_of(cbegin(result), cend(result),
        [&](const auto& elem) { return elem == path; }))
      result.push_back(std::move(path));
  }

  /// @brief Appends `e` to `result` if `e` represents the SQL file or the directory.
  static void push_back_if_sql_file_or_directory(std::vector<filesystem::path>& result,
    const filesystem::directory_entry& e)
  {
    const auto& path = e.path();
    if (is_regular_file(path) && path.extension() == ".sql")
      push_back_if_not_exists(result, path.stem());
    else if (is_directory(path))
      push_back_if_not_exists(result, path.filename());
  }

  /// @returns The vector of database object names from the specified `path`.
  static std::vector<filesystem::path> refs_of_directory(const filesystem::path& path)
  {
    if (!is_directory(path))
      throw std::runtime_error{"directory \"" + path.string() + "\" does not exists"};
    std::vector<filesystem::path> result;
    for (const auto& e : filesystem::directory_iterator{path})
      push_back_if_sql_file_or_directory(result, e);
    return result;
  }

  /// @returns The per-directory configuration.
  static cfg::Flat parsed_config(const filesystem::path& path)
  {
    cfg::Flat result{path};
    for (const auto& pair : result.parameters()) {
      if (pair.first != "explicit")
        throw std::logic_error{"unknown parameter \"" + pair.first +
            "\" specified in \"" + path.string() + "\""};
    }
    return result;
  }
};

// ===========================================================================

/// @brief A batch of SQL commands of a file.
class Sql_batch final {
public:
  explicit Sql_batch(const filesystem::path& path)
    : path_{path}
  {
    vec_ = pgfe::Sql_vector::make(str::file_to_string(*path_));
    ASSERT(is_invariant_ok());
  }

  explicit Sql_batch(std::unique_ptr<pgfe::Sql_vector>&& vec)
    : vec_{std::move(vec)}
  {
    ASSERT(is_invariant_ok());
  }

  /// @returns The vector of SQL batches from the vector of file paths.
  static std::vector<Sql_batch> make_many(const std::vector<filesystem::path>& paths)
  {
    std::vector<Sql_batch> result;
    result.reserve(paths.size());
    for (const auto& path : paths)
      result.emplace_back(path);
    return result;
  }

  const std::optional<filesystem::path>& path() const
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
  std::optional<filesystem::path> path_;
};

// ===========================================================================

/// @brief A transaction guard.
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

/**
 * @brief A dummy exception to throw after handling of a real exception.
 *
 * @remarks The exception handlers of this type shouldn't have side effects.
 */
class Handled_exception final {};

/// @brief A pgspa command.
class Command {
public:
  template<typename ... Types>
  static std::unique_ptr<Command> make(const std::string_view name, Types&& ... params);

  /// @returns A new instance from the textual type identifier.
  static std::unique_ptr<Command> make(const app::Program_parameters& params);

  /// @returns The string with the options specific to this command.
  static std::string options()
  {
    static std::string result;
    return result;
  }

  /// @returns The name of the command.
  virtual const std::string& name() const
  {
    return name_;
  }

  /// @returns The general usage info of this command.
  virtual std::string usage() const
  {
    return std::string{}
      .append("Usage: pgspa " + name() + "\n\n")
      .append("Options:\n")
      .append(options());
  }

  /**
   * @brief Runs the current command.
   *
   * @see run().
   */
  void go()
  {
    run();
  }

protected:
  /// @brief The default constructor. (Used for delegates.)
  Command() = default;

  /// @brief The constructor. (Used for help.)
  explicit Command(std::string name)
    : name_{std::move(name)}
  {}

  /// @brief The constructor.
  explicit Command(const app::Program_parameters& params)
    : name_{params.command_name().value()}
  {}

  /// @returns `true` if the invariant is okay, or `false` otherwise.
  virtual bool is_invariant_ok() const
  {
    return !name_.empty();
  }

  /**
   * @brief Throws `std::runtime_error` if there are an option in `params`
   * which is not in `opts`.
   */
  void check_options(const app::Program_parameters& params, const std::vector<std::string>& opts)
  {
    if (const auto i = params.option_other_than(opts); i != cend(params.options()))
      throw std::runtime_error{"invalid option " + i->first};
  }

private:
  /**
   * @brief Runs the current command.
   *
   * @remarks The API function `go()` calls this function.
   *
   * @see go().
   */
  virtual void run() = 0;

  std::string name_;
};

// =============================================================================

/// @brief The "help" command.
class Help final : public Command {
public:
  Help()
    : Command("help")
  {}

  explicit Help(const app::Program_parameters& params)
    : Command{params}
  {
    const auto& args = params.arguments();
    if (args.size() != 1)
      usage_ = usage();
    else
      usage_ = make(args.back())->usage();
  }

  std::string usage() const override
  {
    return std::string{}
      .append("Usage: pgspa " + name() + " [options] [<command>]\n\n")
      .append("Options:\n")
      .append(Command::options());
  }

private:
  std::string usage_;

  void run() override
  {
    std::cout << usage_ << "\n";
  }
};

// =============================================================================

/// @brief The "version" command.
class Version final : public Command {
public:
  Version()
    : Command("version")
  {}

  explicit Version(const app::Program_parameters& params)
    : Command{params}
  {}

private:
  void run() override
  {
    constexpr int major_version = PGSPA_VERSION_MAJOR;
    constexpr int minor_version = PGSPA_VERSION_MINOR;
    std::cout << major_version << "." << minor_version << "\n";
  }
};

// =============================================================================

/// @brief The "init" command.
class Init final : public Command {
public:
  Init()
    : Command("init")
  {}

  explicit Init(const app::Program_parameters& params)
    : Command{params}
  {}

private:
  void run() override
  {
    const auto p = filesystem::perms::owner_all |
      filesystem::perms::group_read  | filesystem::perms::group_exec |
      filesystem::perms::others_read | filesystem::perms::others_exec;
    filesystem::create_directory(root_marker);
    filesystem::permissions(root_marker, p);
  }
};

// =============================================================================

namespace detail {

/**
 * @brief The base for all "online" commands.
 *
 * An "online" command - is a command that requires the interaction with a
 * PostgreSQL server to run.
 */
class Online : public Command {
protected:
  /// @returns The string with the options specific to this command.
  static std::string options()
  {
    static std::string result{
      "  --host=<name> - the hostname of the PostgreSQL server (\"localhost\" by default).\n"
      "  --address=<IP address> - the IP address of the PostgreSQL server to connect to (\"127.0.0.1\" by default).\n"
      "  --port=<number> - the port number of the PostgreSQL server to operate (\"5432\" by default).\n"
      "  --username=<name> - the name of the user to operate (current username by default).\n"
      "  --password=<password> - the password (be aware, it may appear in the system logs!)\n"
      "  --database=<name> - the name of the database to operate (value of --user by default).\n"
      "  --client_encoding=<name> - the name of the client encoding to operate.\n"
      "  --connect_timeout=<seconds> - the connect timeout in seconds (\"8\" by default)."};
    return result;
  }

  /// @returns The string with the all options of this command.
  static std::string all_options()
  {
    static std::string result{Command::options().append("\n").append(options())};
    return result;
  }

  Online(std::string name)
    : Command{std::move(name)}
  {}

  explicit Online(Online* const delegate)
    : delegate_{delegate}
  {
    ASSERT(is_invariant_ok());
  }

  explicit Online(const app::Program_parameters& params)
    : Command(params)
  {
    if (const auto& o = params.option_with_argument("host"))
      data_->host_ = *o;
    else
      data_->host_ = "localhost";

    if (const auto& o = params.option_with_argument("address"))
      data_->address_ = *o;
    else
      data_->address_ = "127.0.0.1";

    if (const auto& o = params.option_with_argument("port"))
      data_->port_ = *o;
    else
      data_->port_ = "5432";

    if (const auto& o = params.option_with_argument("username"))
      data_->username_ = *o;
    else
      data_->username_ = os::env::current_username();

    if (const auto& o = params.option_with_argument("database"))
      data_->database_ = *o;
    else
      data_->database_ = data_->username_;

    if (const auto& o = params.option_with_argument("password"))
      data_->password_ = *o;

    if (const auto& o = params.option_with_argument("client_encoding"))
      data_->client_encoding_ = *o;

    if (const auto& o = params.option_with_argument("connect_timeout"))
      data_->connect_timeout_ = std::chrono::seconds{std::stoul(*o)};
    else
      data_->connect_timeout_ = std::chrono::seconds{8};

    ASSERT(is_invariant_ok());
  }

  /// @see Command::name().
  const std::string& name() const override
  {
    ASSERT(delegate() || data_);
    if (auto* const d = delegate())
      return d->name();
    else
      return Command::name();
  }

  /// @see Command::usage().
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

  /// @overload
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

  const std::optional<std::string>& host_name() const
  {
    ASSERT(delegate() || data_);
    if (auto* const d = delegate())
      return d->host_name();
    else
      return data_->host_;
  }

  const std::string& host_address() const
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

  /// @returns The opened connection to the PostgreSQL server.
  pgfe::Connection* conn()
  {
    ASSERT(delegate() || data_);
    namespace pgfe = dmitigr::pgfe;
    if (auto* const d = delegate()) {
      return d->conn();
    } else {
      auto& conn = data_->conn_;

      if (!conn) {
        conn = pgfe::Connection_options::make(pgfe::Communication_mode::net)->
          set_net_address(host_address())->
          set_net_hostname(host_name())->
          set_port(std::stoi(host_port()))->
          set_database(database())->
          set_username(username())->
          set_password(password())->
          make_connection();
      }

      if (!conn->is_connected()) {
        conn->connect(data_->connect_timeout_);
        if (!data_->client_encoding_.empty())
          conn->perform("set client_encoding to " +
            conn->to_quoted_identifier(data_->client_encoding_));
      }

      return conn.get();
    }
  }

  bool is_invariant_ok() const override
  {
    return (data_ && !delegate() && Command::is_invariant_ok()) ||
      (!data_ && delegate() && delegate()->is_invariant_ok());
  }

private:
  struct Data final {
    std::string name_;

    std::string address_;
    std::optional<std::string> host_;
    std::string port_;
    std::string database_;
    std::string username_;
    std::optional<std::string> password_;
    std::string client_encoding_;

    std::chrono::seconds connect_timeout_;

    std::unique_ptr<pgfe::Connection> conn_;
  };
  mutable std::optional<Data> data_{Data{}};
  Online* delegate_{};
};

} // namespace detail

// =============================================================================

/**
 * @brief The `exec` command.
 *
 * The `exec` command executes the bunch of specified SQL queries.
 */
class Exec final : public detail::Online {
public:
  /// @returns The string with the options specific to this command.
  static std::string options()
  {
    static std::string result{""};
    return result;
  }

  /// @returns The string with the all options of this command.
  static std::string all_options()
  {
    static std::string result{Online::all_options().append("\n").append(options())};
    return result;
  }

  Exec()
    : Online{"exec"}
  {}

  explicit Exec(Online* const delegate)
    : Online{delegate}
  {
    ASSERT(is_invariant_ok());
  }

  explicit Exec(const app::Program_parameters& params)
    : Online{params}
    , args_{params.arguments()}
  {
    check_options(params, {"host", "address", "port", "database", "username",
                           "password", "client_encoding", "connect_timeout"});

    if (args_.empty())
      throw std::runtime_error("no references specified");

    ASSERT(is_invariant_ok());
  }

  std::string usage() const override
  {
    return std::string()
      .append("Usage: pgspa " + name() + " [<options>] reference ...\n\n")
      .append("Options:\n")
      .append(all_options());
  }

private:
  std::vector<std::string> args_;

  bool is_invariant_ok() const override
  {
    return !args_.empty();
  }

  void run() override
  {
    auto* const cn = conn();
    Tx_guard t{cn};
    run__(cn);
    t.commit();
  }

  void run__(pgfe::Connection* const cn)
  {
    for (const auto& arg : args_) {
      const auto count = execute(cn, Util::sql_paths(Util::root_path() / arg));
      std::cout << "The reference \"" << arg << "\". Executed queries count = " << count << ".\n";
    }
  }

  /// @brief Executes the SQL commands of the specified files in a transaction.
  static std::size_t execute(pgfe::Connection* const conn,
    const std::vector<filesystem::path>& paths)
  {
    return execute(conn, Sql_batch::make_many(paths));
  }

  /// @brief Executes the SQL batches in the same transaction.
  static std::size_t execute(pgfe::Connection* const conn,
    const std::vector<Sql_batch>& batches)
  {
    ASSERT(conn);
    ASSERT_ALWAYS(conn->is_transaction_block_uncommitted());

    std::size_t successes_count{};
    std::size_t total_count = [&batches]()
    {
      std::size_t result{};
      for (const auto& b : batches)
        result += b.sql_vector()->non_empty_count();
      return result;
    }();

    /*
     * The Execution_status `es` indicates:
     *   - if (es == std::nullopt) then the query was not yet executed;
     *   - if (es == nullptr) then the query was executed successfully;
     *   - if (es != nullptr) then the query was executed with a error.
     */
    using Execution_status = std::optional<std::unique_ptr<pgfe::Error>>;
    auto batches_execution_statuses = [&batches]
    {
      std::vector<std::vector<Execution_status>> result;
      for (const auto& b : batches)
        result.emplace_back(b.sql_vector()->sql_string_count());
      return result;
    }();

    const auto query_position = [](const pgfe::Error* const e)
    {
      std::optional<std::size_t> result;
      if (const auto qp = e->query_position())
        result = std::stoul(*qp);
      return result;
    };

    const auto report_error = [&batches, &query_position](const std::size_t i,
      const std::size_t j, const pgfe::Error* const err)
    {
      /// @brief Prints the Emacs-friendly information about an error to the standard error.
      static const auto report_file_error = [](const filesystem::path& path,
        const std::size_t lnum, const std::size_t cnum, const pgfe::Error* const err)
      {
        /*
         * Use GNU style for reporting error messages:
         * foo.sql:3:1:Error: End of file during parsing
         * (See etc/compilation.txt of Emacs installation.)
         */
        std::cerr << absolute(path).string() << ":"
                  << lnum << ":" << cnum << ":Error: " << err->brief();
        if (const auto& d = err->detail())
          std::cerr << "\n Detail: " << *d;
        if (const auto& h = err->hint())
          std::cerr << "\n Hint: " << *h;
        if (const auto& c = err->context())
          std::cerr << "\n Context: " << *c;
        std::cerr << "\n";
      };

      ASSERT(i < batches.size());
      const auto* const sql_vector = batches[i].sql_vector();
      ASSERT(j < sql_vector->sql_string_count());
      ASSERT(!sql_vector->sql_string(j)->is_query_empty());
      ASSERT(err);
      const auto query_offset = query_position(err);
      if (const auto& path = batches[i].path()) {
        const auto content = sql_vector->to_string();
        const auto ssp = sql_vector->query_absolute_position(j);
        const auto qpos = query_offset ?
          ssp + *query_offset :
          ssp + str::position_of_non_space(sql_vector->sql_string(j)->to_query_string(), 0);
        const auto[lnum, cnum] = str::line_column_numbers_by_position(content, qpos - 1);
        report_file_error(*path, lnum + 1, cnum + 1, err);
      } else {
        const auto content = sql_vector->sql_string(j)->to_string();
        const auto qpos = query_offset.value_or(0);
        const auto[lnum, cnum] = str::line_column_numbers_by_position(content, qpos - 1);
        std::cerr << "pgspa internal query (see below):"
                  << lnum + 1 << ":" << cnum + 1 << ":Error: " << err->brief() << ":\n"
                  << content << "\n";
      }
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
                execution_status = nullptr; // done
                ++iteration_successes_count;
                conn->perform("savepoint p1");
              } catch (const pgfe::Server_exception& e) {
                if (e.code() == pgfe::Server_errc::c42_duplicate_table ||
                  e.code() == pgfe::Server_errc::c42_duplicate_function ||
                  e.code() == pgfe::Server_errc::c42_duplicate_object ||
                  e.code() == pgfe::Server_errc::c42_duplicate_schema) {
                  execution_status = nullptr; // done
                  ++iteration_successes_count;
                  conn->perform("rollback to savepoint p1");
                } else if (e.code() == pgfe::Server_errc::c42_undefined_table ||
                  e.code() == pgfe::Server_errc::c42_undefined_function ||
                  e.code() == pgfe::Server_errc::c42_undefined_object ||
                  e.code() == pgfe::Server_errc::c3f_invalid_schema_name ||
                  e.code() == pgfe::Server_errc::c2b_dependent_objects_still_exist) {
                  execution_status = e.error()->to_error(); // error (hope for the next iteration)
                  conn->perform("rollback to savepoint p1");
                  ASSERT(execution_status && *execution_status);
                } else {
                  execution_status = e.error()->to_error(); // fatal error (which will be reported last)
                  goto finish;
                }
              }
            } else
              execution_status = nullptr; // done (short-circuit an empty query execution)
          }
        }
      }
      successes_count += iteration_successes_count;
    } while (iteration_successes_count > 0);

  finish:

    /*
     * If there are queries, which was not executed without errors
     * it's necessary to report about them and to throw an exception.
     */
    if (successes_count < total_count) {
      const auto batches_execution_statuses_size = batches_execution_statuses.size();
      using Counter = std::remove_const_t<decltype (batches_execution_statuses_size)>;
      for (Counter i = 0; i < batches_execution_statuses_size; ++i) {
        const auto batch_execution_statuses_size = batches_execution_statuses[i].size();
        using Counter = std::remove_const_t<decltype (batch_execution_statuses_size)>;
        for (Counter j = 0; j < batch_execution_statuses_size; ++j) {
          if (const auto& execution_status = batches_execution_statuses[i][j]) {
            if (const std::unique_ptr<pgfe::Error>& e = *execution_status)
              report_error(i, j, e.get());
          }
        }
      }
      throw Handled_exception{};
    }

    return total_count;
  }
};

// =============================================================================

template<typename ... Types>
std::unique_ptr<Command> Command::make(const std::string_view name, Types&& ... params)
{
  if (name == "help")
    return std::make_unique<Help>(std::forward<Types>(params)...);
  else if (name == "version")
    return std::make_unique<Version>(std::forward<Types>(params)...);
  else if (name == "init")
    return std::make_unique<Init>(std::forward<Types>(params)...);
  else if (name == "exec")
    return std::make_unique<Exec>(std::forward<Types>(params)...);
  else
    throw std::logic_error{"unknown command \"" + std::string{name} + "\""};
}

std::unique_ptr<Command> Command::make(const app::Program_parameters& params)
{
  DMITIGR_ASSERT(params.is_valid());
  if (const auto& cmd = params.command_name(); cmd)
    return make(*cmd, params);
  else
    throw std::runtime_error{"no command specified"};
}

} // namespace dmitigr:pgspa

int main(const int argc, const char* const argv[])
{
  namespace app = dmitigr::app;
  namespace pgfe = dmitigr::pgfe;
  namespace spa = dmitigr::pgspa;

  if (argc <= 1) {
    std::cerr << spa::usage() << "\n";
    return 1;
  }

  const app::Program_parameters params{argc, argv};
  try {
    const auto command = spa::Command::make(params);
    command->go();
  } catch (const spa::Handled_exception&) {
    return 1;
  } catch (const pgfe::Server_exception& e) {
    std::cerr << params.executable_path() << ": server error" << "\n";
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
    std::cerr << params.executable_path() << ": " << e.what() << "\n";
    return 1;
  } catch (...) {
    std::cerr << params.executable_path() << ": " << "unknown error" << "\n";
    return 2;
  }
}
