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

#include <ieee1588.hpp>
#include <avbts_clock.hpp>
#include <avbts_message.hpp>
#include <ether_port.hpp>
#include <avbts_ostimer.hpp>
#include <ether_tstamper.hpp>

#include <stdio.h>
#include <string.h>
#include <math.h>

PTPMessageCommon::PTPMessageCommon( CommonPort *port )
{
	// Fill in fields using port/clock dataset as a template
	versionPTP = GPTP_VERSION;
	versionNetwork = PTP_NETWORK_VERSION;
	domainNumber = port->getClock()->getDomain();
	// Set flags as necessary
	memset(flags, 0, PTP_FLAGS_LENGTH);
	flags[PTP_PTPTIMESCALE_BYTE] |= (0x1 << PTP_PTPTIMESCALE_BIT);
	correctionField = 0;
	_gc = false;
	sourcePortIdentity = new PortIdentity();

	return;
}

/* Determine whether the message was sent by given communication technology,
   uuid, and port id fields */
bool PTPMessageCommon::isSenderEqual(PortIdentity portIdentity)
{
	return portIdentity == *sourcePortIdentity;
}

PTPMessageCommon *buildPTPMessage
( char *buf, int size, LinkLayerAddress *remote,
  CommonPort *port )
{
	OSTimer *timer = port->getTimerFactory()->createTimer();
	PTPMessageCommon *msg = NULL;
	PTPMessageId messageId;
	MessageType messageType;
	unsigned char tspec_msg_t = 0;
	unsigned char transportSpecific = 0;

	uint16_t sequenceId;
	PortIdentity *sourcePortIdentity;
	Timestamp timestamp(0, 0, 0);
	unsigned counter_value = 0;
	EtherPort *eport = NULL;

#if PTP_DEBUG
	{
		int i;
		GPTP_LOG_VERBOSE("Packet Dump:\n");
		for (i = 0; i < size; ++i) {
			GPTP_LOG_VERBOSE("%hhx\t", buf[i]);
			if (i % 8 == 7)
				GPTP_LOG_VERBOSE("\n");
		}
		if (i % 8 != 0)
			GPTP_LOG_VERBOSE("\n");
	}
#endif

	memcpy(&tspec_msg_t,
	       buf + PTP_COMMON_HDR_TRANSSPEC_MSGTYPE(PTP_COMMON_HDR_OFFSET),
	       sizeof(tspec_msg_t));
	messageType = (MessageType) (tspec_msg_t & 0xF);
	transportSpecific = (tspec_msg_t >> 4) & 0x0F;

	sourcePortIdentity = new PortIdentity
		((uint8_t *)
		 (buf + PTP_COMMON_HDR_SOURCE_CLOCK_ID
		  (PTP_COMMON_HDR_OFFSET)),
		 (uint16_t *)
		 (buf + PTP_COMMON_HDR_SOURCE_PORT_ID
		  (PTP_COMMON_HDR_OFFSET)));

	memcpy
		(&(sequenceId),
		 buf + PTP_COMMON_HDR_SEQUENCE_ID(PTP_COMMON_HDR_OFFSET),
		 sizeof(sequenceId));
	sequenceId = PLAT_ntohs(sequenceId);

	GPTP_LOG_VERBOSE("Captured Sequence Id: %u", sequenceId);
	messageId.setMessageType(messageType);
	messageId.setSequenceId(sequenceId);


	if (!(messageType >> 3)) {
		int iter = 5;
		long req = 4000;	// = 1 ms

		eport = dynamic_cast <EtherPort *> ( port );
		if (eport == NULL)
		{
			GPTP_LOG_ERROR
				( "Received Event Message, but port type "
				  "doesn't support timestamping\n" );
			goto abort;
		}

		int ts_good =
		    eport->getRxTimestamp
			(sourcePortIdentity, messageId, timestamp, counter_value, false);
		while (ts_good != GPTP_EC_SUCCESS && iter-- != 0) {
			// Waits at least 1 time slice regardless of size of 'req'
			timer->sleep(req);
			if (ts_good != GPTP_EC_EAGAIN)
				GPTP_LOG_ERROR(
					"Error (RX) timestamping RX event packet (Retrying), error=%d",
					  ts_good );
			ts_good =
			    eport->getRxTimestamp(sourcePortIdentity, messageId,
						 timestamp, counter_value,
						 iter == 0);
			req *= 2;
		}
		if (ts_good != GPTP_EC_SUCCESS) {
			char err_msg[HWTIMESTAMPER_EXTENDED_MESSAGE_SIZE];
			port->getExtendedError(err_msg);
			GPTP_LOG_ERROR
			    ("*** Received an event packet but cannot retrieve timestamp, discarding. messageType=%u,error=%d\t%s",
			     messageType, ts_good, err_msg);
			//_exit(-1);
			goto abort;
		}

		else {
			GPTP_LOG_VERBOSE("Timestamping event packet");
		}

	}

	if (1 != transportSpecific) {
		GPTP_LOG_EXCEPTION("*** Received message with unsupported transportSpecific type=%d", transportSpecific);
		goto abort;
	}
 
	uint8_t clock_id_str[8];
	uint16_t port_num;
	sourcePortIdentity->getClockIdentityString(clock_id_str);
	sourcePortIdentity->getPortNumber(&port_num);
	GPTP_LOG_DEBUG("Received message type %d, sequenceId %u, sourcePortIdentity %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%u",
		messageType, sequenceId, 
		clock_id_str[0], clock_id_str[1], clock_id_str[2], clock_id_str[3],
		clock_id_str[4], clock_id_str[5], clock_id_str[6], clock_id_str[7], port_num);
	switch (messageType) {
	case SYNC_MESSAGE:

		GPTP_LOG_DEBUG("*** Received Sync message" );
		GPTP_LOG_VERBOSE("Sync RX timestamp = %hu,%u,%u", timestamp.seconds_ms, timestamp.seconds_ls, timestamp.nanoseconds );

		// Be sure buffer is the correction size
		if (size < PTP_COMMON_HDR_LENGTH + PTP_SYNC_LENGTH) {
			goto abort;
		}
		{
			PTPMessageSync *sync_msg = new PTPMessageSync();
			sync_msg->messageType = messageType;
			// Copy in v2 sync specific fields
			memcpy(&(sync_msg->originTimestamp.seconds_ms),
			       buf + PTP_SYNC_SEC_MS(PTP_SYNC_OFFSET),
			       sizeof(sync_msg->originTimestamp.seconds_ms));
			memcpy(&(sync_msg->originTimestamp.seconds_ls),
			       buf + PTP_SYNC_SEC_LS(PTP_SYNC_OFFSET),
			       sizeof(sync_msg->originTimestamp.seconds_ls));
			memcpy(&(sync_msg->originTimestamp.nanoseconds),
			       buf + PTP_SYNC_NSEC(PTP_SYNC_OFFSET),
			       sizeof(sync_msg->originTimestamp.nanoseconds));
			msg = sync_msg;
		}
		break;
	case FOLLOWUP_MESSAGE:

		GPTP_LOG_DEBUG("*** Received Follow Up message");

		// Be sure buffer is the correction size
		if (size < (int)(PTP_COMMON_HDR_LENGTH + PTP_FOLLOWUP_LENGTH + sizeof(FollowUpTLV))) {
			goto abort;
		}
		{
			PTPMessageFollowUp *followup_msg =
			    new PTPMessageFollowUp();
			followup_msg->messageType = messageType;
			// Copy in v2 sync specific fields
			memcpy(&
			       (followup_msg->
				preciseOriginTimestamp.seconds_ms),
			       buf + PTP_FOLLOWUP_SEC_MS(PTP_FOLLOWUP_OFFSET),
			       sizeof(followup_msg->
				      preciseOriginTimestamp.seconds_ms));
			memcpy(&
			       (followup_msg->
				preciseOriginTimestamp.seconds_ls),
			       buf + PTP_FOLLOWUP_SEC_LS(PTP_FOLLOWUP_OFFSET),
			       sizeof(followup_msg->
				      preciseOriginTimestamp.seconds_ls));
			memcpy(&
			       (followup_msg->
				preciseOriginTimestamp.nanoseconds),
			       buf + PTP_FOLLOWUP_NSEC(PTP_FOLLOWUP_OFFSET),
			       sizeof(followup_msg->
				      preciseOriginTimestamp.nanoseconds));

			followup_msg->preciseOriginTimestamp.seconds_ms =
			    PLAT_ntohs(followup_msg->
				       preciseOriginTimestamp.seconds_ms);
			followup_msg->preciseOriginTimestamp.seconds_ls =
			    PLAT_ntohl(followup_msg->
				       preciseOriginTimestamp.seconds_ls);
			followup_msg->preciseOriginTimestamp.nanoseconds =
			    PLAT_ntohl(followup_msg->
				       preciseOriginTimestamp.nanoseconds);

			memcpy( &(followup_msg->tlv),
				buf+PTP_FOLLOWUP_OFFSET+PTP_FOLLOWUP_LENGTH,
				sizeof(followup_msg->tlv) );

			msg = followup_msg;
        }

		break;
	case PATH_DELAY_REQ_MESSAGE:

		GPTP_LOG_DEBUG("*** Received PDelay Request message");

		// Be sure buffer is the correction size
		if (size < PTP_COMMON_HDR_LENGTH + PTP_PDELAY_REQ_LENGTH
		    && /* For Broadcom compatibility */ size != 46) {
			goto abort;
		}
		{
			PTPMessagePathDelayReq *pdelay_req_msg =
			    new PTPMessagePathDelayReq();
			pdelay_req_msg->messageType = messageType;

#if 0
            /*TODO: Do we need the code below? Can we remove it?*/
			// The origin timestamp for PDelay Request packets has been eliminated since it is unused
			// Copy in v2 PDelay Request specific fields
			memcpy(&(pdelay_req_msg->originTimestamp.seconds_ms),
			       buf +
			       PTP_PDELAY_REQ_SEC_MS(PTP_PDELAY_REQ_OFFSET),
			       sizeof(pdelay_req_msg->
				      originTimestamp.seconds_ms));
			memcpy(&(pdelay_req_msg->originTimestamp.seconds_ls),
			       buf +
			       PTP_PDELAY_REQ_SEC_LS(PTP_PDELAY_REQ_OFFSET),
			       sizeof(pdelay_req_msg->
				      originTimestamp.seconds_ls));
			memcpy(&(pdelay_req_msg->originTimestamp.nanoseconds),
			       buf + PTP_PDELAY_REQ_NSEC(PTP_PDELAY_REQ_OFFSET),
			       sizeof(pdelay_req_msg->
				      originTimestamp.nanoseconds));

			pdelay_req_msg->originTimestamp.seconds_ms =
			    PLAT_ntohs(pdelay_req_msg->
				       originTimestamp.seconds_ms);
			pdelay_req_msg->originTimestamp.seconds_ls =
			    PLAT_ntohl(pdelay_req_msg->
				       originTimestamp.seconds_ls);
			pdelay_req_msg->originTimestamp.nanoseconds =
			    PLAT_ntohl(pdelay_req_msg->
				       originTimestamp.nanoseconds);
#endif

			msg = pdelay_req_msg;
		}
		break;
	case PATH_DELAY_RESP_MESSAGE:

		GPTP_LOG_DEBUG("*** Received PDelay Response message, Timestamp %u (sec) %u (ns), seqID %u",
			   timestamp.seconds_ls, timestamp.nanoseconds,
			   sequenceId);
		GPTP_LOG_STATUS("*** PDELAY RESPONSE RECV DEBUG: Received PDelay Response seq=%u, RX timestamp: %u.%09u", 
			sequenceId, timestamp.seconds_ls, timestamp.nanoseconds);

		// Be sure buffer is the correction size
		if (size < PTP_COMMON_HDR_LENGTH + PTP_PDELAY_RESP_LENGTH) {
			GPTP_LOG_ERROR("*** PDELAY RESPONSE RECV DEBUG: FAILED - buffer too small (%d < %d)", 
				size, PTP_COMMON_HDR_LENGTH + PTP_PDELAY_RESP_LENGTH);
			goto abort;
		}
		{
			PTPMessagePathDelayResp *pdelay_resp_msg =
			    new PTPMessagePathDelayResp();
			pdelay_resp_msg->messageType = messageType;
			// Copy in v2 PDelay Response specific fields
			pdelay_resp_msg->requestingPortIdentity =
			    new PortIdentity((uint8_t *) buf +
					     PTP_PDELAY_RESP_REQ_CLOCK_ID
					     (PTP_PDELAY_RESP_OFFSET),
					     (uint16_t *) (buf +
							   PTP_PDELAY_RESP_REQ_PORT_ID
							   (PTP_PDELAY_RESP_OFFSET)));

#ifdef DEBUG
			for (int n = 0; n < PTP_CLOCK_IDENTITY_LENGTH; ++n) {	// MMM
				GPTP_LOG_VERBOSE("%c",
					pdelay_resp_msg->
					requestingPortIdentity.clockIdentity
					[n]);
			}
#endif

			memcpy(& (pdelay_resp_msg->requestReceiptTimestamp.seconds_ms),
			       buf + PTP_PDELAY_RESP_SEC_MS(PTP_PDELAY_RESP_OFFSET),
			       sizeof
				   (pdelay_resp_msg->requestReceiptTimestamp.seconds_ms));
			memcpy(&
			       (pdelay_resp_msg->
				requestReceiptTimestamp.seconds_ls),
			       buf +
			       PTP_PDELAY_RESP_SEC_LS(PTP_PDELAY_RESP_OFFSET),
			       sizeof(pdelay_resp_msg->
				      requestReceiptTimestamp.seconds_ls));
			memcpy(&
			       (pdelay_resp_msg->
				requestReceiptTimestamp.nanoseconds),
			       buf +
			       PTP_PDELAY_RESP_NSEC(PTP_PDELAY_RESP_OFFSET),
			       sizeof(pdelay_resp_msg->
				      requestReceiptTimestamp.nanoseconds));

			pdelay_resp_msg->requestReceiptTimestamp.seconds_ms =
			    PLAT_ntohs(pdelay_resp_msg->requestReceiptTimestamp.seconds_ms);
			pdelay_resp_msg->requestReceiptTimestamp.seconds_ls =
			    PLAT_ntohl(pdelay_resp_msg->requestReceiptTimestamp.seconds_ls);
			pdelay_resp_msg->requestReceiptTimestamp.nanoseconds =
			    PLAT_ntohl(pdelay_resp_msg->requestReceiptTimestamp.nanoseconds);

			msg = pdelay_resp_msg;
		}
		break;
	case PATH_DELAY_FOLLOWUP_MESSAGE:

		GPTP_LOG_DEBUG("*** Received PDelay Response FollowUp message");

		// Be sure buffer is the correction size
//     if( size < PTP_COMMON_HDR_LENGTH + PTP_PDELAY_FOLLOWUP_LENGTH ) {
//       goto abort;
//     }
		{
			PTPMessagePathDelayRespFollowUp *pdelay_resp_fwup_msg =
			    new PTPMessagePathDelayRespFollowUp();
			pdelay_resp_fwup_msg->messageType = messageType;
			// Copy in v2 PDelay Response specific fields
			pdelay_resp_fwup_msg->requestingPortIdentity =
			    new PortIdentity((uint8_t *) buf +
					     PTP_PDELAY_FOLLOWUP_REQ_CLOCK_ID
					     (PTP_PDELAY_RESP_OFFSET),
					     (uint16_t *) (buf +
							   PTP_PDELAY_FOLLOWUP_REQ_PORT_ID
							   (PTP_PDELAY_FOLLOWUP_OFFSET)));

			memcpy(&
			       (pdelay_resp_fwup_msg->
				responseOriginTimestamp.seconds_ms),
			       buf +
			       PTP_PDELAY_FOLLOWUP_SEC_MS
			       (PTP_PDELAY_FOLLOWUP_OFFSET),
			       sizeof
			       (pdelay_resp_fwup_msg->responseOriginTimestamp.
				seconds_ms));
			memcpy(&
			       (pdelay_resp_fwup_msg->
				responseOriginTimestamp.seconds_ls),
			       buf +
			       PTP_PDELAY_FOLLOWUP_SEC_LS
			       (PTP_PDELAY_FOLLOWUP_OFFSET),
			       sizeof
			       (pdelay_resp_fwup_msg->responseOriginTimestamp.
				seconds_ls));
			memcpy(&
			       (pdelay_resp_fwup_msg->
				responseOriginTimestamp.nanoseconds),
			       buf +
			       PTP_PDELAY_FOLLOWUP_NSEC
			       (PTP_PDELAY_FOLLOWUP_OFFSET),
			       sizeof
			       (pdelay_resp_fwup_msg->responseOriginTimestamp.
				nanoseconds));

			pdelay_resp_fwup_msg->
			    responseOriginTimestamp.seconds_ms =
			    PLAT_ntohs
			    (pdelay_resp_fwup_msg->responseOriginTimestamp.
			     seconds_ms);
			pdelay_resp_fwup_msg->
			    responseOriginTimestamp.seconds_ls =
			    PLAT_ntohl
			    (pdelay_resp_fwup_msg->responseOriginTimestamp.
			     seconds_ls);
			pdelay_resp_fwup_msg->
			    responseOriginTimestamp.nanoseconds =
			    PLAT_ntohl
			    (pdelay_resp_fwup_msg->responseOriginTimestamp.
			     nanoseconds);

			msg = pdelay_resp_fwup_msg;
		}
		break;
	case ANNOUNCE_MESSAGE:

		GPTP_LOG_VERBOSE("*** Received Announce message");

		{
			static uint32_t announce_recv_count = 0;
			announce_recv_count++;
			
			GPTP_LOG_STATUS("*** RECEIVED ANNOUNCE MESSAGE #%u *** (size=%d bytes)", announce_recv_count, size);
			
			PTPMessageAnnounce *annc = new PTPMessageAnnounce();
			annc->messageType = messageType;
			int tlv_length = size - PTP_COMMON_HDR_LENGTH + PTP_ANNOUNCE_LENGTH;

			memcpy(&(annc->currentUtcOffset),
			       buf +
			       PTP_ANNOUNCE_CURRENT_UTC_OFFSET
			       (PTP_ANNOUNCE_OFFSET),
			       sizeof(annc->currentUtcOffset));
			annc->currentUtcOffset =
			    PLAT_ntohs(annc->currentUtcOffset);
			memcpy(&(annc->grandmasterPriority1),
			       buf +
			       PTP_ANNOUNCE_GRANDMASTER_PRIORITY1
			       (PTP_ANNOUNCE_OFFSET),
			       sizeof(annc->grandmasterPriority1));
			memcpy( annc->grandmasterClockQuality,
				buf+
				PTP_ANNOUNCE_GRANDMASTER_CLOCK_QUALITY
				(PTP_ANNOUNCE_OFFSET),
				sizeof( *annc->grandmasterClockQuality ));
			annc->
			  grandmasterClockQuality->offsetScaledLogVariance =
			  PLAT_ntohs
			  ( annc->grandmasterClockQuality->
			    offsetScaledLogVariance );
			memcpy(&(annc->grandmasterPriority2),
			       buf +
			       PTP_ANNOUNCE_GRANDMASTER_PRIORITY2
			       (PTP_ANNOUNCE_OFFSET),
			       sizeof(annc->grandmasterPriority2));
			memcpy(&(annc->grandmasterIdentity),
			       buf +
			       PTP_ANNOUNCE_GRANDMASTER_IDENTITY
			       (PTP_ANNOUNCE_OFFSET),
			       PTP_CLOCK_IDENTITY_LENGTH);
			memcpy(&(annc->stepsRemoved),
			       buf +
			       PTP_ANNOUNCE_STEPS_REMOVED(PTP_ANNOUNCE_OFFSET),
			       sizeof(annc->stepsRemoved));
			annc->stepsRemoved = PLAT_ntohs(annc->stepsRemoved);
			memcpy(&(annc->timeSource),
			       buf +
			       PTP_ANNOUNCE_TIME_SOURCE(PTP_ANNOUNCE_OFFSET),
			       sizeof(annc->timeSource));

			// Parse TLV if it exists
			buf += PTP_COMMON_HDR_LENGTH + PTP_ANNOUNCE_LENGTH;
			if( tlv_length > (int) (2*sizeof(uint16_t)) && PLAT_ntohs(*((uint16_t *)buf)) == PATH_TRACE_TLV_TYPE)  {
				buf += sizeof(uint16_t);
				tlv_length -= sizeof(uint16_t);
				annc->tlv.parseClockIdentity((uint8_t *)buf, tlv_length);
			}

			msg = annc;
		}
		break;

	case SIGNALLING_MESSAGE:
		{
			PTPMessageSignalling *signallingMsg = new PTPMessageSignalling();
			signallingMsg->messageType = messageType;

			memcpy(&(signallingMsg->targetPortIdentify),
			       buf + PTP_SIGNALLING_TARGET_PORT_IDENTITY(PTP_SIGNALLING_OFFSET),
			       sizeof(signallingMsg->targetPortIdentify));

			memcpy( &(signallingMsg->tlv), buf + PTP_SIGNALLING_OFFSET + PTP_SIGNALLING_LENGTH, sizeof(signallingMsg->tlv) );

			msg = signallingMsg;
		}
		break;

	default:

		GPTP_LOG_EXCEPTION("Received unsupported message type, %d",
		            (int)messageType);
		port->incCounter_ieee8021AsPortStatRxPTPPacketDiscard();

		goto abort;
	}

	msg->_gc = false;

	// Copy in common header fields
	memcpy(&(msg->versionPTP),
	       buf + PTP_COMMON_HDR_PTP_VERSION(PTP_COMMON_HDR_OFFSET),
	       sizeof(msg->versionPTP));
	memcpy(&(msg->messageLength),
	       buf + PTP_COMMON_HDR_MSG_LENGTH(PTP_COMMON_HDR_OFFSET),
	       sizeof(msg->messageLength));
	msg->messageLength = PLAT_ntohs(msg->messageLength);
	memcpy(&(msg->domainNumber),
	       buf + PTP_COMMON_HDR_DOMAIN_NUMBER(PTP_COMMON_HDR_OFFSET),
	       sizeof(msg->domainNumber));
	memcpy(&(msg->flags), buf + PTP_COMMON_HDR_FLAGS(PTP_COMMON_HDR_OFFSET),
	       PTP_FLAGS_LENGTH);
	memcpy(&(msg->correctionField),
	       buf + PTP_COMMON_HDR_CORRECTION(PTP_COMMON_HDR_OFFSET),
	       sizeof(msg->correctionField));
	msg->correctionField = PLAT_ntohll(msg->correctionField);
	msg->sourcePortIdentity = sourcePortIdentity;
	msg->sequenceId = sequenceId;
	memcpy(&(msg->control),
	       buf + PTP_COMMON_HDR_CONTROL(PTP_COMMON_HDR_OFFSET),
	       sizeof(msg->control));
	memcpy(&(msg->logMeanMessageInterval),
	       buf + PTP_COMMON_HDR_LOG_MSG_INTRVL(PTP_COMMON_HDR_OFFSET),
	       sizeof(msg->logMeanMessageInterval));

	if( eport != NULL )
		eport->addSockAddrMap( msg->sourcePortIdentity, remote );

	msg->_timestamp = timestamp;
	msg->_timestamp_counter_value = counter_value;

	delete timer;

	return msg;

abort:
	GPTP_LOG_ERROR("*** ABORT: Entered abort label in buildPTPMessage or message processing. Returning NULL. ***");
	delete sourcePortIdentity;
	delete timer;
	return NULL;
}

