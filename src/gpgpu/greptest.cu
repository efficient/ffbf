/**
Copyright 2011 Iulian Moraru and David G. Andersen

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

This is the implementation of a feed-forward Bloom filter for GPGPU.
*/

#define _DARWIN_FEATURE_64_BIT_INODE 1
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>

#include "cuda.h"
#include "cuda_runtime.h"
#include "math_functions.h"

#include "sbox.h"

using namespace std;

extern "C" {
int getfile(char *infile, size_t *filesize);
#include "timing.h"
}

//#define BLOOMBITS 1048576 /* 1 millllion bits */
#define BLOOMBITS 0x10000000 /* 32 MB */
#define BLOOMMASK (BLOOMBITS - 1)

#define BLOCK_SIZE 256
#define HASH_LEN 19
#define FILE_MAX 6710886400
#define NR_STREAMS 10

char *pinnedBuf;


struct countmap {
    unsigned int hval;
    unsigned int charloc;
};

texture<unsigned char, 1, cudaReadModeElementType> tex_bloom;

bool is_bit_set(int i, unsigned int *bv) {
    unsigned int word = bv[i >> 5];
    unsigned int bitMask = 1 << (i & 31);
    return (word & bitMask);
}

__device__ bool texbf_is_bit_set(int i) {
    unsigned char word = tex1Dfetch(tex_bloom, i/8);
    unsigned int bitMask = 1 << (i % 8);
    return (word & bitMask);
}

__device__ bool device_is_bit_set(int i, unsigned int *bv) {
    unsigned int word = bv[i >> 5];
    unsigned int bitMask = 1 << (i & 31);
    return (word & bitMask);
}

__device__ void device_set_bit(int i, unsigned int *bv) {
    unsigned int bitMask = 1 << (i & 31);
    atomicOr(&bv[i >> 5], bitMask);
}

inline __device__ unsigned int rol32(unsigned int word, int shift)
{
    return (word << shift) | (word >> (32 - shift));
}

__global__ void grepSetup(unsigned char *d_a,
                          unsigned int *d_b,
                          unsigned int starting_offset)
{
    /* SPEED:  Copy into local memory coalescing and then do this
     * all locally. */
    int i = starting_offset + (blockIdx.y * gridDim.x + blockIdx.x) * blockDim.x * blockDim.y + threadIdx.x;

    int char_offset = i * (HASH_LEN + 1); /* Skip \n */

    unsigned int hval = 0, hval2 = 0;

    for (int j = 0; j < HASH_LEN; j++) {
        hval = rol32(hval, 1);
        hval2 = rol32(hval2, 3);
        unsigned int sbv = sbox[d_a[char_offset + j]];
        hval ^= sbv;
        hval2 ^= sbv;
    }
    device_set_bit(hval & BLOOMMASK, d_b);
    device_set_bit(hval2 & BLOOMMASK, d_b);
    unsigned int hval3 = hval + hval2;
    device_set_bit(hval3 & BLOOMMASK, d_b);
    unsigned int hval4 = hval + 5 * hval2;
    device_set_bit(hval4 & BLOOMMASK, d_b);
//    unsigned int hval5 = (hval << 16) | (hval2 >> 16);
//    device_set_bit(hval5 & BLOOMMASK, d_b);
}

