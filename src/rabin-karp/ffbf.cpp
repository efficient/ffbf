/**
Copyright 2011 Carnegie Mellon University

Authors: Iulian Moraru and David G. Andersen

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

This is the implementation of a feed-forward Bloom filter.
The code has been tested on a Linux 2.6.35 machine, gcc version 4.4.5.
*/

#include <iostream>
#include <stdio.h>
#include <errno.h>
#include <fstream>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <xmmintrin.h>
#include "ffbf.h"


#define DO_TIMING 1
#if DO_TIMING
#define TIME(label, statement) \
    do { \
    struct timeval tvs, tve; \
    gettimeofday(&tvs, NULL); \
    do { statement; } while(0);	\
    gettimeofday(&tve, NULL); \
    double tvsd = (double)tvs.tv_sec + (double)tvs.tv_usec/1000000; \
    double tved = (double)tve.tv_sec + (double)tve.tv_usec/1000000; \
    fprintf(stderr, "%s time: %.5f\n", label, tved-tvsd); \
    } while (0)
#else
#define TIME(label, statement) statement
#endif

using namespace std;

#ifndef DEF_EXT_SIZE
#define DEF_EXT_SIZE 0x10000000
#endif

#ifndef DEF_CACHE_SIZE
#define DEF_CACHE_SIZE 0x1000000
#endif

#ifndef DEF_K
#define DEF_K 5
#endif

#ifndef DEF_S
#define DEF_S 2
#endif

#ifndef HUGEPAGE_FILE_NAME
#define HUGEPAGE_FILE_NAME "./hugepagefile"
#endif

#ifndef DEF_PATT_LEN
#define DEF_PATT_LEN 19
#endif

template <class H, int LEN> const uint32_t Filter<H, LEN>::N = DEF_CACHE_SIZE + DEF_EXT_SIZE;
template <class H, int LEN> const uint32_t Filter<H, LEN>::BloomExtMask = DEF_EXT_SIZE - 1;
template <class H, int LEN> const uint32_t Filter<H, LEN>::BloomCacheMask = DEF_CACHE_SIZE - 1;
template <class H, int LEN> const int Filter<H, LEN>::K = DEF_K;
template <class H, int LEN> const int Filter<H, LEN>::S = DEF_S;

unsigned int contor = 0;

template <class H, int LEN>
Filter<H, LEN>::Filter()
{
    //LARGE PAGE
    fid = open(HUGEPAGE_FILE_NAME, O_CREAT | O_RDWR, 0755);
    if (fid < 0)
    {
        perror("open error: ");
        throw FilterError("Not able to initialize filter");
    }

    mapaddr = mmap(NULL, 2 * N / 8, PROT_READ | PROT_WRITE, MAP_SHARED, fid, 0);

    if (mapaddr == MAP_FAILED)
    {
        perror("map failed: ");
        throw FilterError("Not able to initialize filter");
    }

    bitvector = (uint8_t*)mapaddr;
    reverseBitvector = (uint8_t*)mapaddr + (N / 8);
}

template <class H, int LEN>
Filter<H, LEN>::~Filter()
{
    munmap(mapaddr, 2 * N / 8);
    close(fid);
    unlink(HUGEPAGE_FILE_NAME);
}

template <class H, int LEN>
inline void Filter<H, LEN>::resetHashes()
{
    hash.reset();
}

template <class H, int LEN>
inline void Filter<H, LEN>::updateHashes(u_int8_t nextChar)
{
    if (nextChar & 0x80)
    {
        nextChar = 0;
    }

    hash.update(nextChar);
}

