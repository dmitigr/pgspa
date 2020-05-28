/* -*- SQL -*-
 * Copyright (C) Dmitry Igrishin
 * For conditions of distribution and use, see files LICENSE.txt
 */

-- Preventing of load directly by psql(1)
\echo Use "create extension dmitigr_spa" to load this file. \quit

--------------------------------------------------------------------------------
-- Views
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
create or replace view spa_role as
  select * from pg_catalog.pg_roles
    where rolname not like E'pg\\_%';
comment on view spa_role is 'The role';
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
create or replace view spa_schema as
  select n.* from pg_catalog.pg_namespace n
    where nspname not like E'pg\\_%' and
          nspname <> 'information_schema';
comment on view spa_schema is 'The scheme';
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
create or replace view spa_table as
  select relnamespace::regnamespace::name as schemaname, cl.*
    from pg_catalog.pg_class cl
    join pg_catalog.pg_namespace nsp on (cl.relnamespace = nsp.oid)
    where nsp.nspname not like E'pg\\_%' and
          nsp.nspname <> 'information_schema' and
          cl.relpersistence = 'p' and
          cl.relkind in ('r', 'p');
comment on view spa_table is 'The table';
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
create or replace view spa_sequence as
  select relnamespace::regnamespace::name as schemaname, cl.*
    from pg_catalog.pg_class cl
    join pg_catalog.pg_namespace nsp on (cl.relnamespace = nsp.oid)
    where nsp.nspname not like E'pg\\_%' and
          nsp.nspname <> 'information_schema' and
          cl.relkind = 'S';
comment on view spa_sequence is 'The sequence';
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
create or replace view spa_index as
  select relnamespace::regnamespace::name as schemaname, cl.*
    from pg_catalog.pg_class cl
    join pg_catalog.pg_namespace nsp on (cl.relnamespace = nsp.oid)
    where nsp.nspname not like E'pg\\_%' and
          nsp.nspname <> 'information_schema' and
          cl.relkind = 'i';
comment on view spa_sequence is 'The index';
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
create or replace view spa_rule as
  select * from pg_catalog.pg_rules
    where schemaname not like E'pg\\_%' and
          schemaname <> 'information_schema';
comment on view spa_rule is 'The rule';
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
create or replace view spa_view as
  select * from pg_catalog.pg_views
    where schemaname not like E'pg\\_%' and
          schemaname <> 'information_schema';
comment on view spa_view is 'The view';
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
create or replace view spa_trigger as
  select n.nspname schemaname, c.relname tablename, t.*
    from pg_catalog.pg_trigger t
    join (pg_catalog.pg_class c join pg_catalog.pg_namespace n
            on (c.relnamespace = n.oid)) on (t.tgrelid = c.oid)
    where not t.tgisinternal and
          n.nspname not like E'pg\\_%' and
          n.nspname <> 'information_schema';
comment on view spa_trigger is 'The trigger';
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
create or replace view spa_function as
  select n.nspname schemaname, p.*
    from pg_catalog.pg_proc p
    join pg_catalog.pg_namespace n on (p.pronamespace = n.oid)
    where n.nspname not like E'pg\\_%' and
          n.nspname <> 'information_schema';
comment on view spa_function is 'The function';
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
create or replace view spa_domain_constraint as
  select dn.nspname domain_schemaname, t.typname domain_name,
         n.nspname schemaname, c.*
    from pg_catalog.pg_constraint c
    join pg_catalog.pg_namespace n on (c.connamespace = n.oid)
    join (pg_catalog.pg_type t join pg_catalog.pg_namespace dn
            on (t.typnamespace = dn.oid)) on (c.contypid = t.oid)
    where dn.nspname not like E'pg\\_%' and
          dn.nspname <> 'information_schema';
comment on view spa_domain_constraint is 'The domain constraint';
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
create or replace view spa_type as
  select n.nspname schemaname, t.*
    from pg_catalog.pg_type t
    join pg_catalog.pg_namespace n on (t.typnamespace = n.oid)
    where n.nspname not like E'pg\\_%' and
          n.nspname <> 'information_schema';
comment on view spa_type is 'The type';
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- Functions for working with roles
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
create or replace function spa_groups(role_ name, rolname out name)
  returns setof name
  returns null on null input
  language sql
as $function$
  select r.rolname
    from pg_catalog.pg_auth_members m
    join pg_catalog.pg_roles r on r.oid = m.roleid
    where m.member = role_::regrole::oid;
$function$;
comment on function spa_groups(name, out name) is
  'Set of groups in which the role has membership';
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- Functions for clearing schemas
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
create or replace function spa_clear_schema(schema_ name,
    object_types_ text[] default array['rules', 'triggers', 'functions',
      'sequences', 'views', 'domains', 'domain_constraints', 'types', 'tables'],
    verbose_ boolean default true,
    out deleted_count integer,
    out remains_count integer)
  returns null on null input
  language plpgsql
