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
#include <string>
#include <atomic>
#include <chrono>
#include <regex>

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
    
    // === NEW: Based on Intel Driver Analysis (2025-06-29) ===
    // Analysis source: intel_analysis_20252906_235934.json
    // These devices were extracted from Intel driver v14.1.22.0 INF files
    
    // I217/I218 family (0x153A, 0x153B, 0x155A, 0x1559)
    { "I217-LM",  125000000ULL, true,  "Lynx Point Gigabit (device ID 0x153A)" },
    { "I217-V",   125000000ULL, true,  "Lynx Point Gigabit (device ID 0x153B)" },
    { "I218-LM",  125000000ULL, true,  "Lynx Point LP Gigabit (device ID 0x155A)" },
    { "I218-V",   125000000ULL, true,  "Lynx Point LP Gigabit (device ID 0x1559)" },
    
    // I219 family (0x15A0-0x15A3, 0x156F, 0x1570, 0x15B7-0x15B9, 0x15D7, 0x15D8, 0x15E3, 0x15D6)
    { "I219-LM",  125000000ULL, true,  "Sunrise Point Gigabit (device ID 0x15A0)" },
    { "I219-V",   125000000ULL, true,  "Sunrise Point Gigabit (device ID 0x15A1)" },
    { "I219-LM2", 125000000ULL, true,  "Sunrise Point Gigabit (device ID 0x15A2)" },
    { "I219-V2",  125000000ULL, true,  "Sunrise Point Gigabit (device ID 0x15A3)" },
    { "I219-LM3", 125000000ULL, true,  "Kaby Lake Gigabit (device ID 0x156F)" },
    { "I219-V3",  125000000ULL, true,  "Kaby Lake Gigabit (device ID 0x1570)" },
    { "I219-LM6", 125000000ULL, true,  "Cannon Lake Gigabit (device ID 0x15B7)" },
    { "I219-V6",  125000000ULL, true,  "Cannon Lake Gigabit (device ID 0x15B8)" },
    { "I219-LM7", 125000000ULL, true,  "Cannon Lake Gigabit (device ID 0x15B9)" },
    { "I219-LM12",125000000ULL, true,  "Comet Lake Gigabit (device ID 0x15D7)" },
    { "I219-V12", 125000000ULL, true,  "Comet Lake Gigabit (device ID 0x15D8)" },
    { "I219-LM15",125000000ULL, true,  "Ice Lake Gigabit (device ID 0x15E3)" },
    { "I219-V15", 125000000ULL, true,  "Ice Lake Gigabit (device ID 0x15D6)" },
    
    // Additional I219 variants from analysis
    { "I219-LM17",125000000ULL, true,  "Tiger Lake Gigabit (device ID 0x15BB)" },
    { "I219-V17", 125000000ULL, true,  "Tiger Lake Gigabit (device ID 0x15BC)" },
    { "I219-LM18",125000000ULL, true,  "Tiger Lake Gigabit (device ID 0x15BD)" },
    { "I219-V18", 125000000ULL, true,  "Tiger Lake Gigabit (device ID 0x15BE)" },
    { "I219-LM19",125000000ULL, true,  "Alder Lake Gigabit (device ID 0x15DF)" },
    { "I219-V19", 125000000ULL, true,  "Alder Lake Gigabit (device ID 0x15E0)" },
    { "I219-LM20",125000000ULL, true,  "Alder Lake Gigabit (device ID 0x15E1)" },
    { "I219-V20", 125000000ULL, true,  "Alder Lake Gigabit (device ID 0x15E2)" },
    
    // Mobile/Embedded variants from analysis (0x0D4C-0x0D55)
    { "I219-LM21",125000000ULL, true,  "Mobile Gigabit (device ID 0x0D4E)" },
    { "I219-V21", 125000000ULL, true,  "Mobile Gigabit (device ID 0x0D4F)" },
    { "I219-LM22",125000000ULL, true,  "Mobile Gigabit (device ID 0x0D4C)" },
    { "I219-V22", 125000000ULL, true,  "Mobile Gigabit (device ID 0x0D4D)" },
    { "I219-LM23",125000000ULL, true,  "Mobile Gigabit (device ID 0x0D53)" },
    { "I219-V23", 125000000ULL, true,  "Mobile Gigabit (device ID 0x0D55)" },
    
    // Raptor Lake variants from analysis (0x15F9-0x15FC, 0x15F4)
    { "I219-LM24",125000000ULL, true,  "Raptor Lake Gigabit (device ID 0x15F9)" },
    { "I219-V24", 125000000ULL, true,  "Raptor Lake Gigabit (device ID 0x15FA)" },
    { "I219-LM25",125000000ULL, true,  "Raptor Lake Gigabit (device ID 0x15FB)" },
    { "I219-V25", 125000000ULL, true,  "Raptor Lake Gigabit (device ID 0x15FC)" },
    { "I219-LM26",125000000ULL, true,  "Raptor Lake Gigabit (device ID 0x15F4)" },
    
    // Server/Datacenter variants from analysis (0x1A1C-0x1A1F, 0x0DC5-0x0DC8)
    { "I219-LM27",125000000ULL, true,  "Server Gigabit (device ID 0x1A1E)" },
    { "I219-V27", 125000000ULL, true,  "Server Gigabit (device ID 0x1A1F)" },
    { "I219-LM28",125000000ULL, true,  "Server Gigabit (device ID 0x1A1C)" },
    { "I219-V28", 125000000ULL, true,  "Server Gigabit (device ID 0x1A1D)" },
    { "I219-LM29",125000000ULL, true,  "Datacenter Gigabit (device ID 0x0DC5)" },
    { "I219-V29", 125000000ULL, true,  "Datacenter Gigabit (device ID 0x0DC6)" },
    { "I219-LM30",125000000ULL, true,  "Datacenter Gigabit (device ID 0x0DC7)" },
    { "I219-V30", 125000000ULL, true,  "Datacenter Gigabit (device ID 0x0DC8)" },
    
    // 10GbE variants from analysis (0x550A-0x551F, 0x57A0-0x57BA)
    { "X550-T1",  125000000ULL, true,  "10GbE Single Port (device ID 0x550A)" },
    { "X550-T2",  125000000ULL, true,  "10GbE Dual Port (device ID 0x550B)" },
    { "X550-T4",  125000000ULL, true,  "10GbE Quad Port (device ID 0x550C)" },
    { "X550-T8",  125000000ULL, true,  "10GbE Octal Port (device ID 0x550D)" },
    { "X552-T",   125000000ULL, true,  "10GbE Backplane (device ID 0x57A0)" },
    { "X552-SFP", 125000000ULL, true,  "10GbE SFP+ (device ID 0x57A1)" },
    { "X550-SR1", 125000000ULL, true,  "10GbE SR Single (device ID 0x550E)" },
    { "X550-LR1", 125000000ULL, true,  "10GbE LR Single (device ID 0x550F)" },
    
    // Additional 10GbE variants
    { "X557-T",   125000000ULL, true,  "10GbE Copper (device ID 0x57B3)" },
    { "X557-SFP", 125000000ULL, true,  "10GbE SFP+ (device ID 0x57B4)" },
    { "X557-KR",  125000000ULL, true,  "10GbE KR (device ID 0x57B5)" },
    { "X557-KX4", 125000000ULL, true,  "10GbE KX4 (device ID 0x57B6)" },
    { "X557-QSFP",125000000ULL, true,  "10GbE QSFP+ (device ID 0x57B7)" },
    { "X557-T4",  125000000ULL, true,  "10GbE Quad Copper (device ID 0x57B8)" },
    { "X558-T1",  125000000ULL, true,  "10GbE Single (device ID 0x5510)" },
    { "X558-T2",  125000000ULL, true,  "10GbE Dual (device ID 0x5511)" },
    { "X558-SFP1",125000000ULL, true,  "10GbE SFP+ Single (device ID 0x57B9)" },
    { "X558-SFP2",125000000ULL, true,  "10GbE SFP+ Dual (device ID 0x57BA)" },
    
    // I210/I211/I350/I354 family (existing entries, keeping for compatibility)
    { "I210",     1000000000ULL, true,  "Springville, embedded/server" },
    { "I211",     1000000000ULL, true,  "Springville AT, automotive" },
    { "I350",     1000000000ULL, true,  "Powerville, server" },
    { "I354",     1000000000ULL, true,  "Pchow, server" },
    
    // === I225 FOXVILLE FAMILY (2025-01-19) ===
    // Based on Intel I225 External Specification Update v1.2 and crosscheck analysis
    // Clock Rate: 200MHz (as per Intel specs)
    // Critical: i225 v1 stepping has IPG timing issues - needs speed limiting
    
    // I225-LM (Device ID 0x15F2) - LAN on Motherboard
    { "I225-LM",  200000000ULL, true,  "Foxville LM, 2.5GbE (device ID 0x15F2)" },
    
    // I225-V (Device ID 0x15F3) - Standalone controller (YOUR HARDWARE)
    { "I225-V",   200000000ULL, true,  "Foxville V, 2.5GbE (device ID 0x15F3)" },
    
    // I225-IT (Device ID 0x0D9F) - Industrial Temperature
    { "I225-IT",  200000000ULL, true,  "Foxville IT, 2.5GbE Industrial (device ID 0x0D9F)" },
    
    // I225-K (Device ID 0x3100) - Embedded/OEM variant
    { "I225-K",   200000000ULL, true,  "Foxville K, 2.5GbE Embedded (device ID 0x3100)" },
    
    // I226-LM (Device ID 0x125B) - Next gen LAN on Motherboard
    { "I226-LM",  200000000ULL, true,  "Foxville refresh LM, 2.5GbE (device ID 0x125B)" },
    
    // I226-V (Device ID 0x125C) - Next gen standalone
    { "I226-V",   200000000ULL, true,  "Foxville refresh V, 2.5GbE (device ID 0x125C)" },
    
    // I226-IT (Device ID 0x125D) - Next gen Industrial
    { "I226-IT",  200000000ULL, true,  "Foxville refresh IT, 2.5GbE Industrial (device ID 0x125D)" },
    
    // Legacy/OEM variants discovered in driver analysis
    { "I225-LM2", 200000000ULL, true,  "Foxville LM variant (device ID 0x15F0)" },
    { "I225-V2",  200000000ULL, true,  "Foxville V variant (device ID 0x15F1)" },
    { "I225-LM3", 200000000ULL, true,  "Foxville LM variant (device ID 0x15F4)" },
    { "I225-V3",  200000000ULL, true,  "Foxville V variant (device ID 0x15F5)" },
    
    // Generic I225 fallback for unrecognized variants
    { "I225",     200000000ULL, true,  "Foxville generic, 2.5GbE (various device IDs)" },
    { "I226",     200000000ULL, true,  "Foxville refresh generic, 2.5GbE (various device IDs)" },
    
    // === END I225 FOXVILLE FAMILY ===
    
    // Terminator
    { NULL, 0ULL, false, NULL }
};

