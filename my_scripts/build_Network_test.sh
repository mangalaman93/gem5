#!/bin/tcsh
source my_scripts/set_env.cshrc
#scons -j 4 build/ALPHA_Network_test/gem5.debug
python `which scons` -j 4 build/ALPHA_Network_test/gem5.debug
