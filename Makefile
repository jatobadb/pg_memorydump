MODULE_big = pg_contextdump
OBJS = pg_contextdump.o

EXTENSION = pg_contextdump
DATA = pg_contextdump--1.0.sql pg_contextdump--unpackaged--1.0.sql

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/pg_contextdump
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif