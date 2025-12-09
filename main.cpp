//#include <bits/stdc++.h> 
#include <list>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <ctime>
#include <stdio.h>
#include <string.h>
#include "lru.h"
#include "lfu.h"
//#include "arc.h"
//#include "mq.h"
//#include "mru.h"
//#include "lecar.h"
//#include "harc.h"
//#include "exp.h"
#include <math.h>
#define CACHESIZE 1 // in GB
static const char* pgmname;
using namespace std; 

void usage()
{
	fprintf(stderr,
		"Usage: %s -m <cache policy> -i <1:TCP/2:MSR> -f <filename>\n\n\
		-m <cache policy>  LRU, MRU, LFU, MQ, ARC, LeCar, Exp ...\n\
		-f <TCP or MSR> 1: TPC 2: MSR traces\n\
		-i <filename> \n\
		-s <cacheSize> \n\
		", pgmname);
	exit(1);
}


int main(int argc, char* argv[])
{

	//HARCCache ca(10);
	
	//note: the block size is 512bytes
	//int start_s = clock();
	// initialize the cache table, suppose the cache size is 30GB, the "csize" = 30GB / 16KB (each page is 16KB) =  
	//LRUCache ca(CACHESIZE*1024*1024*2);
	srand((unsigned int)time(NULL));


	// read timestamp
	double timestamp2;
	// read key (or offset or address)
	long long int key;
	int j = 0;
	pgmname = argv[j++];
	string cache_policy;
	int trace_type = 0;
	char AccessPattern;
	char* filename;
	std::string operation = "Write";

	long long int timestamp;
	string device;
	int disk;
	string rwtype;
	long long int offset;
	int size;

	int csize;

	bool LRU = false;
	bool MRU = false; 
	bool LFU = false; 
	bool MQ = false;
	bool ARC = false;
	bool LeCaR = false; 
	bool HARC = false;
	bool Exp = false;


	string temp1;
	string temp2;
	string temp3;
	string temp4;
	string temp5;
	string temp6;
	string temp7;
	// open input file
	if(j >= argc)
	{
	usage();
	}

	while(j < argc)
	{
		if(strcmp(argv[j], "-m") == 0)
		{
		    if(++ j >= argc)
		    {
			fprintf(stderr, "Input caching policy\n");
			usage();
		    }
		    cache_policy = argv[j++];
		    if(cache_policy == "LRU") LRU = true;    //LRUCache ca(CACHESIZE*1024*1024*2);
		    else if(cache_policy == "LFU") LFU = true; //LFUCache ca(CACHESIZE*1024*1024*2);
		    else if(cache_policy == "ARC") ARC = true; //ARCache ca(CACHESIZE*1024*1024*2);
		    else if(cache_policy == "MRU")  MRU = true; //MRUCache ca(CACHESIZE*1024*1024*2);
		    else if(cache_policy == "MQ") MQ = true; //MQCache ca(CACHESIZE*1024*1024*2);
		    else if(cache_policy == "LeCaR") LeCaR = true;//LeCaRCache ca(CACHESIZE*1024*1024*2);
		    else if(cache_policy == "HARC") HARC = true;//LeCaRCache ca(CACHESIZE*1024*1024*2);
		    else if(cache_policy == "Exp") Exp = true;//ExpCache ca(CACHESIZE*1024*1024*2);
		    else{
			fprintf(stderr, "Wrong cache type\n");
			usage();
		    }
		}
		else{
		    if(strcmp(argv[j], "-f") == 0)
		    {
			if(++ j >= argc)
			{
			    fprintf(stderr, "1: TPC-H  2: MSR traces\n");
			    usage();
			}

			trace_type = atoi(argv[j++]);
		    }
		    else{
			if(strcmp(argv[j], "-i") == 0)
			{
			    if(++ j >= argc)
			    {
				fprintf(stderr, "supply number of warmup IOs to -w\n");
				usage();
			    }
			    filename = argv[j++];
			    //break;
			} else if (strcmp(argv[j], "-s") == 0) {

				if(++ j >= argc)
				{
				    fprintf(stderr, "miss cache size\n");
				    usage();
				}
				csize = atoi(argv[j++]);
				break;

			} else{
			    fprintf(stderr, "missing option\n");
			    usage();
			    
			}
		    }
		}
	}


	std::ofstream result("ExperimentalResult.txt", std::ios_base::app);
	if (result.is_open()) { 
		result <<  filename << " ";			
	}
	result.close();


	std::ifstream myfile(filename);
	// check the open is succeeded
	std::cout <<"File: "<< filename<< " "<<"Policy: "<<cache_policy<< "  " <<"Cache size: "<< csize <<std::endl;
	int count = 0;
	//LRU example
	if(LRU){


		LRUCache ca(csize);
		if (myfile.is_open()) {
			while(!myfile.eof())
			{
			    
				if(trace_type == 2){  // for MSR traces                
					getline(myfile,temp1,','); //timestamp
					getline(myfile,temp2,',');  //device
					getline(myfile,temp3,',');  //disk
					getline(myfile,temp4,','); //read or write
					getline(myfile,temp5,',');  //offset   
					getline(myfile,temp6,',');  //request size
					getline(myfile,temp7);  //temp
					if (!temp1.empty()){
					   
					    timestamp = std::stoll(temp1);
					    
					    device = temp2;
					    disk = std::stoi(temp3);
					    rwtype = temp4;
					    offset = std::stoll(temp5);;
					    size = std::stoi(temp6);
					    //temp7 = std::stoi(temp7);
					    
					    //request unit: 0.5KB 
					    for(int i = 0; i< (int)ceil(size/(4.0*1024)); i ++){
						ca.refer(offset + i*1024*4, rwtype); 
					    }
					    //std::cout << timestamp<< " "<< device<<" "<< disk<<" "<< rwtype<<" " <<offset<<" " <<size<<" " <<temp7<< std::endl;
					    //std::cout << count<< " " << temp1<< " "<< temp2<<" "<< temp3<<" "<< temp4<<" "<<temp5<<" "<<temp6<<" "<<temp7<< std::endl;
					    count = count + 1; 
					    
					    
					}
			       
					else{    // for TPC-H traces
						while (myfile >> timestamp2 >> key >> AccessPattern) {
							ca.refer(key, rwtype);
						}
					}

			}
		}else{
		std::cerr << "error: unable to open input file" << std::endl;
			return -1;
		}


		// print cache hit

		ca.cachehits();
		std::cout << std::endl;
		// close the input file 
		myfile.close();
	}
	else if (LFU) {
		//std::ifstream myfile(filename);
		LFUCache lfu_cache(csize);

		if (myfile.is_open()) {
			while (!myfile.eof()) {
				if (trace_type == 2) {  // for MSR traces
					getline(myfile, temp1, ','); //timestamp
					getline(myfile, temp2, ','); //device
					getline(myfile, temp3, ','); //disk
					getline(myfile, temp4, ','); //read or write
					getline(myfile, temp5, ','); //offset
					getline(myfile, temp6, ','); //request size
					getline(myfile, temp7); //temp
					if (!temp1.empty()) {
						timestamp = std::stoll(temp1);
						device = temp2;
						disk = std::stoi(temp3);
						rwtype = temp4;
						offset = std::stoll(temp5);
						size = std::stoi(temp6);
						//temp7 = std::stoi(temp7);

						//request unit: 0.5KB
						for (int i = 0; i < (int)ceil(size / (4.0 * 1024)); i++) {
							lfu_cache.refer(offset + i * 1024 * 4, rwtype);
						}
						count = count + 1;
					}
				}
				else {    // for TPC-H traces
					while (myfile >> timestamp2 >> key >> AccessPattern) {
						// set rwtype based on AccessPattern
						// if (AccessPattern == 'R' || AccessPattern == 'r') rwtype = "Read";
						// else rwtype = "Write";
						lfu_cache.refer(key, rwtype);
					}
				}
			}
		}
		else {
			std::cerr << "error: unable to open input file" << std::endl;
			return -1;
		}

		// print cache hit
		lfu_cache.cacheHits();
		std::cout << std::endl;
		// close the input file
		myfile.close();
	}
	else{

		std::cerr << "cannot find a proper cache policy" << std::endl;
	}


	return 0;
}
