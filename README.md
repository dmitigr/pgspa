The SQL Programming Assistant for PostgreSQL
============================================

The SQL Programming Assistant for [PostgreSQL] (hereinafter referred to as
Pgspa) - is a command line program and the PostgreSQL extension to simplify
the development of the PostgreSQL databases.

**ATTENTION, this software is "alpha" quality, use it at your own risk!**

Pgspa allows to execute a set of SQL queries from files of arbitrary directory
hierarchy in *one* transaction. The queries of the set are executed iteratively.
Queries ending with the ignorable errors are not executed in the next iteration,
while queries ending with the non-fatal errors are executed in the next
iteration. The execution stops when there are no successfully executed queries,
or there are at least one query ended with a fatal error during the iteration.
Finally, if not all the queries of the set considered done, the entire
transaction is rolled back and Pgspa reports about the problems.

Table 1. Ignorable errors (queries ended with these errors are considered done)

|SQLSTATE code|Condition name|
|:------------|:-------------|
|42723|duplicate_function|
|42P06|duplicate_schema|
|42P07|duplicate_table|
|42710|duplicate_object|

Table 2. Non-fatal errors (queries ended with these errors will be re-executed
in the next iteration)

|SQLSTATE code|Condition name|
|:------------|:-------------|
|2BP01|dependent_objects_still_exist|
|3F000|invalid_schema_name|
|42883|undefined_function|
|42P01|undefined_table|
|42704|undefined_object|

All other errors are treated as fatal and leads to failure of the entire transaction.

Pgspa also ships with the server-side PostgreSQL extension `dmitigr_spa` which
provides the convenient set of functions for database developers including
functions to drop the interdependent database objects in the *non-cascade* mode.

Tutorial
========

To start working with Pgspa the directory with the SQL source files must be
marked as the "project directory" by using `pgspa init` command:

    $ cd /path/to/the/database/project
    $ pgspa init

This command will create the directory called `.pgspa`.

The project directory and its subdirectories can contains the following files:

  - the SQL sources - files with ".sql" extension;
  - the shortcuts - files without extension;
  - .pgspa_config - per-directory configuration file.

The SQL source files, directories and shortcuts are so called *references*. The
reference name cannot be empty or starts with the dot (".").

The extension for PostgreSQL can be created by using the following SQL query:

```sql
CREATE EXTENSION dmitigr_spa
```

SQL source files
----------------

Each SQL query (except the very last) must ends with the semicolon. At this
moment, SQL queries cannot be parameterized, but there are plans to add this
feature in the future.

The SQL source files can be organized in the arbitrary directory hierarchies.
They will be executed in lexicographical order of the file names. Each directory
can contain the both file `foo.sql` (so called *heading file*) and directory
`foo/`. In this case, the command `pgspa exec foo` will execute the queries of
`foo.sql` at first, and then the queries of `foo/`.

The concrete SQL file and/or set of SQL files and directories can be specified
as the arguments of the `exec` command, for example:

    $ pgspa exec foo bar.sql baz

All queries of the specified bunch will be run in the *same* transaction. Since
Pgspa is using GNU style for reporting error messages, [Emacs] users can run
Pgspa in the [Compilation Mode][emacs-compilation-mode] to immediately jump to
the erroneous queries.

Shortcuts
---------

Shortcuts - are regular files without an extension that contains the names of
SQL files, directories and/or shortcuts (i.e. *references*) relative to the
directory in which the shortcut is located. Shortcuts are useful to define a
sequence of references to be executed without the need to specifying them in
the command line each time. For example, consider the following simple case:

    schemas/create.sql
            drop.sql

Here obviously, that `create.sql` contains the DDL queries to create the objects
of the schemas, and `drop.sql` contains the DDL queries to drop the objects of
the schemas. To *recreate* the objects, the following command can be used:

    $ pgspa exec schemas/drop schemas/create

The same effect can be achieved by creating the shortcut `recreate` - a file
named as `recreate` with the following content:

    drop
    create

Then this shortcut can be used like that:

    $ pgspa exec schemas/recreate

**Note, that cyclic shortcuts are not allowed.**

But there are still some gotcha: all queries of the whole `schemas` directory
can be accidentally executed as simple as:

    $ pgspa exec schemas

