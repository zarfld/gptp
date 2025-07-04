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


#include <common_port.hpp>
#include <avbts_clock.hpp>
#include <common_tstamper.hpp>
#include <gptp_cfg.hpp>
#include <cmath>

CommonPort::CommonPort( PortInit_t *portInit ) :
	thread_factory( portInit->thread_factory ),
	timer_factory( portInit->timer_factory ),
	lock_factory( portInit->lock_factory ),
	condition_factory( portInit->condition_factory ),
	_hw_timestamper( portInit->timestamper ),
	clock( portInit->clock ),
	isGM( portInit->isGM ),
	phy_delay( portInit->phy_delay )
{
	one_way_delay = ONE_WAY_DELAY_DEFAULT;
	neighbor_prop_delay_thresh = portInit->neighborPropDelayThreshold;
	net_label = portInit->net_label;
	link_thread = thread_factory->createThread();
	listening_thread = thread_factory->createThread();
	sync_receipt_thresh = portInit->syncReceiptThreshold;
	wrongSeqIDCounter = 0;
	_peer_rate_offset = 1.0;
	_peer_offset_init = false;
	ifindex = portInit->index;
	testMode = false;
	port_state = PTP_INITIALIZING;
	clock->registerPort(this, ifindex);
	qualified_announce = NULL;
	
	// Initialize unified profile system
	active_profile = portInit->profile;
	
	// Configure neighbor delay threshold from profile
	if (active_profile.neighbor_prop_delay_thresh != 0) {
		neighbor_prop_delay_thresh = active_profile.neighbor_prop_delay_thresh;
	}
	
	// Configure sync receipt threshold from profile
	if (active_profile.sync_receipt_thresh != 0) {
		sync_receipt_thresh = active_profile.sync_receipt_thresh;
	}
	
	// Configure asCapable initial state from profile
	asCapable = active_profile.initial_as_capable;
	
	// Configure timing intervals from profile
	log_min_mean_pdelay_req_interval = active_profile.pdelay_interval_log;
	log_mean_sync_interval = active_profile.sync_interval_log;
	log_mean_announce_interval = active_profile.announce_interval_log;
	
	GPTP_LOG_INFO("Port initialized with %s profile: %s", 
		active_profile.profile_name.c_str(),
		active_profile.profile_description.c_str());
	
	announce_sequence_id = 0;
	signal_sequence_id = 0;
	sync_sequence_id = 0;
	initialLogPdelayReqInterval = portInit->initialLogPdelayReqInterval;
	initialLogSyncInterval = portInit->initialLogSyncInterval;
	log_mean_announce_interval = 0;
	pdelay_count = 0;
	asCapable = active_profile.initial_as_capable;  // Initialize from profile
	link_speed = INVALID_LINKSPEED;
	allow_negative_correction_field = active_profile.allows_negative_correction_field;
	
	// Initialize profile statistics
	memset(&active_profile.stats, 0, sizeof(active_profile.stats));
	if (active_profile.max_convergence_time_ms > 0) {
		Timestamp start_time = clock->getTime();
		active_profile.stats.convergence_start_time = TIMESTAMP_TO_NS(start_time);
	}
	
	// Profile-specific initialization for late response tracking
	_consecutive_late_responses = 0;
	_consecutive_missing_responses = 0;
	_last_pdelay_req_timestamp = Timestamp(0, 0, 0);
	_pdelay_response_received = false;
	memset(&counters, 0, sizeof(counters));
}

CommonPort::~CommonPort()
{
	delete qualified_announce;
}

void CommonPort::updateProfileJitterStats(uint64_t sync_timestamp)
{
	if (active_profile.max_sync_jitter_ns == 0) return;  // No jitter monitoring configured
	
	if (active_profile.stats.last_sync_time != 0) {
		uint64_t interval = sync_timestamp - active_profile.stats.last_sync_time;
		uint64_t expected_interval = (uint64_t)(pow(2.0, (double)active_profile.sync_interval_log) * 1000000000.0);
		uint32_t jitter = (interval > expected_interval) ? 
			(uint32_t)(interval - expected_interval) : 
			(uint32_t)(expected_interval - interval);
		
		active_profile.stats.current_sync_jitter_ns = jitter;
		active_profile.stats.total_sync_messages++;
		
		// Check compliance with profile jitter requirements
		if (jitter > active_profile.max_sync_jitter_ns) {
			GPTP_LOG_ERROR("PROFILE COMPLIANCE (%s): Sync jitter %u ns exceeds limit %u ns", 
				active_profile.profile_name.c_str(), jitter, active_profile.max_sync_jitter_ns);
		}
	}
	active_profile.stats.last_sync_time = sync_timestamp;
}