template <class H, int LEN>
inline uint32_t Filter<H, LEN>::getHashValue(int i)
{
    uint32_t x = 0;
    uint32_t h1 = hash.hval1(), h2 = hash.hval2();
    switch (i)
    {
        case 0: x = h1; break;
        case 1: x = h2; break;
        case 2: x = (h1 << 16) | (h1 >> 16); break;
        case 3: x = (h2 << 16) | (h2 >> 16); break;
        case 4: x = h1 + h2; break;
        case 5: x = h1 + (h2 << 1); break;
        case 6: x = h1 + (h2 << 2); break;
        case 7: x = h1 + 3*h2; break;
        case 8: x = h1 + 5*h2; break;
        case 9: x = h1 + 6*h2; break;
        case 10: x = h1 + 7*h2; break;
    }
    if (i < S)
    {
        return x & BloomCacheMask;
    }
    return (x & BloomExtMask) + DEF_CACHE_SIZE;
}

template <class H, int LEN>
inline uint32_t Filter<H, LEN>::getHashValue_2(int i)
{
    uint32_t x = 0;
    uint32_t h1 = hash.hval1(), h2 = hash.hval2();
    switch (i)
    {
        case 0: x = h1 + (h2 << 1); break;
        case 1: x = h1 + (h2 << 2); break;
        case 2: x = h1 + 3*h2; break;
        case 3: x = h1 + 5*h2; break;
        case 4: x = h1 + 6*h2; break;
        case 5: x = h1 + 7*h2; break;
        case 6: x = h1; break;
        case 7: x = h2; break;
        case 8: x = (h1 << 16) | (h1 >> 16); break;
        case 9: x = (h2 << 16) | (h2 >> 16); break;
        case 10: x = h1 + h2; break;
    }
    if (i < S)
    {
        return x & BloomCacheMask;
    }
    return (x & BloomExtMask) + DEF_CACHE_SIZE;
}

template <class H, int LEN>
inline void Filter<H, LEN>::setBit(uint32_t index, uint8_t* vector)
{
    uint32_t byteIndex = index >> 3;
    uint8_t bitMask = 1 << (index & 0x00000007);
    vector[byteIndex] |= bitMask;
}

template <class H, int LEN>
inline void Filter<H, LEN>::setBitsInFilter(uint8_t *vector)
{
    for (int i = 0; i < K; i++)
    {
        setBit(getHashValue(i), vector);
    }
}

template <class H, int LEN>
inline void Filter<H, LEN>::setBitsInFilter_2(uint8_t *vector)
{
    for (int i = 0; i < K; i++)
    {
        setBit(getHashValue_2(i), vector);
    }
}


template <class H, int LEN>
inline bool Filter<H, LEN>::checkInFilter(const uint8_t* vector)
{
    for (int i = 0; i < S; i++)
    {
        uint32_t x = getHashValue(i);
        uint32_t byteIndex = x >> 3;
        uint8_t bitMask = 1 << (x & 0x00000007);
        if (!(vector[byteIndex] & bitMask))
        {
            return false;
        }
    }
    for (int i = S; i < K; i++)
    {
        uint32_t x = getHashValue(i);
        uint32_t byteIndex = x >> 3;
        uint8_t bitMask = 1 << (x & 0x00000007);
        _mm_prefetch(&vector[byteIndex], _MM_HINT_NTA);
        if (!(vector[byteIndex] & bitMask))
        {
            return false;
        }
    }
    return true;
}

template <class H, int LEN>
inline bool Filter<H, LEN>::checkInFilter_2(const uint8_t* vector)
{
    for (int i = 0; i < S; i++)
    {
        uint32_t x = getHashValue_2(i);
        uint32_t byteIndex = x >> 3;
        uint8_t bitMask = 1 << (x & 0x00000007);
        if (!(vector[byteIndex] & bitMask))
        {
            return false;
        }
    }
    for (int i = S; i < K; i++)
    {
        uint32_t x = getHashValue_2(i);
        uint32_t byteIndex = x >> 3;
        uint8_t bitMask = 1 << (x & 0x00000007);
        _mm_prefetch(&vector[byteIndex], _MM_HINT_NTA);
        if (!(vector[byteIndex] & bitMask))
        {
            return false;
        }
    }
    return true;
}

