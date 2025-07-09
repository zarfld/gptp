/**
 * @file gptp_clock_quality.hpp
 * @brief gPTP Clock Quality Testing and Measurement Framework
 * 
 * Implements comprehensive clock quality testing capabilities for OpenAvnu
 * based on Avnu Alliance specifications:
 * - 802.1AS Recovered Clock Quality Testing v1.0 (2016-10-18)
 * - gPTP Test Plan v1.0 Certification Program
 * 
 * @author OpenAvnu Development Team
 * @copyright (c) 2025 OpenAvnu Alliance
 */

#ifndef GPTP_CLOCK_QUALITY_HPP
#define GPTP_CLOCK_QUALITY_HPP

#include <memory>
#include <vector>
#include <deque>
#include <chrono>
#include <cstdint>
#include <string>
#include <map>

namespace OpenAvnu {
namespace gPTP {

/**
 * @brief Clock quality measurement method
 */
enum class ClockQualityMethod {
    PPS_HARDWARE,      ///< 1PPS hardware-based measurement
    INGRESS_REPORTING, ///< Software-based ingress monitoring  
    REVERSE_SYNC,      ///< Network-based reverse sync method
    COMBINED           ///< Multiple methods combined
};

/**
 * @brief Clock Quality measurement methods (for configuration management)
 */
enum class MeasurementMethod {
    INGRESS_REPORTING = 0,
    REVERSE_SYNC = 1,
    PPS_HARDWARE = 2,
    COMBINED = 3
};

/**
 * @brief Profile types for certification compliance
 */
enum class ProfileType {
    STANDARD,    ///< IEEE 802.1AS standard profile
    MILAN,       ///< Milan baseline interoperability
    AUTOMOTIVE,  ///< Automotive Ethernet AVB
    AVNU_BASE    ///< AVnu Base/ProAV profile
};

/**
 * @brief Configuration for different profile types
 */
struct ProfileConfig {
    std::string profile_name;
    int32_t accuracy_requirement_ns;        ///< ±accuracy requirement in nanoseconds
    uint32_t max_lock_time_seconds;         ///< Maximum lock time in seconds
    uint32_t observation_window_seconds;    ///< Analysis window in seconds
    uint32_t measurement_interval_ms;       ///< Measurement interval in milliseconds
    bool immediate_ascapable_required;      ///< Automotive requirement
    uint32_t late_response_threshold_ms;    ///< Milan requirement
    uint32_t min_pdelay_successes;          ///< AVnu Base requirement
    uint32_t max_pdelay_successes;          ///< AVnu Base requirement
};

/**
 * @brief Single clock quality measurement
 */
struct ClockQualityMeasurement {
    uint64_t timestamp_ns;           ///< Measurement timestamp (monotonic)
    uint64_t t1_master_tx_ns;        ///< Master transmit time
    uint64_t t2_slave_rx_ns;         ///< Slave receive time
    uint64_t path_delay_ns;          ///< Measured path delay
    int64_t offset_from_master_ns;   ///< Time error (T2 - T1 - path_delay)
    uint64_t correction_field_ns;    ///< Correction field from Follow_Up
    bool valid;                      ///< Measurement validity flag
    
    ClockQualityMeasurement() 
        : timestamp_ns(0), t1_master_tx_ns(0), t2_slave_rx_ns(0)
        , path_delay_ns(0), offset_from_master_ns(0)
        , correction_field_ns(0), valid(false) {}
};

/**
 * @brief Comprehensive clock quality metrics
 */
struct ClockQualityMetrics {
    // Timing Accuracy
    int64_t mean_time_error_ns;      ///< Mean time error in nanoseconds
    int64_t max_time_error_ns;       ///< Maximum time error (absolute)
    int64_t min_time_error_ns;       ///< Minimum time error  
    double std_dev_ns;               ///< Standard deviation in nanoseconds
    double rms_error_ns;             ///< RMS error in nanoseconds
    
    // Lock Performance
    uint32_t lock_time_seconds;      ///< Time to achieve lock
    bool is_locked;                  ///< Current lock status
    uint32_t observation_window_s;   ///< Analysis window in seconds
    uint64_t window_start_time;      ///< Window start timestamp
    
    // Stability Metrics
    double frequency_stability_ppb;   ///< Frequency stability (parts per billion)
    uint32_t consecutive_good_measurements; ///< Consecutive measurements within spec
    uint32_t total_measurements;     ///< Total measurements in window
    uint32_t outlier_count;          ///< Measurements outside tolerance
    
    // Certification Compliance
    bool meets_80ns_requirement;     ///< ±80ns accuracy requirement
    bool meets_lock_time_requirement; ///< ≤6s lock time requirement  
    bool meets_stability_requirement; ///< 5-minute stability requirement
    ProfileType active_profile;      ///< Current profile being validated
    
    // Measurement Context
    ClockQualityMethod measurement_method; ///< Method used for measurement
    uint64_t measurement_start_time; ///< First measurement timestamp
    uint64_t last_measurement_time;  ///< Most recent measurement timestamp
    uint32_t measurement_interval_ms; ///< Measurement interval
    
