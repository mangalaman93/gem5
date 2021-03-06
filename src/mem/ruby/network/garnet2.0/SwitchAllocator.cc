/*
 * Copyright (c) 2008 Princeton University
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
 * Authors: Niket Agarwal
 */

#include "mem/ruby/network/garnet2.0/SwitchAllocator.hh"
#include "mem/ruby/network/garnet2.0/NetworkInterface.hh"
#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/garnet2.0/GarnetNetwork.hh"
#include "mem/ruby/network/garnet2.0/InputUnit.hh"
#include "mem/ruby/network/garnet2.0/OutputUnit.hh"
#include "mem/ruby/network/garnet2.0/Router.hh"
#include "mem/ruby/network/MessageBuffer.hh"
#include "mem/ruby/network/garnet2.0/flitBuffer.hh"
#include "mem/ruby/slicc_interface/Message.hh"
#include "base/stl_helpers.hh"
#include "base/cast.hh"

using namespace std;
using m5::stl_helpers::deletePointers;

SwitchAllocator::SwitchAllocator(Router *router)
    : Consumer(router)
{
    m_router = router;
    m_num_vcs = m_router->get_num_vcs();
    m_vc_per_vnet = m_router->get_vc_per_vnet();

    m_input_arbiter_activity = 0;
    m_output_arbiter_activity = 0;
}

void
SwitchAllocator::init()
{
    m_input_unit = m_router->get_inputUnit_ref();
    m_output_unit = m_router->get_outputUnit_ref();

    m_num_inports = m_router->get_num_inports();
    m_num_outports = m_router->get_num_outports();
    m_round_robin_inport.resize(m_num_outports);
    m_round_robin_invc.resize(m_num_inports);
    m_port_requests.resize(m_num_outports);
    m_vc_winners.resize(m_num_outports);

    for (int i = 0; i < m_num_inports; i++) {
        m_round_robin_invc[i] = 0;
    }

    for (int i = 0; i < m_num_outports; i++) {
        m_port_requests[i].resize(m_num_inports);
        m_vc_winners[i].resize(m_num_inports);

        m_round_robin_inport[i] = 0;

        for (int j = 0; j < m_num_inports; j++) {
            m_port_requests[i][j] = false; // [outport][inport]
        }
    }
}

void
SwitchAllocator::wakeup()
{
    arbitrate_inports(); // First stage of allocation
    arbitrate_outports(); // Second stage of allocation
    clear_request_vector();
    check_for_wakeup();
}

void
SwitchAllocator::arbitrate_inports()
{
    // Select a VC from each input in a round robin manner
    // Independent arbiter at each input port
  //  NetworkLink *inNetLink =  in_link;
 //   flit *t_flit = inNetLink->consumeLink();
    for (int inport = 0; inport < m_num_inports; inport++) {
        int invc = m_round_robin_invc[inport];

        // Select next round robin vc candidate within valid vnet
        int next_round_robin_invc = invc;
        next_round_robin_invc++;
        if (next_round_robin_invc >= m_num_vcs)
            next_round_robin_invc = 0;
        m_round_robin_invc[inport] = next_round_robin_invc;

        for (int invc_iter = 0; invc_iter < m_num_vcs; invc_iter++) {

            if (m_input_unit[inport]->need_stage(invc, SA_, m_router->curCycle()))
            {
                int  outport = m_input_unit[inport]->get_outport(invc);
                int  outvc   = m_input_unit[inport]->get_outvc(invc);		 //  [ICN Project]
                bool make_request = send_allowed(inport, invc, outport, outvc); // [ICN Project]
               if (make_request)	//	[ICN Project]
                {			//	[ICN Project]
                    m_input_arbiter_activity++;
                    m_port_requests[outport][inport] = true;
                    m_vc_winners[outport][inport]= invc;
                    break; // got one vc winner for this port
               }			//	[ICN Project]
                 /*
		Need to add code here to delete packet [ICN Project]
		else
		{ 
		  delete t_flit;
	        }*/
            }

            invc++;
            if (invc >= m_num_vcs)
                invc = 0;
        }
    }
}

