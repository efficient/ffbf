/*
 * Comparison of rolling hash functions framework
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/param.h>
#include <string>

#include "all_hashes.h"

char **orig_argv = NULL;
int orig_argc = 0;

void
usage()
{
    fprintf(stderr,
	    "hashbench [-fl] <input file>\n"
	    );
}

void
help()
{
    usage();
    fprintf(stderr,
	    "  -l   print each line's final hash (default)\n"
	    "  -f   hash the whole file silently, print time\n"
	    );
}

float
timeval_diff(const struct timeval *start, const struct timeval *end)
{
    float r = (end->tv_sec - start->tv_sec)* 1000000;

    if (end->tv_usec > start->tv_usec)
        r += (end->tv_usec - start->tv_usec);
    else if (end->tv_usec < start->tv_usec)
        r -= (start->tv_usec - end->tv_usec);

    return (float)r/1000000;
}

template <class H, int LEN>
class HashTester {
public:
    static const int hashbufsize = 4096;
    std::string testname;
    
    H h;
    void init(std::string tname) {
	testname = tname;
	h.init();
    }

    void hash_file(const char *infile) {
	struct timeval tv_start, tv_end;
	int nread;
	int f;
	unsigned char buf[hashbufsize];
	uint64_t ntotal = 0;
	if (!strcmp(infile, "-")) {
	    f = STDIN_FILENO;
	} else {
	    f = open(infile, O_RDONLY);
	}
	
	gettimeofday(&tv_start, NULL);

	while ((nread = read(f, buf, hashbufsize)) > 0) {
	    ntotal += nread;
	    for (u_int i = 0; i < nread; i++) {
		h.update(buf[i]);
	    }
	}
	gettimeofday(&tv_end, NULL);

	close(f);
	
	char tfmtbuf[32];
	char curpath[MAXPATHLEN+1];
	getcwd(curpath, MAXPATHLEN);
	ctime_r(&(tv_start.tv_sec), tfmtbuf);
	printf("# hashbench.c %s", tfmtbuf);
	printf("# hashfn: %s\n", testname.c_str());
	printf("# input file: %s\n", infile);
	printf("# command line: ");
	for (int i = 0; i < orig_argc; i++) {
	    printf(" %s", orig_argv[i]);
	}
	printf("\n");
	printf("# user: %s\n", getlogin());
	printf("# path: %s\n", curpath);
	printf("#\n");
	printf("# bytes     execution time (seconds)\n");
	printf("%lld %.3f\n", ntotal, timeval_diff(&tv_start, &tv_end));
    }
    
    void hash_lines(const char *infile) {
	char buf[hashbufsize];
	FILE *f;
	if (!strcmp(infile, "-")) {
	    f = stdin;
	} else {
	    f = fopen(infile, "r");
	}

	while (fgets(buf, hashbufsize-1, f) != NULL) {
	    h.reset();
	    for (u_int i = 0; buf[i] != '\0'; i++) {
		if (buf[i] == '\n') {
		    buf[i] = 0;
		    continue;
		}
		h.update(buf[i]);
	    }
	    printf("%x %s\n", h.hval(), buf);

	}
	fclose(f);
    }
};

typedef enum { hash_lines, hash_file } opmode_t;

int
main(int argc, char **argv)
{
    //extern char *optarg; // not yet used, commented to remove warning
    extern int optind;
    int ch;

    opmode_t mode = hash_lines;
    orig_argv = argv;
    orig_argc = argc;

    while ((ch = getopt(argc, argv, "hfl")) != -1)
	switch (ch) {
	case 'h':
	    help();
	    exit(0);
	case 'l':
	    mode = hash_lines;
	    break;
	case 'f':
	    mode = hash_file;
	    break;
	default:
	    usage();
	    exit(-1);
	}
    argc -= optind;
    argv += optind;
    if (argc < 1) {
	usage();
	exit(-1);
	
    }

#ifndef HASHFN
    #define HASHFN hash_rot_sbox
#endif
#define QUOTEDEF(x) #x
#define TNAME(macro) QUOTEDEF(macro)

    std::string testname(TNAME(HASHFN));
    HashTester<HASHFN<19>, 19> ss;
    ss.init(testname);
    switch(mode) {
    case hash_lines:
	ss.hash_lines(argv[0]);
	break;
    case hash_file:
	ss.hash_file(argv[0]);
	break;
    }

    exit(0);
}
