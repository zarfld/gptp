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

#include <ether_port.hpp>
#include <avbts_message.hpp>
#include <avbts_clock.hpp>

#include <avbts_oslock.hpp>
#include <avbts_osnet.hpp>
#include <avbts_oscondition.hpp>
#include <ether_tstamper.hpp>

#include <gptp_log.hpp>
#include <gptp_cfg.hpp>

#include <stdio.h>

#include <math.h>

#include <stdlib.h>

LinkLayerAddress EtherPort::other_multicast(OTHER_MULTICAST);
LinkLayerAddress EtherPort::pdelay_multicast(PDELAY_MULTICAST);
LinkLayerAddress EtherPort::test_status_multicast
( TEST_STATUS_MULTICAST );

OSThreadExitCode watchNetLinkWrapper(void *arg)
{
	EtherPort *port;

	GPTP_LOG_STATUS("*** watchNetLinkWrapper() called ***");
	port = (EtherPort *) arg;
	GPTP_LOG_STATUS("*** Calling port->watchNetLink() ***");
	if (port->watchNetLink() == NULL)
		return osthread_ok;
	else
		return osthread_error;
}

OSThreadExitCode openPortWrapper(void *arg)
{
	EtherPort *port;

	port = (EtherPort *) arg;
	if (port->openPort(port) == NULL)
		return osthread_ok;
	else
		return osthread_error;
}

EtherPort::~EtherPort()
{
	delete port_ready_condition;
}

EtherPort::EtherPort( PortInit_t *portInit ) :
	CommonPort( portInit )
{
	linkUp = portInit->linkUp;
	setTestMode( portInit->testMode );

	pdelay_sequence_id = 0;

	pdelay_started = false;
	pdelay_halted = false;
	sync_rate_interval_timer_started = false;

	duplicate_resp_counter = 0;
	last_invalid_seqid = 0;

	operLogPdelayReqInterval = portInit->operLogPdelayReqInterval;
	operLogSyncInterval = portInit->operLogSyncInterval;

	if (getAutomotiveProfile())
	{
		setAsCapable( true );

		if (getInitSyncInterval() == LOG2_INTERVAL_INVALID)
			setInitSyncInterval( -5 );     // 31.25 ms
		if (getInitPDelayInterval() == LOG2_INTERVAL_INVALID)
			setInitPDelayInterval( 0 );  // 1 second
		if (operLogPdelayReqInterval == LOG2_INTERVAL_INVALID)
			operLogPdelayReqInterval = 0;      // 1 second
		if (operLogSyncInterval == LOG2_INTERVAL_INVALID)
			operLogSyncInterval = 0;           // 1 second
	} 
	else	if (getProfile().profile_name == "milan")
	{
		// Milan profile asCapable initialization per specification
		setAsCapable(getProfile().initial_as_capable);  // Milan starts FALSE, earns via PDelay
		
		// Milan-specific timing requirements for fast convergence
		if (getInitSyncInterval() == LOG2_INTERVAL_INVALID)
			setInitSyncInterval(getProfile().sync_interval_log);  // 125ms (-3) default
		if (getInitPDelayInterval() == LOG2_INTERVAL_INVALID)
			setInitPDelayInterval(getProfile().pdelay_interval_log);  // 1 second (0) default
		if (operLogPdelayReqInterval == LOG2_INTERVAL_INVALID)
			operLogPdelayReqInterval = getProfile().pdelay_interval_log;
		if (operLogSyncInterval == LOG2_INTERVAL_INVALID)
			operLogSyncInterval = getProfile().sync_interval_log;
			
		// Set announce interval for Milan profile
		setAnnounceInterval(getProfile().announce_interval_log);
		
		GPTP_LOG_STATUS("*** MILAN PROFILE ENABLED *** (convergence target: %dms, sync interval: %.3fms)", 
			getProfile().max_convergence_time_ms,
			pow(2.0, (double)getProfile().sync_interval_log) * 1000.0);
	}
	else if (getProfile().profile_name == "avnu_base")
	{
		// AVnu Base profile asCapable initialization per specification
		setAsCapable(getProfile().initial_as_capable);  // AVnu Base starts FALSE, earns via 2-10 PDelay
		
		// AVnu Base/ProAV profile timing settings
		if (getInitSyncInterval() == LOG2_INTERVAL_INVALID)
			setInitSyncInterval(getProfile().sync_interval_log);       // 1s (0) 
		if (getInitPDelayInterval() == LOG2_INTERVAL_INVALID)
			setInitPDelayInterval(getProfile().pdelay_interval_log);      // 1 second (0)
		if (operLogPdelayReqInterval == LOG2_INTERVAL_INVALID)
			operLogPdelayReqInterval = getProfile().pdelay_interval_log;    // 1 second (0)
		if (operLogSyncInterval == LOG2_INTERVAL_INVALID)
			operLogSyncInterval = getProfile().sync_interval_log;         // 1 second (0)
			
		GPTP_LOG_STATUS("*** AVNU BASE/PROAV PROFILE ENABLED *** (asCapable requires 2-10 successful PDelay exchanges)");
	}
	else if (getProfile().profile_name == "automotive")
	{
		// Automotive profile asCapable initialization per specification
		setAsCapable(getProfile().initial_as_capable);  // Automotive starts FALSE, becomes TRUE on link up
		
		// Automotive profile timing settings
		if (getInitSyncInterval() == LOG2_INTERVAL_INVALID)
			setInitSyncInterval(getProfile().sync_interval_log);       // 1s (0) 
		if (getInitPDelayInterval() == LOG2_INTERVAL_INVALID)
			setInitPDelayInterval(getProfile().pdelay_interval_log);      // 1 second (0)
		if (operLogPdelayReqInterval == LOG2_INTERVAL_INVALID)
			operLogPdelayReqInterval = getProfile().pdelay_interval_log;    // 1 second (0)
		if (operLogSyncInterval == LOG2_INTERVAL_INVALID)
			operLogSyncInterval = getProfile().sync_interval_log;         // 1 second (0)
			
		GPTP_LOG_STATUS("*** AUTOMOTIVE PROFILE ENABLED *** (asCapable immediately on link up, test status messages)");
	}
	else
	{
		// Standard IEEE 802.1AS profile
		setAsCapable(getProfile().initial_as_capable);  // Standard starts FALSE

		if (getInitSyncInterval() == LOG2_INTERVAL_INVALID)
			setInitSyncInterval(getProfile().sync_interval_log);       // Use profile default
		if (getInitPDelayInterval() == LOG2_INTERVAL_INVALID)
			setInitPDelayInterval(getProfile().pdelay_interval_log);  // Use profile default
		if (operLogPdelayReqInterval == LOG2_INTERVAL_INVALID)
			operLogPdelayReqInterval = getProfile().pdelay_interval_log;      // Use profile default
		if (operLogSyncInterval == LOG2_INTERVAL_INVALID)
			operLogSyncInterval = getProfile().sync_interval_log;           // Use profile default
			
		GPTP_LOG_STATUS("*** %s PROFILE ENABLED ***", getProfile().profile_description.c_str());
	}

	/*TODO: Add intervals below to a config interface*/
	resetInitPDelayInterval();

	last_sync = NULL;
	last_pdelay_req = NULL;
	last_pdelay_resp = NULL;
	last_pdelay_resp_fwup = NULL;

	setPdelayCount(0);
	setSyncCount(0);

	if( getAutomotiveProfile( ))
	{
		if (isGM) {
			avbSyncState = 1;
		}
		else {
			avbSyncState = 2;
		}
		if (getTestMode())
		{
			linkUpCount = 1;  // TODO : really should check the current linkup status http://stackoverflow.com/questions/15723061/how-to-check-if-interface-is-up
			linkDownCount = 0;
		}
		setStationState(STATION_STATE_RESERVED);
	}
}

