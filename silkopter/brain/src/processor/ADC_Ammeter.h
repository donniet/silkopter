#pragma once

#include "HAL.h"
#include "common/node/stream/IADC.h"
#include "common/node/stream/ICurrent.h"
#include "common/node/IProcessor.h"


namespace sz
{
namespace ADC_Ammeter
{
struct Init_Params;
struct Config;
}
}

namespace silk
{
namespace node
{

class ADC_Ammeter : public IProcessor
{
public:
    ADC_Ammeter(HAL& hal);

    auto init(rapidjson::Value const& init_params) -> bool;
    auto get_init_params() const -> rapidjson::Document const&;

    auto set_config(rapidjson::Value const& json) -> bool;
    auto get_config() const -> rapidjson::Document;

    auto get_inputs() const -> std::vector<Input>;
    auto get_outputs() const -> std::vector<Output>;

    void process();

private:
    auto init() -> bool;

    HAL& m_hal;

    rapidjson::Document m_init_paramsj;
    std::shared_ptr<sz::ADC_Ammeter::Init_Params> m_init_params;
    std::shared_ptr<sz::ADC_Ammeter::Config> m_config;

    stream::IADC_wptr m_adc_stream;

    struct Stream : public stream::ICurrent
    {
        auto get_samples() const -> std::vector<Sample> const& { return samples; }
        auto get_rate() const -> uint32_t { return rate; }

        uint32_t rate = 0;
        std::vector<Sample> samples;
        uint32_t sample_idx = 0;
    };
    mutable std::shared_ptr<Stream> m_output_stream;
};


}
}

