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

#ifndef WINDOWSIPC_HPP
#define WINDOWSIPC_HPP

#include <windows.h>
#include <stdint.h>
#include <minwinbase.h>
#include <gptp_log.hpp>

#include "ipcdef.hpp"

#define OUTSTANDING_MESSAGES 10		/*!< Number of outstanding messages on named pipe declaration*/

#define PIPE_PREFIX "\\\\.\\pipe\\"		/*!< PIPE name prefix */
#define P802_1AS_PIPENAME "gptp-ctrl"	/*!< PIPE group name */

#pragma pack(push,1)

/**
 * @brief Enumeration named pipe message type. Possible values are:
 *  - BASE_MSG;
 *  - CTRL_MSG;
 *  - QUERY_MSG;
 *  - OFFSET_MSG;
 */
typedef enum { BASE_MSG = 0, CTRL_MSG, QUERY_MSG, OFFSET_MSG } NPIPE_MSG_TYPE;

/**
 * @brief Provides a windows named pipe interface
 */
class WindowsNPipeMessage {
    protected:
        DWORD sz;				/*!< Size */
        NPIPE_MSG_TYPE type;	/*!< Message type as NPIPE_MSG_TYPE*/

        OVERLAPPED ol_read;		/*!< Overlapped read*/
        DWORD ol_read_req;		/*!< Overlapped read request */
    public:
        /**
         * @brief  Initializes the interface
         * @return void
         */
        void init() {
            sz = sizeof( WindowsNPipeMessage );
        }
        /**
         * @brief  Writes to the named pipe
         * @param  pipe Pipe handler
         * @return TRUE in case of success, FALSE in case of error.
         */
        bool write( HANDLE pipe ) {
            DWORD bytes_written;
            DWORD last_error = ERROR_SUCCESS;
            if( sz == 0 ) return false;
            if( WriteFile( pipe, this, sz, &bytes_written, NULL ) == 0 ) {
                last_error = GetLastError();
            }
            if( last_error == ERROR_SUCCESS || last_error == ERROR_PIPE_LISTENING ) {
                return true;
            }
            GPTP_LOG_ERROR( "Failed to write to named pipe: %u", last_error );
            return false;
        }
        /**
         * @brief  Reads from the pipe
         * @param  pipe Pipe handle
         * @param  offs base offset
         * @param  event Event handler
         * @return -1 if error, 0 if ERROR_IO_PENDING or bytes_read + offs
         */
        long read_ol( HANDLE pipe, long offs, HANDLE event ) {
            DWORD bytes_read;
            long sz_l = (long) sz;
            LPOVERLAPPED lol;
            if( sz_l - offs <  0 || sz_l == 0 ) return -1;
            if( sz_l - offs == 0 ) return offs;
            ol_read_req = sz_l-offs;
            if( event != NULL ) {
                memset( &ol_read, 0, sizeof( ol_read ));
                ol_read.hEvent = event;
                lol = &ol_read;
            } else {
                lol = NULL;
            }
            if( ReadFile( pipe, ((char *)this)+offs, ol_read_req, &bytes_read, lol ) == 0 ) {
                int err = GetLastError();
                if( err != ERROR_IO_PENDING ) {
                    GPTP_LOG_ERROR( "Failed to read %d bytes from named pipe: %u", ol_read_req, err );
                }
                return err == ERROR_IO_PENDING ? 0 : -1;
            }
            return (bytes_read == sz_l-offs) ? offs+bytes_read : -1;
        }
        /**
         * @brief  Reads completely the overlapped result
         * @param  pipe Pipe handler
         * @return bytes read in case of success, -1 in case of error
         * @todo Its not clear what GetOverlappedResult does
         */
        long read_ol_complete( HANDLE pipe ) {
            DWORD bytes_read;
            if( GetOverlappedResult( pipe, &ol_read, &bytes_read, false ) == 0 ) {
                return -1;
            }
            return bytes_read;
        }
        /**
         * @brief  Reads from the pipe
         * @param  pipe Pipe handler
         * @param  offs base offset
         * @return -1 if error, 0 if ERROR_IO_PENDING or bytes_read + offs
         */
        long read( HANDLE pipe, long offs ) {
            return read_ol( pipe, offs, NULL );
        }
        /**
         * @brief  Gets pipe message type
         * @return ::NPIPE_MSG_TYPE
         */
        NPIPE_MSG_TYPE getType() { return type; }
};

#ifndef PTP_CLOCK_IDENTITY_LENGTH
#define PTP_CLOCK_IDENTITY_LENGTH 8		/*!< Size of a clock identifier stored in the ClockIndentity class, described at IEEE 802.1AS-2011 Clause 8.5.2.4*/
#endif

/**
 * @brief Provides an interface for the phase/frequency offsets
 */
