#ifndef _HASH_NULL_H_
#define _HASH_NULL_H_

#include "hash_common.h"
#include "hash_buf.h"

template<int LEN> class hash_null {
private:

public:
    inline uint32_t hval() { return 0; }
    inline void reset() { }
    inline void init() { }
    inline void update(unsigned char c) { }
};

#endif /* _HASH_NULL_H_ */
