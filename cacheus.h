/*
 * CACHEUS Replacement Policy is the algorithm that decides which cached item to
 * evict when the cache is full and a new item must be inserted. The goal is to maximize 
 * cache hits (minimize misses) given the workload and constraints (latency, memory, write/backend cost).
*/

#ifndef _cacheus_H
#define _cacheus_H
//#include <ctime>
#include <fstream>
#include <iostream>
#include <list>
#include <unordered_map>
#include <string.h>

using namespace std;

class CACHEUSCache {
    public:
        // Constructor and Destructor
        CACHEUSCache(int);
        ~CACHEUSCache();

        // Main access function: addr = block address, rwtype = "Read" or "Write"
        void refer(long long int addr, string rwtype);

        // Summary results for a single run
        void cacheHits();

        private:
        // Cache capacity in number of blocks
        int capacity;

        // Statistics for this run
        long long calls, hits, readHits, writeHits, evictedDirtyPage;

        // Simple page descriptor
        struct PageInfo {
            bool inCache;
            bool dirty;
            int freq;
            // Position in LRU list for this frequency
            std::list<long long int>::iterator lruIter;
        };

        // Main cache structures
        std::unordered_map<long long, PageInfo> table; // page -> info
        std::list<long long> lruList;                 // MRU at front, LRU at back

        // Two expert histories: pages each expert evicted
        std::list<long long> lruHistory;
        std::list<long long> lfuHistory;
        std::unordered_map<long long, std::list<long long>::iterator> lruHistoryIter;
        std::unordered_map<long long, std::list<long long>::iterator> lfuHistoryIter;
        int historyCapacity;  // max size of each history (half of cache size)

        // Expert weights (A and B)
        double wA; // weight for expert A (LRU)
        double wB; // weight for expert B (LFU)

        // Internal helper functions
        void touchPage(long long addr, const string &rwtype);
        void insertNewPage(long long addr, const string &rwtype);
        void evictAndInsert(long long addr, const string &rwtype);

        long long chooseVictimLRU() const;
        long long chooseVictimLFU() const;

        void addToHistoryA(long long victim);
        void addToHistoryB(long long victim);

        void updateWeightsFromHistory(long long addr);
};

#endif