bool PTPMessageCommon::getTxTimestamp( EtherPort *port, uint32_t link_speed )
{
	OSTimer *timer = port->getTimerFactory()->createTimer();
	int ts_good;
	Timestamp tx_timestamp;
	uint32_t unused;
	unsigned req = TX_TIMEOUT_BASE;
	int iter = TX_TIMEOUT_ITER;

	ts_good = port->getTxTimestamp
		( this, tx_timestamp, unused, false );
	while( ts_good != GPTP_EC_SUCCESS && iter-- != 0 )
	{
		timer->sleep(req);
		if (ts_good != GPTP_EC_EAGAIN && iter < 1)
			GPTP_LOG_ERROR(
				"Error (TX) timestamping PDelay request "
				"(Retrying-%d), error=%d", iter, ts_good);
		ts_good = port->getTxTimestamp
			( this, tx_timestamp, unused , iter == 0 );
		req *= 2;
	}

	if( ts_good == GPTP_EC_SUCCESS )
	{
		Timestamp phy_compensation = port->getTxPhyDelay( link_speed );
		GPTP_LOG_DEBUG( "TX PHY compensation: %s sec",
				phy_compensation.toString().c_str() );
		phy_compensation._version = tx_timestamp._version;
		_timestamp = tx_timestamp + phy_compensation;
	} else
	{
		char msg[HWTIMESTAMPER_EXTENDED_MESSAGE_SIZE];
		port->getExtendedError(msg);
		GPTP_LOG_ERROR(
			"Error (TX) timestamping PDelay request, error=%d\t%s",
			ts_good, msg);
		_timestamp = INVALID_TIMESTAMP;
	}

	delete timer;
	return ts_good == GPTP_EC_SUCCESS;
}

