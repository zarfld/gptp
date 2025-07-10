/******************************************************************************

  Copyright (c) 2024, Intel Corporation
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of the Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/

#ifndef WINDOWS_CROSSTSTAMP_HPP
#define WINDOWS_CROSSTSTAMP_HPP

/**@file*/

// Prevent WinSock header conflicts
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <cstdint>
#include "../../common/ieee1588.hpp"
#include "../../common/ptptypes.hpp"
#include "intel_hal_vendor_extensions.hpp"

/**
 * @brief Windows cross-timestamping implementation
 * 
 * This class provides high-precision cross-timestamping functionality for Windows,
 * equivalent to Linux's PTP_HW_CROSSTSTAMP. It uses Windows high-precision timers
 * and hardware-specific timestamping when available.
 */
class WindowsCrossTimestamp {
public:
    /**
     * @brief Constructor
     */
    WindowsCrossTimestamp();

    /**
     * @brief Initialize cross-timestamping for a specific interface
     * @param iface_label Interface label string
     * @return true if successful, false otherwise
     */
    bool initialize(const char* iface_label);

    /**
     * @brief Cleanup cross-timestamping resources
     */
    void cleanup();

    /**
     * @brief Get synchronized system and hardware timestamps
     * @param system_time Output system timestamp
     * @param device_time Output device/hardware timestamp
     * @param local_clock Optional local clock reference
     * @param nominal_clock_rate Optional nominal clock rate
     * @return true if successful, false otherwise
     */
    bool getCrossTimestamp(
        Timestamp* system_time,
        Timestamp* device_time,
        uint32_t* local_clock = nullptr,
        uint32_t* nominal_clock_rate = nullptr
    );

    /**
     * @brief Check if precise cross-timestamping is supported
     * @return true if supported, false otherwise
     */
    bool isPreciseTimestampingSupported() const;

    /**
     * @brief Get the timestamp correlation quality
     * @return Quality metric (0-100, 100 being perfect correlation)
     */
    uint32_t getTimestampQuality() const;

    /**
     * @brief Get the estimated error in nanoseconds
     * @return Estimated error in nanoseconds
     */
    uint64_t getEstimatedError() const;

    /**
     * @brief Initialize Intel HAL integration if available
     * @param device_name Intel device name for HAL initialization
     * @return true if Intel HAL integration successful
     */
    bool initializeIntelHAL(const char* device_name);

    /**
     * @brief Check if Intel HAL is available for this interface
     * @return true if Intel HAL is available and working
     */
    bool isIntelHALAvailable() const;

private:
    /**
     * @brief Hardware-specific cross-timestamp methods
     */
    enum class TimestampMethod {
        UNKNOWN = 0,
        QPC_SYSTEM_TIME,        // QueryPerformanceCounter + GetSystemTimePreciseAsFileTime
        RDTSC_SYSTEM_TIME,      // RDTSC + GetSystemTimePreciseAsFileTime
        HARDWARE_ASSISTED,      // Hardware-assisted timestamping (Intel I210, etc.)
        INTEL_HAL_HARDWARE,     // Intel HAL hardware timestamping
        FALLBACK_CORRELATION    // Fallback correlation method
    };

    /**
     * @brief Detect the best available timestamping method
     * @return Best available method
     */
    TimestampMethod detectBestMethod();

    /**
     * @brief Get cross-timestamp using QueryPerformanceCounter
     * @param system_time Output system timestamp
     * @param device_time Output device timestamp
     * @return true if successful
     */
    bool getCrossTimestamp_QPC(Timestamp* system_time, Timestamp* device_time);

    /**
     * @brief Get cross-timestamp using RDTSC
     * @param system_time Output system timestamp
     * @param device_time Output device timestamp
     * @return true if successful
     */
    bool getCrossTimestamp_RDTSC(Timestamp* system_time, Timestamp* device_time);

    /**
     * @brief Get cross-timestamp using hardware assistance
     * @param system_time Output system timestamp
     * @param device_time Output device timestamp
     * @return true if successful
     */
    bool getCrossTimestamp_Hardware(Timestamp* system_time, Timestamp* device_time);

