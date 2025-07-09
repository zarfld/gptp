// Clock Quality Configuration Management
// OpenAvnu gPTP Clock Quality Testing Framework
// Based on Avnu Alliance 802.1AS Recovered Clock Quality Testing v1.0

#ifndef GPTP_CLOCK_QUALITY_CONFIG_HPP
#define GPTP_CLOCK_QUALITY_CONFIG_HPP

#include <string>
#include <map>
#include <memory>
#include <cstdint>
#include <vector>
#include "gptp_clock_quality.hpp"  // Include full definition

namespace OpenAvnu {
namespace gPTP {

// Note: ClockQualityConfig, MeasurementMethod, and ProfileConfig are now defined in gptp_clock_quality.hpp

class ClockQualityConfigManager {
private:
    ClockQualityConfig config;
    std::map<std::string, std::string> config_values;
    std::string config_file_path;
    bool config_loaded;
    
    // INI file parsing helpers
    bool parse_ini_file(const std::string& file_path);
    bool parse_ini_line(const std::string& line, std::string& section, std::string& key, std::string& value);
    void apply_config_values();
    void set_profile_defaults();
    
    // Type conversion helpers
    bool get_bool_value(const std::string& key, bool default_value = false) const;
    int32_t get_int32_value(const std::string& key, int32_t default_value = 0) const;
    uint32_t get_uint32_value(const std::string& key, uint32_t default_value = 0) const;
    std::string get_string_value(const std::string& key, const std::string& default_value = "") const;
    MeasurementMethod get_method_value(const std::string& key, MeasurementMethod default_value = MeasurementMethod::INGRESS_REPORTING) const;

public:
    ClockQualityConfigManager();
    ~ClockQualityConfigManager();
    
    // Configuration loading
    bool load_config_file(const std::string& file_path);
    bool load_config_from_environment();
    void load_default_config();
    
    // Configuration access
    const ClockQualityConfig& get_config() const { return config; }
    ClockQualityConfig& get_mutable_config() { return config; }
    
    // Profile-specific configuration
    ProfileConfig get_profile_config(const std::string& profile_name) const;
    void set_profile_config(const std::string& profile_name, const ProfileConfig& profile_config);
    
    // Configuration updates
    void update_measurement_method(MeasurementMethod method);
    void update_measurement_interval(uint32_t interval_ms);
    void update_analysis_window(uint32_t window_seconds);
    void enable_tlv_reporting(bool enabled);
    void enable_console_output(bool enabled);
    void enable_csv_export(bool enabled, const std::string& export_path = "");
    
    // Configuration validation
    bool validate_config() const;
    std::vector<std::string> get_validation_errors() const;
    
    // Configuration persistence
    bool save_config_file(const std::string& file_path) const;
    bool save_config_file() const; // Save to original path
    
    // Configuration information
    void print_config_summary() const;
    std::string get_config_summary() const;
    bool is_config_loaded() const { return config_loaded; }
    
    // Static helper methods
    static std::string method_to_string(MeasurementMethod method);
    static MeasurementMethod string_to_method(const std::string& method_str);
    static std::string get_default_config_path();
    static ClockQualityConfigManager& get_instance(); // Singleton pattern
};

// Default configuration values
namespace DefaultConfig {
    // Measurement settings
    constexpr bool INGRESS_MONITORING_ENABLED = true;
    constexpr bool REVERSE_SYNC_ENABLED = false;
    constexpr bool PPS_MONITORING_ENABLED = false;
    constexpr MeasurementMethod PRIMARY_METHOD = MeasurementMethod::INGRESS_REPORTING;
    
    // Analysis parameters
    constexpr uint32_t MEASUREMENT_INTERVAL_MS = 125;
    constexpr uint32_t ANALYSIS_WINDOW_SECONDS = 300;  // 5 minutes
    constexpr uint32_t MAX_HISTORY_MEASUREMENTS = 10000;
    constexpr bool REAL_TIME_ANALYSIS_ENABLED = true;
    
    // Milan profile defaults
    constexpr int32_t MILAN_ACCURACY_NS = 80;
    constexpr uint32_t MILAN_LOCK_TIME_SECONDS = 6;
    constexpr uint32_t MILAN_OBSERVATION_WINDOW_SECONDS = 300;
    constexpr uint32_t MILAN_MEASUREMENT_INTERVAL_MS = 125;
    constexpr uint32_t MILAN_LATE_RESPONSE_THRESHOLD_MS = 15;
    
    // Automotive profile defaults
    constexpr int32_t AUTOMOTIVE_ACCURACY_NS = 50;
    constexpr uint32_t AUTOMOTIVE_LOCK_TIME_SECONDS = 1; // Immediate
    constexpr bool AUTOMOTIVE_IMMEDIATE_ASCAPABLE = true;
    
    // AVnu Base profile defaults
    constexpr int32_t AVNU_BASE_ACCURACY_NS = 80;
    constexpr uint32_t AVNU_BASE_MIN_PDELAY_SUCCESSES = 2;
    constexpr uint32_t AVNU_BASE_MAX_PDELAY_SUCCESSES = 10;
    
    // Reporting defaults
    constexpr bool TLV_REPORTING_ENABLED = false;
    constexpr bool CONSOLE_OUTPUT_ENABLED = true;
    constexpr bool CSV_EXPORT_ENABLED = false;
    
    // Hardware defaults
    constexpr int32_t PPS_GPIO_PIN = -1; // Disabled
    constexpr bool HARDWARE_TIMESTAMPING_ENABLED = false;
    
    // Network defaults
    constexpr uint8_t REVERSE_SYNC_DOMAIN = 1;
    constexpr bool REVERSE_SYNC_BMCA_ENABLED = false;
}

} // namespace gPTP
} // namespace OpenAvnu

#endif // GPTP_CLOCK_QUALITY_CONFIG_HPP
