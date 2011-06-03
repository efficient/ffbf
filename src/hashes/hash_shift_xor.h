#ifndef _HASH_SHIFT_XOR_H_
#define _HASH_SHIFT_XOR_H_

#include "hash_common.h"
#include "hash_buf.h"

template<int LEN> class hash_shift_xor {
private:
    hash_buf<LEN> b;
    uint32_t h;

public:
    inline uint32_t hval() { return h; }
    inline void reset() {
	h = 0;
	b.reset();
    }
    inline void init() {
	reset();
    }
    inline void update(unsigned char c) {
	h = h << 1;
	h ^= c;
	if (b.is_full()) {
	    unsigned char old = b.push(c);
	    uint32_t tmp = old << LEN;
	    h ^= tmp;
	} else {
	    b.push(c);
	}
    }
};


#endif /* _HASH_SHIFT_XOR_H_ */
