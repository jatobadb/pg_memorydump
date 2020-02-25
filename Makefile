MODULE_big = pg_memorydump
OBJS = pg_memorydump.o

EXTENSION = pg_memorydump
DATA = pg_memorydump--1.0.sql pg_memorydump--unpackaged--1.0.sql

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/pg_memorydump
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif