/* Copyright (c) 2014 Mark D. Hill and David A. Wood
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
 */

#include <Python.h>
#include <cstdio>

#include "DSENT.h"
#include "libutil/String.h"
#include "model/Model.h"

using namespace std;
using namespace LibUtil;

static PyObject *DSENTError;
static PyObject* dsent_initialize(PyObject*, PyObject*);
static PyObject* dsent_finalize(PyObject*, PyObject*);
static PyObject* dsent_computeRouterPowerAndArea(PyObject*, PyObject*);
static PyObject* dsent_computeLinkPower(PyObject*, PyObject*);

// Create DSENT configuration map.  This map is supposed to retain all
// the information between calls to initialize() and finalize().
map<String, String> params;
DSENT::Model *ms_model;


static PyMethodDef DSENTMethods[] = {
    {"initialize", dsent_initialize, METH_O,
     "initialize dsent using a config file."},

    {"finalize", dsent_finalize, METH_NOARGS,
     "finalize dsent by dstroying the config object"},

    {"computeRouterPowerAndArea", dsent_computeRouterPowerAndArea,
     METH_VARARGS, "compute quantities related power consumption of a router"},

    {"computeLinkPower", dsent_computeLinkPower, 
     METH_VARARGS, "compute quantities related power consumption of a link"},

    {NULL, NULL, 0, NULL}
};


PyMODINIT_FUNC
initdsent(void)
{
    PyObject *m;

    m = Py_InitModule("dsent", DSENTMethods);
    if (m == NULL) return;

    DSENTError = PyErr_NewException("dsent.error", NULL, NULL);
    Py_INCREF(DSENTError);
    PyModule_AddObject(m, "error", DSENTError);

    ms_model = nullptr;
}


static PyObject *
dsent_initialize(PyObject *self, PyObject *arg)
{
    const char *config_file = PyString_AsString(arg);
    //Read the arguments sent from the python script
    if (!config_file) {
        Py_RETURN_NONE;
    }

    // Initialize DSENT
    ms_model = DSENT::initialize(config_file, params);
    Py_RETURN_NONE;
}


static PyObject *
dsent_finalize(PyObject *self, PyObject *args)
{
    // Finalize DSENT
    DSENT::finalize(params, ms_model);
    ms_model = nullptr;
    Py_RETURN_NONE;
}


static PyObject *
dsent_computeRouterPowerAndArea(PyObject *self, PyObject *args)
{
    uint64_t frequency;
    unsigned int flit_width_bits;
    unsigned int num_in_port;
    unsigned int num_out_port;
    unsigned int num_vnet;
    unsigned int num_vcs_per_vnet;
    unsigned int num_buffers_per_ctrl_vc;
    unsigned int num_buffers_per_data_vc;

    unsigned int num_ticks;
    unsigned int num_buffer_writes;
    unsigned int num_buffer_reads;
    unsigned int num_sw_in_arb;
    unsigned int num_sw_out_arb;
    unsigned int num_crossbar_traversals;

    const char *input_port_buffer_model;
    const char *crossbar_model;
    const char *sa_arbiter_model;
    const char *clk_tree_model;
    unsigned int clk_tree_num_levels;
    const char *clk_tree_wire_layer;
    double clk_tree_wire_width_mult;


    if (!PyArg_ParseTuple(args, "KIIIIIIIIIIIII", 
                          &frequency,
                          &flit_width_bits,
                          &num_in_port,
                          &num_out_port,
                          &num_vnet,
                          &num_vcs_per_vnet,
                          &num_buffers_per_ctrl_vc,
                          &num_buffers_per_data_vc,
                          &num_ticks,
                          &num_buffer_writes,
                          &num_buffer_reads,
                          &num_sw_in_arb,
                          &num_sw_out_arb,
                          &num_crossbar_traversals)) { 
        Py_RETURN_NONE;
    }

    assert(frequency > 0.0);
    assert(flit_width_bits != 0);
    assert(num_in_port != 0);
    assert(num_out_port != 0);
    assert(num_vnet != 0);
    assert(num_vcs_per_vnet != 0);

    vector<unsigned int> num_vc_vec(num_vnet, num_vcs_per_vnet);
    vector<unsigned int> num_buffers_vec(num_vnet, num_buffers_per_ctrl_vc);
    // In gem5, one of the VCs is a response (data) VC
    num_buffers_vec[num_vnet-1] = num_buffers_per_data_vc;

    // DSENT outputs
    map<string, double> outputs;

    // Overwrite default configs
    params["Frequency"] = String(frequency);
    params["NumberBitsPerFlit"] = String(flit_width_bits);
    params["NumberInputPorts"] = String(num_in_port);
    params["NumberOutputPorts"] = String(num_out_port);
    params["NumberVirtualNetworks"] = String(num_vnet);
    params["NumberVirtualChannelsPerVirtualNetwork"] =
        vectorToString<unsigned int>(num_vc_vec);
    params["NumberBuffersPerVirtualChannel"] =
        vectorToString<unsigned int>(num_buffers_vec);

    // Update stats
    params["NumCycles"] = String(num_ticks);
    params["NumBufferWrites"] = String(num_buffer_writes);
    params["NumBufferReads"] = String(num_buffer_reads);
    params["NumSwInportArbs"] = String(num_sw_in_arb);
    params["NumSwOutportArbs"] = String(num_sw_out_arb);
    params["NumCrossbarTraversals"] = String(num_crossbar_traversals);


    // Run DSENT
    DSENT::run(params, ms_model, outputs);

    // Store outputs
    PyObject *r = PyTuple_New(outputs.size());
    int index = 0;

    // Prepare the output.  The assumption is that all the output
    for (const auto &it : outputs) {
        PyObject *s = PyTuple_New(2);
        PyTuple_SetItem(s, 0, PyString_FromString(it.first.c_str()));
        PyTuple_SetItem(s, 1, PyFloat_FromDouble(it.second));

        PyTuple_SetItem(r, index, s);
        index++;
    }

    return r;
}


static PyObject *
dsent_computeLinkPower(PyObject *self, PyObject *args)
{
    uint64_t frequency;
    unsigned int width_bits;
    float length;
    float delay;

    unsigned int num_ticks;
    unsigned int num_traversals;
 

    if (!PyArg_ParseTuple(args, "KIffII", 
                          &frequency,
                          &width_bits,
                          &length,
                          &delay,
                          &num_ticks,
                          &num_traversals)) {
        Py_RETURN_NONE;
    }

    assert(frequency > 0.0);
    assert(width_bits != 0);
    assert(length > 0.0);
    assert(delay > 0.0);
 
    // DSENT outputs
    map<string, double> outputs;

    // Overwrite default configs
    params["Frequency"]     = String(frequency);
    params["NumberBits"]    = String(width_bits);
    params["WireLength"]    = String(length);
    params["Delay"]         = String(delay);

    // Update stats
    params["NumCycles"]     = String(num_ticks);
    params["NumLinkTraversals"] = String(num_traversals);

    // Run DSENT
    DSENT::run(params, ms_model, outputs);

    // Store outputs
    PyObject *r = PyTuple_New(outputs.size());
    int index = 0;

    // Prepare the output.  The assumption is that all the output
    for (const auto &it : outputs) {
        PyObject *s = PyTuple_New(2);
        PyTuple_SetItem(s, 0, PyString_FromString(it.first.c_str()));
        PyTuple_SetItem(s, 1, PyFloat_FromDouble(it.second));

        PyTuple_SetItem(r, index, s);
        index++;
    }

    return r;
}

static PyObject *
dsent_printAvailableModels(PyObject* self, PyObject *args)
{
}
