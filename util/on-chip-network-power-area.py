# Copyright (c) 2014 Mark D. Hill and David A. Wood
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


from ConfigParser import ConfigParser
import string, sys, subprocess, os
import re

# Compile DSENT to generate the Python module and then import it.
# This script assumes it is executed from the gem5 root.
print("Attempting compilation")
from subprocess import call

src_dir = 'ext/dsent'
build_dir = 'build/ext/dsent'

if not os.path.exists(build_dir):
    os.makedirs(build_dir)
os.chdir(build_dir)

error = call(['cmake', '../../../%s' % src_dir])
if error:
    print("Failed to run cmake")
    exit(-1)

error = call(['make'])
if error:
    print("Failed to run make")
    exit(-1)

print("Compiled dsent")
os.chdir("../../../")
sys.path.append("build/ext/dsent")
import dsent

# Parse gem5 config.ini file for the configuration parameters related to
# the on-chip network.
def parseConfig(config_file):
    config = ConfigParser()
    if not config.read(config_file):
        print("ERROR: config file '", config_file, "' not found")
        sys.exit(1)

    if not config.has_section("system.ruby.network"):
        print("ERROR: Ruby network not found in '", config_file)
        sys.exit(1)

    if config.get("system.ruby.network", "type") != "GarnetNetwork" :
        print("ERROR: Garnet network not used in '", config_file)
        sys.exit(1)

    routers = config.get("system.ruby.network", "routers").split()
    int_links = config.get("system.ruby.network", "int_links").split()
    ext_links = config.get("system.ruby.network", "ext_links").split()

    # Network Configs
    network_config = {}

    network_config['num_vnet'] = \
        config.getint("system.ruby.network", "number_of_virtual_networks")

    network_config['flit_size_bits'] = \
        8 * config.getint("system.ruby.network", "ni_flit_size")

    # TODO: this should be a per router parameter:
    network_config['buffers_per_data_vc'] = \
        config.getint("system.ruby.network", "buffers_per_data_vc")

    network_config['buffers_per_ctrl_vc'] = \
        config.getint("system.ruby.network", "buffers_per_ctrl_vc")

    ## Update technology node
    tech_model_path = "ext/dsent/tech/tech_models/"
    tech = sys.argv[5]

    if (tech == "45" or tech == "32" or tech == "22"):
        network_config['tech_model'] = \
            tech_model_path + "Bulk" + tech + "LVT.model"
    elif (tech == "11"):
        network_config['tech_model'] = \
            tech_model_path + "TG" + tech + "LVT.model"
    else:
        print "Unknown Tech Model. Supported models: 45, 32, 22, 11"
        exit(-1)

    return (config, network_config, routers, int_links, ext_links)

def getRouterConfig(config, router, int_links, ext_links):

    router_id = int(router.partition("routers")[2])
    num_ports = 0

    for int_link in int_links:
        if config.get(int_link, "node_a") == router or \
           config.get(int_link, "node_b") == router:
           num_ports += 1

    for ext_link in ext_links:
        if config.get(ext_link, "int_node") == router:
           num_ports += 1

    router_config = {}
    router_config['router_id']      = router_id
    router_config['num_inports']    = num_ports
    router_config['num_outports']   = num_ports

    router_config['vcs_per_vnet'] = \
        config.getint(router, "vcs_per_vnet")

    # add buffers_per_ctrl_vc and buffers_per_data_vc her

    # FIXME: Clock period units are ns in tester, and ps in full-system
    # Make it consistent in gem5
    clock_period = getClock(router, config)
    frequency = 1e12 / float(clock_period)
    router_config['frequency'] = int(frequency)

    return router_config

def getLinkConfig(config, link):

    link_config = {}

    # Frequency (Hz)
    clock_period = getClock(link + ".nls0", config)
    frequency = 1e12 / float(clock_period)
    link_config['frequency'] = int(frequency)

    # Length (m)
    # FIXME: will be part of topology file and appear in config.ini
    length = 4e-3
    link_config['length'] = float(length)

    # Delay (s)
    # Delay of the wire need not be 1.0 / Frequency
    # wire could run faster
    link_config['delay'] = float(1 / frequency)

    return link_config

