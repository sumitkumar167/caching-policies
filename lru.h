/* 
We can use stl container list as a double
ended queue to store the cache keys, with
the descending time of reference from front
to back and a set container to check presence
of a key. But to fetch the address of the key
in the list using find(), it takes O(N) time.
This can be optimized by storing a reference
(iterator) to each key in a hash map. 
*/
#include <string.h>
using namespace std; 
#ifndef _lru_H
#define _lru_H

class LRUCache
{
	// store keys of cache 
	std::list<long long int> dq;

	// store references of key in cache
	// note: using std::unordered_map to decrease average search time to O(1) 
	// std::unordered_map is implemented as Hash Table
	std::unordered_map<long long int, std::list<long long int>::iterator> ma;
	int csize; //maximum capacity of cache 


	// record read-write type of cached page
	std::unordered_map<long long int, string> accessType; 
	


	//count cache hits
	long long int calls, total_calls;
	long long int hits, total_hits;

	long long int readHits; 
	long long int writeHits; 
	long long int evictedDirtyPage; 


	long long int migration, total_migration;

public:
	LRUCache(int);
	~LRUCache();
	void refer(long long int, string);
	void display();

	// summary results
	void cachehits();

	void refresh();
	void summary();

};
#endif