void PTPMessageCommon::processMessage( CommonPort *port )
{
	_gc = true;
	return;
}

void PTPMessageCommon::buildCommonHeader(uint8_t * buf)
{
	unsigned char tspec_msg_t;
    /*TODO: Message type assumes value sbetween 0x0 and 0xD (its an enumeration).
     * So I am not sure why we are adding 0x10 to it
     */
	tspec_msg_t = messageType | 0x10;
	long long correctionField_BE = PLAT_htonll(correctionField);
	uint16_t messageLength_NO = PLAT_htons(messageLength);

	memcpy(buf + PTP_COMMON_HDR_TRANSSPEC_MSGTYPE(PTP_COMMON_HDR_OFFSET),
	       &tspec_msg_t, sizeof(tspec_msg_t));
	memcpy(buf + PTP_COMMON_HDR_PTP_VERSION(PTP_COMMON_HDR_OFFSET),
	       &versionPTP, sizeof(versionPTP));
	memcpy(buf + PTP_COMMON_HDR_MSG_LENGTH(PTP_COMMON_HDR_OFFSET),
	       &messageLength_NO, sizeof(messageLength_NO));
	memcpy(buf + PTP_COMMON_HDR_DOMAIN_NUMBER(PTP_COMMON_HDR_OFFSET),
	       &domainNumber, sizeof(domainNumber));
	memcpy(buf + PTP_COMMON_HDR_FLAGS(PTP_COMMON_HDR_OFFSET), &flags,
	       PTP_FLAGS_LENGTH);
	memcpy(buf + PTP_COMMON_HDR_CORRECTION(PTP_COMMON_HDR_OFFSET),
	       &correctionField_BE, sizeof(correctionField));

	sourcePortIdentity->getClockIdentityString
	  ((uint8_t *) buf+
	   PTP_COMMON_HDR_SOURCE_CLOCK_ID(PTP_COMMON_HDR_OFFSET));
	sourcePortIdentity->getPortNumberNO
	  ((uint16_t *) (buf + PTP_COMMON_HDR_SOURCE_PORT_ID
			 (PTP_COMMON_HDR_OFFSET)));

	GPTP_LOG_VERBOSE("Sending Sequence Id: %u", sequenceId);
	sequenceId = PLAT_htons(sequenceId);
	memcpy(buf + PTP_COMMON_HDR_SEQUENCE_ID(PTP_COMMON_HDR_OFFSET),
	       &sequenceId, sizeof(sequenceId));
	sequenceId = PLAT_ntohs(sequenceId);
	memcpy(buf + PTP_COMMON_HDR_CONTROL(PTP_COMMON_HDR_OFFSET), &control,
	       sizeof(control));
	memcpy(buf + PTP_COMMON_HDR_LOG_MSG_INTRVL(PTP_COMMON_HDR_OFFSET),
	       &logMeanMessageInterval, sizeof(logMeanMessageInterval));

	return;
}

void PTPMessageCommon::getPortIdentity(PortIdentity * identity)
{
	*identity = *sourcePortIdentity;
}

void PTPMessageCommon::setPortIdentity(PortIdentity * identity)
{
	*sourcePortIdentity = *identity;
}

PTPMessageCommon::~PTPMessageCommon(void)
{
	delete sourcePortIdentity;
	return;
}

PTPMessageAnnounce::PTPMessageAnnounce(void)
{
	grandmasterClockQuality = new ClockQuality();
}

PTPMessageAnnounce::~PTPMessageAnnounce(void)
{
	delete grandmasterClockQuality;
}

bool PTPMessageAnnounce::isBetterThan(PTPMessageAnnounce * msg)
{
	unsigned char this1[14];
	unsigned char that1[14];
	uint16_t tmp;

	this1[0] = grandmasterPriority1;
	that1[0] = msg->getGrandmasterPriority1();

	this1[1] = grandmasterClockQuality->cq_class;
	that1[1] = msg->getGrandmasterClockQuality()->cq_class;

	this1[2] = grandmasterClockQuality->clockAccuracy;
	that1[2] = msg->getGrandmasterClockQuality()->clockAccuracy;

	tmp = grandmasterClockQuality->offsetScaledLogVariance;
	tmp = PLAT_htons(tmp);
	memcpy(this1 + 3, &tmp, sizeof(tmp));
	tmp = msg->getGrandmasterClockQuality()->offsetScaledLogVariance;
	tmp = PLAT_htons(tmp);
	memcpy(that1 + 3, &tmp, sizeof(tmp));

	this1[5] = grandmasterPriority2;
	that1[5] = msg->getGrandmasterPriority2();

	this->getGrandmasterIdentity((char *)this1 + 6);
	msg->getGrandmasterIdentity((char *)that1 + 6);

#if 0
	GPTP_LOG_VERBOSE("Us: ");
	for (int i = 0; i < 14; ++i)
		GPTP_LOG_VERBOSE("%hhx", this1[i]);
	GPTP_LOG_VERBOSE("\n");
	GPTP_LOG_VERBOSE("Them: ");
	for (int i = 0; i < 14; ++i)
		GPTP_LOG_VERBOSE("%hhx", that1[i]);
	GPTP_LOG_VERBOSE("\n");
#endif

	return (memcmp(this1, that1, 14) < 0) ? true : false;
}


PTPMessageSync::PTPMessageSync() {
}

PTPMessageSync::~PTPMessageSync() {
}

PTPMessageSync::PTPMessageSync( EtherPort *port ) :
	PTPMessageCommon( port )
{
	messageType = SYNC_MESSAGE;	// This is an event message
	sequenceId = port->getNextSyncSequenceId();
	control = SYNC;

	flags[PTP_ASSIST_BYTE] |= (0x1 << PTP_ASSIST_BIT);

	originTimestamp = port->getClock()->getTime();

	logMeanMessageInterval = port->getSyncInterval();
	return;
}

bool PTPMessageSync::sendPort
( EtherPort *port, PortIdentity *destIdentity )
{
	uint8_t buf_t[256];
	uint8_t *buf_ptr = buf_t + port->getPayloadOffset();
	unsigned char tspec_msg_t = 0x0;
	Timestamp originTimestamp_BE;
	uint32_t link_speed;

	memset(buf_t, 0, 256);
	// Create packet in buf
	// Copy in common header
	messageLength = PTP_COMMON_HDR_LENGTH + PTP_SYNC_LENGTH;
	tspec_msg_t |= messageType & 0xF;
	buildCommonHeader(buf_ptr);
	// Get timestamp
	originTimestamp = port->getClock()->getTime();
	originTimestamp_BE.seconds_ms = PLAT_htons(originTimestamp.seconds_ms);
	originTimestamp_BE.seconds_ls = PLAT_htonl(originTimestamp.seconds_ls);
	originTimestamp_BE.nanoseconds =
	    PLAT_htonl(originTimestamp.nanoseconds);
	// Copy in v2 sync specific fields
	memcpy(buf_ptr + PTP_SYNC_SEC_MS(PTP_SYNC_OFFSET),
	       &(originTimestamp_BE.seconds_ms),
	       sizeof(originTimestamp.seconds_ms));
	memcpy(buf_ptr + PTP_SYNC_SEC_LS(PTP_SYNC_OFFSET),
	       &(originTimestamp_BE.seconds_ls),
	       sizeof(originTimestamp.seconds_ls));
	memcpy(buf_ptr + PTP_SYNC_NSEC(PTP_SYNC_OFFSET),
	       &(originTimestamp_BE.nanoseconds),
	       sizeof(originTimestamp.nanoseconds));

	port->sendEventPort
		( PTP_ETHERTYPE, buf_t, messageLength, MCAST_OTHER,
		  destIdentity, &link_speed );
	port->incCounter_ieee8021AsPortStatTxSyncCount();

	return getTxTimestamp( port, link_speed );
}

PTPMessageAnnounce::PTPMessageAnnounce( CommonPort *port ) :
	PTPMessageCommon( port )
{
	messageType = ANNOUNCE_MESSAGE;	// This is an event message
	sequenceId = port->getNextAnnounceSequenceId();
	ClockIdentity id;
	control = MESSAGE_OTHER;
	ClockIdentity clock_identity;

	id = port->getClock()->getClockIdentity();
	tlv.appendClockIdentity(&id);

	currentUtcOffset = port->getClock()->getCurrentUtcOffset();
	grandmasterPriority1 = port->getClock()->getPriority1();
	grandmasterPriority2 = port->getClock()->getPriority2();
	grandmasterClockQuality = new ClockQuality();
	*grandmasterClockQuality = port->getClock()->getClockQuality();
	stepsRemoved = 0;
	timeSource = port->getClock()->getTimeSource();
	clock_identity = port->getClock()->getGrandmasterClockIdentity();
	clock_identity.getIdentityString(grandmasterIdentity);

	logMeanMessageInterval = port->getAnnounceInterval();
	return;
}