def parseNetworkStats(stats_file):

    try:
        lines = open(stats_file, 'r')
    except IOError:
        print("Failed to open ", stats_file, " for reading")
        exit(-1)

    network_stats = {}

    for line in lines:
        if re.match("sim_seconds", line):
            network_stats['sim_seconds'] = float(re.split('\s+', line)[1])
        if re.match("sim_ticks", line):
            network_stats['sim_ticks'] = int(re.split('\s+', line)[1])
        if re.match("sim_freq", line):
            network_stats['sim_freq'] = float(re.split('\s+', line)[1])

    lines.close()
    return network_stats

def parseRouterStats(stats_file, router):

    try:
        lines = open(stats_file, 'r')
    except IOError:
        print("Failed to open ", stats_file, " for reading")
        exit(-1)

    router_stats = {}
    for line in lines:
        if re.match(router, line):
            if re.search("buffer_writes", line):
                router_stats['buffer_writes'] = int(re.split('\s+', line)[1])
            if re.search("buffer_reads", line):
                router_stats['buffer_reads'] = int(re.split('\s+', line)[1])
            if re.search("crossbar_activity", line):
                router_stats['crossbar_activity'] = int(re.split('\s+', line)[1])
            if re.search("sw_input_arbiter_activity", line):
                router_stats['sw_in_arb_activity'] = int(re.split('\s+', line)[1])
            if re.search("sw_output_arbiter_activity", line):
                router_stats['sw_out_arb_activity'] = int(re.split('\s+', line)[1])

    return router_stats

def parseLinkStats(stats_file, sim_ticks):

    try:
        lines = open(stats_file, 'r')
    except IOError:
        print("Failed to open ", stats_file, " for reading")
        exit(-1)

    link_stats = {}
    for line in lines:
        if re.search("avg_link_utilization", line):
            link_stats['activity'] = \
                int(float(re.split('\s+', line)[1]) * sim_ticks)

    return link_stats

def getClock(obj, config):

    # Use clock period specified from command line
    # if available
    if (len(sys.argv) > 6):
        clock = sys.argv[6]
        return clock

    # Use clock period specified in config.ini
    if config.get(obj, "type") == "SrcClockDomain":
        return config.getint(obj, "clock")

    if config.get(obj, "type") == "DerivedClockDomain":
        source = config.get(obj, "clk_domain")
        divider = config.getint(obj, "clk_divider")
        return getClock(source, config)  / divider

    source = config.get(obj, "clk_domain")
    return getClock(source, config)


## Compute the power consumed by the given router
def updateRouterConfigStats(network_config, router_config,\
                              network_stats, router_stats):
    # DSENT Interface

    # Config
    tech_model   = network_config['tech_model']
    num_vnet = network_config['num_vnet']
    flit_size_bits = network_config['flit_size_bits']

    frequency    = router_config['frequency']
    num_inports  = router_config['num_inports']
    num_outports = router_config['num_outports']
    vcs_per_vnet = router_config['vcs_per_vnet']
    buffers_per_ctrl_vc = network_config['buffers_per_ctrl_vc']
    buffers_per_data_vc = network_config['buffers_per_data_vc']

    # Stats
    sim_ticks           = network_stats['sim_ticks']
    buffer_writes       = router_stats['buffer_writes']
    buffer_reads        = router_stats['buffer_reads']
    sw_in_arb_activity  = router_stats['sw_in_arb_activity']
    sw_out_arb_activity = router_stats['sw_out_arb_activity']
    crossbar_activity   = router_stats['crossbar_activity']

    # Run DSENT (calls function in ext/dsent/interface.cc)
    print("\n|Router %s|" % router_config['router_id'])

    dsent.updateRouterConfigStats(tech_model,
                                  frequency,
                                  flit_size_bits,
                                  num_inports,
                                  num_outports,
                                  num_vnet,
                                  vcs_per_vnet,
                                  buffers_per_ctrl_vc,
                                  buffers_per_data_vc,
                                  sim_ticks,
                                  buffer_writes,
                                  buffer_reads,
                                  sw_in_arb_activity,
                                  sw_out_arb_activity,
                                  crossbar_activity)


## Compute the power consumed by the links
def updateLinkConfigStats(network_config, link_config, \
                     network_stats, link_stats):

    # DSENT Interface

    # Config
    tech_model  = network_config['tech_model']
    width_bits  = network_config['flit_size_bits']
    frequency   = link_config['frequency']
    length      = link_config['length']
    delay       = link_config['delay']

    # Stats
    sim_ticks   = network_stats['sim_ticks']
    activity    = link_stats['activity']

    # Run DSENT
    print("\n|All Links|")

    dsent.updateLinkConfigStats(tech_model,
                                frequency,
                                width_bits,
                                length,
                                delay,
                                sim_ticks,
                                activity)

