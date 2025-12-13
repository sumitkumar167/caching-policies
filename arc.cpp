#include "arc.h"

#include <list>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <fstream>
#include <algorithm>

using namespace std;

struct ARCCache::Impl
{
    int c = 0;     // cache capacity (resident frames)
    int p = 0;     // target size for T1 (recency part)

    // Stats
    long long calls = 0;
    long long hits = 0;
    long long readHits = 0;
    long long writeHits = 0;
    long long evictedDirtyPage = 0;

    // Lists (MRU at front, LRU at back)
    list<long long> T1, T2, B1, B2;

    // Membership maps: key -> iterator
    unordered_map<long long, list<long long>::iterator> posT1, posT2, posB1, posB2;

    // Dirty only for RESIDENT pages (T1/T2)
    unordered_set<long long> dirty;

    static bool isWriteOp(const string& rw) {
        return (rw == "Write" || rw == "write" || rw == "W" || rw == "w");
    }
    static bool isReadOp(const string& rw) {
        return (rw == "Read" || rw == "read" || rw == "R" || rw == "r");
    }

    // ----- Helpers: list ops -----

    void eraseFrom(list<long long>& L, unordered_map<long long, list<long long>::iterator>& pos, long long key) {
        auto it = pos.find(key);
        if (it != pos.end()) {
            L.erase(it->second);
            pos.erase(it);
        }
    }

    void pushFront(list<long long>& L, unordered_map<long long, list<long long>::iterator>& pos, long long key) {
        L.push_front(key);
        pos[key] = L.begin();
    }

    void touchToFront(list<long long>& L, unordered_map<long long, list<long long>::iterator>& pos, long long key) {
        auto it = pos.find(key);
        if (it == pos.end()) return;
        L.erase(it->second);
        L.push_front(key);
        pos[key] = L.begin();
    }

    long long popBack(list<long long>& L, unordered_map<long long, list<long long>::iterator>& pos) {
        if (L.empty()) return -1;
        long long key = L.back();
        L.pop_back();
        pos.erase(key);
        return key;
    }

    int szT1() const { return (int)T1.size(); }
    int szT2() const { return (int)T2.size(); }
    int szB1() const { return (int)B1.size(); }
    int szB2() const { return (int)B2.size(); }

    bool inT1(long long k) const { return posT1.find(k) != posT1.end(); }
    bool inT2(long long k) const { return posT2.find(k) != posT2.end(); }
    bool inB1(long long k) const { return posB1.find(k) != posB1.end(); }
    bool inB2(long long k) const { return posB2.find(k) != posB2.end(); }

    void markDirtyIfWrite(long long k, const string& rw) {
        if (isWriteOp(rw)) dirty.insert(k);
    }

    void countDirtyEvictionIfNeeded(long long k) {
        if (dirty.find(k) != dirty.end()) {
            evictedDirtyPage++;
            dirty.erase(k);
        }
    }

    // ----- ARC core: REPLACE -----
    // Choose victim from T1 or T2 and move to corresponding ghost list.
    void REPLACE(long long x)
    {
        // If T1 has something and (T1 too big) OR (x is in B2 and T1 == p), evict from T1 -> B1
        if (!T1.empty() && (szT1() > p || (inB2(x) && szT1() == p))) {
            long long victim = popBack(T1, posT1);
            if (victim != -1) {
                countDirtyEvictionIfNeeded(victim);
                // move to MRU of B1
                pushFront(B1, posB1, victim);
            }
        } else {
            // else evict from T2 -> B2
            long long victim = popBack(T2, posT2);
            if (victim != -1) {
                countDirtyEvictionIfNeeded(victim);
                pushFront(B2, posB2, victim);
            } else if (!T1.empty()) {
                // fallback safety
                victim = popBack(T1, posT1);
                if (victim != -1) {
                    countDirtyEvictionIfNeeded(victim);
                    pushFront(B1, posB1, victim);
                }
            }
        }
    }

    // Ensure ghost lists don’t exceed 2c total; ARC uses at most 2c ghost entries.
    void trimGhostsIfNeeded()
    {
        // Total tracked entries can grow to 2c (T1+T2+B1+B2 <= 2c ideally)
        // ARC also trims B2 when total == 2c in the “new page” path.
        // Here: just keep B1 and B2 not insane.
        while (szB1() > c) {
            popBack(B1, posB1);
        }
        while (szB2() > c) {
            popBack(B2, posB2);
        }
    }