bool PTPMessageAnnounce::sendPort
( CommonPort *port, PortIdentity *destIdentity )
{
	uint8_t buf_t[256];
	uint8_t *buf_ptr = buf_t + port->getPayloadOffset();
	unsigned char tspec_msg_t = 0x0;

	uint16_t currentUtcOffset_l = PLAT_htons(currentUtcOffset);
	uint16_t stepsRemoved_l = PLAT_htons(stepsRemoved);
	ClockQuality clockQuality_l = *grandmasterClockQuality;
	clockQuality_l.offsetScaledLogVariance =
	    PLAT_htons(clockQuality_l.offsetScaledLogVariance);

	memset(buf_t, 0, 256);
	// Create packet in buf
	// Copy in common header
	messageLength =
		PTP_COMMON_HDR_LENGTH + PTP_ANNOUNCE_LENGTH + tlv.length();
	tspec_msg_t |= messageType & 0xF;
	buildCommonHeader(buf_ptr);
	memcpy(buf_ptr + PTP_ANNOUNCE_CURRENT_UTC_OFFSET(PTP_ANNOUNCE_OFFSET),
	       &currentUtcOffset_l, sizeof(currentUtcOffset));
	memcpy(buf_ptr +
	       PTP_ANNOUNCE_GRANDMASTER_PRIORITY1(PTP_ANNOUNCE_OFFSET),
	       &grandmasterPriority1, sizeof(grandmasterPriority1));
	memcpy(buf_ptr +
	       PTP_ANNOUNCE_GRANDMASTER_CLOCK_QUALITY(PTP_ANNOUNCE_OFFSET),
	       &clockQuality_l, sizeof(clockQuality_l));
	memcpy(buf_ptr +
	       PTP_ANNOUNCE_GRANDMASTER_PRIORITY2(PTP_ANNOUNCE_OFFSET),
	       &grandmasterPriority2, sizeof(grandmasterPriority2));
	memcpy( buf_ptr+
		PTP_ANNOUNCE_GRANDMASTER_IDENTITY(PTP_ANNOUNCE_OFFSET),
		grandmasterIdentity, PTP_CLOCK_IDENTITY_LENGTH );
	memcpy(buf_ptr + PTP_ANNOUNCE_STEPS_REMOVED(PTP_ANNOUNCE_OFFSET),
	       &stepsRemoved_l, sizeof(stepsRemoved));
	memcpy(buf_ptr + PTP_ANNOUNCE_TIME_SOURCE(PTP_ANNOUNCE_OFFSET),
	       &timeSource, sizeof(timeSource));
	tlv.toByteString(buf_ptr + PTP_COMMON_HDR_LENGTH + PTP_ANNOUNCE_LENGTH);

	port->sendGeneralPort(PTP_ETHERTYPE, buf_t, messageLength, MCAST_OTHER, destIdentity);
	port->incCounter_ieee8021AsPortStatTxAnnounce();

	return true;
}

void PTPMessageAnnounce::processMessage( CommonPort *port )
{
	ClockIdentity my_clock_identity;

	port->incCounter_ieee8021AsPortStatRxAnnounce();

	// Delete announce receipt timeout
	port->getClock()->deleteEventTimerLocked
		(port, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES);

	if( stepsRemoved >= 255 ) goto bail;

	// Reject Announce message from myself
	my_clock_identity = port->getClock()->getClockIdentity();
	if( sourcePortIdentity->getClockIdentity() == my_clock_identity ) {
		goto bail;
	}

	if(tlv.has(&my_clock_identity)) {
		goto bail;
	}

	// Add message to the list
	port->setQualifiedAnnounce( this );

	port->getClock()->addEventTimerLocked(port, STATE_CHANGE_EVENT, 16000000);
 bail:
	port->getClock()->addEventTimerLocked
		(port, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES,
		 (unsigned long long)
		 (ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER *
		  (pow
		   ((double)2,
			port->getAnnounceInterval()) *
		   1000000000.0)));
}

void PTPMessageSync::processMessage( CommonPort *port )
{
	EtherPort *eport = dynamic_cast <EtherPort *> (port);
	PTPMessageSync *old_sync;

	if (eport == NULL)
	{
		GPTP_LOG_ERROR( "Discarding sync message on wrong port type" );
		_gc = true;
		goto done;
	}

	if (port->getPortState() == PTP_DISABLED ) {
		// Do nothing Sync messages should be ignored in this state
		return;
	}
	if (port->getPortState() == PTP_FAULTY) {
		// According to spec recovery is implementation specific
		eport->recoverPort();
		return;
	}

	port->incCounter_ieee8021AsPortStatRxSyncCount();

#if CHECK_ASSIST_BIT
	if( flags[PTP_ASSIST_BYTE] & (0x1<<PTP_ASSIST_BIT)) {
#endif
		old_sync = eport->getLastSync();

		if (old_sync != NULL) {
			delete old_sync;
		}
		eport->setLastSync(this);
		_gc = false;
		goto done;
#if CHECK_ASSIST_BIT
	} else {
		GPTP_LOG_ERROR("PTP assist flag is not set, discarding invalid sync");
		_gc = true;
		goto done;
	}
#endif

 done:
	return;
}

PTPMessageFollowUp::PTPMessageFollowUp( CommonPort *port ) :
	PTPMessageCommon( port )
{
	messageType = FOLLOWUP_MESSAGE;	/* This is an event message */
	control = FOLLOWUP;

	logMeanMessageInterval = port->getSyncInterval();

	return;
}

size_t PTPMessageFollowUp::buildMessage( CommonPort *port, uint8_t *buf_ptr )
{
	/* Create packet in buf
	Copy in common header */
	messageLength =
		PTP_COMMON_HDR_LENGTH + PTP_FOLLOWUP_LENGTH + sizeof(tlv);
	unsigned char tspec_msg_t = 0;
	Timestamp preciseOriginTimestamp_BE;

	tspec_msg_t |= messageType & 0xF;
	buildCommonHeader(buf_ptr);
	preciseOriginTimestamp_BE.seconds_ms =
		PLAT_htons(preciseOriginTimestamp.seconds_ms);
	preciseOriginTimestamp_BE.seconds_ls =
		PLAT_htonl(preciseOriginTimestamp.seconds_ls);
	preciseOriginTimestamp_BE.nanoseconds =
		PLAT_htonl(preciseOriginTimestamp.nanoseconds);
	/* Copy in v2 sync specific fields */
	memcpy(buf_ptr + PTP_FOLLOWUP_SEC_MS(PTP_FOLLOWUP_OFFSET),
		&(preciseOriginTimestamp_BE.seconds_ms),
		sizeof(preciseOriginTimestamp.seconds_ms));
	memcpy(buf_ptr + PTP_FOLLOWUP_SEC_LS(PTP_FOLLOWUP_OFFSET),
		&(preciseOriginTimestamp_BE.seconds_ls),
		sizeof(preciseOriginTimestamp.seconds_ls));
	memcpy(buf_ptr + PTP_FOLLOWUP_NSEC(PTP_FOLLOWUP_OFFSET),
		&(preciseOriginTimestamp_BE.nanoseconds),
		sizeof(preciseOriginTimestamp.nanoseconds));

	/*Change time base indicator to Network Order before sending it*/
	uint16_t tbi_NO = PLAT_htonl(tlv.getGMTimeBaseIndicator());
	tlv.setGMTimeBaseIndicator(tbi_NO);
	tlv.toByteString(buf_ptr + PTP_COMMON_HDR_LENGTH +
			 PTP_FOLLOWUP_LENGTH);

	port->incCounter_ieee8021AsPortStatTxFollowUpCount();

	return PTP_COMMON_HDR_LENGTH + PTP_FOLLOWUP_LENGTH + sizeof(tlv);
}

bool PTPMessageFollowUp::sendPort
( EtherPort *port, PortIdentity *destIdentity )
{
	uint8_t buf_t[256];
	uint8_t *buf_ptr = buf_t + port->getPayloadOffset();
	memset(buf_t, 0, 256);
	/* Create packet in buf
	   Copy in common header */
	buildMessage(port, buf_ptr);

	GPTP_LOG_VERBOSE( "Follow-Up Time: %u seconds(hi)",
			  preciseOriginTimestamp.seconds_ms);
	GPTP_LOG_VERBOSE( "Follow-Up Time: %u seconds",
		 preciseOriginTimestamp.seconds_ls);
	GPTP_LOG_VERBOSE( "FW-UP Time: %u nanoseconds",
			  preciseOriginTimestamp.nanoseconds);
	GPTP_LOG_VERBOSE( "FW-UP Time: %x seconds",
			  preciseOriginTimestamp.seconds_ls);
	GPTP_LOG_VERBOSE( "FW-UP Time: %x nanoseconds",
			  preciseOriginTimestamp.nanoseconds);
#ifdef DEBUG
	GPTP_LOG_VERBOSE("Follow-up Dump:");
	for (int i = 0; i < messageLength; ++i) {
		GPTP_LOG_VERBOSE("%d:%02x ", i, (unsigned char)buf_t[i]);
	}
#endif

	port->sendGeneralPort( PTP_ETHERTYPE, buf_t, messageLength,
			       MCAST_OTHER, destIdentity );

	return true;
}