template <class H, int LEN>
Filter<H, LEN>& Filter<H, LEN>::operator<<(string& pattern) throw(FilterError)
{
    if (pattern.length() < LEN)
    {
        return *this;
        //throw PatternError("Search phrase too short");
    }
    resetHashes();
    for (unsigned i = 0; i < LEN; i++)
    {
        updateHashes(pattern[i]);
    }
    setBitsInFilter(bitvector);
    return *this;
}

template <class H, int LEN>
bool Filter<H, LEN>::operator[](const string& line) throw(FilterError)
{
    if (line.length() < LEN)
    {
        return false;
        //throw FilterError("Input line too short");
    }

    resetHashes();
    for (string::const_iterator it = line.begin(); it != line.end(); it++)
    {
        updateHashes(*it);
        if (hash.is_full())
        {
            if (checkInFilter(bitvector))
            {
                return true;
            }
        }
    }

    return false;
}

template <class H, int LEN>
void Filter<H, LEN>::processFile(int fd) throw(FilterError)
{
    const int size = 512 * 1024;
    char* buff = new char[size];

    int crtLineSize = 1000;
    char* line = new char[crtLineSize + 1];
    line[0] = '\0';

    resetHashes();

    bool printLine = false;

    while (true)
    {
        int r = read(fd, (void*)buff, size);
        if (r < 0)
        {
            perror("Error");
            throw FilterError("Error while reading from file");
        }

        if (r == 0)
        {
            break;
        }

        int lineStart = 0;

        for (int i = 0; i < r; i++)
        {
            if (buff[i] == '\n')
            {
                if (printLine)
                {
                    printLine = false;

                    if (line[0] != '\0')
                    {
                        cout << line;
                    }
                    buff[i] = '\0';
                    cout << buff + lineStart << endl;
                }
                line[0] = '\0';
                resetHashes();
                lineStart = i + 1;
                continue;
            }
            updateHashes(buff[i]);
            if (hash.is_full())
            {
                if (checkInFilter(bitvector))
                {
                    setBitsInFilter_2(reverseBitvector);
                    printLine = true;
                    //contor++;
                }
            }
        }

        if (buff[r - 1] != '\n')
        {
            if (r - lineStart > crtLineSize)
            {
                crtLineSize = (r - lineStart) * 2;
                delete [] line;
                line = new char[crtLineSize + 1];
            }
            memcpy(line, buff + lineStart, r - lineStart);
            line[r - lineStart] = '\0';
        }
    }

    if (printLine)
    {
        cout << line << endl;
    }

    delete [] line;
    delete [] buff;
}

template <class H, int LEN>
void Filter<H, LEN>::filterPhrases(int pfd, ofstream& out, ofstream& outIndex) throw (FilterError)
{
    const int size = 512 * 1024;
    char* buff = new char[size];

    int crtLineSize = 1000;
    char* line = new char[crtLineSize + 1];
    line[0] = '\0';

    int lineNumber = 0;

    resetHashes();

    bool printLine = false, skipRest = false;

    while (true)
    {
        int r = read(pfd, (void*)buff, size);
        if (r < 0)
        {
            perror("Error");
            throw FilterError("Error while reading from file");
        }

        if (r == 0)
        {
            break;
        }

        int lineStart = 0;

        for (int i = 0; i < r; i++)
        {
            if (buff[i] == '\n')
            {
                skipRest = false;
                if (printLine)
                {
                    printLine = false;

                    if (line[0] != '\0')
                    {
                        out << line;
                    }
                    buff[i] = '\0';
                    out << buff + lineStart << endl;
                    outIndex << lineNumber << endl;
                }
                line[0] = '\0';
                resetHashes();
                lineStart = i + 1;
                lineNumber++;
                continue;
            }
            if (printLine || skipRest)
            {
                continue;
            }
            updateHashes(buff[i]);
            if (hash.is_full())
            {
                if (checkInFilter_2(reverseBitvector))
                {
                    printLine = true;
                }
                skipRest = true;
            }
        }

        if (buff[r - 1] != '\n')
        {
            if (r - lineStart > crtLineSize)
            {
                crtLineSize = (r - lineStart) * 2;
                delete [] line;
                line = new char[crtLineSize + 1];
            }
            memcpy(line, buff + lineStart, r - lineStart);
            line[r - lineStart] = '\0';
        }
    }

    if (printLine)
    {
        out << line << endl;
        outIndex << lineNumber << endl;
    }

    delete [] line;
    delete [] buff;
}

