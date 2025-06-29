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

#include "windows_hal_vendor_intel.hpp"
#include <string.h>

/**
 * @brief Intel OUI prefixes for MAC address identification
 * 
 * Intel has many registered OUI prefixes. This list includes the most
 * commonly used ones for Ethernet controllers with timestamping support.
 */
static const IntelOUIPrefix intel_oui_prefixes[] = {
    // Common Intel Ethernet controller OUIs
    { {0x00, 0x1C, 0xC8}, "Intel I217/I218/I219 series" },
    { {0x00, 0x15, 0x17}, "Intel newer controllers" },
    { {0x00, 0x1B, 0x21}, "Intel I350 series" },
    { {0x00, 0x0E, 0x0C}, "Intel generic" },
    { {0x00, 0x02, 0xB3}, "Intel 82540/82541 series" },
    { {0x00, 0x03, 0x47}, "Intel 82546/82547 series" },
    { {0x00, 0x07, 0xE9}, "Intel 82571/82572 series" },
    { {0x00, 0x13, 0xCE}, "Intel 82567/82566 series" },
    { {0x00, 0x19, 0x99}, "Intel 82579 series" },
    { {0x00, 0x1F, 0x3C}, "Intel 82598/82599 series" },
    { {0x00, 0x0D, 0x3A}, "Intel X520/X540 series" },
    { {0x90, 0xE2, 0xBA}, "Intel X710/XL710 series" },
    // Terminator
    { {0x00, 0x00, 0x00}, NULL }
};

/**
 * @brief Intel device specifications with precise timing information
 * 
 * Based on Intel datasheets and empirical testing. Clock rates are
 * critical for accurate PTP operation.
 */
static const IntelDeviceSpec intel_device_specs[] = {
    // Desktop/Mobile controllers
    { "I217",     1000000000ULL, true,  "Lynx Point, desktop/mobile" },
    { "I218",     1000000000ULL, true,  "Lynx Point LP, mobile" },
    { "I219",     1008000000ULL, true,  "Sunrise Point, desktop/mobile (corrected frequency)" },
    
    // Server/Professional controllers  
    { "I210",     1000000000ULL, true,  "Springville, embedded/server" },
    { "I211",     1000000000ULL, true,  "Springville AT, automotive" },
    { "I350",     1000000000ULL, true,  "Powerville, server" },
    
    // 10 Gigabit controllers
    { "82599",    1000000000ULL, true,  "Niantic, 10GbE server" },
    { "X520",     1000000000ULL, true,  "Niantic-based, 10GbE" },
    { "X540",     1000000000ULL, true,  "Patsburg, 10GbE integrated" },
    { "X550",     1000000000ULL, true,  "Sageville, 10GbE" },
    { "X710",     1000000000ULL, true,  "Fortville, 10/40GbE" },
    { "XL710",    1000000000ULL, true,  "Fortville, 10/40GbE" },
    { "XXV710",   1000000000ULL, true,  "Fortville, 25GbE" },
    
    // Legacy controllers (limited or no timestamping)
    { "82540",    0ULL,           false, "Copper Gigabit, legacy" },
    { "82541",    0ULL,           false, "Copper Gigabit, legacy" },
    { "82545",    0ULL,           false, "Copper Gigabit, legacy" },
    { "82546",    0ULL,           false, "Dual Copper Gigabit, legacy" },
    { "82566",    0ULL,           false, "ICH8 integrated, legacy" },
    { "82567",    0ULL,           false, "ICH9 integrated, legacy" },
    { "82571",    0ULL,           false, "Quad Copper Gigabit, legacy" },
    { "82572",    0ULL,           false, "Dual Copper Gigabit, legacy" },
    { "82573",    0ULL,           false, "Single Copper Gigabit, legacy" },
    { "82574",    1000000000ULL, false, "Gigabit CT Desktop (limited timestamp)" },
    { "82575",    0ULL,           false, "Quad Copper Gigabit, legacy" },
    { "82576",    1000000000ULL, false, "Quad Copper Gigabit (limited timestamp)" },
    { "82577",    0ULL,           false, "ICH10 integrated, legacy" },
    { "82578",    0ULL,           false, "ICH10 integrated, legacy" },
    { "82579",    1000000000ULL, false, "PCH integrated (limited timestamp)" },
    
    // Terminator
    { NULL, 0ULL, false, NULL }
};

bool isIntelDevice(const uint8_t* mac_bytes) {
    if (!mac_bytes) {
        return false;
    }
    
    const IntelOUIPrefix* oui = intel_oui_prefixes;
    while (oui->description != NULL) {
        if (mac_bytes[0] == oui->prefix[0] && 
            mac_bytes[1] == oui->prefix[1] && 
            mac_bytes[2] == oui->prefix[2]) {
            return true;
        }
        oui++;
    }
    
    return false;
}

bool getIntelDeviceSpecs(const char* device_desc, 
                        uint64_t* clock_rate, 
                        bool* hw_timestamp_support) {
    if (!device_desc) {
        return false;
    }
    
    const IntelDeviceSpec* spec = intel_device_specs;
    while (spec->model_pattern != NULL) {
        if (strstr(device_desc, spec->model_pattern) != NULL) {
            if (clock_rate) {
                *clock_rate = spec->clock_rate;
            }
            if (hw_timestamp_support) {
                *hw_timestamp_support = spec->hw_timestamp_supported;
            }
            return true;
        }
        spec++;
    }
    
    return false;
}

uint64_t getIntelClockRate(const char* device_desc) {
    uint64_t clock_rate = 0;
    getIntelDeviceSpecs(device_desc, &clock_rate, NULL);
    return clock_rate;
}

bool isIntelTimestampSupported(const char* device_desc) {
    bool hw_timestamp_support = false;
    getIntelDeviceSpecs(device_desc, NULL, &hw_timestamp_support);
    return hw_timestamp_support;
}

const IntelOUIPrefix* getIntelOUIPrefixes() {
    return intel_oui_prefixes;
}

const IntelDeviceSpec* getIntelDeviceSpecs() {
    return intel_device_specs;
}
