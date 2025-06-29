#include "windows_hal_iphlpapi.hpp"
#include "windows_hal.hpp" // For DeviceClockRateMapping definition
#include <vector>
#include <cstring>

uint64_t getHardwareClockRate_IPHLPAPI(const char* iface_label) {
    if (!iface_label) {
        return 0;
    }

    // First try the device mapping table from windows_hal.hpp
    for (int i = 0; DeviceClockRateMap[i].device_desc != NULL; i++) {
        if (strstr(iface_label, DeviceClockRateMap[i].device_desc) != NULL) {
            return DeviceClockRateMap[i].clock_rate;
        }
    }

    // Attempt to query the interface capabilities using IPHLPAPI
    ULONG bufLen = 0;
    if (GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, nullptr, &bufLen) != ERROR_BUFFER_OVERFLOW) {
        return 0;
    }

    std::vector<unsigned char> buffer(bufLen);
    PIP_ADAPTER_ADDRESSES addrs = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
    if (GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, addrs, &bufLen) != ERROR_SUCCESS) {
        return 0;
    }

    for (PIP_ADAPTER_ADDRESSES a = addrs; a; a = a->Next) {
        if (a->Description && strcmp(a->Description, iface_label) == 0) {
            INTERFACE_TIMESTAMP_CAPABILITIES caps;
            memset(&caps, 0, sizeof(caps));

            if (GetInterfaceActiveTimestampCapabilities(&a->Luid, &caps) == NO_ERROR &&
                caps.HardwareClockFrequencyHz != 0) {
                return caps.HardwareClockFrequencyHz;
            }
            break;
        }
    }

    return 0;
}

bool isHardwareTimestampSupported_IPHLPAPI(const char* iface_label) {
    if (!iface_label) {
        return false;
    }

    ULONG bufLen = 0;
    if (GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, nullptr, &bufLen) != ERROR_BUFFER_OVERFLOW) {
        return false;
    }

    std::vector<unsigned char> buffer(bufLen);
    PIP_ADAPTER_ADDRESSES addrs = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
    if (GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, addrs, &bufLen) != ERROR_SUCCESS) {
        return false;
    }

    for (PIP_ADAPTER_ADDRESSES a = addrs; a; a = a->Next) {
        if (a->Description && strcmp(a->Description, iface_label) == 0) {
            INTERFACE_TIMESTAMP_CAPABILITIES caps;
            memset(&caps, 0, sizeof(caps));

            if (GetInterfaceActiveTimestampCapabilities(&a->Luid, &caps) == NO_ERROR) {
                INTERFACE_TIMESTAMP_CAPABILITIES zeroCaps;
                memset(&zeroCaps, 0, sizeof(zeroCaps));
                return memcmp(&caps, &zeroCaps, sizeof(caps)) != 0;
            }
            break;
        }
    }

    return false;
}
