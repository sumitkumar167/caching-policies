#include "lfu.h"

LFUCache::LFUCache(int capacity) : capacity(capacity) {
    hits = 0;
    total_hits = 0;
    calls = 0;
    total_calls = 0;
    migration = 0;
    total_migration = 0;
    readHits = 0;
    writeHits = 0;
    evictedDirtyPage = 0;

    std::cout << "LFU Algorithm is used" << std::endl;
    std::cout << "Cache size is: " << capacity << std::endl;
}

LFUCache::~LFUCache() {
    capacity = 0;
    hits = 0;
    total_hits = 0;
    calls = 0;
    total_calls = 0;
    migration = 0;
    total_migration = 0;
    readHits = 0;
    writeHits = 0;
    evictedDirtyPage = 0;

    key_list.clear();
    key_freq_list.clear();
    key_to_freq.clear();
    key_iter.clear();
    lfu_hash_map.clear();
    accessType.clear();
}

void LFUCache::refer(long long int key, string rwtype) {
    calls++;

    // If key is not present in cache
    if (key_to_freq.find(key) == key_to_freq.end()) {
        // If cache is full -> evict one key from the smallest frequency bucket
        if ((int)key_to_freq.size() == capacity) {
            if (!key_freq_list.empty()) {
                // find smallest frequency
                int min_freq = INT_MAX;
                for (const auto &p : key_freq_list) {
                    if (p.first < min_freq) min_freq = p.first;
                }
                auto &freq_list = key_freq_list[min_freq];
                // evict the oldest key in that frequency bucket (front)
                long long int lfu_key = freq_list.front();
                // remove from freq list and bookkeeping
                freq_list.pop_front();
                if (freq_list.empty()) {
                    key_freq_list.erase(min_freq);
                }
                // erase key metadata
                key_to_freq.erase(lfu_key);
                auto itit = key_iter.find(lfu_key);
                if (itit != key_iter.end()) key_iter.erase(itit);
                if (accessType[lfu_key] == "Write") {
                    evictedDirtyPage++;
                }
                accessType.erase(lfu_key);
            }
        }

        // Insert the new key into frequency 1 bucket
        key_to_freq[key] = 1;
        key_freq_list[1].push_back(key);
        auto it = --key_freq_list[1].end();
        key_iter[key] = it;
        accessType[key] = rwtype;
    } else {
        // Key is present in cache -> hit
        hits++;
        int old_freq = key_to_freq[key];
        int new_freq = old_freq + 1;

        // remove from old freq list using stored iterator
        auto it = key_iter[key];
        auto &old_list = key_freq_list[old_freq];
        old_list.erase(it);
        if (old_list.empty()) {
            key_freq_list.erase(old_freq);
        }

        // add to new freq list
        key_freq_list[new_freq].push_back(key);
        auto new_it = --key_freq_list[new_freq].end();
        key_iter[key] = new_it;
        key_to_freq[key] = new_freq;

        if (rwtype == "Read") {
            readHits++;
        } else {
            writeHits++;
            accessType[key] = "Write";
        }
    }
}

void LFUCache::cacheHits() {
    std::cout << "Total Calls: " << calls << std::endl;
    std::cout << "Total Hits: " << hits << std::endl;
    std::cout << "Hit Rate: " << (static_cast<double>(hits) / calls) * 100 << "%" << std::endl;
    std::cout << "Read Hits: " << readHits << std::endl;
    std::cout << "Write Hits: " << writeHits << std::endl;
    std::cout << "Evicted Dirty Pages: " << evictedDirtyPage << std::endl;

    std::ofstream result("ExperimentalResult.txt", std::ios_base::app);
    if (result.is_open()) {
        result << "LFU Algorithm" 
         << "Cache Size: " << capacity 
         << "Total Calls: " << calls 
         << "Total Hits: " << hits 
         << "Hit Rate: " << (static_cast<double>(hits) / calls) * 100 << "%" 
         << "Read Hits: " << readHits 
         << "Write Hits: " << writeHits 
         << "Evicted Dirty Pages: " << evictedDirtyPage << std::endl;
        result.close();
    }

    result.close();
}