__global__ void GrepKernel(unsigned char *d_a,
                           unsigned int *blooms,
                           unsigned int *dev_reverse_bloom,
                           unsigned int *dev_positions_matched,
                           unsigned int char_offset,
                           unsigned int n_chars)
{
    __shared__ unsigned boxed[BLOCK_SIZE + HASH_LEN];

    int i = char_offset + (blockIdx.y * gridDim.x + blockIdx.x) * blockDim.x * blockDim.y + threadIdx.x;

    /* SPeed:  This part takes .06 seconds.  Without boxing and
     * without cleanup, it takes .04.  Without cleanup, .05. */
    /* Step 1:  Bring the base chars in to local memory,
     * sboxing them on the way in.  SPEED:  This is faster or equiv to
     * doing 32 bit reads into a register and then shifting out the
     * chars. */
    /* TIME:  0.03 seconds */
    boxed[threadIdx.x] = sbox[d_a[i]];

    /* Ugly, but let some threads pull in the remainder */
    /* TIME:  0.01 seconds */
    int otid = threadIdx.x;
    if (otid < HASH_LEN) {
        int new_i = blockDim.x + i;
        int new_b = blockDim.x + otid;
        boxed[new_b] = sbox[d_a[new_i]];
    }

    /* TIME:  Almost none.  */
    __syncthreads();

    unsigned int hval = 0, hval2 = 0;

    /* Step 2:  Compute the hash of the next HASH_LEN characters */
    for (int j = 0; j < HASH_LEN; j++) {
        hval = rol32(hval, 1);
        hval2 = rol32(hval2, 3);
        unsigned int sbv = boxed[threadIdx.x+j];
        hval ^= sbv;
        hval2 ^= sbv;
    }


    /* Other idea:  Steal from the blocked bloom filter idea to do two
     * bit lookups in a single bus transaction. */

    /* Attempt X:  Loop over the bit vector, load into local memory,
     * do a subset of the tests.  */


    /* Idea:  Have 4 threads process each character position.
     * And have them only do the bit lookup if hash >> [all but 2 bits]== <index>
     * in some way -- thus forcing locality.  Trading bandwidth, but hey,
     * we've got bandwidth.
     * To really do this right, we might want to optimize the
     * hash computation further so that we don't use too much
     * global bandwidth copying the post-hash results out.
     * XXX - probably not too helpful;  tried using lowest of 4
     * hash functions to improve texture locality, little benefit.  Maybe
     * could combine. */


    /* SPEED: This step takes 0.22 of 0.27 seconds */
    /* searchbig:  0.31 out of 0.37 */
    /* 3 version 1:  Do them into global memory and let threads diverge... */

    /* Unrolling and doing a no-branch, dual fetch is slower. */

    /* Hm.  With more hash functions, might be able to use the sorted
     * hash trick to improve locality at the cost of a bit more
     * computation.  Can we bubble sort 5 hash functions rapidly?  Does
     * that give us a cache advantage with texture memory? */

    unsigned int h1 = hval & BLOOMMASK;
    unsigned int h2 = hval2 & BLOOMMASK;
    unsigned int h3 = (hval + hval2) & BLOOMMASK;
    unsigned int h4 = (hval + 5 * hval2) & BLOOMMASK;
//    unsigned int h5 = ((hval << 16) | (hval2 >> 16)) & BLOOMMASK;

    /* This doesn't help with two hash functions */
    /* Kernel time:  0.38 with, 0.37 without */

    unsigned int w1 = h1 >> 3;
    unsigned char bit1 = 1 << (h1 & 7);
    unsigned int w2 = h2 >> 3;
    unsigned char bit2 = 1 << (h2 & 7);
    unsigned int w3 = h3 >> 3;
    unsigned char bit3 = 1 << (h3 & 7);
    unsigned int w4 = h4 >> 3;
    unsigned char bit4 = 1 << (h4 & 7);
//    unsigned int w5 = h5 >> 3;
//    unsigned char bit5 = 1 << (h5 & 7);



    unsigned char t1 = tex1Dfetch(tex_bloom, w1); /* SPEED:  Slowest part */
    if (t1 & bit1) {
        unsigned char t2 = tex1Dfetch(tex_bloom, w2);
        if (t2 & bit2) {
            unsigned char t3 = tex1Dfetch(tex_bloom, w3);
            if (t3 & bit3) {
                unsigned char t4 = tex1Dfetch(tex_bloom, w4);
                if (t4 & bit4) {
//                unsigned char t5 = tex1Dfetch(tex_bloom, w5);
//                if (t5 & bit5) {
                    unsigned int hh5 = (hval + 7 * hval2) & BLOOMMASK;
                    unsigned int h6 = (hval + 3 * hval2) & BLOOMMASK;
                    unsigned int h7 = ((hval << 1) + hval2) & BLOOMMASK;
                    unsigned int h8 = ((hval << 2) + hval2) & BLOOMMASK;
//                    unsigned int h10 = (hval * 11 + hval2) & BLOOMMASK;
                    device_set_bit(hh5, dev_reverse_bloom);
                    device_set_bit(h6, dev_reverse_bloom);
                    device_set_bit(h7, dev_reverse_bloom);
                    device_set_bit(h8, dev_reverse_bloom);
//                    device_set_bit(h10, dev_reverse_bloom);

                    /* If we hit, annotate in a bit vector */
                    /* SPEED:  If we start doing a lot of matches, do this in local
                     * memory and flush all 64 bytes out to main memory.
                     * Not needed yet. */
                    device_set_bit(i, dev_positions_matched);
                }}
//            }
        }
    }
}

