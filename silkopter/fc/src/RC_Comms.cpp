#include "FCStdAfx.h"
#include "RC_Comms.h"
#include "utils/Timed_Scope.h"
#include "utils/hw/SPI_PIGPIO.h"
#include "utils/hw/SPI_Dev.h"

#include "hal.def.h"


namespace silk
{

constexpr uint8_t SDN_GPIO = 6;

//#define USE_SPI_PIGPIO

#ifdef USE_SPI_PIGPIO
constexpr size_t SPI_PORT = 1;
constexpr size_t SPI_CHANNEL = 2;
constexpr size_t SPI_SPEED = 4000000;
#else
const char* SPI_DEVICE = "/dev/spidev1.0";
constexpr size_t SPI_SPEED = 5000000;
#endif

RC_Comms::RC_Comms(HAL& hal)
    : m_hal(hal)
    , m_rc_phy(false)
    , m_rc_protocol(m_rc_phy, std::bind(&RC_Comms::process_rx_packet, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3))
    //, m_video_streamer()
{
}

auto RC_Comms::start() -> bool
{
    silk::hal::IUAV_Descriptor::Comms const& comms_settings = m_hal.get_uav_descriptor()->get_comms();

#ifdef USE_SPI_PIGPIO
    util::hw::SPI_PIGPIO* spi = new util::hw::SPI_PIGPIO();
    m_spi.reset(spi);

    ts::Result<void> result = spi->init(SPI_PORT, SPI_CHANNEL, SPI_SPEED, 0);
#else
    util::hw::SPI_Dev* spi = new util::hw::SPI_Dev();
    m_spi.reset(spi);

    ts::Result<void> result = spi->init("/dev/spidev1.0", SPI_SPEED);
#endif
    if (result != ts::success)
    {
        QLOGW("Cannot start spi: {}", result.error().what());
        return false;
    }

    try
    {
        util::comms::Video_Streamer::TX_Descriptor tx_descriptor;
        tx_descriptor.interface = comms_settings.get_video_wlan_interface();
        tx_descriptor.coding_k = comms_settings.get_fec_coding_k();
        tx_descriptor.coding_n = comms_settings.get_fec_coding_n();

        m_is_connected = m_rc_phy.init(*m_spi, SDN_GPIO) &&
                         m_rc_protocol.init(2, 3) &&
                         m_video_streamer.init_tx(tx_descriptor);
    }
    catch(std::exception e)
    {
        m_is_connected = false;
    }

    if (!m_is_connected)
    {
        QLOGW("Cannot start comms");
        return false;
    }

    m_rc_phy.set_channel(comms_settings.get_rc_channel());
    m_rc_phy.set_xtal_adjustment(comms_settings.get_rc_xtal_ajdustment());

    //m_rc_phy.set_rate(100);
    m_rc_protocol.add_periodic_packet(std::chrono::milliseconds(30), std::bind(&RC_Comms::compute_multirotor_state_packet, this, std::placeholders::_1, std::placeholders::_2));
    m_rc_protocol.reset_session();

    m_send_home = true;

    QLOGI("Started comms");

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

auto RC_Comms::is_connected() const -> bool
{
    return m_is_connected;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

util::comms::RC_Phy const& RC_Comms::get_rc_phy() const
{
    return m_rc_phy;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

util::comms::RC_Phy& RC_Comms::get_rc_phy()
{
    return m_rc_phy;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool RC_Comms::compute_multirotor_state_packet(util::comms::RC_Protocol::Buffer& buffer, uint8_t& packet_type)
{
    packet_type = static_cast<uint8_t>(m_next_packet_type);

    size_t off = buffer.size();
    {
        std::lock_guard<std::mutex> lg(m_multirotor_state_mutex);
        if (m_next_packet_type == silk::rc_comms::Packet_Type::MULTIROTOR_STATE_PART1)
        {
            util::serialization::serialize_part1(buffer, m_multirotor_state, off);
            m_next_packet_type = silk::rc_comms::Packet_Type::MULTIROTOR_STATE_PART2;
        }
        else
        {
            util::serialization::serialize_part2(buffer, m_multirotor_state, off);
            m_next_packet_type = silk::rc_comms::Packet_Type::MULTIROTOR_STATE_PART1;
        }

        //memcpy(data, m_multirotor_state_sz_buffer.data(), off);
    }

    //size = off;

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void RC_Comms::process_rx_packet(util::comms::RC_Protocol::RX_Packet const& packet, uint8_t* data, size_t size)
{
    m_rx_packet.rx_dBm = packet.rx_dBm;
    m_rx_packet.tx_dBm = packet.tx_dBm;
    m_rx_packet.rx_timepoint = packet.rx_timepoint;

    m_rx_packet_sz_buffer.resize(size);
    if (size > 0)
    {
        memcpy(m_rx_packet_sz_buffer.data(), data, size);
    }

    if (packet.packet_type == static_cast<uint8_t>(rc_comms::Packet_Type::MULTIROTOR_COMMANDS))
    {
        size_t off = 0;
        stream::IMultirotor_Commands::Value value;
        if (util::serialization::deserialize(m_rx_packet_sz_buffer, value, off))
        {
            std::lock_guard<std::mutex> lg(m_new_multirotor_commands_mutex);
            m_new_multirotor_commands = value;
        }
        else
        {
            QLOGW("Cannot deserialize incoming multirotor state value");
        }
    }
    else if (packet.packet_type == static_cast<uint8_t>(rc_comms::Packet_Type::CAMERA_COMMANDS))
    {
        size_t off = 0;
        stream::ICamera_Commands::Value value;
        if (util::serialization::deserialize(m_rx_packet_sz_buffer, value, off))
        {
            std::lock_guard<std::mutex> lg(m_new_camera_commands_mutex);
            m_new_camera_commands = value;
        }
        else
        {
            QLOGW("Cannot deserialize incoming camera state value");
        }
    }
    else if (packet.packet_type == static_cast<uint8_t>(rc_comms::Packet_Type::RC_CONNECTED))
    {
        QLOGI("RC connected, sending vital data");
        m_send_home = true;
    }
    else
    {
        QLOGW("Unknown incoming packet type: {}", static_cast<int>(packet.packet_type));
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

boost::optional<stream::IMultirotor_Commands::Value> const& RC_Comms::get_multirotor_commands() const
{
    return m_multirotor_commands;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

boost::optional<stream::ICamera_Commands::Value> const& RC_Comms::get_camera_commands() const
{
    return m_camera_commands;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void RC_Comms::set_multirotor_state(stream::IMultirotor_State::Value const& value)
{
    std::lock_guard<std::mutex> lg(m_multirotor_state_mutex);

    if (value.home_ecef_position != m_multirotor_state.home_ecef_position)
    {
        m_send_home = true;
    }

    m_multirotor_state = value;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void RC_Comms::add_video_data(stream::IVideo::Value const& value)
{
    m_video_streamer.send(value.data.data(), value.data.size(), value.resolution);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void RC_Comms::process()
{
    if (!is_connected())
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lg(m_new_camera_commands_mutex);
        m_camera_commands = m_new_camera_commands;
        m_new_camera_commands = boost::none;
    }
    {
        std::lock_guard<std::mutex> lg(m_new_multirotor_commands_mutex);
        m_multirotor_commands = m_new_multirotor_commands;
        m_new_multirotor_commands = boost::none;
    }

    auto now = Clock::now();
    if (m_send_home == true && now - m_last_home_sent_tp > std::chrono::seconds(1))
    {
        QLOGI("Home changed, sending it to the RC");
        uint8_t packet_type = static_cast<uint8_t>(silk::rc_comms::Packet_Type::MULTIROTOR_STATE_HOME);
        size_t off = 0;

        std::lock_guard<std::mutex> lg(m_multirotor_state_mutex);
        util::serialization::serialize_home(m_multirotor_state_sz_buffer, m_multirotor_state, off);
        m_rc_protocol.send_reliable_packet(packet_type, m_multirotor_state_sz_buffer.data(), m_multirotor_state_sz_buffer.size());

        m_send_home = false;
        m_last_home_sent_tp = now;
    }
}

}