bool CommonPort::checkProfileConvergence()
{
	if (active_profile.max_convergence_time_ms == 0) return true;  // No convergence requirement
	
	Timestamp current_timestamp = clock->getTime();
	uint64_t current_time = TIMESTAMP_TO_NS(current_timestamp);
	uint64_t convergence_time = current_time - active_profile.stats.convergence_start_time;
	
	// Check if we've exceeded the convergence time limit
	if (convergence_time > (active_profile.max_convergence_time_ms * 1000000ULL)) {
		if (!active_profile.stats.convergence_achieved) {
			GPTP_LOG_ERROR("PROFILE COMPLIANCE (%s): Convergence time %llu ms exceeds target %u ms", 
				active_profile.profile_name.c_str(),
				convergence_time / 1000000ULL, active_profile.max_convergence_time_ms);
		}
		return false;
	}
	
	// Mark convergence achieved if we have sync within the time limit
	if (active_profile.stats.last_sync_time != 0 && !active_profile.stats.convergence_achieved) {
		active_profile.stats.convergence_achieved = true;
		GPTP_LOG_STATUS("PROFILE COMPLIANCE (%s): Convergence achieved in %llu ms (target: %u ms)", 
			active_profile.profile_name.c_str(),
			convergence_time / 1000000ULL, active_profile.max_convergence_time_ms);
	}
	
	return active_profile.stats.convergence_achieved;
}

bool CommonPort::init_port( void )
{
	log_mean_sync_interval = initialLogSyncInterval;

	if (!OSNetworkInterfaceFactory::buildInterface
	    ( &net_iface, factory_name_t("default"), net_label,
	      _hw_timestamper)) {
		GPTP_LOG_ERROR("init_port: OSNetworkInterfaceFactory::buildInterface failed");
		return false;
	}

	this->net_iface->getLinkLayerAddress(&local_addr);
	clock->setClockIdentity(&local_addr);

	this->timestamper_init();

	port_identity.setClockIdentity(clock->getClockIdentity());
	port_identity.setPortNumber(&ifindex);

	syncReceiptTimerLock = lock_factory->createLock(oslock_recursive);
	syncIntervalTimerLock = lock_factory->createLock(oslock_recursive);
	announceIntervalTimerLock = lock_factory->createLock(oslock_recursive);

	return _init_port();
}

void CommonPort::timestamper_init( void )
{
	if( _hw_timestamper != NULL )
	{
		if( !_hw_timestamper->HWTimestamper_init
		    ( net_label, net_iface ))
		{
			GPTP_LOG_ERROR
				( "Failed to initialize hardware timestamper, "
				  "falling back to software timestamping" );
			return;
		}
	}
}

void CommonPort::timestamper_reset( void )
{
	if( _hw_timestamper != NULL )
	{
		_hw_timestamper->HWTimestamper_reset();
	}
}

PTPMessageAnnounce *CommonPort::calculateERBest( void )
{
	return qualified_announce;
}

void CommonPort::recommendState
( PortState state, bool changed_external_master )
{
	bool reset_sync = false;
	switch (state) {
	case PTP_MASTER:
		if ( getPortState() != PTP_MASTER )
		{
			setPortState( PTP_MASTER );
			// Start announce receipt timeout timer
			// Start sync receipt timeout timer
			becomeMaster( true );
			reset_sync = true;
		}
		break;
	case PTP_SLAVE:
		if ( getPortState() != PTP_SLAVE )
		{
			becomeSlave( true );
			reset_sync = true;
		} else {
			if( changed_external_master ) {
				GPTP_LOG_STATUS("Changed master!" );
				clock->newSyntonizationSetPoint();
				clock->updateFUPInfo();
				reset_sync = true;
			}
		}
		break;
	default:
		GPTP_LOG_ERROR
		    ("Invalid state change requested by call to "
		     "1588Port::recommendState()");
		break;
	}
	if( reset_sync ) sync_count = 0;
	return;
}

