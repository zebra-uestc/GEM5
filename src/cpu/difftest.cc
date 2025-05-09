#include "cpu/difftest.hh"

#include <dlfcn.h>

#include <cstdio>
#include <cstdlib>

#include "base/trace.hh"
#include "cpu/nemu_common.hh"
#include "debug/ValueCommit.hh"

const char *reg_name[] = {
    "$0",     "ra",    "sp",       "gp",       "tp",      "t0",
    "t1",     "t2",    "s0",       "s1",       "a0",      "a1",
    "a2",     "a3",    "a4",       "a5",       "a6",      "a7",
    "s2",     "s3",    "s4",       "s5",       "s6",      "s7",
    "s8",     "s9",    "s10",      "s11",      "t3",      "t4",
    "t5",     "t6",
    "ft0",    "ft1",   "ft2",      "ft3",      "ft4",     "ft5",
    "ft6",    "ft7",   "fs0",      "fs1",      "fa0",     "fa1",
    "fa2",    "fa3",   "fa4",      "fa5",      "fa6",     "fa7",
    "fs2",    "fs3",   "fs4",      "fs5",      "fs6",     "fs7",
    "fs8",    "fs9",   "fs10",     "fs11",     "ft8",     "ft9",
    "ft10",   "ft11",
    "mode",   "mstatus","sstatus",  "mepc",    "sepc",    "mtval",
    "stval",  "mtvec",   "stvec",   "mcause",  "scause",  "satp",
    "mip",    "mie",     "mscratch","sscratch","mideleg", "medeleg",
    "this_pc",
    "v0",     "v1",    "v2",       "v3",       "v4",      "v5",
    "v6",     "v7",    "v8",       "v9",       "v10",     "v11",
    "v12",    "v13",   "v14",      "v15",      "v16",     "v17",
    "v18",    "v19",   "v20",      "v21",      "v22",     "v23",
    "v24",    "v25",   "v26",      "v27",      "v28",     "v29",
    "v30",    "v31"
    };

const std::vector<uint64_t> skipCSRs = {
  0xb0200073,
  0xb0000073
};

NemuProxy::NemuProxy(int coreid, const char *ref_so, bool enable_sdcard_diff)
{
    void *handle = dlmopen(LM_ID_NEWLM, ref_so, RTLD_LAZY | RTLD_DEEPBIND);
    printf("Using %s for difftest\n", ref_so);
    if (!handle) {
        printf("%s\n", dlerror());
        assert(0);
    }

    this->memcpy = (void (*)(paddr_t, void *, size_t, bool))dlsym(
        handle, "difftest_memcpy");
    assert(this->memcpy);

    regcpy = (void (*)(void *, bool))dlsym(handle, "difftest_regcpy");
    assert(regcpy);

    csrcpy = (void (*)(void *, bool))dlsym(handle, "difftest_csrcpy");
    assert(csrcpy);

    uarchstatus_cpy =
        (void (*)(void *, bool))dlsym(handle, "difftest_uarchstatus_cpy");
    assert(uarchstatus_cpy);

    exec = (void (*)(uint64_t))dlsym(handle, "difftest_exec");
    assert(exec);

    guided_exec = (vaddr_t(*)(void *))dlsym(handle, "difftest_guided_exec");
    assert(guided_exec);

    update_config = (vaddr_t(*)(void *))dlsym(handle, "update_dynamic_config");
    assert(update_config);

    store_commit = (int (*)(uint64_t *, uint64_t *, uint8_t *))dlsym(
        handle, "difftest_store_commit");
    assert(store_commit);

    raise_intr = (void (*)(uint64_t))dlsym(handle, "difftest_raise_intr");
    assert(raise_intr);

    isa_reg_display = (void (*)(void))dlsym(handle, "isa_reg_display");
    assert(isa_reg_display);

    query = (void (*)(void *, uint64_t))dlsym(handle, "difftest_query_ref");
#ifdef ENABLE_RUNHEAD
    assert(query);
#endif

    auto nemu_difftest_set_mhartid =
        (void (*)(int))dlsym(handle, "difftest_set_mhartid");
    if (NUM_CORES > 1) {
        assert(nemu_difftest_set_mhartid);
        nemu_difftest_set_mhartid(coreid);
    }
    if (enable_sdcard_diff) {
        sdcard_init = (void (*)(const char *, const char *))dlsym(
            handle, "difftest_sdcard_init");
        assert(sdcard_init);
    }

    auto nemu_init = (void (*)(void))dlsym(handle, "difftest_init");
    assert(nemu_init);

    nemu_init();
}


SpikeProxy::SpikeProxy(int coreid, const char *ref_so, bool enable_sdcard_diff)
{
    void *handle = dlmopen(LM_ID_NEWLM, ref_so, RTLD_LAZY | RTLD_DEEPBIND);
    printf("Using %s for difftest\n", ref_so);
    if (!handle) {
        printf("%s\n", dlerror());
        assert(0);
    }

    this->memcpy = (void (*)(paddr_t, void *, size_t, bool))dlsym(
        handle, "difftest_memcpy");
    assert(this->memcpy);

    regcpy = (void (*)(void *, bool))dlsym(handle, "difftest_regcpy");
    assert(regcpy);

    csrcpy = (void (*)(void *, bool))dlsym(handle, "difftest_csrcpy");
    assert(csrcpy);

    uarchstatus_cpy =
        (void (*)(void *, bool))dlsym(handle, "difftest_uarchstatus_sync");
    assert(uarchstatus_cpy);

    exec = (void (*)(uint64_t))dlsym(handle, "difftest_exec");
    assert(exec);

    guided_exec = (vaddr_t(*)(void *))dlsym(handle, "difftest_guided_exec");
    assert(guided_exec);

    update_config = (vaddr_t(*)(void *))dlsym(handle, "update_dynamic_config");
    assert(update_config);

    store_commit = (int (*)(uint64_t *, uint64_t *, uint8_t *))dlsym(
        handle, "difftest_store_commit");
    assert(store_commit);

    raise_intr = (void (*)(uint64_t))dlsym(handle, "difftest_raise_intr");
    assert(raise_intr);

    isa_reg_display = (void (*)(void))dlsym(handle, "isa_reg_display");
    assert(isa_reg_display);

//     query = (void (*)(void *, uint64_t))dlsym(handle, "difftest_query_ref");
// #ifdef ENABLE_RUNHEAD
//     assert(query);
// #endif

    auto nemu_difftest_set_mhartid =
        (void (*)(int))dlsym(handle, "difftest_set_mhartid");
    if (NUM_CORES > 1) {
        assert(nemu_difftest_set_mhartid);
        nemu_difftest_set_mhartid(coreid);
    }

    assert(!enable_sdcard_diff);
    // if (enable_sdcard_diff) {
    //     sdcard_init = (void (*)(const char *, const char *))dlsym(
    //         handle, "difftest_sdcard_init");
    //     assert(sdcard_init);
    // }

    auto nemu_init = (void (*)(void))dlsym(handle, "difftest_init");
    assert(nemu_init);

    nemu_init();
}

