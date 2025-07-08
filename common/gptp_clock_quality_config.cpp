// Clock Quality Configuration Management Implementation
// OpenAvnu gPTP Clock Quality Testing Framework

#include "gptp_clock_quality_config.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstdlib>

namespace OpenAvnu {
namespace gPTP {

ClockQualityConfigManager::ClockQualityConfigManager() 
    : config_loaded(false) {
    load_default_config();
}

ClockQualityConfigManager::~ClockQualityConfigManager() = default;

void ClockQualityConfigManager::load_default_config() {
    // Initialize with default values
    config.ingress_monitoring_enabled = DefaultConfig::INGRESS_MONITORING_ENABLED;
    config.reverse_sync_enabled = DefaultConfig::REVERSE_SYNC_ENABLED;
    config.pps_monitoring_enabled = DefaultConfig::PPS_MONITORING_ENABLED;
    config.primary_measurement_method = DefaultConfig::PRIMARY_METHOD;
    
    config.measurement_interval_ms = DefaultConfig::MEASUREMENT_INTERVAL_MS;
    config.analysis_window_seconds = DefaultConfig::ANALYSIS_WINDOW_SECONDS;
    config.max_history_measurements = DefaultConfig::MAX_HISTORY_MEASUREMENTS;
    config.real_time_analysis_enabled = DefaultConfig::REAL_TIME_ANALYSIS_ENABLED;
    
    config.tlv_reporting_enabled = DefaultConfig::TLV_REPORTING_ENABLED;
    config.console_output_enabled = DefaultConfig::CONSOLE_OUTPUT_ENABLED;
    config.csv_export_enabled = DefaultConfig::CSV_EXPORT_ENABLED;
    
    config.pps_gpio_pin = DefaultConfig::PPS_GPIO_PIN;
    config.hardware_timestamping_enabled = DefaultConfig::HARDWARE_TIMESTAMPING_ENABLED;
    
    config.reverse_sync_domain = DefaultConfig::REVERSE_SYNC_DOMAIN;
    config.reverse_sync_bmca_enabled = DefaultConfig::REVERSE_SYNC_BMCA_ENABLED;
    
    set_profile_defaults();
    config_loaded = true;
}

void ClockQualityConfigManager::set_profile_defaults() {
    // Milan profile
    ProfileConfig milan;
    milan.profile_name = "Milan";
    milan.accuracy_requirement_ns = DefaultConfig::MILAN_ACCURACY_NS;
    milan.max_lock_time_seconds = DefaultConfig::MILAN_LOCK_TIME_SECONDS;
    milan.observation_window_seconds = DefaultConfig::MILAN_OBSERVATION_WINDOW_SECONDS;
    milan.measurement_interval_ms = DefaultConfig::MILAN_MEASUREMENT_INTERVAL_MS;
    milan.immediate_ascapable_required = false;
    milan.late_response_threshold_ms = DefaultConfig::MILAN_LATE_RESPONSE_THRESHOLD_MS;
    milan.min_pdelay_successes = 0;
    milan.max_pdelay_successes = 0;
    config.profile_configs["Milan"] = milan;
    
    // Automotive profile
    ProfileConfig automotive;
    automotive.profile_name = "Automotive";
    automotive.accuracy_requirement_ns = DefaultConfig::AUTOMOTIVE_ACCURACY_NS;
    automotive.max_lock_time_seconds = DefaultConfig::AUTOMOTIVE_LOCK_TIME_SECONDS;
    automotive.observation_window_seconds = DefaultConfig::MILAN_OBSERVATION_WINDOW_SECONDS;
    automotive.measurement_interval_ms = DefaultConfig::MILAN_MEASUREMENT_INTERVAL_MS;
    automotive.immediate_ascapable_required = DefaultConfig::AUTOMOTIVE_IMMEDIATE_ASCAPABLE;
    automotive.late_response_threshold_ms = 0;
    automotive.min_pdelay_successes = 0;
    automotive.max_pdelay_successes = 0;
    config.profile_configs["Automotive"] = automotive;
    
    // AVnu Base profile  
    ProfileConfig base;
    base.profile_name = "AVnu Base";
    base.accuracy_requirement_ns = DefaultConfig::AVNU_BASE_ACCURACY_NS;
    base.max_lock_time_seconds = DefaultConfig::MILAN_LOCK_TIME_SECONDS;
    base.observation_window_seconds = DefaultConfig::MILAN_OBSERVATION_WINDOW_SECONDS;
    base.measurement_interval_ms = DefaultConfig::MILAN_MEASUREMENT_INTERVAL_MS;
    base.immediate_ascapable_required = false;
    base.late_response_threshold_ms = 0;
    base.min_pdelay_successes = DefaultConfig::AVNU_BASE_MIN_PDELAY_SUCCESSES;
    base.max_pdelay_successes = DefaultConfig::AVNU_BASE_MAX_PDELAY_SUCCESSES;
    config.profile_configs["AVnu Base"] = base;
}

bool ClockQualityConfigManager::load_config_file(const std::string& file_path) {
    config_file_path = file_path;
    return parse_ini_file(file_path);
}

bool ClockQualityConfigManager::parse_ini_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return false;
    }
    