bool CommonPort::serializeState( void *buf, off_t *count )
{
	bool ret = true;

	if( buf == NULL ) {
		*count = sizeof(port_state)+sizeof(_peer_rate_offset)+
			sizeof(asCapable)+sizeof(one_way_delay);
		return true;
	}

	if( port_state != PTP_MASTER && port_state != PTP_SLAVE ) {
		*count = 0;
		ret = false;
		goto bail;
	}

	/* asCapable */
	if( ret && *count >= (off_t) sizeof( asCapable )) {
		memcpy( buf, &asCapable, sizeof( asCapable ));
		*count -= sizeof( asCapable );
		buf = ((char *)buf) + sizeof( asCapable );
	} else if( ret == false ) {
		*count += sizeof( asCapable );
	} else {
		*count = sizeof( asCapable )-*count;
		ret = false;
	}

	/* Port State */
	if( ret && *count >= (off_t) sizeof( port_state )) {
		memcpy( buf, &port_state, sizeof( port_state ));
		*count -= sizeof( port_state );
		buf = ((char *)buf) + sizeof( port_state );
	} else if( ret == false ) {
		*count += sizeof( port_state );
	} else {
		*count = sizeof( port_state )-*count;
		ret = false;
	}

	/* Link Delay */
	if( ret && *count >= (off_t) sizeof( one_way_delay )) {
		memcpy( buf, &one_way_delay, sizeof( one_way_delay ));
		*count -= sizeof( one_way_delay );
		buf = ((char *)buf) + sizeof( one_way_delay );
	} else if( ret == false ) {
		*count += sizeof( one_way_delay );
	} else {
		*count = sizeof( one_way_delay )-*count;
		ret = false;
	}

	/* Neighbor Rate Ratio */
	if( ret && *count >= (off_t) sizeof( _peer_rate_offset )) {
		memcpy( buf, &_peer_rate_offset, sizeof( _peer_rate_offset ));
		*count -= sizeof( _peer_rate_offset );
		buf = ((char *)buf) + sizeof( _peer_rate_offset );
	} else if( ret == false ) {
		*count += sizeof( _peer_rate_offset );
	} else {
		*count = sizeof( _peer_rate_offset )-*count;
		ret = false;
	}

 bail:
	return ret;
}

bool CommonPort::restoreSerializedState
( void *buf, off_t *count )
{
	bool ret = true;

	/* asCapable */
	if( ret && *count >= (off_t) sizeof( asCapable )) {
		memcpy( &asCapable, buf, sizeof( asCapable ));
		*count -= sizeof( asCapable );
		buf = ((char *)buf) + sizeof( asCapable );
	} else if( ret == false ) {
		*count += sizeof( asCapable );
	} else {
		*count = sizeof( asCapable )-*count;
		ret = false;
	}

	/* Port State */
	if( ret && *count >= (off_t) sizeof( port_state )) {
		memcpy( &port_state, buf, sizeof( port_state ));
		*count -= sizeof( port_state );
		buf = ((char *)buf) + sizeof( port_state );
	} else if( ret == false ) {
		*count += sizeof( port_state );
	} else {
		*count = sizeof( port_state )-*count;
		ret = false;
	}

	/* Link Delay */
	if( ret && *count >= (off_t) sizeof( one_way_delay )) {
		memcpy( &one_way_delay, buf, sizeof( one_way_delay ));
		*count -= sizeof( one_way_delay );
		buf = ((char *)buf) + sizeof( one_way_delay );
	} else if( ret == false ) {
		*count += sizeof( one_way_delay );
	} else {
		*count = sizeof( one_way_delay )-*count;
		ret = false;
	}

	/* Neighbor Rate Ratio */
	if( ret && *count >= (off_t) sizeof( _peer_rate_offset )) {
		memcpy( &_peer_rate_offset, buf, sizeof( _peer_rate_offset ));
		*count -= sizeof( _peer_rate_offset );
		buf = ((char *)buf) + sizeof( _peer_rate_offset );
	} else if( ret == false ) {
		*count += sizeof( _peer_rate_offset );
	} else {
		*count = sizeof( _peer_rate_offset )-*count;
		ret = false;
	}

	return ret;
}