as $function$
/*
 * Removes the following database objects: rules, triggers, functions, sequences,
 * views, tables, indexes, domains and their constraints, composite types and enums iterational
 * (non-cascading).
 *
 * Returns: the count of removed and remaining objects.
 */
declare
  object_count_ integer;
  object_count_before_drops_ integer;
  object_ record;

  with_rules_ constant boolean := array_position(object_types_, 'rules') is not null;
  with_triggers_ constant boolean := array_position(object_types_, 'triggers') is not null;
  with_functions_ constant boolean := array_position(object_types_, 'functions') is not null;
  with_sequences_ constant boolean := array_position(object_types_, 'sequences') is not null;
  with_views_ constant boolean := array_position(object_types_, 'views') is not null;
  with_domains_ constant boolean := array_position(object_types_, 'domains') is not null;
  with_domain_constraints_ constant boolean := array_position(object_types_, 'domain_constraints') is not null;
  with_types_ constant boolean := array_position(object_types_, 'types') is not null;
  with_tables_ constant boolean := array_position(object_types_, 'tables') is not null;
  with_indexes_ constant boolean := array_position(object_types_, 'indexes') is not null;

  /*
   * @param $1 - the schema name
   * @param $2 - the object types
   */
  count_query_ constant text :=
  $$
  select
    (select count(*) from @extschema@.spa_rule where schemaname = $1 and
      array_position($2, 'rules') is not null) +
    (select count(*) from @extschema@.spa_trigger where schemaname = $1 and
      array_position($2, 'triggers') is not null) +
    (select count(*) from @extschema@.spa_function where schemaname = $1 and
      array_position($2, 'functions') is not null) +
    (select count(*) from @extschema@.spa_sequence where schemaname = $1 and
      array_position($2, 'sequences') is not null) +
    (select count(*) from @extschema@.spa_view where schemaname = $1 and
      array_position($2, 'views') is not null) +
    (select count(*) from @extschema@.spa_table where schemaname = $1 and
      array_position($2, 'tables') is not null) +
    (select count(*) from @extschema@.spa_index where schemaname = $1 and
      array_position($2, 'indexes') is not null) +
    (select count(*) from @extschema@.spa_domain_constraint where domain_schemaname = $1 and
      array_position($2, 'domain_constraints') is not null) +
    (select count(*) from @extschema@.spa_type where typtype = 'd' and schemaname = $1 and
      array_position($2, 'domains') is not null) +
    (select count(*) from @extschema@.spa_type t left join pg_catalog.pg_class c on (t.typrelid = c.oid)
      where typtype in ('b', 'c', 'e', 'r') and
      typcategory <> 'A' and (relkind = 'c' or relkind is null) and schemaname = $1 and
      array_position($2, 'types') is not null)
  $$;

  rules_query_ constant text :=
  $$
  select quote_ident(rulename) nm,
         quote_ident(schemaname)||'.'||quote_ident(tablename) tab
    from @extschema@.spa_rule where schemaname = $1 and
    array_position($2, 'rules') is not null
  $$;

  triggers_query_ constant text :=
  $$
  select quote_ident(tgname) nm, tgrelid::regclass::text tab
    from @extschema@.spa_trigger where schemaname = $1 and
    array_position($2, 'triggers') is not null
  $$;

  functions_query_ constant text :=
  $$
  select quote_ident(schemaname)||'.'||quote_ident(proname) nm,
         pg_get_function_identity_arguments(oid) args
    from @extschema@.spa_function where schemaname = $1 and
    array_position($2, 'functions') is not null
  $$;

  views_query_ constant text :=
  $$
  select quote_ident(schemaname)||'.'||quote_ident(viewname) nm
    from @extschema@.spa_view where schemaname = $1 and
    array_position($2, 'views') is not null
  $$;

  sequences_query_ constant text :=
  $$
  select quote_ident(schemaname)||'.'||quote_ident(relname) nm
    from @extschema@.spa_sequence where schemaname = $1 and
    array_position($2, 'sequences') is not null
  $$;

  tables_query_ constant text :=
  $$
  select quote_ident(schemaname)||'.'||quote_ident(relname) nm
    from @extschema@.spa_table where schemaname = $1 and
    array_position($2, 'tables') is not null
  $$;

  indexes_query_ constant text :=
  $$
  select quote_ident(schemaname)||'.'||quote_ident(relname) nm
    from @extschema@.spa_index where schemaname = $1 and
    array_position($2, 'indexes') is not null
  $$;

  domain_constraints_query_ constant text :=
  $$
  select quote_ident(conname) nm,
         quote_ident(domain_schemaname)||'.'||quote_ident(domain_name) dom
    from @extschema@.spa_domain_constraint where domain_schemaname = $1 and
    array_position($2, 'domain_constraints') is not null
  $$;

  domains_query_ constant text :=
  $$
  select quote_ident(schemaname)||'.'||quote_ident(typname) nm
    from @extschema@.spa_type where typtype = 'd' and schemaname = $1 and
    array_position($2, 'domains') is not null
  $$;

  -- Selects only: the base types, composite types, enums and range types.
  types_query_ constant text :=
  $$
  select quote_ident(schemaname)||'.'||quote_ident(typname) nm
    from @extschema@.spa_type t left join pg_catalog.pg_class c on (t.typrelid = c.oid)
    where typtype in ('b', 'c', 'e', 'r') and
      typcategory <> 'A' and (relkind = 'c' or relkind is null) and schemaname = $1 and
      array_position($2, 'types') is not null
  $$;
