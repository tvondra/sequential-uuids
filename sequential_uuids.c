/*-------------------------------------------------------------------------
 *
 * sequential_uuids.c
 *	  generators of sequential UUID values based on sequence/timestamp
 *
 *-------------------------------------------------------------------------
 */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "postgres.h"

#include "catalog/namespace.h"
#include "commands/sequence.h"
#include "utils/uuid.h"
#include "utils/varlena.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(uuid_sequence_nextval);
PG_FUNCTION_INFO_V1(uuid_time_nextval);

/*
 * uuid_sequence_nextval
 *	generate sequential UUID using a sequence
 *
 * The sequence-based sequential UUID generator define the group size
 * and group count based on number of UUIDs generated.
 *
 * The block_size (65546 by default) determines the number of UUIDs with
 * the same prefix, and block_count (65536 by default) determines the
 * number of blocks before wrapping around to 0. This means that with
 * the default values, the generator wraps around every ~2B UUIDs.
 *
 * You may increase (or rather decrease) the parameters if needed, e.g,
 * by lowering the block size to 256, in wich case the cycle interval
 * is only 16M values.
 */
Datum
uuid_sequence_nextval(PG_FUNCTION_ARGS)
{
	int				i;
	int64			val;
	Oid				relid = PG_GETARG_OID(0);
	int32			block_size = PG_GETARG_INT32(1);
	int32			block_count = PG_GETARG_INT32(2);
	int64			prefix_bytes;
	pg_uuid_t	   *uuid;
	unsigned char  *p;

	/* some basic sanity checks */
	if (block_size < 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("block size must be a positive integer")));

	if (block_count < 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("number of blocks must be a positive integer")));

	/* count the number of bytes to keep from the sequence value */
	prefix_bytes = 0;
	while (block_count > 1)
	{
		block_count /= 256;
		prefix_bytes++;
	}

	/*
	 * Read the next value from the sequence and get rid of the least
	 * significant bytes.
	 */
	val = nextval_internal(relid, true);
	val /= block_size;

	p = (unsigned char *) &val;

	uuid = palloc(sizeof(pg_uuid_t));

	/* copy the desired number of (least significant) bytes as prefix */
	for (i = 0; i < prefix_bytes; i++)
		uuid->data[i] = p[prefix_bytes - 1 - i];

	/* generate the remaining bytes as random (use strong generator) */
	if(!pg_strong_random(uuid->data + prefix_bytes, UUID_LEN - prefix_bytes))
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("could not generate random values")));

	PG_RETURN_UUID_P(uuid);
}

/*
 * uuid_time_nextval
 *	generate sequential UUID using current time
 *
 * The timestamp-based sequential UUID generator define the group size
 * and group count based on data extracted from current timestamp.
 *
 * The interval_length (60 seconds by default) is defined as number of
 * seconds where UUIDs share the same prefix). The prefix length is
 * determined by the number of intervals (65536 by default, i.e. 2B).
 * With these parameters the generator wraps around every ~45 days.
 */
Datum
uuid_time_nextval(PG_FUNCTION_ARGS)
{
	int				i;
	struct timeval	tv;
	int64			val;
	pg_uuid_t	   *uuid;
	int32			interval_length = PG_GETARG_INT32(0);
	int32			interval_count = PG_GETARG_INT32(1);
	int64			prefix_bytes;
	unsigned char  *p;

	/* some basic sanity checks */
	if (interval_length < 1)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("length of interval must be a positive integer")));

	if (interval_count < 1)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("number of intervals must be a positive integer")));

	if (gettimeofday(&tv, NULL) != 0)
		elog(ERROR, "gettimeofday call failed");

	val = (tv.tv_sec / interval_length);

	/* count the number of bytes to keep from the timestamp */
	prefix_bytes = 0;
	while (interval_count > 1)
	{
		interval_count /= 256;
		prefix_bytes++;
	}

	p = (unsigned char *) &val;

	uuid = palloc(sizeof(pg_uuid_t));

	/* copy the desired number of (least significant) bytes as prefix */
	for (i = 0; i < prefix_bytes; i++)
		uuid->data[i] = p[prefix_bytes - 1 - i];

	/* generate the remaining bytes as random (use strong generator) */
	if(!pg_strong_random(uuid->data + prefix_bytes, UUID_LEN - prefix_bytes))
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("could not generate random values")));

	PG_RETURN_UUID_P(uuid);
}
