# sequeantial_uuids/Makefile
#
# Copyright (c) 2018-2022 Tomas Vondra <tomas@pgaddict.com>
#

MODULE_big = sequential_uuids

OBJS = sequential_uuids.o

EXTENSION = sequential_uuids
DATA = sequential_uuids--1.0.1.sql

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

ifndef MAJORVERSION
    MAJORVERSION := $(basename $(VERSION))
endif