class Offset {
    public:
        int64_t ml_phoffset;                                    //!< Master to local phase offset
        FrequencyRatio ml_freqoffset;                           //!< Master to local frequency offset
        int64_t ls_phoffset;                                    //!< Local to system phase offset
        FrequencyRatio ls_freqoffset;                           //!< Local to system frequency offset
        uint64_t local_time;                                    //!< Local time

        /* Current grandmaster information */
        /* Referenced by the IEEE Std 1722.1-2013 AVDECC Discovery Protocol Data Unit (ADPDU) */
        uint8_t gptp_grandmaster_id[PTP_CLOCK_IDENTITY_LENGTH]; //!< Current grandmaster id (all 0's if no grandmaster selected)
        uint8_t gptp_domain_number;                             //!< gPTP domain number

        /* Grandmaster support for the network interface */
        /* Referenced by the IEEE Std 1722.1-2013 AVDECC AVB_INTERFACE descriptor */
        uint8_t  clock_identity[PTP_CLOCK_IDENTITY_LENGTH];     //!< The clock identity of the interface
        uint8_t  priority1;                                     //!< The priority1 field of the grandmaster functionality of the interface, or 0xFF if not supported
        uint8_t  clock_class;                                   //!< The clockClass field of the grandmaster functionality of the interface, or 0xFF if not supported
        uint16_t offset_scaled_log_variance;                    //!< The offsetScaledLogVariance field of the grandmaster functionality of the interface, or 0x0000 if not supported
        uint8_t  clock_accuracy;                                //!< The clockAccuracy field of the grandmaster functionality of the interface, or 0xFF if not supported
        uint8_t  priority2;                                     //!< The priority2 field of the grandmaster functionality of the interface, or 0xFF if not supported
        uint8_t  domain_number;                                 //!< The domainNumber field of the grandmaster functionality of the interface, or 0 if not supported
        int8_t   log_sync_interval;                             //!< The currentLogSyncInterval field of the grandmaster functionality of the interface, or 0 if not supported
        int8_t   log_announce_interval;                         //!< The currentLogAnnounceInterval field of the grandmaster functionality of the interface, or 0 if not supported
        int8_t   log_pdelay_interval;                           //!< The currentLogPDelayReqInterval field of the grandmaster functionality of the interface, or 0 if not supported
        uint16_t port_number;                                   //!< The portNumber field of the interface, or 0x0000 if not supported
};

/**
 * @brief Provides an interface to update the Offset information
 */
class WinNPipeOffsetUpdateMessage : public WindowsNPipeMessage {
    private:
        Offset offset;
    public:
        /**
         * @brief  Initializes interface
         * @return void
         */
        void _init() {
            sz = sizeof(WinNPipeOffsetUpdateMessage);
            type = OFFSET_MSG;
        }
        /**
         * @brief  Initializes interface and clears the Offset message
         * @return void
         */
        void init() {
            _init();
            memset( &this->offset, 0, sizeof( this->offset ));
        }
        /**
         * @brief  Initializes the interface based on the Offset structure
         * @param  offset [in] Offset structure
         * @return void
         */
        void init( Offset *offset ) {
            _init();
            this->offset = *offset;
        }
        /**
         * @brief  Gets master to local phase offset
         * @return master to local phase offset
         */
        int64_t getMasterLocalOffset() { return offset.ml_phoffset; }
        /**
         * @brief  Gets Master to local frequency offset
         * @return Master to local frequency offset
         */
        FrequencyRatio getMasterLocalFreqOffset() { return offset.ml_freqoffset; }
        /**
         * @brief  Gets local to system phase offset
         * @return local to system phase offset
         */
        int64_t getLocalSystemOffset() { return offset.ls_phoffset; }
        /**
         * @brief  Gets Local to system frequency offset
         * @return Local to system frequency offset
         */
        FrequencyRatio getLocalSystemFreqOffset() { return offset.ls_freqoffset; }
        /**
         * @brief  Gets local time
         * @return Local time
         */
        uint64_t getLocalTime() { return offset.local_time; }
};

/**
 * @brief Enumeration CtrlWhich. It can assume the following values:
 *  - ADD_PEER;
 *  - REMOVE_PEER;
 */
typedef enum { ADD_PEER, REMOVE_PEER } CtrlWhich;
/**
 * @brief Enumeration AddrWhich. It can assume the following values:
 *  - MAC_ADDR;
 *  - IP_ADDR;
 *  - INVALID_ADDR;
 */
typedef enum { MAC_ADDR, IP_ADDR, INVALID_ADDR } AddrWhich;

/**
 * @brief Provides an interface for Peer addresses
 */
