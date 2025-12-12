#ifndef _lirs_H
#define _lirs_H

#include <string>
using namespace std;

class LIRSCache
{
public:
    LIRSCache(int);
    ~LIRSCache();

    void refer(long long int addr, string rw);

    // Match the rest of your framework
    void cacheHitsResult();

private:
    // opaque in header; defined in lirs.cpp
    struct Impl;
    Impl* p;
};

#endif