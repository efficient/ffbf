/*
 * Rabin polynomial fingerprinting optimized with precomputed per-byte
 * lookup tables for speed.  This code is derived from a C
 * implementation of Rabin polynomial fingerprinting written by Jan
 * Harkes.  Translated to C++ and templatized by dga 01/2010.
 *
 *  Copyright (c) 2009 Carnegie Mellon University
 *  All Rights Reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef _HASH_RP_TABLE_H_
#define _HASH_RP_TABLE_H_

#include <stdint.h>

template<int LEN> class hash_rp_table {
private:
    /* 64-bit irreducible polynomial */
    static const uint64_t poly = 0xd16a5bde9d0fd0c5ULL;
    
    unsigned int shift;
    uint64_t fingerprint;
    uint64_t mod_table[256];
    uint64_t pop_table[256];
    hash_buf<LEN> b;
    
    static unsigned int degree(uint64_t p) {
	unsigned int msb;
	for (msb = 0; p; msb++) p >>= 1;
	return msb-1;
    }
    
    /* modulo and multiplication operations in GF(2n)
     * http://en.wikipedia.org/wiki/Finite_field_arithmetic */
    static uint64_t poly_mod(uint64_t x, uint64_t p) {
	unsigned int i, d = degree(p);
	for (i = 63; i >= d; i--)
	    if (x & (1ULL << i))
		x ^= p << (i - d);
	return x;
    }
    
    static uint64_t poly_mult(uint64_t x, uint64_t y, uint64_t p) {
	uint64_t sum = 0;
	while (x) {
	    y = poly_mod(y, p);
	    if (x & 1) sum ^= y;
	    x >>= 1; y <<= 1;
	}
	return sum;
    }
public:
    
    void init() { }
    
    hash_rp_table() {
	unsigned int i, d = degree(poly);
	
	shift = d - 8;
	
	/* calculate which bits are flipped when we wrap around */
	for (i = 0; i < 256; i++) {
	    mod_table[i] = poly_mult(i, 1ULL<<d, poly) ^ ((uint64_t)i<<d);
	    //printf("MT[%d]: %llx\n", i, mod_table[i]);
	}
	
	/* clear state so we can calculate the effect of removing a value */
	reset();
	memset(pop_table, 0, 256 * sizeof(uint64_t));
	
	/* push a 1 followed by LEN-1 0's */
	update(1);
	for (i = 1; i < LEN; i++)
	    update(0);
	
	/* calculate what bits should be flipped when bytes are popped */
	for (i = 0; i < 256; i++)
	    pop_table[i] = poly_mult(i, fingerprint, poly);
	
	reset();
    }
    

    void reset() {
	b.reset();
	fingerprint = 0;
    }
    
    inline void update(unsigned char c) {
	if (b.is_full()) {
	    unsigned char old = b.push(c);
	    fingerprint ^= pop_table[old];
	} else {
	    b.push(c);
	}
	
	/* multiply by 256 and add new value */
	unsigned char hi = fingerprint >> shift;
	fingerprint <<= 8;
	fingerprint |= c;
	
	/* modulo polynomial */
	fingerprint ^= mod_table[hi];
    }
    
    inline uint32_t hval() { return fingerprint; }
};
#endif /* _HASH_RP_TABLE_H_ */
