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

// Prevent WinSock header conflicts
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "intel_hal_vendor_extensions.hpp"
#include "../../common/gptp_log.hpp"
#include "windows_hal_vendor_intel.hpp"  // For getIntelDeviceSpecs
#include <cstring>
#include <chrono>

namespace IntelVendorExtensions {

#ifdef OPENAVNU_BUILD_INTEL_HAL

bool IsIntelHALSupported(const char* device_name, uint16_t device_id) {
    if (!device_name) {
        return false;
    }
    
    // Initialize Intel HAL if not already done
    static bool hal_initialized = false;
    if (!hal_initialized) {
        intel_hal_result_t result = intel_hal_init();
        if (result != INTEL_HAL_SUCCESS) {
            GPTP_LOG_WARNING("Intel HAL initialization failed: %d", result);
            return false;
        }
        hal_initialized = true;
    }
    
    // Check if this device type is supported by Intel HAL
    // Map gPTP device names to Intel HAL device IDs
    uint16_t hal_device_id = device_id;
    
    if (hal_device_id == 0) {
        // Map device name to device ID
        if (strstr(device_name, "I219-LM") != nullptr) {
            hal_device_id = 0x0DC7;  // Our test hardware
        } else if (strstr(device_name, "I219") != nullptr) {
            hal_device_id = 0x15B7;  // Common I219 variant
        } else if (strstr(device_name, "I225") != nullptr) {
            hal_device_id = 0x15F3;  // I225-V
        } else if (strstr(device_name, "I226") != nullptr) {
            hal_device_id = 0x125B;  // I226-LM
        } else if (strstr(device_name, "I210") != nullptr) {
            hal_device_id = 0x1533;  // I210
        } else {
            GPTP_LOG_DEBUG("Unknown Intel device name: %s", device_name);
            return false;
        }
    }
    
    // Check if Intel HAL can find this device
    intel_device_info_t devices[16];
    uint32_t device_count = 16;  // Use uint32_t instead of size_t
    
    intel_hal_result_t result = intel_hal_enumerate_devices(devices, &device_count);
    if (result != INTEL_HAL_SUCCESS) {
        GPTP_LOG_DEBUG("Intel HAL device enumeration failed: %d", result);
        return false;
    }
    
    // Look for matching device
    for (uint32_t i = 0; i < device_count; i++) {
        if (devices[i].device_id == hal_device_id) {
            GPTP_LOG_INFO("Intel HAL supports device %s (0x%04X)", device_name, hal_device_id);
            return true;
        }
    }
    
    GPTP_LOG_DEBUG("Intel HAL does not support device %s (0x%04X)", device_name, hal_device_id);
    return false;
}

bool InitializeIntelHAL(const char* device_name, uint16_t device_id, IntelHALContext* hal_context) {
    if (!device_name || !hal_context) {
        return false;
    }
    
    // Clean up any existing context
    CleanupIntelHAL(hal_context);
    
    // Initialize context
    memset(hal_context, 0, sizeof(IntelHALContext));
    
    // Map device name to device ID if needed
    uint16_t hal_device_id = device_id;
    if (hal_device_id == 0) {
        if (strstr(device_name, "I219-LM") != nullptr) {
            hal_device_id = 0x0DC7;
        } else if (strstr(device_name, "I219") != nullptr) {
            hal_device_id = 0x15B7;
        } else if (strstr(device_name, "I225") != nullptr) {
            hal_device_id = 0x15F3;
        } else if (strstr(device_name, "I226") != nullptr) {
            hal_device_id = 0x125B;
        } else if (strstr(device_name, "I210") != nullptr) {
            hal_device_id = 0x1533;
        }
    }
    
    // Enumerate devices to find our target
    intel_device_info_t devices[16];
    uint32_t device_count = 16;  // Use uint32_t instead of size_t
    
    intel_hal_result_t result = intel_hal_enumerate_devices(devices, &device_count);
    if (result != INTEL_HAL_SUCCESS) {
        GPTP_LOG_ERROR("Intel HAL device enumeration failed: %d", result);
        return false;
    }
    
    // Find target device
    intel_device_info_t* target_device = nullptr;
    for (uint32_t i = 0; i < device_count; i++) {
        if (devices[i].device_id == hal_device_id) {
            target_device = &devices[i];
            hal_context->device_info = devices[i];
            break;
        }
    }
    
    if (!target_device) {
        GPTP_LOG_ERROR("Intel HAL: Device %s (0x%04X) not found", device_name, hal_device_id);
        return false;
    }
    
    // Open the device using device name/ID
    char device_id_str[32];
    snprintf(device_id_str, sizeof(device_id_str), "0x%04X", hal_device_id);
    
    intel_device_t* device = nullptr;
    result = intel_hal_open_device(device_id_str, &device);
    if (result != INTEL_HAL_SUCCESS) {
        GPTP_LOG_ERROR("Intel HAL: Failed to open device %s: %d", device_name, result);
        return false;
    }
    
    // Store device handle in context
    hal_context->device_ctx = device;
    
    // Get device capabilities
    uint32_t capabilities = 0;
    result = intel_hal_get_capabilities(device, &capabilities);
    if (result == INTEL_HAL_SUCCESS) {
        hal_context->hw_timestamping_available = (capabilities & INTEL_CAP_BASIC_1588) != 0;
        hal_context->mdio_access_available = (capabilities & INTEL_CAP_MDIO) != 0;
    }
    
    // Set clock rate based on device family
    if (strstr(device_name, "I219") != nullptr) {
        hal_context->clock_rate_hz = 1008000000ULL;  // I219 corrected frequency
    } else {
        hal_context->clock_rate_hz = 1000000000ULL;  // Standard 1GHz
    }
    
    hal_context->initialized = true;
    hal_context->hal_available = true;
    
    GPTP_LOG_STATUS("Intel HAL initialized for device %s (0x%04X)", device_name, hal_device_id);
    GPTP_LOG_STATUS("  - HW Timestamping: %s", hal_context->hw_timestamping_available ? "Yes" : "No");
    GPTP_LOG_STATUS("  - MDIO Access: %s", hal_context->mdio_access_available ? "Yes" : "No");
    GPTP_LOG_STATUS("  - Clock Rate: %I64u Hz", hal_context->clock_rate_hz);
    
    return true;
}

bool GetHardwareTimestamp(IntelHALContext* hal_context, uint64_t* timestamp_ns) {
    if (!hal_context || !hal_context->initialized || !hal_context->device_ctx || !timestamp_ns) {
        return false;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    intel_timestamp_t hw_timestamp;
    intel_hal_result_t result = intel_hal_read_timestamp(hal_context->device_ctx, &hw_timestamp);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    hal_context->timestamp_latency_us = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count());
    
    if (result != INTEL_HAL_SUCCESS) {
        hal_context->error_count++;
        GPTP_LOG_DEBUG("Intel HAL timestamp failed: %d", result);
        return false;
    }
    
    // Convert Intel HAL timestamp to nanoseconds
    *timestamp_ns = (uint64_t)(hw_timestamp.seconds) * 1000000000ULL + hw_timestamp.nanoseconds;
    
    return true;
}

bool SetFrequencyAdjustment(IntelHALContext* hal_context, int32_t freq_offset_ppb) {
    if (!hal_context || !hal_context->initialized || !hal_context->device_ctx) {
        return false;
    }
    
    // Intel HAL doesn't currently implement frequency adjustment
    // This would be implemented in future versions
    GPTP_LOG_DEBUG("Intel HAL frequency adjustment not yet implemented: %d ppb", freq_offset_ppb);
    return false;
}

bool GetEnhancedCapabilities(IntelHALContext* hal_context, uint32_t* capabilities) {
    if (!hal_context || !hal_context->initialized || !hal_context->device_ctx || !capabilities) {
        return false;
    }
    
    intel_hal_result_t result = intel_hal_get_capabilities(hal_context->device_ctx, capabilities);
    return (result == INTEL_HAL_SUCCESS);
}

void CleanupIntelHAL(IntelHALContext* hal_context) {
    if (!hal_context) {
        return;
    }
    
    if (hal_context->device_ctx) {
        intel_hal_close_device(hal_context->device_ctx);
        hal_context->device_ctx = nullptr;
    }
    
    memset(hal_context, 0, sizeof(IntelHALContext));
}

bool GetHALStatus(IntelHALContext* hal_context, char* status_message, size_t buffer_size) {
    if (!hal_context || !status_message || buffer_size == 0) {
        return false;
    }
    
    if (!hal_context->initialized) {
        strncpy_s(status_message, buffer_size, "Intel HAL not initialized", _TRUNCATE);
        return true;
    }
    
    _snprintf_s(status_message, buffer_size, _TRUNCATE,
        "Intel HAL: Device available, HW TS: %s, Latency: %u us, Errors: %u",
        hal_context->hw_timestamping_available ? "Yes" : "No",
        hal_context->timestamp_latency_us,
        hal_context->error_count);
    
    return true;
}

bool IsIntelDeviceWithHAL(const char* device_desc, IntelHALContext* enhanced_info) {
    if (!device_desc) {
        return false;
    }
    
    // First check if it's an Intel device using existing detection
    bool is_intel = false;
    uint64_t clock_rate = 0;
    bool hw_timestamp_support = false;
    
    if (getIntelDeviceSpecs(device_desc, &clock_rate, &hw_timestamp_support)) {
        is_intel = true;
    }
    
    if (!is_intel) {
        return false;
    }
    
    // If Intel HAL is requested, try to initialize it
    if (enhanced_info) {
        if (IsIntelHALSupported(device_desc, 0)) {
            if (InitializeIntelHAL(device_desc, 0, enhanced_info)) {
                return true;
            }
        }
        
        // HAL not available, but still Intel device - fill basic info
        memset(enhanced_info, 0, sizeof(IntelHALContext));
        enhanced_info->hal_available = false;
        enhanced_info->clock_rate_hz = clock_rate;
        enhanced_info->hw_timestamping_available = hw_timestamp_support;
    }
    
    return is_intel;
}

bool EnableTimestamping(IntelHALContext* hal_context, bool enable) {
    if (!hal_context || !hal_context->initialized || !hal_context->device_ctx) {
        GPTP_LOG_ERROR("Intel HAL: Invalid context for timestamping enable/disable");
        return false;
    }
    
    intel_hal_result_t result = intel_hal_enable_timestamping(hal_context->device_ctx, enable);
    if (result != INTEL_HAL_SUCCESS) {
        hal_context->error_count++;
        GPTP_LOG_ERROR("Intel HAL: Failed to %s timestamping: %d", 
                       enable ? "enable" : "disable", result);
        return false;
    }
    
    GPTP_LOG_STATUS("Intel HAL: Timestamping %s successfully", 
                    enable ? "enabled" : "disabled");
    return true;
}

#else // !OPENAVNU_BUILD_INTEL_HAL

// Stub implementations when Intel HAL is not built
bool IsIntelHALSupported(const char* device_name, uint16_t device_id) {
    (void)device_name; (void)device_id;
    return false;
}

bool InitializeIntelHAL(const char* device_name, uint16_t device_id, IntelHALContext* hal_context) {
    (void)device_name; (void)device_id;
    if (hal_context) {
        memset(hal_context, 0, sizeof(IntelHALContext));
        hal_context->hal_available = false;
    }
    return false;
}

bool GetHardwareTimestamp(IntelHALContext* hal_context, uint64_t* timestamp_ns) {
    (void)hal_context; (void)timestamp_ns;
    return false;
}

bool SetFrequencyAdjustment(IntelHALContext* hal_context, int32_t freq_offset_ppb) {
    (void)hal_context; (void)freq_offset_ppb;
    return false;
}

bool GetEnhancedCapabilities(IntelHALContext* hal_context, uint32_t* capabilities) {
    (void)hal_context; (void)capabilities;
    return false;
}

void CleanupIntelHAL(IntelHALContext* hal_context) {
    if (hal_context) {
        memset(hal_context, 0, sizeof(IntelHALContext));
    }
}

bool GetHALStatus(IntelHALContext* hal_context, char* status_message, size_t buffer_size) {
    (void)hal_context;
    if (status_message && buffer_size > 0) {
        strncpy_s(status_message, buffer_size, "Intel HAL not compiled", _TRUNCATE);
    }
    return false;
}

bool IsIntelDeviceWithHAL(const char* device_desc, IntelHALContext* enhanced_info) {
    if (enhanced_info) {
        memset(enhanced_info, 0, sizeof(IntelHALContext));
        enhanced_info->hal_available = false;
    }
    
    // Fall back to existing vendor detection
    uint64_t clock_rate = 0;
    bool hw_timestamp_support = false;
    return getIntelDeviceSpecs(device_desc, &clock_rate, &hw_timestamp_support);
}

#endif // OPENAVNU_BUILD_INTEL_HAL

} // namespace IntelVendorExtensions
