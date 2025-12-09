/* 
   Least Frequently Used (LFU) page replacement algorithm belongs to the class of algorithms which
   are based on the frequency of usage of items in the memory. The main goal of the LFU algorithm
   is to discard, in each step, the item with the smallest frequency of usage. The LFU algorithm
   counts how often an item is needed. Those that are used least often are discarded first.
*/
#include <ctime>
#include <fstream>
#include <iostream>
#include <list>
#include <string.h>
#include <unordered_map>
using namespace std;
#ifndef _lfu_H
#define _lfu_H

class LFUCache
{
    // store keys of cache
    std::list<long long int> key_list;

    // store frequency of use of keys in cache {freq, list of keys with this freq}
    std::unordered_map<int, std::list<long long int>> key_freq_list;

    // store each key's current frequency
    std::unordered_map<long long int, int> key_to_freq;

    // store iterator to a key's position inside its frequency list
    std::unordered_map<long long int, std::list<long long int>::iterator> key_iter;
    // store key-value pairs of cache
    std::unordered_map<long long int, std::list<long long int>::iterator> lfu_hash_map;

    // current capacity of cache
    int capacity;

    // record read/write type of cached page
    std::unordered_map<long long int, string> accessType;

    long long int calls, total_calls;
    long long int hits, total_hits;
    long long int readHits;
    long long int writeHits;
    long long int evictedDirtyPage;
    long long int migration, total_migration;

    public:
    LFUCache(int);
    ~LFUCache();

    void refer(long long int, string);
    void cacheHits();
};

#endif