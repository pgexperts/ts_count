MODULES = ts_count
DATA_built = ts_count.sql
DATA = uninstall_ts_count.sql
REGRESS = ts_count

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)



