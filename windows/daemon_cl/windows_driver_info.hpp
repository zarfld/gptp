/******************************************************************************

  Copyright (c) 2024, Intel Corporation
  All rights reserved.

  Windows Driver-Specific Information Framework for gPTP

******************************************************************************/

#ifndef WINDOWS_DRIVER_INFO_HPP
#define WINDOWS_DRIVER_INFO_HPP

#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <memory>

/**
 * @brief Comprehensive driver-specific information structure
 * 
 * This framework organizes all driver-specific information needed for
 * optimal gPTP timestamping performance. It goes beyond INF file data
 * to include runtime capabilities and performance characteristics.
 */

/**
 * @brief Hardware timestamping capabilities flags
 */
enum TimestampCapabilities : uint32_t {
    TIMESTAMP_CAP_NONE = 0x00000000,
    TIMESTAMP_CAP_TX_SOFTWARE = 0x00000001,
    TIMESTAMP_CAP_TX_HARDWARE = 0x00000002,
    TIMESTAMP_CAP_RX_SOFTWARE = 0x00000004,
    TIMESTAMP_CAP_RX_HARDWARE = 0x00000008,
    TIMESTAMP_CAP_CROSS_TIMESTAMP = 0x00000010,
    TIMESTAMP_CAP_ONE_STEP_TX = 0x00000020,
    TIMESTAMP_CAP_ONE_STEP_RX = 0x00000040,
    TIMESTAMP_CAP_ONE_STEP_SYNC = 0x00000080,
    TIMESTAMP_CAP_ONE_STEP_P2P = 0x00000100,
    TIMESTAMP_CAP_PPS_OUTPUT = 0x00000200,
    TIMESTAMP_CAP_PPS_INPUT = 0x00000400,
    TIMESTAMP_CAP_FREQ_ADJUSTMENT = 0x00000800,
    TIMESTAMP_CAP_PHASE_ADJUSTMENT = 0x00001000
};

/**
 * @brief Driver vendor identification
 */
enum class VendorType {
    UNKNOWN,
    INTEL,
    BROADCOM,
    MELLANOX,
    REALTEK,
    MARVELL,
    MICROSOFT  // Hyper-V synthetic
};

/**
 * @brief Hardware clock source information
 */
struct ClockSourceInfo {
    uint64_t nominal_frequency_hz;     //!< Nominal clock frequency
    uint64_t actual_frequency_hz;      //!< Measured actual frequency
    uint32_t resolution_ns;            //!< Clock resolution in nanoseconds
    uint32_t stability_ppb;            //!< Clock stability in parts per billion
    uint32_t accuracy_ns;              //!< Clock accuracy in nanoseconds
    bool supports_adjustment;          //!< Supports frequency adjustment
    bool supports_cross_timestamp;    //!< Supports cross-timestamping
};

/**
 * @brief Driver performance characteristics
 */
struct DriverPerformance {
    uint32_t tx_timestamp_latency_ns;    //!< TX timestamp latency
    uint32_t rx_timestamp_latency_ns;    //!< RX timestamp latency
    uint32_t interrupt_latency_ns;       //!< Interrupt processing latency
    uint32_t oid_call_overhead_ns;       //!< OID call overhead
    uint32_t cross_timestamp_overhead_ns; //!< Cross-timestamp call overhead
    double timestamp_precision_ns;       //!< Measured timestamp precision
};

/**
 * @brief Complete driver information structure
 */
struct DriverInfo {
    // === Basic Identification ===
    std::string device_description;      //!< From IPHLPAPI
    std::string device_instance_id;      //!< From Device Manager
    std::string inf_file_path;           //!< INF file location
    std::string driver_version;          //!< Driver version string
    std::string driver_date;             //!< Driver date
    
    // === Hardware Identification ===
    VendorType vendor;                   //!< Detected vendor
    uint32_t vendor_id;                  //!< PCI vendor ID
    uint32_t device_id;                  //!< PCI device ID
    uint32_t subsystem_vendor_id;        //!< PCI subsystem vendor ID
    uint32_t subsystem_device_id;        //!< PCI subsystem device ID
    uint8_t revision_id;                 //!< PCI revision ID
    
    // === MAC/Link Layer ===
    uint8_t mac_address[6];              //!< MAC address
    std::string mac_oui_prefix;          //!< OUI prefix for vendor detection
    uint32_t link_speed_mbps;            //!< Link speed in Mbps
    
    // === Timestamping Capabilities ===
    TimestampCapabilities capabilities;  //!< Supported timestamping features
    ClockSourceInfo clock_source;        //!< Clock source characteristics
    DriverPerformance performance;       //!< Performance metrics
    
    // === Registry/Configuration ===
    std::map<std::string, std::string> registry_settings; //!< Driver registry settings
    std::map<std::string, uint32_t> advanced_properties;  //!< Advanced driver properties
    