    config_values.clear();
    std::string line;
    std::string current_section;
    
    while (std::getline(file, line)) {
        std::string section, key, value;
        if (parse_ini_line(line, section, key, value)) {
            if (!section.empty()) {
                current_section = section;
            } else if (!key.empty()) {
                std::string full_key = current_section.empty() ? key : current_section + "." + key;
                config_values[full_key] = value;
            }
        }
    }
    
    apply_config_values();
    config_loaded = true;
    return true;
}

bool ClockQualityConfigManager::parse_ini_line(const std::string& line, std::string& section, std::string& key, std::string& value) {
    // Clear outputs
    section.clear();
    key.clear();
    value.clear();
    
    // Skip empty lines and comments
    std::string trimmed = line;
    trimmed.erase(0, trimmed.find_first_not_of(" \t"));
    trimmed.erase(trimmed.find_last_not_of(" \t") + 1);
    
    if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') {
        return false;
    }
    
    // Check for section header [section]
    if (trimmed[0] == '[' && trimmed.back() == ']') {
        section = trimmed.substr(1, trimmed.length() - 2);
        return true;
    }
    
    // Check for key=value pair
    size_t equals_pos = trimmed.find('=');
    if (equals_pos != std::string::npos) {
        key = trimmed.substr(0, equals_pos);
        value = trimmed.substr(equals_pos + 1);
        
        // Trim key and value
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        return true;
    }
    
    return false;
}

void ClockQualityConfigManager::apply_config_values() {
    // Apply measurement settings
    config.ingress_monitoring_enabled = get_bool_value("measurement.ingress_monitoring_enabled", DefaultConfig::INGRESS_MONITORING_ENABLED);
    config.reverse_sync_enabled = get_bool_value("measurement.reverse_sync_enabled", DefaultConfig::REVERSE_SYNC_ENABLED);
    config.pps_monitoring_enabled = get_bool_value("measurement.pps_monitoring_enabled", DefaultConfig::PPS_MONITORING_ENABLED);
    config.primary_measurement_method = get_method_value("measurement.primary_method", DefaultConfig::PRIMARY_METHOD);
    
    config.measurement_interval_ms = get_uint32_value("measurement.interval_ms", DefaultConfig::MEASUREMENT_INTERVAL_MS);
    config.analysis_window_seconds = get_uint32_value("analysis.window_seconds", DefaultConfig::ANALYSIS_WINDOW_SECONDS);
    config.max_history_measurements = get_uint32_value("analysis.max_history", DefaultConfig::MAX_HISTORY_MEASUREMENTS);
    config.real_time_analysis_enabled = get_bool_value("analysis.real_time_enabled", DefaultConfig::REAL_TIME_ANALYSIS_ENABLED);
    
    // Apply reporting settings
    config.tlv_reporting_enabled = get_bool_value("reporting.tlv_enabled", DefaultConfig::TLV_REPORTING_ENABLED);
    config.console_output_enabled = get_bool_value("reporting.console_enabled", DefaultConfig::CONSOLE_OUTPUT_ENABLED);
    config.csv_export_enabled = get_bool_value("reporting.csv_enabled", DefaultConfig::CSV_EXPORT_ENABLED);
    config.csv_export_path = get_string_value("reporting.csv_path", "");
    
    // Apply hardware settings
    config.pps_gpio_pin = get_int32_value("hardware.pps_gpio_pin", DefaultConfig::PPS_GPIO_PIN);
    config.hardware_timestamping_enabled = get_bool_value("hardware.timestamping_enabled", DefaultConfig::HARDWARE_TIMESTAMPING_ENABLED);
    
    // Apply network settings
    config.reverse_sync_domain = static_cast<uint8_t>(get_uint32_value("network.reverse_sync_domain", DefaultConfig::REVERSE_SYNC_DOMAIN));
    config.reverse_sync_bmca_enabled = get_bool_value("network.reverse_sync_bmca_enabled", DefaultConfig::REVERSE_SYNC_BMCA_ENABLED);
}

