/*
  CACHEUS hybrid cache implementation
  This file implements a combined expert-based cache (CACHEUS) that
  adaptively chooses between an LRU expert (A) and an LFU expert (B).
  Data structures:
     - `table`: map from page -> PageInfo (tracks freq, dirty bit, iterators)
     - `lruList`: global recency list for the LRU expert
     - `freqBuckets`: map from frequency -> list of pages (LFU buckets)
     - `lruHistory` / `lfuHistory`: regret/history lists used to adjust expert weights
  The implementation maintains `minFreq` to speed up LFU victim selection.
*/
#include "cacheus.h"
#include <cmath>
#include <algorithm>
#include <climits>

/*!
    @brief: Constructor — initialize counters, capacities and heuristic defaults.
    @details: `historyCapacity` is set as 10% of the cache size by default.
    @param size: cache size in pages
*/
CACHEUSCache::CACHEUSCache(int size)
    : capacity(size),
      calls(0), hits(0), readHits(0), writeHits(0), evictedDirtyPage(0),
      minFreq(1),
      historyCapacity((int)std::ceil(size * 0.1)),
      wA(0.5), wB(0.5)
{
    // Reserve buckets to reduce rehash costs on large traces
    table.reserve((size_t)(capacity * 1.3) + 16);
    lruHistoryIter.reserve((size_t)(historyCapacity * 1.3) + 16);
    lfuHistoryIter.reserve((size_t)(historyCapacity * 1.3) + 16);
    freqBuckets.reserve(128);
}

/*!
    @brief: Destructor — currently no special cleanup required.
*/
CACHEUSCache::~CACHEUSCache() {}

/*!
    @brief: Check whether a rw string represents a write operation.
    @param rw: string representing the operation type
*/
static inline bool isWrite(const std::string& rw) {
    return (rw == "Write" || rw == "write" || rw == "W" || rw == "w");
}

/*!
    @brief: Check whether a rw string represents a read operation.
    @param rw: string representing the operation type
*/
static inline bool isRead(const std::string& rw) {
        return (rw == "Read" || rw == "read" || rw == "R" || rw == "r");
}

/*!
    @brief: Remove a page from its current LFU frequency bucket.
    @details: Uses the iterator stored in PageInfo to perform O(1) erasure.
    @param addr: page address to remove
    @param info: PageInfo reference where the iterator and freq are stored
*/
void CACHEUSCache::removeFromFreqBucket(long long addr, PageInfo &info) {
    auto fb = freqBuckets.find(info.freq);
    if (fb == freqBuckets.end()) return;
    fb->second.erase(info.freqIter);
    // If list becomes empty, erase the bucket to keep map small
    if (fb->second.empty()) {
        freqBuckets.erase(fb);
    }
}

/*!
    @brief: Update `minFreq` after removing pages from frequency buckets.
    @details: If the removed frequency was equal to `minFreq`, advance `minFreq`
                     until a non-empty bucket is found.
    @param removedFreq: frequency of the removed page
*/
void CACHEUSCache::fixMinFreqAfterRemoval(int removedFreq) {
    if (removedFreq != minFreq) return;
    // Advance minFreq until we find a non-empty bucket
    while (true) {
        auto it = freqBuckets.find(minFreq);
        if (it != freqBuckets.end() && !it->second.empty()) return;
        minFreq++;
        // minFreq can grow; that's fine (amortized).
        // No need to cap, because it only increases with total accesses.
    }
}

/*!
    @brief: Add a page to the front (MRU) of a frequency bucket.
    @param addr: page address to add
    @param info: PageInfo reference where the iterator and freq are stored
    @param newFreq: target frequency bucket
*/
void CACHEUSCache::addToFreqBucketFront(long long addr, PageInfo &info, int newFreq) {
    info.freq = newFreq;
    auto &lst = freqBuckets[newFreq];
    lst.push_front(addr);          // MRU at front
    info.freqIter = lst.begin();
}

/*!
    @brief: Handle a cache hit by updating LRU and LFU structures.
    @details: Moves the page to the front of global LRU, increments its
                     frequency in LFU buckets, and sets the dirty flag on writes.
    @param addr: page address being accessed
    @param rwtype: read/write operation type
                     */
