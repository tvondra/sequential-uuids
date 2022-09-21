/*-------------------------------------------------------------------------
 *
 * sequential_uuids.c
 *	  generators of sequential UUID values based on sequence/timestamp
 *
 *
 * Currently, this only works on PostgreSQL 10. Adding support for older
 * releases is possible, but it would require solving a couple issues:
 *
 * 1) pg_uuid_t hidden in uuid.c (can be solved by local struct definition)
 *
 * 2) pg_strong_random not available (can fallback to random, probably)
 *
 * 3) functions defined as PARALLEL SAFE, which fails on pre-9.6 releases
 *
 *-------------------------------------------------------------------------
 */
#include <math.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "postgres.h"

#include "catalog/namespace.h"
#include "commands/sequence.h"
#include "utils/uuid.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(uuid_sequence_nextval);
PG_FUNCTION_INFO_V1(uuid_time_nextval);

static Datum
sequential_uuid(int64 val, int32 block_size, int32 count)
{
	pg_uuid_t	*uuid = palloc(sizeof(pg_uuid_t));
	unsigned char   *p;
	int		prefix_bits;
	int		log2_count;
	int		i;
	uint64	tmp;
	unsigned char   *mask = (unsigned char *) &tmp;

	/* generate random bytes (use strong generator) */
	if(!pg_strong_random(uuid->data, UUID_LEN))
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("could not generate random values")));

	/* count the number of bits to keep from the value */
	prefix_bits = log2(count);
	log2_count = pow(2, prefix_bits);

	/* make sure we've reached the count value */
	while (count > log2_count)
	{
		log2_count *= 2;
		prefix_bits++;
	}

	/* determine in which block the value belongs */
	val = (val / block_size) % log2_count;

	/* convert to big endian (easier to copy) */
	val = (val << (64 - prefix_bits));

	/*
	 * Calculate the mask (to zero the random bits before adding the
	 * prefix bytes).
	 */
	tmp = (0xFFFFFFFFFFFFFFFF >> prefix_bits);

	/* easier to deal with big endian byte order */
	val = htobe64(val);
	tmp = htobe64(tmp);

	/*
	 * Copy the prefix in. We already have random data, so use the mask
	 * to zero the bits first.
	 */
	p = (unsigned char *) &val;
	for (i = 0; i < 8; i++)
		uuid->data[i] = (uuid->data[i] & mask[i]) | p[i];

	/*
	 * Set the UUID version flags according to "version 4" (pseudorandom)
	 * UUID, see http://tools.ietf.org/html/rfc4122#section-4.4
	 *
	 * This does reduce the randomness a bit, because it determines the
	 * value of certain bits, but that should be negligible (certainly
	 * compared to the reduction due to prefix).
	 *
	 * UUID v4 is probably the safest choice here. There is v1 which is
	 * time-based, but it includes MAC address (which we don't use) and
	 * works with very special timestamp (starting at 1582 etc.). So we
	 * just use v4 and claim this is pseudorandom.
	 */
	uuid->data[6] = (uuid->data[6] & 0x0f) | 0x40;	/* time_hi_and_version */
	uuid->data[8] = (uuid->data[8] & 0x3f) | 0x80;	/* clock_seq_hi_and_reserved */

	PG_RETURN_UUID_P(uuid);
}

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
 * the default values, the generator wraps around every ~4B UUIDs.
 *
 * You may increase (or rather decrease) the parameters if needed, e.g,
 * by lowering the block size to 256, in wich case the cycle interval
 * is only 16M values.
 */
Datum
uuid_sequence_nextval(PG_FUNCTION_ARGS)
{
	Oid				relid = PG_GETARG_OID(0);
	int32			block_size = PG_GETARG_INT32(1);
	int32			block_count = PG_GETARG_INT32(2);

	/* some basic sanity checks */
	if (block_size < 1)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("block size must be a positive integer")));

	if (block_count < 1)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("number of blocks must be a positive integer")));

	/*
	 * Read the next value from the sequence and get rid of the least
	 * significant bytes. Subtract one, because sequences start at 1.
	 */
	return sequential_uuid(
		/* Create sequential uuid using next value of sequence */
		nextval_internal(relid, true) - 1,
		block_size,
		block_count);
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
	struct timeval	tv;
	int32			interval_length = PG_GETARG_INT32(0);
	int32			interval_count = PG_GETARG_INT32(1);

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

	/* Create sequential uuid using current time in seconds */
	return sequential_uuid(
		tv.tv_sec,
		interval_length,
		interval_count);
}
