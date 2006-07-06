/*
 * Copyright (c) 2006 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Ali Saidi
 */

/**
 * @file Definition of a bus object.
 */


#include "base/misc.hh"
#include "base/trace.hh"
#include "mem/bus.hh"
#include "sim/builder.hh"

Port *
Bus::getPort(const std::string &if_name, int idx)
{
    if (if_name == "default")
        if (defaultPort == NULL) {
            defaultPort = new BusPort(csprintf("%s-default",name()), this,
                    defaultId);
            return defaultPort;
        } else
            fatal("Default port already set\n");

    // if_name ignored?  forced to be empty?
    int id = interfaces.size();
    BusPort *bp = new BusPort(csprintf("%s-p%d", name(), id), this, id);
    interfaces.push_back(bp);
    return bp;
}

/** Get the ranges of anyone other buses that we are connected to. */
void
Bus::init()
{
    std::vector<Port*>::iterator intIter;

    for (intIter = interfaces.begin(); intIter != interfaces.end(); intIter++)
        (*intIter)->sendStatusChange(Port::RangeChange);
}


/** Function called by the port when the bus is receiving a Timing
 * transaction.*/
bool
Bus::recvTiming(Packet *pkt)
{
    Port *port;
    DPRINTF(Bus, "recvTiming: packet src %d dest %d addr 0x%x cmd %s\n",
            pkt->getSrc(), pkt->getDest(), pkt->getAddr(), pkt->cmdString());

    short dest = pkt->getDest();
    if (dest == Packet::Broadcast) {
        port = findPort(pkt->getAddr(), pkt->getSrc());
    } else {
        assert(dest >= 0 && dest < interfaces.size());
        assert(dest != pkt->getSrc()); // catch infinite loops
        port = interfaces[dest];
    }
    if (port->sendTiming(pkt))  {
        // packet was successfully sent, just return true.
        return true;
    }

    // packet not successfully sent
    retryList.push_back(interfaces[pkt->getSrc()]);
    return false;
}

void
Bus::recvRetry(int id)
{
    // Go through all the elements on the list calling sendRetry on each
    // This is not very efficient at all but it works. Ultimately we should end
    // up with something that is more intelligent.
    int initialSize = retryList.size();
    int i;
    Port *p;

    for (i = 0; i < initialSize; i++) {
        assert(retryList.size() > 0);
        p = retryList.front();
        retryList.pop_front();
        p->sendRetry();
    }
}


Port *
Bus::findPort(Addr addr, int id)
{
    /* An interval tree would be a better way to do this. --ali. */
    int dest_id = -1;
    int i = 0;
    bool found = false;
    AddrRangeIter iter;

    while (i < portList.size() && !found)
    {
        if (portList[i].range == addr) {
            dest_id = portList[i].portId;
            found = true;
            DPRINTF(Bus, "  found addr 0x%llx on device %d\n", addr, dest_id);
        }
        i++;
    }

    // Check if this matches the default range
    if (dest_id == -1) {
        for (iter = defaultRange.begin(); iter != defaultRange.end(); iter++) {
            if (*iter == addr) {
                DPRINTF(Bus, "  found addr 0x%llx on default\n", addr);
                return defaultPort;
            }
        }
        panic("Unable to find destination for addr: %llx", addr);
    }


    // we shouldn't be sending this back to where it came from
    assert(dest_id != id);

    return interfaces[dest_id];
}

/** Function called by the port when the bus is receiving a Atomic
 * transaction.*/
Tick
Bus::recvAtomic(Packet *pkt)
{
    DPRINTF(Bus, "recvAtomic: packet src %d dest %d addr 0x%x cmd %s\n",
            pkt->getSrc(), pkt->getDest(), pkt->getAddr(), pkt->cmdString());
    assert(pkt->getDest() == Packet::Broadcast);
    return findPort(pkt->getAddr(), pkt->getSrc())->sendAtomic(pkt);
}

/** Function called by the port when the bus is receiving a Functional
 * transaction.*/
void
Bus::recvFunctional(Packet *pkt)
{
    DPRINTF(Bus, "recvFunctional: packet src %d dest %d addr 0x%x cmd %s\n",
            pkt->getSrc(), pkt->getDest(), pkt->getAddr(), pkt->cmdString());
    assert(pkt->getDest() == Packet::Broadcast);
    findPort(pkt->getAddr(), pkt->getSrc())->sendFunctional(pkt);
}

