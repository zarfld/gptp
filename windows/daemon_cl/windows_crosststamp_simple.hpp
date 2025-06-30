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

#ifndef WINDOWS_CROSSTSTAMP_SIMPLE_HPP
#define WINDOWS_CROSSTSTAMP_SIMPLE_HPP

/**@file*/

#include <windows.h>
#include <cstdint>

// Forward declaration for common types
struct Timestamp;

/**
 * @brief Simple Windows cross-timestamping functionality
 * 
 * This provides a simplified cross-timestamping implementation that doesn't
 * require extensive changes to the existing Windows HAL.
 */
namespace WindowsSimpleCrossTimestamp {

    /**
     * @brief Initialize cross-timestamping subsystem
     * @return true if successful, false otherwise
     */
    bool initialize();

    /**
     * @brief Cleanup cross-timestamping subsystem
     */
    void cleanup();

    /**
     * @brief Get cross-timestamp using high-precision Windows APIs
     * @param system_time_ns Output system time in nanoseconds since epoch
     * @param device_time_ns Output device time in nanoseconds (QPC-based)
     * @param quality_percent Quality metric (0-100, 100 being best)
     * @return true if successful, false otherwise
     */
    bool getCrossTimestamp(
        uint64_t* system_time_ns,
        uint64_t* device_time_ns,
        uint32_t* quality_percent = nullptr
    );

    /**
     * @brief Check if high-precision timestamping is available
     * @return true if available, false otherwise
     */
    bool isPreciseTimestampingAvailable();

    /**
     * @brief Get estimated error in nanoseconds
     * @return Estimated error in nanoseconds
     */
    uint64_t getEstimatedErrorNs();

    /**
     * @brief Convert nanoseconds since Unix epoch to Windows FILETIME
     * @param unix_ns Nanoseconds since Unix epoch
     * @param ft Output FILETIME structure
     */
    void unixNsToFileTime(uint64_t unix_ns, FILETIME* ft);

    /**
     * @brief Convert Windows FILETIME to nanoseconds since Unix epoch
     * @param ft FILETIME structure
     * @return Nanoseconds since Unix epoch
     */
    uint64_t fileTimeToUnixNs(const FILETIME* ft);

    /**
     * @brief Get high-precision system time
     * @return System time in nanoseconds since Unix epoch
     */
    uint64_t getSystemTimeNs();

    /**
     * @brief Get high-precision QPC-based time
     * @return QPC time in nanoseconds
     */
    uint64_t getQPCTimeNs();

} // namespace WindowsSimpleCrossTimestamp

#endif // WINDOWS_CROSSTSTAMP_SIMPLE_HPP
