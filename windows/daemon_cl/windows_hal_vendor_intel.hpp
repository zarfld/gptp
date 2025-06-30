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

#endif // WINDOWS_HAL_VENDOR_INTEL_HPP
