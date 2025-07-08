/**
 * @file gptp_clock_quality.cpp  
 * @brief Implementation of gPTP Clock Quality Testing and Measurement Framework
 */

#include "gptp_clock_quality.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <cassert>

#if defined(_WIN32)
#define NOMINMAX  // Prevent Windows min/max macros
#include <windows.h>
#else
#include <time.h>
#endif

namespace OpenAvnu {
namespace gPTP {

// ============================================================================
// Utility Functions
// ============================================================================

static uint64_t get_monotonic_time_ns() {
#if defined(_WIN32)
    static LARGE_INTEGER frequency = {0};
    if (frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
    }
    
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (counter.QuadPart * 1000000000ULL) / frequency.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
#endif
}

static double calculate_standard_deviation(const std::vector<int64_t>& values, double mean) {
    if (values.size() < 2) return 0.0;
    
    double sum_squared_diff = 0.0;
    for (int64_t value : values) {
        double diff = static_cast<double>(value) - mean;
        sum_squared_diff += diff * diff;
    }
    
    return std::sqrt(sum_squared_diff / (values.size() - 1));
}

static double calculate_rms_error(const std::vector<int64_t>& values) {
    if (values.empty()) return 0.0;
    
    double sum_squared = 0.0;
    for (int64_t value : values) {
        double val = static_cast<double>(value);
        sum_squared += val * val;
    }
    
    return std::sqrt(sum_squared / values.size());
}

// ============================================================================
// IngressEventMonitor Implementation
// ============================================================================

IngressEventMonitor::IngressEventMonitor(const ClockQualityConfig& config)
    : config_(config)
    , monitoring_enabled_(false)
    , monitoring_start_time_(0)
    , last_sync_sequence_id_(0)
{
    measurements_.clear();
}

uint64_t IngressEventMonitor::get_monotonic_time_ns() const {
    return ::OpenAvnu::gPTP::get_monotonic_time_ns();
}

void IngressEventMonitor::enable_monitoring(uint32_t interval_ms) {
    config_.measurement_interval_ms = interval_ms;
    monitoring_enabled_ = true;
    monitoring_start_time_ = get_monotonic_time_ns();
    measurements_.clear();
    
    // Log monitoring start
    // TODO: Add proper logging framework integration
}

void IngressEventMonitor::disable_monitoring() {
    monitoring_enabled_ = false;
    
    // Log monitoring stop with summary
    // TODO: Add proper logging framework integration
}

bool IngressEventMonitor::is_measurement_valid(const ClockQualityMeasurement& measurement) const {
    // Basic sanity checks
    if (measurement.t1_master_tx_ns == 0 || measurement.t2_slave_rx_ns == 0) {
        return false;
    }
    
    // Check for reasonable time error bounds (±10ms as outlier detection)
    const int64_t MAX_REASONABLE_ERROR_NS = 10000000; // 10ms
    if (std::abs(measurement.offset_from_master_ns) > MAX_REASONABLE_ERROR_NS) {
        return false;
    }
    
    // Check for reasonable path delay (should be < 1ms for direct connection)
    const uint64_t MAX_REASONABLE_PATH_DELAY_NS = 1000000; // 1ms
    if (measurement.path_delay_ns > MAX_REASONABLE_PATH_DELAY_NS) {
        return false;
    }
    
    return true;
}

void IngressEventMonitor::trim_measurement_history() {
    while (measurements_.size() > config_.max_history_measurements) {
        measurements_.pop_front();
    }
}

void IngressEventMonitor::record_sync_ingress(uint64_t t1_master_tx, uint64_t t2_slave_rx,
                                            uint64_t path_delay, uint64_t correction_field,
                                            uint16_t sequence_id) {
    if (!monitoring_enabled_) {
        return;
    }
    
    // Detect and skip duplicate sequence IDs
    if (sequence_id == last_sync_sequence_id_) {
        return;
    }
    last_sync_sequence_id_ = sequence_id;
    
    ClockQualityMeasurement measurement;
    measurement.timestamp_ns = get_monotonic_time_ns();
    measurement.t1_master_tx_ns = t1_master_tx;
    measurement.t2_slave_rx_ns = t2_slave_rx;
    measurement.path_delay_ns = path_delay;
    measurement.correction_field_ns = correction_field;
    
    // Calculate offset from master (time error)
    // Per IEEE 802.1AS: offsetFromMaster = T2 - T1 - pathDelay - correctionField
    measurement.offset_from_master_ns = static_cast<int64_t>(t2_slave_rx) 
                                      - static_cast<int64_t>(t1_master_tx)
                                      - static_cast<int64_t>(path_delay)
                                      - static_cast<int64_t>(correction_field);
    
    // Validate measurement
    measurement.valid = is_measurement_valid(measurement);
    
    // Store measurement
    measurements_.push_back(measurement);
    trim_measurement_history();
}

void IngressEventMonitor::clear_measurements() {
    measurements_.clear();
    monitoring_start_time_ = get_monotonic_time_ns();
}

void IngressEventMonitor::update_config(const ClockQualityConfig& config) {
    config_ = config;
    
    // Trim history if new limit is smaller
    if (measurements_.size() > config_.max_history_measurements) {
        trim_measurement_history();
    }
}

std::vector<uint8_t> IngressEventMonitor::export_tlv_data() const {
    // TODO: Implement TLV encoding for remote monitoring
    // This would follow IEEE 1588 TLV format for ingress monitoring data
    std::vector<uint8_t> tlv_data;
    
    // TLV Header (simplified)
    // Type: 0x8001 (custom OpenAvnu clock quality TLV)
    tlv_data.push_back(0x80);
    tlv_data.push_back(0x01);
    
    // Length: (measurements count * measurement size) + header
    uint16_t length = static_cast<uint16_t>(measurements_.size() * 32 + 8);
    tlv_data.push_back((length >> 8) & 0xFF);
    tlv_data.push_back(length & 0xFF);
    
    // Config data
    tlv_data.push_back((config_.measurement_interval_ms >> 8) & 0xFF);
    tlv_data.push_back(config_.measurement_interval_ms & 0xFF);
    tlv_data.push_back(static_cast<uint8_t>(config_.profile_type));
    tlv_data.push_back(monitoring_enabled_ ? 1 : 0);
    
    // Measurement count
    uint32_t count = static_cast<uint32_t>(measurements_.size());
    tlv_data.push_back((count >> 24) & 0xFF);
    tlv_data.push_back((count >> 16) & 0xFF);
    tlv_data.push_back((count >> 8) & 0xFF);
    tlv_data.push_back(count & 0xFF);
    
    // Measurements (simplified encoding - timestamp + offset)
    for (const auto& measurement : measurements_) {
        if (!measurement.valid) continue;
        
        // Timestamp (8 bytes)
        uint64_t ts = measurement.timestamp_ns;
        for (int i = 7; i >= 0; i--) {
            tlv_data.push_back((ts >> (i * 8)) & 0xFF);
        }
        
        // Offset from master (8 bytes, signed)
        uint64_t offset = static_cast<uint64_t>(measurement.offset_from_master_ns);
        for (int i = 7; i >= 0; i--) {
            tlv_data.push_back((offset >> (i * 8)) & 0xFF);
        }
        
        // Path delay (8 bytes)
        uint64_t delay = measurement.path_delay_ns;
        for (int i = 7; i >= 0; i--) {
            tlv_data.push_back((delay >> (i * 8)) & 0xFF);
        }
        
        // Correction field (8 bytes)
        uint64_t corr = measurement.correction_field_ns;
        for (int i = 7; i >= 0; i--) {
            tlv_data.push_back((corr >> (i * 8)) & 0xFF);
        }
    }
    
    return tlv_data;
}

bool IngressEventMonitor::import_tlv_data(const std::vector<uint8_t>& tlv_data) {
    // TODO: Implement TLV decoding for remote monitoring data
    // This would parse the TLV format created by export_tlv_data()
    
    if (tlv_data.size() < 8) {
        return false; // Minimum TLV header size
    }
    
    // Validate TLV type
    uint16_t type = (static_cast<uint16_t>(tlv_data[0]) << 8) | tlv_data[1];
    if (type != 0x8001) {
        return false; // Wrong TLV type
    }
    
    // Parse length  
    uint16_t length = (static_cast<uint16_t>(tlv_data[2]) << 8) | tlv_data[3];
    if (tlv_data.size() < length + 4) {
        return false; // Insufficient data
    }
    
    // TODO: Complete implementation
    return true;
}

// ============================================================================
// ClockQualityAnalyzer Implementation  
// ============================================================================

ClockQualityAnalyzer::ClockQualityAnalyzer(const ClockQualityConfig& config)
    : config_(config)
{
}

void ClockQualityAnalyzer::calculate_statistics(
    const std::deque<ClockQualityMeasurement>& measurements,
    ClockQualityMetrics& metrics) const {
    
    if (measurements.empty()) {
        return;
    }
    
    // Extract valid time errors
    std::vector<int64_t> time_errors;
    for (const auto& measurement : measurements) {
        if (measurement.valid) {
            time_errors.push_back(measurement.offset_from_master_ns);
        }
    }
    
    if (time_errors.empty()) {
        return;
    }
    
    metrics.total_measurements = static_cast<uint32_t>(time_errors.size());
    
    // Calculate mean
    int64_t sum = 0;
    for (int64_t error : time_errors) {
        sum += error;
    }
    metrics.mean_time_error_ns = sum / static_cast<int64_t>(time_errors.size());
    
    // Calculate min/max
    auto minmax = std::minmax_element(time_errors.begin(), time_errors.end());
    metrics.min_time_error_ns = *minmax.first;
    metrics.max_time_error_ns = *minmax.second;
    
    // Calculate standard deviation
    double mean_double = static_cast<double>(metrics.mean_time_error_ns);
    metrics.std_dev_ns = calculate_standard_deviation(time_errors, mean_double);
    
    // Calculate RMS error
    metrics.rms_error_ns = calculate_rms_error(time_errors);
    
    // Count outliers (measurements exceeding target accuracy)
    metrics.outlier_count = 0;
    metrics.consecutive_good_measurements = 0;
    uint32_t current_consecutive = 0;
    
    for (int64_t error : time_errors) {
        if (std::abs(error) > config_.target_accuracy_ns) {
            metrics.outlier_count++;
            current_consecutive = 0;
        } else {
            current_consecutive++;
            metrics.consecutive_good_measurements = std::max(
                metrics.consecutive_good_measurements, current_consecutive);
        }
    }
    
    // Set timestamps
    if (!measurements.empty()) {
        metrics.measurement_start_time = measurements.front().timestamp_ns;
        metrics.last_measurement_time = measurements.back().timestamp_ns;
        
        uint64_t duration_ns = metrics.last_measurement_time - metrics.measurement_start_time;
        metrics.observation_window_s = static_cast<uint32_t>(duration_ns / 1000000000ULL);
    }
}

bool ClockQualityAnalyzer::detect_lock_state(
    const std::deque<ClockQualityMeasurement>& measurements) const {
    
    if (measurements.size() < 10) {
        return false; // Need minimum measurements to determine lock
    }
    
    // Check if recent measurements are within accuracy bounds
    uint32_t recent_good_count = 0;        const uint32_t CHECK_COUNT = std::min(static_cast<uint32_t>(measurements.size()), 10u);
    
    for (auto it = measurements.rbegin(); 
         it != measurements.rbegin() + static_cast<std::deque<ClockQualityMeasurement>::const_reverse_iterator::difference_type>(CHECK_COUNT); 
         ++it) {
        if (it->valid && std::abs(it->offset_from_master_ns) <= config_.target_accuracy_ns) {
            recent_good_count++;
        }
    }
    
    // Consider locked if 80% of recent measurements are within bounds
    return (recent_good_count * 100 / CHECK_COUNT) >= 80;
}

uint32_t ClockQualityAnalyzer::calculate_lock_time(
    const std::deque<ClockQualityMeasurement>& measurements) const {
    
    if (measurements.empty()) {
        return 0;
    }
    
    // Find first sustained period of good measurements
    uint32_t consecutive_good = 0;
    const uint32_t REQUIRED_CONSECUTIVE = 5; // 5 consecutive good measurements
    
    for (const auto& measurement : measurements) {
        if (measurement.valid && 
            std::abs(measurement.offset_from_master_ns) <= config_.target_accuracy_ns) {
            consecutive_good++;
            if (consecutive_good >= REQUIRED_CONSECUTIVE) {
                // Found lock point - calculate time from start
                uint64_t lock_time_ns = measurement.timestamp_ns - measurements.front().timestamp_ns;
                return static_cast<uint32_t>(lock_time_ns / 1000000000ULL);
            }
        } else {
            consecutive_good = 0;
        }
    }
    
    return 0; // Never achieved lock
}

ClockQualityMetrics ClockQualityAnalyzer::analyze_measurements(
    const std::deque<ClockQualityMeasurement>& measurements,
    uint32_t window_seconds) const {
    
    ClockQualityMetrics metrics;
    metrics.active_profile = config_.profile_type;
    metrics.measurement_method = ClockQualityMethod::INGRESS_REPORTING;
    metrics.measurement_interval_ms = config_.measurement_interval_ms;
    
    if (measurements.empty()) {
        return metrics;
    }
    
    // Select measurements within the specified window
    std::deque<ClockQualityMeasurement> window_measurements;
    if (window_seconds == 0) {
        // Use all measurements
        window_measurements = measurements;
    } else {
        // Select recent measurements within window
        uint64_t current_time = get_monotonic_time_ns();
        uint64_t window_start = current_time - (static_cast<uint64_t>(window_seconds) * 1000000000ULL);
        
        for (const auto& measurement : measurements) {
            if (measurement.timestamp_ns >= window_start) {
                window_measurements.push_back(measurement);
            }
        }
    }
    
    // Calculate statistics
    calculate_statistics(window_measurements, metrics);
    
    // Determine lock state
    metrics.is_locked = detect_lock_state(window_measurements);
    metrics.lock_time_seconds = calculate_lock_time(measurements); // Use full history for lock time
    
    // Validate certification requirements
    metrics.meets_80ns_requirement = (std::abs(metrics.max_time_error_ns) <= 80);
    metrics.meets_lock_time_requirement = (metrics.lock_time_seconds <= config_.max_lock_time_s);
    metrics.meets_stability_requirement = (metrics.observation_window_s >= config_.stability_window_s) &&
                                        metrics.is_locked;
    
    // Calculate frequency stability (simplified)
    if (window_measurements.size() > 1) {
        // Estimate frequency stability from time error drift
        double time_span_s = static_cast<double>(metrics.observation_window_s);
        double error_drift_ns = static_cast<double>(
            window_measurements.back().offset_from_master_ns - 
            window_measurements.front().offset_from_master_ns);
        
        if (time_span_s > 0) {
            // Convert to parts per billion
            metrics.frequency_stability_ppb = (error_drift_ns / time_span_s) / 1000.0;
        }
    }
    
    return metrics;
}

bool ClockQualityAnalyzer::validate_certification_requirements(
    const ClockQualityMetrics& metrics, ProfileType profile) const {
    
    switch (profile) {
        case ProfileType::MILAN:
            return validate_milan_requirements(metrics);
        case ProfileType::AUTOMOTIVE:
            return validate_automotive_requirements(metrics);
        case ProfileType::AVNU_BASE:
            return validate_avnu_base_requirements(metrics);
        case ProfileType::STANDARD:
        default:
            return metrics.meets_80ns_requirement && 
                   metrics.meets_lock_time_requirement && 
                   metrics.meets_stability_requirement;
    }
}

bool ClockQualityAnalyzer::validate_milan_requirements(const ClockQualityMetrics& metrics) const {
    // Milan specific requirements:
    // - ±80ns accuracy (standard requirement)
    // - 15ms late response tolerance (handled in profile)
    // - Enhanced stability for professional audio
    
    return metrics.meets_80ns_requirement &&
           metrics.meets_lock_time_requirement &&
           metrics.meets_stability_requirement &&
           metrics.std_dev_ns <= 20.0; // Stricter stability requirement
}

bool ClockQualityAnalyzer::validate_automotive_requirements(const ClockQualityMetrics& metrics) const {
    // Automotive specific requirements:
    // - ±50ns accuracy (stricter than standard)
    // - Immediate asCapable behavior
    // - Enhanced frequency stability
    
    const int64_t AUTOMOTIVE_ACCURACY_NS = 50;
    
    return (std::abs(metrics.max_time_error_ns) <= AUTOMOTIVE_ACCURACY_NS) &&
           (metrics.lock_time_seconds <= 1) && // Faster lock requirement
           metrics.meets_stability_requirement &&
           (std::abs(metrics.frequency_stability_ppb) <= 100.0); // ±100ppb
}

bool ClockQualityAnalyzer::validate_avnu_base_requirements(const ClockQualityMetrics& metrics) const {
    // AVnu Base specific requirements:
    // - ±80ns accuracy (standard requirement)
    // - 2-10 PDelay successes for asCapable (handled in profile)
    // - Professional AV requirements
    
    return metrics.meets_80ns_requirement &&
           metrics.meets_lock_time_requirement &&
           metrics.meets_stability_requirement &&
           (metrics.outlier_count * 100 / metrics.total_measurements) <= 5; // ≤5% outliers
}

std::string ClockQualityAnalyzer::generate_compliance_report(const ClockQualityMetrics& metrics) const {
    std::ostringstream report;
    
    report << "=== OpenAvnu gPTP Clock Quality Compliance Report ===\n\n";
    
    // Header information
    report << "Profile: ";
    switch (metrics.active_profile) {
        case ProfileType::MILAN: report << "Milan Baseline Interoperability\n"; break;
        case ProfileType::AUTOMOTIVE: report << "Automotive Ethernet AVB\n"; break;
        case ProfileType::AVNU_BASE: report << "AVnu Base/ProAV\n"; break;
        case ProfileType::STANDARD: report << "IEEE 802.1AS Standard\n"; break;
    }
    
    report << "Measurement Method: ";
    switch (metrics.measurement_method) {
        case ClockQualityMethod::INGRESS_REPORTING: report << "Ingress Reporting\n"; break;
        case ClockQualityMethod::REVERSE_SYNC: report << "Reverse Sync\n"; break;
        case ClockQualityMethod::PPS_HARDWARE: report << "1PPS Hardware\n"; break;
        case ClockQualityMethod::COMBINED: report << "Combined Methods\n"; break;
    }
    
    report << "Observation Window: " << metrics.observation_window_s << " seconds\n";
    report << "Total Measurements: " << metrics.total_measurements << "\n\n";
    
    // Accuracy metrics
    report << "--- Accuracy Metrics ---\n";
    report << "Mean Time Error: " << metrics.mean_time_error_ns << " ns\n";
    report << "Max Time Error: " << metrics.max_time_error_ns << " ns\n";
    report << "Min Time Error: " << metrics.min_time_error_ns << " ns\n";
    report << "Standard Deviation: " << std::fixed << std::setprecision(2) 
           << metrics.std_dev_ns << " ns\n";
    report << "RMS Error: " << std::fixed << std::setprecision(2) 
           << metrics.rms_error_ns << " ns\n\n";
    
    // Lock performance
    report << "--- Lock Performance ---\n";
    report << "Lock Time: " << metrics.lock_time_seconds << " seconds\n";
    report << "Currently Locked: " << (metrics.is_locked ? "YES" : "NO") << "\n";
    report << "Consecutive Good Measurements: " << metrics.consecutive_good_measurements << "\n\n";
    
    // Stability metrics
    report << "--- Stability Metrics ---\n";
    report << "Frequency Stability: " << std::fixed << std::setprecision(2) 
           << metrics.frequency_stability_ppb << " ppb\n";
    report << "Outlier Count: " << metrics.outlier_count << " / " << metrics.total_measurements;
    if (metrics.total_measurements > 0) {
        report << " (" << std::fixed << std::setprecision(1) 
               << (metrics.outlier_count * 100.0 / metrics.total_measurements) << "%)\n";
    } else {
        report << "\n";
    }
    report << "\n";
    
    // Certification compliance
    report << "--- Certification Compliance ---\n";
    report << "±80ns Accuracy: " << (metrics.meets_80ns_requirement ? "PASS" : "FAIL") << "\n";
    report << "≤6s Lock Time: " << (metrics.meets_lock_time_requirement ? "PASS" : "FAIL") << "\n";
    report << "5min Stability: " << (metrics.meets_stability_requirement ? "PASS" : "FAIL") << "\n";
    
    bool overall_pass = validate_certification_requirements(metrics, metrics.active_profile);
    report << "\nOVERALL COMPLIANCE: " << (overall_pass ? "PASS" : "FAIL") << "\n";
    
    return report.str();
}

std::string ClockQualityAnalyzer::generate_certification_report(const ClockQualityMetrics& metrics) const {
    std::ostringstream report;
    
    report << "=== Avnu Alliance Certification Test Report ===\n\n";
    report << "Test: gPTP Clock Quality Measurement\n";
    report << "Specification: 802.1AS Recovered Clock Quality Testing v1.0\n";
    report << "Date: " << "2025-07-08" << "\n"; // TODO: Use actual date
    report << "Device Under Test: OpenAvnu gPTP Implementation\n\n";
    
    // Test parameters
    report << "--- Test Parameters ---\n";
    report << "Profile: ";
    switch (metrics.active_profile) {
        case ProfileType::MILAN: report << "Milan\n"; break;
        case ProfileType::AUTOMOTIVE: report << "Automotive\n"; break;
        case ProfileType::AVNU_BASE: report << "AVnu Base\n"; break;
        case ProfileType::STANDARD: report << "Standard\n"; break;
    }
    report << "Measurement Method: Ingress Reporting (Section 5.2)\n";
    report << "Target Accuracy: ±80ns\n";
    report << "Required Lock Time: ≤6 seconds\n";
    report << "Observation Window: 5 minutes (300 seconds)\n\n";
    
    // Test results
    report << "--- Test Results ---\n";
    report << "Measured Accuracy: " << std::abs(metrics.max_time_error_ns) << "ns\n";
    report << "Achieved Lock Time: " << metrics.lock_time_seconds << " seconds\n";
    report << "Stability Duration: " << metrics.observation_window_s << " seconds\n";
    report << "Statistical Quality: σ = " << std::fixed << std::setprecision(2) 
           << metrics.std_dev_ns << "ns\n\n";
    
    // Pass/Fail criteria
    report << "--- Certification Criteria ---\n";
    report << "Accuracy ≤ 80ns: " << (metrics.meets_80ns_requirement ? "PASS" : "FAIL") << "\n";
    report << "Lock Time ≤ 6s: " << (metrics.meets_lock_time_requirement ? "PASS" : "FAIL") << "\n";
    report << "Stability ≥ 5min: " << (metrics.meets_stability_requirement ? "PASS" : "FAIL") << "\n";
    
    bool overall_pass = validate_certification_requirements(metrics, metrics.active_profile);
    report << "\n--- CERTIFICATION RESULT ---\n";
    report << "STATUS: " << (overall_pass ? "PASS - COMPLIANT" : "FAIL - NON-COMPLIANT") << "\n";
    
    if (overall_pass) {
        report << "\nThe device meets all requirements for Avnu Alliance certification\n";
        report << "under the " << (metrics.active_profile == ProfileType::MILAN ? "Milan" :
                                  metrics.active_profile == ProfileType::AUTOMOTIVE ? "Automotive" :
                                  metrics.active_profile == ProfileType::AVNU_BASE ? "AVnu Base" :
                                  "Standard") << " profile.\n";
    } else {
        report << "\nThe device does NOT meet certification requirements.\n";
        report << "Please review and address the failed criteria above.\n";
    }
    
    return report.str();
}

} // namespace gPTP
} // namespace OpenAvnu
