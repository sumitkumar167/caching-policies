#ifndef _arc_H
#define _arc_H

#include <string>
using namespace std;

class ARCCache
{
public:
    ARCCache(int);
    ~ARCCache();

    void refer(long long int addr, string rw);

    void cacheHitsSummary();

private:
    struct Impl;
    Impl* p;
};

#endif