void PTPMessageFollowUp::processMessage
( CommonPort *port, Timestamp sync_arrival )
{
	uint64_t delay;
	Timestamp system_time(0, 0, 0);
	Timestamp device_time(0, 0, 0);

	signed long long local_system_offset;
	signed long long scalar_offset;

	FrequencyRatio local_clock_adjustment;
	FrequencyRatio local_system_freq_offset;
	FrequencyRatio master_local_freq_offset;
	int64_t correction;
	int32_t scaledLastGmFreqChange = 0;
	scaledNs scaledLastGmPhaseChange;

	port->incCounter_ieee8021AsPortStatRxFollowUpCount();

	if (!port->getLinkDelay(&delay))
	{
		GPTP_LOG_ERROR( "Received Follow up but "
				"there is no valid link delay" );
		goto done;
	}

	master_local_freq_offset = tlv.getRateOffset();
	master_local_freq_offset /= 1ULL << 41;
	master_local_freq_offset += 1.0;
	master_local_freq_offset /= port->getPeerRateOffset();

	correctionField /= 1 << 16;
	if( correctionField < 0 )
	{
		if( port->getAllowNegativeCorrField() )
		{
			GPTP_LOG_INFO
					( "Received Follow Up with negative correctionField: %Ld", correctionField );
		}
		else
		{
			GPTP_LOG_ERROR
					( "Discard received Follow Up with negative correctionField: %Ld", correctionField );
			goto done;
		}
	}
	correction = (int64_t)
		((delay * master_local_freq_offset) + correctionField);

	if (correction > 0)
		TIMESTAMP_ADD_NS(preciseOriginTimestamp, correction);
	else TIMESTAMP_SUB_NS(preciseOriginTimestamp, -correction);

	local_clock_adjustment =
		port->getClock()->
		calcMasterLocalClockRateDifference
		(preciseOriginTimestamp, sync_arrival);

	if( local_clock_adjustment == NEGATIVE_TIME_JUMP )
	{
		GPTP_LOG_VERBOSE
			( "Received Follow Up but preciseOrigintimestamp "
			  "indicates negative time jump" );
		goto done;
	}

	scalar_offset = TIMESTAMP_TO_NS( sync_arrival );
	scalar_offset -= TIMESTAMP_TO_NS( preciseOriginTimestamp );

	GPTP_LOG_VERBOSE( "Followup Correction Field: %lld, Link Delay: %lu",
			  correctionField, delay );
	GPTP_LOG_VERBOSE( "FollowUp Scalar = %lld",
			  scalar_offset );


	/* Otherwise synchronize clock with approximate Sync time */
	uint32_t local_clock, nominal_clock_rate;
	uint32_t device_sync_time_offset;

	port->getDeviceTime(system_time, device_time, local_clock,
		nominal_clock_rate);
	GPTP_LOG_VERBOSE( "Device Time = %llu,System Time = %llu",
			  TIMESTAMP_TO_NS( device_time ),
			  TIMESTAMP_TO_NS( system_time ));

	/* Adjust local_clock to correspond to sync_arrival */
	device_sync_time_offset = (uint32_t)
		( TIMESTAMP_TO_NS( device_time ) -
		  TIMESTAMP_TO_NS( sync_arrival ));

	GPTP_LOG_VERBOSE
	( "ptp_message::FollowUp::processMessage System time: %u,%u "
	  "Device Time: %u,%u",
	  system_time.seconds_ls, system_time.nanoseconds,
	  device_time.seconds_ls, device_time.nanoseconds);

	/*Update information on local status structure.*/
	scaledLastGmFreqChange = (int32_t)
		((1.0 / local_clock_adjustment - 1.0) * (1ULL << 41));
	scaledLastGmPhaseChange.setLSB(tlv.getRateOffset( ));
	port->getClock()->getFUPStatus()->setScaledLastGmFreqChange
		( scaledLastGmFreqChange );
	port->getClock()->getFUPStatus()->setScaledLastGmPhaseChange
		( scaledLastGmPhaseChange );

	if( port->getPortState() == PTP_SLAVE )
	{
		/*
		 * The sync_count counts the number of sync messages received
		 * that influence the time on the device. Since adjustments are
		 * only made in the PTP_SLAVE state, increment it here
		 */
		port->incSyncCount();
		
		// Profile-specific performance tracking: Update jitter statistics when receiving sync messages
		if (port->getProfile().max_sync_jitter_ns > 0) {
			uint64_t sync_timestamp = TIMESTAMP_TO_NS(sync_arrival);
			port->updateProfileJitterStats(sync_timestamp);
			port->checkProfileConvergence();
		}

		// ...existing code...
	}

	uint16_t lastGmTimeBaseIndicator;
	lastGmTimeBaseIndicator = port->getLastGmTimeBaseIndicator();
	if (( lastGmTimeBaseIndicator > 0 ) &&
	    ( tlv.getGmTimeBaseIndicator( ) != lastGmTimeBaseIndicator ))
	{
		GPTP_LOG_EXCEPTION( "Sync discontinuity" );
	}
	port->setLastGmTimeBaseIndicator( tlv.getGmTimeBaseIndicator( ));

done:
	_gc = true;

	return;
}

void PTPMessageFollowUp::processMessage( CommonPort *port )
{
	Timestamp sync_arrival;
	EtherPort *eport = dynamic_cast <EtherPort *> (port);
	if (eport == NULL)
	{
		GPTP_LOG_ERROR
			( "Discarding followup message on wrong port type" );
		return;
	}

	GPTP_LOG_DEBUG("Processing a follow-up message");

	// Expire any SYNC_RECEIPT timers that exist
	port->stopSyncReceiptTimer();

	if (port->getPortState() == PTP_DISABLED ) {
		// Do nothing Sync messages should be ignored when in this state
		return;
	}
	if (port->getPortState() == PTP_FAULTY) {
		// According to spec recovery is implementation specific
		eport->recoverPort();
		return;
	}

	PTPMessageSync *sync = eport->getLastSync();
	{
		PortIdentity sync_id;
		if( sync == NULL )
		{
			GPTP_LOG_ERROR("Received Follow Up but there is no "
				       "sync message");
			return;
		}
		sync->getPortIdentity(&sync_id);

		if( sync->getSequenceId() != sequenceId ||
		    sync_id != *sourcePortIdentity )
		{
			unsigned int cnt = 0;

			if( !port->incWrongSeqIDCounter( &cnt ))
			{
				port->becomeMaster( true );
				port->setWrongSeqIDCounter(0);
			}
			GPTP_LOG_ERROR
				( "Received Follow Up %d times but cannot "
				  "find corresponding Sync", cnt );
			goto done;
		}
	}

	if( sync->getTimestamp()._version != port->getTimestampVersion( ))
	{
		GPTP_LOG_ERROR( "Received Follow Up but timestamp version "
				"indicates Sync is out of date" );
		goto done;
	}

	sync_arrival = sync->getTimestamp();

	processMessage(port, sync_arrival);

done:
	eport->setLastSync(NULL);
	delete sync;
}

PTPMessagePathDelayReq::PTPMessagePathDelayReq
( EtherPort *port ) : PTPMessageCommon( port )
{
	logMeanMessageInterval = port->getPDelayInterval();
	control = MESSAGE_OTHER;
	messageType = PATH_DELAY_REQ_MESSAGE;
	sequenceId = port->getNextPDelaySequenceId();
	return;
}

void PTPMessagePathDelayReq::processMessage( CommonPort *port )
{
	GPTP_LOG_INFO("*** PTPMessagePathDelayReq::processMessage - START processing PDelay Request");
	GPTP_LOG_DEBUG("PDELAY RESPONSE: Processing request seq=%u", sequenceId);
	
	OSTimer *timer = port->getTimerFactory()->createTimer();
	PortIdentity resp_fwup_id;
	PortIdentity requestingPortIdentity_p;
	PTPMessagePathDelayResp *resp;
	PortIdentity resp_id;
	PTPMessagePathDelayRespFollowUp *resp_fwup;

	EtherPort *eport = dynamic_cast <EtherPort *> (port);
	if (eport == NULL)
	{
		GPTP_LOG_ERROR( "Received Pdelay Request on wrong port type" );
		GPTP_LOG_DEBUG("PDELAY RESPONSE: FAILED - wrong port type");
		goto done;
	}

	GPTP_LOG_DEBUG("PDELAY RESPONSE: Port state check - current state: %d", port->getPortState());
	if (port->getPortState() == PTP_DISABLED) {
		// Do nothing all messages should be ignored when in this state
		GPTP_LOG_DEBUG("PDELAY RESPONSE: SKIPPED - port disabled");
		goto done;
	}

	if (port->getPortState() == PTP_FAULTY) {
		// According to spec recovery is implementation specific
		GPTP_LOG_DEBUG("PDELAY RESPONSE: RECOVERING - port faulty");
		eport->recoverPort();
		goto done;
	}

	port->incCounter_ieee8021AsPortStatRxPdelayRequest();

	GPTP_LOG_INFO("*** PTPMessagePathDelayReq::processMessage - passed state checks, creating response");
	GPTP_LOG_DEBUG("PDELAY RESPONSE: State checks passed - creating PDelay Response");

	/* Generate and send message */
	resp = new PTPMessagePathDelayResp(eport);
	port->getPortIdentity(resp_id);
	resp->setPortIdentity(&resp_id);
	resp->setSequenceId(sequenceId);

	GPTP_LOG_DEBUG("Process PDelay Request SeqId: %u\t", sequenceId);

#ifdef DEBUG
	for (int n = 0; n < PTP_CLOCK_IDENTITY_LENGTH; ++n) {
		GPTP_LOG_VERBOSE("%c", resp_id.clockIdentity[n]);
	}
#endif

	this->getPortIdentity(&requestingPortIdentity_p);
	resp->setRequestingPortIdentity(&requestingPortIdentity_p);
	resp->setRequestReceiptTimestamp(_timestamp);

	GPTP_LOG_INFO("*** About to send PDelay Response - acquiring TX lock");
	port->getTxLock();
	GPTP_LOG_INFO("*** TX lock acquired, calling sendPort");
	resp->sendPort(eport, sourcePortIdentity);
	GPTP_LOG_INFO("*** sendPort returned, releasing TX lock");
	GPTP_LOG_DEBUG("*** Sent PDelay Response message");
	port->putTxLock();
	GPTP_LOG_INFO("*** TX lock released");

	if( resp->getTimestamp()._version != _timestamp._version ) {
		GPTP_LOG_ERROR("TX timestamp version mismatch: %u/%u",
			       resp->getTimestamp()._version, _timestamp._version);
#if 0 // discarding the request could lead to the peer setting the link to non-asCapable
		delete resp;
		goto done;
#endif
	}

	resp_fwup = new PTPMessagePathDelayRespFollowUp(eport);
	port->getPortIdentity(resp_fwup_id);
	resp_fwup->setPortIdentity(&resp_fwup_id);
	resp_fwup->setSequenceId(sequenceId);
	resp_fwup->setRequestingPortIdentity(sourcePortIdentity);
	resp_fwup->setResponseOriginTimestamp(resp->getTimestamp());
	long long turnaround;
	turnaround = (resp->getTimestamp().seconds_ls - _timestamp.seconds_ls)
		* 1000000000LL;

	GPTP_LOG_VERBOSE("Response Depart(sec): %u",
			 resp->getTimestamp().seconds_ls);
	GPTP_LOG_VERBOSE("Request Arrival(sec): %u", _timestamp.seconds_ls);
	GPTP_LOG_VERBOSE("#1 Correction Field: %Ld", turnaround);

	turnaround += resp->getTimestamp().nanoseconds;

	GPTP_LOG_VERBOSE("#2 Correction Field: %Ld", turnaround);

	turnaround -= _timestamp.nanoseconds;

	GPTP_LOG_VERBOSE("#3 Correction Field: %Ld", turnaround);

	resp_fwup->setCorrectionField(0);
	resp_fwup->sendPort(eport, sourcePortIdentity);

	GPTP_LOG_DEBUG("*** Sent PDelay Response FollowUp message");

	delete resp;
	delete resp_fwup;

done:
	delete timer;
	_gc = true;
	return;
}

bool PTPMessagePathDelayReq::sendPort
( EtherPort *port, PortIdentity *destIdentity )
{
	uint32_t link_speed;

	if(port->pdelayHalted()) {
		GPTP_LOG_WARNING("PTPMessagePathDelayReq::sendPort: PDelay is halted, not sending request, returning false");
		return false;
	}

	uint8_t buf_t[256];
	uint8_t *buf_ptr = buf_t + port->getPayloadOffset();
	unsigned char tspec_msg_t = 0;
	memset(buf_t, 0, 256);
	/* Create packet in buf */
	/* Copy in common header */
	messageLength = PTP_COMMON_HDR_LENGTH + PTP_PDELAY_REQ_LENGTH;
	tspec_msg_t |= messageType & 0xF;
	buildCommonHeader(buf_ptr);
	port->sendEventPort
		( PTP_ETHERTYPE, buf_t, messageLength, MCAST_PDELAY,
		  destIdentity, &link_speed );
	port->incCounter_ieee8021AsPortStatTxPdelayRequest();

	return getTxTimestamp( port, link_speed );
}

PTPMessagePathDelayResp::PTPMessagePathDelayResp
( EtherPort *port ) : PTPMessageCommon( port )
{
    /*TODO: Why 0x7F?*/
	logMeanMessageInterval = 0x7F;
	control = MESSAGE_OTHER;
	messageType = PATH_DELAY_RESP_MESSAGE;
	versionPTP = GPTP_VERSION;
	requestingPortIdentity = new PortIdentity();

	flags[PTP_ASSIST_BYTE] |= (0x1 << PTP_ASSIST_BIT);

	return;
}

PTPMessagePathDelayResp::~PTPMessagePathDelayResp()
{
	delete requestingPortIdentity;
}