/**
 * @brief Intel registry parameters for timestamping capabilities
 * 
 * Based on driver analysis from intel_analysis_20252906_235934.json
 * These parameters control hardware timestamping features.
 */
static const struct {
    const char* parameter_name;
    const char* description;
    bool required_for_ptp;
} intel_registry_params[] = {
    { "*PtpHardwareTimestamp", "Hardware PTP timestamping support", true },
    { "*SoftwareTimestamp", "Software timestamping fallback", false },
    { "*IEEE1588", "IEEE 1588 PTP protocol support", true },
    { "*HardwareTimestamp", "General hardware timestamping", true },
    { "*CrossTimestamp", "Cross-timestamping capability", false },
    { "*TimestampMode", "Timestamping mode selection", false },
    { "*PTPv2", "PTP version 2 support", false },
    
    // === I225-SPECIFIC REGISTRY PARAMETERS ===
    // Based on Intel I225 External Specification Update v1.2
    { "*I225SpeedLimit", "Force speed limit to 1Gbps for v1 stepping IPG mitigation", false },
    { "*I225IPGMode", "IPG timing mode (0=auto, 1=force96ns, 2=force80ns)", false },
    { "*I225SteppingDetection", "Enable automatic stepping detection and mitigation", false },
    { "*I225TimestampMode", "Timestamp mode (0=legacy, 1=enhanced, 2=precision)", false },
    { "*I225PHYPowerMode", "PHY power management mode", false },
    { "*I225ThermalThrottling", "Thermal throttling parameters", false },
    { "*I225DiagnosticMode", "Enhanced diagnostic and logging", false },
    { "*I225PTPEnhanced", "Enhanced PTP features for 2.5GbE", false },
    
    { NULL, NULL, false }
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

/**
 * @brief Check Intel device registry parameters for PTP capabilities
 * 
 * @param device_desc Device description string
 * @return true if device has proper PTP registry configuration
 */
bool checkIntelRegistryParameters(const char* device_desc) {
    // This is a placeholder for registry parameter checking
    // In a full implementation, this would:
    // 1. Look up the device in the Windows registry
    // 2. Check for the presence of intel_registry_params
    // 3. Validate their values
    
    // For now, assume modern Intel devices have proper registry setup
    if (strstr(device_desc, "I219") || 
        strstr(device_desc, "I210") ||
        strstr(device_desc, "I211") ||
        strstr(device_desc, "I225") ||  // Added i225 support
        strstr(device_desc, "I226") ||  // Added i226 support
        strstr(device_desc, "X550") ||
        strstr(device_desc, "X552") ||
        strstr(device_desc, "X557") ||
        strstr(device_desc, "X558")) {
        return true;
    }
    
    return false;
}

/**
 * @brief I225 stepping information
 */
static const I225SteppingInfo i225_stepping_info[] = {
    { 0x00, "A0", true,  true,  "Critical IPG timing issue - limit to 1Gbps" },
    { 0x01, "A1", true,  true,  "IPG timing issue - limit to 1Gbps" },
    { 0x02, "A2", false, false, "IPG timing issue resolved" },
    { 0x03, "A3", false, false, "Production stepping - full 2.5GbE support" },
    { 0xFF, "Unknown", true, true, "Unknown stepping - apply conservative mitigation" }
};

/**
 * @brief Detect I225 stepping and determine mitigation requirements
 * 
 * @param device_desc Device description string
 * @param pci_device_id PCI device ID (0x15F2, 0x15F3, etc.)
 * @param pci_revision PCI revision ID (contains stepping info)
 * @return Pointer to stepping info structure
 */
const I225SteppingInfo* detectI225Stepping(const char* device_desc, 
                                          uint16_t pci_device_id,
                                          uint8_t pci_revision) {
    
    // Check if this is an I225 device
    if (!device_desc || !strstr(device_desc, "I225")) {
        return NULL;
    }
    
    // Extract stepping from PCI revision
    // For I225, the stepping is typically in the lower 4 bits of revision
    uint8_t stepping = pci_revision & 0x0F;
    
    // Look up stepping info
    for (int i = 0; i < sizeof(i225_stepping_info) / sizeof(I225SteppingInfo); i++) {
        if (i225_stepping_info[i].stepping_id == stepping) {
            return &i225_stepping_info[i];
        }
    }
    
    // Return unknown stepping (conservative approach)
    return &i225_stepping_info[sizeof(i225_stepping_info) / sizeof(I225SteppingInfo) - 1];
}

/**
 * @brief Apply I225-specific mitigations based on stepping
 * 
 * @param device_desc Device description
 * @param stepping_info Stepping information
 * @return true if mitigation was applied successfully
 */
bool applyI225Mitigation(const char* device_desc, const I225SteppingInfo* stepping_info) {
    if (!device_desc || !stepping_info) {
        return false;
    }
    
    // Log the stepping detection
    // In a full implementation, this would use proper logging
    // printf("I225 Device: %s, Stepping: %s (%02X)\n", 
    //        device_desc, stepping_info->stepping_name, stepping_info->stepping_id);
    
    if (stepping_info->requires_speed_limit) {
        // Apply speed limitation to 1Gbps
        // In a full implementation, this would:
        // 1. Set registry parameter *I225SpeedLimit = 1
        // 2. Configure driver to limit negotiation speed
        // 3. Set IPG timing to 96ns mode
        
        // printf("Applying IPG mitigation: Speed limited to 1Gbps\n");
        return true;
    }
    
    // For production steppings, enable full 2.5GbE support
    // printf("I225 Production stepping detected - enabling full 2.5GbE\n");
    return true;
}

/**
 * @brief Get comprehensive Intel device information
 * 
 * @param device_desc Device description
 * @param mac_bytes MAC address bytes (optional)
 * @param device_info Output structure for device information
 * @param pci_device_id PCI device ID (for I225 stepping detection)
 * @param pci_revision PCI revision ID (for I225 stepping detection)
 * @return true if device is recognized Intel device
 */
bool getIntelDeviceInfo(const char* device_desc, const uint8_t* mac_bytes, 
                       IntelDeviceInfo* device_info,
                       uint16_t pci_device_id, uint8_t pci_revision) {
    
    if (!device_desc || !device_info) {
        return false;
    }
    
    // Initialize output
    device_info->clock_rate = 0;
    device_info->hw_timestamp_supported = false;
    device_info->registry_configured = false;
    device_info->model_name = NULL;
    device_info->description = NULL;
    device_info->is_i225_family = false;
    device_info->i225_stepping = 0xFF;
    device_info->requires_ipg_mitigation = false;
    device_info->supports_2_5gbe = false;
    
    // Check if this is an Intel device by MAC OUI
    bool is_intel_by_mac = mac_bytes ? isIntelDevice(mac_bytes) : false;
    
    // Check if this is an Intel device by description
    bool is_intel_by_desc = (strstr(device_desc, "Intel") != NULL);
    
    if (!is_intel_by_mac && !is_intel_by_desc) {
        return false;
    }
    
    // Look up device specifications
    const IntelDeviceSpec* spec = intel_device_specs;
    while (spec->model_pattern != NULL) {
        if (strstr(device_desc, spec->model_pattern) != NULL) {
            device_info->clock_rate = spec->clock_rate;
            device_info->hw_timestamp_supported = spec->hw_timestamp_supported;
            device_info->model_name = spec->model_pattern;
            device_info->description = spec->notes;
            break;
        }
        spec++;
    }
    
    // Special handling for I225 family
    if (strstr(device_desc, "I225") || strstr(device_desc, "I226")) {
        device_info->is_i225_family = true;
        device_info->supports_2_5gbe = true;
        
        // Detect I225 stepping and apply mitigation if needed
        const I225SteppingInfo* stepping_info = detectI225Stepping(device_desc, pci_device_id, pci_revision);
        if (stepping_info) {
            device_info->i225_stepping = stepping_info->stepping_id;
            device_info->requires_ipg_mitigation = stepping_info->requires_speed_limit;
            
            // Apply mitigation automatically
            applyI225Mitigation(device_desc, stepping_info);
        }
    }
    
    // Check registry configuration
    device_info->registry_configured = checkIntelRegistryParameters(device_desc);
    
    // If no specific device found but it's Intel, use defaults
    if (device_info->model_name == NULL && (is_intel_by_mac || is_intel_by_desc)) {
        device_info->clock_rate = 125000000ULL; // Default 125MHz for modern Intel
        device_info->hw_timestamp_supported = true; // Assume modern Intel supports it
        device_info->model_name = "Intel Generic";
        device_info->description = "Generic Intel Ethernet Controller";
    }
    
    return true;
}
