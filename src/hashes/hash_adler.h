#ifndef _HASH_ADLER_H_
#define _HASH_ADLER_H_

/* rsync's rolling hash function */

#include "hash_common.h"
#include "hash_buf.h"

template<int LEN> class hash_adler {
private:
    hash_buf<LEN> b;
    uint32_t h;
    uint16_t ha, hb;

public:
    inline uint32_t hval() { return h; }
    inline void reset() {
	h = ha = hb = 0;
	b.reset();
    }
    inline void init() {
	reset();
    }
    
    inline void update(unsigned char c) {
	unsigned char old = 0;
	if (b.is_full()) {
	    old = b.push(c);
	} else {
	    b.push(c);
	}
	
	ha = ha - old + c;
	hb = hb - LEN*old + ha;
	h = (hb << 16) | ha & 0xffff;
    }
};

#endif /* _HASH_ADLER_H_ */