    // Handle a cache hit
    void onHit(long long k, const string& rw)
    {
        hits++;
        if (isReadOp(rw)) readHits++;
        else if (isWriteOp(rw)) writeHits++;

        if (inT1(k)) {
            // move from T1 to MRU of T2
            eraseFrom(T1, posT1, k);
            pushFront(T2, posT2, k);
        } else {
            // in T2: just move to MRU
            touchToFront(T2, posT2, k);
        }

        markDirtyIfWrite(k, rw);
    }

    // Main ARC access
    void access(long long k, const string& rw)
    {
        calls++;

        // Case 1: hit in T1 or T2
        if (inT1(k) || inT2(k)) {
            onHit(k, rw);
            return;
        }

        // Case 2: k is in B1 (recently evicted from T1)
        if (inB1(k)) {
            int inc = max(1, (szB1() == 0 ? 1 : (szB2() / max(szB1(), 1))));
            p = min(c, p + inc);

            REPLACE(k);
            // move k from B1 to T2 (resident)
            eraseFrom(B1, posB1, k);
            pushFront(T2, posT2, k);
            markDirtyIfWrite(k, rw);
            return;
        }

        // Case 3: k is in B2 (recently evicted from T2)
        if (inB2(k)) {
            int dec = max(1, (szB2() == 0 ? 1 : (szB1() / max(szB2(), 1))));
            p = max(0, p - dec);

            REPLACE(k);
            eraseFrom(B2, posB2, k);
            pushFront(T2, posT2, k);
            markDirtyIfWrite(k, rw);
            return;
        }

        // Case 4: k is new (not in any list)
        // Follow ARC rules about balancing resident + ghosts.

        if (c <= 0) return; // degenerate

        // If |T1| + |B1| == c
        if (szT1() + szB1() == c) {
            if (szT1() < c) {
                // evict LRU from B1, then REPLACE
                popBack(B1, posB1);
                REPLACE(k);
            } else {
                // evict LRU from T1 directly -> B1
                long long victim = popBack(T1, posT1);
                if (victim != -1) {
                    countDirtyEvictionIfNeeded(victim);
                    pushFront(B1, posB1, victim);
                }
            }
        }
        // else if |T1| + |B1| < c and total tracked >= c
        else if (szT1() + szB1() < c) {
            int total = szT1() + szT2() + szB1() + szB2();
            if (total >= c) {
                if (total == 2 * c) {
                    // remove LRU from B2
                    popBack(B2, posB2);
                }
                REPLACE(k);
            }
        }

        // Finally insert into T1 (recency list)
        pushFront(T1, posT1, k);
        markDirtyIfWrite(k, rw);

        trimGhostsIfNeeded();
    }
};

ARCCache::ARCCache(int size)
{
    p = new Impl();
    p->c = max(0, size);
    p->p = 0;
}

ARCCache::~ARCCache()
{
    delete p;
}

void ARCCache::refer(long long int addr, string rw)
{
    p->access(addr, rw);
}

void ARCCache::cacheHitsSummary()
{
    cout << "ARC CacheSize " << p->c << endl;
    cout << " calls " << p->calls << endl;
    cout     << " hits " << p->hits << endl;
    cout     << " hitRatio " << (p->calls > 0 ? (double)p->hits / (double)p->calls : 0.0) << endl;
    cout     << " readHits " << p->readHits << endl;
    cout     << " readHitRatio " << (p->calls > 0 ? (double)p->readHits / (double)p->calls : 0.0) << endl;
    cout     << " writeHits " << p->writeHits << endl;
    cout     << " writeHitRatio " << (p->calls > 0 ? (double)p->writeHits / (double)p->calls : 0.0) << endl;
    cout     << " evictedDirtyPage " << p->evictedDirtyPage << endl;

    ofstream result("ExperimentalResult.txt", ios_base::app);
    if (result.is_open()) {
        result << "ARC " 
            << "CacheSize " << p->c 
               << " calls " << p->calls 
               << " hits " << p->hits 
               << " hitRatio " << (p->calls > 0 ? (double)p->hits / (double)p->calls : 0.0) 
               << " readHits " << p->readHits 
               << " readHitRatio " << (p->calls > 0 ? (double)p->readHits / (double)p->calls : 0.0) 
               << " writeHits " << p->writeHits 
               << " writeHitRatio " << (p->calls > 0 ? (double)p->writeHits / (double)p->calls : 0.0) 
               << " evictedDirtyPage " << p->evictedDirtyPage << endl;
        result.close();
    }
    result.close();
}
