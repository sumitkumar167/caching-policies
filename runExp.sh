#!/bin/bash 

#hm_0.csv
for policy in Exp
do
        for csize in 703 3516 7031 35156 70313 140626 281251 562502 632815
        do
                ./cache -m $policy -f 2 -i hm_0.csv -s $csize
        done
done


#hm_1.csv
for policy in Exp
do
        for csize in 54 272 544 2720 5440 10881 21762 43523 48964
        do
                ./cache -m $policy -f 2 -i hm_1.csv -s $csize
        done
done


#mds_0.csv
for policy in Exp
do
        for csize in 703 3514 7028 35142 70284 140568 281137 562274 632558
        do
                ./cache -m $policy -f 2 -i mds_0.csv -s $csize
        done
done


#prn_0.csv
for policy in Exp
do
        for csize in 4084 20420 40840 204201 408403 816806 1633612 3267223 3675626
        do
                ./cache -m $policy -f 2 -i prn_0.csv -s $csize
        done
done


#proj_0.csv
for policy in Exp
do
        for csize in 826 4129 8257 41286 82572 165143 330286 660573 743144
        do
                ./cache -m $policy -f 2 -i proj_0.csv -s $csize
        done
done


#proj_3.csv
for policy in Exp
do
        for csize in 1463 7317 14633 73167 146334 292667 585334 1170668 1317002
        do
                ./cache -m $policy -f 2 -i proj_3.csv -s $csize
        done
done


#prxy_0.csv
for policy in Exp
do
        for csize in 243 1215 2430 12148 24295 48590 97180 194360 218655
        do
                ./cache -m $policy -f 2 -i prxy_0.csv -s $csize
        done
done


#rsrch_0.csv
for policy in Exp
do
        for csize in 107 537 1073 5367 10733 21467 42933 85866 96600
        do
                ./cache -m $policy -f 2 -i rsrch_0.csv -s $csize
        done
done


#src1_2.csv
for policy in Exp
do
        for csize in 501 2503 5005 25026 50053 100106 200212 400423 450476
        do
                ./cache -m $policy -f 2 -i src1_2.csv -s $csize
        done
done


#src2_0.csv
for policy in Exp
do
        for csize in 224 1120 2239 11196 22391 44783 89565 179130 201522
        do
                ./cache -m $policy -f 2 -i src2_0.csv -s $csize
        done
done


#stg_0.csv
for policy in Exp
do
        for csize in 1447 7234 14468 72340 144680 289359 578718 1157436 1302116
        do
                ./cache -m $policy -f 2 -i stg_0.csv -s $csize
        done
done


#ts_0.csv
for policy in Exp
do
        for csize in 277 1384 2767 13837 27675 55350 110699 221398 249073
        do
                ./cache -m $policy -f 2 -i ts_0.csv -s $csize
        done
done


#usr_0.csv
for policy in Exp
do
        for csize in 645 3227 6453 32267 64535 129070 258139 516278 580813
        do
                ./cache -m $policy -f 2 -i usr_0.csv -s $csize
        done
done


#wdev_0.csv
for policy in Exp
do
        for csize in 156 779 1557 7785 15570 31140 62280 124561 140131
        do
                ./cache -m $policy -f 2 -i wdev_0.csv -s $csize
        done
done


#wdev_1.csv
for policy in Exp
do
        for csize in 1 2 3 16 33 66 131 262 295
        do
                ./cache -m $policy -f 2 -i wdev_1.csv -s $csize
        done
done


#wdev_2.csv
for policy in Exp
do
        for csize in 21 107 214 1068 2135 4271 8542 17083 19219
        do
                ./cache -m $policy -f 2 -i wdev_2.csv -s $csize
        done
done


#wdev_3.csv
for policy in Exp
do
        for csize in 1 1 3 15 30 60 119 238 268
        do
                ./cache -m $policy -f 2 -i wdev_3.csv -s $csize
        done
done


#web_0.csv
for policy in Exp
do
        for csize in 1735 8674 17348 86742 173483 346967 693934 1387867 1561351
        do
                ./cache -m $policy -f 2 -i web_0.csv -s $csize
        done
done


#web_3.csv
for policy in Exp
do
        for csize in 138 688 1376 6880 13760 27521 55041 110082 123843
        do
                ./cache -m $policy -f 2 -i web_3.csv -s $csize
        done
done