class PeerAddr {
    public:
        AddrWhich which;	/*!< Peer address */
        /**
         * @brief shared memory between mac and ip addresses
         */
        union {
            uint8_t mac[ETHER_ADDR_OCTETS];	/*!< Link Layer address */
            uint8_t ip[IP_ADDR_OCTETS];		/*!< IP Address */
        };
        /**
         * @brief  Implements the operator '==' overloading method.
         * @param  other [in] Reference to the peer addresses
         * @return TRUE if mac or ip are the same. FALSE otherwise.
         */
        bool operator==(const PeerAddr &other) const {
            int result;
            switch( which ) {
                case MAC_ADDR:
                    result = memcmp( &other.mac, &mac, ETHER_ADDR_OCTETS );
                    break;
                case IP_ADDR:
                    result = memcmp( &other.ip, &ip, IP_ADDR_OCTETS );
                    break;
                default:
                    result = -1; // != 0
                    break;
            }
            return (result == 0) ? true : false;
        }
        /**
         * @brief  Implements the operator '<' overloading method.
         * @param  other Reference to the peer addresses to be compared.
         * @return TRUE if mac or ip address from the object is lower than the peer's.
         */
        bool operator<(const PeerAddr &other) const {
            int result;
            switch( which ) {
                case MAC_ADDR:
                    result = memcmp( &other.mac, &mac, ETHER_ADDR_OCTETS );
                    break;
                case IP_ADDR:
                    result = memcmp( &other.ip, &ip, IP_ADDR_OCTETS );
                    break;
                default:
                    result = 1; // > 0
                    break;
            }
            return (result < 0) ? true : false;
        }
};

/**
 * @brief Provides an interface for named pipe control messages
 */
class WinNPipeCtrlMessage : public WindowsNPipeMessage {
    private:
        CtrlWhich which;
        PeerAddr addr;
        uint16_t flags;
    public:
        /**
         * @brief  Initializes interface's internal variables.
         * @return void
         */
        void init() {
            sz = sizeof(WinNPipeCtrlMessage);
            type = CTRL_MSG;
        }
        /**
         * @brief  Initializes Interface's internal variables and sets
         * control and addresses values
         * @param  which ::CtrlWhich enumeration
         * @param  addr Peer addresses
         * @return void
         */
        void init( CtrlWhich which, PeerAddr addr ) {
            init();
            this->which = which;
            this->addr = addr;
            flags = 0;
        }
        /**
         * @brief  Gets peer addresses
         * @return PeerAddr structure
         */
        PeerAddr getPeerAddr() { return addr; }
        /**
         * @brief  Sets peer address
         * @param  addr PeerAddr to set
         * @return void
         */
        void setPeerAddr( PeerAddr addr ) { this->addr = addr; }
        /**
         * @brief  Gets control type
         * @return ::CtrlWhich type
         */
        CtrlWhich getCtrlWhich() { return which; }
        /**
         * @brief  Sets control message type
         * @param  which ::CtrlWhich message
         * @return void
         */
        void setCtrlWhich( CtrlWhich which ) { this->which = which; }
        /**
         * @brief  Gets internal flags
         * @return Internal flags
         * @todo What are these flags used for? Apparently its not in use.
         */
        uint16_t getFlags() { return flags; }
};

/**
 * @brief WindowsNPipeQueryMessage is sent from the client to gPTP daemon to query the
 * offset of type ::NPIPE_MSG_TYPE.  The daemon sends WindowsNPipeMessage in response.
 * Currently there is no data associated with this message.
 */
class WinNPipeQueryMessage : public WindowsNPipeMessage {
    public:
        /**
         * @brief  Initializes the interface
         * @return void
         */
        void init() { type = OFFSET_MSG; sz = sizeof(*this); }
};

/**
 * @brief Provides the client's named pipe interface
 * @todo Not in use and should be removed.
 */
typedef union {
    WinNPipeCtrlMessage a;	/*!< Control message */
    WinNPipeQueryMessage b;	/*!< Query message */
} WindowsNPipeMsgClient;

/**
 * @brief Provides the server's named pipe interface
 * @todo Not in use and should be removed.
 */
typedef union {
    WinNPipeOffsetUpdateMessage a;	/*!< Offset update message */
} WindowsNPipeMsgServer;

#define NPIPE_MAX_CLIENT_MSG_SZ (sizeof( WindowsNPipeMsgClient )) /*!< Maximum message size for the client */
#define NPIPE_MAX_SERVER_MSG_SZ (sizeof( WindowsNPipeMsgServer ))	/*!< Maximum message size for the server */
#define NPIPE_MAX_MSG_SZ (NPIPE_MAX_CLIENT_MSG_SZ > NPIPE_MAX_SERVER_MSG_SZ ? NPIPE_MAX_CLIENT_MSG_SZ : NPIPE_MAX_SERVER_MSG_SZ)	/*!< Maximum message size */

#pragma pack(pop)

#endif /*WINDOWSIPC_HPP*/

