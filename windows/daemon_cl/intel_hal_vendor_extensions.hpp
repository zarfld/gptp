/******************************************************************************

  Copyright (c) 2024-2025, Intel Corporation
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

#ifndef INTEL_HAL_VENDOR_EXTENSIONS_HPP
#define INTEL_HAL_VENDOR_EXTENSIONS_HPP

/**@file*/

#include <cstdint>
#include <string>
#include <cstring>

// Prevent WinSock header conflicts by only including what we need
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Don't include windows.h here to avoid conflicts
#include <cstdint>
#include <string>
#include <cstring>

// Forward declarations for Intel HAL integration
#ifdef OPENAVNU_BUILD_INTEL_HAL
extern "C" {
    #include "../../../../intel-ethernet-hal/include/intel_ethernet_hal.h"
}
#endif

/**
 * @brief Intel HAL vendor extensions for gPTP integration
 * 
 * This namespace provides integration between the existing gPTP Intel vendor
 * detection and the new Intel Ethernet HAL for enhanced timestamping capabilities.
 * 
 * The extensions work as follows:
 * 1. Existing vendor detection identifies Intel devices
 * 2. Intel HAL vendor extensions provide enhanced timestamp capabilities
 * 3. Cross-timestamping uses Intel HAL when available, falls back to standard methods
 */
namespace IntelVendorExtensions {

/**
 * @brief Intel HAL device context wrapper
 * 
 * Wraps the Intel HAL device context for use within gPTP infrastructure
 */
struct IntelHALContext {
    bool initialized;
    bool hal_available;
    
#ifdef OPENAVNU_BUILD_INTEL_HAL
    intel_device_t* device_ctx;  // Changed from intel_device_context_t*
    intel_device_info_t device_info;
#else
    void* device_ctx;      // Placeholder when HAL not built
    char device_info[256]; // Placeholder when HAL not built
#endif
    
    // Capabilities detected from Intel HAL
    bool hw_timestamping_available;
    bool mdio_access_available;
    uint64_t clock_rate_hz;
    
    // Performance metrics
    uint32_t timestamp_latency_us;
    uint32_t error_count;
    
    IntelHALContext() : 
        initialized(false), hal_available(false), device_ctx(nullptr), 
        hw_timestamping_available(false), mdio_access_available(false),
        clock_rate_hz(0), timestamp_latency_us(0), error_count(0) {
        memset(&device_info, 0, sizeof(device_info));
    }
};

/**
 * @brief Check if Intel HAL is available and supports the given device
 * 
 * @param device_name Device name (e.g., "I219-LM", "I225-V")
 * @param device_id PCI device ID (e.g., 0x0DC7 for I219-LM)
 * @return true if Intel HAL supports this device
 */
bool IsIntelHALSupported(const char* device_name, uint16_t device_id = 0);

/**
 * @brief Initialize Intel HAL for the specified device
 * 
 * @param device_name Device name identified by existing vendor detection
 * @param device_id PCI device ID if available
 * @param hal_context Output parameter for HAL context
 * @return true if Intel HAL initialization successful
 */
bool InitializeIntelHAL(const char* device_name, uint16_t device_id, IntelHALContext* hal_context);

/**
 * @brief Get hardware timestamp using Intel HAL
 * 
 * @param hal_context Intel HAL context from InitializeIntelHAL
 * @param timestamp_ns Output parameter for timestamp in nanoseconds
 * @return true if timestamp retrieval successful
 */
bool GetHardwareTimestamp(IntelHALContext* hal_context, uint64_t* timestamp_ns);

/**
 * @brief Set frequency adjustment using Intel HAL
 * 
 * @param hal_context Intel HAL context
 * @param freq_offset_ppb Frequency offset in parts per billion
 * @return true if frequency adjustment successful
 */
bool SetFrequencyAdjustment(IntelHALContext* hal_context, int32_t freq_offset_ppb);

/**
 * @brief Get enhanced device capabilities using Intel HAL
 * 
 * @param hal_context Intel HAL context
 * @param capabilities Output parameter for enhanced capabilities
 * @return true if capability detection successful
 */
bool GetEnhancedCapabilities(IntelHALContext* hal_context, uint32_t* capabilities);

/**
 * @brief Clean up Intel HAL resources
 * 
 * @param hal_context Intel HAL context to clean up
 */
void CleanupIntelHAL(IntelHALContext* hal_context);

/**
 * @brief Get Intel HAL status and diagnostics
 * 
 * @param hal_context Intel HAL context
 * @param status_message Output buffer for status message
 * @param buffer_size Size of status message buffer
 * @return true if status retrieval successful
 */
bool GetHALStatus(IntelHALContext* hal_context, char* status_message, size_t buffer_size);

/**
 * @brief Enhanced Intel device detection using HAL
 * 
 * Extends the existing isIntelDevice() function with Intel HAL capabilities
 * 
 * @param device_desc Device description string
 * @param enhanced_info Output parameter for enhanced device information
 * @return true if device is Intel and HAL provides enhanced capabilities
 */
bool IsIntelDeviceWithHAL(const char* device_desc, IntelHALContext* enhanced_info);

/**
 * @brief Enable or disable IEEE 1588 timestamping using Intel HAL
 * 
 * @param hal_context Intel HAL context from InitializeIntelHAL
 * @param enable True to enable timestamping, false to disable
 * @return true if timestamping enable/disable successful
 */
bool EnableTimestamping(IntelHALContext* hal_context, bool enable);

} // namespace IntelVendorExtensions

#endif // INTEL_HAL_VENDOR_EXTENSIONS_HPP