/** Function called by the port when the bus is receiving a status change.*/
void
Bus::recvStatusChange(Port::Status status, int id)
{
    AddrRangeList ranges;
    AddrRangeList snoops;
    int x;
    AddrRangeIter iter;

    assert(status == Port::RangeChange &&
           "The other statuses need to be implemented.");

    DPRINTF(BusAddrRanges, "received RangeChange from device id %d\n", id);

    if (id == defaultId) {
        defaultRange.clear();
        defaultPort->getPeerAddressRanges(ranges, snoops);
        assert(snoops.size() == 0);
        for(iter = ranges.begin(); iter != ranges.end(); iter++) {
            defaultRange.push_back(*iter);
            DPRINTF(BusAddrRanges, "Adding range %llx - %llx for default\n",
                    iter->start, iter->end);
        }
    } else {

        assert((id < interfaces.size() && id >= 0) || id == -1);
        Port *port = interfaces[id];
        std::vector<DevMap>::iterator portIter;

        // Clean out any previously existent ids
        for (portIter = portList.begin(); portIter != portList.end(); ) {
            if (portIter->portId == id)
                portIter = portList.erase(portIter);
            else
                portIter++;
        }

        port->getPeerAddressRanges(ranges, snoops);

        // not dealing with snooping yet either
        assert(snoops.size() == 0);
        for(iter = ranges.begin(); iter != ranges.end(); iter++) {
            DevMap dm;
            dm.portId = id;
            dm.range = *iter;

            DPRINTF(BusAddrRanges, "Adding range %llx - %llx for id %d\n",
                    dm.range.start, dm.range.end, id);
            portList.push_back(dm);
        }
    }
    DPRINTF(MMU, "port list has %d entries\n", portList.size());

    // tell all our peers that our address range has changed.
    // Don't tell the device that caused this change, it already knows
    for (x = 0; x < interfaces.size(); x++)
        if (x != id)
            interfaces[x]->sendStatusChange(Port::RangeChange);

    if (id != defaultId && defaultPort)
        defaultPort->sendStatusChange(Port::RangeChange);
}

void
Bus::addressRanges(AddrRangeList &resp, AddrRangeList &snoop, int id)
{
    std::vector<DevMap>::iterator portIter;
    AddrRangeIter dflt_iter;
    bool subset;

    resp.clear();
    snoop.clear();

    DPRINTF(BusAddrRanges, "received address range request, returning:\n");

    for (dflt_iter = defaultRange.begin(); dflt_iter != defaultRange.end();
            dflt_iter++) {
        resp.push_back(*dflt_iter);
        DPRINTF(BusAddrRanges, "  -- %#llX : %#llX\n",dflt_iter->start,
                dflt_iter->end);
    }
    for (portIter = portList.begin(); portIter != portList.end(); portIter++) {
        subset = false;
        for (dflt_iter = defaultRange.begin(); dflt_iter != defaultRange.end();
                dflt_iter++) {
            if ((portIter->range.start < dflt_iter->start &&
                portIter->range.end >= dflt_iter->start) ||
               (portIter->range.start < dflt_iter->end &&
                portIter->range.end >= dflt_iter->end))
                fatal("Devices can not set ranges that itersect the default set\
                        but are not a subset of the default set.\n");
            if (portIter->range.start >= dflt_iter->start &&
                portIter->range.end <= dflt_iter->end) {
                subset = true;
                DPRINTF(BusAddrRanges, "  -- %#llX : %#llX is a SUBSET\n",
                    portIter->range.start, portIter->range.end);
            }
        }
        if (portIter->portId != id && !subset) {
            resp.push_back(portIter->range);
            DPRINTF(BusAddrRanges, "  -- %#llX : %#llX\n",
                    portIter->range.start, portIter->range.end);
        }
    }
}

BEGIN_DECLARE_SIM_OBJECT_PARAMS(Bus)

    Param<int> bus_id;

END_DECLARE_SIM_OBJECT_PARAMS(Bus)

BEGIN_INIT_SIM_OBJECT_PARAMS(Bus)
    INIT_PARAM(bus_id, "a globally unique bus id")
END_INIT_SIM_OBJECT_PARAMS(Bus)

CREATE_SIM_OBJECT(Bus)
{
    return new Bus(getInstanceName(), bus_id);
}

REGISTER_SIM_OBJECT("Bus", Bus)
