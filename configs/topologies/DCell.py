# Copyright (c) 2010 Advanced Micro Devices, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Authors: Brad Beckmann

from m5.params import *
from m5.objects import *

from BaseTopology import SimpleTopology

class DCell(SimpleTopology):
    description='DCell'

    def __init__(self, controllers):
        self.nodes = controllers

    # Makes a generic mesh assuming an equal number of cache and directory cntrls

    def makeTopology(self, options, network, IntLink, ExtLink, Router):
        nodes = self.nodes

        
        #num_rows = options.num_rows
	num_of_DCell_modules = 5
        num_of_servers_DCell0 = 4

        if (options.num_cpus == 6):
	    num_of_DCell_modules = 3
            num_of_servers_DCell0 = 2

        if (options.num_cpus == 2):
	    num_of_DCell_modules = 2
            num_of_servers_DCell0 = 1

        if (options.num_cpus == 20):
	    num_of_DCell_modules = 5
            num_of_servers_DCell0 = 4

        if (options.num_cpus == 30):
	    num_of_DCell_modules = 6
            num_of_servers_DCell0 = 5

        if (options.num_cpus == 42):
	    num_of_DCell_modules = 7
            num_of_servers_DCell0 = 6

        #g = num_of_servers_DCell0 * num_of_DCell_modules
        num_routers = options.num_cpus + num_of_DCell_modules

        # There must be an evenly divisible number of cntrls to routers
        # Also, obviously the number or rows must be <= the number of routers
        cntrls_per_router, remainder = divmod(len(nodes), options.num_cpus)   #[ICN Project]
        #cntrls_per_router, remainder = divmod(len(nodes), num_routers) 
        #assert(num_rows <= num_routers)
        #num_columns = int(num_routers / num_rows)
        #assert(num_columns * num_rows == num_routers)

        # Create the routers in the mesh
        routers = [Router(router_id=i) for i in range(num_routers)]
        network.routers = routers

        # link counter to set unique link ids
        link_count = 0

        # Add all but the remainder nodes to the list of nodes to be uniformly
        # distributed across the network.
        network_nodes = []
        remainder_nodes = []
        for node_index in xrange(len(nodes)):
            if node_index < (len(nodes) - remainder):
                network_nodes.append(nodes[node_index])
            else:
                remainder_nodes.append(nodes[node_index])
        #print "rem " + str(len(remainder_nodes))
        # Connect each node to the appropriate router
        ext_links = []
        for (i, n) in enumerate(network_nodes):
            cntrl_level, router_id = divmod(i, options.num_cpus)    #[ICN Project]
            #cntrl_level, router_id = divmod(i, num_routers) 
            assert(cntrl_level < cntrls_per_router)
            ext_links.append(ExtLink(link_id=link_count, ext_node=n,
                                    int_node=routers[router_id]))
            #print "link_count " + str(link_count)
            link_count += 1
            #print "node " + str(n) + " created a link to " + str(router_id)

        #print "network_nodes " + str(len(network_nodes))
        #print "options.num_cpus" + str(options.num_cpus)
         #Connect the remainding nodes to router 0.  These should only be
         #DMA nodes.
        for (i, node) in enumerate(remainder_nodes):
            #assert(node.type == 'DMA_Controller')		[ICN Project]
            assert(i < remainder)
            ext_links.append(ExtLink(link_id=link_count, ext_node=node,
                                    int_node=routers[0]))
            link_count += 1

        network.ext_links = ext_links

        int_links = []
        for i in range(0,num_of_DCell_modules):
            for j in range(0, num_of_servers_DCell0):
                 int_links.append(IntLink(link_id=link_count,
                                            node_a=routers[num_of_servers_DCell0*i + j],
                                            node_b=routers[options.num_cpus+i],
                                            latency=10,                                    
                                            weight=1))
                 #print "link_count " + str(link_count)                
                 link_count += 1
                 #print "i " + str(i) 
                 #print "j " + str(j)
                 #print "Router " + str(num_of_servers_DCell0*i + j) + " created a link to " + str(options.num_cpus+i)
                 
        if (num_of_DCell_modules > 1):
	  for i in range(0,num_of_DCell_modules+1):
	      for j in range(0, num_of_servers_DCell0+1):                 
		  if(j>i):
		      int_links.append(IntLink(link_id=link_count,
                                           node_a=routers[num_of_servers_DCell0*i + j-1],
                                            node_b=routers[num_of_servers_DCell0*j + i],
                                            latency=10,                                    
                                            weight=1))
                      #print "link_count " + str(link_count)
		      link_count += 1
		 #     #print "i " + str(i) 
		     #print "j " + str(j)
		      #print "Router " + str(num_of_servers_DCell0*i + j-1) + " created a link to " + str(num_of_servers_DCell0*j + i)
                      
                

        network.int_links = int_links


