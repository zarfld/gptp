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


#ifndef PACKET_H
#define PACKET_H

#include "stdint.h"

/**@file*/

#define ETHER_ADDR_OCTETS 6		/*!< Link layer number of octets */
#define PACKET_HDR_LENGTH 14	/*!< Packet header length in bytes */

/**
 * @brief Packet error enumeration. Possible values are:
 * 	- PACKET_NO_ERROR;
 * 	- PACKET_NOMEMORY_ERROR;
 * 	- PACKET_BADBUFFER_ERROR;
 * 	- PACKET_XMIT_ERROR;
 * 	- PACKET_IFLOOKUP_ERROR;
 * 	- PACKET_IFNOTFOUND_ERROR;
 * 	- PACKET_CREATEMUTEX_ERROR;
 * 	- PACKET_GETMUTEX_ERROR;
 * 	- PACKET_RLSMUTEX_ERROR;
 * 	- PACKET_RECVTIMEOUT_ERROR;
 * 	- PACKET_RECVFAILED_ERROR;
 * 	- PACKET_BIND_ERROR;
 */
typedef enum {
    PACKET_NO_ERROR = 0, PACKET_NOMEMORY_ERROR, PACKET_BADBUFFER_ERROR, PACKET_XMIT_ERROR, PACKET_IFLOOKUP_ERROR, PACKET_IFNOTFOUND_ERROR,
    PACKET_CREATEMUTEX_ERROR, PACKET_GETMUTEX_ERROR, PACKET_RLSMUTEX_ERROR, PACKET_RECVTIMEOUT_ERROR, PACKET_RECVFAILED_ERROR,
    PACKET_BIND_ERROR, PACKET_IFCHECK_ERROR,PACKET_PROCESSING_ERROR
} packet_error_t; 

/**
 * @brief type to ethernet address
 */
typedef struct { 
	uint8_t addr[ETHER_ADDR_OCTETS];	/*!< Link layer address*/
} packet_addr_t;
/**
 * @brief Type to packet handle
 */
typedef struct packet_handle * pfhandle_t;

/**
 * @brief Allocate memory for the packet handle
 * @param pfhandle_r [inout] Packet handle type
 * @return void
 */
packet_error_t mallocPacketHandle( pfhandle_t *pfhandle_r );
/**
 * @brief  Close the packet handle
 * @param  pfhandle [in] Packet handler
 * @return void
 */
void freePacketHandle( pfhandle_t pfhandle );

/**
 * @brief  Opens the interface by link layer address. Uses libpcap.
 * @param  pfhandle [in] Packet interface handler
 * @param  addr Link layer address
 * @param timeout Timeout
 * @return Type packet_error_t with the error code
 */
packet_error_t openInterfaceByAddr( pfhandle_t pfhandle, packet_addr_t *addr, int timeout );
/**
 * @brief  Close the interface
 * @param  pfhandle [in] Packet interface handler
 * @return void
 */
void closeInterface( pfhandle_t pfhandle );
/**
 * @brief  Sends a frame through an interface using libpcap
 * @param  pfhandle Packet Inteface handler
 * @param  addr [in] Link layer address
 * @param  ethertype Ethertype
 * @param  payload [in] Data buffer
 * @param  length Data length
 * @return packet_error_t error codes
 */
packet_error_t sendFrame( pfhandle_t pfhandle, packet_addr_t *addr, uint16_t ethertype, uint8_t *payload, size_t length );
/**
 * @brief  Receives a frame
 * @param  pfhandle Packet interface handler
 * @param  addr Link layer address
 * @param  payload [out] Data buffer
 * @param  length [out] Data length
 * @return packet_error_t error codes
 */
packet_error_t recvFrame( pfhandle_t pfhandle, packet_addr_t *addr,                     uint8_t *payload, size_t &length );

/**
 * @brief  Set filters for packet handler
 * @param  handle [in] packet interface handler
 * @param  ethertype Ethertype
 * @return packet_error_t error codes
 */
packet_error_t packetBind( struct packet_handle *handle, uint16_t ethertype );

/**
 * @brief  Enables enhanced debug logging for packet reception
 * @param  enable [in] true to enable debug mode, false to disable
 * @return void
 */
void enablePacketReceptionDebug(bool enable);

/**
 * @brief  Runs comprehensive packet reception test for debugging
 * @param  interface_name [in] Interface name to test
 * @return true if packets received successfully
 */
bool runPacketReceptionTest(const char* interface_name);

#endif /* PACKET_H */