bool ClockQualityConfigManager::get_bool_value(const std::string& key, bool default_value) const {
    auto it = config_values.find(key);
    if (it == config_values.end()) {
        return default_value;
    }
    
    std::string value = it->second;
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);
    return value == "true" || value == "1" || value == "yes" || value == "on";
}

int32_t ClockQualityConfigManager::get_int32_value(const std::string& key, int32_t default_value) const {
    auto it = config_values.find(key);
    if (it == config_values.end()) {
        return default_value;
    }
    
    try {
        return std::stoi(it->second);
    } catch (...) {
        return default_value;
    }
}

uint32_t ClockQualityConfigManager::get_uint32_value(const std::string& key, uint32_t default_value) const {
    auto it = config_values.find(key);
    if (it == config_values.end()) {
        return default_value;
    }
    
    try {
        return static_cast<uint32_t>(std::stoul(it->second));
    } catch (...) {
        return default_value;
    }
}

std::string ClockQualityConfigManager::get_string_value(const std::string& key, const std::string& default_value) const {
    auto it = config_values.find(key);
    if (it == config_values.end()) {
        return default_value;
    }
    return it->second;
}

MeasurementMethod ClockQualityConfigManager::get_method_value(const std::string& key, MeasurementMethod default_value) const {
    auto it = config_values.find(key);
    if (it == config_values.end()) {
        return default_value;
    }
    
    return string_to_method(it->second);
}

ProfileConfig ClockQualityConfigManager::get_profile_config(const std::string& profile_name) const {
    auto it = config.profile_configs.find(profile_name);
    if (it != config.profile_configs.end()) {
        return it->second;
    }
    
    // Return default profile if not found
    ProfileConfig default_profile;
    default_profile.profile_name = profile_name;
    default_profile.accuracy_requirement_ns = DefaultConfig::MILAN_ACCURACY_NS;
    default_profile.max_lock_time_seconds = DefaultConfig::MILAN_LOCK_TIME_SECONDS;
    default_profile.observation_window_seconds = DefaultConfig::MILAN_OBSERVATION_WINDOW_SECONDS;
    default_profile.measurement_interval_ms = DefaultConfig::MILAN_MEASUREMENT_INTERVAL_MS;
    default_profile.immediate_ascapable_required = false;
    default_profile.late_response_threshold_ms = 0;
    default_profile.min_pdelay_successes = 0;
    default_profile.max_pdelay_successes = 0;
    return default_profile;
}

void ClockQualityConfigManager::set_profile_config(const std::string& profile_name, const ProfileConfig& profile_config) {
    config.profile_configs[profile_name] = profile_config;
}

