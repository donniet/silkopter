#pragma once

#include "UAV.h"
#include "common/node/ISource.h"
#include "common/stream/IDistance.h"
#include "common/bus/II2C.h"

#include "Basic_Output_Stream.h"


namespace silk
{
struct SRF02_Descriptor;
struct SRF02_Config;
}


namespace silk
{
namespace node
{

class SRF02 : public ISource
{
public:
    SRF02(UAV& uav);

    bool init(std::shared_ptr<Node_Descriptor_Base> descriptor) override;
    std::shared_ptr<Node_Descriptor_Base> get_descriptor() const override;

    bool set_config(std::shared_ptr<Node_Config_Base> config) override;
    std::shared_ptr<Node_Config_Base> get_config() const override;

    auto send_message(rapidjson::Value const& json) -> rapidjson::Document;

    auto start(q::Clock::time_point tp) -> bool override;

    auto get_outputs() const -> std::vector<Output>;

    void process();

private:
    auto init() -> bool;

    void trigger(bus::II2C& bus);

    UAV& m_uav;

    std::weak_ptr<bus::II2C> m_bus;

    std::shared_ptr<SRF02_Descriptor> m_descriptor;
    std::shared_ptr<SRF02_Config> m_config;

    typedef Basic_Output_Stream<stream::IDistance> Output_Stream;
    mutable std::shared_ptr<Output_Stream> m_output_stream;
    q::Clock::time_point m_last_trigger_tp;

};

}
}
