#ifndef _cacheus_H
#define _cacheus_H

#include <fstream>
#include <iostream>
#include <list>
#include <unordered_map>
#include <string>

using namespace std;

class CACHEUSCache {
public:
    CACHEUSCache(int);
    ~CACHEUSCache();

    void refer(long long int addr, string rwtype);
    void cacheHits();

private:
    int capacity;
    long long calls, hits, readHits, writeHits, evictedDirtyPage;

    // Global LRU list (Expert A)
    std::list<long long> lruList; // MRU front, LRU back

    // LFU buckets (Expert B): freq -> keys (MRU front, LRU back)
    std::unordered_map<int, std::list<long long>> freqBuckets;
    int minFreq;

    struct PageInfo {
        bool dirty;
        int freq;

        // Iterators for O(1) removals
        std::list<long long>::iterator lruIter;   // in lruList
        std::list<long long>::iterator freqIter;  // in freqBuckets[freq]
    };

    std::unordered_map<long long, PageInfo> table;

    // Histories
    std::list<long long> lruHistory;
    std::list<long long> lfuHistory;
    std::unordered_map<long long, std::list<long long>::iterator> lruHistoryIter;
    std::unordered_map<long long, std::list<long long>::iterator> lfuHistoryIter;
    int historyCapacity;

    // Expert weights
    double wA, wB;

    // Helpers
    void touchPage(long long addr, const string &rwtype);
    void insertNewPage(long long addr, const string &rwtype);
    void evictAndInsert(long long addr, const string &rwtype);

    long long chooseVictimLRU() const;
    long long chooseVictimLFU(); // updates minFreq if needed

    void addToHistoryA(long long victim);
    void addToHistoryB(long long victim);
    void updateWeightsFromHistory(long long addr);

    void removeFromFreqBucket(long long addr, PageInfo &info);
    void addToFreqBucketFront(long long addr, PageInfo &info, int newFreq);
    void fixMinFreqAfterRemoval(int removedFreq);
};

#endif