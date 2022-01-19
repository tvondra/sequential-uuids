# sequeantial_uuids/Makefile
#
# Copyright (c) 2018-2022 Tomas Vondra <tomas@pgaddict.com>
#

MODULE_big = sequential_uuids

OBJS = sequential_uuids.o

EXTENSION = sequential_uuids
DATA = sequential_uuids--1.0.1.sql sequential_uuids--1.0.1--1.0.2.sql

TESTS        = $(wildcard test/sql/*.sql)
REGRESS      = $(patsubst test/sql/%.sql,%,$(TESTS))
REGRESS_OPTS = --inputdir=test

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