void CommonPort::startSyncReceiptTimer
( long long unsigned int waitTime )
{
	clock->getTimerQLock();
	syncReceiptTimerLock->lock();
	clock->deleteEventTimer( this, SYNC_RECEIPT_TIMEOUT_EXPIRES );
	clock->addEventTimer
		( this, SYNC_RECEIPT_TIMEOUT_EXPIRES, waitTime );
	syncReceiptTimerLock->unlock();
	clock->putTimerQLock();
}

void CommonPort::stopSyncReceiptTimer( void )
{
	clock->getTimerQLock();
	syncReceiptTimerLock->lock();
	clock->deleteEventTimer( this, SYNC_RECEIPT_TIMEOUT_EXPIRES );
	syncReceiptTimerLock->unlock();
	clock->putTimerQLock();
}

void CommonPort::startSyncIntervalTimer
( long long unsigned int waitTime )
{
	if( syncIntervalTimerLock->trylock() == oslock_fail ) return;
	clock->deleteEventTimerLocked(this, SYNC_INTERVAL_TIMEOUT_EXPIRES);
	clock->addEventTimerLocked
		(this, SYNC_INTERVAL_TIMEOUT_EXPIRES, waitTime);
	syncIntervalTimerLock->unlock();
}

void CommonPort::startAnnounceIntervalTimer
( long long unsigned int waitTime )
{
	announceIntervalTimerLock->lock();
	clock->deleteEventTimerLocked
		( this, ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES );
	clock->addEventTimerLocked
		( this, ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES, waitTime );
	announceIntervalTimerLock->unlock();
}

void CommonPort::stopAnnounceIntervalTimer()
{
	announceIntervalTimerLock->lock();
	clock->deleteEventTimerLocked( this, ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES );
	announceIntervalTimerLock->unlock();
}

void CommonPort::stopPDelayIntervalTimer()
{
	// Default implementation: do nothing. Derived classes should override.
}

bool CommonPort::processStateChange( Event e )
{
	bool changed_external_master;
	uint8_t LastEBestClockIdentity[PTP_CLOCK_IDENTITY_LENGTH];
	int number_ports, j;
	PTPMessageAnnounce *EBest = NULL;
	char EBestClockIdentity[PTP_CLOCK_IDENTITY_LENGTH];
	CommonPort **ports;

	// Nothing to do if we are slave only
	if ( clock->getPriority1() == 255 )
		return true;

	clock->getPortList(number_ports, ports);

	/* Find EBest for all ports */
	j = 0;
	for (int i = 0; i < number_ports; ++i) {
		while (ports[j] == NULL)
			++j;
		if ( ports[j]->getPortState() == PTP_DISABLED ||
		     ports[j]->getPortState() == PTP_FAULTY )
		{
			continue;
		}
		if( EBest == NULL )
		{
			EBest = ports[j]->calculateERBest();
		}
		else if( ports[j]->calculateERBest() )
		{
			if( ports[j]->
			    calculateERBest()->isBetterThan(EBest))
			{
				EBest = ports[j]->calculateERBest();
			}
		}
	}

	if (EBest == NULL)
	{
		return true;
	}

	/* Check if we've changed */
	clock->getLastEBestIdentity().
		getIdentityString( LastEBestClockIdentity );
	EBest->getGrandmasterIdentity( EBestClockIdentity );
	if( memcmp( EBestClockIdentity, LastEBestClockIdentity,
		    PTP_CLOCK_IDENTITY_LENGTH ) != 0 )
	{
		ClockIdentity newGM;
		changed_external_master = true;
		newGM.set((uint8_t *) EBestClockIdentity );
		clock->setLastEBestIdentity( newGM );
	}
	else
	{
		changed_external_master = false;
	}

	if( clock->isBetterThan( EBest ))
	{
		// We're Grandmaster, set grandmaster info to me
		ClockIdentity clock_identity;
		unsigned char priority1;
		unsigned char priority2;
		ClockQuality clock_quality;

		clock_identity = getClock()->getClockIdentity();
		getClock()->setGrandmasterClockIdentity( clock_identity );
		priority1 = getClock()->getPriority1();
		getClock()->setGrandmasterPriority1( priority1 );
		priority2 = getClock()->getPriority2();
		getClock()->setGrandmasterPriority2( priority2 );
		clock_quality = getClock()->getClockQuality();
		getClock()->setGrandmasterClockQuality( clock_quality );
	}

	j = 0;
	for( int i = 0; i < number_ports; ++i )
	{
		while (ports[j] == NULL)
			++j;
		if ( ports[j]->getPortState() ==
		     PTP_DISABLED ||
		     ports[j]->getPortState() ==
		     PTP_FAULTY)
		{
			continue;
		}
		if (clock->isBetterThan(EBest))
		{
			// We are the GrandMaster, all ports are master
			EBest = NULL;	// EBest == NULL : we were grandmaster
			ports[j]->recommendState( PTP_MASTER,
						  changed_external_master );
		} else {
			if( EBest == ports[j]->calculateERBest() ) {
				// The "best" Announce was received on this
				// port
				ClockIdentity clock_identity;
				unsigned char priority1;
				unsigned char priority2;
				ClockQuality *clock_quality;

				ports[j]->recommendState
					( PTP_SLAVE, changed_external_master );
				clock_identity =
					EBest->getGrandmasterClockIdentity();
				getClock()->setGrandmasterClockIdentity
					( clock_identity );
				priority1 = EBest->getGrandmasterPriority1();
				getClock()->setGrandmasterPriority1
					( priority1 );
				priority2 =
					EBest->getGrandmasterPriority2();
				getClock()->setGrandmasterPriority2
					( priority2 );
				clock_quality =
					EBest->getGrandmasterClockQuality();
				getClock()->setGrandmasterClockQuality
					(*clock_quality);
			} else {
				/* Otherwise we are the master because we have
				   sync'd to a better clock */
				ports[j]->recommendState
					(PTP_MASTER, changed_external_master);
			}
		}
	}

	return true;
}