void PTPMessagePathDelayResp::processMessage( CommonPort *port )
{
	GPTP_LOG_INFO("*** PTPMessagePathDelayResp::processMessage - START processing PDelay Response");
	GPTP_LOG_STATUS("*** PDELAY RESPONSE RECV DEBUG: Processing response seq=%u", sequenceId);
	
	EtherPort *eport = dynamic_cast <EtherPort *> (port);
	if (eport == NULL)
	{
		GPTP_LOG_ERROR( "Received Pdelay Resp on wrong port type" );
		GPTP_LOG_STATUS("*** PDELAY RESPONSE RECV DEBUG: FAILED - wrong port type");
		_gc = true;
		return;
	}

	GPTP_LOG_STATUS("*** PDELAY RESPONSE RECV DEBUG: Port state check - current state: %d", port->getPortState());
	if (port->getPortState() == PTP_DISABLED) {
		// Do nothing all messages should be ignored when in this state
		GPTP_LOG_STATUS("*** PDELAY RESPONSE RECV DEBUG: SKIPPED - port disabled");
		return;
	}
	if (port->getPortState() == PTP_FAULTY) {
		// According to spec recovery is implementation specific
		GPTP_LOG_STATUS("*** PDELAY RESPONSE RECV DEBUG: RECOVERING - port faulty");
		eport->recoverPort();
		return;
	}

	port->incCounter_ieee8021AsPortStatRxPdelayResponse();

	if (eport->tryPDelayRxLock() != true) {
		GPTP_LOG_ERROR("Failed to get PDelay RX Lock");
		return;
	}

	PortIdentity resp_id;
	PortIdentity oldresp_id;
	uint16_t resp_port_number;
	uint16_t oldresp_port_number;

	PTPMessagePathDelayResp *old_pdelay_resp = eport->getLastPDelayResp();
	if( old_pdelay_resp == NULL ) {
		goto bypass_verify_duplicate;
	}

	old_pdelay_resp->getPortIdentity(&oldresp_id);
	oldresp_id.getPortNumber(&oldresp_port_number);
	getPortIdentity(&resp_id);
	resp_id.getPortNumber(&resp_port_number);

	/* In the case where we have multiple PDelay responses for the same
	 * PDelay request, and they come from different sources, it is necessary
	 * to verify if this happens 3 times (sequentially). If it does, PDelayRequests
	 * are halted for 5 minutes
	 */
	if( getSequenceId() == old_pdelay_resp->getSequenceId() )
	{
		/*If the duplicates are in sequence and from different sources*/
		if( (resp_port_number != oldresp_port_number ) && (
					(eport->getLastInvalidSeqID() + 1 ) == getSequenceId() ||
					eport->getDuplicateRespCounter() == 0 ) ){
			GPTP_LOG_ERROR("Two responses for same Request. seqID %d. First Response Port# %hu. Second Port# %hu. Counter %d",
				getSequenceId(), oldresp_port_number, resp_port_number, eport->getDuplicateRespCounter());

			if( eport->incrementDuplicateRespCounter() ) {
				GPTP_LOG_ERROR("Remote misbehaving. Stopping PDelay Requests for 5 minutes.");
				eport->stopPDelay();
				eport->getClock()->addEventTimerLocked
					(port, PDELAY_RESP_PEER_MISBEHAVING_TIMEOUT_EXPIRES, (int64_t)(300 * 1000000000.0));
			}
		}
		else {
			eport->setDuplicateRespCounter(0);
		}
		eport->setLastInvalidSeqID(getSequenceId());
	}
	else
	{
		eport->setDuplicateRespCounter(0);
	}

bypass_verify_duplicate:
	eport->setLastPDelayResp(this);

	if (old_pdelay_resp != NULL) {
		delete old_pdelay_resp;
	}

	eport->putPDelayRxLock();
	_gc = false;

	return;
}

bool PTPMessagePathDelayResp::sendPort
( EtherPort *port, PortIdentity *destIdentity )
{
	uint8_t buf_t[256];
	uint8_t *buf_ptr = buf_t + port->getPayloadOffset();
	unsigned char tspec_msg_t = 0;
	Timestamp requestReceiptTimestamp_BE;
	uint32_t link_speed;

	memset(buf_t, 0, 256);
	// Create packet in buf
	// Copy in common header
	messageLength = PTP_COMMON_HDR_LENGTH + PTP_PDELAY_RESP_LENGTH;
	tspec_msg_t |= messageType & 0xF;
	buildCommonHeader(buf_ptr);
	requestReceiptTimestamp_BE.seconds_ms =
	    PLAT_htons(requestReceiptTimestamp.seconds_ms);
	requestReceiptTimestamp_BE.seconds_ls =
	    PLAT_htonl(requestReceiptTimestamp.seconds_ls);
	requestReceiptTimestamp_BE.nanoseconds =
	    PLAT_htonl(requestReceiptTimestamp.nanoseconds);

	// Copy in v2 PDelay_Req specific fields
	requestingPortIdentity->getClockIdentityString
	  (buf_ptr + PTP_PDELAY_RESP_REQ_CLOCK_ID
	   (PTP_PDELAY_RESP_OFFSET));
	requestingPortIdentity->getPortNumberNO
		((uint16_t *)
		 (buf_ptr + PTP_PDELAY_RESP_REQ_PORT_ID
		  (PTP_PDELAY_RESP_OFFSET)));
	memcpy(buf_ptr + PTP_PDELAY_RESP_SEC_MS(PTP_PDELAY_RESP_OFFSET),
	       &(requestReceiptTimestamp_BE.seconds_ms),
	       sizeof(requestReceiptTimestamp.seconds_ms));
	memcpy(buf_ptr + PTP_PDELAY_RESP_SEC_LS(PTP_PDELAY_RESP_OFFSET),
	       &(requestReceiptTimestamp_BE.seconds_ls),
	       sizeof(requestReceiptTimestamp.seconds_ls));
	memcpy(buf_ptr + PTP_PDELAY_RESP_NSEC(PTP_PDELAY_RESP_OFFSET),
	       &(requestReceiptTimestamp_BE.nanoseconds),
	       sizeof(requestReceiptTimestamp.nanoseconds));

	GPTP_LOG_VERBOSE("PDelay Resp Timestamp: %u,%u",
		   requestReceiptTimestamp.seconds_ls,
		   requestReceiptTimestamp.nanoseconds);

	port->sendEventPort
		( PTP_ETHERTYPE, buf_t, messageLength, MCAST_PDELAY,
		  destIdentity, &link_speed );
	port->incCounter_ieee8021AsPortStatTxPdelayResponse();

	return getTxTimestamp( port, link_speed );
}

void PTPMessagePathDelayResp::setRequestingPortIdentity
(PortIdentity * identity)
{
	*requestingPortIdentity = *identity;
}

void PTPMessagePathDelayResp::getRequestingPortIdentity
(PortIdentity * identity)
{
	*identity = *requestingPortIdentity;
}

PTPMessagePathDelayRespFollowUp::PTPMessagePathDelayRespFollowUp
( EtherPort *port ) : PTPMessageCommon( port )
{
	logMeanMessageInterval = 0x7F;
	control = MESSAGE_OTHER;
	messageType = PATH_DELAY_FOLLOWUP_MESSAGE;
	versionPTP = GPTP_VERSION;
	requestingPortIdentity = new PortIdentity();

	return;
}

PTPMessagePathDelayRespFollowUp::~PTPMessagePathDelayRespFollowUp()
{
	delete requestingPortIdentity;
}

