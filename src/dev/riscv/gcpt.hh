#ifndef __DEV_GCPT_HH__
#define __DEV_GCPT_HH__

#include "dev/io_device.hh"
#include "dev/serial/serial.hh"
#include "params/GCPT.hh"

namespace gem5
{
class GCPT : public BasicPioDevice
{
  public:
    Tick read(PacketPtr pkt) override;
    Tick write(PacketPtr pkt) override;

    explicit GCPT(const GCPTParams& params);

  private:
    uint8_t* pmem;
};
}  // namespace gem5


#endif