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

#ifdef _WIN32
#include <windows.h>
#endif

#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#endif

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
	GPTP_LOG_STATUS("*** openPortWrapper() called (thread_id=%lu, arg=%p) ***", (unsigned long)GetCurrentThreadId(), arg);
	
	try {
		port = (EtherPort *) arg;
		GPTP_LOG_STATUS("*** Calling port->openPort() (thread_id=%lu, port=%p) ***", (unsigned long)GetCurrentThreadId(), port);
		void* result = port->openPort(port);
		GPTP_LOG_STATUS("*** port->openPort() returned %p (thread_id=%lu, port=%p) ***", result, (unsigned long)GetCurrentThreadId(), port);
		if (result == NULL)
			return osthread_ok;
		else
			return osthread_error;
	} catch (const std::exception& ex) {
		GPTP_LOG_ERROR("*** EXCEPTION in openPortWrapper: %s (thread_id=%lu) ***", ex.what(), (unsigned long)GetCurrentThreadId());
		return osthread_error;
	} catch (...) {
		GPTP_LOG_ERROR("*** UNKNOWN EXCEPTION in openPortWrapper (thread_id=%lu) ***", (unsigned long)GetCurrentThreadId());
		return osthread_error;
	}
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

	// Initialize asCapable based on profile configuration (PAL approach)
	setAsCapable(shouldSetAsCapableOnStartup());

	// Set profile-specific intervals from profile configuration (unified approach)
	if (getInitSyncInterval() == LOG2_INTERVAL_INVALID)
		setInitSyncInterval(getProfileSyncInterval());
	if (getInitPDelayInterval() == LOG2_INTERVAL_INVALID)
		setInitPDelayInterval(getProfilePDelayInterval());
	if (operLogPdelayReqInterval == LOG2_INTERVAL_INVALID)
		operLogPdelayReqInterval = getProfilePDelayInterval();
	if (operLogSyncInterval == LOG2_INTERVAL_INVALID)
		operLogSyncInterval = getProfileSyncInterval();
		
	// Set announce interval from profile configuration
	setAnnounceInterval(getProfileAnnounceInterval());
	
	// Log profile activation with configuration details
	GPTP_LOG_STATUS("*** %s PROFILE ENABLED *** (sync: %.3fms, convergence target: %dms)", 
		getProfile().profile_description.c_str(),
		pow(2.0, (double)getProfileSyncInterval()) * 1000.0,
		getProfile().max_convergence_time_ms);

	/*TODO: Add intervals below to a config interface*/
	resetInitPDelayInterval();

	last_sync = NULL;
	last_pdelay_req = NULL;
	last_pdelay_resp = NULL;
	last_pdelay_resp_fwup = NULL;

	setPdelayCount(0);
	setSyncCount(0);

	// Initialize automotive-specific state if automotive test status is enabled
	if( getProfile().automotive_test_status )
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
	GPTP_LOG_DEBUG("startPDelay() called, pdelayHalted=%s, active_profile=%s ***", 
		pdelayHalted() ? "true" : "false",
		getProfile().profile_name.c_str());
	
	if(!pdelayHalted()) {
		if( getPDelayInterval() != PTPMessageSignalling::sigMsgInterval_NoSend) {
			// Use profile-configured PDelay interval for all profiles
			uint64_t pdelay_interval_ns = (uint64_t)(pow(2.0, (double)getPDelayInterval()) * 1000000000.0);
			GPTP_LOG_DEBUG("%s profile - starting PDelay timer with %llu ns interval (%.3f s) ***", 
				getProfile().profile_name.c_str(), pdelay_interval_ns, pdelay_interval_ns / 1000000000.0);
			pdelay_started = true;
			startPDelayIntervalTimer(pdelay_interval_ns);
		}
	} else {
		GPTP_LOG_WARNING("*** DEBUG: PDelay is halted, not starting timer ***");
	}
}

