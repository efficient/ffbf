#ifndef _HASH_RK_STATIC_H_
#define _HASH_RK_STATIC_H_

#include "hash_common.h"
#include "hash_buf.h"

template<int LEN> class hash_rk_static {
private:
    const static uint32_t A = 246049789;
    const static uint32_t A_inverse;
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
	h *= A;
	h += c;
	if (b.is_full()) {
	    unsigned char old = b.push(c);
	    h -= A_inverse * old;
	} else {
	    b.push(c);
	}
    }
};

#include "rk_vals.h"
#endif /* _HASH_RK_STATIC_H_ */