bool CommonPort::processSyncAnnounceTimeout( Event e )
{
	// We're Grandmaster, set grandmaster info to me
	ClockIdentity clock_identity;
	unsigned char priority1;
	unsigned char priority2;
	ClockQuality clock_quality;

	Timestamp system_time;
	Timestamp device_time;
	uint32_t local_clock, nominal_clock_rate;

	// Nothing to do
	if( clock->getPriority1() == 255 )
		return true;

	// Restart timer
	if( e == ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES ) {
		clock->addEventTimerLocked
			(this, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES,
			 (ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER*
			  (unsigned long long)
			  (pow((double)2,getAnnounceInterval())*
			   1000000000.0)));
	} else {
		startSyncReceiptTimer
			((unsigned long long)
			 (SYNC_RECEIPT_TIMEOUT_MULTIPLIER *
			  ((double) pow((double)2, getSyncInterval()) *
			   1000000000.0)));
	}

	if( getPortState() == PTP_MASTER )
		return true;

	GPTP_LOG_STATUS(
		"*** %s Timeout Expired - Becoming Master",
		e == ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES ? "Announce" :
		"Sync" );

	clock_identity = getClock()->getClockIdentity();
	getClock()->setGrandmasterClockIdentity( clock_identity );
	priority1 = getClock()->getPriority1();
	getClock()->setGrandmasterPriority1( priority1 );
	priority2 = getClock()->getPriority2();
	getClock()->setGrandmasterPriority2( priority2 );
	clock_quality = getClock()->getClockQuality();
	getClock()->setGrandmasterClockQuality( clock_quality );

	setPortState( PTP_MASTER );

	getDeviceTime( system_time, device_time,
		       local_clock, nominal_clock_rate );

	(void) clock->calcLocalSystemClockRateDifference
		( device_time, system_time );

	setQualifiedAnnounce( NULL );

	clock->addEventTimerLocked
		( this, SYNC_INTERVAL_TIMEOUT_EXPIRES,
		  16000000 );

	startAnnounce();

	return true;
}