template <class H, int LEN>
void Filter<H, LEN>::loadFilterFromFile(int fd) throw(FilterError)
{
    uint32_t prev = 0;
    uint32_t buf[1024];
    ssize_t nr;
    while ((nr = read(fd, buf, 1024 * sizeof(uint32_t))) > 0)
    {
        for (uint32_t i = 0; i < nr / sizeof(uint32_t); i++)
        {
            prev += buf[i];
            setBit(prev, bitvector);
        }
    }
    if (read < 0)
    {
        perror("Error");
        throw FilterError("Error while reading from the filter file");
    }
}

template <class H, int LEN>
void Filter<H, LEN>::saveFilterToFile(int fd) throw(FilterError)
{
    uint32_t prev = 0;
    for (uint32_t i = 0; i < N; i++)
    {
        uint32_t byteIndex = i >> 3;
        uint8_t bitMask = 1 << (i & 0x00000007);
        if (bitvector[byteIndex] & bitMask)
        {
            uint32_t delta = i - prev;
            prev = i;
            if (write(fd, &delta, sizeof(uint32_t)) < 0)
            {
                perror("Error");
                throw FilterError("Error while writing to the filter file");
            }
        }
    }
}

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        cerr << "Usage: ffbf phrases phrases.out < corpus > filtered_corpus" << endl;
        return -1;
    }

    ofstream outPhrases(argv[2]);
    char phrasesIndexFileName[100];
    sprintf(phrasesIndexFileName, "__index_%s", argv[2]);
    ofstream outPhrasesIndex(phrasesIndexFileName);

    Filter<hash_rot_sbox_pre_2<DEF_PATT_LEN>, DEF_PATT_LEN> filter;


    //try to open the file that contains the bloom filter for phrases
    char fname[100];
    sprintf(fname, "__bloom_filter_%s", argv[1]);
    int bfd;
    if ((bfd = open(fname, O_RDONLY)) > 0)
    {
        TIME("filter load", filter.loadFilterFromFile(bfd));
        close(bfd);
    }
    else
    {
        ifstream inPhrases(argv[1]);
        string phrase;
        while (getline(inPhrases, phrase))
        {
            try
            {
                filter << phrase;
            }
            catch (FilterError& fe)
            {
                cerr << "------at phrase:" << endl;
                cerr << phrase << endl;
                cerr << "Exception " << fe.what() << endl;
            }
        }
        inPhrases.close();
        bfd = open(fname, O_CREAT | O_TRUNC| O_WRONLY, 0666);
        filter.saveFilterToFile(bfd);
        close(bfd);
    }

    int pfd = open(argv[1], O_RDONLY);
    if (pfd < 0)
    {
        perror("Error while trying to open the phrases file");
        return -1;
    }

    try
    {
        TIME("processFile", filter.processFile(0));
        TIME("filterPhrases", filter.filterPhrases(pfd, outPhrases, outPhrasesIndex));
    }
    catch (FilterError& fe)
    {
        cerr << "Exception " << fe.what() << endl;
    }

    //cerr << "fprate: " << contor << endl;

    close(pfd);
    outPhrases.close();
    outPhrasesIndex.close();

    return 0;
}
