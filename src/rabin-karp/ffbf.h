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
*/

#ifndef RABIN_KARP_FILTER_H
#define RABIN_KARP_FILTER_H

#include <string>
#include <queue>
#include <vector>
#include <stdint.h>

#include "../hashes/hash_rot_sbox_pre_2.h"

using namespace std;

class FilterError
{
    const char* const message;

public:
    FilterError(const char* const msg = 0) : message(msg) {}
    const char* const what()
    {
        return message;
    }
};

template<class H, int LEN>
class Filter
{
    //the number of bits in the Bloom filter
    static const uint32_t N;
    //the number of hash functions
    static const int K;
    //the number of hash functions for the cache-resident part
    static const int S;

    static const uint32_t BloomCacheMask;
    static const uint32_t BloomExtMask;

    H hash;

    //the Bloom filter bit vector
    uint8_t* bitvector;
    //reverse Bloom filter bit vector
    uint8_t* reverseBitvector;

    int fid;
    void* mapaddr;

    //computes rolling hash functions
    void updateHashes(u_int8_t nextChar);

    //reset rolling hash computation
    void resetHashes();

    //set a bit in a Bloom filter
    void setBit(uint32_t index, uint8_t* vector);

    //checks if the current hash values hit in a Bloom filter
    bool checkInFilter(const uint8_t* vector);
    void setBitsInFilter(uint8_t *vector);
    uint32_t getHashValue(int i);

    bool checkInFilter_2(const uint8_t* vector);
    void setBitsInFilter_2(uint8_t *vector);
    uint32_t getHashValue_2(int i);

public:
    Filter();
    ~Filter();
    bool operator[](const string& pattern) throw(FilterError);
    Filter& operator<<(string& pattern) throw(FilterError);
    void loadFilterFromFile(int fd) throw(FilterError);
    void saveFilterToFile(int fd) throw(FilterError);
    void processFile(int fd) throw(FilterError);
    void filterPhrases(int pfd, ofstream& out, ofstream& outIndex) throw(FilterError);
};

/* Handy crap for filling in templates */
template <int P>
struct kpowerto
{ enum { value = 2654435769U * kpowerto<P-1>::value };};

template<>
struct kpowerto<0>
{ enum { value = 1 }; };

template <int P>
struct fpowerto
{ enum { value = (0xffffffff & (2166136261U * kpowerto<P-1>::value)) };};

template<>
struct fpowerto<0>
{ enum { value = 1 }; };

#endif
