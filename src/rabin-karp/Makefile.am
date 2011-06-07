# Feed-forward Bloom filter parameters:
#
# The size of the non-cache-resident portion of the filter, in bits
# (must be a power of 2) -- the default value is 32 MB
EXT_SIZE = 0x10000000

# The size of the cache-resident portion of the filter, in bits
# (must be a power of 2, usually half of the largest CPU cache) -- the default value is 2 MB
CACHED_SIZE = 0x1000000

# The total number of hashes (cache + non-cache)
TOTAL_HASHES = 5

# The number of hashes that hash in the cache-resident portion
CACHE_HASHES = 2

# The path to a file in a HUGETLBFS file system.
HUGEPAGE_FILE = '"./mnt/hugepagefile"'

# The minimum length of the patterns.
PATTERN_LEN = 19


noinst_PROGRAMS = ffbf
ffbf_SOURCES = ffbf.cpp
ffbf_CPPFLAGS = -Wall -g -O3 -march=core2 -I$(top_srcdir)/src/hashes \
				-DDEF_EXT_SIZE=$(EXT_SIZE)\
			   	-DDEF_CACHE_SIZE=$(CACHED_SIZE)\
				-DDEF_K=$(TOTAL_HASHES)\
			   	-DDEF_S=$(CACHE_HASHES)\
			   	-DHUGEPAGE_FILE_NAME=$(HUGEPAGE_FILE)\
				-DDEF_PATT_LEN=$(PATTERN_LEN)
MAINTAINERCLEANFILES = Makefile.in