begin
  perform 1 from @extschema@.spa_schema where nspname = schema_;
  if (not found) then
    raise 'The schema % does not exists', schema_;
  end if;

  /*
   * The rules and triggers is best to remove at first. This is because the
   * dependence of other objects on them is unlikely (if ever possible).
   * On the other hand such objects are always depends on tables, functions etc.
   */
  execute count_query_ into strict object_count_ using schema_, object_types_;
  remains_count := object_count_;
  while (object_count_ > 0) loop
    object_count_before_drops_ := object_count_;

    -- The rules removal
    for object_ in execute rules_query_ using schema_, object_types_ loop
      begin
        execute 'drop rule if exists '||object_.nm||' on '||object_.tab;
        if (verbose_) then
          raise notice 'The rule % of the table % has been removed', object_.nm, object_.tab;
        end if;
      exception
        when invalid_schema_name then null;
        when undefined_table then null;
        when dependent_objects_still_exist then null;
      end;
    end loop;

    -- The triggers removal
    for object_ in execute triggers_query_ using schema_, object_types_ loop
      begin
        execute 'drop trigger if exists '||object_.nm||' on '||object_.tab;
        if (verbose_) then
          raise notice 'The trigger % of the table % has been removed', object_.nm, object_.tab;
        end if;
      exception
        when invalid_schema_name then null;
        when undefined_table then null;
        when dependent_objects_still_exist then null;
      end;
    end loop;

    -- The functions removal
    for object_ in execute functions_query_ using schema_, object_types_ loop
      begin
        execute 'drop function if exists '||object_.nm||'('||object_.args||')';
        if (verbose_) then
          raise notice 'The function %(%) has been removed', object_.nm, object_.args;
        end if;
      exception
        when invalid_schema_name then null;
        when dependent_objects_still_exist then null;
      end;
    end loop;

    -- The views removal
    for object_ in execute views_query_ using schema_, object_types_ loop
      begin
        execute 'drop view if exists '||object_.nm;
        if (verbose_) then
          raise notice 'The view % has been removed', object_.nm;
        end if;
      exception
        when invalid_schema_name then null;
        when dependent_objects_still_exist then null;
      end;
    end loop;

    -- The sequences removal
    for object_ in execute sequences_query_ using schema_, object_types_ loop
      begin
        execute 'drop sequence if exists '||object_.nm;
        if (verbose_) then
          raise notice 'The sequence % has been removed', object_.nm;
        end if;
      exception
        when invalid_schema_name then null;
        when dependent_objects_still_exist then null;
      end;
    end loop;

    -- The tables removal
    for object_ in execute tables_query_ using schema_, object_types_ loop
      begin
        execute 'drop table if exists '||object_.nm;
        if (verbose_) then
          raise notice 'The table % has been removed', object_.nm;
        end if;
      exception
        when invalid_schema_name then null;
        when dependent_objects_still_exist then null;
      end;
    end loop;

    -- The indexes removal
    for object_ in execute indexes_query_ using schema_, object_types_ loop
      begin
        execute 'drop index if exists '||object_.nm;
        if (verbose_) then
          raise notice 'The index % has been removed', object_.nm;
        end if;
      exception
        when invalid_schema_name then null;
        when dependent_objects_still_exist then null;
      end;
    end loop;

    -- The domain constraints removal
    for object_ in execute domain_constraints_query_ using schema_, object_types_ loop
      begin
        execute 'alter domain '||object_.dom||' drop constraint if exists '||object_.nm;
        if (verbose_) then
          raise notice 'The constraint % of the domain % has been removed', object_.nm, object_.dom;
        end if;
      exception
        when invalid_schema_name then null;
        when undefined_object then null;
        when dependent_objects_still_exist then null;
      end;
    end loop;

    -- The domains removal
    for object_ in execute domains_query_ using schema_, object_types_ loop
      begin
        execute 'drop domain if exists '||object_.nm;
        if (verbose_) then
          raise notice 'The domain % has been removed', object_.nm;
        end if;
      exception
        when invalid_schema_name then null;
        when dependent_objects_still_exist then null;
      end;
    end loop;

    -- The types removal
    for object_ in execute types_query_ using schema_, object_types_ loop
      begin
        execute 'drop type if exists '||object_.nm;
        if (verbose_) then
          raise notice 'The type % has been removed', object_.nm;
        end if;
      exception
        when invalid_schema_name then null;
        when dependent_objects_still_exist then null;
      end;
    end loop;

    /*
     * If no objects were removed during the last iteration and the
     * database still have the objects that should have been removed,
     * then it is necessary to display information about such objects.
     */
    execute count_query_ into strict object_count_ using schema_, object_types_;
    if (object_count_ > 0 and object_count_before_drops_ = object_count_) then
      if (verbose_) then
        for object_ in execute rules_query_ using schema_, object_types_ loop
          raise notice 'The rule % of the table % was not removed', object_.nm, object_.tab;
        end loop;

        for object_ in execute triggers_query_ using schema_, object_types_ loop
          raise notice 'The trigger % of the table % was not removed', object_.nm, object_.tab;
        end loop;

        for object_ in execute functions_query_ using schema_, object_types_ loop
          raise notice 'The function %(%) was not removed', object_.nm, object_.args;
        end loop;

        for object_ in execute views_query_ using schema_, object_types_ loop
          raise notice 'The view % was not removed', object_.nm;
        end loop;

        for object_ in execute sequences_query_ using schema_, object_types_ loop
          raise notice 'The sequence % was not removed', object_.nm;
        end loop;

        for object_ in execute tables_query_ using schema_, object_types_ loop
          raise notice 'The table % was not removed', object_.nm;
        end loop;

        for object_ in execute indexes_query_ using schema_, object_types_ loop
          raise notice 'The index % was not removed', object_.nm;
        end loop;

        for object_ in execute domain_constraints_query_ using schema_, object_types_ loop
          raise notice 'The constraint % of the domain % was not removed', object_.nm, object_.dom;
        end loop;

        for object_ in execute domains_query_ using schema_, object_types_ loop
          raise notice 'The domain % was not removed', object_.nm;
        end loop;

        for object_ in execute types_query_ using schema_, object_types_ loop
          raise notice 'The type % was not removed', object_.nm;
        end loop;
      end if;

      exit;
    end if;
  end loop;

  deleted_count := remains_count - object_count_;
  remains_count := object_count_;
