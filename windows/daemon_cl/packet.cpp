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


#include <winsock2.h>
#include <windows.h>
#include <ws2def.h>
#define WPCAP
#include <pcap.h>
#ifdef inline
#undef inline
#endif
#include <iphlpapi.h>
#include <atomic>

#include "packet.hpp"
#include "platform.hpp"

#include "../../common/ether_port.hpp"
extern EtherPort *gptp_ether_port;

void update_network_thread_heartbeat() {
    GPTP_LOG_DEBUG("update_network_thread_heartbeat: ENTER (thread_id=%lu, gptp_ether_port=%p)\n", GetCurrentThreadId(), gptp_ether_port);
    
    // Add more defensive checks
    if (!gptp_ether_port) {
        GPTP_LOG_DEBUG("update_network_thread_heartbeat: gptp_ether_port=NULL (thread_id=%lu)\n", GetCurrentThreadId());
        return;
    }
    
    // Check if the pointer is valid with defensive checks
    try {
        // Test if we can access the object safely
        if (gptp_ether_port != nullptr) {
            auto tid = GetCurrentThreadId();
            gptp_ether_port->network_thread_heartbeat.fetch_add(1, std::memory_order_relaxed);
            ULONGLONG now_gk = GetTickCount64();
            LARGE_INTEGER qpc;
            if (QueryPerformanceCounter(&qpc) != 0) {
                // Store QPC ticks for liveness/activity, not GetTickCount64
                gptp_ether_port->network_thread_last_activity.store((uint64_t)qpc.QuadPart, std::memory_order_relaxed);
                GPTP_LOG_DEBUG("update_network_thread_heartbeat: thread_id=%lu, GetTickCount64()=%llu ms, QPC=%lld, gptp_ether_port=%p, heartbeat=%llu, last_activity(QPC)=%llu\n",
                       tid, now_gk, qpc.QuadPart, gptp_ether_port, gptp_ether_port->network_thread_heartbeat.load(), gptp_ether_port->network_thread_last_activity.load());
            } else {
                GPTP_LOG_ERROR("*** ERROR: QueryPerformanceCounter failed in update_network_thread_heartbeat ***");
            }
        } else {
            GPTP_LOG_ERROR("*** ERROR: gptp_ether_port is null in update_network_thread_heartbeat ***");
        }
    } catch (...) {
        GPTP_LOG_ERROR("*** FATAL: Exception in update_network_thread_heartbeat - gptp_ether_port pointer is invalid! (thread_id=%lu, ptr=%p) ***", GetCurrentThreadId(), gptp_ether_port);
        // Don't access the pointer anymore
        gptp_ether_port = nullptr;
    }
}

// Enhanced debugging support
#ifdef DEBUG_PACKET_RECEPTION
#include "debug_packet_reception.hpp"
#endif

#define MAX_FRAME_SIZE 96

// Enhanced timeout configuration for direct NIC-to-NIC debugging
#define OPTIMIZED_TIMEOUT_MS 1      // 1ms for near real-time response
#define STANDARD_TIMEOUT_MS 100     // 100ms for normal operation
#define DEBUG_TIMEOUT_MS 10         // 10ms for debugging

// Global debug flag and packet tracking
static bool g_debug_mode = false;
static uint32_t total_packet_count = 0;
static uint32_t ptp_packet_count = 0;
static uint32_t timeout_count = 0;
static uint32_t last_debug_report = 0;

void enablePacketReceptionDebug(bool enable) {
    g_debug_mode = enable;
    if (enable) {
        GPTP_LOG_DEBUG("Enhanced packet reception debugging enabled\n");
        GPTP_LOG_DEBUG("Packet tracking initialized - total=%u, ptp=%u, timeouts=%u\n", 
               total_packet_count, ptp_packet_count, timeout_count);
    }
}

// ✅ IMPLEMENTING: WinPcap to Npcap Migration - Runtime Backend Detection
// Support both modern Npcap and legacy WinPcap with proper identification

#define PCAP_INTERFACENAMEPREFIX "rpcap://\\Device\\NPF_"
#define PCAP_INTERFACENAMEPREFIX_LENGTH 20
#define PCAP_INTERFACENAMESUFFIX_LENGTH 38
#define PCAP_INTERFACENAME_LENGTH PCAP_INTERFACENAMEPREFIX_LENGTH+PCAP_INTERFACENAMESUFFIX_LENGTH

