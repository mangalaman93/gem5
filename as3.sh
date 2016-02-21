#!/bin/bash

MAX=50
OFOLDER=out

ofile=${OFOLDER}/as3.txt
for((routing=1; routing<=3; routing+=1)); do
    dfile=${OFOLDER}/pr${routing}.txt
    echo "######### ROUTING ALGORITHM: ${routing} ) #########" >> ${ofile}
    for((rate=2; rate<=${MAX}; rate+=2)); do
        ./build/ALPHA_Network_test/gem5.debug configs/example/ruby_network_test.py --network=garnet2.0 --num-cpus=64 --num-dirs=64 --topology=Mesh --num-rows=8 --sim-cycles=100000 --injectionrate=$(bc <<<"scale=2; ${rate}/100") --synthetic=0 --vcs-per-vnet=8 --routing-algorithm=${routing}
        ./my_scripts/extract_network_stats.sh
        cat network_stats.txt >> ${ofile}
        grep "packets_received" network_stats.txt | sed 's/packets_received = //' >> ${dfile}
        rm network_stats.txt
    done
    echo "" >> ${ofile}
done
