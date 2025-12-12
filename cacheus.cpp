#include "cacheus.h"

CACHEUSCache::CACHEUSCache(int size) 
    : capacity(size), calls(0), hits(0), readHits(0), 
       writeHits(0), evictedDirtyPage(0), historyCapacity(std::ceil(size * 0.1)) , wA(0.5), wB(0.5) {
    // Constructor implementation
}

CACHEUSCache::~CACHEUSCache(){
    // Destructor implementation
}

/*!
  @brief: Helper function to mark page as recently used
  @param: addr: page address
  @param: rwtype: read/write type of page
*/

void CACHEUSCache::touchPage(long long addr, const string &rwtype) {
    auto it = table.find(addr);
    if (it != table.end()) {
        // Update access type
        //it->second.accessType = rwtype;
        
        if (it->second.lruIter != lruList.end()) {
            // Move to front of LRU list
            lruList.erase(it->second.lruIter);
        }

        lruList.push_front(addr);
        it->second.lruIter = lruList.begin();

        // Update frequency count
        it->second.freq++;

        // Track dierty bit if write
        if (rwtype == "Write" || rwtype == "write" || rwtype == "W") {
            it->second.dirty = true;
        }
    }
    else {
        // Page not found in cache
        std::cerr << "Error: Attempting to touch a page not in cache." << std::endl;
        return;
    }
}

/*!
  @brief: Helper function to insert a new page into the cache
  @param: addr: page address
  @param: rwtype: read/write type of page
*/
void CACHEUSCache::insertNewPage(long long addr, const string &rwtype) {
    // Create new PageInfo
    PageInfo pinfo;
    pinfo.inCache = true;
    pinfo.dirty = (rwtype == "Write" || rwtype == "write" || rwtype == "W");
    pinfo.freq = 1;

    // Insert into LRU list at front
    lruList.push_front(addr);
    pinfo.lruIter = lruList.begin();

    // Insert into table
    table[addr] = pinfo;
}

/*!
  @brief: Helper function to choose victim page using LRU policy (A)
*/
long long CACHEUSCache::chooseVictimLRU() const {
    if (lruList.empty()) {
        std::cerr << "Error: LRU list is empty." << std::endl;
        return -1;
    }
    // Victim is the least recently used page (back of LRU list)
    return lruList.back();
}

/*!
  @brief: Helper function to choose victim page using LFU policy (B)
*/
long long CACHEUSCache::chooseVictimLFU() const {
    if (lruList.empty()) {
        std::cerr << "Error: Cache is empty." << std::endl;
        return -1;
    }

    // Find the page with the minimum frequency count
    long long victimAddr = -1;
    int minFreq = INT_MAX;

    for (auto it = lruList.rbegin(); it != lruList.rend(); ++it) {
        long long addr = *it;
        auto pageIt = table.find(addr);
        if (pageIt != table.end()) {
            if (pageIt->second.freq < minFreq) {
                minFreq = pageIt->second.freq;
                victimAddr = addr;
            }
        }
    }

    if (victimAddr == -1) {
        std::cerr << "Error: Unable to find victim page using LFU." << std::endl;
        // Fallback to LRU if LFU fails (shouldn't happen if cache isn't empty)
        victimAddr = chooseVictimLRU();
    }
    return victimAddr;
}

/*!
  @brief: Helper function to add a victim page to LRU history
  @param: victim: page address of victim
*/
void CACHEUSCache::addToHistoryA(long long victim) {
    // LRU expert history update
    auto it = lruHistoryIter.find(victim);
    if (it != lruHistoryIter.end()) {
        // Remove from history if already present
        lruHistory.erase(it->second);
    }

    // Add victim to LRU history
    lruHistory.push_front(victim);
    lruHistoryIter[victim] = lruHistory.begin();

    // Maintain history capacity (capacity / 2)
    while ((int)lruHistory.size() > historyCapacity) {
        long long oldVictim = lruHistory.back();
        lruHistory.pop_back();
        lruHistoryIter.erase(oldVictim);
    }
}

/*!
  @brief: Helper function to add a victim page to LFU history
  @param: victim: page address of victim
*/
void CACHEUSCache::addToHistoryB(long long victim) {
    // LFU expert history update
    auto it = lfuHistoryIter.find(victim);
    if (it != lfuHistoryIter.end()) {
        // Remove from history if already present
        lfuHistory.erase(it->second);
    }
    // Add victim to LFU history
    lfuHistory.push_front(victim);
    lfuHistoryIter[victim] = lfuHistory.begin();
    // Maintain history capacity (capacity / 2)
    while ((int)lfuHistory.size() > historyCapacity) {
        long long oldVictim = lfuHistory.back();
        lfuHistory.pop_back();
        lfuHistoryIter.erase(oldVictim);
    }
}

