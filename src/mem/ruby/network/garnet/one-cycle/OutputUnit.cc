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

#include "mem/ruby/network/garnet/one-cycle/OutputUnit.hh"

#include "base/stl_helpers.hh"
#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/garnet/one-cycle/Router.hh"

using namespace std;
using m5::stl_helpers::deletePointers;

OutputUnit::OutputUnit(int id, Router *router)
    : Consumer(router)
{
    m_id = id;
    m_router = router;
    m_num_vcs = m_router->get_num_vcs();
    m_vc_per_vnet = m_router->get_vc_per_vnet();
    m_out_buffer = new flitBuffer();

    for (int i = 0; i < m_num_vcs; i++) {
        m_outvc_state.push_back(new OutVcState(i, m_router->get_net_ptr()));
    }
}

OutputUnit::~OutputUnit()
{
    delete m_out_buffer;
    deletePointers(m_outvc_state);
}

void
OutputUnit::decrement_credit(int out_vc)
{
        DPRINTF(RubyNetwork, "Router %d OutputUnit %d decrementing credit for outvc %d at time: %lld\n",
            m_router->get_id(), m_id, out_vc, m_router->curCycle());

    m_outvc_state[out_vc]->decrement_credit();
// TODO: delete
//    m_router->update_incredit(m_outvc_state[out_vc]->get_inport(),
//                              m_outvc_state[out_vc]->get_invc(),
//                              m_outvc_state[out_vc]->get_credit_count());
}

void
OutputUnit::increment_credit(int out_vc)
{
        DPRINTF(RubyNetwork, "Router %d OutputUnit %d incrementing credit for outvc %d at time: %lld\n",
            m_router->get_id(), m_id, out_vc, m_router->curCycle());

    m_outvc_state[out_vc]->increment_credit();
}

bool
OutputUnit::has_credit(int out_vc)
{
    assert(m_outvc_state[out_vc]->isInState(ACTIVE_, m_router->curCycle()));
    return m_outvc_state[out_vc]->has_credit();
}


bool
OutputUnit::has_free_vc(int vnet)
{
    int vc_base = vnet*m_vc_per_vnet;
    for (int vc = vc_base; vc < vc_base + m_vc_per_vnet; vc++)
    {
        if (is_vc_idle(vc, m_router->curCycle()))
            return true;
    }

    return false;
}

int
OutputUnit::select_free_vc(int vnet)
{
    int vc_base = vnet*m_vc_per_vnet;
    for (int vc = vc_base; vc < vc_base + m_vc_per_vnet; vc++)
    {
        if (is_vc_idle(vc, m_router->curCycle()))
        {
            m_outvc_state[vc]->setState(ACTIVE_, m_router->curCycle());
            return vc;
        }
    }

    return -1;
}


void
OutputUnit::wakeup()
{
    if (m_credit_link->isReady(m_router->curCycle())) {
        flit *t_flit = m_credit_link->consumeLink();
        increment_credit(t_flit->get_vc());

        // TODO: delete
//        m_router->update_incredit(m_outvc_state[out_vc]->get_inport(),
//                                  m_outvc_state[out_vc]->get_invc(),
//                                  m_outvc_state[out_vc]->get_credit_count());

        if (t_flit->is_free_signal())
            set_vc_state(IDLE_, t_flit->get_vc(), m_router->curCycle());

        delete t_flit;
    }
}

flitBuffer*
OutputUnit::getOutQueue()
{
    return m_out_buffer;
}

void
OutputUnit::set_out_link(NetworkLink *link)
{
    m_out_link = link;
}

void
OutputUnit::set_credit_link(CreditLink *credit_link)
{
    m_credit_link = credit_link;
}

uint32_t
OutputUnit::functionalWrite(Packet *pkt)
{
    return m_out_buffer->functionalWrite(pkt);
}
