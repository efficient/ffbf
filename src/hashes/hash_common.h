#ifndef _HASH_COMMON_H_
#define _HASH_COMMON_H_

#include <stdio.h>
#include <sys/types.h>
#include <stdint.h>

inline uint32_t rol32(uint32_t word, int shift)
{
    return (word << shift) | (word >> (32 - shift));
}

inline uint64_t rol64(uint64_t word, int shift)
{
    return (word << shift) | (word >> (64 - shift));
}

#endif /* _HASH_COMMON_H_ */
