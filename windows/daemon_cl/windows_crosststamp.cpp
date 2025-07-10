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

#include "windows_crosststamp.hpp"
#include "../../common/gptp_log.hpp"
#include <intrin.h>

// Windows epoch (1601-01-01) to Unix epoch (1970-01-01) in 100ns units
static const uint64_t WINDOWS_TO_UNIX_EPOCH_OFFSET = 116444736000000000ULL;

// Function pointer for GetSystemTimePreciseAsFileTime (available on Windows 8+)
typedef VOID (WINAPI *GetSystemTimePreciseAsFileTimeProc)(LPFILETIME);
static GetSystemTimePreciseAsFileTimeProc g_GetSystemTimePreciseAsFileTime = nullptr;
static bool g_SystemTimePreciseChecked = false;

WindowsCrossTimestamp::WindowsCrossTimestamp()
    : m_initialized(false)
    , m_method(TimestampMethod::UNKNOWN)
    , m_rdtsc_frequency(0)
    , m_quality(0)
    , m_estimated_error_ns(0)
    , m_hw_context(nullptr)
    , m_hw_available(false)
    , m_intel_hal_context(nullptr)
    , m_intel_hal_available(false)
{
    memset(&m_interface_label, 0, sizeof(m_interface_label));
    memset(&m_qpc_frequency, 0, sizeof(m_qpc_frequency));
    memset(&m_correlation, 0, sizeof(m_correlation));
    memset(&m_stats, 0, sizeof(m_stats));
}

bool WindowsCrossTimestamp::initialize(const char* iface_label)
{
    if (m_initialized) {
        cleanup();
    }

    if (!iface_label) {
        GPTP_LOG_ERROR("Invalid interface label for cross-timestamping");
        return false;
    }

    // Store interface label
    strncpy_s(m_interface_label, sizeof(m_interface_label), iface_label, _TRUNCATE);

    // Initialize QueryPerformanceCounter frequency
    if (!QueryPerformanceFrequency(&m_qpc_frequency) || m_qpc_frequency.QuadPart == 0) {
        GPTP_LOG_ERROR("QueryPerformanceFrequency failed or returned zero");
        return false;
    }

    // Detect the best available timestamping method
    m_method = detectBestMethod();

    // Initialize method-specific resources
    switch (m_method) {
    case TimestampMethod::RDTSC_SYSTEM_TIME:
        m_rdtsc_frequency = WindowsTimestampUtils::getRDTSCFrequency();
        if (m_rdtsc_frequency == 0) {
            GPTP_LOG_WARNING("RDTSC frequency detection failed, falling back to QPC");
            m_method = TimestampMethod::QPC_SYSTEM_TIME;
        }
        break;

    case TimestampMethod::INTEL_HAL_HARDWARE:
        // Intel HAL hardware timestamping
        if (!initializeIntelHAL(iface_label)) {
            GPTP_LOG_WARNING("Intel HAL initialization failed, falling back to QPC");
            m_method = TimestampMethod::QPC_SYSTEM_TIME;
        }
        break;

    case TimestampMethod::HARDWARE_ASSISTED:
        // TODO: Initialize hardware-specific timestamping
        // For now, fall back to QPC method
        m_method = TimestampMethod::QPC_SYSTEM_TIME;
        break;

    default:
        break;
    }

    // Perform initial calibration
    calibrateCorrelation();
    
    // Perform initial quality assessment
    assessInitialQuality();

    m_initialized = true;

    GPTP_LOG_STATUS("Cross-timestamping initialized for interface %s using method %d", 
                   m_interface_label, static_cast<int>(m_method));

    return true;
}