bool CommonPort::processEvent( Event e )
{
	bool ret;

	switch( e )
	{
	default:
		// Unhandled event
		
		ret = _processEvent( e );
		GPTP_LOG_ERROR("default switch - Unhandled event %d in CommonPort::processEvent()", e);
		break;

	case POWERUP:
	case INITIALIZE:
		GPTP_LOG_DEBUG("Received POWERUP/INITIALIZE event");

		// If port has been configured as master or slave, run media
		// specific configuration. If it hasn't been configured
		// start listening for announce messages
		if( clock->getPriority1() == 255 ||
		    port_state == PTP_SLAVE )
		{
			becomeSlave( true );
		}
		else if( port_state == PTP_MASTER )
		{
			becomeMaster( true );
		}
		else
		{
			clock->addEventTimerLocked(this, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES,
				(uint64_t) ( ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER * pow(2.0, getAnnounceInterval()) * 1000000000.0 ));
		}

		// Do any media specific initialization
		ret = _processEvent( e );
		break;

	case STATE_CHANGE_EVENT:
		ret = _processEvent( e );

		// If this event wasn't handled in a media specific way, call
		// the default action
		if( !ret )
			ret = processStateChange( e );
		break;

	case ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES:
	case SYNC_RECEIPT_TIMEOUT_EXPIRES:
		if (e == ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES) {
			incCounter_ieee8021AsPortStatAnnounceReceiptTimeouts();
		}
		else if (e == SYNC_RECEIPT_TIMEOUT_EXPIRES) {
			incCounter_ieee8021AsPortStatRxSyncReceiptTimeouts();
		}

		ret = _processEvent( e );

		// If this event wasn't handled in a media specific way, call
		// the default action
		if( !ret )
			ret = processSyncAnnounceTimeout( e );
		break;

	case ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES:
		GPTP_LOG_DEBUG("ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES occured");

		// Send an announce message
		if ( asCapable)
		{
			static uint32_t announce_count = 0;
			announce_count++;
			
			GPTP_LOG_STATUS("*** SENDING ANNOUNCE MESSAGE #%u *** (asCapable=true, interval=%d)", 
				announce_count, getAnnounceInterval());
			
			PTPMessageAnnounce *annc =
				new PTPMessageAnnounce(this);
			PortIdentity dest_id;
			PortIdentity gmId;
			ClockIdentity clock_id = clock->getClockIdentity();
			gmId.setClockIdentity(clock_id);
			getPortIdentity( dest_id );
			annc->setPortIdentity( &dest_id );
			
			
			bool send_result = annc->sendPort( this, NULL );
			if (send_result) {
				GPTP_LOG_STATUS("Announce message #%u sent successfully", announce_count);
			} else {
				GPTP_LOG_WARNING("Announce message #%u FAILED to send", announce_count);
			}
			delete annc;
		} else {
			static uint32_t announce_blocked_count = 0;
			announce_blocked_count++;
			GPTP_LOG_WARNING("*** ANNOUNCE MESSAGE BLOCKED #%u *** (asCapable=false - not ready to send)", announce_blocked_count);
		}

		startAnnounceIntervalTimer
			((uint64_t)( pow((double)2, getAnnounceInterval()) *
				     1000000000.0 ));
		ret = true;
		break;

	case SYNC_INTERVAL_TIMEOUT_EXPIRES:
		GPTP_LOG_DEBUG("SYNC_INTERVAL_TIMEOUT_EXPIRES occured");
		// If asCapable is true attempt some media specific action
		ret = true;
		if( asCapable )
			ret = _processEvent( e );

		/* Do getDeviceTime() after transmitting sync frame
		   causing an update to local/system timestamp */
		{
			Timestamp system_time;
			Timestamp device_time;
			uint32_t local_clock, nominal_clock_rate;
			FrequencyRatio local_system_freq_offset;
			int64_t local_system_offset;

			getDeviceTime
				( system_time, device_time,
				  local_clock, nominal_clock_rate );

			GPTP_LOG_VERBOSE
				( "port::processEvent(): System time: %u,%u "
				  "Device Time: %u,%u",
				  system_time.seconds_ls,
				  system_time.nanoseconds,
				  device_time.seconds_ls,
				  device_time.nanoseconds );

			local_system_offset =
				TIMESTAMP_TO_NS(system_time) -
				TIMESTAMP_TO_NS(device_time);
			local_system_freq_offset =
				clock->calcLocalSystemClockRateDifference
				( device_time, system_time );
			clock->setMasterOffset
				( this, 0, device_time, 1.0,
				  local_system_offset, system_time,
				  local_system_freq_offset, getSyncCount(),
				  pdelay_count, port_state, asCapable );
		}

		// Call media specific action for completed sync
		syncDone();

		// Restart the timer
		startSyncIntervalTimer
			((uint64_t)( pow((double)2, getSyncInterval()) *
				     1000000000.0 ));

		break;
	case PDELAY_INTERVAL_TIMEOUT_EXPIRES:
		GPTP_LOG_DEBUG("PDELAY_INTERVAL_TIMEOUT_EXPIRES occured");
		// If asCapable is true attempt some media specific action
		// TODO: implement profile specific handling on that case
		// gPTPprofile maintain_as_capable_on_timeout

		bool maintain_as_capable_on_timeout = active_profile.maintain_as_capable_on_timeout;
		if(!maintain_as_capable_on_timeout && asCapable) {
			GPTP_LOG_WARNING("PDelay interval expired, but asCapable is false - not sending PDelay messages");
			asCapable = false; // Disable asCapable if profile requires it
		}

		ret = true; // No action needed if not asCapable
		break;
	}

	if( ret == false )
	{
		GPTP_LOG_ERROR
			("CommonPort::processEvent: Unhandled event %d",
			 e);
	}
	return ret;
}

