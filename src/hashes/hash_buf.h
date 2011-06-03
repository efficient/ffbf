#ifndef _HASH_BUF_H_
#define _HASH_BUF_H_

template<int LEN> class hash_buf {
private:
    int bufptr;
    bool full;
    unsigned char backbuf[LEN];
    
public:
    hash_buf() : bufptr(0), full(false) { }
    inline void reset() {
	bufptr = 0;
	full = 0;
    }
    inline unsigned char push(unsigned char n) {
	unsigned char tmp = backbuf[bufptr];
	backbuf[bufptr] = n;
	if (++bufptr >= LEN) {
	    full = true;
	    bufptr = 0;
	}
	return tmp;
    }
    inline bool is_full() { return full; }
};

#endif /* _HASH_BUF_H_ */