bool EtherPort::_init_port( void )
{
	pdelay_rx_lock = lock_factory->createLock(oslock_recursive);
	port_tx_lock = lock_factory->createLock(oslock_recursive);

	pDelayIntervalTimerLock = lock_factory->createLock(oslock_recursive);

	port_ready_condition = condition_factory->createCondition();

	return true;
}

void EtherPort::startPDelay()
{
	GPTP_LOG_STATUS("*** DEBUG: startPDelay() called, pdelayHalted=%s, automotive=%s, milan=%s, avnu_base=%s ***", 
		pdelayHalted() ? "true" : "false",
		getAutomotiveProfile() ? "true" : "false", 
		getMilanProfile() ? "true" : "false",
		getAvnuBaseProfile() ? "true" : "false");
	
	if(!pdelayHalted()) {
		if( getAutomotiveProfile( ))
		{
			if( getPDelayInterval() !=
			    PTPMessageSignalling::sigMsgInterval_NoSend)
			{
				GPTP_LOG_STATUS("*** DEBUG: Automotive profile - starting PDelay timer with EVENT_TIMER_GRANULARITY ***");
				pdelay_started = true;
				startPDelayIntervalTimer(EVENT_TIMER_GRANULARITY);
			}
		}
		else {
			// Non-automotive profile - use profile-configured PDelay interval
			uint64_t pdelay_interval_ns = (uint64_t)(pow(2.0, (double)getPDelayInterval()) * 1000000000.0);
			GPTP_LOG_STATUS("*** DEBUG: Non-automotive profile - starting PDelay timer with %llu ns interval (%.3f s) ***", 
				pdelay_interval_ns, pdelay_interval_ns / 1000000000.0);
			pdelay_started = true;
			startPDelayIntervalTimer(pdelay_interval_ns);
		}
	} else {
		GPTP_LOG_WARNING("*** DEBUG: PDelay is halted, not starting timer ***");
	}
}

void EtherPort::stopPDelay()
{
	haltPdelay(true);
	pdelay_started = false;
	clock->deleteEventTimerLocked( this, PDELAY_INTERVAL_TIMEOUT_EXPIRES);
}

void EtherPort::startSyncRateIntervalTimer()
{
	if( getAutomotiveProfile( ))
	{
		sync_rate_interval_timer_started = true;
		if (isGM) {
			// GM will wait up to 8  seconds for signaling rate
			// TODO: This isn't according to spec but set because it is believed that some slave devices aren't signalling
			//  to reduce the rate
			clock->addEventTimerLocked( this, SYNC_RATE_INTERVAL_TIMEOUT_EXPIRED, 8000000000 );
		}
		else {
			// Slave will time out after 4 seconds
			clock->addEventTimerLocked( this, SYNC_RATE_INTERVAL_TIMEOUT_EXPIRED, 4000000000 );
		}
	}
}