void WindowsCrossTimestamp::cleanup()
{
    if (m_initialized) {
        // Clean up Intel HAL resources
        if (m_intel_hal_context) {
            IntelVendorExtensions::IntelHALContext* hal_ctx = 
                static_cast<IntelVendorExtensions::IntelHALContext*>(m_intel_hal_context);
            IntelVendorExtensions::CleanupIntelHAL(hal_ctx);
            delete hal_ctx;
            m_intel_hal_context = nullptr;
            m_intel_hal_available = false;
        }
        
        // Clean up hardware-specific resources
        if (m_hw_context) {
            // TODO: Hardware-specific cleanup
            m_hw_context = nullptr;
        }

        memset(&m_correlation, 0, sizeof(m_correlation));
        memset(&m_stats, 0, sizeof(m_stats));
        
        m_initialized = false;
        m_method = TimestampMethod::UNKNOWN;
    }
}

bool WindowsCrossTimestamp::getCrossTimestamp(
    Timestamp* system_time,
    Timestamp* device_time,
    uint32_t* local_clock,
    uint32_t* nominal_clock_rate)
{
    if (!m_initialized || !system_time || !device_time) {
        return false;
    }

    bool success = false;

    // Try the selected method
    switch (m_method) {
    case TimestampMethod::QPC_SYSTEM_TIME:
        success = getCrossTimestamp_QPC(system_time, device_time);
        break;

    case TimestampMethod::RDTSC_SYSTEM_TIME:
        success = getCrossTimestamp_RDTSC(system_time, device_time);
        break;

    case TimestampMethod::INTEL_HAL_HARDWARE:
        success = getCrossTimestamp_IntelHAL(system_time, device_time);
        if (!success) {
            // Fall back to QPC method
            success = getCrossTimestamp_QPC(system_time, device_time);
        }
        break;

    case TimestampMethod::HARDWARE_ASSISTED:
        success = getCrossTimestamp_Hardware(system_time, device_time);
        if (!success) {
            // Fall back to QPC method
            success = getCrossTimestamp_QPC(system_time, device_time);
        }
        break;

    default:
        success = getCrossTimestamp_Fallback(system_time, device_time);
        break;
    }

    // Update statistics
    if (success) {
        m_stats.successful_timestamps++;
        
        // Optionally fill in local clock information
        if (local_clock) {
            *local_clock = static_cast<uint32_t>(system_time->seconds_ls);
        }
        if (nominal_clock_rate) {
            *nominal_clock_rate = 1000000000; // 1 GHz nominal rate
        }

        // Update correlation periodically
        updateCorrelation();
    } else {
        m_stats.failed_timestamps++;
    }

    return success;
}

bool WindowsCrossTimestamp::isPreciseTimestampingSupported() const
{
    return m_initialized && (m_method != TimestampMethod::FALLBACK_CORRELATION);
}

uint32_t WindowsCrossTimestamp::getTimestampQuality() const
{
    return m_quality;
}

uint64_t WindowsCrossTimestamp::getEstimatedError() const
{
    return m_estimated_error_ns;
}

WindowsCrossTimestamp::TimestampMethod WindowsCrossTimestamp::detectBestMethod()
{
    // Check for Intel HAL support (highest priority for Intel devices)
    if (IntelVendorExtensions::IsIntelHALSupported(m_interface_label, 0)) {
        GPTP_LOG_INFO("Intel HAL timestamping available for interface: %s", m_interface_label);
        return TimestampMethod::INTEL_HAL_HARDWARE;
    }
    
    // Check for hardware-assisted timestamping
    if (m_hw_available) {
        GPTP_LOG_INFO("Hardware-assisted timestamping available");
        return TimestampMethod::HARDWARE_ASSISTED;
    }

    // Check for RDTSC availability
    if (WindowsTimestampUtils::isRDTSCAvailable()) {
        GPTP_LOG_INFO("RDTSC timestamping available");
        return TimestampMethod::RDTSC_SYSTEM_TIME;
    }

    // Fall back to QueryPerformanceCounter
    GPTP_LOG_INFO("Using QueryPerformanceCounter timestamping");
    return TimestampMethod::QPC_SYSTEM_TIME;
}