void CACHEUSCache::touchPage(long long addr, const string &rwtype) {
    auto it = table.find(addr);
    if (it == table.end()) return;

    PageInfo &info = it->second;

    // Update global LRU (expert A)
    lruList.erase(info.lruIter);
    lruList.push_front(addr);
    info.lruIter = lruList.begin();

    // Update LFU buckets (expert B)
    int oldFreq = info.freq;
    removeFromFreqBucket(addr, info);
    int newFreq = oldFreq + 1;
    addToFreqBucketFront(addr, info, newFreq);

    // If we removed the last element of minFreq bucket, minFreq may need to move
    fixMinFreqAfterRemoval(oldFreq);

    // Dirty tracking
    if (isWrite(rwtype)) info.dirty = true;
}

/*!
    @brief: Insert a new page into the cache.
    @details: Adds the page to the front of the global LRU and the
                     frequency-1 LFU bucket; initializes its dirty flag.
    @param addr: page address being inserted
*/
void CACHEUSCache::insertNewPage(long long addr, const string &rwtype) {
    // Insert into global LRU list
    lruList.push_front(addr);

    PageInfo info;
    info.dirty = isWrite(rwtype);
    info.freq  = 1;
    info.lruIter = lruList.begin();

    // Insert into LFU bucket freq=1 (MRU front)
    auto &lst = freqBuckets[1];
    lst.push_front(addr);
    info.freqIter = lst.begin();

    table.emplace(addr, info);

    // New pages always set minFreq=1
    minFreq = 1;
}

/*!
    @brief: Choose a victim using the LRU expert (least recently used).
    @details: Returns the LRU page (back of the list).
    @return: address of the victim page, or -1 if cache is empty
*/
long long CACHEUSCache::chooseVictimLRU() const {
    if (lruList.empty()) return -1;
    return lruList.back();
}

/*!
    @brief: Choose a victim according to the LFU expert.
    @details: Uses `minFreq` as a hint and applies CR-LFU tie-break (evict MRU
                     among the minimum-frequency bucket).
    @return: address of the victim page, or -1 if cache is empty
*/
long long CACHEUSCache::chooseVictimLFU() {
    // Ensure minFreq points to an existing bucket
    auto it = freqBuckets.find(minFreq);
    while (it == freqBuckets.end() || it->second.empty()) {
        minFreq++;
        it = freqBuckets.find(minFreq);
        if (minFreq > INT_MAX / 2) return chooseVictimLRU(); // extreme safeguard
    }
    // CR-LFU tie-break (per paper): MRU among min-freq items → front()
    return it->second.front();
}

/*!
    @brief: Record an evicted victim into the LRU regret/history (expert A).
    @details: Keeps history bounded and stores iterators for O(1) removal.
    @param victim: address of the evicted page
*/
void CACHEUSCache::addToHistoryA(long long victim) {
    auto it = lruHistoryIter.find(victim);
    if (it != lruHistoryIter.end()) lruHistory.erase(it->second);

    lruHistory.push_front(victim);
    lruHistoryIter[victim] = lruHistory.begin();

    while ((int)lruHistory.size() > historyCapacity) {
        long long old = lruHistory.back();
        lruHistory.pop_back();
        lruHistoryIter.erase(old);
    }
}

/*!
    @brief: Record an evicted victim into the LFU regret/history (expert B).
    @details: Keeps history bounded and stores iterators for O(1) removal.
    @param victim: address of the evicted page
*/
void CACHEUSCache::addToHistoryB(long long victim) {
    auto it = lfuHistoryIter.find(victim);
    if (it != lfuHistoryIter.end()) lfuHistory.erase(it->second);

    lfuHistory.push_front(victim);
    lfuHistoryIter[victim] = lfuHistory.begin();

    while ((int)lfuHistory.size() > historyCapacity) {
        long long old = lfuHistory.back();
        lfuHistory.pop_back();
        lfuHistoryIter.erase(old);
    }
}

