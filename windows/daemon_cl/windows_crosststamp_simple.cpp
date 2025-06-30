/******************************************************************************

  Copyright (c) 2024, Intel Corporation
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

#include "windows_crosststamp_simple.hpp"
#include <intrin.h>

// Windows epoch (1601-01-01) to Unix epoch (1970-01-01) in 100ns units
static const uint64_t WINDOWS_TO_UNIX_EPOCH_OFFSET = 116444736000000000ULL;

// Function pointer for GetSystemTimePreciseAsFileTime (available on Windows 8+)
typedef VOID (WINAPI *GetSystemTimePreciseAsFileTimeProc)(LPFILETIME);
static GetSystemTimePreciseAsFileTimeProc g_GetSystemTimePreciseAsFileTime = nullptr;

// Global state
static bool g_initialized = false;
static LARGE_INTEGER g_qpc_frequency = {0};
static bool g_system_time_precise_available = false;
static uint64_t g_last_estimated_error_ns = 0;

namespace WindowsSimpleCrossTimestamp {

bool initialize()
{
    if (g_initialized) {
        return true;
    }

    // Initialize QueryPerformanceCounter frequency
    if (!QueryPerformanceFrequency(&g_qpc_frequency) || g_qpc_frequency.QuadPart == 0) {
        return false;
    }

    // Check for GetSystemTimePreciseAsFileTime availability
    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    if (hKernel32) {
        g_GetSystemTimePreciseAsFileTime = 
            (GetSystemTimePreciseAsFileTimeProc)GetProcAddress(hKernel32, "GetSystemTimePreciseAsFileTime");
        g_system_time_precise_available = (g_GetSystemTimePreciseAsFileTime != nullptr);
    }

    g_initialized = true;
    return true;
}

void cleanup()
{
    g_initialized = false;
    g_GetSystemTimePreciseAsFileTime = nullptr;
    g_system_time_precise_available = false;
    g_qpc_frequency.QuadPart = 0;
    g_last_estimated_error_ns = 0;
}

bool getCrossTimestamp(
    uint64_t* system_time_ns,
    uint64_t* device_time_ns,
    uint32_t* quality_percent)
{
    if (!g_initialized || !system_time_ns || !device_time_ns) {
        return false;
    }

    LARGE_INTEGER qpc_before, qpc_after, qpc_mid;
    FILETIME ft;

    // Get timestamps as close together as possible
    QueryPerformanceCounter(&qpc_before);
    
    if (g_system_time_precise_available) {
        g_GetSystemTimePreciseAsFileTime(&ft);
    } else {
        GetSystemTimeAsFileTime(&ft);
    }
    
    QueryPerformanceCounter(&qpc_after);

    // Use the average QPC value as the device timestamp
    qpc_mid.QuadPart = (qpc_before.QuadPart + qpc_after.QuadPart) / 2;

    // Convert system time from FILETIME to nanoseconds since Unix epoch
    *system_time_ns = fileTimeToUnixNs(&ft);

    // Convert QPC to nanoseconds
    *device_time_ns = (qpc_mid.QuadPart * 1000000000ULL) / g_qpc_frequency.QuadPart;

    // Calculate quality based on measurement window
    uint64_t qpc_delta = qpc_after.QuadPart - qpc_before.QuadPart;
    uint64_t error_ns = (qpc_delta * 1000000000ULL) / g_qpc_frequency.QuadPart;
    g_last_estimated_error_ns = error_ns;

    if (quality_percent) {
        if (g_system_time_precise_available) {
            // High precision system time available
            if (error_ns < 100) {
                *quality_percent = 95;
            } else if (error_ns < 500) {
                *quality_percent = 85;
            } else if (error_ns < 1000) {
                *quality_percent = 70;
            } else {
                *quality_percent = 50;
            }
        } else {
            // Standard system time
            if (error_ns < 500) {
                *quality_percent = 70;
            } else if (error_ns < 1000) {
                *quality_percent = 50;
            } else if (error_ns < 5000) {
                *quality_percent = 30;
            } else {
                *quality_percent = 15;
            }
        }
    }

    return true;
}

bool isPreciseTimestampingAvailable()
{
    return g_initialized && g_system_time_precise_available;
}

uint64_t getEstimatedErrorNs()
{
    return g_last_estimated_error_ns;
}

void unixNsToFileTime(uint64_t unix_ns, FILETIME* ft)
{
    if (!ft) return;

    // Convert nanoseconds to 100ns units and add Windows epoch offset
    uint64_t windows_time = (unix_ns / 100) + WINDOWS_TO_UNIX_EPOCH_OFFSET;
    
    ft->dwLowDateTime = (DWORD)(windows_time & 0xFFFFFFFF);
    ft->dwHighDateTime = (DWORD)(windows_time >> 32);
}

uint64_t fileTimeToUnixNs(const FILETIME* ft)
{
    if (!ft) return 0;

    uint64_t windows_time = ((uint64_t)ft->dwHighDateTime << 32) | ft->dwLowDateTime;
    uint64_t unix_time_100ns = windows_time - WINDOWS_TO_UNIX_EPOCH_OFFSET;
    return unix_time_100ns * 100;
}

uint64_t getSystemTimeNs()
{
    if (!g_initialized) {
        return 0;
    }

    FILETIME ft;
    if (g_system_time_precise_available) {
        g_GetSystemTimePreciseAsFileTime(&ft);
    } else {
        GetSystemTimeAsFileTime(&ft);
    }
    
    return fileTimeToUnixNs(&ft);
}

uint64_t getQPCTimeNs()
{
    if (!g_initialized) {
        return 0;
    }

    LARGE_INTEGER qpc;
    QueryPerformanceCounter(&qpc);
    return (qpc.QuadPart * 1000000000ULL) / g_qpc_frequency.QuadPart;
}

} // namespace WindowsSimpleCrossTimestamp