bool WindowsCrossTimestamp::getCrossTimestamp_QPC(Timestamp* system_time, Timestamp* device_time)
{
    LARGE_INTEGER qpc_before, qpc_after, qpc_device;
    FILETIME ft;

    // Get timestamps as close together as possible
    QueryPerformanceCounter(&qpc_before);
    
    if (WindowsTimestampUtils::isSystemTimePreciseAvailable()) {
        g_GetSystemTimePreciseAsFileTime(&ft);
    } else {
        GetSystemTimeAsFileTime(&ft);
    }
    
    QueryPerformanceCounter(&qpc_after);
    
    // Use the average QPC value as the "device" timestamp
    qpc_device.QuadPart = (qpc_before.QuadPart + qpc_after.QuadPart) / 2;

    // Convert system time
    *system_time = fileTimeToTimestamp(ft);

    // Convert device time (QPC-based)
    uint64_t device_ns = performanceCounterToNanoseconds(qpc_device);
    device_time->set64(device_ns);

    // Estimate error based on QPC delta
    uint64_t qpc_delta = qpc_after.QuadPart - qpc_before.QuadPart;
    LARGE_INTEGER qpc_delta_li;
    qpc_delta_li.QuadPart = qpc_delta;
    uint64_t error_ns = performanceCounterToNanoseconds(qpc_delta_li);
    m_estimated_error_ns = error_ns;

    // Quality is inversely related to the measurement time window
    if (error_ns < 100) {
        m_quality = 95;
    } else if (error_ns < 500) {
        m_quality = 85;
    } else if (error_ns < 1000) {
        m_quality = 70;
    } else {
        m_quality = 50;
    }

    return true;
}

bool WindowsCrossTimestamp::getCrossTimestamp_RDTSC(Timestamp* system_time, Timestamp* device_time)
{
    uint64_t rdtsc_before, rdtsc_after, rdtsc_device;
    FILETIME ft;

    // Get timestamps as close together as possible
    rdtsc_before = WindowsTimestampUtils::readRDTSC();
    
    if (WindowsTimestampUtils::isSystemTimePreciseAvailable()) {
        g_GetSystemTimePreciseAsFileTime(&ft);
    } else {
        GetSystemTimeAsFileTime(&ft);
    }
    
    rdtsc_after = WindowsTimestampUtils::readRDTSC();

    // Use the average RDTSC value as the "device" timestamp
    rdtsc_device = (rdtsc_before + rdtsc_after) / 2;

    // Convert system time
    *system_time = fileTimeToTimestamp(ft);

    // Convert device time (RDTSC-based)
    uint64_t device_ns = (rdtsc_device * 1000000000ULL) / m_rdtsc_frequency;
    device_time->set64(device_ns);

    // Estimate error based on RDTSC delta
    uint64_t rdtsc_delta = rdtsc_after - rdtsc_before;
    uint64_t error_ns = (rdtsc_delta * 1000000000ULL) / m_rdtsc_frequency;
    m_estimated_error_ns = error_ns;

    // Quality assessment for RDTSC
    if (error_ns < 50) {
        m_quality = 98;
    } else if (error_ns < 200) {
        m_quality = 90;
    } else if (error_ns < 500) {
        m_quality = 75;
    } else {
        m_quality = 60;
    }

    return true;
}

bool WindowsCrossTimestamp::getCrossTimestamp_Hardware(Timestamp* system_time, Timestamp* device_time)
{
    // TODO: Implement hardware-assisted cross-timestamping
    // This would use adapter-specific timestamp capabilities
    // For now, fall back to QPC method
    return getCrossTimestamp_QPC(system_time, device_time);
}

bool WindowsCrossTimestamp::getCrossTimestamp_Fallback(Timestamp* system_time, Timestamp* device_time)
{
    // Simple fallback using system time for both
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    
    *system_time = fileTimeToTimestamp(ft);
    *device_time = *system_time;
    
    m_estimated_error_ns = 15600000; // ~15.6ms typical GetSystemTimeAsFileTime resolution
    m_quality = 30;
    
    return true;
}

