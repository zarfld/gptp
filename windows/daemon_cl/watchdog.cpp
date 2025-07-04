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

#include "watchdog.hpp"
#include "../../common/gptp_log.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ether_port.hpp"
#include <atomic>
#include <thread>

// Global pointer to EtherPort for heartbeat monitoring
extern EtherPort *gptp_ether_port;

// Heartbeat timeout in milliseconds
#define NETWORK_THREAD_HEARTBEAT_TIMEOUT_MS 5000

// Helper to get current thread ID as uint64_t
static uint64_t get_current_thread_id() {
#ifdef _WIN32
    return (uint64_t)GetCurrentThreadId();
#else
    return (uint64_t)std::this_thread::get_id().hash();
#endif
}

DWORD WINAPI watchdogUpdateThreadFunction(LPVOID arg)
{
    WindowsWatchdogHandler *watchdog = (WindowsWatchdogHandler*) arg;
    if (watchdog) {
        watchdog->run_update();
    }
    return 0;
}

WindowsWatchdogHandler::WindowsWatchdogHandler()
{
    GPTP_LOG_INFO("Creating Windows watchdog handler.");
    
    // Initialize with default 30-second interval (similar to systemd default)
    update_interval = 30000000; // 30 seconds in microseconds (to fit in unsigned long)
    
    // Initialize handles
    timer_handle = NULL;
    watchdog_thread = NULL;
    thread_id = 0;
    stop_watchdog = false;
    service_handle = NULL;
    service_mode = false;
    
    // Initialize critical section for thread safety
    InitializeCriticalSection(&health_lock);
    
    // Check if running as service and initialize accordingly
    service_mode = isRunningAsService();
    if (service_mode) {
        initializeServiceWatchdog();
    }
    
    GPTP_LOG_INFO("Windows watchdog handler initialized. Service mode: %s", 
                  service_mode ? "YES" : "NO");
}

WindowsWatchdogHandler::~WindowsWatchdogHandler()
{
    stopWatchdog();
    DeleteCriticalSection(&health_lock);
    GPTP_LOG_INFO("Windows watchdog handler destroyed.");
}

long unsigned int WindowsWatchdogHandler::getWindowsWatchdogInterval(int *result)
{
    *result = 1; // Indicate watchdog is enabled by default
    
    // Try to read interval from configuration
    // TODO: Add proper config file reading when config system is enhanced
    // For now, use reasonable default similar to systemd
    long unsigned int watchdog_interval = update_interval;
    
    // Check if watchdog should be disabled via environment variable
    char* watchdog_disabled = getenv("GPTP_WATCHDOG_DISABLED");
    if (watchdog_disabled && (strcmp(watchdog_disabled, "1") == 0 || 
                              _stricmp(watchdog_disabled, "true") == 0)) {
        *result = 0; // Disable watchdog
        GPTP_LOG_INFO("Windows watchdog disabled via environment variable");
        return 0;
    }
    
    // Check for custom interval via environment variable
    char* custom_interval = getenv("GPTP_WATCHDOG_INTERVAL");
    if (custom_interval) {
        long unsigned int custom_val = strtoul(custom_interval, NULL, 10);
        if (custom_val > 0 && custom_val <= 300000000) { // Max 5 minutes
            watchdog_interval = custom_val;
            GPTP_LOG_INFO("Using custom watchdog interval from environment: %lu us", watchdog_interval);
        }
    }
    
    GPTP_LOG_INFO("Windows watchdog interval: %lu microseconds", watchdog_interval);
    return watchdog_interval;
}

bool WindowsWatchdogHandler::startWatchdog()
{
    if (watchdog_thread != NULL) {
        GPTP_LOG_WARNING("Watchdog already running");
        return true;
    }
    
    stop_watchdog = false;
    
    // Create watchdog thread
    watchdog_thread = CreateThread(
        NULL,                          // Security attributes
        0,                             // Stack size (default)
        watchdogUpdateThreadFunction,  // Thread function
        this,                          // Thread parameter
        0,                             // Creation flags
        &thread_id                     // Thread ID
    );
    
    if (watchdog_thread == NULL) {
        GPTP_LOG_ERROR("Failed to create watchdog thread: %d", GetLastError());
        return false;
    }
    
    GPTP_LOG_INFO("Windows watchdog started successfully. Thread ID: %d", thread_id);
    return true;
}

void WindowsWatchdogHandler::stopWatchdog()
{
    if (watchdog_thread == NULL) {
        return;
    }
    
    GPTP_LOG_INFO("Stopping Windows watchdog...");
    
    // Signal thread to stop
    stop_watchdog = true;
    
    // Wait for thread to complete (10 second timeout)
    DWORD result = WaitForSingleObject(watchdog_thread, 10000);
    if (result == WAIT_TIMEOUT) {
        GPTP_LOG_WARNING("Watchdog thread did not stop gracefully, terminating");
        TerminateThread(watchdog_thread, 1);
    }
    
    CloseHandle(watchdog_thread);
    watchdog_thread = NULL;
    thread_id = 0;
    
    GPTP_LOG_INFO("Windows watchdog stopped.");
}

