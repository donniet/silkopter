#include "RF_Config_Menu_Page.h"
#include "Adafruit_GFX.h"
#include "Input.h"
#include "IBattery_Info.h"
#include "IRotary_Encoder.h"
#include "IButton.h"

#include "Menu_System.h"
#include "HAL.h"

#include "settings.def.h"

namespace silk
{

constexpr uint8_t MIN_CHANNEL = 0;
constexpr uint8_t MAX_CHANNEL = 128;
constexpr float MIN_XTAL_ADJUSTMENT = -1.f;
constexpr float MAX_XTAL_ADJUSTMENT = 1.f;

constexpr size_t CHANNEL_SUBMENU = 2;
constexpr size_t XTAL_ADJUSTMENT_SUBMENU = 3;

///////////////////////////////////////////////////////////////////////////////////////////////////

RF_Config_Menu_Page::RF_Config_Menu_Page(HAL& hal)
    : m_hal(hal)
{
    m_menu.push_submenu({
                         "<-",
                         "Save",
                         "Frequency",
                         "XTal Adj",
                        }, 0, 12);

    m_initial_channel = m_hal.get_settings().get_comms().get_rc_channel();
    m_initial_xtal_adjustment = m_hal.get_settings().get_comms().get_rc_xtal_adjustment();
    m_channel = m_initial_channel;
    m_xtal_adjustment = m_initial_xtal_adjustment;

    refresh_menu();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void RF_Config_Menu_Page::refresh_menu()
{
    IBattery_Info const& bi = m_hal.get_battery_info();

    size_t se = m_selected_entry == boost::none ? size_t(-1) : *m_selected_entry;

    char buffer[128];

    sprintf(buffer, "Chan: %s%d", se == CHANNEL_SUBMENU ? ">" : " ", (int)m_channel);
    m_menu.set_submenu_entry(CHANNEL_SUBMENU, buffer);

    sprintf(buffer, "XTal: %s%d", se == XTAL_ADJUSTMENT_SUBMENU ? ">" : " ", static_cast<int>(m_xtal_adjustment * 100.f));
    m_menu.set_submenu_entry(XTAL_ADJUSTMENT_SUBMENU, buffer);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool RF_Config_Menu_Page::process(Input& input, Menu_System& menu_system)
{
    Comms& comms = m_hal.get_comms();

    if (m_selected_entry == boost::none)
    {
        m_selected_entry = m_menu.process(input);
        if (m_selected_entry != boost::none)
        {
            if (*m_selected_entry == 0)
            {
                m_hal.get_settings().get_comms().set_rc_channel(m_initial_channel);
                m_hal.get_settings().get_comms().set_rc_xtal_adjustment(m_initial_xtal_adjustment);
                return false;
            }
            else if (*m_selected_entry == 1)
            {
                m_hal.get_settings().get_comms().set_rc_channel(m_channel);
                m_hal.get_settings().get_comms().set_rc_xtal_adjustment(m_xtal_adjustment);
                m_hal.save_settings();

                m_initial_channel = m_channel;
                m_initial_xtal_adjustment = m_xtal_adjustment;

                m_selected_entry = boost::none;
            }
        }
    }
    else
    {
        if (input.get_menu_switch().was_released())
        {
            m_selected_entry = boost::none;
            refresh_menu();
        }
        else
        {
            if (*m_selected_entry == CHANNEL_SUBMENU)
            {
                uint8_t ch = math::clamp<uint8_t>(m_channel + input.get_menu_encoder().get_delta(), MIN_CHANNEL, MAX_CHANNEL);
                if (ch != m_channel)
                {
                    m_channel = ch;
                    comms.get_rc_phy().set_channel(m_channel);
                    refresh_menu();
                }
            }
            else if (*m_selected_entry == XTAL_ADJUSTMENT_SUBMENU)
            {
                float f = math::clamp(m_xtal_adjustment + input.get_menu_encoder().get_delta() / 100.f, MIN_XTAL_ADJUSTMENT, MAX_XTAL_ADJUSTMENT);
                if (!math::equals(f, m_xtal_adjustment, math::epsilon<float>()))
                {
                    m_xtal_adjustment = f;
                    comms.get_rc_phy().set_xtal_adjustment(m_xtal_adjustment);
                    refresh_menu();
                }
            }
        }
    }

    Clock::time_point now = Clock::now();
    if (now - m_last_dbm_tp >= std::chrono::seconds(1))
    {
        m_last_dbm_tp = now;
        m_min_rx_dBm = comms.get_rx_dBm();
        m_min_tx_dBm = comms.get_tx_dBm();
    }
    m_min_rx_dBm = std::min<int>(m_min_rx_dBm, comms.get_rx_dBm());
    m_min_tx_dBm = std::min<int>(m_min_tx_dBm, comms.get_tx_dBm());

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void RF_Config_Menu_Page::render(Adafruit_GFX& display)
{
    display.setCursor(0, 0);
    display.setTextWrap(true);
    display.printf("v%ddBm ^%ddBm\n", m_min_rx_dBm, m_min_tx_dBm);

    m_menu.render(display, 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

}