// Runtime backend detection
#ifdef USING_NPCAP
    #define PCAP_BACKEND_NAME "Npcap"
    #define PCAP_BACKEND_MODERN true
#elif defined(USING_WINPCAP)
    #define PCAP_BACKEND_NAME "WinPcap"
    #define PCAP_BACKEND_MODERN false
#else
    #define PCAP_BACKEND_NAME "Unknown"
    #define PCAP_BACKEND_MODERN false
#endif

struct packet_handle {
    pcap_t *iface;
    char errbuf[PCAP_ERRBUF_SIZE];
    packet_addr_t iface_addr;
    HANDLE capture_lock;
    uint16_t ethertype;
    struct bpf_program filter;
};


packet_error_t mallocPacketHandle( struct packet_handle **phandle ) {
    packet_error_t ret = PACKET_NO_ERROR;

    packet_handle *handle = (struct packet_handle *) malloc((size_t) sizeof( *handle ));
    if( handle == NULL ) {
        ret = PACKET_NOMEMORY_ERROR;
        goto fnexit;
    }
    *phandle = handle;

    if(( handle->capture_lock = CreateMutex( NULL, FALSE, NULL )) == NULL ) {
        ret = PACKET_CREATEMUTEX_ERROR;
        goto fnexit;
    }
    handle->iface = NULL;

fnexit:
    return ret;
}

void freePacketHandle( struct packet_handle *handle ) {
    CloseHandle( handle->capture_lock );
    free((void *) handle );
}

packet_error_t openInterfaceByAddr( struct packet_handle *handle, packet_addr_t *addr, int timeout ) {
    packet_error_t ret = PACKET_NO_ERROR;
    char name[PCAP_INTERFACENAME_LENGTH+1] = "\0";

    PIP_ADAPTER_ADDRESSES pAdapterAddress;
    IP_ADAPTER_ADDRESSES AdapterAddress[32];       // Allocate information for up to 32 NICs
    DWORD dwBufLen = sizeof(AdapterAddress);  // Save memory size of buffer

    DWORD dwStatus = GetAdaptersAddresses( AF_UNSPEC, 0, NULL, AdapterAddress, &dwBufLen);

    if( dwStatus != ERROR_SUCCESS ) {
        ret = PACKET_IFLOOKUP_ERROR;
        goto fnexit;
    }

    // Debug: Log all available adapters and target MAC address
    GPTP_LOG_INFO("Target MAC address: %02X-%02X-%02X-%02X-%02X-%02X",
        addr->addr[0], addr->addr[1], addr->addr[2], 
        addr->addr[3], addr->addr[4], addr->addr[5]);
    GPTP_LOG_INFO("Available network adapters:");
    
    for( pAdapterAddress = AdapterAddress; pAdapterAddress != NULL; pAdapterAddress = pAdapterAddress->Next ) {
        if( pAdapterAddress->PhysicalAddressLength == ETHER_ADDR_OCTETS ) {
            GPTP_LOG_INFO("  Adapter: %s, MAC: %02X-%02X-%02X-%02X-%02X-%02X", 
                pAdapterAddress->AdapterName,
                pAdapterAddress->PhysicalAddress[0], pAdapterAddress->PhysicalAddress[1],
                pAdapterAddress->PhysicalAddress[2], pAdapterAddress->PhysicalAddress[3],
                pAdapterAddress->PhysicalAddress[4], pAdapterAddress->PhysicalAddress[5]);
        }
        
        if( pAdapterAddress->PhysicalAddressLength == ETHER_ADDR_OCTETS &&
            memcmp( pAdapterAddress->PhysicalAddress, addr->addr, ETHER_ADDR_OCTETS ) == 0 ) {
            GPTP_LOG_INFO("  ✅ Found matching adapter: %s", pAdapterAddress->AdapterName);
            break;
        }
    }
    
    GPTP_LOG_INFO("Search for MAC address completed. Adapter found: %s", 
                  pAdapterAddress ? "YES" : "NO");

    if( pAdapterAddress != NULL ) {
        strcpy_s( name, PCAP_INTERFACENAMEPREFIX );
        strncpy_s( name+PCAP_INTERFACENAMEPREFIX_LENGTH, PCAP_INTERFACENAMESUFFIX_LENGTH+1, pAdapterAddress->AdapterName, PCAP_INTERFACENAMESUFFIX_LENGTH );
        
        // ✅ Log packet capture backend information for debugging
        GPTP_LOG_INFO("Packet capture backend: %s (modern: %s)\n", 
               PCAP_BACKEND_NAME, 
               PCAP_BACKEND_MODERN ? "yes" : "no");
        GPTP_LOG_INFO("Opening: %s\n", name );
        
        // Enhanced timeout configuration for better packet arrival detection
        int optimized_timeout = timeout;
        if (g_debug_mode) {
            optimized_timeout = DEBUG_TIMEOUT_MS;
            GPTP_LOG_DEBUG("Using debug timeout %dms for enhanced packet detection\n", optimized_timeout);
        } else if (timeout > STANDARD_TIMEOUT_MS) {
            // For direct NIC-to-NIC scenarios, use optimized timeout
            optimized_timeout = OPTIMIZED_TIMEOUT_MS;
            GPTP_LOG_INFO("Using optimized timeout %dms for direct connection\n", optimized_timeout);
        }
        
        GPTP_LOG_INFO("About to open interface: %s", name);
        handle->iface = pcap_open(  name, MAX_FRAME_SIZE, 
                                   PCAP_OPENFLAG_MAX_RESPONSIVENESS | PCAP_OPENFLAG_PROMISCUOUS | PCAP_OPENFLAG_NOCAPTURE_LOCAL,
                                   optimized_timeout, NULL, handle->errbuf );
        if( handle->iface == NULL ) {
            GPTP_LOG_ERROR("pcap_open failed for %s: %s", name, handle->errbuf);
            ret = PACKET_IFLOOKUP_ERROR;
            goto fnexit;
        }
        GPTP_LOG_INFO("Successfully opened interface: %s", name);
        handle->iface_addr = *addr;
    } else {
        ret = PACKET_IFNOTFOUND_ERROR;
        goto fnexit;
    }

fnexit:
    return ret;
}

