#include "lirs.h"
#include <list>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>

using namespace std;

struct LIRSCache::Impl {

    int csize;
    int hirCap;
    int lirTarget;

    long long calls = 0;
    long long hits = 0;
    long long readHits = 0;
    long long writeHits = 0;
    long long evictedDirtyPage = 0;

    // Stack S (MRU front)
    list<long long> S;
    unordered_map<long long, list<long long>::iterator> Spos;

    // Queue Q (resident HIR only)
    list<long long> Q;
    unordered_map<long long, list<long long>::iterator> Qpos;

    list<long long> L;
    unordered_map<long long, list<long long>::iterator> Lpos;

    struct PageInfo {
        bool isLIR = false;
        bool resident = false;
        bool dirty = false;
    };
    unordered_map<long long, PageInfo> page;

    int residentCount = 0;
    int lirCount = 0;

    static bool isWrite(const string& rw) {
        return (rw == "Write" || rw == "write" || rw == "W" || rw == "w");
    }
    static bool isRead(const string& rw) {
        return (rw == "Read" || rw == "read" || rw == "R" || rw == "r");
    }

    void moveToTopS(long long k) {
        auto it = Spos.find(k);
        if (it != Spos.end()) S.erase(it->second);
        S.push_front(k);
        Spos[k] = S.begin();
    }

    void moveToTopL(long long k) {
        auto it = Lpos.find(k);
        if (it != Lpos.end()) L.erase(it->second);
        L.push_front(k);
        Lpos[k] = L.begin();
    }

    void pushFrontQ(long long k) {
        auto it = Qpos.find(k);
        if (it != Qpos.end()) Q.erase(it->second);
        Q.push_front(k);
        Qpos[k] = Q.begin();
    }

    void removeFromQ(long long k) {
        auto it = Qpos.find(k);
        if (it != Qpos.end()) {
            Q.erase(it->second);
            Qpos.erase(it);
        }
    }

    void pruneS() {
        while (!S.empty()) {
            long long b = S.back();
            auto pit = page.find(b);
            if (pit == page.end() || (!pit->second.isLIR && !pit->second.resident)) {
                Spos.erase(b);
                S.pop_back();
                Lpos.erase(b);
                page.erase(b);
            } else break;
        }
    }

    void demoteOneLIR() {
        if (L.empty()) return;
        long long victim = L.back();
        L.pop_back();
        Lpos.erase(victim);

        PageInfo &info = page[victim];
        info.isLIR = false;
        lirCount--;

        pushFrontQ(victim);
    }

    void evictHIR() {
        if (Q.empty()) return;
        long long victim = Q.back();
        Q.pop_back();
        Qpos.erase(victim);

        PageInfo &info = page[victim];
        if (info.dirty) evictedDirtyPage++;
        info.resident = false;
        info.dirty = false;

        residentCount--;
    }

    void onHit(long long k, const string& rw) {
        hits++;
        if (isRead(rw)) readHits++;
        else if (isWrite(rw)) writeHits++;

        PageInfo &info = page[k];
        if (isWrite(rw)) info.dirty = true;

        moveToTopS(k);

        if (info.isLIR) {
            moveToTopL(k);
        } else {
            removeFromQ(k);
            info.isLIR = true;
            lirCount++;
            moveToTopL(k);
            demoteOneLIR();
        }
        pruneS();
    }

    void onMiss(long long k, const string& rw) {
        bool write = isWrite(rw);

        PageInfo &info = page[k];
        bool seenBefore = (Spos.find(k) != Spos.end());

        if (residentCount >= csize) evictHIR();

        info.resident = true;
        residentCount++;
        info.dirty = write;

        moveToTopS(k);

        if (seenBefore) {
            info.isLIR = true;
            lirCount++;
            moveToTopL(k);
            demoteOneLIR();
        } else if (lirCount < lirTarget) {
            info.isLIR = true;
            lirCount++;
            moveToTopL(k);
        } else {
            info.isLIR = false;
            pushFrontQ(k);
        }

        pruneS();
    }
};

/* ================== PUBLIC ================== */

LIRSCache::LIRSCache(int size) {
    p = new Impl();
    p->csize = size;

    if (size <= 1) {
        p->hirCap = 1;
        p->lirTarget = 0;
    } else {
        p->hirCap = max(1, (int)ceil(size * 0.01));
        p->hirCap = min(p->hirCap, size - 1);
        p->lirTarget = size - p->hirCap;
    }

    // Reserve maps for speed
    p->page.reserve(size * 2);
    p->Spos.reserve(size * 2);
    p->Qpos.reserve(size);
    p->Lpos.reserve(size);
}

LIRSCache::~LIRSCache() { delete p; }

void LIRSCache::refer(long long int addr, string rw) {
    p->calls++;

    auto it = p->page.find(addr);
    if (it != p->page.end() && it->second.resident) {
        p->onHit(addr, rw);
    } else {
        p->onMiss(addr, rw);
    }
}

void LIRSCache::cacheHitsResult() {
    cout << "LIRS CacheSize " << p->csize
         << " calls " << p->calls
         << " hits " << p->hits
         << " hitRatio " << (p->calls ? (double)p->hits / p->calls : 0.0)
         << " readHits " << p->readHits
         << " readHitRatio " << (p->calls ? (double)p->readHits / p->calls : 0.0)
         << " writeHits " << p->writeHits
         << " writeHitRatio " << (p->calls ? (double)p->writeHits / p->calls : 0.0)
         << " evictedDirtyPage " << p->evictedDirtyPage
         << endl;

    ofstream out("ExperimentalResult.txt", ios::app);
    if (out.is_open()) {
        out << "LIRS CacheSize " << p->csize
            << " calls " << p->calls
            << " hits " << p->hits
            << " hitRatio " << (p->calls ? (double)p->hits / p->calls : 0.0)
            << " readHits " << p->readHits
            << " writeHits " << p->writeHits
            << " evictedDirtyPage " << p->evictedDirtyPage
            << "\n";
    }
}
