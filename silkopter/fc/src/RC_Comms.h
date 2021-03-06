#pragma once

#include "common/Comm_Data.h"
#include "HAL.h"

#include "common/Manual_Clock.h"
#include "common/node/ISource.h"
#include "common/stream/ICamera_Commands.h"
#include "common/stream/IMultirotor_Commands.h"
#include "common/stream/IMultirotor_State.h"
#include "common/stream/IVideo.h"
#include "utils/comms/RC_Phy.h"
#include "utils/comms/RC_Protocol.h"
#include "utils/comms/Video_Streamer.h"
#include "utils/Clock.h"
#include "utils/hw/ISPI.h"

namespace util
{
namespace comms
{
class RCP;
class ISocket;
}
}

namespace silk
{

class RC_Comms : q::util::Noncopyable
{
public:
    RC_Comms(HAL& hal);

    bool start();

    bool is_connected() const;

    void process();

    util::comms::RC_Phy const& get_rc_phy() const;
    util::comms::RC_Phy& get_rc_phy();

    boost::optional<stream::IMultirotor_Commands::Value> const& get_multirotor_commands() const;
    boost::optional<stream::ICamera_Commands::Value> const& get_camera_commands() const;

    void set_multirotor_state(stream::IMultirotor_State::Value const& value);
    void add_video_data(stream::IVideo::Value const& value);

    struct Impl; //this needs to be public...
private:
    bool compute_multirotor_state_packet(util::comms::RC_Protocol::Buffer& buffer, uint8_t& packet_type);
    void process_rx_packet(util::comms::RC_Protocol::RX_Packet const& packet, uint8_t* data, size_t size);

    HAL& m_hal;
    Clock::time_point m_uav_sent_tp = Clock::now();

    std::unique_ptr<util::hw::ISPI> m_spi;

    boost::optional<stream::ICamera_Commands::Value> m_camera_commands;
    boost::optional<stream::IMultirotor_Commands::Value> m_multirotor_commands;

    mutable std::mutex m_new_camera_commands_mutex;
    boost::optional<stream::ICamera_Commands::Value> m_new_camera_commands;

    mutable std::mutex m_new_multirotor_commands_mutex;
    boost::optional<stream::IMultirotor_Commands::Value> m_new_multirotor_commands;

    mutable std::mutex m_multirotor_state_mutex;
    stream::IMultirotor_State::Value m_multirotor_state;
    std::vector<uint8_t> m_multirotor_state_sz_buffer;
    std::vector<uint8_t> m_rx_packet_sz_buffer;

    util::comms::RC_Phy m_rc_phy;
    util::comms::RC_Protocol m_rc_protocol;
    util::comms::RC_Protocol::RX_Packet m_rx_packet;
    util::comms::Video_Streamer m_video_streamer;

    silk::rc_comms::Packet_Type m_next_packet_type = silk::rc_comms::Packet_Type::MULTIROTOR_STATE_PART1;
    bool m_send_home = false;
    Clock::time_point m_last_home_sent_tp = Clock::now();

    bool m_is_connected = false;

};


}
