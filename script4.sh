#!/bin/csh

set injectionrate = 2

while ($injectionrate <= 150)

  set injection = `echo "$injectionrate * (1/100)" | bc -l`

  ./build/ALPHA_Network_test/gem5.debug configs/example/ruby_network_test.py --network=garnet2.0 --num-cpus=16 --num-dirs=16 --topology=GoogleFatTree_m --num-rows=0 --sim-cycles=10000 --injectionrate=$injection --synthetic=1 --vcs-per-vnet=100 --routing-algorithm=0 

    @ injectionrate = $injectionrate + 5
#./my_scripts/build_Network_test.sh
  ./my_scripts/extract_network_stats.sh

   grep "average_packet_latency" network_stats.txt | sed 's/average_packet_latency = //' >> Google_lat_tornado.txt
end

set injectionrate = 2

while ($injectionrate <= 150)

  set injection = `echo "$injectionrate * (1/100)" | bc -l`

  ./build/ALPHA_Network_test/gem5.debug configs/example/ruby_network_test.py --network=garnet2.0 --num-cpus=16 --num-dirs=16 --topology=GoogleFatTree_m --num-rows=0 --sim-cycles=10000 --injectionrate=$injection --synthetic=2 --vcs-per-vnet=100 --routing-algorithm=0 

    @ injectionrate = $injectionrate + 5
#./my_scripts/build_Network_test.sh
  ./my_scripts/extract_network_stats.sh

   grep "average_packet_latency" network_stats.txt | sed 's/average_packet_latency = //' >> Google_lat_bitcomp.txt
end

set injectionrate = 2

while ($injectionrate <= 150)

  set injection = `echo "$injectionrate * (1/100)" | bc -l`

  ./build/ALPHA_Network_test/gem5.debug configs/example/ruby_network_test.py --network=garnet2.0 --num-cpus=16 --num-dirs=16 --topology=GoogleFatTree_m --num-rows=0 --sim-cycles=10000 --injectionrate=$injection --synthetic=3 --vcs-per-vnet=100 --routing-algorithm=0 

    @ injectionrate = $injectionrate + 5
#./my_scripts/build_Network_test.sh
  ./my_scripts/extract_network_stats.sh

   grep "average_packet_latency" network_stats.txt | sed 's/average_packet_latency = //' >> Google_lat_bitrev.txt
end

#set injectionrate = 2
