/******************************************************************************

  Windows Driver Information Collection - Minimal Implementation

******************************************************************************/

#include "windows_driver_info.hpp"
#include <iphlpapi.h>

#pragma comment(lib, "iphlpapi.lib")

std::unique_ptr<DriverInfo> WindowsDriverInfoCollector::collectDriverInfo(const uint8_t* mac_address) {
    auto info = std::make_unique<DriverInfo>();
    
    // Initialize with defaults
    info->vendor = VendorType::UNKNOWN;
    info->vendor_id = 0;
    info->device_id = 0;
    info->capabilities = static_cast<TimestampCapabilities>(TIMESTAMP_CAP_TX_SOFTWARE | TIMESTAMP_CAP_RX_SOFTWARE);
    info->is_initialized = false;
    info->supports_oids = false;
    info->supports_ndis_timestamp = false;
    info->supports_ptp_hardware = false;
    info->timestamp_quality_score = 50;
    info->reliability_score = 50;
    info->feature_completeness_score = 50;
    
    // Set MAC address if provided
    if (mac_address) {
        memcpy(info->mac_address, mac_address, 6);
    }
    
    // Basic device detection
    info->device_description = "Generic Network Adapter";
    
    info->is_initialized = true;
    return info;
}

void WindowsDriverInfoCollector::updatePerformanceMetrics(DriverInfo& info, HANDLE interface_handle) {
    // Stub implementation
    info.performance.tx_timestamp_latency_ns = 1000;
    info.performance.rx_timestamp_latency_ns = 1000;
}

uint32_t WindowsDriverInfoCollector::validateCompatibility(const DriverInfo& info) {
    // Basic compatibility scoring
    uint32_t score = 0;
    
    if (info.is_initialized) score += 20;
    if (info.supports_oids) score += 20;
    if (info.supports_ptp_hardware) score += 30;
    if (info.vendor == VendorType::INTEL) score += 30;
    
    return score;
}

std::string WindowsDriverInfoCollector::generateDiagnosticReport(const DriverInfo& info) {
    std::string report = "Driver Information Report\n";
    report += "========================\n";
    report += "Device: " + info.device_description + "\n";
    report += "Initialized: " + std::string(info.is_initialized ? "Yes" : "No") + "\n";
    report += "Quality Score: " + std::to_string(info.timestamp_quality_score) + "\n";
    return report;
}

// Private method stubs
void WindowsDriverInfoCollector::collectBasicInfo(DriverInfo& info, const uint8_t* mac_address) {
    info.device_description = "Network Adapter";
}

void WindowsDriverInfoCollector::collectHardwareInfo(DriverInfo& info) {
    info.vendor = VendorType::UNKNOWN;
    info.vendor_id = 0;
    info.device_id = 0;
}

void WindowsDriverInfoCollector::collectTimestampCapabilities(DriverInfo& info, HANDLE interface_handle) {
    info.capabilities = static_cast<TimestampCapabilities>(TIMESTAMP_CAP_TX_SOFTWARE);
}

void WindowsDriverInfoCollector::collectPerformanceMetrics(DriverInfo& info, HANDLE interface_handle) {
    info.performance.tx_timestamp_latency_ns = 1000;
    info.performance.rx_timestamp_latency_ns = 1000;
}

void WindowsDriverInfoCollector::collectRegistrySettings(DriverInfo& info) {
    // Stub
}

VendorType WindowsDriverInfoCollector::detectVendor(const std::string& description, uint32_t vendor_id) {
    if (description.find("Intel") != std::string::npos || vendor_id == 0x8086) {
        return VendorType::INTEL;
    }
    return VendorType::UNKNOWN;
}

void WindowsDriverInfoCollector::integrateAnalysisBasedInfo(DriverInfo& info) {
    // Check if this is an Intel device
    if (info.device_description.find("Intel") != std::string::npos) {
        info.vendor = VendorType::INTEL;
        info.vendor_id = 0x8086;
        info.timestamp_quality_score = 80;
    }
}

void WindowsDriverInfoCollector::integrateIntelAnalysisData(DriverInfo& info) {
    // Enhanced Intel device detection
    if (info.device_description.find("I210") != std::string::npos) {
        info.clock_source.nominal_frequency_hz = 125000000;
        info.timestamp_quality_score = 90;
    } else if (info.device_description.find("I225") != std::string::npos) {
        info.clock_source.nominal_frequency_hz = 200000000;
        info.timestamp_quality_score = 95;
    }
}

bool WindowsDriverInfoCollector::isIntelDevice(const std::string& device_description) {
    return device_description.find("Intel") != std::string::npos;
}

void WindowsDriverInfoCollector::calculateQualityMetrics(DriverInfo& info) {
    // Basic quality calculation
    if (info.vendor == VendorType::INTEL) {
        uint32_t new_score = info.timestamp_quality_score + 20;
        info.timestamp_quality_score = (new_score > 100) ? 100 : new_score;
    }
}

TimestampCapabilities WindowsDriverInfoCollector::detectTimestampCapabilities(HANDLE interface_handle) {
    return static_cast<TimestampCapabilities>(TIMESTAMP_CAP_TX_SOFTWARE | TIMESTAMP_CAP_RX_SOFTWARE);
}

ClockSourceInfo WindowsDriverInfoCollector::detectClockSource(HANDLE interface_handle, const std::string& description) {
    ClockSourceInfo info = {};
    info.nominal_frequency_hz = 125000000; // Default
    info.resolution_ns = 8; // 8ns resolution for 125MHz
    return info;
}

bool WindowsDriverInfoCollector::testCrossTimestampCapability(HANDLE interface_handle) {
    return false; // Conservative default
}

bool WindowsDriverInfoCollector::testOIDSupport(HANDLE interface_handle) {
    return false; // Conservative default
}

uint32_t WindowsDriverInfoCollector::measureTimestampLatency(HANDLE interface_handle, bool is_tx) {
    return 1000; // 1 microsecond default
}

double WindowsDriverInfoCollector::measureTimestampPrecision(HANDLE interface_handle) {
    return 8.0; // 8ns default precision
}

uint32_t WindowsDriverInfoCollector::measureOIDOverhead(HANDLE interface_handle) {
    return 100; // 100ns default overhead
}

// DriverTaskPreconditions implementation
bool DriverTaskPreconditions::canPerformCrossTimestamping(const DriverInfo& info) {
    return (static_cast<uint32_t>(info.capabilities) & TIMESTAMP_CAP_CROSS_TIMESTAMP) != 0;
}

bool DriverTaskPreconditions::canPerformHardwareTimestamping(const DriverInfo& info) {
    return (static_cast<uint32_t>(info.capabilities) & TIMESTAMP_CAP_TX_HARDWARE) != 0;
}

bool DriverTaskPreconditions::canPerformFrequencyAdjustment(const DriverInfo& info) {
    return (static_cast<uint32_t>(info.capabilities) & TIMESTAMP_CAP_FREQ_ADJUSTMENT) != 0;
}

std::string DriverTaskPreconditions::getRecommendedTimestampingMethod(const DriverInfo& info) {
    if (canPerformCrossTimestamping(info)) return "cross-timestamp";
    if (canPerformHardwareTimestamping(info)) return "hardware";
    return "software";
}

std::map<std::string, uint32_t> DriverTaskPreconditions::getOptimizationParameters(const DriverInfo& info) {
    std::map<std::string, uint32_t> params;
    params["clock_rate"] = static_cast<uint32_t>(info.clock_source.nominal_frequency_hz);
    params["precision_ns"] = info.clock_source.resolution_ns;
    return params;
}