#define US_PER_SEC 1000000
void PTPMessagePathDelayRespFollowUp::processMessage
( CommonPort *port )
{
	PTPMessagePathDelayReq *req = NULL;
	PTPMessagePathDelayResp *resp = NULL;

	GPTP_LOG_STATUS("*** PDELAY FOLLOWUP DEBUG: Starting processMessage for seq=%u", sequenceId);

	Timestamp remote_resp_tx_timestamp(0, 0, 0);
	Timestamp request_tx_timestamp(0, 0, 0);
	Timestamp remote_req_rx_timestamp(0, 0, 0);
	Timestamp response_rx_timestamp(0, 0, 0);

	EtherPort *eport = dynamic_cast <EtherPort *> (port);
	if (eport == NULL)
	{
		GPTP_LOG_ERROR( "Received Pdelay Response FollowUp on wrong port type" );
		_gc = true;
		return;
	}

	GPTP_LOG_STATUS("*** PDELAY FOLLOWUP DEBUG: Port type check passed");

	if (port->getPortState() == PTP_DISABLED) {
		GPTP_LOG_STATUS("*** PDELAY FOLLOWUP DEBUG: Port disabled - ignoring");
		// Do nothing all messages should be ignored when in this state
		return;
	}
	if (port->getPortState() == PTP_FAULTY) {
		GPTP_LOG_STATUS("*** PDELAY FOLLOWUP DEBUG: Port faulty - recovering");
		// According to spec recovery is implementation specific
		eport->recoverPort();
		return;
	}

	GPTP_LOG_STATUS("*** PDELAY FOLLOWUP DEBUG: Port state check passed (state=%d)", port->getPortState());

	port->incCounter_ieee8021AsPortStatRxPdelayResponseFollowUp();

	if (eport->tryPDelayRxLock() != true) {
		GPTP_LOG_STATUS("*** PDELAY FOLLOWUP DEBUG: Could not acquire PDelay RX lock - returning");
		return;
	}

	GPTP_LOG_STATUS("*** PDELAY FOLLOWUP DEBUG: PDelay RX lock acquired");

	req = eport->getLastPDelayReq();
	resp = eport->getLastPDelayResp();

	if (req == NULL) {
		GPTP_LOG_ERROR("*** PDELAY FOLLOWUP DEBUG: No PDelay request exists - aborting");
		/* Shouldn't happen */
		GPTP_LOG_ERROR
		    (">>> Received PDelay followup but no REQUEST exists");
		goto abort;
	}

	GPTP_LOG_STATUS("*** PDELAY FOLLOWUP DEBUG: PDelay request found (seq=%u)", req->getSequenceId());

	if (resp == NULL) {
		GPTP_LOG_ERROR("*** PDELAY FOLLOWUP DEBUG: No PDelay response exists - aborting");
		/* Probably shouldn't happen either */
		GPTP_LOG_ERROR
		    (">>> Received PDelay followup but no RESPONSE exists");

		goto abort;
	}

	GPTP_LOG_STATUS("*** PDELAY FOLLOWUP DEBUG: PDelay response found (seq=%u)", resp->getSequenceId());

	if( req->getSequenceId() != sequenceId ) {
		GPTP_LOG_ERROR("*** PDELAY FOLLOWUP DEBUG: ABORT - req->getSequenceId() != sequenceId (%u != %u)", req->getSequenceId(), sequenceId);
		GPTP_LOG_ERROR
			( "Received PDelay Response Follow Up but cannot find corresponding response");
		goto abort;
	}

	GPTP_LOG_STATUS("*** PDELAY FOLLOWUP DEBUG: Sequence ID check passed (seq=%u)", sequenceId);

	{
		PortIdentity req_id;
		PortIdentity resp_id;
		uint16_t resp_port_number;
		uint16_t req_port_number;

		ClockIdentity resp_clkId = resp_id.getClockIdentity();
		ClockIdentity req_clkId = req_id.getClockIdentity();
		PortIdentity fup_sourcePortIdentity;
		PortIdentity resp_sourcePortIdentity;

		req->getPortIdentity(&req_id);
		resp->getRequestingPortIdentity(&resp_id);

		resp_id.getPortNumber(&resp_port_number);
		requestingPortIdentity->getPortNumber(&req_port_number);

		resp->getPortIdentity(&resp_sourcePortIdentity);
		getPortIdentity(&fup_sourcePortIdentity);

		/*
		* IEEE 802.1AS, Figure 11-8, subclause 11.2.15.3
		*/
		if (resp->getSequenceId() != sequenceId) {
			GPTP_LOG_ERROR("*** PDELAY FOLLOWUP DEBUG: ABORT - resp->getSequenceId() != sequenceId (%u != %u)", resp->getSequenceId(), sequenceId);
			GPTP_LOG_ERROR
			("Received PDelay Response Follow Up but cannot find "
				"corresponding response");
			GPTP_LOG_ERROR( "%hu, %hu, %hu, %hu",
					resp->getSequenceId(), sequenceId,
					resp_port_number, req_port_number );

			goto abort;
		}

		/*
		* IEEE 802.1AS, Figure 11-8, subclause 11.2.15.3
		*/
		if (req_clkId != resp_clkId) {
			GPTP_LOG_ERROR("*** PDELAY FOLLOWUP DEBUG: ABORT - req_clkId != resp_clkId");
			GPTP_LOG_ERROR
			( "ClockID Resp/Req differs. PDelay Response ClockID: "
			  "%s PDelay Request ClockID: %s",
			  req_clkId.getIdentityString().c_str(),
			  resp_clkId.getIdentityString().c_str( ));
			goto abort;
		}

		/*
		* IEEE 802.1AS, Figure 11-8, subclause 11.2.15.3
		*/
		if (resp_port_number != req_port_number) {
			GPTP_LOG_ERROR("*** PDELAY FOLLOWUP DEBUG: ABORT - resp_port_number != req_port_number (%hu != %hu)", resp_port_number, req_port_number);
			GPTP_LOG_ERROR
			( "Request port number (%hu) is different from "
			  "Response port number (%hu)",
			  req_port_number, resp_port_number );

			goto abort;
		}

		/*
		* IEEE 802.1AS, Figure 11-8, subclause 11.2.15.3
		*/
		if (fup_sourcePortIdentity != resp_sourcePortIdentity) {
			GPTP_LOG_ERROR("*** PDELAY FOLLOWUP DEBUG: ABORT - fup_sourcePortIdentity != resp_sourcePortIdentity");
			GPTP_LOG_ERROR( "Source port identity from "
					"PDelay Response/FUP differ" );

			goto abort;
		}
	}

	GPTP_LOG_STATUS("*** PDELAY FOLLOWUP DEBUG: About to cancel timer (deleteEventTimerLocked) ***");
	port->getClock()->deleteEventTimerLocked
		(port, PDELAY_RESP_RECEIPT_TIMEOUT_EXPIRES);
	GPTP_LOG_STATUS("*** PDELAY FOLLOWUP DEBUG: Timer cancelled successfully - PDelay exchange complete");

    // Profile-specific late response tracking: mark that we received a response (even if late)
    if( eport->getProfile().late_response_threshold_ms > 0 ) {
        eport->setPDelayResponseReceived(true);
        
        // Check if response is late based on expected timing
        Timestamp now = port->getClock()->getTime();
        Timestamp req_time = eport->getLastPDelayReqTimestamp();
        if( req_time.nanoseconds != 0 ) { // Valid request timestamp
            uint64_t elapsed_ns = TIMESTAMP_TO_NS(now) - TIMESTAMP_TO_NS(req_time);
            uint64_t expected_response_time_ns = (uint64_t)(pow(2.0, eport->getPDelayInterval()) * 1000000000.0);
            
            if( elapsed_ns > expected_response_time_ns + 10000000 ) // More than 10ms late
                {
                unsigned late_count = eport->getConsecutiveLateResponses() + 1;
                eport->setConsecutiveLateResponses(late_count);
                eport->setConsecutiveMissingResponses(0); // Reset missing count
                GPTP_LOG_STATUS("*** MILAN: PDelay response is late by %.3f ms (consecutive late: %d) ***", 
                    (elapsed_ns - expected_response_time_ns) / 1000000.0, late_count);
            } else {
                // On-time response, reset counters
                eport->setConsecutiveLateResponses(0);
                eport->setConsecutiveMissingResponses(0);
                GPTP_LOG_DEBUG("*** MILAN: PDelay response on-time, resetting late/missing counters ***");
            }
        }
    }

	int64_t link_delay;
	unsigned long long turn_around;

    GPTP_LOG_STATUS("*** PDELAY FOLLOWUP DEBUG: About to increment PdelayCount ***");
    port->incPdelayCount();
    GPTP_LOG_STATUS("*** PDELAY FOLLOWUP DEBUG: PdelayCount incremented, now %u", port->getPdelayCount());

    /* Assume that we are a two-step clock, otherwise originTimestamp
	   may be used */
	request_tx_timestamp = req->getTimestamp();
	if( request_tx_timestamp.nanoseconds == INVALID_TIMESTAMP.nanoseconds )
	{
		/* Stop processing the packet */
		goto abort;
	}

	if (request_tx_timestamp.nanoseconds ==
	    PDELAY_PENDING_TIMESTAMP.nanoseconds) {
		// Defer processing
		if(
			eport->getLastPDelayRespFollowUp() != NULL &&
			eport->getLastPDelayRespFollowUp() != this )
		{
			delete eport->getLastPDelayRespFollowUp();
		}
		eport->setLastPDelayRespFollowUp(this);
		port->getClock()->addEventTimerLocked
			(port, PDELAY_DEFERRED_PROCESSING, 1000000);
		goto defer;
	}
	remote_req_rx_timestamp = resp->getRequestReceiptTimestamp();
	response_rx_timestamp = resp->getTimestamp();
	remote_resp_tx_timestamp = responseOriginTimestamp;

	if( request_tx_timestamp._version != response_rx_timestamp._version ) {
		GPTP_LOG_ERROR( "Received Follow Up but timestamp version "
				"indicates Sync is out of date" );
		goto done;
	}

	port->incPdelayCount();


	link_delay =
		((response_rx_timestamp.seconds_ms * 1LL -
		  request_tx_timestamp.seconds_ms) << 32) * 1000000000;
	link_delay +=
		(response_rx_timestamp.seconds_ls * 1LL -
		 request_tx_timestamp.seconds_ls) * 1000000000;
	link_delay +=
		(response_rx_timestamp.nanoseconds * 1LL -
		 request_tx_timestamp.nanoseconds);

	turn_around =
		((remote_resp_tx_timestamp.seconds_ms * 1LL -
		  remote_req_rx_timestamp.seconds_ms) << 32) * 1000000000;
	turn_around +=
		(remote_resp_tx_timestamp.seconds_ls * 1LL -
		 remote_req_rx_timestamp.seconds_ls) * 1000000000;
	turn_around +=
		(remote_resp_tx_timestamp.nanoseconds * 1LL -
		 remote_req_rx_timestamp.nanoseconds);

	// Adjust turn-around time for peer to local clock rate difference
	// TODO: Are these .998 and 1.002 specifically defined in the standard?
	// Should we create a define for them ?
	if
		( port->getPeerRateOffset() > .998 &&
		  port->getPeerRateOffset() < 1.002 ) {
		turn_around = (int64_t)
			(turn_around * port->getPeerRateOffset());
	}

	GPTP_LOG_VERBOSE
		("Turn Around Adjustment %Lf",
		 ((long long)turn_around * port->getPeerRateOffset()) /
		 1000000000000LL);
	GPTP_LOG_VERBOSE
		("Step #1: Turn Around Adjustment %Lf",
		 ((long long)turn_around * port->getPeerRateOffset()));
	GPTP_LOG_VERBOSE("Adjusted Peer turn around is %Lu", turn_around);

	/* Subtract turn-around time from link delay after rate adjustment */
	link_delay -= turn_around;
	link_delay /= 2;
	GPTP_LOG_DEBUG( "Link delay: %ld ns", link_delay );

	{
		uint64_t mine_elapsed;
		uint64_t theirs_elapsed;
		Timestamp prev_peer_ts_mine;
		Timestamp prev_peer_ts_theirs;
		FrequencyRatio rate_offset;
		if( port->getPeerOffset( prev_peer_ts_mine, prev_peer_ts_theirs )) {
			FrequencyRatio upper_ratio_limit, lower_ratio_limit;
			upper_ratio_limit =
				PPM_OFFSET_TO_RATIO(UPPER_LIMIT_PPM);
			lower_ratio_limit =
				PPM_OFFSET_TO_RATIO(LOWER_LIMIT_PPM);

			mine_elapsed =  TIMESTAMP_TO_NS(request_tx_timestamp) -
				TIMESTAMP_TO_NS(prev_peer_ts_mine);
			theirs_elapsed =
				TIMESTAMP_TO_NS(remote_req_rx_timestamp) -
				TIMESTAMP_TO_NS(prev_peer_ts_theirs);
			theirs_elapsed -= port->getLinkDelay();
			theirs_elapsed += link_delay < 0 ? 0 : link_delay;
			rate_offset =  ((FrequencyRatio) mine_elapsed)
						       / theirs_elapsed;

			if( rate_offset < upper_ratio_limit &&
			    rate_offset > lower_ratio_limit )
				port->setPeerRateOffset(rate_offset);
		}
	}
	if( !port->setLinkDelay( link_delay ))
	{
		// Profile-specific neighbor delay threshold handling: some profiles don't enforce strict thresholds
		if( eport->getProfile().neighbor_prop_delay_thresh > 0 )
		{
			GPTP_LOG_ERROR( "Link delay %ld beyond "
					"neighborPropDelayThresh %ld; "
					"not AsCapable", link_delay, eport->getProfile().neighbor_prop_delay_thresh );
			port->setAsCapable( false );
		}
		else
		{
			GPTP_LOG_STATUS( "Link delay %ld beyond threshold but profile allows flexible delay handling", link_delay );
		}
	} else
	{
		// Profile-specific PDelay success handling: only for profiles that enforce strict PDelay requirements
		if( eport->getProfile().min_pdelay_successes > 0 )
		{
			// Profile-specific PDelay success handling based on configuration
			unsigned int pdelay_count = port->getPdelayCount();
			unsigned int min_successes = eport->getProfile().min_pdelay_successes;
			unsigned int max_successes = eport->getProfile().max_pdelay_successes;
			
			// Reset consecutive late/missing response counters on successful processing
			eport->setConsecutiveLateResponses(0);
			eport->setConsecutiveMissingResponses(0);
			
			if( pdelay_count >= min_successes && (max_successes == 0 || pdelay_count <= max_successes) ) {
				// Profile: Set asCapable=true after required successful exchanges
				GPTP_LOG_STATUS("*** %s COMPLIANCE: Setting asCapable=true after %d successful PDelay exchanges (requirement: %d-%s) ***", 
					eport->getProfile().profile_name.c_str(), pdelay_count, min_successes, 
					(max_successes == 0) ? "unlimited" : std::to_string(max_successes).c_str());
				port->setAsCapable( true );
			} else if( pdelay_count >= min_successes ) {
				// Profile: Keep asCapable=true after initial establishment
				port->setAsCapable( true );
			} else {
				// Profile: Less than required successful exchanges, keep current state
				unsigned needed = min_successes - pdelay_count;
				long long next_interval = ((long long)(pow((double)2,eport->getPDelayInterval())*1000000000.0));
				double estimated_time_to_capable = needed * (next_interval / 1000000000.0);
				
				GPTP_LOG_STATUS("*** %s COMPLIANCE: PDelay success %d/%d - need %d more before setting asCapable=true ***", 
					eport->getProfile().profile_name.c_str(), pdelay_count, min_successes, needed);
				GPTP_LOG_STATUS("*** ASCAPABLE PROGRESS: Estimated time to asCapable=true: %.1f seconds (assuming no timeouts) ***", 
					estimated_time_to_capable);
				GPTP_LOG_STATUS("*** ASCAPABLE TIMING: Next PDelay request in %.1f seconds ***", 
					next_interval / 1000000000.0);
			}
		}
		else
		{
			GPTP_LOG_STATUS("PDelay success handling disabled - profile does not enforce strict PDelay requirements");
		}
	}
	port->setPeerOffset( request_tx_timestamp, remote_req_rx_timestamp );

done:
	// Clean up and exit
	GPTP_LOG_STATUS("*** PDELAY FOLLOWUP DEBUG: Done label reached - exiting processMessage");
	goto defer;

 abort:
	GPTP_LOG_ERROR("*** PDELAY FOLLOWUP DEBUG: Aborting PDelay followup processing - timer NOT cancelled");
	delete resp;
	eport->setLastPDelayResp(NULL);

	_gc = true;

defer:
	GPTP_LOG_STATUS("*** PDELAY FOLLOWUP DEBUG: Releasing PDelay RX lock and returning");
	eport->putPDelayRxLock();
	GPTP_LOG_STATUS("*** PDELAY FOLLOWUP DEBUG: processMessage function exit");
	return;
}

