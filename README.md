Sequential UUID generators
==========================

This PostgreSQL extension implements two UUID generators with sequential
patterns, which helps to reduce random I/O patterns associated with
regular entirely-random UUID.

Regular random UUIDs are distributed uniformly over the whole range of
possible values.  This results in poor locality when inserting data into
indexes - all index leaf pages are equally likely to be hit, forcing
the whole index into memory.  With small indexes that's fine, but once
the index size exceeds shared buffers (or RAM), the cache hit ratio
quickly deteriorates.

Compare this to sequences and timestamps, which have a more sequential
pattern and the new data almost always end up in the right-most part of
the index (new sequence value is larger than all preceding values, same
for timestamp).  This results in a nicer and cache-friendlier behavior,
but the values are predictable and may easily collide cross machines.

The main goal of the two generators implemented by this extension, is
generating UUIDS in a more sequential pattern, but without reducing the
randomness too much (which could increase the probability of collision
and predictability of the generated UUIDs).  This idea is not new, and
is described as 

This idea is pretty much what the UUID wikipedia article [1] calls COMB
(combined-time GUID) and is more more thoroughly explained in [2].


Generators
----------

The extension provides two functions generating sequential UUIDs using
either a sequence or timestamp.

* `uuid_sequence_nextval(sequence regclass, block_size int default 65536, block_count int default 65536)`

* `uuid_time_nextval(interval_length int default 60, interval_count int default 65536) RETURNS uuid`

The default values for parameters are selected to work well for a range
of workloads.  See the next section explaining the design for additional
information about the meaning of those parameters.


Design
------

The easiest way to make UUIDs more sequential is to use some sequential
value as a prefix. For example, we might take a sequence or a timestamp
and add random data until we have 16B in total.  The resulting values
would be almost perfectly sequential, but there are two issues with it:

* reduction of randomness - E.g. with a sequence producing bigint values
  this would reduce the randomness from 16B to 8B.  Timestamps do reduce
  the randomness in a similar way, depending on the accuracy.  This
  increases both the collision probability and predictability (e.g. it
  allows determining which UUIDs were generated close to each other, and
  perhaps the exact timestamp).

* bloat - If the values only grow, this may result in bloat in indexes
  after deleting historical data.  This is a well-known issue e.g. with
  indexes on timestamps in log tables.

To address both of these issues, the implemented generators are designed
to wrap-around regularly, either after generating certain number of UUIDs
or some amount of time.  In both cases, the UUIDs are generates in blocks
and have the form of

    (block ID; random data)

The size of the block ID depends on the number of blocks and is fixed
(depends on generator parameters).  For example with the default 64k
blocks we need 2 bytes to store it.  The block ID increments regularly,
and eventually wraps around.

For sequence-based generators the block size is determined by number of
UUIDs generated.  For example we may use blocks of 256 values, in which
case the two-byte block ID may be computed like this:

    (nextval('s') / 256) % 65536

So the generator wraps-around every ~16M UUIDs (because 256 * 65536).

For timestamp-based generators, the block size is defined as interval
length, with the default value 60 seconds.  As the default number of
blocks is 64k (same as for sequence-based generators), the bloc may be
computed like this

    (timestamp / 60) % 65536

Which means the generator wraps around every ~45 days.


Supported Releases
------------------

Currently, this extension works only on releases since PostgreSQL 10. It
can be made working on older releases with some minor code tweaks if
someone wants to spend a bit of time on that.


[1] https://en.wikipedia.org/wiki/Universally_unique_identifier

[2] http://www.informit.com/articles/article.aspx?p=25862
