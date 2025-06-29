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

#include "windows_hal_ndis.hpp"
#include "windows_hal.hpp" // For NETWORK_CARD_ID_PREFIX
#include <iphlpapi.h>
#include <vector>
#include <cstring>

static auto open_adapter = [](const char* label, HANDLE& handle) -> bool {
    IP_ADAPTER_INFO info[32];
    DWORD len = sizeof(info);
    if (GetAdaptersInfo(info, &len) != ERROR_SUCCESS) {
        return false;
    }
    for (PIP_ADAPTER_INFO p = info; p; p = p->Next) {
        if (p->Description && strcmp(p->Description, label) == 0) {
            char deviceName[128];
            strncpy(deviceName, NETWORK_CARD_ID_PREFIX, sizeof(deviceName));
            strncat(deviceName, p->AdapterName, sizeof(deviceName) - strlen(deviceName) - 1);
            handle = CreateFile(deviceName,
                               GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_EXISTING, 0, NULL);
            return handle != INVALID_HANDLE_VALUE;
        }
    }
    return false;
};

uint64_t getHardwareClockRate_NDIS(const char* iface_label) {
    if (!iface_label) {
        return 0;
    }

#if defined(NDIS_TIMESTAMP_CAPABILITIES_REVISION_1) && defined(NDIS_TIMESTAMP_CAPABILITY_FLAGS)
    HANDLE h = INVALID_HANDLE_VALUE;
    if (!open_adapter(iface_label, h)) {
        return 0;
    }

    NDIS_TIMESTAMP_CAPABILITIES caps;
    DWORD returned = 0;
    NDIS_OID oid = OID_TIMESTAMP_CAPABILITY;
    memset(&caps, 0, sizeof(caps));

    BOOL ok = DeviceIoControl(h, IOCTL_NDIS_QUERY_GLOBAL_STATS,
                              &oid, sizeof(oid),
                              &caps, sizeof(caps),
                              &returned, NULL);
    CloseHandle(h);

    if (!ok || returned < sizeof(caps) || caps.TimestampFlags == 0) {
        return 0;
    }

    return caps.HardwareClockFrequencyHz;
#else
    (void)iface_label;
    return 0;
#endif
}

bool isHardwareTimestampSupported_NDIS(const char* iface_label) {
    if (!iface_label) {
        return false;
    }

#if defined(NDIS_TIMESTAMP_CAPABILITIES_REVISION_1) && defined(NDIS_TIMESTAMP_CAPABILITY_FLAGS)
    HANDLE h = INVALID_HANDLE_VALUE;
    if (!open_adapter(iface_label, h)) {
        return false;
    }

    NDIS_TIMESTAMP_CAPABILITIES caps;
    DWORD returned = 0;
    NDIS_OID oid = OID_TIMESTAMP_CAPABILITY;
    memset(&caps, 0, sizeof(caps));

    BOOL ok = DeviceIoControl(h, IOCTL_NDIS_QUERY_GLOBAL_STATS,
                              &oid, sizeof(oid),
                              &caps, sizeof(caps),
                              &returned, NULL);
    CloseHandle(h);

    if (!ok || returned < sizeof(caps)) {
        return false;
    }

    return caps.TimestampFlags != 0;
#else
    (void)iface_label;
    return false;
#endif
}

bool configureHardwareTimestamp_NDIS(const char* iface_label) {
    if (!iface_label) {
        return false;
    }

#if defined(NDIS_TIMESTAMP_CAPABILITIES_REVISION_1) && defined(NDIS_TIMESTAMP_CAPABILITY_FLAGS)
    HANDLE h = INVALID_HANDLE_VALUE;
    if (!open_adapter(iface_label, h)) {
        return false;
    }

    NDIS_TIMESTAMP_CAPABILITIES caps;
    DWORD returned = 0;
    NDIS_OID oid = OID_TIMESTAMP_CAPABILITY;
    memset(&caps, 0, sizeof(caps));

    BOOL ok = DeviceIoControl(h, IOCTL_NDIS_QUERY_GLOBAL_STATS,
                              &oid, sizeof(oid),
                              &caps, sizeof(caps),
                              &returned, NULL);
    if (!ok || returned < sizeof(caps) || caps.TimestampFlags == 0) {
        CloseHandle(h);
        return false;
    }

#ifdef OID_TIMESTAMP_CURRENT_CONFIG
    NDIS_TIMESTAMP_CAPABILITIES cfg;
    memset(&cfg, 0, sizeof(cfg));
    oid = OID_TIMESTAMP_CURRENT_CONFIG;
    if (DeviceIoControl(h, IOCTL_NDIS_QUERY_GLOBAL_STATS,
                        &oid, sizeof(oid),
                        &cfg, sizeof(cfg),
                        &returned, NULL)) {
        cfg.TimestampFlags = caps.TimestampFlags;
        DeviceIoControl(h, IOCTL_NDIS_QUERY_GLOBAL_STATS,
                        &oid, sizeof(oid),
                        &cfg, sizeof(cfg),
                        &returned, NULL);
    }
#endif

    CloseHandle(h);
    return true;
#else
    (void)iface_label;
    return false;
#endif
}