void EtherPort::processMessage
( char *buf, int length, LinkLayerAddress *remote, uint32_t link_speed )
{
	GPTP_LOG_VERBOSE("Processing network buffer");

	PTPMessageCommon *msg =
		buildPTPMessage( buf, (int)length, remote, this );

	if (msg == NULL)
	{
		GPTP_LOG_ERROR("Discarding invalid message");
		return;
	}
	GPTP_LOG_VERBOSE("Processing message type: %d", msg->getMessageType());
	
	// Enhanced debug for PDelay Request messages
	if (msg->getMessageType() == PATH_DELAY_REQ_MESSAGE) {
		GPTP_LOG_INFO("*** RECEIVED PDelay Request - calling processMessage");
		GPTP_LOG_STATUS("*** PDELAY DEBUG: Received PDelay Request seq=%u from source", msg->getSequenceId());
		
		// Log source port identity
		PortIdentity sourcePortId;
		msg->getPortIdentity(&sourcePortId);
		ClockIdentity clockId = sourcePortId.getClockIdentity();
		uint16_t portNum;
		sourcePortId.getPortNumber(&portNum);
		
		uint8_t clockBytes[PTP_CLOCK_IDENTITY_LENGTH];
		clockId.getIdentityString(clockBytes);
		GPTP_LOG_STATUS("*** PDELAY DEBUG: Source Clock ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, Port: %u",
			clockBytes[0], clockBytes[1], clockBytes[2], clockBytes[3], 
			clockBytes[4], clockBytes[5], clockBytes[6], clockBytes[7], portNum);
		
		// Log reception timestamp
		if (msg->isEvent()) {
			Timestamp rxTime = msg->getTimestamp();
			GPTP_LOG_STATUS("*** PDELAY DEBUG: RX timestamp: %llu.%09u", 
				(unsigned long long)rxTime.seconds_ls, rxTime.nanoseconds);
		}
		
		// Log port state
		GPTP_LOG_STATUS("*** PDELAY DEBUG: Port state: %d, asCapable: %s", 
			getPortState(), getAsCapable() ? "true" : "false");
	}

	// Enhanced debug for PDelay Response messages
	if (msg->getMessageType() == PATH_DELAY_RESP_MESSAGE) {
		GPTP_LOG_INFO("*** RECEIVED PDelay Response - calling processMessage");
		GPTP_LOG_STATUS("*** PDELAY RESPONSE DEBUG: Received PDelay Response seq=%u from source", msg->getSequenceId());
		
		// Log source port identity
		PortIdentity sourcePortId;
		msg->getPortIdentity(&sourcePortId);
		ClockIdentity clockId = sourcePortId.getClockIdentity();
		uint16_t portNum;
		sourcePortId.getPortNumber(&portNum);
		
		uint8_t clockBytes[PTP_CLOCK_IDENTITY_LENGTH];
		clockId.getIdentityString(clockBytes);
		GPTP_LOG_STATUS("*** PDELAY RESPONSE DEBUG: Source Clock ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, Port: %u",
			clockBytes[0], clockBytes[1], clockBytes[2], clockBytes[3], 
			clockBytes[4], clockBytes[5], clockBytes[6], clockBytes[7], portNum);
		
		// Log reception timestamp
		if (msg->isEvent()) {
			Timestamp rxTime = msg->getTimestamp();
			GPTP_LOG_STATUS("*** PDELAY RESPONSE DEBUG: RX timestamp: %llu.%09u", 
				(unsigned long long)rxTime.seconds_ls, rxTime.nanoseconds);
		}
		
		// Log port state
		GPTP_LOG_STATUS("*** PDELAY RESPONSE DEBUG: Port state: %d, asCapable: %s", 
			getPortState(), getAsCapable() ? "true" : "false");
	}

	if( msg->isEvent() )
	{
		Timestamp rx_timestamp = msg->getTimestamp();
		Timestamp phy_compensation = getRxPhyDelay( link_speed );
		GPTP_LOG_DEBUG( "RX PHY compensation: %s sec",
			 phy_compensation.toString().c_str() );
		phy_compensation._version = rx_timestamp._version;
		rx_timestamp = rx_timestamp - phy_compensation;
		msg->setTimestamp( rx_timestamp );
	}

	msg->processMessage(this);
	if (msg->garbage())
		delete msg;
}

void *EtherPort::openPort( EtherPort *port )
{
	port_ready_condition->signal();

	setListeningThreadRunning(true);

	while ( getListeningThreadRunning() ) {
		uint8_t buf[128];
		LinkLayerAddress remote;
		net_result rrecv;
		size_t length = sizeof(buf);
		uint32_t link_speed;

		if ( ( rrecv = recv( &remote, buf, length, link_speed ))
		     == net_succeed )
		{
			processMessage
				((char *)buf, (int)length, &remote, link_speed );
		} else if (rrecv == net_fatal) {
			GPTP_LOG_ERROR("read from network interface failed");
			this->processEvent(FAULT_DETECTED);
			break;
		}
	}
	GPTP_LOG_DEBUG("Listening thread terminated ...");
	return NULL;
}

