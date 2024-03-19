/*
 * Copyright (c) 2012-2013, 2015, 2018, 2020-2021 ARM Limited
 * All rights reserved.
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2002-2005 The Regents of The University of Michigan
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

#ifndef __CPU_SIMPLE_NEMU_HH__
#define __CPU_SIMPLE_NEMU_HH__

// C includes
#include <dlfcn.h>

// C++ includes
#include <cstdint>
#include <memory>

// GEM5 includes
#include "cpu/simple/base.hh"
#include "cpu/simple/exec_context.hh"
#include "mem/request.hh"
#include "params/BaseNemuCPU.hh"
#include "sim/probe/probe.hh"

namespace gem5
{
class NemuCPU : public BaseSimpleCPU
{
  public:

    NemuCPU(const BaseNemuCPUParams &params);
    virtual ~NemuCPU();

    void init() override;
    void startup() override;

  protected:
    Addr maxInsts;
    EventFunctionWrapper tickEvent;
    void tick();
    void sendFunctionalPacket(RequestPort& port, const PacketPtr& pkt);
    Tick sendAtomicPacket(RequestPort &port, const PacketPtr &pkt);

  public:

    Counter totalInsts() const override;

    Counter totalOps() const override;

    class NemuCPUPort : public RequestPort
    {
      public:

        NemuCPUPort(const std::string &_name, BaseSimpleCPU* _cpu)
            : RequestPort(_name, _cpu)
        { }

      protected:

        bool recvTimingResp(PacketPtr pkt)
        {
          panic("Nemu CPU doesn't expect recvTimingResp!\n");
        }

        void
        recvReqRetry()
        {
          panic("Nemu CPU doesn't expect recvRetry!\n");
        }
    };

    NemuCPUPort icachePort;
    NemuCPUPort dcachePort;


    /** Return a reference to the data port. */
    Port &getDataPort() override { return dcachePort; }

    /** Return a reference to the instruction port. */
    Port &getInstPort() override { return icachePort; }


  public:
    Addr functionalFetch(Addr addr, int len);
    Addr functionalRead(Addr addr, int len);
    void functionalWrite(Addr addr, int len, Addr data);

    Addr atomicFetch(Addr addr, int len);
    Addr atomicRead(Addr addr, int len);
    void atomicWrite(Addr addr, int len, Addr data);

    Fault
    initiateMemMgmtCmd(Request::Flags flags) override
    {
      panic("initiateMemMgmtCmd() is for timing accesses, and "
            "should never be called on AtomicSimpleCPU.\n");
    }
};

typedef uint64_t (*ReadPtr)(uint64_t, int);
typedef void (*WritePtr)(uint64_t, int, uint64_t);

typedef void(*SetGIF)(ReadPtr);
typedef void(*SetGR)(ReadPtr);
typedef void(*SetGW)(WritePtr);
typedef int(*CPUExec)(uint64_t);
typedef void(*BareFunc)(void);

static NemuCPU* nemu = nullptr;
static void* lib = nullptr;

static SetGIF set_gem5_ifetch = nullptr;
static SetGR set_gem5_read = nullptr;
static SetGW set_gem5_write = nullptr;
static CPUExec cpu_execute = nullptr;
static BareFunc init_isa = nullptr;



void load_nemu_so();

uint64_t array_to_uint64(uint8_t* array, int len);
void uint64_to_array(uint64_t value, uint8_t* array, int len);

uint64_t gem5_fetch(uint64_t addr, int len);
uint64_t gem5_read(uint64_t addr, int len);
void gem5_write(uint64_t addr, int len, uint64_t data);

}

#endif