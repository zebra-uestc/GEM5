/*
 * Copyright (c) 2011-2013, 2017, 2020 ARM Limited
 * All rights reserved
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
 * Copyright (c) 2011 Regents of the University of California
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

#ifndef __CPU_BASE_HH__
#define __CPU_BASE_HH__

#include <queue>
#include <vector>

// Before we do anything else, check if this build is the NULL ISA,
// and if so stop here
#include "config/the_isa.hh"

#if IS_NULL_ISA
#error Including BaseCPU in a system without CPU support
#else
#include "arch/generic/interrupts.hh"
#include "base/statistics.hh"
#include "cpu/difftest.hh"
#include "debug/Mwait.hh"
#include "mem/htm.hh"
#include "mem/port_proxy.hh"
#include "sim/clocked_object.hh"
#include "sim/eventq.hh"
#include "sim/full_system.hh"
#include "sim/insttracer.hh"
#include "sim/probe/pmu.hh"
#include "sim/probe/probe.hh"
#include "sim/system.hh"

namespace gem5
{

class BaseCPU;
struct BaseCPUParams;
class CheckerCPU;
class ThreadContext;

enum CsrRegIndex
{
  mode , mstatus, sstatus, mepc, sepc, mtval, stval,
  mtvec, stvec  , mcause , scause, satp,
  mip, mie,
  mscratch, sscratch,
  mideleg, medeleg,
  pc
};

struct AddressMonitor
{
    AddressMonitor();
    bool doMonitor(PacketPtr pkt);

    bool armed;
    Addr vAddr;
    Addr pAddr;
    uint64_t val;
    bool waiting;   // 0=normal, 1=mwaiting
    bool gotWakeup;
};

class CPUProgressEvent : public Event
{
  protected:
    Tick _interval;
    Counter lastNumInst;
    BaseCPU *cpu;
    bool _repeatEvent;

  public:
    CPUProgressEvent(BaseCPU *_cpu, Tick ival = 0);

    void process();

    void interval(Tick ival) { _interval = ival; }
    Tick interval() { return _interval; }

    void repeatEvent(bool repeat) { _repeatEvent = repeat; }

    virtual const char *description() const;
};

struct DiffAllStates
{
    riscv64_CPU_regfile gem5RegFile;
    riscv64_CPU_regfile referenceRegFile;
    DiffState diff;
    RefProxy *proxy;

    bool scFenceInFlight{false};
    bool hasCommit{false};
};

class BaseCPU : public ClockedObject
{
  protected:

    const unsigned IntRegIndexBase = 0;
    const unsigned FPRegIndexBase = 32;

    /// Instruction count used for SPARC misc register
    /// @todo unify this with the counters that cpus individually keep
    Tick instCnt;

    // every cpu has an id, put it in the base cpu
    // Set at initialization, only time a cpuId might change is during a
    // takeover (which should be done from within the BaseCPU anyway,
    // therefore no setCpuId() method is provided
    int _cpuId;

    /** Each cpu will have a socket ID that corresponds to its physical location
     * in the system. This is usually used to bucket cpu cores under single DVFS
     * domain. This information may also be required by the OS to identify the
     * cpu core grouping (as in the case of ARM via MPIDR register)
     */
    const uint32_t _socketId;

    /** instruction side request id that must be placed in all requests */
    RequestorID _instRequestorId;

    /** data side request id that must be placed in all requests */
    RequestorID _dataRequestorId;

    /** An intrenal representation of a task identifier within gem5. This is
     * used so the CPU can add which taskId (which is an internal representation
     * of the OS process ID) to each request so components in the memory system
     * can track which process IDs are ultimately interacting with them
     */
    uint32_t _taskId;

    /** The current OS process ID that is executing on this processor. This is
     * used to generate a taskId */
    uint32_t _pid;

    /** Is the CPU switched out or active? */
    bool _switchedOut;

    /** Cache the cache line size that we get from the system */
    const unsigned int _cacheLineSize;

    /** Global CPU statistics that are merged into the Root object. */
    struct GlobalStats : public statistics::Group
    {
        GlobalStats(statistics::Group *parent);

        statistics::Value simInsts;
        statistics::Value simOps;

        statistics::Formula hostInstRate;
        statistics::Formula hostOpRate;
    };

    /**
     * Pointer to the global stat structure. This needs to be
     * constructed from regStats since we merge it into the root
     * group. */
    static std::unique_ptr<GlobalStats> globalStats;

  public:

    /**
     * Purely virtual method that returns a reference to the data
     * port. All subclasses must implement this method.
     *
     * @return a reference to the data port
     */
    virtual Port &getDataPort() = 0;

    /**
     * Purely virtual method that returns a reference to the instruction
     * port. All subclasses must implement this method.
     *
     * @return a reference to the instruction port
     */
    virtual Port &getInstPort() = 0;

    /** Reads this CPU's ID. */
    int cpuId() const { return _cpuId; }

    /** Reads this CPU's Socket ID. */
    uint32_t socketId() const { return _socketId; }

    /** Reads this CPU's unique data requestor ID */
    RequestorID dataRequestorId() const { return _dataRequestorId; }
    /** Reads this CPU's unique instruction requestor ID */
    RequestorID instRequestorId() const { return _instRequestorId; }

    /**
     * Get a port on this CPU. All CPUs have a data and
     * instruction port, and this method uses getDataPort and
     * getInstPort of the subclasses to resolve the two ports.
     *
     * @param if_name the port name
     * @param idx ignored index
     *
     * @return a reference to the port with the given name
     */
    Port &getPort(const std::string &if_name,
                  PortID idx=InvalidPortID) override;

    /** Get cpu task id */
    uint32_t taskId() const { return _taskId; }
    /** Set cpu task id */
    void taskId(uint32_t id) { _taskId = id; }

    uint32_t getPid() const { return _pid; }
    void setPid(uint32_t pid) { _pid = pid; }

    inline void workItemBegin() { baseStats.numWorkItemsStarted++; }
    inline void workItemEnd() { baseStats.numWorkItemsCompleted++; }
    // @todo remove me after debugging with legion done
    Tick instCount() { return instCnt; }

  protected:
    std::vector<BaseInterrupts*> interrupts;

  public:
    BaseInterrupts *
    getInterruptController(ThreadID tid)
    {
        if (interrupts.empty())
            return NULL;

        assert(interrupts.size() > tid);
        return interrupts[tid];
    }

    virtual void wakeup(ThreadID tid) = 0;

    void postInterrupt(ThreadID tid, int int_num, int index);

    void
    clearInterrupt(ThreadID tid, int int_num, int index)
    {
        interrupts[tid]->clear(int_num, index);
    }

    void
    clearInterrupts(ThreadID tid)
    {
        interrupts[tid]->clearAll();
    }

    bool
    checkInterrupts(ThreadID tid) const
    {
        return FullSystem && interrupts[tid]->checkInterrupts();
    }

  protected:
    std::vector<ThreadContext *> threadContexts;

    Trace::InstTracer * tracer;

  public:


    /** Invalid or unknown Pid. Possible when operating system is not present
     *  or has not assigned a pid yet */
    static const uint32_t invldPid = std::numeric_limits<uint32_t>::max();

    /// Provide access to the tracer pointer
    Trace::InstTracer * getTracer() { return tracer; }

    /// Notify the CPU that the indicated context is now active.
    virtual void activateContext(ThreadID thread_num);

    /// Notify the CPU that the indicated context is now suspended.
    /// Check if possible to enter a lower power state
    virtual void suspendContext(ThreadID thread_num);

    /// Notify the CPU that the indicated context is now halted.
    virtual void haltContext(ThreadID thread_num);

    /// Given a Thread Context pointer return the thread num
    int findContext(ThreadContext *tc);

    /// Given a thread num get tho thread context for it
    virtual ThreadContext *getContext(int tn) { return threadContexts[tn]; }

    /// Get the number of thread contexts available
    unsigned
    numContexts()
    {
        return static_cast<unsigned>(threadContexts.size());
    }

    /// Convert ContextID to threadID
    ThreadID
    contextToThread(ContextID cid)
    {
        return static_cast<ThreadID>(cid - threadContexts[0]->contextId());
    }

  public:
    PARAMS(BaseCPU);
    BaseCPU(const Params &params, bool is_checker = false);
    virtual ~BaseCPU();

    void init() override;
    void startup() override;
    void regStats() override;

    void regProbePoints() override;

    void registerThreadContexts();

    // Functions to deschedule and reschedule the events to enter the
    // power gating sleep before and after checkpoiting respectively.
    void deschedulePowerGatingEvent();
    void schedulePowerGatingEvent();

    /**
     * Prepare for another CPU to take over execution.
     *
     * When this method exits, all internal state should have been
     * flushed. After the method returns, the simulator calls
     * takeOverFrom() on the new CPU with this CPU as its parameter.
     */
    virtual void switchOut();

    /**
     * Load the state of a CPU from the previous CPU object, invoked
     * on all new CPUs that are about to be switched in.
     *
     * A CPU model implementing this method is expected to initialize
     * its state from the old CPU and connect its memory (unless they
     * are already connected) to the memories connected to the old
     * CPU.
     *
     * @param cpu CPU to initialize read state from.
     */
    virtual void takeOverFrom(BaseCPU *cpu);

    /**
     * Flush all TLBs in the CPU.
     *
     * This method is mainly used to flush stale translations when
     * switching CPUs. It is also exported to the Python world to
     * allow it to request a TLB flush after draining the CPU to make
     * it easier to compare traces when debugging
     * handover/checkpointing.
     */
    virtual void flushTLBs();

    /**
     * Determine if the CPU is switched out.
     *
     * @return True if the CPU is switched out, false otherwise.
     */
    bool switchedOut() const { return _switchedOut; }

    /**
     * Verify that the system is in a memory mode supported by the
     * CPU.
     *
     * Implementations are expected to query the system for the
     * current memory mode and ensure that it is what the CPU model
     * expects. If the check fails, the implementation should
     * terminate the simulation using fatal().
     */
    virtual void verifyMemoryMode() const { };

    /**
     *  Number of threads we're actually simulating (<= SMT_MAX_THREADS).
     * This is a constant for the duration of the simulation.
     */
    ThreadID numThreads;

    System *system;

    /**
     * Get the cache line size of the system.
     */
    inline unsigned int cacheLineSize() const { return _cacheLineSize; }

    /**
     * Serialize this object to the given output stream.
     *
     * @note CPU models should normally overload the serializeThread()
     * method instead of the serialize() method as this provides a
     * uniform data format for all CPU models and promotes better code
     * reuse.
     *
     * @param cp The stream to serialize to.
     */
    void serialize(CheckpointOut &cp) const override;

    /**
     * Reconstruct the state of this object from a checkpoint.
     *
     * @note CPU models should normally overload the
     * unserializeThread() method instead of the unserialize() method
     * as this provides a uniform data format for all CPU models and
     * promotes better code reuse.

     * @param cp The checkpoint use.
     */
    void unserialize(CheckpointIn &cp) override;

    /**
     * Serialize a single thread.
     *
     * @param cp The stream to serialize to.
     * @param tid ID of the current thread.
     */
    virtual void serializeThread(CheckpointOut &cp, ThreadID tid) const {};

    /**
     * Unserialize one thread.
     *
     * @param cp The checkpoint use.
     * @param tid ID of the current thread.
     */
    virtual void unserializeThread(CheckpointIn &cp, ThreadID tid) {};

    virtual Counter totalInsts() const = 0;

    virtual Counter totalOps() const = 0;

    /**
     * Schedule an event that exits the simulation loops after a
     * predefined number of instructions.
     *
     * This method is usually called from the configuration script to
     * get an exit event some time in the future. It is typically used
     * when the script wants to simulate for a specific number of
     * instructions rather than ticks.
     *
     * @param tid Thread monitor.
     * @param insts Number of instructions into the future.
     * @param cause Cause to signal in the exit event.
     */
    void scheduleInstStop(ThreadID tid, Counter insts, const char *cause);

    /**
     * Get the number of instructions executed by the specified thread
     * on this CPU. Used by Python to control simulation.
     *
     * @param tid Thread monitor
     * @return Number of instructions executed
     */
    uint64_t getCurrentInstCount(ThreadID tid);

  public:
    /**
     * @{
     * @name PMU Probe points.
     */

    /**
     * Helper method to trigger PMU probes for a committed
     * instruction.
     *
     * @param inst Instruction that just committed
     * @param pc PC of the instruction that just committed
     */
    virtual void probeInstCommit(const StaticInstPtr &inst, Addr pc);

   protected:
    /**
     * Helper method to instantiate probe points belonging to this
     * object.
     *
     * @param name Name of the probe point.
     * @return A unique_ptr to the new probe point.
     */
    probing::PMUUPtr pmuProbePoint(const char *name);

    /**
     * Instruction commit probe point.
     *
     * This probe point is triggered whenever one or more instructions
     * are committed. It is normally triggered once for every
     * instruction. However, CPU models committing bundles of
     * instructions may call notify once for the entire bundle.
     */
    probing::PMUUPtr ppRetiredInsts;
    probing::PMUUPtr ppRetiredInstsPC;

    /** Retired load instructions */
    probing::PMUUPtr ppRetiredLoads;
    /** Retired store instructions */
    probing::PMUUPtr ppRetiredStores;

    /** Retired branches (any type) */
    probing::PMUUPtr ppRetiredBranches;

    /** CPU cycle counter even if any thread Context is suspended*/
    probing::PMUUPtr ppAllCycles;

    /** CPU cycle counter, only counts if any thread contexts is active **/
    probing::PMUUPtr ppActiveCycles;

    /**
     * ProbePoint that signals transitions of threadContexts sets.
     * The ProbePoint reports information through it bool parameter.
     * - If the parameter is true then the last enabled threadContext of the
     * CPU object was disabled.
     * - If the parameter is false then a threadContext was enabled, all the
     * remaining threadContexts are disabled.
     */
    ProbePointArg<bool> *ppSleeping;
    /** @} */

    enum CPUState
    {
        CPU_STATE_ON,
        CPU_STATE_SLEEP,
        CPU_STATE_WAKEUP
    };

    Cycles previousCycle;
    CPUState previousState;

    /** base method keeping track of cycle progression **/
    inline void
    updateCycleCounters(CPUState state)
    {
        uint32_t delta = curCycle() - previousCycle;

        if (previousState == CPU_STATE_ON) {
            ppActiveCycles->notify(delta);
        }

        switch (state) {
          case CPU_STATE_WAKEUP:
            ppSleeping->notify(false);
            break;
          case CPU_STATE_SLEEP:
            ppSleeping->notify(true);
            break;
          default:
            break;
        }

        ppAllCycles->notify(delta);

        previousCycle = curCycle();
        previousState = state;
    }

    // Function tracing
  private:
    bool functionTracingEnabled;
    std::ostream *functionTraceStream;
    Addr currentFunctionStart;
    Addr currentFunctionEnd;
    Tick functionEntryTick;
    void enableFunctionTrace();
    void traceFunctionsInternal(Addr pc);

  private:
    static std::vector<BaseCPU *> cpuList;   //!< Static global cpu list

  public:
    void
    traceFunctions(Addr pc)
    {
        if (functionTracingEnabled)
            traceFunctionsInternal(pc);
    }

    static int numSimulatedCPUs() { return cpuList.size(); }
    static Counter
    numSimulatedInsts()
    {
        Counter total = 0;

        int size = cpuList.size();
        for (int i = 0; i < size; ++i)
            total += cpuList[i]->totalInsts();

        return total;
    }

    static Counter
    numSimulatedOps()
    {
        Counter total = 0;

        int size = cpuList.size();
        for (int i = 0; i < size; ++i)
            total += cpuList[i]->totalOps();

        return total;
    }

  public:
    struct BaseCPUStats : public statistics::Group
    {
        BaseCPUStats(statistics::Group *parent);
        // Number of CPU cycles simulated
        statistics::Scalar numCycles;
        statistics::Scalar numWorkItemsStarted;
        statistics::Scalar numWorkItemsCompleted;
    } baseStats;

  private:
    std::vector<AddressMonitor> addressMonitor;

  public:
    void armMonitor(ThreadID tid, Addr address);
    bool mwait(ThreadID tid, PacketPtr pkt);
    void mwaitAtomic(ThreadID tid, ThreadContext *tc, BaseMMU *mmu);
    AddressMonitor *
    getCpuAddrMonitor(ThreadID tid)
    {
        assert(tid < numThreads);
        return &addressMonitor[tid];
    }

    Cycles syscallRetryLatency;

    /** This function is used to instruct the memory subsystem that a
     * transaction should be aborted and the speculative state should be
     * thrown away.  This is called in the transaction's very last breath in
     * the core.  Afterwards, the core throws away its speculative state and
     * resumes execution at the point the transaction started, i.e. reverses
     * time.  When instruction execution resumes, the core expects the
     * memory subsystem to be in a stable, i.e. pre-speculative, state as
     * well. */
    virtual void
    htmSendAbortSignal(ThreadID tid, uint64_t htm_uid,
                       HtmFailureFaultCause cause)
    {
        panic("htmSendAbortSignal not implemented");
    }

  // Enables CPU to enter power gating on a configurable cycle count
  protected:
    void enterPwrGating();

    const Cycles pwrGatingLatency;
    const bool powerGatingOnIdle;
    EventFunctionWrapper enterPwrGatingEvent;

    const uint64_t warmupInstCount;

    //const uint64_t repeatDumpInstCount;

    uint64_t nextDumpInstCount{0};

    // difftest
  protected:
    bool enableDifftest;
    bool dumpCommitFlag;
    int dumpStartNum;
    bool enableRVV{false};
    std::shared_ptr<DiffAllStates> diffAllStates{};

    virtual void readGem5Regs()
    {
        panic("difftest:readGem5Regs() is not implemented\n");
    }
    std::pair<int, bool> diffWithNEMU(ThreadID tid, InstSeqNum seq);

  public:
    struct
    {
        gem5::StaticInstPtr inst;
        // the result of currently inst
        gem5::RegVal result;
        uint64_t vecResult[RiscvISA::NumVecElemPerVecReg];
        // the lambda expr of get srcOperand
        gem5::RegVal getSrcReg(const gem5::RegId &regid) { return 0; };
        const gem5::PCStateBase *pc;
        bool curInstStrictOrdered{false};
        gem5::Addr physEffAddr;
        // Register address causing difftest error
        bool errorRegsValue[96];// 32 regs + 32fprs +32 vprs
        bool errorCsrsValue[32];// CsrRegIndex
        bool errorPcValue;

        std::queue<std::string> lastCommittedMsg;
    } diffInfo;


    virtual RegVal readMiscRegNoEffect(int misc_reg, ThreadID tid) const
    {
        panic("difftest:readGem5Regs() is not implemented\n");
        return 0;
    }

    virtual RegVal readMiscReg(int misc_reg, ThreadID tid)
    {
        panic("difftest:readGem5Regs() is not implemented\n");
        return 0;
    }

    void difftestStep(ThreadID tid) { difftestStep(tid, 0);}

    void difftestStep(ThreadID tid, InstSeqNum seq);

    inline bool difftestEnabled() const { return enableDifftest; }

    void displayGem5Regs();

    void difftestRaiseIntr(uint64_t no);

    void setSCSuccess(bool success, paddr_t addr);

    void setExceptionGuideExecInfo(uint64_t exception_num, uint64_t mtval,
                          uint64_t stval,
                          // force set jump target
                          bool force_set_jump_target, uint64_t jump_target);

    void clearGuideExecInfo();

    void enableDiffPrint();

    std::pair<bool, std::shared_ptr<DiffAllStates>> getDiffAllStates()
    {
        return std::make_pair(enableDifftest, diffAllStates);
    }

    void takeOverDiffAllStates(std::shared_ptr<DiffAllStates> diffAllStates)
    {
        this->diffAllStates = diffAllStates;
    }

    int committedInstNum = 0;

    std::vector<std::pair<Addr, std::string>> committedInsts;
};

} // namespace gem5

#endif // !IS_NULL_ISA

#endif // __CPU_BASE_HH__