net_result EtherPort::port_send
( uint16_t etherType, uint8_t *buf, int size, MulticastType mcast_type,
  PortIdentity *destIdentity, bool timestamp )
{
	LinkLayerAddress dest;

	if (mcast_type != MCAST_NONE) {
		if (mcast_type == MCAST_PDELAY) {
			dest = pdelay_multicast;
		}
		else if (mcast_type == MCAST_TEST_STATUS) {
			dest = test_status_multicast;
		}
		else {
			dest = other_multicast;
		}
	} else {
		mapSocketAddr(destIdentity, &dest);
	}

	return send(&dest, etherType, (uint8_t *) buf, size, timestamp);
}

void EtherPort::sendEventPort
( uint16_t etherType, uint8_t *buf, int size, MulticastType mcast_type,
  PortIdentity *destIdentity, uint32_t *link_speed )
{
	net_result rtx = port_send
		( etherType, buf, size, mcast_type, destIdentity, true );
	if( rtx != net_succeed )
	{
		GPTP_LOG_ERROR("sendEventPort(): failure");
		*link_speed = INVALID_LINKSPEED;
		return;
	}

	*link_speed = this->getLinkSpeed();

	return;
}

void EtherPort::sendGeneralPort
( uint16_t etherType, uint8_t *buf, int size, MulticastType mcast_type,
  PortIdentity * destIdentity )
{
	net_result rtx = port_send(etherType, buf, size, mcast_type, destIdentity, false);
	if (rtx != net_succeed) {
		GPTP_LOG_ERROR("sendGeneralPort(): failure");
	}

	return;
}

