#!/bin/bash 

#mds_1.csv
for policy in LRU LFU LIRS ARC CACHEUS
do
        for csize in 703 3514 7028 35142 70284 140568 281137 562274 632558
        do
                ./cache -m $policy -f 2 -i mds_1.csv -s $csize
        done
done

#prn_0.csv
for policy in LRU LFU LIRS ARC CACHEUS
do
        for csize in 703 3514 7028 35142 70284 140568 281137 562274 632558
        do
                ./cache -m $policy -f 2 -i prn_0.csv -s $csize
        done
done

#hm_1.csv
for policy in LRU LFU LIRS ARC CACHEUS
do
        for csize in 703 3514 7028 35142 70284 140568 281137 562274 632558
        do
                ./cache -m $policy -f 2 -i hm_1.csv -s $csize
        done
done

#mds_0.csv
for policy in LRU LFU LIRS ARC CACHEUS
do
        for csize in 703 3514 7028 35142 70284 140568 281137 562274 632558
        do
                ./cache -m $policy -f 2 -i mds_0.csv -s $csize
        done
done