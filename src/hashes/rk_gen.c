#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

static const u_int32_t A = 246049789;

int main() {
    int A_i = A;
    int i;
    for (i = 1; i < 64; i++) {
	printf("template<> const u_int32_t hash_rk_static<%d>::A_inverse = %uU;\n",
	       i, A_i);
	A_i *= A;
    }
    exit(0);
}