    // === Runtime State ===
    bool is_initialized;                 //!< Driver initialization status
    bool supports_oids;                  //!< OID support available
    bool supports_ndis_timestamp;       //!< NDIS timestamp support
    bool supports_ptp_hardware;         //!< Hardware PTP support
    uint64_t initialization_time_ms;    //!< Time taken to initialize
    
    // === Quality Metrics ===
    uint32_t timestamp_quality_score;   //!< 0-100 quality score
    uint32_t reliability_score;         //!< 0-100 reliability score
    uint32_t feature_completeness_score; //!< 0-100 feature completeness
    
    // === Error Information ===
    std::vector<std::string> initialization_errors; //!< Errors during init
    std::vector<std::string> capability_warnings;   //!< Capability warnings
    std::string last_error_message;     //!< Last error encountered
};

/**
 * @brief Driver information collector and manager
 */
class WindowsDriverInfoCollector {
public:
    /**
     * @brief Collect comprehensive driver information
     * @param mac_address MAC address of the interface
     * @return Complete driver information structure
     */
    static std::unique_ptr<DriverInfo> collectDriverInfo(const uint8_t* mac_address);
    
    /**
     * @brief Update runtime performance metrics
     * @param info Driver info to update
     * @param interface_handle Handle to the network interface
     */
    static void updatePerformanceMetrics(DriverInfo& info, HANDLE interface_handle);
    
    /**
     * @brief Validate driver compatibility for gPTP
     * @param info Driver information
     * @return Compatibility score (0-100)
     */
    static uint32_t validateCompatibility(const DriverInfo& info);
    
    /**
     * @brief Generate driver report for diagnostics
     * @param info Driver information
     * @return Formatted diagnostic report
     */
    static std::string generateDiagnosticReport(const DriverInfo& info);

private:
    // === Information Gathering Methods ===
    static void collectBasicInfo(DriverInfo& info, const uint8_t* mac_address);
    static void collectHardwareInfo(DriverInfo& info);
    static void collectTimestampCapabilities(DriverInfo& info, HANDLE interface_handle);
    static void collectPerformanceMetrics(DriverInfo& info, HANDLE interface_handle);
    static void collectRegistrySettings(DriverInfo& info);
    static VendorType detectVendor(const std::string& description, uint32_t vendor_id);
    
    // === Analysis Integration Methods ===
    static void integrateAnalysisBasedInfo(DriverInfo& info);
    static void integrateIntelAnalysisData(DriverInfo& info);
    static bool isIntelDevice(const std::string& device_description);
    static void calculateQualityMetrics(DriverInfo& info);
    
    // === Capability Detection Methods ===
    static TimestampCapabilities detectTimestampCapabilities(HANDLE interface_handle);
    static ClockSourceInfo detectClockSource(HANDLE interface_handle, const std::string& description);
    static bool testCrossTimestampCapability(HANDLE interface_handle);
    static bool testOIDSupport(HANDLE interface_handle);
    
    // === Performance Testing Methods ===
    static uint32_t measureTimestampLatency(HANDLE interface_handle, bool is_tx);
    static double measureTimestampPrecision(HANDLE interface_handle);
    static uint32_t measureOIDOverhead(HANDLE interface_handle);
};

/**
 * @brief Driver-specific task preconditions framework
 * 
 * This addresses the TODO in common_tstamper.hpp by defining what
 * driver-specific information should be organized as preconditions
 * for various timestamping tasks.
 */
class DriverTaskPreconditions {
public:
    /**
     * @brief Check if cross-timestamping is supported
     * @param info Driver information
     * @return true if supported with adequate quality
     */
    static bool canPerformCrossTimestamping(const DriverInfo& info);
    
    /**
     * @brief Check if hardware timestamping is supported
     * @param info Driver information
     * @return true if hardware timestamping available
     */
    static bool canPerformHardwareTimestamping(const DriverInfo& info);
    
    /**
     * @brief Check if frequency adjustment is supported
     * @param info Driver information
     * @return true if frequency adjustment available
     */
    static bool canPerformFrequencyAdjustment(const DriverInfo& info);
    
    /**
     * @brief Get recommended timestamping method
     * @param info Driver information
     * @return Recommended method (cross-timestamp, hardware, software)
     */
    static std::string getRecommendedTimestampingMethod(const DriverInfo& info);
    
    /**
     * @brief Get driver-specific optimization parameters
     * @param info Driver information
     * @return Map of optimization parameters
     */
    static std::map<std::string, uint32_t> getOptimizationParameters(const DriverInfo& info);
};

/**
 * @brief Analysis-based device information structure
 * 
 * Contains information extracted from driver file analysis
 * that supplements runtime detection capabilities.
 */
struct AnalysisBasedDeviceInfo {
    uint32_t clock_frequency;      ///< Hardware clock frequency from analysis
    uint32_t capabilities;         ///< Capability flags from INF/registry analysis
    uint32_t confidence_score;     ///< Confidence in analysis data (0-100)
    std::string source_version;    ///< Driver version this analysis is based on
    std::string analysis_date;     ///< When this analysis was performed
};

#endif // WINDOWS_DRIVER_INFO_HPP
