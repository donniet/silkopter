#pragma once

#include "ISPI.h"
#include <mutex>
#include <vector>

namespace util
{
namespace hw
{

class SPI_BCM : public ISPI
{
public:
    SPI_BCM();
    ~SPI_BCM();

    ts::Result<void> init(uint32_t device, uint32_t speed, uint32_t mode);

    void lock() override;
    bool try_lock() override;
    void unlock() override;

    bool transfer(void const* tx_data, void* rx_data, size_t size, uint32_t speed = 0) override;
    bool transfer_register(uint8_t reg, void const* tx_data, void* rx_data, size_t size, uint32_t speed = 0) override;

private:
    bool do_transfer(void const* tx_data, void* rx_data, size_t size, uint32_t speed);

    uint32_t get_divider(uint32_t speed) const;

    uint32_t m_device = 0;
    uint32_t m_speed = 0;
    uint32_t m_mode = 0;

    uint32_t m_clock_divider = 0;

    mutable std::vector<uint8_t> m_tx_buffer;
    mutable std::vector<uint8_t> m_rx_buffer;

    mutable std::recursive_mutex m_mutex;
};

}
}