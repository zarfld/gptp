#include "windows_hal_iphlpapi.hpp"
#include "windows_hal.hpp" // For DeviceClockRateMapping definition
#include "windows_hal_vendor_intel.hpp" // For Intel-specific detection

uint64_t getHardwareClockRate_IPHLPAPI(const char* iface_label) {
    if (!iface_label) {
        return 0;
    }

    // ✅ IMPLEMENTING: IPHLPAPI-specific clock rate detection
    // Replaces TODO: "Implement IPHLPAPI-specific clock rate detection"
    //
    // Strategy:
    // 1. Find the network adapter using GetAdaptersAddresses
    // 2. Try GetInterfaceActiveTimestampCapabilities (Windows 10+)
    // 3. Fall back to device mapping table if modern APIs unavailable
    // 4. Use registry-based detection as additional fallback

    PIP_ADAPTER_ADDRESSES pAddresses = NULL;
    PIP_ADAPTER_ADDRESSES pCurrAddresses = NULL;
    ULONG outBufLen = 0;
    DWORD dwRetVal = 0;
    uint64_t detected_rate = 0;

    // Step 1: Get adapter information using GetAdaptersAddresses
    dwRetVal = GetAdaptersAddresses(AF_UNSPEC, 
        GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
        NULL, pAddresses, &outBufLen);

    if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
        pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(outBufLen);
        if (pAddresses == NULL) {
            return 0; // Memory allocation failed
        }
    } else if (dwRetVal != ERROR_SUCCESS) {
        return 0; // Initial call failed
    }

    // Get the actual adapter information
    dwRetVal = GetAdaptersAddresses(AF_UNSPEC,
        GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
        NULL, pAddresses, &outBufLen);

    if (dwRetVal == NO_ERROR) {
        pCurrAddresses = pAddresses;
        while (pCurrAddresses) {
            // Check if this adapter matches our interface description
            // Note: pCurrAddresses->Description is WCHAR*, need to convert for comparison
            if (pCurrAddresses->Description) {
                // Convert wide string to narrow string for comparison
                char desc_narrow[256] = {0};
                WideCharToMultiByte(CP_UTF8, 0, pCurrAddresses->Description, -1, 
                                   desc_narrow, sizeof(desc_narrow), NULL, NULL);
                
                if (strstr(iface_label, desc_narrow) != NULL) {
                    // Found our adapter, try different detection methods
                    
                    // Method 1: Try GetInterfaceActiveTimestampCapabilities (Windows 10 1903+)
#ifdef INTERFACE_TIMESTAMP_CAPABILITIES_VERSION
                    INTERFACE_TIMESTAMP_CAPABILITIES timestampCaps = {0};
                    timestampCaps.Version = INTERFACE_TIMESTAMP_CAPABILITIES_VERSION;
                    
                    DWORD result = GetInterfaceActiveTimestampCapabilities(pCurrAddresses->IfIndex, &timestampCaps);
                    if (result == NO_ERROR && timestampCaps.HardwareClockFrequencyHz != 0) {
                        detected_rate = timestampCaps.HardwareClockFrequencyHz;
                        break; // Success with modern API
                    }
#endif

                    // Method 2: Try Intel-specific detection using vendor module
                    if (detected_rate == 0 && pCurrAddresses->PhysicalAddressLength == 6) {
                        // Check if it's an Intel adapter using the vendor module
                        BYTE* mac = pCurrAddresses->PhysicalAddress;
                        
                        if (isIntelDevice(mac)) {
                            // Use Intel vendor module for device-specific detection
                            detected_rate = getIntelClockRate(desc_narrow);
                        }
                    }
                    
                    break; // Found our adapter, stop searching
                }
            }
            pCurrAddresses = pCurrAddresses->Next;
        }
    }

    if (pAddresses) {
        free(pAddresses);
    }

    // Method 3: Fall back to device mapping table if IPHLPAPI detection failed
    if (detected_rate == 0) {
        for (int i = 0; DeviceClockRateMap[i].device_desc != NULL; i++) {
            if (strstr(iface_label, DeviceClockRateMap[i].device_desc) != NULL) {
                detected_rate = DeviceClockRateMap[i].clock_rate;
                break;
            }
        }
    }

    return detected_rate;
}