void WindowsWatchdogHandler::run_update()
{
    GPTP_LOG_INFO("Windows watchdog update thread started (thread id: %llu)", get_current_thread_id());
    unsigned long update_count = 0;
    uint64_t last_heartbeat = 0;
    uint64_t last_activity = 0;
    bool last_healthy = true;
    bool first_heartbeat_received = false;
    
    // Give the network thread time to start up before we start monitoring
    GPTP_LOG_INFO("Watchdog: Waiting 5 seconds for network thread startup...");
    Sleep(5000);  // Wait 5 seconds before starting monitoring

    // Wait for the first heartbeat to be initialized before starting strict monitoring
    GPTP_LOG_INFO("Watchdog: Waiting for first network thread heartbeat...");
    while (!stop_watchdog && !first_heartbeat_received) {
        if (gptp_ether_port) {
            uint64_t current_heartbeat = gptp_ether_port->network_thread_heartbeat.load(std::memory_order_relaxed);
            if (current_heartbeat > 0) {
                first_heartbeat_received = true;
                last_heartbeat = current_heartbeat;
                GPTP_LOG_INFO("Watchdog: First heartbeat received (%llu), starting monitoring", current_heartbeat);
                break;
            }
        }
        Sleep(1000);  // Check every second for first heartbeat
    }

    while (!stop_watchdog)
    {
        update_count++;
        bool healthy = true;
        char health_message[256];

        // Heartbeat check: is the network thread alive?
        if (gptp_ether_port) {
            uint64_t current_heartbeat = gptp_ether_port->network_thread_heartbeat.load(std::memory_order_relaxed);
            uint64_t current_activity = gptp_ether_port->network_thread_last_activity.load(std::memory_order_relaxed);
            LARGE_INTEGER qpc_now, qpc_freq;
            QueryPerformanceCounter(&qpc_now);
            QueryPerformanceFrequency(&qpc_freq);
            uint64_t activity_age_ticks = qpc_now.QuadPart - current_activity;
            double activity_age_ms = (double)activity_age_ticks * 1000.0 / (double)qpc_freq.QuadPart;
            DWORD watchdog_tid = GetCurrentThreadId();
            
            // Be more tolerant during the first few updates (startup grace period)
            bool is_startup_period = (update_count <= 3);
            double timeout_threshold = is_startup_period ? (NETWORK_THREAD_HEARTBEAT_TIMEOUT_MS * 3) : NETWORK_THREAD_HEARTBEAT_TIMEOUT_MS;
            
            // Only check for heartbeat stalls if we've received the first heartbeat
            if (first_heartbeat_received && 
                ((current_heartbeat == last_heartbeat && update_count > 1) || 
                 (activity_age_ms > timeout_threshold && current_activity > 0))) {
                // Debug print for diagnosis
                GPTP_LOG_DEBUG("watchdog: gptp_ether_port=%p, last_heartbeat=%llu, current_heartbeat=%llu, last_activity(QPC)=%llu, now(QPC)=%lld, activity_age_ms=%.2f, watchdog_thread_id=%lu\n",
                    gptp_ether_port,
                    last_heartbeat,
                    current_heartbeat,
                    current_activity,
                    qpc_now.QuadPart,
                    activity_age_ms,
                    watchdog_tid);
                fflush(stdout);
                healthy = false;
                snprintf(health_message, sizeof(health_message),
                    "gPTP daemon ERROR: Network thread heartbeat lost (last=%llu, now=%llu, activity_age=%.2f s) [update #%lu]",
                    last_heartbeat, current_heartbeat, activity_age_ms / 1000.0, update_count);
            } else {
                if (update_count <= 5) {
                    // Log more details during startup
                    GPTP_LOG_DEBUG("watchdog: startup check #%lu - heartbeat=%llu, activity_age=%.2f ms, threshold=%.2f ms", 
                        update_count, current_heartbeat, activity_age_ms, timeout_threshold);
                }
                snprintf(health_message, sizeof(health_message),
                    "gPTP daemon healthy - network thread heartbeat OK (heartbeat=%llu, activity(QPC)=%llu, now(QPC)=%lld) [update #%lu]",
                    current_heartbeat, current_activity, qpc_now.QuadPart, update_count);
            }
            last_heartbeat = current_heartbeat;
            last_activity = current_activity;
        } else {
            snprintf(health_message, sizeof(health_message), "gPTP daemon healthy - watchdog update #%lu", update_count);
        }

        // Report health to Windows
        if (service_mode) {
            reportServiceStatus(SERVICE_RUNNING);
            reportHealth(health_message, healthy);
        } else {
            reportHealth(health_message, healthy);
        }

        if (!healthy && last_healthy) {
            GPTP_LOG_ERROR("Network thread heartbeat lost - reporting unhealthy to watchdog");
        }
        last_healthy = healthy;

        if (update_count % 10 == 0) {
            snprintf(health_message, sizeof(health_message),
                "gPTP daemon extended health check - %lu updates completed", update_count);
            reportHealth(health_message, healthy);
        }

        GPTP_LOG_DEBUG("GOING TO SLEEP %lu microseconds", update_interval);
        DWORD sleep_ms = (DWORD)(update_interval / 1000);
        if (sleep_ms < 100) sleep_ms = 100;
        Sleep(sleep_ms);
        GPTP_LOG_DEBUG("WATCHDOG WAKE UP");
    }
    GPTP_LOG_INFO("Windows watchdog update thread stopped after %lu updates", update_count);
}

