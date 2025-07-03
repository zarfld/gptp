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
#include "windows_hal.hpp"
#include "windows_hal_iphlpapi.hpp"
#include "windows_hal_ndis.hpp"
#include "gptp_log.hpp"
#include "avbts_osthread.hpp"

// Thread callback implementation
DWORD WINAPI OSThreadCallback( LPVOID input ) {
    GPTP_LOG_STATUS("*** OSThreadCallback ENTRY (thread_id=%lu, input=%p) ***", GetCurrentThreadId(), input);
    
    OSThreadArg *arg = (OSThreadArg*) input;
    
    if (!arg) {
        GPTP_LOG_ERROR("*** OSThreadCallback: arg is NULL! ***");
        return 1;
    }
    
    GPTP_LOG_STATUS("*** OSThreadCallback: About to call func=%p with arg=%p (thread_id=%lu) ***", 
        (void*)arg->func, arg->arg, GetCurrentThreadId());
    
    try {
        if (arg->func) {
            arg->ret = arg->func( arg->arg );
            GPTP_LOG_STATUS("*** OSThreadCallback: func returned successfully (thread_id=%lu) ***", GetCurrentThreadId());
        } else {
            GPTP_LOG_ERROR("*** OSThreadCallback: func is NULL! ***");
            arg->ret = (OSThreadExitCode)1; // osthread_error
        }
    } catch (const std::exception& ex) {
        GPTP_LOG_ERROR("*** OSThreadCallback: Exception caught: %s (thread_id=%lu) ***", ex.what(), GetCurrentThreadId());
        arg->ret = (OSThreadExitCode)1; // osthread_error
    } catch (...) {
        GPTP_LOG_ERROR("*** OSThreadCallback: Unknown exception caught (thread_id=%lu) ***", GetCurrentThreadId());
        arg->ret = (OSThreadExitCode)1; // osthread_error
    }
    
    GPTP_LOG_STATUS("*** OSThreadCallback EXIT (thread_id=%lu) ***", GetCurrentThreadId());
    return 0;
}

// Timer queue handler implementation
VOID CALLBACK WindowsTimerQueueHandler( PVOID arg_in, BOOLEAN ignore ) {
    WindowsTimerQueueHandlerArg *arg = (WindowsTimerQueueHandlerArg *) arg_in;
    
    if (!arg) {
        GPTP_LOG_ERROR("Timer queue handler called with NULL argument");
        return;
    }
    
    GPTP_LOG_DEBUG("Timer queue handler fired: type=%d", arg->type);
    
    // Call the timer callback function
    if (arg->func) {
        arg->func(arg->inner_arg);
    } else {
        GPTP_LOG_ERROR("Timer queue handler: callback function is NULL for type %d", arg->type);
    }
    
    // Mark timer as completed - this will be cleaned up later
    // We do this atomically to avoid race conditions with cleanup
    arg->completed = true;
    
    // For one-shot timers that completed normally, clean up the inner argument immediately
    // (This matches the Linux implementation behavior)
    if (arg->rm && arg->inner_arg) {
        delete arg->inner_arg;
        arg->inner_arg = nullptr;  // Prevent double deletion
    }
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
