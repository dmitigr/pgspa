Introduction to The SQL Programming Assistant for PostgreSQL
============================================================

The SQL Programming Assistant for [PostgreSQL] (*Pgspa*) is a simple client program and (optionally)
a PostgreSQL extension that helps one develop in SQL. **ATTENTION, this software is "alpha" quality,
use at your own risk. Do not use with real production databases!** Any [feedback][mailbox]
(*especially results of testing*) is highly appreciated!

Rationale
=========

Usually, to deploy a database the following objects should be created: roles, database itself, tables and theirs
dependencies (such as sequences or functions that used for default values), types, domains, functions, triggers,
rules, views and others. Finally, privileges on these objects should be granted.

To alter the existing database objects or to add the new ones it's necessary to execute the corresponding SQL queries.
Often it's not so trivial, since the existing objects may depends on each other. `DROP ... CASCADE` commands are
often harmful in such cases because their execution may affects the objects that are *not* a subject of refactoring
at all. For example, if the table *t* using function *f* as a default value for some column, *f* depends on the view
*v*, and one need to add a column to the view *v*, it must be dropped first, but since *f* depends on it, the
`DROP VIEW v CASCADE` command will also affects *f* and *t*! Pgspa tries to help out to cope with this problem
allowing to drop bunch of objects in *non-cascade* mode. Pgspa can also create the database objects from the SQL
files of arbitrary directory hierarchy without worrying about the existence of these objects in the database. Both
of these operations can be performed in the *same* transaction!

Tutorial
========

Pgspa ships with the extension for PostgreSQL that can safely *drop* the following types of database objects:

  - rules;
  - triggers;
  - functions;
  - views;
  - domain constraints;
  - domains;
  - types.

This extension can be created by using `CREATE EXTENSION dmitigr_spa` SQL query.

To start working with Pgspa the root directory of the SQL files must be initialized:

    $ cd /path/to/the/database/project
    $ pgspa init

This command will create the directory `.pgspa` in the current working directory where the command `pgspa init` was called.

The database project directory or any of its subdirectories can contains the following files:

  - the SQL sources - files with ".sql" extension;
  - the shortcuts - files without extension;
  - .pgspa - per-directory configuration file.

The SQL files, directories and shortcuts are *references*. Reference names cannot be empty or starts with the dot (".").

SQL sources
-----------

Each SQL query (except the very last) must ends with the semicolon. At this moment, SQL queries cannot
be parameterized, but there are plans to add this feature.

SQL sources can be grouped in the arbitrary directory hierarchies. They will be executed in lexicographical
order of the file names. Each directory can contain the both file `foo.sql` (so called *heading file*) and
directory `foo/`. In this case, the `pgspa exec foo`will run the queries of `foo.sql` at first, and then
the queries of `foo/`.

The concrete SQL file and/or set of SQL files and directories can be specified as the arguments of the `exec`
command, for example:

  $ pgspa exec foo bar.sql baz

All queries of the specified set will be run in the same transaction. Since Pgspa is using GNU style for
reporting error messages, [Emacs] users can run Pgspa in the [Compilation Mode][emacs-compilation-mode]
to immediately jump to the erroneous queries.

Shortcuts
---------

Shortcuts - are regular files without extension that contains the names of SQL files, directories and/or shortcuts
(i.e. *references*) relative to the directory in which the shortcut is located. Shortcuts are useful to define the
sequence of references to be executed without the need to specify this sequence in the command line. For
example, consider the following simple case:

  schemas/create.sql
          drop.sql

Here obviously, that `create.sql` contains the DDL queries to create the objects of the schemas, and `drop.sql`
contains the DDL queries to drop the objects of the schemas. To *recreate* the objects, the following command
can be used:

  $ pgspa exec schemas/drop schemas/create

The same effect can be achieved by creating the shortcut `recreate` with the following lines:

  drop
  create

Then the shortcut can be used line that:

  $ pgspa exec schemas/recreate

But there are still some gotcha. The queries of the whole "schema" directory can be accidentally executed as simple as:

  $ pgspa exec schemas

It will lead to executing `schema/create.sql` and then `schema/drop.sql`. It is absolutely senselessly and may be even
dangerous, since all of the objects will be eventually dropped. Such incidents can be prevented by using the `explicit`
per-directory configuration parameter (see below).

**Note** Cyclic shortcuts are *not* allowed.

Per-directory configuration
---------------------------

Each subdirectory of the database project directory can contain the configuration file. The format of this file
is very simple:

    parameter1 = value_without_spaces
    parameter2 = 'value with spaces'
    # commented_out_parameter_with_empty_value =

The currently supported parameters are:

  - `explicit` - is a boolean parameter (possible values are "yes"/"no") which prevents to run *all* the SQL
     queries the directory contains. Only SQL queries by the *references* that are specified explicitly allowed to run.
     For example, if the directory `foo` is configured as `explicit`, the only way to run the SQL queries this
     directory contains is to use one of the reference of this directory, like `foo/bar` or `foo/baz.sql`.

