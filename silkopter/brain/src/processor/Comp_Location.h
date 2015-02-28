#pragma once

#include "common/node/processor/IProcessor.h"
#include "common/node/stream/ILinear_Acceleration.h"
#include "common/node/stream/ILocation.h"
#include "common/node/stream/IPressure.h"

#include "HAL.h"


namespace sz
{
namespace Comp_Location
{
class Init_Params;
class Config;
}
}


namespace silk
{
namespace node
{
namespace processor
{

class Comp_Location : public IProcessor
{
public:
    Comp_Location(HAL& hal);

    auto init(rapidjson::Value const& json) -> bool;
    auto get_init_params() -> boost::optional<rapidjson::Value const&>;

    auto set_config(rapidjson::Value const& json) -> bool;
    auto get_config() -> boost::optional<rapidjson::Value const&>;

    auto get_input_stream_count() const -> size_t;
    auto get_input_stream(size_t idx) -> stream::IStream&;
    auto get_output_stream_count() const -> size_t;
    auto get_output_stream(size_t idx) -> stream::ILocation&;

    auto get_name() const -> std::string const&;

    void process();

private:
    auto init() -> bool;

    HAL& m_hal;

    std::shared_ptr<sz::Comp_Location::Init_Params> m_init_params;
    rapidjson::Document m_init_params_json;

    std::shared_ptr<sz::Comp_Location::Config> m_config;
    rapidjson::Document m_config_json;

    q::Clock::duration m_dt = q::Clock::duration(0);

    stream::ILocation* m_location_stream = nullptr;
    stream::ILinear_Acceleration* m_linear_acceleration_stream = nullptr;
    stream::IPressure* m_pressure_stream = nullptr;

    std::vector<stream::ILocation::Sample> m_location_samples;
    std::vector<stream::ILinear_Acceleration::Sample> m_linear_acceleration_samples;
    std::vector<stream::IPressure::Sample> m_pressure_samples;

    struct Stream : public stream::ILocation
    {
        auto get_samples() const -> std::vector<Sample> const& { return samples; }
        auto get_rate() const -> uint32_t { return rate; }
        auto get_name() const -> std::string const& { return name; }

        std::string name;
        Sample last_sample;
        std::vector<Sample> samples;
        uint32_t rate = 0;
    } m_stream;
};



}
}
}