bool EtherPort::_processEvent( Event e )
{
	bool ret = false;

	switch (e) {
	case POWERUP:
	case INITIALIZE:
		if( !getAutomotiveProfile( ))
		{
			if ( getPortState() != PTP_SLAVE &&
			     getPortState() != PTP_MASTER )
			{
				GPTP_LOG_STATUS("Starting PDelay");
				startPDelay();
			}
		}
		else {
			startPDelay();
		}

		port_ready_condition->wait_prelock();

		GPTP_LOG_STATUS("*** ATTEMPTING TO START LINK WATCH THREAD ***");
		if( !linkWatch(watchNetLinkWrapper, (void *)this) )
		{
			GPTP_LOG_ERROR("*** FAILED TO CREATE LINK WATCH THREAD ***");
			ret = false;
			break;
		}
		GPTP_LOG_STATUS("*** LINK WATCH THREAD STARTED SUCCESSFULLY ***");

		GPTP_LOG_STATUS("*** ATTEMPTING TO START LISTENING THREAD ***");
		if( !linkOpen(openPortWrapper, (void *)this) )
		{
			GPTP_LOG_ERROR("Error creating port thread");
			ret = false;
			break;
		}

		port_ready_condition->wait();

		if( getAutomotiveProfile( ))
		{
			setStationState(STATION_STATE_ETHERNET_READY);
			if (getTestMode())
			{
				APMessageTestStatus *testStatusMsg = new APMessageTestStatus(this);
				if (testStatusMsg) {
					testStatusMsg->sendPort(this);
					delete testStatusMsg;
				}
			}
			if (!isGM) {
				// Send an initial signalling message
				PTPMessageSignalling *sigMsg = new PTPMessageSignalling(this);
				if (sigMsg) {
					sigMsg->setintervals(PTPMessageSignalling::sigMsgInterval_NoSend, getSyncInterval(), PTPMessageSignalling::sigMsgInterval_NoSend);
					sigMsg->sendPort(this, NULL);
					delete sigMsg;
				}

				startSyncReceiptTimer((unsigned long long)
					 (SYNC_RECEIPT_TIMEOUT_MULTIPLIER *
					  ((double) pow((double)2, getSyncInterval()) *
					   1000000000.0)));
			}
		}

		ret = true;
		break;
	case STATE_CHANGE_EVENT:
		// Profile-specific handling of state change events
		if (getProfile().bmca_enabled == false)
		{
			// Profiles with BMCA disabled (e.g., Automotive) do nothing
			ret = true;
			GPTP_LOG_STATUS("*** %s PROFILE: STATE_CHANGE_EVENT - BMCA disabled ***", 
				getProfile().profile_name.c_str());
		}
		else
		{
			// Profiles with BMCA enabled allow default processing
			ret = false;  // Allow default BMCA processing
			GPTP_LOG_STATUS("*** %s PROFILE: STATE_CHANGE_EVENT - BMCA enabled ***", 
				getProfile().profile_name.c_str());
		}
		break;
	case LINKUP:
		haltPdelay(false);
		startPDelay();
		
		// Profile-specific link up handling
		if (getProfile().profile_name == "automotive") {
			// Automotive profile: Set asCapable immediately on link up
			if (getProfile().as_capable_on_link_up) {
				setAsCapable(true);
				GPTP_LOG_STATUS("*** AUTOMOTIVE LINKUP *** (asCapable set TRUE immediately)");
			} else {
				GPTP_LOG_STATUS("*** AUTOMOTIVE LINKUP ***");
			}
		} else if (getProfile().max_convergence_time_ms > 0) {
			GPTP_LOG_STATUS("*** %s LINKUP *** (Target convergence: %dms)", 
				getProfile().profile_name.c_str(), getProfile().max_convergence_time_ms);
		} else {
			GPTP_LOG_STATUS("*** %s LINKUP ***", getProfile().profile_name.c_str());
		}

		GPTP_LOG_STATUS("*** LINKUP BMCA DECISION: Priority1=%d, PortState=%d ***", 
			clock->getPriority1(), getPortState());
		if( clock->getPriority1() == 255 || getPortState() == PTP_SLAVE ) {
			GPTP_LOG_STATUS("*** BECOMING SLAVE (Priority1=255 or PTP_SLAVE) ***");
			becomeSlave( true );
		} else if( getPortState() == PTP_MASTER ) {
			GPTP_LOG_STATUS("*** BECOMING MASTER (Already PTP_MASTER) ***");
			becomeMaster( true );
		} else {
			uint64_t timeout_ns = (uint64_t)( ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER * 
				pow( 2.0, getAnnounceInterval( )) * 1000000000.0 );
			GPTP_LOG_STATUS("*** STARTING ANNOUNCE RECEIPT TIMEOUT: %llu ns (%.1f seconds) ***", 
				timeout_ns, timeout_ns / 1000000000.0);
			clock->addEventTimerLocked
				( this, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES, timeout_ns );
		}

		if( getAutomotiveProfile( ))
		{
			setAsCapable( true );

			setStationState(STATION_STATE_ETHERNET_READY);
			if (getTestMode())
			{
				APMessageTestStatus *testStatusMsg = new APMessageTestStatus(this);
				if (testStatusMsg) {
					testStatusMsg->sendPort(this);
					delete testStatusMsg;
				}
			}

			resetInitSyncInterval();
			setAnnounceInterval( 0 );
			resetInitPDelayInterval();

			if (!isGM) {
				// Send an initial signaling message
				PTPMessageSignalling *sigMsg = new PTPMessageSignalling(this);
				if (sigMsg) {
					sigMsg->setintervals(PTPMessageSignalling::sigMsgInterval_NoSend, getSyncInterval(), PTPMessageSignalling::sigMsgInterval_NoSend);
					sigMsg->sendPort(this, NULL);
					delete sigMsg;
				}

				startSyncReceiptTimer((unsigned long long)
					 (SYNC_RECEIPT_TIMEOUT_MULTIPLIER *
					  ((double) pow((double)2, getSyncInterval()) *
					   1000000000.0)));
			}

			// Reset Sync count and pdelay count
			setPdelayCount(0);
			setSyncCount(0);

			// Start AVB SYNC at 2. It will decrement after each sync. When it reaches 0 the Test Status message
			// can be sent
			if (isGM) {
				avbSyncState = 1;
			}
			else {
				avbSyncState = 2;
			}

			if (getTestMode())
			{
				linkUpCount++;
			}
		}
		this->timestamper_reset();

		ret = true;
		break;
	case LINKDOWN:
		stopPDelay();
		if( getAutomotiveProfile( ))
		{
			GPTP_LOG_EXCEPTION("LINK DOWN");
		}
		else {
			setAsCapable(false);
			GPTP_LOG_STATUS("LINK DOWN");
		}
		if (getTestMode())
		{
			linkDownCount++;
		}

		ret = true;
		break;
	case ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES:
	case SYNC_RECEIPT_TIMEOUT_EXPIRES:
		if( !getAutomotiveProfile( ))
		{
			ret = false;
			break;
		}

		// Automotive Profile specific action
		if (e == SYNC_RECEIPT_TIMEOUT_EXPIRES) {
			GPTP_LOG_EXCEPTION("SYNC receipt timeout");

			startSyncReceiptTimer((unsigned long long)
					      (SYNC_RECEIPT_TIMEOUT_MULTIPLIER *
					       ((double) pow((double)2, getSyncInterval()) *
						1000000000.0)));
		}
		ret = true;
		break;
	case PDELAY_INTERVAL_TIMEOUT_EXPIRES:
		GPTP_LOG_STATUS("*** DEBUG: PDELAY_INTERVAL_TIMEOUT_EXPIRES occurred - sending PDelay request ***");
		{
			Timestamp req_timestamp;

			PTPMessagePathDelayReq *pdelay_req =
			    new PTPMessagePathDelayReq(this);
			PortIdentity dest_id;
			getPortIdentity(dest_id);
			pdelay_req->setPortIdentity(&dest_id);

			{
				Timestamp pending =
				    PDELAY_PENDING_TIMESTAMP;
				pdelay_req->setTimestamp(pending);
			}

			if (last_pdelay_req != NULL) {
				delete last_pdelay_req;
			}
			setLastPDelayReq(pdelay_req);

			getTxLock();
			pdelay_req->sendPort(this, NULL);
			GPTP_LOG_DEBUG("*** Sent PDelay Request message");
			
			// Milan profile: track when PDelay request was sent and reset response flag
			if( getMilanProfile() ) {
				setLastPDelayReqTimestamp(clock->getTime());
				setPDelayResponseReceived(false);
				GPTP_LOG_DEBUG("*** MILAN: Tracking PDelay request sent at timestamp for late response detection ***");
			}
			
			putTxLock();

			{
				long long timeout;
				long long interval;

				timeout = PDELAY_RESP_RECEIPT_TIMEOUT_MULTIPLIER *
					((long long)
					 (pow((double)2,getPDelayInterval())*1000000000.0));

				timeout = timeout > EVENT_TIMER_GRANULARITY ?
					timeout : EVENT_TIMER_GRANULARITY;
				clock->addEventTimerLocked
					(this, PDELAY_RESP_RECEIPT_TIMEOUT_EXPIRES, timeout );
				GPTP_LOG_DEBUG("Schedule PDELAY_RESP_RECEIPT_TIMEOUT_EXPIRES, "
					"PDelay interval %d, timeout %lld",
					getPDelayInterval(), timeout);

				interval =
					((long long)
					 (pow((double)2,getPDelayInterval())*1000000000.0));
				interval = interval > EVENT_TIMER_GRANULARITY ?
					interval : EVENT_TIMER_GRANULARITY;
				GPTP_LOG_STATUS("*** DEBUG: Restarting PDelay timer with interval=%lld ns (%.3f ms) ***", 
					interval, interval / 1000000.0);
				startPDelayIntervalTimer(interval);
			}
		}
		break;
	case SYNC_INTERVAL_TIMEOUT_EXPIRES:
		{
			/* Set offset from master to zero, update device vs
			   system time offset */

			// Send a sync message and then a followup to broadcast
			PTPMessageSync *sync = new PTPMessageSync(this);
			PortIdentity dest_id;
			bool tx_succeed;
			getPortIdentity(dest_id);
			sync->setPortIdentity(&dest_id);
			getTxLock();
			tx_succeed = sync->sendPort(this, NULL);
			GPTP_LOG_DEBUG("Sent SYNC message");

			if( getAutomotiveProfile() &&
			    getPortState() == PTP_MASTER )
			{
				if (avbSyncState > 0) {
					avbSyncState--;
					if (avbSyncState == 0) {
						// Send Avnu Automotive Profile status message
						setStationState(STATION_STATE_AVB_SYNC);
						if (getTestMode()) {
							APMessageTestStatus *testStatusMsg = new APMessageTestStatus(this);
							if (testStatusMsg) {
								testStatusMsg->sendPort(this);
								delete testStatusMsg;
							}
						}
					}
				}
			}
			putTxLock();

			if ( tx_succeed )
			{
				Timestamp sync_timestamp = sync->getTimestamp();

				GPTP_LOG_VERBOSE("Successful Sync timestamp");
				GPTP_LOG_VERBOSE("Seconds: %u",
						 sync_timestamp.seconds_ls);
				GPTP_LOG_VERBOSE("Nanoseconds: %u",
						 sync_timestamp.nanoseconds);

				PTPMessageFollowUp *follow_up = new PTPMessageFollowUp(this);
				PortIdentity dest_id;
				getPortIdentity(dest_id);

				follow_up->setClockSourceTime(getClock()->getFUPInfo());
				follow_up->setPortIdentity(&dest_id);
				follow_up->setSequenceId(sync->getSequenceId());
				follow_up->setPreciseOriginTimestamp
					(sync_timestamp);
				follow_up->sendPort(this, NULL);
				delete follow_up;
			} else {
				GPTP_LOG_ERROR
					("*** Unsuccessful Sync timestamp");
			}
			delete sync;
		}
		break;
	case FAULT_DETECTED:
		GPTP_LOG_ERROR("Received FAULT_DETECTED event");
		if( !getAutomotiveProfile( ))
		{
			setAsCapable(false);
		}
		break;
	case PDELAY_DEFERRED_PROCESSING:
		GPTP_LOG_DEBUG("PDELAY_DEFERRED_PROCESSING occured");
		pdelay_rx_lock->lock();
		if (last_pdelay_resp_fwup == NULL) {
			GPTP_LOG_ERROR("PDelay Response Followup is NULL!");
			abort();
		}
		last_pdelay_resp_fwup->processMessage(this);
		if (last_pdelay_resp_fwup->garbage()) {
			delete last_pdelay_resp_fwup;
			this->setLastPDelayRespFollowUp(NULL);
		}
		pdelay_rx_lock->unlock();
		break;
	case PDELAY_RESP_RECEIPT_TIMEOUT_EXPIRES:
		if( !getAutomotiveProfile( ))
		{
			GPTP_LOG_EXCEPTION("PDelay Response Receipt Timeout");
			
			// Milan Specification 5.6.2.4 compliance:
			// asCapable should be TRUE after 2-5 successful PDelay exchanges
			// Only disable asCapable if we haven't met the Milan requirement
			if( getMilanProfile() ) {
				// Check if this is a truly missing response vs a late one that never arrived
				bool response_received = getPDelayResponseReceived();
				if( !response_received ) {
					// Truly missing response
					unsigned missing_count = getConsecutiveMissingResponses() + 1;
					setConsecutiveMissingResponses(missing_count);
					setConsecutiveLateResponses(0); // Reset late count
					
					GPTP_LOG_STATUS("*** MILAN COMPLIANCE: PDelay response missing (consecutive missing: %d) ***", missing_count);
					
					// Milan: Only set asCapable=false after multiple consecutive missing responses
					// AND only if we haven't established the minimum 2 successful exchanges
					if( getPdelayCount() < 2 ) {
						// Haven't reached minimum requirement yet
						GPTP_LOG_STATUS("*** MILAN COMPLIANCE: asCapable remains false - need %d more successful PDelay exchanges (current: %d/2 minimum) ***", 
							2 - getPdelayCount(), getPdelayCount());
					} else if( missing_count >= 3 ) {
						// Had successful exchanges before, but now 3+ consecutive missing - this indicates link failure
						GPTP_LOG_STATUS("*** MILAN COMPLIANCE: %d consecutive missing responses after %d successful exchanges - setting asCapable=false ***", 
							missing_count, getPdelayCount());
						setAsCapable(false);
					} else {
						// Had successful exchanges, temporary missing response - maintain asCapable
						GPTP_LOG_STATUS("*** MILAN COMPLIANCE: %d missing response(s) after %d successful exchanges - maintaining asCapable=true ***", 
							missing_count, getPdelayCount());
					}
				} else {
					// Response was received but processed as late - don't count as missing
					GPTP_LOG_STATUS("*** MILAN COMPLIANCE: PDelay response was late but received - not counting as missing ***");
				}
			} else if( getAvnuBaseProfile() ) {
				if( getPdelayCount() < 2 ) {
					// AVnu Base/ProAV: Haven't reached minimum 2 successful PDelay exchanges yet
					GPTP_LOG_STATUS("*** AVNU BASE/PROAV COMPLIANCE: asCapable remains false - need %d more successful PDelay exchanges (current: %d/2 minimum) ***", 
						2 - getPdelayCount(), getPdelayCount());
					setAsCapable(false);  // Keep asCapable=false until requirement met
				} else {
					// AVnu Base/ProAV: Had successful exchanges before, temporary timeout - don't immediately disable
					GPTP_LOG_STATUS("*** AVNU BASE/PROAV COMPLIANCE: PDelay timeout after %d successful exchanges - maintaining asCapable=true ***", getPdelayCount());
				}
			} else {
				// Standard profile: disable asCapable on timeout
				setAsCapable(false);
			}
		}
		
		// Reset pdelay_count based on profile behavior
		if( getMilanProfile() ) {
			// Milan: Only reset pdelay_count if we're losing asCapable or haven't established it yet
			if( !getAsCapable() || getPdelayCount() < 2 ) {
				setPdelayCount( 0 );
				GPTP_LOG_STATUS("*** MILAN: Resetting pdelay_count due to asCapable=false or insufficient exchanges ***");
			} else {
				GPTP_LOG_STATUS("*** MILAN: Maintaining pdelay_count=%d with asCapable=true ***", getPdelayCount());
			}
		} else {
			// Other profiles: always reset on timeout
			setPdelayCount( 0 );
		}
		break;

	case PDELAY_RESP_PEER_MISBEHAVING_TIMEOUT_EXPIRES:
		GPTP_LOG_EXCEPTION("PDelay Resp Peer Misbehaving timeout expired! Restarting PDelay");

		haltPdelay(false);
		if( getPortState() != PTP_SLAVE &&
		    getPortState() != PTP_MASTER )
		{
			GPTP_LOG_STATUS("Starting PDelay" );
			startPDelay();
		}
		break;
	case SYNC_RATE_INTERVAL_TIMEOUT_EXPIRED:
		{
			GPTP_LOG_INFO("SYNC_RATE_INTERVAL_TIMEOUT_EXPIRED occured");

			sync_rate_interval_timer_started = false;

			bool sendSignalMessage = false;
			if ( getSyncInterval() != operLogSyncInterval )
			{
				setSyncInterval( operLogSyncInterval );
				sendSignalMessage = true;
			}

			if( getPDelayInterval() != operLogPdelayReqInterval)
			{
				setPDelayInterval( operLogPdelayReqInterval );
				sendSignalMessage = true;
			}

			if (sendSignalMessage) {
				if (!isGM) {
				// Send operational signalling message
					PTPMessageSignalling *sigMsg = new PTPMessageSignalling(this);
					if (sigMsg) {
						if( getAutomotiveProfile( ))
							sigMsg->setintervals(PTPMessageSignalling::sigMsgInterval_NoChange, getSyncInterval(), PTPMessageSignalling::sigMsgInterval_NoChange);
						else
							sigMsg->setintervals(getPDelayInterval(), getSyncInterval(), PTPMessageSignalling::sigMsgInterval_NoChange);
						sigMsg->sendPort(this, NULL);
						delete sigMsg;
					}

					startSyncReceiptTimer((unsigned long long)
						 (SYNC_RECEIPT_TIMEOUT_MULTIPLIER *
						  ((double) pow((double)2, getSyncInterval()) *
						   1000000000.0)));
				}
			}
		}

		break;
	default:
		GPTP_LOG_ERROR
		  ( "Unhandled event type in "
		    "EtherPort::processEvent(), %d", e );
		ret = false;
		break;
	}

	return ret;
}