__global__ void filterPatterns(unsigned char *d_a,
                               unsigned int *d_b,
                               unsigned int *dev_patterns_matched,
                               unsigned int starting_offset)
{
  /* SPEED:  Copy into local memory coalescing and then do this
   * all locally. */
    int i = starting_offset + (blockIdx.y * gridDim.x + blockIdx.x) * blockDim.x * blockDim.y + threadIdx.x;

    int char_offset = i * (HASH_LEN + 1); /* Skip \n */

    unsigned int hval = 0, hval2 = 0;

    for (int j = 0; j < HASH_LEN; j++) {
        hval = rol32(hval, 1);
        hval2 = rol32(hval2, 3);
        unsigned int sbv = sbox[d_a[char_offset + j]];
        hval ^= sbv;
        hval2 ^= sbv;
    }
    unsigned int h5 = (hval + 7 * hval2) & BLOOMMASK;
    unsigned int h6 = (hval + 3 * hval2) & BLOOMMASK;
    unsigned int h7 = ((hval << 1) + hval2) & BLOOMMASK;
    unsigned int h8 = ((hval << 2) + hval2) & BLOOMMASK;
//    unsigned int h10 = (hval * 11 + hval2) & BLOOMMASK;

    if (device_is_bit_set(h5, d_b)) {
        if (device_is_bit_set(h6, d_b)) {
            if (device_is_bit_set(h7, d_b)) {
                if (device_is_bit_set(h8, d_b)) {
//                    if (device_is_bit_set(h10, d_b)) {
                        device_set_bit(i, dev_patterns_matched);
//                    }
                }
            }
        }
    }
}

void checkReportCudaStatus(const char *name) {
    cudaError_t err = cudaGetLastError();
    printf("CudaStatus %s: ", name);
    if (err) printf("Error: %s\n", cudaGetErrorString(err));
    else printf("Success\n");
}

void exitOnError(const char *name, cudaError_t err) {
    if (err) {
        if (err) printf("%s Error: %s\n", name, cudaGetErrorString(err));
        exit(-1);
    }
}


size_t filetodevice(char *filename, void **devMemPtr)
{
    size_t filesize;
    int f = getfile(filename, &filesize);
    if (f == -1) {
        perror(filename);
        exit(-1);
    }
    char *buf = (char *)mmap(NULL, filesize, PROT_READ,  MAP_FILE | MAP_SHARED, f, 0);
    if (!buf) {
        perror("filetodevice mmap failed");
        exit(-1);
    }
    filesize = min((unsigned long long)filesize, (unsigned long long)FILE_MAX);
    posix_madvise(buf, filesize, POSIX_MADV_SEQUENTIAL);

    printf("filesize = %lu\n", filesize);

    //exitOnError("cudaMallocHost", cudaMallocHost(&pinnedBuf, filesize));

    //memcpy(pinnedBuf, buf, filesize);

    exitOnError("cudaMalloc",
		cudaMalloc(devMemPtr, filesize + HASH_LEN));

    exitOnError("cudaMemcpy",
		cudaMemcpy(*devMemPtr, buf, filesize, cudaMemcpyHostToDevice));
    munmap(buf, filesize);

    close(f);
    return filesize;
}


void bvDump(char *bloomname, unsigned int *dev_bloom, unsigned int bits) {
    printf("bvDump %s\n", bloomname);
    unsigned int *blooms = (unsigned int *)malloc(bits/8);
    cudaMemcpy(blooms, dev_bloom, bits/8, cudaMemcpyDeviceToHost);
    for (int i = 0; i < bits; i++) {
        if (is_bit_set(i, blooms)) {
            printf("%d\n", i);
        }
    }
    free(blooms);
}

void printpositions(char *filename,
                    unsigned int *bv,
                    unsigned int file_ints)
{
    size_t filesize;
    int f = getfile(filename, &filesize);
    if (f == -1) {
        perror(filename);
        exit(-1);
    }
    char *buf = pinnedBuf;

    filesize = min((unsigned long long)filesize, (unsigned long long)FILE_MAX);

    int prev_end = -1;
    for (int i = 0; i < file_ints; i++) {
        if (bv[i]) {
	        for (int j = ffs(bv[i]) - 1; j < 32; j++) {
	            int offset = i*32 + j;
	            if (is_bit_set(offset, bv)) {
	                /* Find end of previous line */
	                if (offset > prev_end && buf[offset] != '\n') {
                        char *sol = ((char*)memrchr(buf, '\n', offset));
                        int start_line;
                        if (sol) {
                            start_line = sol - buf;
                        } else {
                            start_line = 0;
                        }

	                    int end_line;
                        char *eol = (char*)memchr(buf + offset, '\n', filesize - offset + 1);
                        end_line = eol - buf;
                        j += end_line - offset;
	                    if (buf[start_line] == '\n') start_line++;
	                    fwrite(buf + start_line, 1, end_line - start_line, stdout);
	                    fputc('\n', stdout);
	                    prev_end = end_line;
	                }
	            }
	        }
        }
    }

    close(f);
}

