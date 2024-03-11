#include <cstdint>

#include "gcpt.hh"
#include "mem/packet_access.hh"

namespace gem5
{
Tick GCPT::read(PacketPtr pkt)
{
    Addr addr = pkt->getAddr();
    unsigned size = pkt->getSize();
    assert(addr >= pioAddr && addr + size < pioAddr + pioSize);
    assert(size <= 8);
    auto offset = addr - pioAddr;

    pkt->setData(pmem + offset);

    pkt->makeAtomicResponse();
    return pioDelay;
}

Tick GCPT::write(PacketPtr pkt)
{
    Addr addr = pkt->getAddr();
    unsigned size = pkt->getSize();
    uint8_t* data = pkt->getPtr<uint8_t>();
    assert(addr >= pioAddr && addr + size < pioAddr + pioSize);
    assert(size <= 8);
    assert(data != nullptr);
    auto offset = addr - pioAddr;

    std::memcpy(pmem + offset, data, size);

    pkt->makeAtomicResponse();
    return pioDelay;
}

GCPT::GCPT(const GCPTParams& params)
    : BasicPioDevice(params, params.pio_size)
{
    pmem = new uint8_t[pioSize];
    std::memset(pmem, 0, pioSize);
}

}  // namespace gem5
