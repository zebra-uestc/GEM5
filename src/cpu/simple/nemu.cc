/*
 * Copyright 2014 Google, Inc.
 * Copyright (c) 2012-2013,2015,2017-2020 ARM Limited
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

#include "cpu/simple/nemu.hh"

#include "arch/generic/decoder.hh"
#include "base/output.hh"
#include "base/types.hh"
#include "cpu/exetrace.hh"
#include "cpu/utils.hh"
#include "debug/Drain.hh"
#include "debug/ExecFaulting.hh"
#include "debug/SimpleCPU.hh"
#include "mem/packet.hh"
#include "mem/packet_access.hh"
#include "mem/physical.hh"
#include "sim/faults.hh"
#include "sim/full_system.hh"
#include "sim/sim_exit.hh"
#include "sim/system.hh"

namespace gem5
{

NemuCPU::NemuCPU(const BaseNemuCPUParams &p)
    : BaseSimpleCPU(p),
    tickEvent([this]{tick(); }, "NemuCPU tick", false, Event::CPU_Tick_Pri),
    icachePort(name() + ".icache_port", this),
    dcachePort(name() + ".dcache_port", this)
{
    nemu = this;
}

NemuCPU::~NemuCPU()
{
    if (tickEvent.scheduled()) {
      deschedule(tickEvent);
    }
}

void
NemuCPU::init()
{
    BaseSimpleCPU::init();
    load_nemu_so();
    set_gem5_ifetch(gem5_fetch);
    set_gem5_read(gem5_read);
    set_gem5_write(gem5_write);
    init_isa();
    std::cout << "=== NemuCPU: invoke init end ===" << std::endl;
}

void
NemuCPU::startup()
{
    schedule(tickEvent, clockPeriod());
    std::cout << "=== NemuCPU: invoke startup end ===" << std::endl;
}

void
NemuCPU::tick()
{
    int res = cpu_execute(1);
    if (res <= 0) {
      dlclose(lib);
      if (res == 0) {
        exitSimLoop("exitSimLoop: nemu_trap ==> NEMU_END", 0);
      } else {
        exitSimLoop("exitSimLoop: nemu_trap ==> NEMU_ABORT", 1);
      }
      return;
    }
    instCnt += res;
    reschedule(tickEvent, curTick() + clockPeriod(), true);
    return;
}

Tick
NemuCPU::sendPacket(RequestPort &port, const PacketPtr& pkt)
{
    return port.sendAtomic(pkt);
}

void
NemuCPU::wakeup(ThreadID tid)
{
    panic("NemuCPU doesn't need to wake up a thread!");
    return;
}

Counter
NemuCPU::totalInsts() const
{
    return instCnt;
}

Counter
NemuCPU::totalOps() const
{
    return instCnt;
}

Addr
NemuCPU::fetch(Addr addr, int len)
{
    assert(len <= 8);
    uint8_t array[len];
    std::memset(array, 0, len);

    Request::Flags flags;
    flags.set(Request::INST_FETCH | Request::PHYSICAL);
    Request ifetch_req(addr, len, flags, _instRequestorId);
    RequestPtr req = std::make_shared<Request>(ifetch_req);
    req->taskId(taskId());

    Packet pkt(req, Packet::makeReadCmd(req));
    pkt.dataStatic(array);

    sendPacket(icachePort, &pkt);

    panic_if(pkt.isError(), "Data fetch (%s) failed: %s",
             pkt.getAddrRange().to_string(), pkt.print());

    Addr result = array_to_uint64(array, len);

    return result;
}

Addr
NemuCPU::read(Addr addr, int len)
{
    assert(len <= 8);

    uint8_t array[len];
    std::memset(array, 0, len);

    Request::Flags flags;
    flags.set(Request::PHYSICAL);
    Request data_read_req(addr, len, flags, _dataRequestorId);
    RequestPtr req = std::make_shared<Request>(data_read_req);
    req->taskId(taskId());

    Packet pkt(req, Packet::makeReadCmd(req));
    pkt.dataStatic(array);

    sendPacket(dcachePort, &pkt);

    panic_if(pkt.isError(), "Data read (%s) failed: %s",
             pkt.getAddrRange().to_string(), pkt.print());

    Addr result = array_to_uint64(array, len);

    return result;
}

void
NemuCPU::write(Addr addr, int len, Addr data)
{
    assert(len <= 8);

    uint8_t array[len];
    std::memset(array, 0, len);
    uint64_to_array(data, array, len);

    Request::Flags flags;
    flags.set(Request::PHYSICAL);
    Request data_write_req(addr, len, flags, _dataRequestorId);
    RequestPtr req = std::make_shared<Request>(data_write_req);
    req->taskId(taskId());

    Packet pkt(req, Packet::makeWriteCmd(req));
    pkt.dataStatic(array);

    sendPacket(dcachePort, &pkt);

    panic_if(pkt.isError(), "Data write (%s) failed: %s",
             pkt.getAddrRange().to_string(), pkt.print());

    return;
}

void
load_nemu_so()
{
    const char *filename = "/home/sachi/xs-gem5/nemu.so";
    lib = dlopen(filename, RTLD_NOW);
    if (!lib) {
      printf("load dlopen(%s): %s\n", filename, dlerror());
      return;
    }

    set_gem5_ifetch = (SetGIF)dlsym(lib, "set_gi");
    if (!set_gem5_ifetch) {
      printf("load dlsym(set_gi): %s\n", dlerror());
      dlclose(lib);
      return;
    }

    set_gem5_read = (SetGR)dlsym(lib, "set_gr");
    if (!set_gem5_read) {
      printf("load dlsym(set_gr): %s\n", dlerror());
      dlclose(lib);
      return;
    }

    set_gem5_write = (SetGW)dlsym(lib, "set_gw");
    if (!set_gem5_write) {
      printf("load dlsym(set_gw): %s\n", dlerror());
      dlclose(lib);
      return;
    }

    cpu_execute = (CPUExec)dlsym(lib, "cpu_exec");
    if (!cpu_execute) {
      printf("load dlsym(cpu_exec): %s\n", dlerror());
      dlclose(lib);
      return;
    }

    init_isa = (BareFunc)dlsym(lib, "init_isa");
    if (!init_isa) {
      printf("load dlsym(init_isa): %s\n", dlerror());
      dlclose(lib);
      return;
    }
}

uint64_t
array_to_uint64(uint8_t* array, int len)
{
    uint64_t result = 0;
    for (int i = 0; i < len; i++) {
      result |= ((uint64_t)array[i] << (8 * i));
    }
    return result;
}

void
uint64_to_array(uint64_t value, uint8_t* array, int len)
{
    for (int i = 0; i < len; i++) {
      array[i] = (value >> (i * 8)) & 0xFF;
    }
}

uint64_t
gem5_fetch(uint64_t addr, int len)
{
    uint64_t data = nemu->fetch(addr, len);
    return data;
}

uint64_t
gem5_read(uint64_t addr, int len)
{
    uint64_t data = nemu->read(addr, len);
    return data;
}

void
gem5_write(uint64_t addr, int len, uint64_t data)
{
    return nemu->write(addr, len, data);
}

}