end;
$function$;
comment on function spa_clear_schema(name, text[], boolean) is 'Drops the objects of the given schema';
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
create or replace function spa_existing_schemas(schemas_ name[], nspname out name)
  returns setof name
  returns null on null input
  language sql
as $function$
  with s(nm) as (select unnest(schemas_))
    select nspname from @extschema@.spa_schema join s on (nspname = nm);
$function$;
comment on function spa_existing_schemas(name[], out name) is
  'Set of schemas from the given array which are really exists';
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
create or replace function spa_clear_schemas_if_exists(schemas_ name[],
    object_types_ text[] default array['rules', 'triggers', 'functions',
      'sequences', 'views', 'domains', 'domain_constraints', 'types'],
    verbose_ boolean default true,
    out deleted_count integer,
    out remains_count integer)
  returns setof record
  returns null on null input
  language sql
as $function$
  select coalesce(dc, 0), coalesce(rc, 0) from
    (select sum(deleted_count)::integer as dc, sum(remains_count)::integer as rc from
      (select (@extschema@.spa_clear_schema(nspname, object_types_, verbose_ := verbose_)).* from
        @extschema@.spa_existing_schemas(schemas_)) as foo) as bar;
$function$;
comment on function spa_clear_schemas_if_exists(name[], text[], boolean) is
  'Removes the given logic from the given schemas';
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- Utilities
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
create or replace function spa_terminate_backends(database_ text default current_database())
  returns integer
  returns null on null input
  language sql
as $function$
  /*
   * Terminates all of the backends thats servicing clients of the specified database.
   *
   * Returns: the count of the terminated backends.
   *
   * Remarks: the current backend will not be affected.
   */
  select count(x)::integer from
    (select pg_terminate_backend(pid) x from
      pg_catalog.pg_stat_activity where
      datname = database_ and
      pid <> pg_backend_pid()) foo
    where x;
$function$;
comment on function spa_terminate_backends(text) is
  'Terminates all of the backends servicing the clients of the specified database';
--------------------------------------------------------------------------------