Installation
============

Dependencies
------------

- [CMake] build system version 3.10+;
- [Pgfe] library;
- C++17 compiler ([GCC] 8+ or [Microsoft Visual C++][Visual_Studio] 15.7+).

Build time settings
-------------------

Settings that may be specified at build time by using [CMake] variables are:
  1. the type of the build;
  2. installation directories.

Details:

|CMake variable|Possible values|Default on Unix|Default on Windows|
|:-------------|:--------------|:--------------|:-----------------|
|**The type of the build**||||
|CMAKE_BUILD_TYPE|Debug \| Release \| RelWithDebInfo \| MinSizeRel|Debug|Debug|
|**Installation directories**||||
|CMAKE_INSTALL_PREFIX|*an absolute path*|"/usr/local"|"%ProgramFiles%\dmitigr_pgspa"|
|PGSPA_BIN_INSTALL_DIR|*a path relative to CMAKE_INSTALL_PREFIX*|"bin"|*not set*|
|PG_SHAREDIR|*an absolute path*|*not set (can be set manually)*|*not set (can be set manually)*|

Installation in common
----------------------

Pgspa ships with the PostgreSQL extension called `dmitigr_spa`. It should be placed to the directory
where the PostgreSQL extensions resides. (Usually, it something like `/usr/local/pgsql/share/extension`
on Linux and `C:\Program Files\PostgreSQL\10\share\extension` on Microsoft Windows.)

`dmitigr_spa` extension consists of two files:

  - dmitigr_spa--1.0.sql
  - dmitigr_spa.control

If Pgspa is building from sources the location of the "share" directory can be specified with
PG_SHAREDIR variable (see above), like this:

    $ cmake -DPG_SHAREDIR="/usr/local/pgsql/share" /path/to/pgspa/sources

Installation on Linux
---------------------

    $ git clone https://github.com/dmitigr/pgspa.git
    $ mkdir -p pgspa/build
    $ cd pgspa/build
    $ cmake -DBUILD_TYPE=Debug -DPG_SHAREDIR=/usr/local/pgsql/share ..
    $ make
    $ sudo make install

The values of `BUILD_TYPE` or `PG_SHAREDIR` could be replaced.

The `make install` command will place `dmitigr_spa` PostgreSQL extension to the directory specified with `PG_SHAREDIR`.

Installation on Microsoft Windows
---------------------------------

Run the Developer Command Prompt for Visual Studio and type:

    > git clone https://github.com/dmitigr/pgspa.git
    > mkdir pgspa\build
    > cd pgspa\build
    > cmake -G "Visual Studio 15 2017 Win64" -DPG_SHAREDIR="C:\Program Files\PostgreSQL\10\share" ..
    > cmake --build -DBUILD_TYPE=Debug ..

Next, run the Elevated Command Prompt (i.e. the command prompt with administrator privileges) and type:

    > cd pgspa\build
    > cmake -DBUILD_TYPE=Debug -P cmake_install.cmake

If the target architecture is Win32 or ARM, then "Win64" should be replaced by "Win32" or "ARM" accordingly.

The values of the `BUILD_TYPE` and `PG_SHAREDIR` could be replaced.

The "install" command will place `dmitigr_spa` PostgreSQL extension to the directory specified with `PG_SHAREDIR`.

**WARNING** The target architecture must corresponds to the bitness of [Pgfe] to link!

License
=======

Pgspa is distributed under zlib license. For conditions of distribution and use,
see file `LICENSE.txt`.

Contributions, sponsorship, partnership
=======================================

Pgspa has been developed on the own funds. Donations are welcome!

If you are using Pgspa for commercial purposes it is reasonable to donate or
even sponsor the further development of Pgspa.

To make a donation, via [PayPal] please go [here][paypal1] or [here][paypal2].

If you need a commercial support, or you need to develop a custom client-side or
server-side software based on [PostgreSQL], please contact us by sending email to <dmitigr@gmail.com>.

Pgspa is a free software. Enjoy using it!

Feedback
========

Any feedback are welcome. Contact us by sending email to <dmitigr@gmail.com>.

Copyright
=========

Copyright (C) Dmitry Igrishin

[Pgfe]: https://github.com/dmitigr/pgfe.git
[mailbox]: mailto:dmitigr@gmail.com

[CMake]: https://cmake.org/
[Emacs]: https://www.gnu.org/software/emacs/
[emacs-compilation-mode]: https://www.gnu.org/software/emacs/manual/html_node/emacs/Compilation-Mode.html
[GCC]: https://gcc.gnu.org/
[PayPal]: https://paypal.com/
[paypal1]: https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=38TY2K8KYKYJC&lc=US&item_name=Pgfe%20library&item_number=1&currency_code=USD&bn=PP%2dDonationsBF%3abtn_donateCC_LG%2egif%3aNonHosted
[paypal2]: https://paypal.me/dmitigr
[PostgreSQL]: https://www.postgresql.org/
[Visual_Studio]: https://www.visualstudio.com/
