This tool, which only works on Linux, uses your system's locale data to
generate a collator partition for Tangara. This partition is uses to sort
database information (album names, etc.) in a way that makes sense to humans.

Because this script isn't portable, `Generic.LC_COLLATE` is included as a
pregenerated collator partition that should work well for most people's
music libraries.

## Partition format

The collator partition has 3MiB reserved for it in the partition table, which
is enough to hold the compiled collation data for ISO14651 character sorting,
plus a small amount of free space.

The first 8 bytes of the partition are reserved for a partition name; usually
the name of the locale that the collation data was compiled for, or 'Generic'
for ISO14651 data.

Following the 8 byte name, is a complete LC_COLLATE file, as generate by GNU
libc's `localedef` utility. Debian's version 2.37 of this utility is known to
work.

FIXME: We should vendor this version of the tool, so that our format remains
stable across glibc changes.