void printpatterns(char **patterns,
                   int *lengths,
                   unsigned int *bv,
                   unsigned int file_ints,
                   char* out_filename)
{
    FILE *out = fopen(out_filename, "w");
    if (!out) {
        perror("Error opening patterns output file");
        exit(-1);
    }

    for (int i = 0; i < file_ints; i++) {
        if (!bv[i]) {
            continue;
        }
        unsigned qw = bv[i];
        int offset = i << 5;
        for (unsigned mask = 1; mask != 0; mask = mask << 1, offset++) {
            if (qw & mask) {
                fwrite(patterns[offset], 1, lengths[offset], out);
                fputc('\n', out);
            }
        }
    }
    fclose(out);
}

int dimPick(dim3 &dimGrid,
            dim3 &dimBlock,
            int numthreads,
            int blocksize)
{
    unsigned int blocks_y = 1;
    unsigned int blocks_x = 1;
    unsigned int threads_1d = numthreads % blocksize;

    if (numthreads > (256 * blocksize)) {
        blocks_y = numthreads / (256 * blocksize);
        blocks_x = 256;
        threads_1d = blocksize;
    } else if (numthreads > blocksize) {
        blocks_x = numthreads / blocksize;
        threads_1d = blocksize;
    }

    unsigned int threads_used = blocks_y * blocks_x * threads_1d;
    numthreads -= threads_used;
    //printf("dimPick %d %d %d\n", blocks_y, blocks_x, threads_1d);
    dimGrid = dim3(blocks_x, blocks_y);
    dimBlock = dim3(threads_1d);
    return threads_used;
}


void setup_bloom_search(int grepsize,
                        unsigned char *dev_greps,
                        unsigned int *dev_bloom)
{
    exitOnError("setup_bloom_search cudaMemSet dev_bloom = 0",
	            cudaMemset(dev_bloom, 0, BLOOMBITS/8));

    int numthreads = grepsize / (HASH_LEN + 1);
    unsigned int char_offset = 0;
    dim3 dimGrid, dimBlock;

    while (numthreads > 0) {
        unsigned int tu = dimPick(dimGrid, dimBlock, numthreads, BLOCK_SIZE);
        printf("Executing grepSetup (%d,%d,%d)\n", dimGrid.x, dimGrid.y, dimBlock.x);
        grepSetup<<<dimGrid, dimBlock>>>(dev_greps, dev_bloom, char_offset);
        checkReportCudaStatus("grepSetup kernel");
        numthreads -= tu;
        char_offset += tu;
    }
}


/*
void executeGrep(int filesize,
                 unsigned char *dev_chars,
                 unsigned int *dev_bloom,
                 unsigned int *dev_reverse_bloom,
                 unsigned int *dev_positions_matched)
{
    int numthreads = filesize;
    unsigned int char_offset = 0;
    dim3 dimGrid, dimBlock;

    exitOnError("executeGrep cudaMemSet dev_reverse_bloom = 0",
	            cudaMemset(dev_reverse_bloom, 0, BLOOMBITS/8));


    printf("Executing grep on %d\n", filesize);
    while (numthreads > 0) {
        unsigned int tu = dimPick(dimGrid, dimBlock, numthreads, BLOCK_SIZE);
        printf("Executing GrepKernel (%d,%d,%d) @ %u\n", dimGrid.x, dimGrid.y, dimBlock.x, char_offset);
        GrepKernel<<<dimGrid, dimBlock>>>(dev_chars, dev_bloom, dev_reverse_bloom,
                                          dev_positions_matched, char_offset, filesize);
        numthreads -= tu;
        char_offset += tu;
    }
}
 */