bool isHardwareTimestampSupported_IPHLPAPI(const char* iface_label) {
    if (!iface_label) {
        return false;
    }

    // ✅ IMPLEMENTING: IPHLPAPI-specific hardware timestamp support detection
    // Replaces TODO: "Implement IPHLPAPI-specific hardware timestamp support detection"
    //
    // Strategy:
    // 1. Find the network adapter using GetAdaptersAddresses
    // 2. Try GetInterfaceActiveTimestampCapabilities (Windows 10+)
    // 3. Fall back to checking adapter description for known timestamping hardware

    PIP_ADAPTER_ADDRESSES pAddresses = NULL;
    PIP_ADAPTER_ADDRESSES pCurrAddresses = NULL;
    ULONG outBufLen = 0;
    DWORD dwRetVal = 0;
    bool supports_hw_timestamp = false;

    // Step 1: Get adapter information using GetAdaptersAddresses
    dwRetVal = GetAdaptersAddresses(AF_UNSPEC, 
        GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
        NULL, pAddresses, &outBufLen);

    if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
        pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(outBufLen);
        if (pAddresses == NULL) {
            return false; // Memory allocation failed
        }
    } else if (dwRetVal != ERROR_SUCCESS) {
        return false; // Initial call failed
    }

    // Get the actual adapter information
    dwRetVal = GetAdaptersAddresses(AF_UNSPEC,
        GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
        NULL, pAddresses, &outBufLen);

    if (dwRetVal == NO_ERROR) {
        pCurrAddresses = pAddresses;
        while (pCurrAddresses) {
            // Check if this adapter matches our interface description
            if (pCurrAddresses->Description) {
                // Convert wide string to narrow string for comparison
                char desc_narrow[256] = {0};
                WideCharToMultiByte(CP_UTF8, 0, pCurrAddresses->Description, -1, 
                                   desc_narrow, sizeof(desc_narrow), NULL, NULL);
                
                if (strstr(iface_label, desc_narrow) != NULL) {
                    // Found our adapter, try different detection methods
                    
                    // Method 1: Try GetInterfaceActiveTimestampCapabilities (Windows 10 1903+)
#ifdef INTERFACE_TIMESTAMP_CAPABILITIES_VERSION
                    INTERFACE_TIMESTAMP_CAPABILITIES timestampCaps = {0};
                    timestampCaps.Version = INTERFACE_TIMESTAMP_CAPABILITIES_VERSION;
                    
                    DWORD result = GetInterfaceActiveTimestampCapabilities(pCurrAddresses->IfIndex, &timestampCaps);
                    if (result == NO_ERROR) {
                        // Check if hardware timestamping is supported
                        supports_hw_timestamp = (timestampCaps.HardwareTimestampingCapabilities.AllReceive != 0) ||
                                               (timestampCaps.HardwareTimestampingCapabilities.AllTransmit != 0) ||
                                               (timestampCaps.HardwareTimestampingCapabilities.TaggedTransmit != 0);
                        break; // Success with modern API
                    }
#endif

                    // Method 2: Use Intel vendor module for known hardware detection
                    if (!supports_hw_timestamp && pCurrAddresses->PhysicalAddressLength == 6) {
                        // Check if it's Intel hardware with known timestamping support
                        BYTE* mac = pCurrAddresses->PhysicalAddress;
                        
                        if (isIntelDevice(mac)) {
                            // Use Intel vendor module for timestamp capability detection
                            supports_hw_timestamp = isIntelTimestampSupported(desc_narrow);
                        }
                        
                        // Check other vendors that support hardware timestamping
                        if (!supports_hw_timestamp) {
                            // Broadcom NetXtreme-E (some models)
                            if (strstr(desc_narrow, "NetXtreme-E") || 
                                strstr(desc_narrow, "BCM57") || strstr(desc_narrow, "BCM58")) {
                                supports_hw_timestamp = true;
                            }
                            
                            // Mellanox ConnectX series
                            if (strstr(desc_narrow, "ConnectX") || strstr(desc_narrow, "Mellanox")) {
                                supports_hw_timestamp = true;
                            }
                        }
                    }
                    
                    break; // Found our adapter, stop searching
                }
            }
            pCurrAddresses = pCurrAddresses->Next;
        }
    }

    if (pAddresses) {
        free(pAddresses);
    }

    return supports_hw_timestamp;
}
