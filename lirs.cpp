#include "lirs.h"

#include <list>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>

using namespace std;

struct LIRSCache::Impl
{
    // Capacity (number of blocks)
    int csize;

    // Choose HIR portion (~1% of cache, but at least 1)
    int hirCap;       // max size of Q (resident HIR)
    int lirTarget;    // desired # resident LIR = csize - hirCap

    // Stats
    long long calls = 0;
    long long hits = 0;
    long long readHits = 0;
    long long writeHits = 0;
    long long evictedDirtyPage = 0;

    // Stack S (MRU front)
    list<long long> S;
    unordered_map<long long, list<long long>::iterator> Spos;

    // Queue Q: resident HIR only (FIFO-ish: push_front, evict back)
    list<long long> Q;
    unordered_map<long long, list<long long>::iterator> Qpos;

    struct PageInfo {
        bool isLIR = false;
        bool resident = false;
        bool dirty = false;
    };
    unordered_map<long long, PageInfo> page;

    int residentCount = 0;
    int lirCount = 0;

    static bool isWriteOp(const string& rw) {
        return (rw == "Write" || rw == "write" || rw == "W" || rw == "w");
    }
    static bool isReadOp(const string& rw) {
        return (rw == "Read" || rw == "read" || rw == "R" || rw == "r");
    }

    void moveToTopS(long long key) {
        auto it = Spos.find(key);
        if (it != Spos.end()) {
            S.erase(it->second);
        }
        S.push_front(key);
        Spos[key] = S.begin();
    }

    void removeFromQ(long long key) {
        auto it = Qpos.find(key);
        if (it != Qpos.end()) {
            Q.erase(it->second);
            Qpos.erase(it);
        }
    }

    void pushFrontQ(long long key) {
        // key must be resident HIR
        removeFromQ(key);
        Q.push_front(key);
        Qpos[key] = Q.begin();
    }

    void pruneStackS() {
        // Remove bottom entries that are HIR and non-resident
        while (!S.empty()) {
            long long b = S.back();
            auto pit = page.find(b);
            if (pit == page.end()) {
                // stale
                Spos.erase(b);
                S.pop_back();
                continue;
            }
            PageInfo &info = pit->second;
            if (!info.isLIR && !info.resident) {
                Spos.erase(b);
                S.pop_back();
                // also drop metadata to avoid unbounded growth
                page.erase(b);
            } else {
                break;
            }
        }

        // Optional: prevent S from growing too large (keep ~2*csize history)
        const size_t maxS = (size_t)max(2, 2 * csize);
        while (S.size() > maxS) {
            long long b = S.back();
            auto pit = page.find(b);
            if (pit == page.end()) {
                Spos.erase(b);
                S.pop_back();
                continue;
            }
            PageInfo &info = pit->second;
            if (!info.isLIR && !info.resident) {
                Spos.erase(b);
                S.pop_back();
                page.erase(b);
            } else {
                break; // don't remove resident/LIR bottoms
            }
        }
    }

    long long findDemoteLIRVictim() {
        // Find from bottom of S the first resident LIR
        for (auto it = S.rbegin(); it != S.rend(); ++it) {
            long long k = *it;
            auto pit = page.find(k);
            if (pit == page.end()) continue;
            PageInfo &info = pit->second;
            if (info.resident && info.isLIR) return k;
        }
        return -1;
    }

    void demoteOneLIRToHIR() {
        long long victim = findDemoteLIRVictim();
        if (victim == -1) return; // should not happen if lirCount > 0

        PageInfo &vinfo = page[victim];
        if (!vinfo.isLIR || !vinfo.resident) return;

        vinfo.isLIR = false;
        lirCount = max(0, lirCount - 1);

        // Now it becomes resident HIR → must be in Q
        pushFrontQ(victim);
    }

    void evictFromQIfNeeded() {
        // Ensure residentCount <= csize by evicting from Q tail
        while (residentCount > csize) {
            if (Q.empty()) {
                // This should never happen if hirCap >= 1 and policy is consistent,
                // but prevent crash.
                break;
            }
            long long victim = Q.back();
            Q.pop_back();
            Qpos.erase(victim);

            auto pit = page.find(victim);
            if (pit == page.end()) continue;

            PageInfo &vinfo = pit->second;
            // victim must be resident HIR
            if (vinfo.dirty) evictedDirtyPage++;
            vinfo.resident = false;
            vinfo.dirty = false;   // no longer resident
            // keep its record in S as non-resident HIR history
            vinfo.isLIR = false;

            residentCount = max(0, residentCount - 1);
        }
    }

    void insertAsResidentHIR(long long key, bool isWrite) {
        PageInfo &info = page[key];
        info.isLIR = false;
        if (!info.resident) {
            info.resident = true;
            residentCount++;
        }
        if (isWrite) info.dirty = true;
        pushFrontQ(key);
        moveToTopS(key);
    }

