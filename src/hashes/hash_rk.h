#ifndef _HASH_RK_H_
#define _HASH_RK_H_

#include "hash_common.h"
#include "hash_buf.h"

template<int LEN> class hash_rk {
private:
    const static uint32_t A = 246049789;
    uint32_t A_inverse;
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
	A_inverse = 1;
	for (int i = 0; i < LEN; i++) {
	    A_inverse *= A;
	}
    }
    
    inline void update(unsigned char c) {
	uint32_t tmp = 0;
	h *= A;
	h += c;
	if (b.is_full()) {
	    unsigned char old = b.push(c);
	    tmp = old * A_inverse;
	    h -= tmp;
	} else {
	    b.push(c);
	}
    }
};

#endif /* _HASH_RK_H_ */