void EtherPort::recoverPort( void )
{
	return;
}

void EtherPort::becomeMaster( bool annc ) {
	setPortState( PTP_MASTER );
	// Stop announce receipt timeout timer
	clock->deleteEventTimerLocked( this, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES );

	// Stop sync receipt timeout timer
	stopSyncReceiptTimer();

	if( annc ) {
		if( !getAutomotiveProfile( ))
		{
			startAnnounce();
		}
	}
	startSyncIntervalTimer(16000000);
	GPTP_LOG_STATUS("Switching to Master" );

	clock->updateFUPInfo();

	return;
}

void EtherPort::becomeSlave( bool restart_syntonization ) {
	clock->deleteEventTimerLocked( this, ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES );
	clock->deleteEventTimerLocked( this, SYNC_INTERVAL_TIMEOUT_EXPIRES );

	setPortState( PTP_SLAVE );

	if( !getAutomotiveProfile( ))
	{
		clock->addEventTimerLocked
		  (this, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES,
		   (ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER*
			(unsigned long long)
			(pow((double)2,getAnnounceInterval())*1000000000.0)));
	}

	GPTP_LOG_STATUS("Switching to Slave" );
	if( restart_syntonization ) clock->newSyntonizationSetPoint();

	getClock()->updateFUPInfo();

	return;
}