void
SwitchAllocator::arbitrate_outports()
{
    // Now there are a set of input vc requests for output vcs.
    // Again do round robin arbitration on these requests
    // Independent arbiter at each output port
    for (int outport = 0; outport < m_num_outports; outport++) {
        int inport = m_round_robin_inport[outport];
        m_round_robin_inport[outport]++;

        if (m_round_robin_inport[outport] >= m_num_inports)
            m_round_robin_inport[outport] = 0;

        for (int inport_iter = 0; inport_iter < m_num_inports; inport_iter++) {

            // inport has a request this cycle for outport
            if (m_port_requests[outport][inport]) {

                // grant this outport to this inport
                int invc = m_vc_winners[outport][inport];
                int outvc = m_input_unit[inport]->get_outvc(invc);
                if (outvc == -1)
                {
                    // VC Allocation - select any free VC from outport
                    outvc = vc_allocate(outport, inport, invc);
                }
               // [ICN Project]             

                // remove flit from Input VC
                flit *t_flit = m_input_unit[inport]->getTopFlit(invc);

                DPRINTF(RubyNetwork, "SwitchAllocator at Router %d \
                                      granted outvc %d at outport %d \
                                      to invc %d at inport %d at time: %lld\n",
                        m_router->get_id(), outvc,
                        m_router->getPortDirectionName(
                            m_output_unit[outport]->get_direction()),
                        invc,
                        m_router->getPortDirectionName(
                            m_input_unit[inport]->get_direction()),
                        m_router->curCycle());


                // flit ready for Switch Traversal
                // the outport was already updated in the flit
                // in the InputUnit (after route_compute)
                t_flit->advance_stage(ST_, m_router->curCycle());

                // update outport field in switch
                // (used by switch code to send it out of correct outport
                t_flit->set_outport(outport);

                // set outvc (i.e., invc for next hop) in flit
                t_flit->set_vc(outvc);
                m_output_unit[outport]->decrement_credit(outvc);

                m_router->grant_switch(inport, t_flit);
                m_output_arbiter_activity++;

                if ((t_flit->get_type() == TAIL_) ||
                    t_flit->get_type() == HEAD_TAIL_) {

                    // This Input VC should now be empty
                    assert(m_input_unit[inport]->isReady(invc,
                        m_router->curCycle()) == false);

                    // Free this VC
                    m_input_unit[inport]->set_vc_idle(invc, m_router->curCycle());

                    // Send a credit back
                    // along with the information that this VC is now idle
                    m_input_unit[inport]->increment_credit(invc, true,
                        m_router->curCycle());
                } else {
                    // Send a credit back
                    // but do not indicate that the VC is idle
                    m_input_unit[inport]->increment_credit(invc, false,
                        m_router->curCycle());
                }

                // remove this request
                m_port_requests[outport][inport] = false;
	       
                break; // got a input winner for this outport
            }

            inport++;
            if (inport >= m_num_inports)
                inport = 0;
        }
    }
}

bool
SwitchAllocator::send_allowed(int inport, int invc, int outport, int outvc)
{
    PortDirection inport_dirn  = m_input_unit[inport]->get_direction();
    PortDirection outport_dirn = m_output_unit[outport]->get_direction();

    // Check if outvc needed
    // Check if credit needed (for multi-flit packet)
    // Check if ordering violated (in ordered vnet)

    int vnet = get_vnet(invc);
    bool has_outvc = (outvc != -1);
    bool has_credit = false;

    if (has_outvc == false) // needs outvc
    {
        if (m_output_unit[outport]->has_free_vc(vnet, inport_dirn, outport_dirn))
        {
            has_outvc = true;
            has_credit = true; // each VC has at least one buffer, so no need for additional credit check
        }
    }
    else
    {
        has_credit = m_output_unit[outport]->has_credit(outvc);
    }

    if (!has_outvc || !has_credit)
        return false;


    // protocol ordering check
    if ((m_router->get_net_ptr())->isVNetOrdered(vnet))
    {
        Cycles t_enqueue_time = m_input_unit[inport]->get_enqueue_time(invc);
        int vc_base = vnet*m_vc_per_vnet;
        for (int vc_offset = 0; vc_offset < m_vc_per_vnet; vc_offset++) {
            int temp_vc = vc_base + vc_offset;
            if (m_input_unit[inport]->need_stage(temp_vc, SA_,
                                                 m_router->curCycle()) &&
               (m_input_unit[inport]->get_outport(temp_vc) == outport) &&
               (m_input_unit[inport]->get_enqueue_time(temp_vc) <
                    t_enqueue_time)) {
                return false;
            }
        }
    }

    return true;
}

int
SwitchAllocator::vc_allocate(int outport, int inport, int invc)
{
    PortDirection inport_dirn  = m_input_unit[inport]->get_direction();
    PortDirection outport_dirn = m_output_unit[outport]->get_direction();

    // Select a free VC from the output port
    int outvc = m_output_unit[outport]->select_free_vc(get_vnet(invc), inport_dirn, outport_dirn);
    assert(outvc != -1); // has to get a valid VC since it checked before performing SA  [ICN Project]
    m_input_unit[inport]->grant_outvc(invc, outvc);
    return outvc;
}

void
SwitchAllocator::check_for_wakeup()
{
    Cycles nextCycle = m_router->curCycle() + Cycles(1);

    for (int i = 0; i < m_num_inports; i++) {
        for (int j = 0; j < m_num_vcs; j++) {
            if (m_input_unit[i]->need_stage(j, SA_, nextCycle)) {
                m_router->schedule_wakeup(Cycles(1));
                return;
            }
        }
    }
}

int
SwitchAllocator::get_vnet(int invc)
{
    int vnet = invc/m_vc_per_vnet;
    assert(vnet < m_router->get_num_vnets());
    return vnet;
}

void
SwitchAllocator::clear_request_vector()
{
    for (int i = 0; i < m_num_outports; i++) {
        for (int j = 0; j < m_num_inports; j++) {
            m_port_requests[i][j] = false;
        }
    }
}
