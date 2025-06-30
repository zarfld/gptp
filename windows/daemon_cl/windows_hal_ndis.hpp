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

#ifndef WINDOWS_HAL_NDIS_HPP
#define WINDOWS_HAL_NDIS_HPP

/**@file*/

#include <WinSock2.h>
#include <windows.h>
#include <ntddndis.h>

#include <cstdint>

// NDIS-specific hardware timestamp detection functions

/**
 * @brief Get hardware clock rate using NDIS method
 * @param iface_label Interface label string
 * @return Clock rate in Hz, or 0 if not found
 */
uint64_t getHardwareClockRate_NDIS(const char* iface_label);

/**
 * @brief Check if hardware timestamping is supported using NDIS
 * @param iface_label Interface label string  
 * @return true if supported, false otherwise
 */
bool isHardwareTimestampSupported_NDIS(const char* iface_label);

/**
 * @brief Configure hardware timestamping using NDIS
 * @param iface_label Interface label string
 * @return true if successful, false otherwise
 */
bool configureHardwareTimestamp_NDIS(const char* iface_label);

// NDIS-specific event-driven link monitoring (conceptual framework)

/**
 * @brief NDIS-style interface change notification callback
 */
typedef void (*NDIS_INTERFACE_CHANGE_CALLBACK)(
    const char* interfaceDesc,
    bool linkUp,
    void* context
);

/**
 * @brief NDIS-style link monitoring context
 */
struct NDISLinkMonitorContext;

/**
 * @brief Start NDIS-style event-driven link monitoring
 * @param interfaceDesc Interface description
 * @param macAddress Interface MAC address
 * @param callback Callback function for link state changes
 * @param context User context for callback
 * @return Monitoring context or NULL on failure
 */
NDISLinkMonitorContext* startNDISLinkMonitoring(
    const char* interfaceDesc,
    const BYTE* macAddress,
    NDIS_INTERFACE_CHANGE_CALLBACK callback,
    void* context
);

/**
 * @brief Stop NDIS-style event-driven link monitoring
 * @param context NDIS monitoring context
 */
void stopNDISLinkMonitoring(NDISLinkMonitorContext* context);

#endif // WINDOWS_HAL_NDIS_HPP