void executeGrepOverlap(char *filename,
                        unsigned char **devMemPtr,
                        unsigned int *dev_bloom,
                        unsigned int *dev_reverse_bloom,
                        unsigned int *dev_positions_matched)
{
    size_t filesize;
    int f = getfile(filename, &filesize);
    if (f == -1) {
        perror(filename);
        exit(-1);
    }
    filesize = min((unsigned long long)filesize, (unsigned long long)FILE_MAX);

    exitOnError("cudaMallocHost", cudaMallocHost(&pinnedBuf, filesize));

    exitOnError("cudaMalloc",
		cudaMalloc(devMemPtr, filesize + HASH_LEN));
    unsigned char *dev_chars = *devMemPtr;

    int numthreads;
    dim3 dimGrid, dimBlock;

    exitOnError("executeGrep cudaMemSet dev_reverse_bloom = 0",
	            cudaMemset(dev_reverse_bloom, 0, BLOOMBITS/8));

    cudaStream_t streams[NR_STREAMS];

    for (int i = 0; i < NR_STREAMS; i++) {
        exitOnError("cudaStreamCreate",
            cudaStreamCreate(&streams[i]));
    }

    int size = filesize / NR_STREAMS;

    int fd = open(filename, O_RDONLY);

    for (int i = 0; i < NR_STREAMS; i++) {
        unsigned offset = i * size;
        if (i == NR_STREAMS - 1) {
            size = filesize - i * size;
        }
        numthreads = size;

        printf("Executing grep on %d\n", size);

        read(fd, pinnedBuf + offset, size);

        exitOnError("cudaMemcpyAsync",
            cudaMemcpyAsync(dev_chars + offset, pinnedBuf + offset, size, cudaMemcpyHostToDevice, streams[i]));

        unsigned int char_offset = 0;
        while (numthreads > 0) {
            unsigned int tu = dimPick(dimGrid, dimBlock, numthreads, BLOCK_SIZE);
            printf("Executing GrepKernel (%d,%d,%d) @ %u\n", dimGrid.x, dimGrid.y, dimBlock.x, (offset + char_offset));
            GrepKernel<<<dimGrid, dimBlock, 0, streams[i]>>>(dev_chars, dev_bloom, dev_reverse_bloom,
                                                             dev_positions_matched, offset + char_offset, size);
            checkReportCudaStatus("GrepKernel");
            numthreads -= tu;
            char_offset += tu;
        }
    }

    close(f);
    close(fd);

    cudaThreadSynchronize();

    for (int i = 0; i < NR_STREAMS; i++) {
        cudaStreamDestroy(streams[i]);
    }
}

void executePatternFiltering(int grepsize,
                             unsigned char *dev_greps,
                             unsigned int *dev_reverse_bloom,
                             unsigned int *dev_patterns_matched)
{
    int numthreads = grepsize / (HASH_LEN + 1);
    printf("NUMTHREADS = %d\n", numthreads);
    unsigned int line_offset = 0;
    dim3 dimGrid, dimBlock;

    while (numthreads > 0) {
        unsigned int tu = dimPick(dimGrid, dimBlock, numthreads, BLOCK_SIZE);
        printf("Executing pattern filtering (%d,%d,%d)\n", dimGrid.x, dimGrid.y, dimBlock.x);
        filterPatterns<<<dimGrid, dimBlock>>>(dev_greps, dev_reverse_bloom, dev_patterns_matched, line_offset);
        checkReportCudaStatus("filterPatterns kernel");
        numthreads -= tu;
        line_offset += tu;
    }
}

