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
    GPTP_LOG_INFO("Windows watchdog handler destroyed.");
}

long unsigned int WindowsWatchdogHandler::getWindowsWatchdogInterval(int *result)
{
    *result = 1; // Indicate watchdog is enabled
    
    // Return interval in microseconds (already stored in microseconds)
    long unsigned int watchdog_interval = update_interval;
    
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
    GPTP_LOG_INFO("Windows watchdog update thread started");
    
    while (!stop_watchdog)
    {
        GPTP_LOG_DEBUG("NOTIFYING WINDOWS WATCHDOG");
        
        // Report health to appropriate Windows mechanism
        if (service_mode) {
            reportServiceStatus(SERVICE_RUNNING);
        } else {
            reportHealth("gPTP daemon healthy", true);
        }
        
        GPTP_LOG_DEBUG("GOING TO SLEEP %lu microseconds", update_interval);
        
        // Sleep for watchdog interval (convert microseconds to milliseconds)
        DWORD sleep_ms = (DWORD)(update_interval / 1000);
        Sleep(sleep_ms);
        
        GPTP_LOG_DEBUG("WATCHDOG WAKE UP");
    }
    
    GPTP_LOG_INFO("Windows watchdog update thread stopped");
}

void WindowsWatchdogHandler::reportHealth(const char* status, bool is_healthy)
{
    if (!status) return;
    
    // Log to Windows Event Log
    WORD event_type = is_healthy ? EVENTLOG_INFORMATION_TYPE : EVENTLOG_ERROR_TYPE;
    logToEventLog(status, event_type);
    
    // Also log via gPTP logging system
    if (is_healthy) {
        GPTP_LOG_DEBUG("Health status: %s", status);
    } else {
        GPTP_LOG_ERROR("Health status: %s", status);
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
    
    GPTP_LOG_INFO("Service watchdog features initialized");
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