    void insertAsResidentLIR(long long key, bool isWrite) {
        PageInfo &info = page[key];
        if (!info.resident) {
            info.resident = true;
            residentCount++;
        }
        if (!info.isLIR) {
            info.isLIR = true;
            lirCount++;
        }
        if (isWrite) info.dirty = true;

        // LIR should NOT be in Q
        removeFromQ(key);
        moveToTopS(key);
    }

    void onHit(long long key, const string& rw) {
        hits++;
        if (isReadOp(rw)) readHits++;
        else if (isWriteOp(rw)) writeHits++;

        bool isWrite = isWriteOp(rw);
        PageInfo &info = page[key];
        if (isWrite) info.dirty = true;

        if (info.isLIR) {
            // LIR hit: just move in S
            moveToTopS(key);
            pruneStackS();
            return;
        }

        // resident HIR hit: promote to LIR, demote one LIR to HIR
        removeFromQ(key);            // not HIR in Q anymore after promotion
        insertAsResidentLIR(key, isWrite);
        demoteOneLIRToHIR();
        pruneStackS();
    }

    void onMiss(long long key, const string& rw) {
        bool isWrite = isWriteOp(rw);

        bool inS = (Spos.find(key) != Spos.end());
        // If full, make room by evicting from Q after we insert
        // (we keep logic simple)

        if (residentCount < csize) {
            // Still filling cache.
            // First fill LIR set up to lirTarget.
            if (lirCount < lirTarget) {
                insertAsResidentLIR(key, isWrite);
            } else {
                insertAsResidentHIR(key, isWrite);
            }
            pruneStackS();
            return;
        }

        // Cache full: we must ensure a HIR victim exists in Q to evict.
        // Two cases:
        // 1) key is in S (nonresident HIR history) → promote to LIR, demote one LIR to HIR
        // 2) key is new → insert as resident HIR
        if (inS) {
            // Miss on a nonresident HIR with history → becomes LIR
            insertAsResidentLIR(key, isWrite);
            demoteOneLIRToHIR(); // keeps lirCount ~ constant at lirTarget
        } else {
            // New page: insert as resident HIR (must go to Q)
            insertAsResidentHIR(key, isWrite);
        }

        // Now evict if needed
        evictFromQIfNeeded();
        pruneStackS();
    }
};

LIRSCache::LIRSCache(int size) {
    p = new Impl();
    p->csize = max(0, size);

    // HIR ~ 1% but at least 1 and at most csize-1 (if csize>1)
    if (p->csize <= 1) {
        p->hirCap = 1;         // degenerate
        p->lirTarget = 0;
    } else {
        int onePercent = (int)floor(p->csize * 0.01);
        p->hirCap = max(1, onePercent);
        p->hirCap = min(p->hirCap, p->csize - 1);
        p->lirTarget = p->csize - p->hirCap;
    }
}

LIRSCache::~LIRSCache() {
    delete p;
}

void LIRSCache::refer(long long int addr, string rw) {
    p->calls++;

    auto it = p->page.find(addr);
    bool resident = (it != p->page.end() && it->second.resident);

    if (resident) {
        p->onHit(addr, rw);
    } else {
        p->onMiss(addr, rw);
    }
}

void LIRSCache::cacheHitsResult() {
    cout << "LIRS CacheSize " << p->csize << endl;
     cout  << " calls " << p->calls << endl;
       cout  << " hits " << p->hits << endl;
        cout << " hitRatio " << (p->calls > 0 ? (double)p->hits / (double)p->calls : 0.0) << endl;
       cout  << " readHits " << p->readHits << endl;
      cout   << " readHitRatio " << (p->calls > 0 ? (double)p->readHits / (double)p->calls : 0.0) << endl;
       cout  << " writeHits " << p->writeHits << endl;
      cout   << " writeHitRatio " << (p->calls > 0 ? (double)p->writeHits / (double)p->calls : 0.0) << endl;
       cout  << " evictedDirtyPage " << p->evictedDirtyPage  << endl;

    ofstream result("ExperimentalResult.txt", ios_base::app);
    if (result.is_open()) {
        result << "LIRS " << endl;
        result  << "CacheSize " << p->csize << endl;
        result  << " calls " << p->calls << endl;
             result  << " hits " << p->hits << endl;
            result   << " hitRatio " << (p->calls > 0 ? (double)p->hits / (double)p->calls : 0.0) << endl;
           result    << " readHits " << p->readHits << endl;
              result << " readHitRatio " << (p->calls > 0 ? (double)p->readHits / (double)p->calls : 0.0) << endl;
             result  << " writeHits " << p->writeHits << endl;
             result  << " writeHitRatio " << (p->calls > 0 ? (double)p->writeHits / (double)p->calls : 0.0) << endl;
             result  << " evictedDirtyPage " << p->evictedDirtyPage << endl;
             result  << "----------------------------------------" << endl;
    }
}