void EtherPort::mapSocketAddr
( PortIdentity *destIdentity, LinkLayerAddress *remote )
{
	*remote = identity_map[*destIdentity];
	return;
}

void EtherPort::addSockAddrMap
( PortIdentity *destIdentity, LinkLayerAddress *remote )
{
	identity_map[*destIdentity] = *remote;
	return;
}

int EtherPort::getTxTimestamp
( PTPMessageCommon *msg, Timestamp &timestamp, unsigned &counter_value,
  bool last )
{
	PortIdentity identity;
	msg->getPortIdentity(&identity);
	return getTxTimestamp
		(&identity, msg->getMessageId(), timestamp, counter_value, last);
}

int EtherPort::getRxTimestamp
( PTPMessageCommon * msg, Timestamp & timestamp, unsigned &counter_value,
  bool last )
{
	PortIdentity identity;
	msg->getPortIdentity(&identity);
	return getRxTimestamp
		(&identity, msg->getMessageId(), timestamp, counter_value, last);
}

int EtherPort::getTxTimestamp
(PortIdentity *sourcePortIdentity, PTPMessageId messageId,
 Timestamp &timestamp, unsigned &counter_value, bool last )
{
	EtherTimestamper *timestamper =
		dynamic_cast<EtherTimestamper *>(_hw_timestamper);
	if (timestamper)
	{
		return timestamper->HWTimestamper_txtimestamp
			( sourcePortIdentity, messageId, timestamp,
			  counter_value, last );
	}
	GPTP_LOG_ERROR("No hardware timestamper available, falling back to system time (TX)");
	timestamp = clock->getSystemTime();
	return 0;
}

