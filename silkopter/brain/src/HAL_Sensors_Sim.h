#pragma once

#include "HAL_Sensors.h"
#include "Sim_Comms.h"

namespace silk
{

class Sim_Comms;

class HAL_Sensors_Sim : public HAL_Sensors
{
public:
    HAL_Sensors_Sim(Sim_Comms& sim_comms);

    //----------------------------------------------------------------------
    //calibration
    void set_accelerometer_calibration_data(math::vec3f const& bias, math::vec3f const& scale);
    void get_accelerometer_calibration_data(math::vec3f& bias, math::vec3f& scale) const;

    void set_gyroscope_calibration_data(math::vec3f const& bias);
    void get_gyroscope_calibration_data(math::vec3f& bias) const;

    void set_compass_calibration_data(math::vec3f const& bias);
    void get_compass_calibration_data(math::vec3f& bias) const;

    auto get_accelerometer_samples() const -> std::vector<Accelerometer_Sample> const&;
    auto get_gyroscope_samples() const -> std::vector<Gyroscope_Sample> const&;
    auto get_compass_samples() const -> std::vector<Compass_Sample> const&;
    auto get_barometer_samples() const -> std::vector<Barometer_Sample> const&;
    auto get_sonar_samples() const -> std::vector<Sonar_Sample> const&;
    auto get_thermometer_samples() const -> std::vector<Thermometer_Sample> const&;
    auto get_voltage_samples() const -> std::vector<Voltage_Sample> const&;
    auto get_current_samples() const -> std::vector<Current_Sample> const&;
    auto get_gps_samples() const -> std::vector<GPS_Sample> const&;

    size_t get_error_count() const;
    void process();

    struct Config
    {
        math::vec3f accelerometer_bias;
        math::vec3f accelerometer_scale = math::vec3f(1);
        math::vec3f gyroscope_bias;
        math::vec3f compass_bias;
    };

private:
    Sim_Comms& m_sim_comms;

    void handle_message(Sim_Comms::Message message, Sim_Comms::Channel& channel);

    void process_message_sensor_data(Sim_Comms::Channel& channel);
    void process_message_gps_data();

    template<class SAMPLE_T>
    auto unpack_sensor_sample(Sim_Comms::Channel& channel, SAMPLE_T& sample, std::vector<SAMPLE_T>& samples) -> bool;

    auto process_accelerometer_sensor(Sim_Comms::Channel& channel) -> bool;
    auto process_gyroscope_sensor(Sim_Comms::Channel& channel) -> bool;
    auto process_compass_sensor(Sim_Comms::Channel& channel) -> bool;
    auto process_barometer_sensor(Sim_Comms::Channel& channel) -> bool;
    auto process_thermometer_sensor(Sim_Comms::Channel& channel) -> bool;
    auto process_sonar_sensor(Sim_Comms::Channel& channel) -> bool;
    auto process_voltage_sensor(Sim_Comms::Channel& channel) -> bool;
    auto process_current_sensor(Sim_Comms::Channel& channel) -> bool;
    auto process_gps_sensor(Sim_Comms::Channel& channel) -> bool;

    Accelerometer_Sample m_accelerometer_sample;
    std::vector<Accelerometer_Sample> m_accelerometer_samples;

    Gyroscope_Sample m_gyroscope_sample;
    std::vector<Gyroscope_Sample> m_gyroscope_samples;

    Compass_Sample m_compass_sample;
    std::vector<Compass_Sample> m_compass_samples;

    Barometer_Sample m_barometer_sample;
    std::vector<Barometer_Sample> m_barometer_samples;

    Sonar_Sample m_sonar_sample;
    std::vector<Sonar_Sample> m_sonar_samples;

    Thermometer_Sample m_thermometer_sample;
    std::vector<Thermometer_Sample> m_thermometer_samples;

    Voltage_Sample m_voltage_sample;
    std::vector<Voltage_Sample> m_voltage_samples;

    Current_Sample m_current_sample;
    std::vector<Current_Sample> m_current_samples;

    GPS_Sample m_gps_sample;
    std::vector<GPS_Sample> m_gps_samples;

    Config m_config;

    size_t m_error_count = 0;

    auto load_settings() -> bool;
    void save_settings();
};

}