    ClockQualityMetrics() 
        : mean_time_error_ns(0), max_time_error_ns(0), min_time_error_ns(0)
        , std_dev_ns(0.0), rms_error_ns(0.0)
        , lock_time_seconds(0), is_locked(false), observation_window_s(0)
        , window_start_time(0)
        , frequency_stability_ppb(0.0), consecutive_good_measurements(0)
        , total_measurements(0), outlier_count(0)
        , meets_80ns_requirement(false), meets_lock_time_requirement(false)
        , meets_stability_requirement(false), active_profile(ProfileType::STANDARD)
        , measurement_method(ClockQualityMethod::INGRESS_REPORTING)
        , measurement_start_time(0), last_measurement_time(0)
        , measurement_interval_ms(125) {}
};

/**
 * @brief Configuration for clock quality monitoring
 */
struct ClockQualityConfig {
    // Monitoring Control
    bool ingress_monitoring_enabled;     ///< Enable ingress event monitoring
    bool reverse_sync_enabled;           ///< Enable reverse sync method
    bool pps_monitoring_enabled;         ///< Enable 1PPS hardware monitoring
    MeasurementMethod primary_measurement_method; ///< Primary measurement method
    
    // Measurement Parameters
    uint32_t measurement_interval_ms;    ///< Measurement interval (default: 125ms)
    uint32_t analysis_window_seconds;    ///< Analysis window (default: 300s)
    uint32_t max_history_measurements;   ///< Maximum stored measurements
    bool real_time_analysis_enabled;     ///< Enable real-time analysis
    
    // Certification Requirements
    int64_t target_accuracy_ns;          ///< Target accuracy (default: ±80ns)
    uint32_t max_lock_time_s;            ///< Maximum lock time (default: 6s)
    uint32_t stability_window_s;         ///< Stability window (default: 300s)
    
    // Reporting Settings
    bool tlv_reporting_enabled;          ///< Enable TLV reporting
    bool console_output_enabled;         ///< Enable console output
    bool csv_export_enabled;             ///< Enable CSV export
    std::string csv_export_path;         ///< CSV export path
    
    // Hardware Settings
    int32_t pps_gpio_pin;                ///< PPS GPIO pin (-1 = disabled)
    bool hardware_timestamping_enabled;  ///< Enable hardware timestamping
    
    // Network Settings
    uint8_t reverse_sync_domain;         ///< Reverse sync domain
    bool reverse_sync_bmca_enabled;      ///< Enable reverse sync BMCA
    
    // Profile-Specific Settings
    ProfileType profile_type;            ///< Active profile for validation
    std::map<std::string, ProfileConfig> profile_configs; ///< Profile configurations
    
    ClockQualityConfig()
        : ingress_monitoring_enabled(true), reverse_sync_enabled(false)
        , pps_monitoring_enabled(false)
        , primary_measurement_method(MeasurementMethod::INGRESS_REPORTING)
        , measurement_interval_ms(125), analysis_window_seconds(300)
        , max_history_measurements(10000), real_time_analysis_enabled(true)
        , target_accuracy_ns(80), max_lock_time_s(6), stability_window_s(300)
        , tlv_reporting_enabled(false), console_output_enabled(true)
        , csv_export_enabled(false), csv_export_path("")
        , pps_gpio_pin(-1), hardware_timestamping_enabled(false)
        , reverse_sync_domain(1), reverse_sync_bmca_enabled(false)
        , profile_type(ProfileType::STANDARD) {}
};

/**
 * @brief Ingress Event Monitor for software-based clock quality measurement
 * 
 * Implements the Ingress Reporting Method from Section 5.2 of the 
 * 802.1AS Recovered Clock Quality Testing specification.
 */
class IngressEventMonitor {
private:
    std::deque<ClockQualityMeasurement> measurements_;
    ClockQualityConfig config_;
    bool monitoring_enabled_;
    uint64_t monitoring_start_time_;
    uint64_t last_sync_sequence_id_;
    
    /**
     * @brief Get current monotonic timestamp in nanoseconds
     */
    uint64_t get_monotonic_time_ns() const;
    
    /**
     * @brief Validate measurement against outlier detection
     */
    bool is_measurement_valid(const ClockQualityMeasurement& measurement) const;
    
    /**
     * @brief Trim measurement history to configured maximum
     */
    void trim_measurement_history();
    
public:
    /**
     * @brief Constructor
     */
    explicit IngressEventMonitor(const ClockQualityConfig& config = ClockQualityConfig());
    
    /**
     * @brief Destructor
     */
    ~IngressEventMonitor() = default;
    
    /**
     * @brief Enable monitoring with specified interval
     * @param interval_ms Measurement interval in milliseconds
     */
    void enable_monitoring(uint32_t interval_ms = 125);
    
    /**
     * @brief Disable monitoring
     */
    void disable_monitoring();
    
