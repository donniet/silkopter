#pragma once

#include "HAL.h"
#include "common/stream/IPressure.h"
#include "common/stream/IVelocity.h"
#include "common/node/IProcessor.h"

#include "Sample_Accumulator.h"
#include "Basic_Output_Stream.h"

namespace sz
{
namespace Pressure_Velocity
{
struct Init_Params;
struct Config;
}
}

namespace silk
{
namespace node
{

class Pressure_Velocity : public IProcessor
{
public:
    Pressure_Velocity(HAL& hal);

    auto init(rapidjson::Value const& init_params) -> bool;
    auto get_init_params() const -> rapidjson::Document;

    auto set_config(rapidjson::Value const& json) -> bool;
    auto get_config() const -> rapidjson::Document;

    auto send_message(rapidjson::Value const& json) -> rapidjson::Document;

    auto start(q::Clock::time_point tp) -> bool override;

    void set_input_stream_path(size_t idx, q::Path const& path);
    auto get_inputs() const -> std::vector<Input>;
    auto get_outputs() const -> std::vector<Output>;

    void process();

private:
    auto init() -> bool;

    HAL& m_hal;

    std::shared_ptr<sz::Pressure_Velocity::Init_Params> m_init_params;
    std::shared_ptr<sz::Pressure_Velocity::Config> m_config;

    Sample_Accumulator<stream::IPressure> m_accumulator;

    boost::optional<float> m_last_altitude;

    typedef Basic_Output_Stream<stream::IENU_Velocity> Output_Stream;
    mutable std::shared_ptr<Output_Stream> m_output_stream;
};


}
}
