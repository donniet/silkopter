#include "FCStdAfx.h"
#include "Oscillator.h"

#include "hal.def.h"
//#include "sz_Oscillator.hpp"

namespace silk
{
namespace node
{

Oscillator::Oscillator(HAL& hal)
    : m_hal(hal)
    , m_descriptor(new hal::Oscillator_Descriptor())
    , m_config(new hal::Oscillator_Config())
    , m_rnd_distribution(0, 0)
{
    m_output_stream = std::make_shared<Output_Stream>();
}

ts::Result<void> Oscillator::init(hal::INode_Descriptor const& descriptor)
{
    QLOG_TOPIC("Oscillator::init");

    auto specialized = dynamic_cast<hal::Oscillator_Descriptor const*>(&descriptor);
    if (!specialized)
    {
        return make_error("Wrong descriptor type");
    }
    *m_descriptor = *specialized;

    return init();
}
ts::Result<void> Oscillator::init()
{
    m_output_stream->set_rate(m_descriptor->get_rate());

    auto components = m_config->get_components();
    components.resize(m_descriptor->get_component_count());
    m_config->set_components(components);

    return ts::success;
}

ts::Result<void> Oscillator::start(Clock::time_point tp)
{
    m_output_stream->set_tp(tp);
    return ts::success;
}

auto Oscillator::get_inputs() const -> std::vector<Input>
{
    return std::vector<Input>(0);
}

auto Oscillator::get_outputs() const -> std::vector<Output>
{
    std::vector<Output> outputs =
    {{
         { "output", m_output_stream }
    }};
    return outputs;
}

void Oscillator::process()
{
    QLOG_TOPIC("Oscillator::process");

    m_output_stream->clear();

    float amplitude = m_config->get_amplitude();

    constexpr size_t MAX_SAMPLES_NEEDED = 10000;

    size_t samples_needed = m_output_stream->compute_samples_needed();
    if (samples_needed > MAX_SAMPLES_NEEDED)
    {
        QLOGW("Too many samples needed: {}. Clamping to an arbitrary MAX: {}", samples_needed, MAX_SAMPLES_NEEDED);
        samples_needed = MAX_SAMPLES_NEEDED;
    }

    float period = std::chrono::duration<float>(m_output_stream->get_dt()).count();

    while (samples_needed > 0)
    {
       float value = 0;

       m_period += period;

       float a = m_period * math::anglef::_2pi;
       for (auto& c: m_config->get_components())
       {
           float v = math::sin(a * c.get_frequency()) * 0.5f;
           if (c.get_square())
           {
               v = v < 0.f ? -0.5f : 0.5f;
           }

           value += v * c.get_amplitude();
       }
       if (m_has_noise)
       {
           value += m_rnd_distribution(m_rnd_engine);
       }

       value *= amplitude;

       m_output_stream->push_sample(value, true);

       samples_needed--;
    }
}

ts::Result<void> Oscillator::set_input_stream_path(size_t idx, std::string const& path)
{
    return ts::success;
}

ts::Result<void> Oscillator::set_config(hal::INode_Config const& config)
{
    QLOG_TOPIC("Oscillator::set_config");

    auto specialized = dynamic_cast<hal::Oscillator_Config const*>(&config);
    if (!specialized)
    {
        return make_error("Wrong config type");
    }
    *m_config = *specialized;

    m_rnd_distribution = std::uniform_real_distribution<float>(-m_config->get_noise()*0.5f, m_config->get_noise()*0.5f);
    m_has_noise = !math::is_zero(m_config->get_noise(), math::epsilon<float>());

    return ts::success;
}
auto Oscillator::get_config() const -> std::shared_ptr<const hal::INode_Config>
{
    return m_config;
}

auto Oscillator::get_descriptor() const -> std::shared_ptr<const hal::INode_Descriptor>
{
    return m_descriptor;
}

ts::Result<std::shared_ptr<messages::INode_Message>> Oscillator::send_message(messages::INode_Message const& message)
{
    return make_error("Unknown message");
}


}
}