/*!
    @brief: Update expert weights based on regret history hits.
    @details: If the missed page is present in one history (A or B) but not the
                     other, slightly favor that expert by adjusting wA/wB.
    @param addr: address of the missed page
*/
void CACHEUSCache::updateWeightsFromHistory(long long addr) {
    bool inA = false, inB = false;

    auto itA = lruHistoryIter.find(addr);
    if (itA != lruHistoryIter.end()) {
        inA = true;
        lruHistory.erase(itA->second);
        lruHistoryIter.erase(itA);
    }

    auto itB = lfuHistoryIter.find(addr);
    if (itB != lfuHistoryIter.end()) {
        inB = true;
        lfuHistory.erase(itB->second);
        lfuHistoryIter.erase(itB);
    }

    const double alpha = 0.1;
    if (inA && !inB) {
        // Favor LRU slightly
        wA = std::max(0.0, wA - alpha);
        wB = 1.0 - wA;
    } else if (inB && !inA) {
        // Favor LFU slightly
        wB = std::max(0.0, wB - alpha);
        wA = 1.0 - wB;
    }
}

/*!
    @brief: Evict a victim (by favored expert) and insert a new page.
    @details: Updates dirty-eviction counts, removes entries from both experts,
                     records regret, and inserts the new page.
    @param addr: page address being inserted
    @param rwtype: read/write operation type
*/
void CACHEUSCache::evictAndInsert(long long addr, const string &rwtype) {
    if (capacity <= 0) return;

    bool useLRU = (wA >= wB);
    long long victim = useLRU ? chooseVictimLRU() : chooseVictimLFU();
    if (victim == -1) {
        insertNewPage(addr, rwtype);
        return;
    }

    auto vit = table.find(victim);
    if (vit != table.end()) {
        PageInfo &vinfo = vit->second;

        if (vinfo.dirty) evictedDirtyPage++;

        // Remove from global LRU
        lruList.erase(vinfo.lruIter);

        // Remove from LFU bucket
        int oldFreq = vinfo.freq;
        removeFromFreqBucket(victim, vinfo);
        fixMinFreqAfterRemoval(oldFreq);

        // Remove from table
        table.erase(vit);
    }

    // Record regret history
    if (useLRU) addToHistoryA(victim);
    else        addToHistoryB(victim);

    insertNewPage(addr, rwtype);
}

/*!
    @brief: Process a cache reference (hit or miss).
    @details: On hit updates stats and moves the page in both experts. On miss
                     consults history for weight updates and inserts/evicts accordingly.
    @param addr: page address being accessed
    @param rwtype: read/write operation type
*/
void CACHEUSCache::refer(long long int addr, string rwtype) {
    calls++;

    auto it = table.find(addr);
    if (it != table.end()) {
        // HIT: update stats and move the page
        hits++;
        if (isWrite(rwtype)) writeHits++;
        else if (isRead(rwtype)) readHits++;

        touchPage(addr, rwtype);
        return;
    }

    // MISS: adjust expert weights from regret history and bring page in
    updateWeightsFromHistory(addr);

    if ((int)table.size() < capacity) insertNewPage(addr, rwtype);
    else evictAndInsert(addr, rwtype);
}

/*!
    @brief: Summary function to print cache hit statistics.
*/
void CACHEUSCache::cacheHits() {
    std::cout << "Total Calls: " << calls << std::endl;
    std::cout << "Total Hits: " << hits << std::endl;
    std::cout << "Hit Rate: " << (static_cast<double>(hits) / calls) * 100 << "%" << std::endl;
    std::cout << "Read Hits: " << readHits << std::endl;
    std::cout << "Write Hits: " << writeHits << std::endl;
    std::cout << "Evicted Dirty Pages: " << evictedDirtyPage << std::endl;

    std::ofstream result("ExperimentalResult.txt", std::ios_base::app);
    if (result.is_open()) {
        result << "CACHEUS Algorithm" << 
         "Cache Size: " << capacity << 
         "Total Calls: " << calls << 
         "Total Hits: " << hits << 
         "Hit Rate: " << (static_cast<double>(hits) / calls) * 100 << "%" << 
         "Read Hits: " << readHits << 
         "Write Hits: " << writeHits << 
         "Evicted Dirty Pages: " << evictedDirtyPage << std::endl;
        result.close();
    }

    result.close();
}  