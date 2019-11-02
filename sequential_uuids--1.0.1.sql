/* sequential_uuids.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION sequential_uuids" to load this file. \quit

CREATE FUNCTION uuid_sequence_nextval(regclass, block_size int default 65536, block_count int default 65536) RETURNS uuid
AS 'MODULE_PATHNAME', 'uuid_sequence_nextval'
LANGUAGE C STRICT PARALLEL SAFE;

CREATE FUNCTION uuid_time_nextval(interval_length int default 60, interval_count int default 65536) RETURNS uuid
AS 'MODULE_PATHNAME', 'uuid_time_nextval'
LANGUAGE C STRICT PARALLEL SAFE;
