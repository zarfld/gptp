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

#include "windows_hal_common.hpp"
#include "windows_hal_iphlpapi.hpp"
#include "windows_hal_ndis.hpp"

// Thread callback implementation
DWORD WINAPI OSThreadCallback( LPVOID input ) {
    // Implementation would need to be moved from the original windows_hal.cpp
    // For now, this is a placeholder structure
    return 0;
}

// Timer queue handler implementation
VOID CALLBACK WindowsTimerQueueHandler( PVOID arg_in, BOOLEAN ignore ) {
    // Implementation would need to be moved from the original windows_hal.cpp
    // For now, this is a placeholder structure
}

// Unified hardware clock rate detection with fallback strategy
uint64_t getHardwareClockRate(const char* iface_label) {
    if (!iface_label) {
        return 0;
    }

    // Try IPHLPAPI method first (more modern and reliable)
    uint64_t rate = getHardwareClockRate_IPHLPAPI(iface_label);
    if (rate != 0) {
        return rate;
    }

    // Fallback to NDIS method if IPHLPAPI fails
    rate = getHardwareClockRate_NDIS(iface_label);
    if (rate != 0) {
        return rate;
    }

    // No hardware clock rate could be determined
    return 0;
}

bool isHardwareTimestampSupported(const char* iface_label) {
    if (!iface_label) {
        return false;
    }

    // Check IPHLPAPI support first
    if (isHardwareTimestampSupported_IPHLPAPI(iface_label)) {
        return true;
    }

    // Fallback to NDIS method
    return isHardwareTimestampSupported_NDIS(iface_label);
}

bool configureHardwareTimestamp(const char* iface_label) {
    if (!iface_label) {
        return false;
    }

    // Try NDIS configuration first (typically needed for enabling timestamping)
    if (configureHardwareTimestamp_NDIS(iface_label)) {
        return true;
    }

    // IPHLPAPI doesn't typically require explicit configuration
    // but we can check if hardware timestamping is available
    return isHardwareTimestampSupported_IPHLPAPI(iface_label);
}