void closeInterface( struct packet_handle *pfhandle ) {
    if( pfhandle->iface != NULL ) pcap_close( pfhandle->iface );
}


packet_error_t sendFrame( struct packet_handle *handle, packet_addr_t *addr, uint16_t ethertype, uint8_t *payload, size_t length ) {
    packet_error_t ret = PACKET_NO_ERROR;
    uint16_t ethertype_no = PLAT_htons( ethertype );
    uint8_t *payload_ptr = payload;

    if( length < PACKET_HDR_LENGTH ) {
        ret = PACKET_BADBUFFER_ERROR;
    }

    // Build Header
    memcpy( payload_ptr, addr->addr, ETHER_ADDR_OCTETS ); payload_ptr+= ETHER_ADDR_OCTETS;
    memcpy( payload_ptr, handle->iface_addr.addr, ETHER_ADDR_OCTETS ); payload_ptr+= ETHER_ADDR_OCTETS;
    memcpy( payload_ptr, &ethertype_no, sizeof( ethertype_no ));

    if( pcap_sendpacket( handle->iface, payload, (int) length+PACKET_HDR_LENGTH ) != 0 ) {
        ret = PACKET_XMIT_ERROR;
        goto fnexit;
    }

fnexit:
    return ret;
}

packet_error_t packetBind( struct packet_handle *handle, uint16_t ethertype ) {
    packet_error_t ret = PACKET_NO_ERROR;
    char filter_expression[32] = "ether proto 0x";

    snprintf( filter_expression + strlen(filter_expression),
              sizeof(filter_expression) - strlen(filter_expression),
              "%hx", ethertype );
    if( pcap_compile( handle->iface, &handle->filter, filter_expression, 1, 0 ) == -1 ) {
        ret = PACKET_BIND_ERROR;
        goto fnexit;
    }
    if( pcap_setfilter( handle->iface, &handle->filter ) == -1 ) {
        ret = PACKET_BIND_ERROR;
        goto fnexit;
    }

    handle->ethertype = ethertype;

fnexit:
    return ret;
}