Timestamp WindowsCrossTimestamp::fileTimeToTimestamp(const FILETIME& ft)
{
    // Convert FILETIME to nanoseconds since Unix epoch
    uint64_t windows_time = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    uint64_t unix_time_100ns = windows_time - WINDOWS_TO_UNIX_EPOCH_OFFSET;
    uint64_t unix_time_ns = unix_time_100ns * 100;
    
    Timestamp ts;
    ts.set64(unix_time_ns);
    return ts;
}

uint64_t WindowsCrossTimestamp::performanceCounterToNanoseconds(const LARGE_INTEGER& counter)
{
    // Convert QPC ticks to nanoseconds
    return (counter.QuadPart * 1000000000ULL) / m_qpc_frequency.QuadPart;
}

void WindowsCrossTimestamp::calibrateCorrelation()
{
    const int num_samples = 10;
    
    for (int i = 0; i < num_samples; i++) {
        LARGE_INTEGER qpc;
        FILETIME ft;
        
        QueryPerformanceCounter(&qpc);
        if (WindowsTimestampUtils::isSystemTimePreciseAvailable()) {
            g_GetSystemTimePreciseAsFileTime(&ft);
        } else {
            GetSystemTimeAsFileTime(&ft);
        }
        
        // Store calibration data
        m_correlation.last_qpc = qpc;
        m_correlation.last_systime = ft;
        m_correlation.calibration_count++;
        
        // Small delay between samples
        Sleep(1);
    }
    
    m_correlation.last_calibration = m_correlation.last_qpc.QuadPart;
    
    GPTP_LOG_VERBOSE("Cross-timestamp correlation calibrated with %d samples", num_samples);
}

void WindowsCrossTimestamp::assessInitialQuality()
{
    // Perform a test cross-timestamp to establish initial quality
    Timestamp test_system, test_device;
    
    switch (m_method) {
    case TimestampMethod::RDTSC_SYSTEM_TIME:
        if (getCrossTimestamp_RDTSC(&test_system, &test_device)) {
            GPTP_LOG_VERBOSE("Initial RDTSC cross-timestamp quality assessment: %d%% (error: %llu ns)", 
                           m_quality, m_estimated_error_ns);
        } else {
            GPTP_LOG_WARNING("RDTSC cross-timestamp test failed, falling back to QPC");
            m_method = TimestampMethod::QPC_SYSTEM_TIME;
            m_quality = 50; // Default QPC quality
        }
        break;
        
    case TimestampMethod::QPC_SYSTEM_TIME:
        if (getCrossTimestamp_QPC(&test_system, &test_device)) {
            GPTP_LOG_VERBOSE("Initial QPC cross-timestamp quality assessment: %d%% (error: %llu ns)", 
                           m_quality, m_estimated_error_ns);
        } else {
            GPTP_LOG_ERROR("QPC cross-timestamp test failed");
            m_quality = 0;
        }
        break;
        
    case TimestampMethod::HARDWARE_ASSISTED:
        // Hardware method falls back to QPC currently
        if (getCrossTimestamp_QPC(&test_system, &test_device)) {
            GPTP_LOG_VERBOSE("Initial hardware-assisted cross-timestamp quality assessment: %d%% (error: %llu ns)", 
                           m_quality, m_estimated_error_ns);
        } else {
            GPTP_LOG_WARNING("Hardware-assisted cross-timestamp test failed");
            m_quality = 0;
        }
        break;
        
    default:
        GPTP_LOG_WARNING("Unknown cross-timestamp method, setting default quality");
        m_quality = 30;
        break;
    }
    
    GPTP_LOG_INFO("Cross-timestamp initial quality assessment complete: %d%% (method %d)", 
                  m_quality, static_cast<int>(m_method));
}