void CommonPort::getDeviceTime
( Timestamp &system_time, Timestamp &device_time,
  uint32_t &local_clock, uint32_t &nominal_clock_rate )
{
	if (_hw_timestamper) {
		_hw_timestamper->HWTimestamper_gettime
			( &system_time, &device_time,
			  &local_clock, &nominal_clock_rate );
	} else {
		device_time = system_time = clock->getSystemTime();
		local_clock = nominal_clock_rate = 0;
	}

	return;
}

void CommonPort::startAnnounce()
{
	startAnnounceIntervalTimer(16000000);
}

int CommonPort::getTimestampVersion()
{
	return _hw_timestamper->getVersion();
}

bool CommonPort::_adjustClockRate( FrequencyRatio freq_offset )
{
	if( _hw_timestamper )
	{
		return _hw_timestamper->HWTimestamper_adjclockrate
			((float) freq_offset );
	}

	return false;
}

void CommonPort::getExtendedError( char *msg )
{
	if (_hw_timestamper)
	{
		_hw_timestamper->HWTimestamper_get_extderror(msg);
		return;
	}

	*msg = '\0';
}

bool CommonPort::adjustClockPhase( int64_t phase_adjust )
{
	if( _hw_timestamper )
		return _hw_timestamper->
			HWTimestamper_adjclockphase( phase_adjust );

	return false;
}

FrequencyRatio CommonPort::getLocalSystemFreqOffset()
{
	return clock->getLocalSystemFreqOffset();
}

Timestamp CommonPort::getTxPhyDelay( uint32_t link_speed ) const
{
	if( phy_delay->count( link_speed ) != 0 )
		return Timestamp
			( phy_delay->at(link_speed).get_tx_delay(), 0, 0 );

	return Timestamp(0, 0, 0);
}

Timestamp CommonPort::getRxPhyDelay( uint32_t link_speed ) const
{
	if( phy_delay->count( link_speed ) != 0 )
		return Timestamp
			( phy_delay->at(link_speed).get_rx_delay(), 0, 0 );

	return Timestamp(0, 0, 0);
}

void CommonPort::startPDelayIntervalTimer(uint64_t interval)
{
	// Default implementation - derived classes should override
	// This is a placeholder that does nothing
}

void CommonPort::sendGeneralPort()
{
	// Default implementation - derived classes should override
	// This is a placeholder that does nothing
}

void CommonPort::sendGeneralPort(int etherType, uint8_t* buf, uint16_t len, MulticastType mcast_type, PortIdentity* destIdentity)
{
	// Default implementation - derived classes should override
	// This is a placeholder that does nothing
}

void CommonPort::stopSyncIntervalTimer() {
	// Default implementation: do nothing. Derived classes should override if needed.
}