void ClockQualityConfigManager::update_measurement_method(MeasurementMethod method) {
    config.primary_measurement_method = method;
    
    // Enable/disable specific monitoring based on method
    switch (method) {
        case MeasurementMethod::INGRESS_REPORTING:
            config.ingress_monitoring_enabled = true;
            config.reverse_sync_enabled = false;
            config.pps_monitoring_enabled = false;
            break;
        case MeasurementMethod::REVERSE_SYNC:
            config.ingress_monitoring_enabled = false;
            config.reverse_sync_enabled = true;
            config.pps_monitoring_enabled = false;
            break;
        case MeasurementMethod::PPS_HARDWARE:
            config.ingress_monitoring_enabled = false;
            config.reverse_sync_enabled = false;
            config.pps_monitoring_enabled = true;
            break;
        case MeasurementMethod::COMBINED:
            config.ingress_monitoring_enabled = true;
            config.reverse_sync_enabled = true;
            config.pps_monitoring_enabled = true;
            break;
    }
}

void ClockQualityConfigManager::update_measurement_interval(uint32_t interval_ms) {
    config.measurement_interval_ms = interval_ms;
}

void ClockQualityConfigManager::update_analysis_window(uint32_t window_seconds) {
    config.analysis_window_seconds = window_seconds;
}

void ClockQualityConfigManager::enable_tlv_reporting(bool enabled) {
    config.tlv_reporting_enabled = enabled;
}

void ClockQualityConfigManager::enable_console_output(bool enabled) {
    config.console_output_enabled = enabled;
}

void ClockQualityConfigManager::enable_csv_export(bool enabled, const std::string& export_path) {
    config.csv_export_enabled = enabled;
    if (!export_path.empty()) {
        config.csv_export_path = export_path;
    }
}

bool ClockQualityConfigManager::validate_config() const {
    return get_validation_errors().empty();
}

std::vector<std::string> ClockQualityConfigManager::get_validation_errors() const {
    std::vector<std::string> errors;
    
    // Validate measurement interval
    if (config.measurement_interval_ms == 0) {
        errors.push_back("Measurement interval cannot be zero");
    }
    
    // Validate analysis window
    if (config.analysis_window_seconds == 0) {
        errors.push_back("Analysis window cannot be zero");
    }
    
    // Validate that at least one measurement method is enabled
    if (!config.ingress_monitoring_enabled && !config.reverse_sync_enabled && !config.pps_monitoring_enabled) {
        errors.push_back("At least one measurement method must be enabled");
    }
    
    return errors;
}

bool ClockQualityConfigManager::save_config_file(const std::string& file_path) const {
    std::ofstream file(file_path);
    if (!file.is_open()) {
        return false;
    }
    
    // Write header
    file << "# OpenAvnu gPTP Clock Quality Configuration" << std::endl;
    file << "# Generated automatically" << std::endl;
    file << std::endl;
    
    // Write measurement settings
    file << "[measurement]" << std::endl;
    file << "ingress_monitoring_enabled=" << (config.ingress_monitoring_enabled ? "true" : "false") << std::endl;
    file << "reverse_sync_enabled=" << (config.reverse_sync_enabled ? "true" : "false") << std::endl;
    file << "pps_monitoring_enabled=" << (config.pps_monitoring_enabled ? "true" : "false") << std::endl;
    file << "primary_method=" << method_to_string(config.primary_measurement_method) << std::endl;
    file << "interval_ms=" << config.measurement_interval_ms << std::endl;
    file << std::endl;
    
    // Write analysis settings
    file << "[analysis]" << std::endl;
    file << "window_seconds=" << config.analysis_window_seconds << std::endl;
    file << "max_history=" << config.max_history_measurements << std::endl;
    file << "real_time_enabled=" << (config.real_time_analysis_enabled ? "true" : "false") << std::endl;
    file << std::endl;
    
    // Write reporting settings
    file << "[reporting]" << std::endl;
    file << "tlv_enabled=" << (config.tlv_reporting_enabled ? "true" : "false") << std::endl;
    file << "console_enabled=" << (config.console_output_enabled ? "true" : "false") << std::endl;
    file << "csv_enabled=" << (config.csv_export_enabled ? "true" : "false") << std::endl;
    if (!config.csv_export_path.empty()) {
        file << "csv_path=" << config.csv_export_path << std::endl;
    }
    file << std::endl;
    
    // Write hardware settings
    file << "[hardware]" << std::endl;
    file << "pps_gpio_pin=" << config.pps_gpio_pin << std::endl;
    file << "timestamping_enabled=" << (config.hardware_timestamping_enabled ? "true" : "false") << std::endl;
    file << std::endl;
    
    // Write network settings
    file << "[network]" << std::endl;
    file << "reverse_sync_domain=" << static_cast<int>(config.reverse_sync_domain) << std::endl;
    file << "reverse_sync_bmca_enabled=" << (config.reverse_sync_bmca_enabled ? "true" : "false") << std::endl;
    
    return true;
}