void WindowsCrossTimestamp::updateCorrelation()
{
    // Recalibrate periodically (every 1000 timestamps or 10 seconds)
    static uint64_t last_update = 0;
    uint64_t current_time = m_correlation.last_qpc.QuadPart;
    
    if (m_stats.successful_timestamps % 1000 == 0 || 
        (current_time - last_update) > (10 * m_qpc_frequency.QuadPart)) {
        calibrateCorrelation();
        last_update = current_time;
    }
}

// Global instance
static WindowsCrossTimestamp g_crossTimestamp;

WindowsCrossTimestamp& getGlobalCrossTimestamp()
{
    return g_crossTimestamp;
}

// Namespace implementation
namespace WindowsTimestampUtils {

bool isSystemTimePreciseAvailable()
{
    if (!g_SystemTimePreciseChecked) {
        HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
        if (hKernel32) {
            g_GetSystemTimePreciseAsFileTime = 
                (GetSystemTimePreciseAsFileTimeProc)GetProcAddress(hKernel32, "GetSystemTimePreciseAsFileTime");
        }
        g_SystemTimePreciseChecked = true;
    }
    
    return g_GetSystemTimePreciseAsFileTime != nullptr;
}

bool isRDTSCAvailable()
{
    // Check if RDTSC is available and invariant
    int cpuInfo[4];
    __cpuid(cpuInfo, 0x80000007);
    
    // Check for invariant TSC (bit 8 of EDX)
    return (cpuInfo[3] & (1 << 8)) != 0;
}

uint64_t getRDTSCFrequency()
{
    if (!isRDTSCAvailable()) {
        return 0;
    }
    
    // Estimate RDTSC frequency using QueryPerformanceCounter
    LARGE_INTEGER qpc_freq, qpc_start, qpc_end;
    uint64_t rdtsc_start, rdtsc_end;
    
    QueryPerformanceFrequency(&qpc_freq);
    
    QueryPerformanceCounter(&qpc_start);
    rdtsc_start = readRDTSC();
    
    Sleep(100); // 100ms measurement period
    
    QueryPerformanceCounter(&qpc_end);
    rdtsc_end = readRDTSC();
    
    uint64_t qpc_delta = qpc_end.QuadPart - qpc_start.QuadPart;
    uint64_t rdtsc_delta = rdtsc_end - rdtsc_start;
    
    // Calculate RDTSC frequency
    uint64_t rdtsc_freq = (rdtsc_delta * qpc_freq.QuadPart) / qpc_delta;
    
    return rdtsc_freq;
}

uint64_t readRDTSC()
{
    return __rdtsc();
}

uint64_t fileTimeToUnixNanos(const FILETIME& ft)
{
    uint64_t windows_time = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    uint64_t unix_time_100ns = windows_time - WINDOWS_TO_UNIX_EPOCH_OFFSET;
    return unix_time_100ns * 100;
}

uint64_t getHighPrecisionSystemTime()
{
    FILETIME ft;
    if (isSystemTimePreciseAvailable()) {
        g_GetSystemTimePreciseAsFileTime(&ft);
    } else {
        GetSystemTimeAsFileTime(&ft);
    }
    return fileTimeToUnixNanos(ft);
}

} // namespace WindowsTimestampUtils

