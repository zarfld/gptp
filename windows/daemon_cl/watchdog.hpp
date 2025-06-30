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

#ifndef WINDOWS_WATCHDOG_HPP
#define WINDOWS_WATCHDOG_HPP

/**@file*/

#include <windows.h>
#include <process.h>

/**
 * @brief Windows-specific watchdog handler for system health monitoring
 * 
 * Provides functionality equivalent to Linux systemd watchdog, using:
 * - Windows Service Control Manager integration when running as service
 * - Application-level health monitoring for console applications
 * - Hardware watchdog timer support where available
 * 
 * This implementation mirrors the Linux watchdog.hpp interface for
 * cross-platform compatibility while providing Windows-native integration.
 */

/**
 * @brief Thread function for watchdog updates
 * @param arg Pointer to WindowsWatchdogHandler instance
 * @return Thread exit code
 */
DWORD WINAPI watchdogUpdateThreadFunction(LPVOID arg);

/**
 * @brief Windows watchdog handler class
 * 
 * Provides system health monitoring and watchdog functionality:
 * - Service heartbeat reporting to SCM
 * - Application health status updates
 * - Hardware watchdog integration (where supported)
 * - Periodic health checks and logging
 */
class WindowsWatchdogHandler
{
public:
    /**
     * @brief Watchdog update interval in nanoseconds
     * Default: 30 seconds (similar to systemd default)
     */
    long unsigned int update_interval;

    /**
     * @brief Constructor - initializes Windows watchdog handler
     */
    WindowsWatchdogHandler();

    /**
     * @brief Destructor - cleanup watchdog resources
     */
    virtual ~WindowsWatchdogHandler();

    /**
     * @brief Get watchdog interval from Windows configuration
     * @param result [out] Pointer to store configuration result
     * @return Watchdog interval in microseconds, or 0 if disabled
     */
    long unsigned int getWindowsWatchdogInterval(int *result);

    /**
     * @brief Start watchdog monitoring
     * Creates background thread for periodic health updates
     * @return true if started successfully, false on error
     */
    bool startWatchdog();

    /**
     * @brief Stop watchdog monitoring
     * Gracefully stops background thread and cleanup
     */
    void stopWatchdog();

    /**
     * @brief Main watchdog update loop
     * Called by background thread to perform periodic updates
     */
    void run_update();

    /**
     * @brief Report application health to Windows SCM/Event Log
     * @param status Health status message
     * @param is_healthy true if application is healthy, false for error
     */
    void reportHealth(const char* status, bool is_healthy = true);

    /**
     * @brief Report an error condition to the watchdog
     * @param error_message Description of the error
     * @param is_critical true if this is a critical error that should affect service status
     */
    void reportError(const char* error_message, bool is_critical = false);

    /**
     * @brief Check if running as Windows service
     * @return true if running as service, false if console application
     */
    bool isRunningAsService();

private:
    HANDLE timer_handle;                ///< Windows timer for watchdog intervals
    HANDLE watchdog_thread;             ///< Background watchdog thread handle
    DWORD thread_id;                    ///< Watchdog thread ID
    volatile bool stop_watchdog;        ///< Flag to stop watchdog thread
    SERVICE_STATUS_HANDLE service_handle; ///< Windows service handle (if applicable)
    bool service_mode;                  ///< true if running as Windows service
    CRITICAL_SECTION health_lock;       ///< Critical section for thread-safe health reporting

    /**
     * @brief Initialize service-specific watchdog features
     * @return true if service integration successful
     */
    bool initializeServiceWatchdog();

    /**
     * @brief Report service status to Windows SCM
     * @param status Service status to report
     */
    void reportServiceStatus(DWORD status);

    /**
     * @brief Log health status to Windows Event Log
     * @param message Health status message
     * @param event_type Event log type (EVENTLOG_INFORMATION_TYPE, etc.)
     */
    void logToEventLog(const char* message, WORD event_type);
};

#endif // WINDOWS_WATCHDOG_HPP
