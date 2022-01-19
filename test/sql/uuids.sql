CREATE EXTENSION sequential_uuids;

-- uuid_sequence_nextval

CREATE SEQUENCE s;

-- invalid block size
SELECT uuid_sequence_nextval('s'::regclass, 0, 1);
SELECT uuid_sequence_nextval('s'::regclass, -1, 1);

-- invalid block count
SELECT uuid_sequence_nextval('s'::regclass, 1, 0);
SELECT uuid_sequence_nextval('s'::regclass, 1, -1);

CREATE TABLE uuid_tmp AS SELECT uuid_sequence_nextval('s'::regclass, 256, 65536)::text AS val FROM generate_series(1,10000) s(i);

-- there should be 10k distinct UUID values (collisions unlikely)
SELECT COUNT(DISTINCT val) FROM uuid_tmp;

-- there should be 40 blocks (each up to 256 values)
SELECT COUNT(DISTINCT substring(val, 1, 4)) FROM uuid_tmp;

-- there should be two blocks that are not exactly 256 values - the first one
-- (because sequences start at 1) and the last one (not fully generated yet)
WITH x AS (SELECT substring(val, 1, 4) AS block, count(*) AS cnt FROM uuid_tmp GROUP BY 1)
SELECT * FROM x WHERE cnt != 256;

DROP SEQUENCE s;
DROP TABLE uuid_tmp;

-- try with a larger block
CREATE SEQUENCE s;

CREATE TABLE uuid_tmp AS SELECT uuid_sequence_nextval('s'::regclass, 65536, 256)::text AS val FROM generate_series(1,200000) s(i);

-- there should be 200k distinct UUID values (collisions unlikely)
SELECT COUNT(DISTINCT val) FROM uuid_tmp;

-- there should be 4 blocks (each up to 65536 values)
SELECT COUNT(DISTINCT substring(val, 1, 2)) FROM uuid_tmp;

-- there should be two blocks that are not exactly 256 values - the first one
-- (because sequences start at 1) and the last one (not fully generated yet)
SELECT substring(val, 1, 2) AS block, count(*) AS cnt FROM uuid_tmp GROUP BY 1;

DROP SEQUENCE s;
DROP TABLE uuid_tmp;


-- try wrapping the block count
CREATE SEQUENCE s;
CREATE TABLE uuid_tmp AS SELECT uuid_sequence_nextval('s'::regclass, 256, 256)::text AS val FROM generate_series(1,67000) s(i);

-- there should be 256 blocks
WITH x AS (SELECT substring(val, 1, 2) AS block, count(*) AS cnt FROM uuid_tmp GROUP BY 1 ORDER BY 1)
SELECT * FROM x WHERE cnt != 256;

DROP SEQUENCE s;
DROP TABLE uuid_tmp;

-- uuid_time_nextval

CREATE TABLE uuid_tmp (val text);

INSERT INTO uuid_tmp SELECT uuid_time_nextval(1, 256);
SELECT pg_sleep(2);
INSERT INTO uuid_tmp SELECT uuid_time_nextval(1, 256);

SELECT COUNT(DISTINCT substring(val, 1, 2)) FROM uuid_tmp;

DROP TABLE uuid_tmp;