/*!
  @brief: Helper function to update expert weights based on history hits
  @param: addr: page address being referenced
*/
void CACHEUSCache::updateWeightsFromHistory(long long addr) {
    bool inA = false;
    bool inB = false;

    // Check if addr is in LRU history
    auto itA = lruHistoryIter.find(addr);
    if (itA != lruHistoryIter.end()) {
        inA = true;
        lruHistory.erase(itA->second);
        lruHistoryIter.erase(itA);
        //lruHistory.erase(itA);
    }

    // Check if addr is in LFU history
    auto itB = lfuHistoryIter.find(addr);
    if (itB != lfuHistoryIter.end()) {
        inB = true;
        lfuHistory.erase(itB->second);
        lfuHistoryIter.erase(itB);
        //lfuHistory.erase(itB);
    }

    // If a miss happens on spmething evicted by A (LRU), penalize A, reward B (LFU)
    // and vice versa

    // Learning rate
    const double alpha = 0.1;

    // Update weights based on history hits
    if (inA && inB) {
        // Both experts predicted this page - no change
        return;
    } else if (inA && !inB) {
        // Only LRU predicted this page
        wA = std::max(0.0, wA - alpha);
        wB = 1.0 - wA;
    } else if (inB) {
        // Only LFU predicted this page
        wB = std::max(0.0, wB - alpha);
        wA = 1.0 - wB;
    } else {
        // Neither predicted this page - no change
        return;
    }
}

/*!
  @brief: Helper function to evict a page and insert a new page when cache is full
  @param: addr: page address to insert
  @param: rwtype: read/write type of page
*/
void CACHEUSCache::evictAndInsert(long long addr, const string &rwtype) {
    if (capacity <= 0) {
        std::cerr << "Error: Cache capacity is zero." << std::endl;
        return;
    }

    // Choose which expert to follow (pick higher weight expert)
    bool useLRU = (wA >= wB);

    long long victim = useLRU ? chooseVictimLRU() : chooseVictimLFU();
    if (victim == -1) {
        std::cerr << "Error: No victim selected for eviction." << std::endl;
        // Should not happen, but safeguard, treat as empty cache
        insertNewPage(addr, rwtype);
        return;
    }

    // Update dirty page eviction count
    auto victimIt = table.find(victim);
    if (victimIt != table.end()) {
        if (victimIt->second.dirty) {
            evictedDirtyPage++;
        }
    }

    // Remove victim from cache structures
    if (victimIt != table.end()) {
        lruList.erase(victimIt->second.lruIter);
        table.erase(victimIt);
    }

    // Add to appropriate history
    if (useLRU) {
        addToHistoryA(victim);
    } else {
        addToHistoryB(victim);
    }

    // Insert the new page
    insertNewPage(addr, rwtype);
}

/*!
  @brief: Main access function to reference a page
  @param: addr: page address
  @param: rwtype: read/write type of page
*/
void CACHEUSCache::refer(long long int addr, string rwtype) {
    std::cout << "Referencing page for CACHEUS " << std::endl;
    calls++;
    auto it = table.find(addr);
    if (it != table.end()) {
        // Page hit
        hits++;
        if (rwtype == "W" || rwtype == "write" || rwtype == "Write") {
            writeHits++;
        } else if (rwtype == "R" || rwtype == "read" || rwtype == "Read"){
            readHits++;
        }
        touchPage(addr, rwtype);
        return;
    } 

    // Page miss
    // Use history to update expert weights based on who evicted this page
    updateWeightsFromHistory(addr);

    // If cache is full, evict a page
    if ((int)table.size() < capacity) {
        insertNewPage(addr, rwtype);
    } else {
        evictAndInsert(addr, rwtype);
    }
}

/*!
  @brief: Summary function to print cache hit statistics
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
        result << "CACHEUS Algorithm" << std::endl;
        result << "Cache Size: " << capacity << std::endl;
        result << "Total Calls: " << calls << std::endl;
        result << "Total Hits: " << hits << std::endl;
        result << "Hit Rate: " << (static_cast<double>(hits) / calls) * 100 << "%" << std::endl;
        result << "Read Hits: " << readHits << std::endl;
        result << "Write Hits: " << writeHits << std::endl;
        result << "Evicted Dirty Pages: " << evictedDirtyPage << std::endl;
        result << "----------------------------------------" << std::endl;
        result.close();
    }

    result.close();
}  