    /**
     * @brief Get cross-timestamp using Intel HAL hardware timestamping
     * @param system_time Output system timestamp
     * @param device_time Output device timestamp
     * @return true if successful, false otherwise
     */
    bool getCrossTimestamp_IntelHAL(Timestamp* system_time, Timestamp* device_time);

    /**
     * @brief Fallback correlation method
     * @param system_time Output system timestamp
     * @param device_time Output device timestamp
     * @return true if successful
     */
    bool getCrossTimestamp_Fallback(Timestamp* system_time, Timestamp* device_time);

    /**
     * @brief Convert FILETIME to Timestamp
     * @param ft FILETIME structure
     * @return Timestamp
     */
    Timestamp fileTimeToTimestamp(const FILETIME& ft);

    /**
     * @brief Convert performance counter to nanoseconds
     * @param counter Performance counter value
     * @return Nanoseconds
     */
    uint64_t performanceCounterToNanoseconds(const LARGE_INTEGER& counter);

    /**
     * @brief Calibrate the correlation between different time sources
     */
    void calibrateCorrelation();

    /**
     * @brief Assess initial cross-timestamp quality during initialization
     */
    void assessInitialQuality();

    /**
     * @brief Update correlation parameters
     */
    void updateCorrelation();

private:
    bool m_initialized;
    char m_interface_label[256];
    TimestampMethod m_method;
    
    // Performance counter frequency
    LARGE_INTEGER m_qpc_frequency;
    
    // RDTSC frequency (if available)
    uint64_t m_rdtsc_frequency;
    
    // Correlation parameters
    struct CorrelationData {
        double offset_ns;           // Offset between time sources in nanoseconds
        double drift_ppm;           // Drift in parts per million
        LARGE_INTEGER last_qpc;     // Last QPC reading
        uint64_t last_rdtsc;        // Last RDTSC reading
        FILETIME last_systime;      // Last system time
        uint32_t calibration_count; // Number of calibrations performed
        uint64_t last_calibration;  // Last calibration time (QPC units)
    } m_correlation;
    
    // Quality metrics
    uint32_t m_quality;             // Timestamp quality (0-100)
    uint64_t m_estimated_error_ns;  // Estimated error in nanoseconds
    
    // Hardware-specific data
    void* m_hw_context;             // Hardware-specific context
    bool m_hw_available;            // Hardware timestamping available
    
    // Intel HAL integration
    void* m_intel_hal_context;      // Intel HAL context (IntelVendorExtensions::IntelHALContext*)
    bool m_intel_hal_available;     // Intel HAL available and working
    
    // Statistics
    struct Statistics {
        uint64_t successful_timestamps;
        uint64_t failed_timestamps;
        uint64_t max_error_ns;
        uint64_t min_error_ns;
        uint64_t avg_error_ns;
    } m_stats;
};

/**
 * @brief Global cross-timestamp instance (singleton pattern)
 * @return Reference to global instance
 */
WindowsCrossTimestamp& getGlobalCrossTimestamp();

/**
 * @brief Windows-specific timestamp utilities
 */
namespace WindowsTimestampUtils {
    /**
     * @brief Check if GetSystemTimePreciseAsFileTime is available
     * @return true if available
     */
    bool isSystemTimePreciseAvailable();

    /**
     * @brief Check if RDTSC is available and stable
     * @return true if available and stable
     */
    bool isRDTSCAvailable();

    /**
     * @brief Get RDTSC frequency
     * @return RDTSC frequency in Hz, 0 if not available
     */
    uint64_t getRDTSCFrequency();

    /**
     * @brief Get current RDTSC value
     * @return RDTSC value
     */
    uint64_t readRDTSC();

    /**
     * @brief Convert Windows FILETIME to Unix timestamp
     * @param ft FILETIME structure
     * @return Unix timestamp in nanoseconds
     */
    uint64_t fileTimeToUnixNanos(const FILETIME& ft);

    /**
     * @brief Get high-precision system time
     * @return System time in nanoseconds since Unix epoch
     */
    uint64_t getHighPrecisionSystemTime();
}

#endif // WINDOWS_CROSSTSTAMP_HPP