// Call to recvFrame must be thread-safe.  However call to pcap_next_ex() isn't because of somewhat undefined memory management semantics.
// Wrap call to pcap library with mutex
packet_error_t recvFrame( struct packet_handle *handle, packet_addr_t *addr, uint8_t *payload, size_t &length ) {
    GPTP_LOG_DEBUG("recvFrame: ENTER (thread_id=%lu, handle=%p)\n", GetCurrentThreadId(), handle);
    packet_error_t ret = PACKET_NO_ERROR;
    struct pcap_pkthdr *hdr_r;
    u_char *data;

    int pcap_result;
    DWORD wait_result;
    
    static int timeout_count = 0;
    static int packet_count = 0;
    static int consecutive_timeouts = 0;
    static int consecutive_errors = 0;
    const int MAX_CONSECUTIVE_TIMEOUTS = 200; // Increased for single-device robustness
    const int MAX_CONSECUTIVE_ERRORS = 20;   // Increased error tolerance
    const int INTERFACE_REOPEN_RETRY_LIMIT = 3; // Max attempts to reopen interface
    static int interface_reopen_attempts = 0;

    // --- Heartbeat update: call on every receive attempt ---
    extern void update_network_thread_heartbeat();
    GPTP_LOG_DEBUG("recvFrame: Calling update_network_thread_heartbeat (thread_id=%lu)\n", GetCurrentThreadId());
    update_network_thread_heartbeat();
    GPTP_LOG_DEBUG("recvFrame: update_network_thread_heartbeat returned(thread_id=%lu)\n", GetCurrentThreadId());
    
    try{
        // --- DEBUG: Before WaitForSingleObject ---
        GPTP_LOG_DEBUG("recvFrame: Before WaitForSingleObject (thread_id=%lu)\n", GetCurrentThreadId());
        wait_result = WaitForSingleObject( handle->capture_lock, 1000 );
        // --- DEBUG: After WaitForSingleObject ---
        GPTP_LOG_DEBUG("recvFrame: After WaitForSingleObject (thread_id=%lu, wait_result=%lu, WAIT_OBJECT_0=%lu, WAIT_TIMEOUT=%lu, WAIT_FAILED=%lu, GetLastError()=%lu)\n", GetCurrentThreadId(), wait_result, WAIT_OBJECT_0, WAIT_TIMEOUT, WAIT_FAILED, GetLastError());
        if( wait_result != WAIT_OBJECT_0 ) {
            GPTP_LOG_DEBUG("recvFrame: Early return - failed to get capture_lock mutex (thread_id=%lu)\n", GetCurrentThreadId());
            ret = PACKET_GETMUTEX_ERROR;
            GPTP_LOG_ERROR("recvFrame: Failed to get capture_lock mutex\n");
            goto fnexit;
        }
    } catch (const std::exception& ex) {
        GPTP_LOG_DEBUG("recvFrame: Exception caught while waiting for mutex: %s (thread_id=%lu)\n", ex.what(), GetCurrentThreadId());
        ret = PACKET_GETMUTEX_ERROR;
        goto fnexit;
    } catch (...) {
        GPTP_LOG_DEBUG("recvFrame: Unknown exception caught while waiting for mutex (thread_id=%lu)\n", GetCurrentThreadId());
        ret = PACKET_GETMUTEX_ERROR;
        goto fnexit;
    }

    try{
        // --- Robust recovery: If iface is NULL, try to re-open before proceeding ---
        if (handle->iface == NULL) {
            GPTP_LOG_DEBUG("recvFrame: Early return - iface is NULL (thread_id=%lu)\n", GetCurrentThreadId());
            GPTP_LOG_ERROR("recvFrame: Interface handle is NULL, attempting to re-open...\n");
            packet_error_t reopen_ret = openInterfaceByAddr(handle, &handle->iface_addr, DEBUG_TIMEOUT_MS);
            if (reopen_ret != PACKET_NO_ERROR) {
                GPTP_LOG_ERROR("recvFrame: Failed to re-open interface (error=%d), sleeping 100ms before retry...\n", reopen_ret);
                ReleaseMutex(handle->capture_lock);
                Sleep(100);
                goto fnexit;
            } else {
                GPTP_LOG_INFO("recvFrame: Interface re-opened successfully after being NULL.\n");
                consecutive_timeouts = 0;
                consecutive_errors = 0;
            }
        }
    }
    catch (const std::exception& ex) {
        GPTP_LOG_DEBUG("recvFrame: Exception caught while checking iface: %s (thread_id=%lu)\n", ex.what(), GetCurrentThreadId());
        ret = PACKET_IFCHECK_ERROR;
        goto fnexit;
    } catch (...) {
        GPTP_LOG_DEBUG("recvFrame: Unknown exception caught while checking iface (thread_id=%lu)\n", GetCurrentThreadId());
        ret = PACKET_IFCHECK_ERROR;
        goto fnexit;
    }

    try{
        // --- DEBUG: Before pcap_next_ex ---
        GPTP_LOG_DEBUG("recvFrame: Before pcap_next_ex (thread_id=%lu)\n", GetCurrentThreadId());
        pcap_result = pcap_next_ex( handle->iface, &hdr_r, (const u_char **) &data );
        // --- DEBUG: After pcap_next_ex ---
        GPTP_LOG_DEBUG("recvFrame: After pcap_next_ex (thread_id=%lu, pcap_result=%d)\n", GetCurrentThreadId(), pcap_result);

        if( pcap_result == 0 ) {
            GPTP_LOG_DEBUG("recvFrame: Early return - pcap_result == 0 (thread_id=%lu)\n", GetCurrentThreadId());
            ret = PACKET_RECVTIMEOUT_ERROR;
            timeout_count++;
            consecutive_timeouts++;
            consecutive_errors = 0;
            GPTP_LOG_DEBUG("recvFrame: Timeout occurred (total timeouts: %d, consecutive: %d)\n", timeout_count, consecutive_timeouts);
            
            // Provide helpful information for single-device scenarios
            if (consecutive_timeouts == 100) {
                GPTP_LOG_INFO("*** No gPTP packets received after 100 timeouts - this is normal in single-device test scenarios ***");
                GPTP_LOG_INFO("*** gPTP will continue running and automatically detect peers when they connect ***");
            } else if (consecutive_timeouts % 100 == 0 && consecutive_timeouts > 100) {
                GPTP_LOG_INFO("*** Still waiting for gPTP peers (timeouts: %d) - continuing to monitor network ***", consecutive_timeouts);
            }
            if (consecutive_timeouts >= MAX_CONSECUTIVE_TIMEOUTS) {
                GPTP_LOG_ERROR("recvFrame: Too many consecutive timeouts (%d), closing and reopening interface!\n", consecutive_timeouts);
                closeInterface(handle);
                packet_error_t reopen_ret = openInterfaceByAddr(handle, &handle->iface_addr, DEBUG_TIMEOUT_MS);
                if (reopen_ret != PACKET_NO_ERROR) {
                    interface_reopen_attempts++;
                    GPTP_LOG_ERROR("recvFrame: Failed to re-open interface after timeout (error=%d, attempt %d/%d), sleeping 100ms before retry...\n", 
                                   reopen_ret, interface_reopen_attempts, INTERFACE_REOPEN_RETRY_LIMIT);
                    ReleaseMutex(handle->capture_lock);
                    Sleep(100);
                    
                    // If we've reached the retry limit, treat as fatal
                    if (interface_reopen_attempts >= INTERFACE_REOPEN_RETRY_LIMIT) {
                        GPTP_LOG_ERROR("recvFrame: Interface reopen attempts exhausted (%d/%d) - treating as fatal network error", 
                                       interface_reopen_attempts, INTERFACE_REOPEN_RETRY_LIMIT);
                        ret = PACKET_RECVFAILED_ERROR;
                        goto fnexit;
                    }
                    
                    // Otherwise, just return timeout and try again next time
                    ret = PACKET_RECVTIMEOUT_ERROR;
                    goto fnexit;
                } else {
                    GPTP_LOG_INFO("recvFrame: Interface re-opened successfully after timeout.\n");
                    consecutive_timeouts = 0;
                    consecutive_errors = 0;
                    interface_reopen_attempts = 0; // Reset attempts on success
                }
            }
        } else if( pcap_result < 0 ) {
            GPTP_LOG_DEBUG("recvFrame: Early return - pcap_result < 0 (thread_id=%lu)\n", GetCurrentThreadId());
            ret = PACKET_RECVFAILED_ERROR;
            consecutive_errors++;
            consecutive_timeouts = 0;
            GPTP_LOG_ERROR("recvFrame: pcap_next_ex failed with result %d (consecutive errors: %d)\n", pcap_result, consecutive_errors);
            if (consecutive_errors >= MAX_CONSECUTIVE_ERRORS) {
                GPTP_LOG_ERROR("recvFrame: Too many consecutive errors (%d), closing and reopening interface!\n", consecutive_errors);
                closeInterface(handle);
                packet_error_t reopen_ret = openInterfaceByAddr(handle, &handle->iface_addr, DEBUG_TIMEOUT_MS);
                if (reopen_ret != PACKET_NO_ERROR) {
                    interface_reopen_attempts++;
                    GPTP_LOG_ERROR("recvFrame: Failed to re-open interface after error (error=%d, attempt %d/%d), sleeping 100ms before retry...\n", 
                                   reopen_ret, interface_reopen_attempts, INTERFACE_REOPEN_RETRY_LIMIT);
                    ReleaseMutex(handle->capture_lock);
                    Sleep(100);
                    
                    // If we've reached the retry limit, treat as fatal
                    if (interface_reopen_attempts >= INTERFACE_REOPEN_RETRY_LIMIT) {
                        GPTP_LOG_ERROR("recvFrame: Interface reopen attempts exhausted (%d/%d) - treating as fatal network error", 
                                       interface_reopen_attempts, INTERFACE_REOPEN_RETRY_LIMIT);
                        ret = PACKET_RECVFAILED_ERROR;
                        goto fnexit;
                    }
                    
                    // Otherwise, just return error and try again next time
                    ret = PACKET_RECVFAILED_ERROR;
                    goto fnexit;
                } else {
                    GPTP_LOG_INFO("recvFrame: Interface re-opened successfully after error.\n");
                    consecutive_timeouts = 0;
                    consecutive_errors = 0;
                    interface_reopen_attempts = 0; // Reset attempts on success
                }
            }
        } else {
            packet_count++;
            total_packet_count++;
            consecutive_timeouts = 0;
            consecutive_errors = 0;
            interface_reopen_attempts = 0; // Reset attempts when we receive packets
            if (g_debug_mode) {
                GPTP_LOG_DEBUG("Packet #%d received, size=%d bytes\n", total_packet_count, hdr_r->len);
                
                // Check if it's a PTP packet
                if (hdr_r->len >= 14 && data) {
                    uint16_t ethertype = (data[12] << 8) | data[13];
                    if (ethertype == 0x88F7) {
                        ptp_packet_count++;
                        GPTP_LOG_DEBUG("*** PTP PACKET #%d DETECTED *** Type=0x88F7, Size=%d\n", ptp_packet_count, hdr_r->len);
                        if (hdr_r->len >= 15) {
                            uint8_t messageType = data[14] & 0x0F;
                            uint8_t transportSpecific = (data[14] & 0xF0) >> 4;
                            GPTP_LOG_DEBUG("PTP Message Type=%d, Transport=%d, Total PTP: %u of %u packets\n", 
                                messageType, transportSpecific, ptp_packet_count, total_packet_count);
                        }
                    } else if (total_packet_count % 50 == 1) {
                        GPTP_LOG_DEBUG("Non-PTP packet #%d, EtherType=0x%04X\n", total_packet_count, ethertype);
                    }
                }
            }
            
            length = hdr_r->len-PACKET_HDR_LENGTH >= length ? length : hdr_r->len-PACKET_HDR_LENGTH;
            memcpy( payload, data+PACKET_HDR_LENGTH, length );
            memcpy( addr->addr, data+ETHER_ADDR_OCTETS, ETHER_ADDR_OCTETS );
        }
    }
    catch (const std::exception& ex) {
        GPTP_LOG_DEBUG("recvFrame: Exception caught while processing packet: %s (thread_id=%lu)\n", ex.what(), GetCurrentThreadId());
        ret = PACKET_PROCESSING_ERROR;
        goto fnexit;
    } catch (...) {
        GPTP_LOG_DEBUG("recvFrame: Unknown exception caught while processing packet (thread_id=%lu)\n", GetCurrentThreadId());
        ret = PACKET_PROCESSING_ERROR;
        goto fnexit;
    }
    if( !ReleaseMutex( handle->capture_lock )) {
        GPTP_LOG_DEBUG("recvFrame: Early return - failed to release capture_lock mutex (thread_id=%lu)\n", GetCurrentThreadId());
        ret = PACKET_RLSMUTEX_ERROR;
        GPTP_LOG_ERROR("recvFrame: Failed to release capture_lock mutex\n");
        goto fnexit;
    }

    GPTP_LOG_DEBUG("recvFrame: EXIT (thread_id=%lu, ret=%d, length=%zu)\n", GetCurrentThreadId(), ret, length);
fnexit:
    GPTP_LOG_DEBUG("recvFrame: EXIT (thread_id=%lu, ret=%d)\n", GetCurrentThreadId(), ret);
    return ret;
}