void WindowsWatchdogHandler::reportHealth(const char* status, bool is_healthy)
{
    if (!status) return;
    
    EnterCriticalSection(&health_lock);
    
    // Log to Windows Event Log
    WORD event_type = is_healthy ? EVENTLOG_INFORMATION_TYPE : EVENTLOG_ERROR_TYPE;
    logToEventLog(status, event_type);
    
    // Also log via gPTP logging system
    if (is_healthy) {
        GPTP_LOG_DEBUG("Health status: %s", status);
    } else {
        GPTP_LOG_ERROR("Health status: %s", status);
    }
    
    // In service mode, also update service status if this is an error
    if (service_mode && !is_healthy) {
        // Report service problem
        reportServiceStatus(SERVICE_PAUSED);
        GPTP_LOG_WARNING("Service status set to PAUSED due to health issue");
    }
    
    LeaveCriticalSection(&health_lock);
}

void WindowsWatchdogHandler::reportError(const char* error_message, bool is_critical)
{
    if (!error_message) return;
    
    char full_message[512];
    snprintf(full_message, sizeof(full_message), 
             "gPTP Error%s: %s", 
             is_critical ? " (CRITICAL)" : "", 
             error_message);
    
    // Report as health issue
    reportHealth(full_message, false);
    
    // For critical errors in service mode, consider stopping the service
    if (is_critical && service_mode) {
        GPTP_LOG_ERROR("Critical error reported to watchdog - service may need restart");
        // In a full implementation, this might trigger service restart logic
    }
}

bool WindowsWatchdogHandler::isRunningAsService()
{
    // Simple heuristic: check if running in console session
    // In a real implementation, this could be more sophisticated
    DWORD session_id = 0;
    if (ProcessIdToSessionId(GetCurrentProcessId(), &session_id)) {
        // Session 0 is typically the services session
        return (session_id == 0);
    }
    
    // Fallback: check if we have a console window
    return (GetConsoleWindow() == NULL);
}

bool WindowsWatchdogHandler::initializeServiceWatchdog()
{
    // In a full service implementation, this would:
    // 1. Register service control handlers
    // 2. Set up service status reporting
    // 3. Configure service-specific watchdog features
    
    // For now, just set up basic service status reporting
    // In a real service, the service_handle would be set during service registration
    // This is a placeholder for when gPTP runs as a proper Windows service
    
    GPTP_LOG_INFO("Service watchdog features initialized");
    
    // Try to determine if we're actually running as a service
    if (service_mode) {
        // Register a simplified service control handler
        // In a full implementation, this would integrate with the main service control
        GPTP_LOG_INFO("Running in service mode - enhanced watchdog monitoring enabled");
        return true;
    }
    
    return true;
}

void WindowsWatchdogHandler::reportServiceStatus(DWORD status)
{
    if (!service_mode || service_handle == NULL) {
        return;
    }
    
    SERVICE_STATUS service_status;
    ZeroMemory(&service_status, sizeof(service_status));
    
    service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    service_status.dwCurrentState = status;
    service_status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    service_status.dwWin32ExitCode = NO_ERROR;
    service_status.dwCheckPoint = 0;
    service_status.dwWaitHint = 0;
    
    SetServiceStatus(service_handle, &service_status);
    
    GPTP_LOG_DEBUG("Service status reported: %d", status);
}

void WindowsWatchdogHandler::logToEventLog(const char* message, WORD event_type)
{
    if (!message) return;
    
    HANDLE event_log = RegisterEventSourceA(NULL, "gPTP");
    if (event_log != NULL) {
        const char* strings[1] = { message };
        
        ReportEventA(
            event_log,          // Event log handle
            event_type,         // Event type
            0,                  // Event category
            0,                  // Event ID
            NULL,               // User security identifier
            1,                  // Number of strings
            0,                  // Binary data size
            strings,            // String array
            NULL                // Binary data
        );
        
        DeregisterEventSource(event_log);
    }
}
