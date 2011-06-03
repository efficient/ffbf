#define _DARWIN_FEATURE_64_BIT_INODE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>

int getfile(char *infile, size_t *filesize) {
    int f;
    if ((f = open(infile, O_RDONLY)) < 0) {
      perror(infile);
      exit(-1);
    }
    
    //printf("FD %d\n", f);

    struct stat sb;
    if (fstat(f, &sb) < 0) {
      perror("fstat");
      exit(-1);
    }
#if 0
    printf("%s has %lu bytes\n", infile, (unsigned long)sb.st_size);
    printf("Blocks %d\n", sb.st_blocks);
    printf("Blocksize: %d\n", sb.st_blksize);
    printf("Flags: %x\n", sb.st_flags);
    printf("Inode: %lu\n", sb.st_ino);
    printf("Mode: %o\n", sb.st_mode);
#endif
    *filesize = sb.st_size;
    return f;
}