void EtherPort::startSyncRateIntervalTimer()
{
	// Start sync rate interval timer for profiles that require it (automotive profile)
	if( getProfile().automotive_test_status )
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

	// Log incoming message details at entry
	GPTP_LOG_DEBUG("*** MSG PROCESSING: Processing %d bytes from network", length);
	
	Timestamp process_start_time = clock->getTime();
	
	PTPMessageCommon *msg =
		buildPTPMessage( buf, (int)length, remote, this );

	if (msg == NULL)
	{
		GPTP_LOG_ERROR("*** MSG PROCESSING: Discarding invalid message (%d bytes)", length);
		// Log first few bytes for debugging
		if (length >= 8) {
			GPTP_LOG_DEBUG("*** MSG PROCESSING: Invalid packet header: %02x %02x %02x %02x %02x %02x %02x %02x", 
				buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
		}
		return;
	}
	
	// Log all message types with details
	const char* msgTypeStr = "UNKNOWN";
	switch(msg->getMessageType()) {
		case SYNC_MESSAGE: msgTypeStr = "SYNC"; break;
		case DELAY_REQ_MESSAGE: msgTypeStr = "DELAY_REQ"; break;
		case PATH_DELAY_REQ_MESSAGE: msgTypeStr = "PDELAY_REQ"; break;
		case PATH_DELAY_RESP_MESSAGE: msgTypeStr = "PDELAY_RESP"; break;
		case FOLLOWUP_MESSAGE: msgTypeStr = "FOLLOWUP"; break;
		case DELAY_RESP_MESSAGE: msgTypeStr = "DELAY_RESP"; break;
		case PATH_DELAY_FOLLOWUP_MESSAGE: msgTypeStr = "PDELAY_FOLLOWUP"; break;
		case ANNOUNCE_MESSAGE: msgTypeStr = "ANNOUNCE"; break;
		case SIGNALLING_MESSAGE: msgTypeStr = "SIGNALLING"; break;
		case MANAGEMENT_MESSAGE: msgTypeStr = "MANAGEMENT"; break;
	}
	
	GPTP_LOG_STATUS("*** MSG RX: %s (type=%d, seq=%u, len=%d)", 
		msgTypeStr, msg->getMessageType(), msg->getSequenceId(), length);
	
	// Log source identity for all messages
	PortIdentity sourcePortId;
	msg->getPortIdentity(&sourcePortId);
	ClockIdentity clockId = sourcePortId.getClockIdentity();
	uint16_t portNum;
	sourcePortId.getPortNumber(&portNum);
	
	uint8_t clockBytes[PTP_CLOCK_IDENTITY_LENGTH];
	clockId.getIdentityString(clockBytes);
	GPTP_LOG_DEBUG("*** MSG RX: Source: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%u", 
		clockBytes[0], clockBytes[1], clockBytes[2], clockBytes[3], 
		clockBytes[4], clockBytes[5], clockBytes[6], clockBytes[7], portNum);
	
	GPTP_LOG_VERBOSE("Processing message type: %d", msg->getMessageType());
	
	// Enhanced debug for PDelay Request messages
	if (msg->getMessageType() == PATH_DELAY_REQ_MESSAGE) {
		GPTP_LOG_INFO("*** RECEIVED PDelay Request - calling processMessage");
		GPTP_LOG_STATUS("*** PDELAY DEBUG: Received PDelay Request seq=%u from source", msg->getSequenceId());
		
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

	// Enhanced debug for Sync messages
	if (msg->getMessageType() == SYNC_MESSAGE) {
		GPTP_LOG_STATUS("*** SYNC DEBUG: Received Sync seq=%u", msg->getSequenceId());
		if (msg->isEvent()) {
			Timestamp rxTime = msg->getTimestamp();
			GPTP_LOG_DEBUG("*** SYNC DEBUG: RX timestamp: %llu.%09u", 
				(unsigned long long)rxTime.seconds_ls, rxTime.nanoseconds);
		}
	}

	// Enhanced debug for Announce messages
	if (msg->getMessageType() == ANNOUNCE_MESSAGE) {
		GPTP_LOG_STATUS("*** ANNOUNCE DEBUG: Received Announce seq=%u", msg->getSequenceId());
	}

	// Enhanced debug for Signalling messages
	if (msg->getMessageType() == SIGNALLING_MESSAGE) {
		GPTP_LOG_STATUS("*** SIGNALLING DEBUG: Received Signalling seq=%u", msg->getSequenceId());
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
    // Stack canary to detect stack overflow
    const uint32_t stack_canary = 0xDEADBEEF;
    volatile uint32_t stack_check = stack_canary;
    
    GPTP_LOG_STATUS("*** EtherPort::openPort ENTRY (thread_id=%lu, port=%p, stack_canary=%08x, stack_ptr=%p) ***", 
        (unsigned long)GetCurrentThreadId(), port, stack_canary, (void*)&stack_check);
    
    GPTP_LOG_STATUS("*** NETWORK THREAD: About to signal port_ready_condition ***");
    if (!port_ready_condition) {
        GPTP_LOG_ERROR("*** FATAL: port_ready_condition is NULL! ***");
        return (void*)1;
    }
    port_ready_condition->signal();
    GPTP_LOG_STATUS("*** NETWORK THREAD: port_ready_condition->signal() completed ***");
    
    GPTP_LOG_STATUS("*** NETWORK THREAD: About to set listening thread running ***");
    setListeningThreadRunning(true);
    GPTP_LOG_STATUS("*** NETWORK THREAD: setListeningThreadRunning(true) completed ***");

    // Heartbeat: set initial value with defensive checks
    if (!port) {
        GPTP_LOG_ERROR("*** FATAL: EtherPort::openPort called with NULL port pointer! ***");
        return (void*)1;
    }
    
    GPTP_LOG_STATUS("*** NETWORK THREAD: About to initialize heartbeat ***");
    // Initialize heartbeat with defensive checks (removed SEH to avoid C2713/C2712 errors)
    try {
        network_thread_heartbeat.store(0, std::memory_order_relaxed);
        LARGE_INTEGER qpc_init;
        if (QueryPerformanceCounter(&qpc_init) == 0) {
            GPTP_LOG_ERROR("*** ERROR: QueryPerformanceCounter failed during initialization ***");
            // Fall back to GetTickCount64 if QPC fails
            network_thread_last_activity.store(GetTickCount64(), std::memory_order_relaxed);
        } else {
            network_thread_last_activity.store((uint64_t)qpc_init.QuadPart, std::memory_order_relaxed);
        }
        GPTP_LOG_STATUS("*** NETWORK THREAD: Heartbeat initialization completed ***");
    } catch (...) {
        GPTP_LOG_ERROR("*** FATAL: Exception initializing heartbeat in openPort (thread_id=%lu, port=%p) ***", (unsigned long)GetCurrentThreadId(), port);
        return (void*)1;
    }

    GPTP_LOG_STATUS("*** NETWORK THREAD: Starting packet reception loop ***");
    uint64_t loop_counter = 0;
    Timestamp last_activity_time;
    
    // Defensive check for clock pointer before accessing it
    if (!clock) {
        GPTP_LOG_ERROR("*** FATAL: clock pointer is NULL in network thread! ***");
        return (void*)1;
    }
    
    try {
        last_activity_time = clock->getTime();
        GPTP_LOG_DEBUG("*** NETWORK THREAD: Successfully got initial timestamp from clock ***");
    } catch (const std::exception& ex) {
        GPTP_LOG_ERROR("*** FATAL: Exception getting time from clock: %s ***", ex.what());
        return (void*)1;
    } catch (...) {
        GPTP_LOG_ERROR("*** FATAL: Unknown exception getting time from clock ***");
        return (void*)1;
    }
    
    try {
        while ( getListeningThreadRunning() ) {
            GPTP_LOG_DEBUG("*** NETWORK THREAD: LOOP START (loop_counter=%llu, thread_id=%lu, stack_ptr=%p) ***", loop_counter, (unsigned long)GetCurrentThreadId(), (void*)&loop_counter);
            uint8_t buf[128];
            LinkLayerAddress remote;
            net_result rrecv;
            size_t length = sizeof(buf);
            uint32_t link_speed;
            
            loop_counter++;

            // Heartbeat: update on every loop with defensive checks
            try {
                if (&network_thread_heartbeat != nullptr && &network_thread_last_activity != nullptr) {
                    network_thread_heartbeat.fetch_add(1, std::memory_order_relaxed);
                    LARGE_INTEGER qpc_loop;
                    if (QueryPerformanceCounter(&qpc_loop) != 0) {
                        network_thread_last_activity.store((uint64_t)qpc_loop.QuadPart, std::memory_order_relaxed);
                    } else {
                        GPTP_LOG_ERROR("*** ERROR: QueryPerformanceCounter failed in main loop ***");
                    }
                } else {
                    GPTP_LOG_ERROR("*** FATAL: Heartbeat pointers are null in main loop ***");
                    break;
                }
            } catch (...) {
                GPTP_LOG_ERROR("*** FATAL: Exception updating heartbeat in main loop (loop_counter=%llu, thread_id=%lu) ***", loop_counter, (unsigned long)GetCurrentThreadId());
                break;
            }

            // Log thread activity every 100 loops to prove thread is alive
            if (loop_counter % 100 == 0) {
                Timestamp current_time = clock->getTime();
                Timestamp time_diff = current_time - last_activity_time;
                uint64_t time_diff_ms = (uint64_t)time_diff.seconds_ls * 1000 + time_diff.nanoseconds / 1000000;
                GPTP_LOG_STATUS("*** NETWORK THREAD: Loop #%llu, thread alive, last_activity=%llu ms ago, heartbeat=%llu", 
                    loop_counter, time_diff_ms, network_thread_heartbeat.load(std::memory_order_relaxed));
            }

            if (loop_counter == 1) {
                GPTP_LOG_STATUS("*** NETWORK THREAD: First entry into receive loop (loop_counter=1) ***");
            }

            // Log before every lock/unlock/wait/timer operation
            GPTP_LOG_DEBUG("*** NETWORK THREAD: About to check port_ready_condition (signal) (loop_counter=%llu) ***", loop_counter);
            // port_ready_condition->signal(); // Already signaled at top

            // Log before every recv call
            GPTP_LOG_DEBUG("*** NETWORK THREAD: About to call recv() - loop #%llu", loop_counter);
            // Add a flush to ensure log is written before possible crash
            fflush(stdout);
            fflush(stderr);
            rrecv = recv( &remote, buf, length, link_speed );
            GPTP_LOG_DEBUG("*** NETWORK THREAD: recv() returned %d - loop #%llu", rrecv, loop_counter);

            if ( rrecv == net_succeed )
            {
                last_activity_time = clock->getTime();
            	// Log all incoming packets at network level
            	GPTP_LOG_DEBUG("*** NETWORK RX: Received %zu bytes from network, link_speed=%u", 
            		length, link_speed);
            	// Log raw packet header info if it looks like PTP
            	if (length >= 34) { // Minimum PTP packet size
            		uint16_t messageType = buf[0] & 0x0F;
            		uint16_t seqId = (buf[30] << 8) | buf[31];
            		GPTP_LOG_DEBUG("*** NETWORK RX: PTP-like packet - messageType=%u, seqId=%u, length=%zu", 
            			messageType, seqId, length);
            	}
            	// Log before calling processMessage to check if it blocks
            	GPTP_LOG_DEBUG("*** NETWORK RX: About to call processMessage for %zu bytes (loop_counter=%llu) ***", length, loop_counter);
                processMessage((char *)buf, (int)length, &remote, link_speed );
                GPTP_LOG_DEBUG("*** NETWORK RX: processMessage completed successfully (loop_counter=%llu) ***", loop_counter);
            } else if (rrecv == net_fatal) {
                GPTP_LOG_ERROR("*** NETWORK THREAD: Fatal error in network receive - terminating (loop #%llu) ***", loop_counter);
                this->processEvent(FAULT_DETECTED);
                break;
            } else if (rrecv == net_trfail) {
                GPTP_LOG_DEBUG("*** NETWORK RX: Temporary receive failure (net_trfail) (loop #%llu)", loop_counter);
            } else {
                GPTP_LOG_DEBUG("*** NETWORK RX: Receive returned: %d (loop #%llu)", rrecv, loop_counter);
            }

            // Check if getListeningThreadRunning() changed
            if (!getListeningThreadRunning()) {
                GPTP_LOG_STATUS("*** NETWORK THREAD: getListeningThreadRunning() returned false - exiting loop (loop #%llu) ***", loop_counter);
                break;
            }
            GPTP_LOG_DEBUG("*** NETWORK THREAD: LOOP END (loop_counter=%llu, thread_id=%lu, stack_ptr=%p) ***", loop_counter, (unsigned long)GetCurrentThreadId(), (void*)&loop_counter);
        }
    } catch (const std::exception& ex) {
        GPTP_LOG_ERROR("*** NETWORK THREAD: Unhandled std::exception caught: %s (loop_counter=%llu) ***", ex.what(), loop_counter);
    } catch (...) {
        GPTP_LOG_ERROR("*** NETWORK THREAD: Unhandled unknown exception caught (loop_counter=%llu) ***", loop_counter);
    }
    
    // Final stack canary check
    if (stack_check != stack_canary) {
        GPTP_LOG_ERROR("*** FATAL: Stack overflow detected at exit! Canary corrupted: expected=%08x, got=%08x ***", 
            stack_canary, stack_check);
    }
    
    GPTP_LOG_ERROR("*** NETWORK THREAD: Listening thread terminated - loop_counter=%llu (reason: loop exit) ***", loop_counter);
    setListeningThreadRunning(false);
    return NULL;
}

net_result EtherPort::port_send
( uint16_t etherType, uint8_t *buf, int size, MulticastType mcast_type,
  PortIdentity *destIdentity, bool timestamp )
{
	LinkLayerAddress dest;

	// Log outgoing packet details
	const char* mcastStr = "UNICAST";
	switch(mcast_type) {
		case MCAST_PDELAY: mcastStr = "PDELAY_MCAST"; break;
		case MCAST_TEST_STATUS: mcastStr = "TEST_STATUS_MCAST"; break;
		case MCAST_OTHER: mcastStr = "OTHER_MCAST"; break;
		case MCAST_NONE: mcastStr = "UNICAST"; break;
	}
	
	// Extract message type from buffer if it looks like PTP
	uint8_t messageType = 0xFF;
	uint16_t seqId = 0;
	if (size >= 32) {
		messageType = buf[0] & 0x0F;
		seqId = (buf[30] << 8) | buf[31];
	}
	
	GPTP_LOG_STATUS("*** MSG TX: Sending %d bytes, type=%u, seq=%u, %s, timestamp=%s", 
		size, messageType, seqId, mcastStr, timestamp ? "true" : "false");

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
		if (destIdentity) {
			ClockIdentity clockId = destIdentity->getClockIdentity();
			uint16_t portNum;
			destIdentity->getPortNumber(&portNum);
			uint8_t clockBytes[PTP_CLOCK_IDENTITY_LENGTH];
			clockId.getIdentityString(clockBytes);
			GPTP_LOG_DEBUG("*** MSG TX: Dest: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%u", 
				clockBytes[0], clockBytes[1], clockBytes[2], clockBytes[3], 
				clockBytes[4], clockBytes[5], clockBytes[6], clockBytes[7], portNum);
		}
	}

	net_result result = send(&dest, etherType, (uint8_t *) buf, size, timestamp);
	
	if (result != net_succeed) {
		GPTP_LOG_ERROR("*** MSG TX: Send failed with result: %d", result);
	} else {
		GPTP_LOG_DEBUG("*** MSG TX: Send successful");
	}
	
	return result;
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
    bool listenThreadOk = false; // Moved declaration here to avoid C2360 error

    switch (e) {
    case POWERUP:
    case INITIALIZE:
        // Start PDelay based on profile configuration
        if( shouldStartPDelayOnLinkUp() )
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
        GPTP_LOG_DEBUG("*** NETWORK THREAD: port_ready_condition->wait_prelock() returned (thread_id=%lu, stack_ptr=%p) ***", (unsigned long)GetCurrentThreadId(), (void*)&ret);

        GPTP_LOG_STATUS("*** ATTEMPTING TO START LINK WATCH THREAD ***");
        if( !linkWatch(watchNetLinkWrapper, (void *)this) )
        {
            GPTP_LOG_ERROR("*** FAILED TO CREATE LINK WATCH THREAD ***");
            ret = false;
            break;
        }
        GPTP_LOG_STATUS("*** LINK WATCH THREAD STARTED SUCCESSFULLY ***");

        GPTP_LOG_STATUS("*** ATTEMPTING TO START LISTENING THREAD ***");
        GPTP_LOG_DEBUG("About to call linkOpen(openPortWrapper, this) (thread_id=%lu, stack_ptr=%p)", (unsigned long)GetCurrentThreadId(), (void*)this);
        listenThreadOk = linkOpen(openPortWrapper, (void *)this);
        GPTP_LOG_DEBUG("linkOpen(openPortWrapper, this) returned %d (thread_id=%lu, stack_ptr=%p)", (int)listenThreadOk, (unsigned long)GetCurrentThreadId(), (void*)this);
        if( !listenThreadOk )
        {
            GPTP_LOG_ERROR("Error creating port thread (listening thread)!");
            ret = false;
            break;
        }
        GPTP_LOG_STATUS("*** LISTENING THREAD STARTED SUCCESSFULLY ***");

        port_ready_condition->wait();
        GPTP_LOG_DEBUG("*** NETWORK THREAD: port_ready_condition->wait() returned (thread_id=%lu, stack_ptr=%p) ***", (unsigned long)GetCurrentThreadId(), (void*)&ret);

        if( getProfile().automotive_test_status )
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
		
		// Profile-specific link up handling - use property-driven logic
		if (getProfile().as_capable_on_link_up) {
			setAsCapable(true);
			GPTP_LOG_STATUS("*** %s LINKUP *** (asCapable set TRUE immediately per profile config)", 
				getProfile().profile_name.c_str());
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

		// Profile-specific behavior: set asCapable based on profile configuration
		if( getProfile().initial_as_capable )
		{
			setAsCapable( true );
		}

		// Profile-specific behavior: send test status messages if enabled
		if( getProfile().automotive_test_status )
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
		
		// Profile-specific link down behavior: use property to control asCapable setting
		if( getProfile().as_capable_on_link_down )
		{
			// Profile maintains asCapable state on link down (e.g., automotive)
			GPTP_LOG_EXCEPTION("LINK DOWN (maintaining asCapable per profile config)");
		}
		else {
			// Standard behavior: set asCapable=false on link down
			setAsCapable(false);
			GPTP_LOG_STATUS("LINK DOWN (asCapable set to false)");
		}
		if (getTestMode())
		{
			linkDownCount++;
		}

		ret = true;
		break;
	case ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES:
	case SYNC_RECEIPT_TIMEOUT_EXPIRES:
		// Profile-specific timeout handling: strict timeout handling for some profiles
		if( !getProfile().requires_strict_timeouts )
		{
			ret = false;
			break;
		}

		// Profile-specific action for strict timeout handling
		if (e == SYNC_RECEIPT_TIMEOUT_EXPIRES) {
			GPTP_LOG_EXCEPTION("SYNC receipt timeout (strict timeout handling enabled)");

			startSyncReceiptTimer((unsigned long long)
					      (SYNC_RECEIPT_TIMEOUT_MULTIPLIER *
					       ((double) pow((double)2, getSyncInterval()) *
						1000000000.0)));
		}
		ret = true;
		break;
	case PDELAY_INTERVAL_TIMEOUT_EXPIRES:
		GPTP_LOG_DEBUG("PDELAY_INTERVAL_TIMEOUT_EXPIRES occurred - sending PDelay request ***");
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
			
			// Profile-specific late response tracking: track when PDelay request was sent
			if( getProfile().late_response_threshold_ms > 0 ) {
				setLastPDelayReqTimestamp(clock->getTime());
				setPDelayResponseReceived(false);
				GPTP_LOG_DEBUG("*** %s: Tracking PDelay request sent at timestamp for late response detection ***", 
					getProfile().profile_name.c_str());
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
				GPTP_LOG_DEBUG("Restarting PDelay timer with interval=%lld ns (%.3f ms) ***", 
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

			// Profile-specific sync state and test status handling
			if( getProfile().automotive_test_status &&
			    getPortState() == PTP_MASTER )
			{
				if (avbSyncState > 0) {
					avbSyncState--;
					if (avbSyncState == 0) {
						// Send profile-specific status message
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
		// Profile-specific fault handling: some profiles maintain asCapable on faults
		if( getProfile().as_capable_on_link_down )
		{
			// Profile maintains asCapable state on faults (e.g., automotive)
			GPTP_LOG_STATUS("FAULT_DETECTED - maintaining asCapable per profile config");
		}
		else {
			// Standard behavior: set asCapable=false on fault
			setAsCapable(false);
			GPTP_LOG_STATUS("FAULT_DETECTED - asCapable set to false");
		}
		break;
	case PDELAY_DEFERRED_PROCESSING:
		GPTP_LOG_DEBUG("*** NETWORK THREAD: About to acquire pdelay_rx_lock (thread_id=%lu, stack_ptr=%p) ***", (unsigned long)GetCurrentThreadId(), (void*)&e);
		pdelay_rx_lock->lock();
		GPTP_LOG_DEBUG("*** NETWORK THREAD: Acquired pdelay_rx_lock (thread_id=%lu, stack_ptr=%p) ***", (unsigned long)GetCurrentThreadId(), (void*)&e);
		if (last_pdelay_resp_fwup == NULL) {
			GPTP_LOG_ERROR("PDelay Response Followup is NULL! About to abort().");
			abort();
		}
		last_pdelay_resp_fwup->processMessage(this);
		if (last_pdelay_resp_fwup->garbage()) {
			delete last_pdelay_resp_fwup;
			this->setLastPDelayRespFollowUp(NULL);
		}
		GPTP_LOG_DEBUG("*** NETWORK THREAD: About to release pdelay_rx_lock (thread_id=%lu, stack_ptr=%p) ***", (unsigned long)GetCurrentThreadId(), (void*)&e);
		pdelay_rx_lock->unlock();
		GPTP_LOG_DEBUG("*** NETWORK THREAD: Released pdelay_rx_lock (thread_id=%lu, stack_ptr=%p) ***", (unsigned long)GetCurrentThreadId(), (void*)&e);
		break;
	case PDELAY_RESP_RECEIPT_TIMEOUT_EXPIRES:
		{
		// Profile-agnostic PDelay timeout handling based on profile configuration
		GPTP_LOG_EXCEPTION("PDelay Response Receipt Timeout");
		
		// Use profile properties to determine timeout behavior
		unsigned min_pdelay_required = getProfile().min_pdelay_successes;
		bool maintain_on_timeout = getProfile().maintain_as_capable_on_timeout;
		bool reset_count_on_timeout = getProfile().reset_pdelay_count_on_timeout;
		
		// Check if this is a truly missing response vs a late one that never arrived
		bool response_received = getPDelayResponseReceived();
		if( !response_received ) {
			// Truly missing response
			unsigned missing_count = getConsecutiveMissingResponses() + 1;
		 setConsecutiveMissingResponses(missing_count);
			setConsecutiveLateResponses(0); // Reset late count
			
			GPTP_LOG_STATUS("*** %s COMPLIANCE: PDelay response missing (consecutive missing: %d) ***", 
				getProfile().profile_name.c_str(), missing_count);
			
			// Use profile configuration to determine asCapable behavior
			if( getPdelayCount() < min_pdelay_required ) {
				// Haven't reached minimum requirement yet
				GPTP_LOG_STATUS("*** %s COMPLIANCE: asCapable remains false - need %d more successful PDelay exchanges (current: %d/%d minimum) ***", 
					getProfile().profile_name.c_str(), min_pdelay_required - getPdelayCount(), getPdelayCount(), min_pdelay_required);
			} else if( missing_count >= 3 && !maintain_on_timeout ) {
				// Had successful exchanges before, but now 3+ consecutive missing - this indicates link failure
				GPTP_LOG_STATUS("*** %s COMPLIANCE: %d consecutive missing responses after %d successful exchanges - setting asCapable=false ***", 
					getProfile().profile_name.c_str(), missing_count, getPdelayCount());
				setAsCapable(false);
			} else {
				// Had successful exchanges, temporary missing response - behavior based on profile
				if( maintain_on_timeout ) {
					GPTP_LOG_STATUS("*** %s COMPLIANCE: %d missing response(s) after %d successful exchanges - maintaining asCapable=true (profile config) ***", 
						getProfile().profile_name.c_str(), missing_count, getPdelayCount());
				} else {
					GPTP_LOG_STATUS("*** %s COMPLIANCE: PDelay timeout after %d successful exchanges - disabling asCapable (profile config) ***", 
						getProfile().profile_name.c_str(), getPdelayCount());
					setAsCapable(false);
				}
			}
		} else {
			// Response was received but processed as late - don't count as missing
			GPTP_LOG_STATUS("*** %s COMPLIANCE: PDelay response was late but received - not counting as missing ***", 
				getProfile().profile_name.c_str());
		}
		
		// Reset pdelay_count based on profile configuration
		if( reset_count_on_timeout ) {
			// Profile requires reset on timeout (most profiles)
			if( !getAsCapable() || getPdelayCount() < min_pdelay_required ) {
				setPdelayCount( 0 );
				GPTP_LOG_STATUS("*** %s: Resetting pdelay_count due to asCapable=false or insufficient exchanges ***", 
					getProfile().profile_name.c_str());
			} else {
				GPTP_LOG_STATUS("*** %s: Maintaining pdelay_count=%d with asCapable=true ***", 
					getProfile().profile_name.c_str(), getPdelayCount());
			}
		} else {
			// Profile doesn't reset count on timeout
			setPdelayCount( 0 );
			GPTP_LOG_STATUS("*** %s: Always resetting pdelay_count on timeout (profile config) ***", 
				getProfile().profile_name.c_str());
		}
		}  // End of case block scope
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
						// Profile-specific signaling intervals: some profiles don't change PDelay interval
						if( getProfile().automotive_test_status )
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
		// Profile-specific announce behavior: some profiles disable announce messages
		if( getProfile().supports_bmca )
		{
			startAnnounce();
		}
		else
		{
			GPTP_LOG_STATUS("BMCA/Announce disabled per profile configuration");
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

	// Profile-specific announce receipt timeout: only for profiles with BMCA support
	if( getProfile().supports_bmca )
	{
		clock->addEventTimerLocked
		  (this, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES,
		   (ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER*
			(unsigned long long)
			(pow((double)2,getAnnounceInterval())*1000000000.0)));
	}
	else
	{
		GPTP_LOG_STATUS("BMCA/Announce receipt timeout disabled per profile configuration");
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
	GPTP_LOG_DEBUG("startPDelayIntervalTimer() called with waitTime=%llu ns (%.3f ms) ***", 
		waitTime, waitTime / 1000000.0);
    GPTP_LOG_DEBUG("*** NETWORK THREAD: About to acquire pDelayIntervalTimerLock (thread_id=%lu, stack_ptr=%p) ***", (unsigned long)GetCurrentThreadId(), (void*)&waitTime);
    
    // Check if lock pointer is valid before using it
    if (!pDelayIntervalTimerLock) {
        GPTP_LOG_ERROR("*** FATAL: pDelayIntervalTimerLock is NULL! Cannot proceed. ***");
        return;
    }
    
    pDelayIntervalTimerLock->lock();
    GPTP_LOG_DEBUG("*** NETWORK THREAD: Acquired pDelayIntervalTimerLock (thread_id=%lu, stack_ptr=%p) ***", (unsigned long)GetCurrentThreadId(), (void*)&waitTime);
    
    // Defensive check for clock pointer before timer operations
    if (!clock) {
        GPTP_LOG_ERROR("*** FATAL: clock pointer is NULL in startPDelayIntervalTimer! ***");
        pDelayIntervalTimerLock->unlock();
        return;
    }
    
    try {
        GPTP_LOG_DEBUG("*** NETWORK THREAD: About to call clock->deleteEventTimerLocked ***");
        clock->deleteEventTimerLocked(this, PDELAY_INTERVAL_TIMEOUT_EXPIRES);
        GPTP_LOG_DEBUG("*** NETWORK THREAD: About to call clock->addEventTimerLocked ***");
        clock->addEventTimerLocked(this, PDELAY_INTERVAL_TIMEOUT_EXPIRES, waitTime);
        GPTP_LOG_DEBUG("*** NETWORK THREAD: Clock timer operations completed successfully ***");
    } catch (const std::exception& ex) {
        GPTP_LOG_ERROR("*** FATAL: Exception in clock timer operations: %s ***", ex.what());
        pDelayIntervalTimerLock->unlock();
        return;
    } catch (...) {
        GPTP_LOG_ERROR("*** FATAL: Unknown exception in clock timer operations ***");
        pDelayIntervalTimerLock->unlock();
        return;
    }
    
    GPTP_LOG_DEBUG("*** NETWORK THREAD: About to release pDelayIntervalTimerLock (thread_id=%lu, stack_ptr=%p) ***", (unsigned long)GetCurrentThreadId(), (void*)&waitTime);
    pDelayIntervalTimerLock->unlock();
    GPTP_LOG_DEBUG("*** NETWORK THREAD: Released pDelayIntervalTimerLock (thread_id=%lu, stack_ptr=%p) ***", (unsigned long)GetCurrentThreadId(), (void*)&waitTime);
	GPTP_LOG_DEBUG("PDelay interval timer set successfully ***");
}

void EtherPort::stopPDelayIntervalTimer()
{
	pDelayIntervalTimerLock->lock();
	clock->deleteEventTimerLocked(this, PDELAY_INTERVAL_TIMEOUT_EXPIRES);
	pDelayIntervalTimerLock->unlock();
	GPTP_LOG_STATUS("PDelay message transmission stopped per signaling request");
}

void EtherPort::syncDone() {
	GPTP_LOG_VERBOSE("Sync complete");

	// Profile-specific sync state handling for test status messages
	if( getProfile().automotive_test_status && getPortState() == PTP_SLAVE )
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

	// Profile-specific sync rate interval timer for test status messages
	if( getProfile().automotive_test_status )
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