int main(int argc, char **argv)
{
    if (argc != 5)
    {
        cerr << "usage: ./greptest truncatedPatternsFile corpus fullPatternsFile outPatternsFile" << endl;
        return -1;
    }

    char *searchfile = argv[1];
    char *infile = argv[2];
    char *full_patterns_file = argv[3];
    char *out_patterns_file = argv[4];

    unsigned char *dev_chars;
    unsigned char *dev_greps;
    /* Bit vectors */
    unsigned int *dev_bloom, *dev_reverse_bloom, *dev_positions_matched, *dev_patterns_matched;

    timing_stamp("start", false);

    size_t grepsize = filetodevice(searchfile, (void **)&dev_greps);
    exitOnError("cudaMalloc dev_bloom",
		cudaMalloc((void **)&dev_bloom, BLOOMBITS/8));
    exitOnError("cudaMalloc dev_reverse_bloom",
		cudaMalloc((void **)&dev_reverse_bloom, BLOOMBITS/8));

    setup_bloom_search(grepsize, dev_greps, dev_bloom);

    /* Bind the bloom filter to a texture */
    exitOnError("cudaBindTexture tex_bloom to dev_bloom",
		cudaBindTexture(NULL, tex_bloom, dev_bloom, BLOOMBITS/8));

    //bvDump(searchfile, dev_bloom, BLOOMBITS);

    //index patterns by line
    int nr_patterns = grepsize / (HASH_LEN + 1);
    char **patterns = new char*[nr_patterns];
    int *lengths = new int[nr_patterns];
    ifstream in_patterns(full_patterns_file);
    printf("nr_patterns = %d\n", nr_patterns);
    for (int i = 0; i < nr_patterns; i++) {
        char line[1001];
        in_patterns.getline(line, 1000);
        patterns[i] = new char[strlen(line) + 1];
        memcpy(patterns[i], line, strlen(line) + 1);
        lengths[i] = strlen(patterns[i]);
    }
    in_patterns.close();

    timing_stamp("setup complete", false);

    printf("GPUGrep opening %s\n", infile);
    size_t filesize;
    int f = getfile(infile, &filesize);
    if (f == -1) {
        perror(infile);
        exit(-1);
    }

    exitOnError("cudaMalloc dev_positions_matched",
		cudaMalloc((void **)&dev_positions_matched, filesize / 8 + 1));
    exitOnError("cudaMemset dev_positions_matched = 0",
		cudaMemset(dev_positions_matched, 0, filesize/8 + 1));

    timing_stamp("posmatch init", false);

    printf("\nPhase 3:  Executing kernel\n");
    executeGrepOverlap(infile, &dev_chars, dev_bloom, dev_reverse_bloom, dev_positions_matched);

    cudaThreadSynchronize();
    timing_stamp("grep done", false);
    checkReportCudaStatus("Grep Kernel");

    exitOnError("cudaMalloc dev_patterns_matched",
		cudaMalloc((void **)&dev_patterns_matched, nr_patterns / 8));
    exitOnError("cudaMemset dev_patterns_matched = 0",
		cudaMemset(dev_patterns_matched, 0, nr_patterns / 8));


    executePatternFiltering(grepsize, dev_greps, dev_reverse_bloom, dev_patterns_matched);
    cudaThreadSynchronize();
    cudaFree(dev_greps);

    timing_stamp("patterns filtering done", false);

    /* Idea:
     * Record array of bit positions to check + chars;
     * Sort that array.
     * Divvy up the array to threads.  Compute min, max of the bit vector
     * address space accessed by that array, and pull that part of the BV array
     * (as much as fits?) into local shared memory.  Check in parallel, and
     * issue atomic increments to the set bit positions into a global count
     * array (presumably somewhat rare???).
     * If that takes too long, then output the maps of counts and
     * char offsets, sort that, merge, and then do the bit sets. */
    /* But :  radixSort only gets 20 MElements/sec;  very possibly
     * slower than what we're already doing. */

#if 1
    printf("\nPhase 4:  Copying results to host memory.\n");

    unsigned int *host_positions_matched = (unsigned int *)malloc(filesize / 8);
    unsigned int *host_patterns_matched = (unsigned int *)malloc(grepsize);

    exitOnError("cudaMemcpy corpus results to host",
		cudaMemcpy(host_positions_matched, dev_positions_matched,
			   filesize / 8, cudaMemcpyDeviceToHost));
    exitOnError("cudaMemcpy pattern results to host",
		cudaMemcpy(host_patterns_matched, dev_patterns_matched,
			   grepsize / (HASH_LEN + 1) / 8, cudaMemcpyDeviceToHost));

    timing_stamp("copyout done", false);
#if 1
    printpositions(infile, host_positions_matched, filesize / 32);
    printpatterns(patterns, lengths, host_patterns_matched, grepsize / (HASH_LEN + 1) / 32, out_patterns_file);
    timing_stamp("printout done", false);
#endif
    printf("\n");
    free(host_positions_matched);
    free(host_patterns_matched);
#endif
    cudaFree(dev_bloom);
    cudaFree(dev_reverse_bloom);
    cudaFree(dev_chars);
    cudaFree(dev_positions_matched);
    timing_stamp("cleanup done", true);
    timing_report();

    struct cudaDeviceProp cdp;
    cudaGetDeviceProperties(&cdp, 0);
    printf("\ndeviceOverlap = %d\n", cdp.deviceOverlap);
}

