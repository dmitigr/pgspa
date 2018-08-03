select dmitigr.spa_clear_schemas_if_exists(
  schemas_ := array['schema1'],
  object_types_ := array['rules', 'triggers', 'functions', 'sequences', 'views', 'domains', 'domain_constraints', 'types'],
  verbose_ := true);