bool PTPMessagePathDelayRespFollowUp::sendPort
( EtherPort *port, PortIdentity *destIdentity )
{
	uint8_t buf_t[256];
	uint8_t *buf_ptr = buf_t + port->getPayloadOffset();
	unsigned char tspec_msg_t = 0;
	Timestamp responseOriginTimestamp_BE;
	memset(buf_t, 0, 256);
	/* Create packet in buf
	   Copy in common header */
	messageLength = PTP_COMMON_HDR_LENGTH + PTP_PDELAY_RESP_LENGTH;
	tspec_msg_t |= messageType & 0xF;
	buildCommonHeader(buf_ptr);
	responseOriginTimestamp_BE.seconds_ms =
		PLAT_htons(responseOriginTimestamp.seconds_ms);
	responseOriginTimestamp_BE.seconds_ls =
		PLAT_htonl(responseOriginTimestamp.seconds_ls);
	responseOriginTimestamp_BE.nanoseconds =
		PLAT_htonl(responseOriginTimestamp.nanoseconds);

	// Copy in v2 PDelay_Req specific fields
	requestingPortIdentity->getClockIdentityString
		(buf_ptr + PTP_PDELAY_FOLLOWUP_REQ_CLOCK_ID
		 (PTP_PDELAY_FOLLOWUP_OFFSET));
	requestingPortIdentity->getPortNumberNO
		((uint16_t *)
		 (buf_ptr + PTP_PDELAY_FOLLOWUP_REQ_PORT_ID
		  (PTP_PDELAY_FOLLOWUP_OFFSET)));
	memcpy
		(buf_ptr + PTP_PDELAY_FOLLOWUP_SEC_MS(PTP_PDELAY_FOLLOWUP_OFFSET),
		 &(responseOriginTimestamp_BE.seconds_ms),
		 sizeof(responseOriginTimestamp.seconds_ms));
	memcpy
		(buf_ptr + PTP_PDELAY_FOLLOWUP_SEC_LS(PTP_PDELAY_FOLLOWUP_OFFSET),
		 &(responseOriginTimestamp_BE.seconds_ls),
		 sizeof(responseOriginTimestamp.seconds_ls));
	memcpy
		(buf_ptr + PTP_PDELAY_FOLLOWUP_NSEC(PTP_PDELAY_FOLLOWUP_OFFSET),
		 &(responseOriginTimestamp_BE.nanoseconds),
		 sizeof(responseOriginTimestamp.nanoseconds));

	GPTP_LOG_VERBOSE("PDelay Resp Timestamp: %u,%u",
		   responseOriginTimestamp.seconds_ls,
		   responseOriginTimestamp.nanoseconds);

	port->sendGeneralPort(PTP_ETHERTYPE, buf_t, messageLength, MCAST_PDELAY, destIdentity);
	port->incCounter_ieee8021AsPortStatTxPdelayResponseFollowUp();

	return true;
}

void PTPMessagePathDelayRespFollowUp::setRequestingPortIdentity
(PortIdentity * identity)
{
	*requestingPortIdentity = *identity;
}


//
// Signalling Message
//
PTPMessageSignalling::PTPMessageSignalling(void)
{
}

PTPMessageSignalling::PTPMessageSignalling
( EtherPort *port ) : PTPMessageCommon( port )
{
	messageType = SIGNALLING_MESSAGE;
	sequenceId = port->getNextSignalSequenceId();

	targetPortIdentify = (int8_t)0xff;

	control = MESSAGE_OTHER;

	logMeanMessageInterval = 0x7F;    // 802.1AS 2011 10.5.2.2.11 logMessageInterval (Integer8)
}

 PTPMessageSignalling::~PTPMessageSignalling(void)
{
}

void PTPMessageSignalling::setintervals(int8_t linkDelayInterval, int8_t timeSyncInterval, int8_t announceInterval)
{
	tlv.setLinkDelayInterval(linkDelayInterval);
	tlv.setTimeSyncInterval(timeSyncInterval);
	tlv.setAnnounceInterval(announceInterval);
}

bool PTPMessageSignalling::sendPort
( EtherPort *port, PortIdentity *destIdentity )
{
	uint8_t buf_t[256];
	uint8_t *buf_ptr = buf_t + port->getPayloadOffset();
	unsigned char tspec_msg_t = 0x0;

	memset(buf_t, 0, 256);
	// Create packet in buf
	// Copy in common header
	messageLength = PTP_COMMON_HDR_LENGTH + PTP_SIGNALLING_LENGTH + sizeof(tlv);
	tspec_msg_t |= messageType & 0xF;
	buildCommonHeader(buf_ptr);

	memcpy(buf_ptr + PTP_SIGNALLING_TARGET_PORT_IDENTITY(PTP_SIGNALLING_OFFSET),
	       &targetPortIdentify, sizeof(targetPortIdentify));

	tlv.toByteString(buf_ptr + PTP_COMMON_HDR_LENGTH + PTP_SIGNALLING_LENGTH);

	port->sendGeneralPort(PTP_ETHERTYPE, buf_t, messageLength, MCAST_OTHER, destIdentity);

	return true;
}

void PTPMessageSignalling::processMessage( CommonPort *port )
{
	long long unsigned int waitTime;

	GPTP_LOG_STATUS("Signalling Link Delay Interval: %d", tlv.getLinkDelayInterval());
	GPTP_LOG_STATUS("Signalling Sync Interval: %d", tlv.getTimeSyncInterval());
	GPTP_LOG_STATUS("Signalling Announce Interval: %d", tlv.getAnnounceInterval());

	char linkDelayInterval = tlv.getLinkDelayInterval();
	char timeSyncInterval = tlv.getTimeSyncInterval();
	char announceInterval = tlv.getAnnounceInterval();

	if (linkDelayInterval == PTPMessageSignalling::sigMsgInterval_Initial) {
		port->resetInitPDelayInterval();

		waitTime = ((long long) (pow(2.0, port->getPDelayInterval()) *  1000000000.0));
		waitTime = waitTime > EVENT_TIMER_GRANULARITY ? waitTime : EVENT_TIMER_GRANULARITY;
		port->startPDelayIntervalTimer(waitTime);
		GPTP_LOG_DEBUG("Link delay interval reset to initial value: %d (%.3f seconds)", 
			port->getPDelayInterval(), pow(2.0, (double)port->getPDelayInterval()));
	}
	else if (linkDelayInterval == PTPMessageSignalling::sigMsgInterval_NoSend) {
		port->stopPDelayIntervalTimer();
		GPTP_LOG_STATUS("PDelay message transmission stopped per signaling request");
	}
	else if (linkDelayInterval == PTPMessageSignalling::sigMsgInterval_NoChange) {
		// Nothing to do
	}
	else {
		port->setPDelayInterval(linkDelayInterval);

		waitTime = ((long long) (pow(2.0, port->getPDelayInterval()) *  1000000000.0));
		waitTime = waitTime > EVENT_TIMER_GRANULARITY ? waitTime : EVENT_TIMER_GRANULARITY;
		port->startPDelayIntervalTimer(waitTime);
		GPTP_LOG_DEBUG("Link delay interval set to: %d (%.3f seconds)", 
			port->getPDelayInterval(), pow(2.0, (double)port->getPDelayInterval()));
	}

	if (timeSyncInterval == PTPMessageSignalling::sigMsgInterval_Initial) {
		port->resetInitSyncInterval();

		waitTime = ((long long) (pow((double)2, port->getSyncInterval()) *  1000000000.0));
		waitTime = waitTime > EVENT_TIMER_GRANULARITY ? waitTime : EVENT_TIMER_GRANULARITY;
		port->startSyncIntervalTimer(waitTime);
	}
	else if (timeSyncInterval == PTPMessageSignalling::sigMsgInterval_NoSend) {
		port->stopSyncIntervalTimer();
		GPTP_LOG_STATUS("Sync message transmission stopped per signaling request");
	}
	else if (timeSyncInterval == PTPMessageSignalling::sigMsgInterval_NoChange) {
		// Nothing to do
	}
	else {
		port->setSyncInterval(timeSyncInterval);

		waitTime = ((long long) (pow((double)2, port->getSyncInterval()) *  1000000000.0));
		waitTime = waitTime > EVENT_TIMER_GRANULARITY ? waitTime : EVENT_TIMER_GRANULARITY;
		port->startSyncIntervalTimer(waitTime);
	}

	// Profile-specific announce interval handling: only for profiles with BMCA support
	if (port->getProfile().supports_bmca) {
		if (announceInterval == PTPMessageSignalling::sigMsgInterval_Initial) {
			// Set announce interval to profile's initial (default) value
			int8_t initial_interval = port->getProfileAnnounceInterval();
			port->setAnnounceInterval(initial_interval);
			
			waitTime = ((long long) (pow((double)2, port->getAnnounceInterval()) *  1000000000.0));
			waitTime = waitTime > EVENT_TIMER_GRANULARITY ? waitTime : EVENT_TIMER_GRANULARITY;
			port->startAnnounceIntervalTimer(waitTime);
			
			GPTP_LOG_STATUS("Announce interval reset to profile initial value: %d (%.3f seconds)", 
				initial_interval, pow(2.0, (double)initial_interval));
		}
		else if (announceInterval == PTPMessageSignalling::sigMsgInterval_NoSend) {
			// Stop sending announce messages by stopping the timer
			port->stopAnnounceIntervalTimer();
			GPTP_LOG_STATUS("Announce message transmission stopped per signaling request");
		}
		else if (announceInterval == PTPMessageSignalling::sigMsgInterval_NoChange) {
			// Nothing to do
		}
		else {
			port->setAnnounceInterval(announceInterval);

			waitTime = ((long long) (pow((double)2, port->getAnnounceInterval()) *  1000000000.0));
			waitTime = waitTime > EVENT_TIMER_GRANULARITY ? waitTime : EVENT_TIMER_GRANULARITY;
			port->startAnnounceIntervalTimer(waitTime);
		}
	}
	else {
		GPTP_LOG_STATUS("Announce interval signaling ignored - BMCA disabled per profile configuration");
	}
}
