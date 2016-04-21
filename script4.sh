#!/bin/csh


set injectionrate = 2

while ($injectionrate <= 40)

  set injection = `echo "$injectionrate * (1/100)" | bc -l`

  ./build/ALPHA_Network_test/gem5.debug configs/example/ruby_network_test.py --network=garnet2.0 --num-cpus=4 --num-dirs=4 --topology=GoogleFatTree --num-rows=0 --sim-cycles=100000 --injectionrate=$injection --synthetic=0 --routing-algorithm=0

   @ injectionrate++
   @ injectionrate++

#./my_scripts/build_Network_test.sh
  ./my_scripts/extract_network_stats.sh


   grep "average_packet_latency" network_stats.txt | sed 's/average_packet_latency = //' >> GoogleFatTree.txt
end