# This script parses the config.ini and the stats.txt from a run and
# generates the power and the area of the on-chip network using DSENT
def main():
    if len(sys.argv) < 6:
        print("\nUsage: python ./" + sys.argv[0] + " <gem5 root directory>" \
              " <simulation directory> " \
              " <dsent router config file> <dsent link config file>" \
              " <technology node>" \
              " [<clock period in ps> (optional)]\n"
              "Note: supported tech nodes: 45, 32, 22, and 11.\n" \
              "If clock period is not specified, it will be read from " \
               "simulation directory/config.ini and can be different for "\
               "each router and link")
        print("\nExample: python ./" + sys.argv[0] + " . m5out " \
              "ext/dsent/configs/garnet_router.cfg " \
              "ext/dsent/configs/garnet_link.cfg " \
              "32 500\n" \
              "This will model 500ps (2GHz) at 32nm")
        exit(-1)

    tech = sys.argv[5]
    if (not(tech == "45" or tech == "32" or tech == "22" or tech == "11")):
        print("Error!! DSENT only supports 45nm (bulk), 32nm (bulk), "\
               " 22nm (bulk), and 11nm (tri-gate) models currently.\n"\
               "To model some other technology, add a model in " \
               "ext/dsent/tech/tech_models.\n"
               "To model photonic links, remove this warning, and update " \
               + sys.argv[0] + " to run DSENT with photonics.model with the "\
               "appropriate configs and stats")
        exit(-1)

    print("WARNING: configuration files for DSENT and McPAT are separate. " \
          "Changes made to one are not reflected in the other.")

    config_file = "%s/%s/config.ini" % (sys.argv[1], sys.argv[2])
    stats_file = "%s/%s/stats.txt" % (sys.argv[1], sys.argv[2])

    ### Parse Config File
    (all_config, network_config, routers, int_links, ext_links) = \
        parseConfig(config_file)

    ### Parse Network Stats
    network_stats = parseNetworkStats(stats_file)

    ### Run DSENT
    router_config_default = sys.argv[3]
    link_config_default = sys.argv[4]

    if not os.path.isfile(router_config_default):
        print("ERROR: router config file '", router_config_default, "' not found")
        sys.exit(1)

    if not os.path.isfile(link_config_default):
        print("ERROR: link config file '", link_config_default, "' not found")
        sys.exit(1)

    ## Router Power and Area

    # Initialize DSENT with the router configuration file
    dsent.initialize(router_config_default)

    # Update default configs for each router and run
    # Compute the power consumed by the routers
    for router in routers:
        #frequency = getClock(router, all_config)
        router_config = getRouterConfig(all_config, router, \
            int_links, ext_links)
        router_stats = parseRouterStats(stats_file, router)
        updateRouterConfigStats(network_config, router_config, \
                                network_stats, router_stats)

        # Run DSENT
        router_power_area = dsent.run()

    # Finalize DSENT
    dsent.finalize()

    ## Link Power

    # Initialize DSENT with a configuration file
    dsent.initialize(link_config_default)

    for link in int_links:
        link_config = getLinkConfig(all_config, link)
#       link_stats = parseLinkStats(stats_file, link)
#       updateLinkConfigStats(link, link_stats, frequency)

    for link in ext_links:
        link_config = getLinkConfig(all_config, link)
#        link_stats = parseLinkStats(stats_file, link)
#        updateLinkConfigStats(link, link_stats, frequency)


    # Update default configs file for links and run
    # (Stats file print total link activity rather than per link)
    # If per link power is required, garnet should print out activity
    # for each link, and updateLinkConfigStats should be called for each
    # link by uncommenting the link_stats and updateLinkConfigStats lines above
    sim_ticks = network_stats['sim_ticks']
    link_stats = parseLinkStats(stats_file, sim_ticks)
    updateLinkConfigStats(network_config, link_config, \
                          network_stats, link_stats)

    # Run DSENT
    link_power = dsent.run()

    # Finalize DSENT
    dsent.finalize()


if __name__ == "__main__":
    main()
