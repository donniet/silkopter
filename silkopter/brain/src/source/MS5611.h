#pragma once

#include "HAL.h"
#include "common/node/source/ISource.h"
#include "common/node/stream/IPressure.h"
#include "common/node/stream/ITemperature.h"
#include "common/node/bus/II2C.h"
#include "common/node/bus/ISPI.h"

namespace silk
{
namespace node
{
namespace source
{

class MS5611 : public ISource
{
public:
    MS5611(HAL& hal);

    struct Init_Params
    {
        std::string name;
        bus::IBus* bus = nullptr;
        uint32_t rate = 100;
        uint32_t temperature_rate_ratio = 10;
    };

    auto init(rapidjson::Value const& json) -> bool;
    auto init(Init_Params const& params) -> bool;

    auto get_name() const -> std::string const&;
    auto get_output_stream_count() const -> size_t;
    auto get_output_stream(size_t idx) -> stream::IStream&;

    void process();

private:
    auto init() -> bool;

    void lock();
    void unlock();
    auto bus_read(uint8_t reg, uint8_t* data, uint32_t size) -> bool;
    auto bus_read_u8(uint8_t reg, uint8_t& dst) -> bool;
    auto bus_read_u16(uint8_t reg, uint16_t& dst) -> bool;
    auto bus_write(uint8_t reg, uint8_t const* data, uint32_t size) -> bool;
    auto bus_write_u8(uint8_t reg, uint8_t const& t) -> bool;
    auto bus_write_u16(uint8_t reg, uint16_t const& t) -> bool;

    HAL& m_hal;
    bus::II2C* m_i2c = nullptr;
    bus::ISPI* m_spi = nullptr;

    Init_Params m_params;

    struct Pressure : public stream::IPressure
    {
        auto get_samples() const -> std::vector<Sample> const& { return samples; }
        auto get_rate() const -> uint32_t { return rate; }
        auto get_name() const -> std::string const& { return name; }

        uint32_t rate = 0;
        std::vector<Sample> samples;
        Sample last_sample;
        double      reading = 0;
        std::string name;
    } m_pressure;

    struct Temperature : public stream::ITemperature
    {
        auto get_samples() const -> std::vector<Sample> const& { return samples; }
        auto get_rate() const -> uint32_t { return rate; }
        auto get_name() const -> std::string const& { return name; }

        uint32_t rate = 0;
        std::vector<Sample> samples;
        Sample last_sample;
        double      reading = 0;
        std::string name;
    } m_temperature;

    double		m_c1 = 0;
    double		m_c2 = 0;
    double		m_c3 = 0;
    double		m_c4 = 0;
    double		m_c5 = 0;
    double		m_c6 = 0;

    void calculate(q::Clock::duration dt);

    uint8_t         m_stage = 0;

    q::Clock::time_point m_last_timestamp;
    q::Clock::duration m_dt;
};


}
}
}