It will lead to executing `schemas/create.sql` first and then `schemas/drop.sql`.
It's absolutely senselessly and may be even dangerous, since all of the objects
will be eventually dropped. Such incidents can be prevented by using
per-directory configuration (namely, the parameter `explicit`, described below).

Per-directory configuration
---------------------------

Each subdirectory of the project directory can contain the configuration file
named `.pgspa_config`. The format of this file is very simple:

    parameter1 = value_without_spaces
    parameter2 = 'value with spaces'
    # commented_out_parameter_with_empty_value =

Currently supported parameters are:

  - `explicit` - is a boolean parameter (possible values are "yes"/"no") which
    allows to run only SQL queries by the explicitly specified *references*.
    For example, if the directory `foo` is marked with `explicit` parameter,
    the only way to run the SQL queries of this directory is to use one of the
    reference of this directory, like `foo/bar` or `foo/baz.sql`.

Dependencies
============

- [CMake] build system version 3.10+;
- C++17 compiler ([GCC] 7.4+ or [Microsoft Visual C++][Visual_Studio] 15.7+);
- [Cefeika][dmitigr_cefeika] libraries.

Customization
=============

The table below (may need to use horizontal scrolling for full view) contains
variables which can be passed to CMake for customization.

|CMake variable|Possible values|Default on Unix|Default on Windows|
|:-------------|:--------------|:--------------|:-----------------|
|**The type of the build**||||
|CMAKE_BUILD_TYPE|Debug \| Release \| RelWithDebInfo \| MinSizeRel|Debug|Debug|
|**Installation directories**||||
|CMAKE_INSTALL_PREFIX|*an absolute path*|"/usr/local"|"%ProgramFiles%\dmitigr_pgspa"|
|DMITIGR_PGSPA_BIN_INSTALL_DIR|*a path relative to CMAKE_INSTALL_PREFIX*|"bin"|*not set*|
|DMITIGR_PGSPA_PG_SHAREDIR|*an absolute path*|*not set (can be set manually)*|*not set (can be set manually)*|

Installation
============

Pgspa ships with the PostgreSQL extension called `dmitigr_spa` which consists
of two files:

  - dmitigr_spa--0.1.sql
  - dmitigr_spa.control

It will be placed to the directory specified via the `DMITIGR_PGSPA_PG_SHAREDIR`
variable. (Usually, it should be something like `/usr/local/pgsql/share/extension`
on Linux and `C:\Program Files\PostgreSQL\10\share\extension` on Microsoft Windows.)

The default build type is "Debug".

Installation on Linux
---------------------

    $ git clone https://github.com/dmitigr/pgspa.git
    $ mkdir -p pgspa/build
    $ cd pgspa/build
    $ cmake -DDMITIGR_PGSPA_PG_SHAREDIR=/usr/local/pgsql/share ..
    $ make
    $ sudo make install

Installation on Microsoft Windows
---------------------------------

Run Developer Command Prompt for Visual Studio and type:

    > git clone https://github.com/dmitigr/pgspa.git
    > mkdir pgspa\build
    > cd pgspa\build
    > cmake -G "Visual Studio 15 2017 Win64" -DDMITIGR_PGSPA_PG_SHAREDIR="C:\Program Files\PostgreSQL\10\share" ..
    > cmake --build --config Debug .

Next, run the elevated command prompt (i.e. the command prompt with
administrator privileges) and type:

    > cd pgspa\build
    > cmake -DBUILD_TYPE=Debug -P cmake_install.cmake

License
=======

Pgspa is distributed under zlib license. For conditions of distribution and use,
see file `LICENSE.txt`.

Participation
=============

Contributions and any feedback are highly appreciated! Donations are
[welcome][dmitigr_paypal]!

Copyright
=========

Copyright (C) [Dmitry Igrishin][dmitigr_mail]

[dmitigr_mail]: mailto:dmitigr@gmail.com
[dmitigr_paypal]: https://paypal.me/dmitigr
[dmitigr_cefeika]: https://github.com/dmitigr/cefeika.git

[CMake]: https://cmake.org/
[Emacs]: https://www.gnu.org/software/emacs/
[emacs-compilation-mode]: https://www.gnu.org/software/emacs/manual/html_node/emacs/Compilation-Mode.html
[GCC]: https://gcc.gnu.org/
[PostgreSQL]: https://www.postgresql.org/
[Visual_Studio]: https://www.visualstudio.com/
