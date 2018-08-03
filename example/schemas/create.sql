create rule rule1 as on insert to schema1.view1 do nothing;

create view schema1.view1 as select * from schema1.table1;

create trigger trigger1 before insert on schema1.table1 execute procedure schema1.ft1();

create table schema1.table1(id integer default schema1.f());

create function schema1.f() returns integer language sql as $$ select (random() * 1000000)::integer; $$;

create index table1_idx1 on schema1.table1(id);

create function schema1.ft1() returns trigger language plpgsql as $$ begin null; end; $$;

create table schema1.table2(id integer default nextval('schema1.sequence1'), dom1 schema1.domain1, enm1 schema1.enum1);

create sequence schema1.sequence1;

create domain schema1.domain1 as integer constraint domain1_constraint1 check (value > 0);

create type schema1.enum1 as enum ('one', 'two', 'three');

create schema schema1;
