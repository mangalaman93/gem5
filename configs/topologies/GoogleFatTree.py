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

class GoogleFatTree(SimpleTopology):
    description='GoogleFatTree'

    def __init__(self, controllers):
        self.nodes = controllers

    # Makes a generic mesh assuming an equal number of cache and directory cntrls
    def makeTopology(self, options, network, IntLink, ExtLink, Router):
        nodes = self.nodes

        k = options.num_cpus
        #print "cpu " + str(cpu)
        #k = (4*options.num_cpus)**(1./3.)
	
        #print "k " + str(k) 
	#assert( k**3/4 = options.num_cpus);

        assert((k/2) * 2 == k);

        # K**3/4 - number of hosts (one host per router)
        # k*k    - aggregation routers
        # k*k/4  - core routers
        num_nodes = k**3/4
        num_routers = int(num_nodes + k*k + k*k/4)
        #print "num_routers " + str(num_routers) 
        # There must be an evenly divisible number of cntrls to routers
        # Also, obviously the number or rows must be <= the number of routers
        cntrls_per_router, remainder = divmod(num_routers, num_routers)

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

        # Connect each node to the appropriate router
        ext_links = []
        for (i, n) in enumerate(network_nodes):
            cntrl_level, router_id = divmod(i, num_routers)
            assert(cntrl_level < cntrls_per_router)
            ext_links.append(ExtLink(link_id=link_count, ext_node=n,
                                    int_node=routers[router_id]))
            link_count += 1

        # Connect the remainding nodes to router 0.  These should only be
        # DMA nodes.
        for (i, node) in enumerate(remainder_nodes):
            assert(node.type == 'DMA_Controller')
            assert(i < remainder)
            ext_links.append(ExtLink(link_id=link_count, ext_node=node,
                                    int_node=routers[0]))
            link_count += 1

        network.ext_links = ext_links

        # pod links
        int_links = []
        for pod_index in xrange(0, k):  # iterating over all pods
            for edge_index in xrange(0, k/2):  # iterting over all Edge layer switches in a pod
                edge_router_id = num_nodes + pod_index*k + edge_index
                for host_index in xrange(0, k/2):  # connecting host routers to edge layer
                    host_router_id = pod_index*(k*k/4) + edge_index*k/2 + host_index
                    int_links.append(IntLink(link_id=link_count,
                                             node_a=routers[edge_router_id],
                                             node_b=routers[host_router_id],
					     latency=10,
                                             weight=1))
                    link_count += 1
                    #print "Router " + str(edge_router_id) + " created a link to " + str(host_router_id)

                for aggr_index in xrange(0, k/2):
                    aggr_router_id = num_nodes + pod_index*k + k/2 + aggr_index
                    int_links.append(IntLink(link_id=link_count,
                                             node_a=routers[edge_router_id],
                                             node_b=routers[aggr_router_id],
					     latency=10,
                                             weight=1))
                    link_count += 1
                    #print "Router " + str(edge_router_id) + " created a link to " + str(aggr_router_id)

            for aggr_index in xrange(0, k/2):
                aggr_router_id = num_nodes + pod_index*k + k/2 + aggr_index
                for core_index in xrange(0, k/2):
                    core_router_id = k**3/4 + k*k + aggr_index*k/2 + core_index
                    int_links.append(IntLink(link_id=link_count,
                                             node_a=routers[aggr_router_id],
                                             node_b=routers[core_router_id],
					     latency=10,
                                             weight=1))
                    link_count += 1
                    #print "Router " + str(aggr_router_id) + " created a link to " + str(core_router_id)

        network.int_links = int_links
