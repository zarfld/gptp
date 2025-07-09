/******************************************************************************

  Copyright (c) 2009-2012, Intel Corporation
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

#ifndef WINDOWS_HAL_VENDOR_INTEL_HPP
#define WINDOWS_HAL_VENDOR_INTEL_HPP

/**@file*/

#include <cstdint>

/**
 * @brief Intel vendor-specific hardware detection utilities
 * 
 * This module contains Intel-specific knowledge for:
 * - MAC OUI prefix detection
 * - Device model identification
 * - Clock rate specifications
 * - Hardware timestamping capabilities
 */

/**
 * @brief Intel OUI (Organizationally Unique Identifier) prefixes
 */
struct IntelOUIPrefix {
    uint8_t prefix[3];
    const char* description;
};

/**
 * @brief Intel device specifications
 */
struct IntelDeviceSpec {
    const char* model_pattern;      // String pattern to match in device description
    uint64_t clock_rate;           // Hardware clock rate in Hz
    bool hw_timestamp_supported;   // Hardware timestamping capability
    const char* notes;             // Additional notes about the device
};

/**
 * @brief Intel device information structure (enhanced for I225 support)
 */
struct IntelDeviceInfo {
    uint64_t clock_rate;
    bool hw_timestamp_supported;
    bool registry_configured;
    const char* model_name;
    const char* description;
    
    // I225-specific fields
    bool is_i225_family;
    uint8_t i225_stepping;        // Hardware stepping (A0, A1, A2, etc.)
    bool requires_ipg_mitigation; // True if stepping has IPG timing issues
    bool supports_2_5gbe;         // True if 2.5GbE is supported
};

/**
 * @brief I225 stepping information
 */
struct I225SteppingInfo {
    uint8_t stepping_id;
    const char* stepping_name;
    bool has_ipg_issue;
    bool requires_speed_limit;
    const char* mitigation_notes;
};

/**
 * @brief Check if MAC address belongs to Intel
 * @param mac_bytes MAC address bytes (must be at least 6 bytes)
 * @return true if Intel OUI detected, false otherwise
 */
bool isIntelDevice(const uint8_t* mac_bytes);

/**
 * @brief Get Intel device specifications by description
 * @param device_desc Device description string
 * @param clock_rate Output parameter for clock rate (Hz)
 * @param hw_timestamp_support Output parameter for timestamp support
 * @return true if Intel device recognized, false otherwise
 */
bool getIntelDeviceSpecs(const char* device_desc, 
                        uint64_t* clock_rate, 
                        bool* hw_timestamp_support);

/**
 * @brief Get Intel device clock rate by description
 * @param device_desc Device description string
 * @return Clock rate in Hz, or 0 if not recognized
 */
uint64_t getIntelClockRate(const char* device_desc);

/**
 * @brief Check if Intel device supports hardware timestamping
 * @param device_desc Device description string
 * @return true if hardware timestamping supported, false otherwise
 */
bool isIntelTimestampSupported(const char* device_desc);

/**
 * @brief Get all known Intel OUI prefixes
 * @return Array of Intel OUI prefixes (terminated by {0,0,0})
 */
const IntelOUIPrefix* getIntelOUIPrefixes();

/**
 * @brief Get all known Intel device specifications
 * @return Array of Intel device specs (terminated by NULL model_pattern)
 */
const IntelDeviceSpec* getIntelDeviceSpecs();

/**
 * @brief Get comprehensive Intel device information (enhanced for I225 support)
 * @param device_desc Device description string
 * @param mac_bytes MAC address bytes (optional)
 * @param device_info Output structure for device information
 * @param pci_device_id PCI device ID (for I225 stepping detection)
 * @param pci_revision PCI revision ID (for I225 stepping detection)
 * @return true if device is recognized Intel device
 */
bool getIntelDeviceInfo(const char* device_desc, const uint8_t* mac_bytes, 
                       IntelDeviceInfo* device_info,
                       uint16_t pci_device_id = 0, uint8_t pci_revision = 0);

/**
 * @brief Detect I225 stepping and determine mitigation requirements
 * @param device_desc Device description string
 * @param pci_device_id PCI device ID (0x15F2, 0x15F3, etc.)
 * @param pci_revision PCI revision ID (contains stepping info)
 * @return Pointer to stepping info structure, or NULL if not I225
 */
const I225SteppingInfo* detectI225Stepping(const char* device_desc, 
                                          uint16_t pci_device_id,
                                          uint8_t pci_revision);

/**
 * @brief Apply I225-specific mitigations based on stepping
 * @param device_desc Device description
 * @param stepping_info Stepping information
 * @return true if mitigation was applied successfully
 */
bool applyI225Mitigation(const char* device_desc, const I225SteppingInfo* stepping_info);

#endif // WINDOWS_HAL_VENDOR_INTEL_HPP