int EtherPort::getRxTimestamp
( PortIdentity * sourcePortIdentity, PTPMessageId messageId,
  Timestamp &timestamp, unsigned &counter_value, bool last )
{
	EtherTimestamper *timestamper =
		dynamic_cast<EtherTimestamper *>(_hw_timestamper);
	if (timestamper)
	{
		return timestamper->HWTimestamper_rxtimestamp
		    (sourcePortIdentity, messageId, timestamp, counter_value,
		     last);
	}
	timestamp = clock->getSystemTime();
	return 0;
}

void EtherPort::startPDelayIntervalTimer
( long long unsigned int waitTime )
{
	GPTP_LOG_STATUS("*** DEBUG: startPDelayIntervalTimer() called with waitTime=%llu ns (%.3f ms) ***", 
		waitTime, waitTime / 1000000.0);
	pDelayIntervalTimerLock->lock();
	clock->deleteEventTimerLocked(this, PDELAY_INTERVAL_TIMEOUT_EXPIRES);
	clock->addEventTimerLocked(this, PDELAY_INTERVAL_TIMEOUT_EXPIRES, waitTime);
	pDelayIntervalTimerLock->unlock();
	GPTP_LOG_STATUS("*** DEBUG: PDelay interval timer set successfully ***");
}

void EtherPort::syncDone() {
	GPTP_LOG_VERBOSE("Sync complete");

	if( getAutomotiveProfile() && getPortState() == PTP_SLAVE )
	{
		if (avbSyncState > 0) {
			avbSyncState--;
			if (avbSyncState == 0) {
				setStationState(STATION_STATE_AVB_SYNC);
				if (getTestMode()) {
					APMessageTestStatus *testStatusMsg =
						new APMessageTestStatus(this);
					if (testStatusMsg) {
						testStatusMsg->sendPort(this);
						delete testStatusMsg;
					}
				}
			}
		}
	}

	if( getAutomotiveProfile( ))
	{
		if (!sync_rate_interval_timer_started) {
			if ( getSyncInterval() != operLogSyncInterval )
			{
				startSyncRateIntervalTimer();
			}
		}
	}

	if( !pdelay_started ) {
		startPDelay();
	}
}