bool ClockQualityConfigManager::save_config_file() const {
    if (config_file_path.empty()) {
        return false;
    }
    return save_config_file(config_file_path);
}

void ClockQualityConfigManager::print_config_summary() const {
    std::cout << get_config_summary();
}

std::string ClockQualityConfigManager::get_config_summary() const {
    std::stringstream ss;
    
    ss << "Clock Quality Configuration Summary:" << std::endl;
    ss << "  Primary Method: " << method_to_string(config.primary_measurement_method) << std::endl;
    ss << "  Measurement Interval: " << config.measurement_interval_ms << "ms" << std::endl;
    ss << "  Analysis Window: " << config.analysis_window_seconds << "s" << std::endl;
    
    ss << "  Enabled Methods:" << std::endl;
    if (config.ingress_monitoring_enabled) ss << "    - Ingress Monitoring" << std::endl;
    if (config.reverse_sync_enabled) ss << "    - Reverse Sync" << std::endl;
    if (config.pps_monitoring_enabled) ss << "    - PPS Hardware" << std::endl;
    
    ss << "  Reporting:" << std::endl;
    ss << "    TLV: " << (config.tlv_reporting_enabled ? "enabled" : "disabled") << std::endl;
    ss << "    Console: " << (config.console_output_enabled ? "enabled" : "disabled") << std::endl;
    ss << "    CSV: " << (config.csv_export_enabled ? "enabled" : "disabled") << std::endl;
    
    ss << "  Profiles:" << std::endl;
    for (const auto& profile : config.profile_configs) {
        ss << "    " << profile.first << ": +/-" << profile.second.accuracy_requirement_ns << "ns" << std::endl;
    }
    
    return ss.str();
}

std::string ClockQualityConfigManager::method_to_string(MeasurementMethod method) {
    switch (method) {
        case MeasurementMethod::INGRESS_REPORTING: return "ingress_reporting";
        case MeasurementMethod::REVERSE_SYNC: return "reverse_sync";
        case MeasurementMethod::PPS_HARDWARE: return "pps_hardware";
        case MeasurementMethod::COMBINED: return "combined";
        default: return "unknown";
    }
}

MeasurementMethod ClockQualityConfigManager::string_to_method(const std::string& method_str) {
    if (method_str == "ingress_reporting") return MeasurementMethod::INGRESS_REPORTING;
    if (method_str == "reverse_sync") return MeasurementMethod::REVERSE_SYNC;
    if (method_str == "pps_hardware") return MeasurementMethod::PPS_HARDWARE;
    if (method_str == "combined") return MeasurementMethod::COMBINED;
    return MeasurementMethod::INGRESS_REPORTING; // default
}

std::string ClockQualityConfigManager::get_default_config_path() {
    // Check environment variable first
    const char* env_path = std::getenv("GPTP_CLOCK_QUALITY_CONFIG");
    if (env_path) {
        return std::string(env_path);
    }
    
    return "gptp_clock_quality.ini";
}

ClockQualityConfigManager& ClockQualityConfigManager::get_instance() {
    static ClockQualityConfigManager instance;
    return instance;
}

bool ClockQualityConfigManager::load_config_from_environment() {
    const char* env_path = std::getenv("GPTP_CLOCK_QUALITY_CONFIG");
    if (env_path) {
        return load_config_file(std::string(env_path));
    }
    return false;
}

} // namespace gPTP
} // namespace OpenAvnu