bool WindowsCrossTimestamp::initializeIntelHAL(const char* device_name)
{
    if (!device_name) {
        return false;
    }
    
    // Clean up any existing Intel HAL context
    if (m_intel_hal_context) {
        IntelVendorExtensions::IntelHALContext* hal_ctx = 
            static_cast<IntelVendorExtensions::IntelHALContext*>(m_intel_hal_context);
        IntelVendorExtensions::CleanupIntelHAL(hal_ctx);
        delete hal_ctx;
        m_intel_hal_context = nullptr;
    }
    
    // Check if Intel HAL is supported for this device
    if (!IntelVendorExtensions::IsIntelHALSupported(device_name, 0)) {
        GPTP_LOG_DEBUG("Intel HAL not supported for device: %s", device_name);
        return false;
    }
    
    // Create Intel HAL context
    IntelVendorExtensions::IntelHALContext* hal_ctx = 
        new IntelVendorExtensions::IntelHALContext();
    
    // Initialize Intel HAL
    if (!IntelVendorExtensions::InitializeIntelHAL(device_name, 0, hal_ctx)) {
        GPTP_LOG_ERROR("Failed to initialize Intel HAL for device: %s", device_name);
        delete hal_ctx;
        return false;
    }
    
    m_intel_hal_context = hal_ctx;
    m_intel_hal_available = true;
    
    GPTP_LOG_STATUS("Intel HAL initialized successfully for device: %s", device_name);
    
    // Get HAL status for logging
    char status_message[256];
    if (IntelVendorExtensions::GetHALStatus(hal_ctx, status_message, sizeof(status_message))) {
        GPTP_LOG_INFO("Intel HAL Status: %s", status_message);
    }
    
    return true;
}

bool WindowsCrossTimestamp::isIntelHALAvailable() const
{
    return m_intel_hal_available && m_intel_hal_context != nullptr;
}

bool WindowsCrossTimestamp::getCrossTimestamp_IntelHAL(Timestamp* system_time, Timestamp* device_time)
{
    if (!isIntelHALAvailable() || !system_time || !device_time) {
        return false;
    }
    
    IntelVendorExtensions::IntelHALContext* hal_ctx = 
        static_cast<IntelVendorExtensions::IntelHALContext*>(m_intel_hal_context);
    
    LARGE_INTEGER qpc_before, qpc_after;
    uint64_t hw_timestamp_ns = 0;
    FILETIME ft;
    
    // Get synchronized timestamps as close together as possible
    QueryPerformanceCounter(&qpc_before);
    
    // Get Intel HAL hardware timestamp
    bool hal_success = IntelVendorExtensions::GetHardwareTimestamp(hal_ctx, &hw_timestamp_ns);
    
    // Get system time
    if (WindowsTimestampUtils::isSystemTimePreciseAvailable()) {
        g_GetSystemTimePreciseAsFileTime(&ft);
    } else {
        GetSystemTimeAsFileTime(&ft);
    }
    
    QueryPerformanceCounter(&qpc_after);
    
    if (!hal_success) {
        GPTP_LOG_DEBUG("Intel HAL timestamp failed");
        return false;
    }
    
    // Calculate timing synchronization quality
    uint64_t qpc_delta = qpc_after.QuadPart - qpc_before.QuadPart;
    double sync_window_ns = (static_cast<double>(qpc_delta) * 1000000000.0) / m_qpc_frequency.QuadPart;
    
    // Convert system time to timestamp
    *system_time = fileTimeToTimestamp(ft);
    
    // Convert Intel HAL timestamp to gPTP timestamp format
    device_time->seconds_ms = static_cast<uint32_t>(hw_timestamp_ns / 1000000000ULL) << 16;
    device_time->seconds_ls = static_cast<uint32_t>(hw_timestamp_ns / 1000000000ULL) & 0xFFFF;
    device_time->nanoseconds = static_cast<uint32_t>(hw_timestamp_ns % 1000000000ULL);
    
    // Update quality metrics based on synchronization window
    if (sync_window_ns < 1000.0) {         // < 1µs
        m_quality = 95;
        m_estimated_error_ns = static_cast<uint64_t>(sync_window_ns);
    } else if (sync_window_ns < 10000.0) {  // < 10µs
        m_quality = 85;
        m_estimated_error_ns = static_cast<uint64_t>(sync_window_ns);
    } else {                               // >= 10µs
        m_quality = 70;
        m_estimated_error_ns = static_cast<uint64_t>(sync_window_ns);
    }
    
    GPTP_LOG_VERBOSE("Intel HAL timestamp: %llu ns, sync window: %.1f ns, quality: %u", 
                     hw_timestamp_ns, sync_window_ns, m_quality);
    
    return true;
}