    /**
     * @brief Check if monitoring is enabled
     */
    bool is_monitoring_enabled() const { return monitoring_enabled_; }
    
    /**
     * @brief Record ingress event from Sync/Follow_Up message pair
     * @param t1_master_tx Master transmit time from Follow_Up originTimestamp
     * @param t2_slave_rx Slave receive timestamp from hardware
     * @param path_delay Measured path delay from PDelay mechanism
     * @param correction_field Correction field from Follow_Up message
     * @param sequence_id Sequence ID for duplicate detection
     */
    void record_sync_ingress(uint64_t t1_master_tx, uint64_t t2_slave_rx, 
                           uint64_t path_delay, uint64_t correction_field,
                           uint16_t sequence_id);
    
    /**
     * @brief Get current measurement count
     */
    size_t get_measurement_count() const { return measurements_.size(); }
    
    /**
     * @brief Get measurement history (read-only)
     */
    const std::deque<ClockQualityMeasurement>& get_measurement_history() const {
        return measurements_;
    }
    
    /**
     * @brief Clear all measurement history
     */
    void clear_measurements();
    
    /**
     * @brief Update configuration
     */
    void update_config(const ClockQualityConfig& config);
    
    /**
     * @brief Get current configuration
     */
    const ClockQualityConfig& get_config() const { return config_; }
    
    /**
     * @brief Export measurement data as TLV for remote monitoring
     * @return TLV-encoded measurement data
     */
    std::vector<uint8_t> export_tlv_data() const;
    
    /**
     * @brief Import measurement data from TLV
     * @param tlv_data TLV-encoded measurement data
     * @return true if successfully imported
     */
    bool import_tlv_data(const std::vector<uint8_t>& tlv_data);
};

/**
 * @brief Clock Quality Analyzer for statistical analysis and certification validation
 */
class ClockQualityAnalyzer {
private:
    ClockQualityConfig config_;
    
    /**
     * @brief Calculate statistical metrics from measurements
     */
    void calculate_statistics(const std::deque<ClockQualityMeasurement>& measurements,
                            ClockQualityMetrics& metrics) const;
    
    /**
     * @brief Validate against profile-specific requirements
     */
    bool validate_profile_requirements(const ClockQualityMetrics& metrics,
                                     ProfileType profile) const;
    
    /**
     * @brief Detect lock state based on measurement stability
     */
    bool detect_lock_state(const std::deque<ClockQualityMeasurement>& measurements) const;
    
    /**
     * @brief Calculate lock time from measurement history
     */
    uint32_t calculate_lock_time(const std::deque<ClockQualityMeasurement>& measurements) const;
    
public:
    /**
     * @brief Constructor
     */
    explicit ClockQualityAnalyzer(const ClockQualityConfig& config = ClockQualityConfig());
    
    /**
     * @brief Analyze measurement window and generate metrics
     * @param measurements Measurement history to analyze
     * @param window_seconds Analysis window in seconds (0 = all measurements)
     * @return Comprehensive clock quality metrics
     */
    ClockQualityMetrics analyze_measurements(
        const std::deque<ClockQualityMeasurement>& measurements,
        uint32_t window_seconds = 0) const;
    
    /**
     * @brief Validate certification requirements for specific profile
     * @param metrics Clock quality metrics to validate
     * @param profile Profile type for validation
     * @return true if all requirements are met
     */
    bool validate_certification_requirements(const ClockQualityMetrics& metrics,
                                           ProfileType profile) const;
    
    /**
     * @brief Validate Milan profile requirements
     * @param metrics Clock quality metrics
     * @return true if Milan requirements are met
     */
    bool validate_milan_requirements(const ClockQualityMetrics& metrics) const;
    
    /**
     * @brief Validate Automotive profile requirements  
     * @param metrics Clock quality metrics
     * @return true if Automotive requirements are met
     */
    bool validate_automotive_requirements(const ClockQualityMetrics& metrics) const;
    
    /**
     * @brief Validate AVnu Base profile requirements
     * @param metrics Clock quality metrics
     * @return true if AVnu Base requirements are met
     */
    bool validate_avnu_base_requirements(const ClockQualityMetrics& metrics) const;
    
    /**
     * @brief Generate comprehensive compliance report
     * @param metrics Clock quality metrics
     * @return Human-readable compliance report
     */
    std::string generate_compliance_report(const ClockQualityMetrics& metrics) const;
    
    /**
     * @brief Generate certification test report
     * @param metrics Clock quality metrics
     * @return Certification-ready test report
     */
    std::string generate_certification_report(const ClockQualityMetrics& metrics) const;
    
    /**
     * @brief Update configuration
     */
    void update_config(const ClockQualityConfig& config) { config_ = config; }
    
    /**
     * @brief Get current configuration
     */
    const ClockQualityConfig& get_config() const { return config_; }
};

} // namespace gPTP
} // namespace OpenAvnu

#endif // GPTP_CLOCK_QUALITY_HPP
