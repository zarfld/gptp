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
#include <avbts_oslock.hpp>
#include <avbts_ostimerq.hpp>

#include <stdio.h>

#include <string.h>

#include <stdlib.h>

#include <string.h>

#include <math.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#endif

static uint64_t get_current_thread_id() {
#ifdef _WIN32
    return (uint64_t)GetCurrentThreadId();
#else
    return (uint64_t)syscall(SYS_gettid);
#endif
}

std::string ClockIdentity::getIdentityString()
{
	uint8_t cid[PTP_CLOCK_IDENTITY_LENGTH];
	getIdentityString(cid);
	char scid[PTP_CLOCK_IDENTITY_LENGTH * 3 + 1];
	char* pscid = scid;
	for (unsigned i = 0; i < PTP_CLOCK_IDENTITY_LENGTH; ++i) {
		unsigned byte = cid[i];
		PLAT_snprintf(pscid, 4, "%2.2X:", byte);
		pscid += 3;
	}
	scid[PTP_CLOCK_IDENTITY_LENGTH * 3 - 1] = '\0';

	return std::string(scid);
}

void ClockIdentity::set(LinkLayerAddress * addr)
{
	uint64_t tmp1 = 0;
	uint32_t tmp2;
	addr->toOctetArray((uint8_t *) & tmp1);
	tmp2 = tmp1 & 0xFFFFFF;
	tmp1 >>= 24;
	tmp1 <<= 16;
	tmp1 |= 0xFEFF;
	tmp1 <<= 24;
	tmp1 |= tmp2;
	memcpy(id, &tmp1, PTP_CLOCK_IDENTITY_LENGTH);
}

IEEE1588Clock::IEEE1588Clock
( bool forceOrdinarySlave, bool syntonize, uint8_t priority1,
  OSTimerQueueFactory *timerq_factory, OS_IPC *ipc,
  OSLockFactory *lock_factory )
{
	this->priority1 = priority1;
	priority2 = 248;

	number_ports = 0;

	this->forceOrdinarySlave = forceOrdinarySlave;

    // Initialize clock quality with standard defaults
    // Will be updated by setProfileClockQuality() when port is created
	clock_quality.clockAccuracy = 0x22;
	clock_quality.cq_class = 248;
	clock_quality.offsetScaledLogVariance = 0x436A;

	time_source = 160;

	domain_number = 0;

	_syntonize = syntonize;
	_new_syntonization_set_point = false;
	_ppm = 0;

	_phase_error_violation = 0;

	_master_local_freq_offset_init = false;
	_local_system_freq_offset_init = false;

	this->ipc = ipc;

 	memset( &LastEBestIdentity, 0xFF, sizeof( LastEBestIdentity ));

	timerq_lock = lock_factory->createLock( oslock_recursive );

	// This should be done LAST!! to pass fully initialized clock object
	timerq = timerq_factory->createOSTimerQueue( this );

    fup_info = new FollowUpTLV();
    fup_status = new FollowUpTLV();

	return;
}

bool IEEE1588Clock::serializeState( void *buf, off_t *count ) {
  bool ret = true;

  if( buf == NULL ) {
    *count = sizeof( _master_local_freq_offset ) + sizeof( _local_system_freq_offset ) + sizeof( LastEBestIdentity );
    return true;
  }

  // Master-Local Frequency Offset
  if( ret && *count >= (off_t) sizeof( _master_local_freq_offset )) {
	  memcpy
		  ( buf, &_master_local_freq_offset,
			sizeof( _master_local_freq_offset ));
	  *count -= sizeof( _master_local_freq_offset );
	  buf = ((char *)buf) + sizeof( _master_local_freq_offset );
  } else if( ret == false ) {
	  *count += sizeof( _master_local_freq_offset );
  } else {
	  *count = sizeof( _master_local_freq_offset )-*count;
	  ret = false;
  }

  // Local-System Frequency Offset
  if( ret && *count >= (off_t) sizeof( _local_system_freq_offset )) {
	  memcpy
		  ( buf, &_local_system_freq_offset,(off_t)
			sizeof( _local_system_freq_offset ));
	  *count -= sizeof( _local_system_freq_offset );
	  buf = ((char *)buf) + sizeof( _local_system_freq_offset );
  } else if( ret == false ) {
	  *count += sizeof( _local_system_freq_offset );
  } else {
	  *count = sizeof( _local_system_freq_offset )-*count;
	  ret = false;
  }

  // LastEBestIdentity
  if( ret && *count >= (off_t) sizeof( LastEBestIdentity )) {
    memcpy( buf, &LastEBestIdentity, (off_t) sizeof( LastEBestIdentity ));
    *count -= sizeof( LastEBestIdentity );
    buf = ((char *)buf) + sizeof( LastEBestIdentity );
  } else if( ret == false ) {
    *count += sizeof( LastEBestIdentity );
  } else {
    *count = sizeof( LastEBestIdentity )-*count;
    ret = false;
  }

  return ret;
}

bool IEEE1588Clock::restoreSerializedState( void *buf, off_t *count ) {
	bool ret = true;

	/* Master-Local Frequency Offset */
	if( ret && *count >= (off_t) sizeof( _master_local_freq_offset )) {
		memcpy
			( &_master_local_freq_offset, buf,
			  sizeof( _master_local_freq_offset ));
		*count -= sizeof( _master_local_freq_offset );
		buf = ((char *)buf) + sizeof( _master_local_freq_offset );
	} else if( ret == false ) {
		*count += sizeof( _master_local_freq_offset );
	} else {
		*count = sizeof( _master_local_freq_offset )-*count;
		ret = false;
	}

	/* Local-System Frequency Offset */
	if( ret && *count >= (off_t) sizeof( _local_system_freq_offset )) {
		memcpy
			( &_local_system_freq_offset, buf,
			  sizeof( _local_system_freq_offset ));
		*count -= sizeof( _local_system_freq_offset );
		buf = ((char *)buf) + sizeof( _local_system_freq_offset );
	} else if( ret == false ) {
		*count += sizeof( _local_system_freq_offset );
	} else {
		*count = sizeof( _local_system_freq_offset )-*count;
		ret = false;
	}

	/* LastEBestIdentity */
  if( ret && *count >= (off_t) sizeof( LastEBestIdentity )) {
	  memcpy( &LastEBestIdentity, buf, sizeof( LastEBestIdentity ));
	  *count -= sizeof( LastEBestIdentity );
	  buf = ((char *)buf) + sizeof( LastEBestIdentity );
  } else if( ret == false ) {
	  *count += sizeof( LastEBestIdentity );
  } else {
	  *count = sizeof( LastEBestIdentity )-*count;
	  ret = false;
  }

  return ret;
}

Timestamp IEEE1588Clock::getSystemTime(void)
{
	return (Timestamp(0, 0, 0));
}

void timerq_handler(void *arg)
{

	event_descriptor_t *event_descriptor = (event_descriptor_t *) arg;
	event_descriptor->port->processEvent(event_descriptor->event);
}

void IEEE1588Clock::addEventTimer
( CommonPort *target, Event e, unsigned long long time_ns )
{
	event_descriptor_t *event_descriptor = new event_descriptor_t();
	event_descriptor->event = e;
	event_descriptor->port = target;
	timerq->addEvent
		((unsigned)(time_ns / 1000), (int)e, timerq_handler, event_descriptor,
		 true, NULL);
}

void IEEE1588Clock::addEventTimerLocked
( CommonPort *target, Event e, unsigned long long time_ns )
{
    GPTP_LOG_DEBUG("*** addEventTimerLocked: target=%p, event=%d, time_ns=%llu, thread_id=%llu ***", 
        target, (int)e, time_ns, get_current_thread_id());
    
    // Check for null pointers first
    if (!target) {
        GPTP_LOG_ERROR("*** FATAL: addEventTimerLocked called with NULL target pointer! ***");
        return;
    }
    
    if (!timerq) {
        GPTP_LOG_ERROR("*** FATAL: addEventTimerLocked called with NULL timerq! ***");
        return;
    }
    
    GPTP_LOG_DEBUG("*** addEventTimerLocked: About to acquire timer queue lock ***");
    OSLockResult lock_result = getTimerQLock();
    if( lock_result == oslock_fail ) {
        GPTP_LOG_ERROR("*** ERROR: Failed to acquire timer queue lock in addEventTimerLocked (result=%d) ***", (int)lock_result);
        return;
    }
    GPTP_LOG_DEBUG("*** addEventTimerLocked: Timer queue lock acquired successfully (result=%d) ***", (int)lock_result);
    
    try {
        GPTP_LOG_DEBUG("*** addEventTimerLocked: About to call addEventTimer ***");
        
        // Extra defensive check - verify timerq pointer is still valid
        if (!timerq) {
            GPTP_LOG_ERROR("*** FATAL: timerq became NULL after acquiring lock in addEventTimerLocked! ***");
            OSLockResult unlock_result = putTimerQLock();
            if (unlock_result == oslock_fail) {
                GPTP_LOG_ERROR("*** CRITICAL: Failed to release timer queue lock after NULL timerq in addEventTimerLocked! ***");
            }
            return;
        }
        
        GPTP_LOG_DEBUG("*** addEventTimerLocked: timerq=%p, calling addEventTimer ***", timerq);
        addEventTimer( target, e, time_ns );
        GPTP_LOG_DEBUG("*** addEventTimerLocked: addEventTimer completed successfully ***");
    } catch (const std::exception& ex) {
        GPTP_LOG_ERROR("*** FATAL: Exception in addEventTimer: %s ***", ex.what());
        // Always release the lock on exception
        OSLockResult unlock_result = putTimerQLock();
        if (unlock_result == oslock_fail) {
            GPTP_LOG_ERROR("*** CRITICAL: Failed to release timer queue lock after exception! ***");
        }
        return;
    } catch (...) {
        GPTP_LOG_ERROR("*** FATAL: Unknown exception in addEventTimer ***");
        // Always release the lock on exception
        OSLockResult unlock_result = putTimerQLock();
        if (unlock_result == oslock_fail) {
            GPTP_LOG_ERROR("*** CRITICAL: Failed to release timer queue lock after unknown exception! ***");
        }
        return;
    }
    
    GPTP_LOG_DEBUG("*** addEventTimerLocked: About to release timer queue lock ***");
    OSLockResult unlock_result = putTimerQLock();
    if( unlock_result == oslock_fail ) {
        GPTP_LOG_ERROR("*** CRITICAL: Failed to release timer queue lock in addEventTimerLocked (result=%d) ***", (int)unlock_result);
        return;
    }
    GPTP_LOG_DEBUG("*** addEventTimerLocked: Timer queue lock released successfully (result=%d) ***", (int)unlock_result);
}



void IEEE1588Clock::deleteEventTimer
( CommonPort *target, Event event )
{
	timerq->cancelEvent((int)event, NULL);
}

void IEEE1588Clock::deleteEventTimerLocked
( CommonPort *target, Event event )
{
    GPTP_LOG_DEBUG("*** deleteEventTimerLocked: target=%p, event=%d, thread_id=%llu ***", 
        target, (int)event, get_current_thread_id());
    
    // Check for null pointers first
    if (!target) {
        GPTP_LOG_ERROR("*** ERROR: deleteEventTimerLocked called with NULL target pointer! ***");
        return;
    }
    
    if (!timerq) {
        GPTP_LOG_ERROR("*** FATAL: deleteEventTimerLocked called with NULL timerq! ***");
        return;
    }
    
    GPTP_LOG_DEBUG("*** deleteEventTimerLocked: About to acquire timer queue lock ***");
    OSLockResult lock_result = getTimerQLock();
    if( lock_result == oslock_fail ) {
        GPTP_LOG_ERROR("*** ERROR: Failed to acquire timer queue lock in deleteEventTimerLocked (result=%d) ***", (int)lock_result);
        return;
    }
    GPTP_LOG_DEBUG("*** deleteEventTimerLocked: Timer queue lock acquired successfully (result=%d) ***", (int)lock_result);

    try {
        GPTP_LOG_DEBUG("*** deleteEventTimerLocked: About to call timerq->cancelEvent ***");
        
        // Extra defensive check - verify timerq pointer is still valid
        if (!timerq) {
            GPTP_LOG_ERROR("*** FATAL: timerq became NULL after acquiring lock! ***");
            OSLockResult unlock_result = putTimerQLock();
            if (unlock_result == oslock_fail) {
                GPTP_LOG_ERROR("*** CRITICAL: Failed to release timer queue lock after NULL timerq! ***");
            }
            return;
        }
        
        GPTP_LOG_DEBUG("*** deleteEventTimerLocked: timerq=%p, event=%d, calling cancelEvent ***", timerq, (int)event);
        timerq->cancelEvent((int)event, NULL);
        GPTP_LOG_DEBUG("*** deleteEventTimerLocked: timerq->cancelEvent completed successfully ***");
    } catch (const std::exception& ex) {
        GPTP_LOG_ERROR("*** FATAL: Exception in timerq->cancelEvent: %s ***", ex.what());
        // Always release the lock on exception
        OSLockResult unlock_result = putTimerQLock();
        if (unlock_result == oslock_fail) {
            GPTP_LOG_ERROR("*** CRITICAL: Failed to release timer queue lock after exception! ***");
        }
        return;
    } catch (...) {
        GPTP_LOG_ERROR("*** FATAL: Unknown exception in timerq->cancelEvent ***");
        // Always release the lock on exception
        OSLockResult unlock_result = putTimerQLock();
        if (unlock_result == oslock_fail) {
            GPTP_LOG_ERROR("*** CRITICAL: Failed to release timer queue lock after unknown exception! ***");
        }
        return;
    }

    GPTP_LOG_DEBUG("*** deleteEventTimerLocked: About to release timer queue lock ***");
    OSLockResult unlock_result = putTimerQLock();
    if( unlock_result == oslock_fail ) {
        GPTP_LOG_ERROR("*** CRITICAL: Failed to release timer queue lock in deleteEventTimerLocked (result=%d) ***", (int)unlock_result);
        return;
    }
    GPTP_LOG_DEBUG("*** deleteEventTimerLocked: Timer queue lock released successfully (result=%d) ***", (int)unlock_result);
}



FrequencyRatio IEEE1588Clock::calcLocalSystemClockRateDifference( Timestamp local_time, Timestamp system_time ) {
	unsigned long long inter_system_time;
	unsigned long long inter_local_time;
	FrequencyRatio ppt_offset;

	GPTP_LOG_DEBUG( "Calculated local to system clock rate difference" );

	if( !_local_system_freq_offset_init ) {
		_prev_system_time = system_time;
		_prev_local_time = local_time;

		_local_system_freq_offset_init = true;

		return 1.0;
	}

	inter_system_time =
		TIMESTAMP_TO_NS(system_time) - TIMESTAMP_TO_NS(_prev_system_time);
	inter_local_time  =
		TIMESTAMP_TO_NS(local_time) -  TIMESTAMP_TO_NS(_prev_local_time);

	if( inter_system_time != 0 ) {
		ppt_offset = ((FrequencyRatio)inter_local_time)/inter_system_time;
	} else {
		ppt_offset = 1.0;
	}

	_prev_system_time = system_time;
	_prev_local_time = local_time;

  return ppt_offset;
}



FrequencyRatio IEEE1588Clock::calcMasterLocalClockRateDifference( Timestamp master_time, Timestamp sync_time ) {
	unsigned long long inter_sync_time;
	unsigned long long inter_master_time;
	FrequencyRatio ppt_offset;

	GPTP_LOG_DEBUG( "Calculated master to local clock rate difference" );

	if( !_master_local_freq_offset_init ) {
		_prev_sync_time = sync_time;
		_prev_master_time = master_time;

		_master_local_freq_offset_init = true;

		return 1.0;
	}

	inter_sync_time =
		TIMESTAMP_TO_NS(sync_time) - TIMESTAMP_TO_NS(_prev_sync_time);

	uint64_t master_time_ns = TIMESTAMP_TO_NS(master_time);
	uint64_t prev_master_time_ns = TIMESTAMP_TO_NS(_prev_master_time);

	inter_master_time = master_time_ns - prev_master_time_ns;

	if( inter_sync_time != 0 ) {
		ppt_offset = ((FrequencyRatio)inter_master_time)/inter_sync_time;
	} else {
		ppt_offset = 1.0;
	}

	if( master_time_ns < prev_master_time_ns ) {
		GPTP_LOG_ERROR("Negative time jump detected - inter_master_time: %lld, inter_sync_time: %lld, incorrect ppt_offset: %Lf",
					   inter_master_time, inter_sync_time, ppt_offset);
		_master_local_freq_offset_init = false;

		return NEGATIVE_TIME_JUMP;
	}

	_prev_sync_time = sync_time;
	_prev_master_time = master_time;

	return ppt_offset;
}

void IEEE1588Clock::setMasterOffset
( CommonPort *port, int64_t master_local_offset,
  Timestamp local_time, FrequencyRatio master_local_freq_offset,
  int64_t local_system_offset, Timestamp system_time,
  FrequencyRatio local_system_freq_offset, unsigned sync_count,
  unsigned pdelay_count, PortState port_state, bool asCapable )
{
	_master_local_freq_offset = master_local_freq_offset;
	_local_system_freq_offset = local_system_freq_offset;

	if (port->getTestMode()) {
		GPTP_LOG_STATUS("Clock offset:%lld   Clock rate ratio:%Lf   Sync Count:%u   PDelay Count:%u", 
						master_local_offset, master_local_freq_offset, sync_count, pdelay_count);
	}

	if( ipc != NULL ) {
		uint8_t grandmaster_id[PTP_CLOCK_IDENTITY_LENGTH];
		uint8_t clock_id[PTP_CLOCK_IDENTITY_LENGTH];
		PortIdentity port_identity;
		uint16_t port_number;

		grandmaster_clock_identity.getIdentityString(grandmaster_id);
		clock_identity.getIdentityString(clock_id);
		port->getPortIdentity(port_identity);
		port_identity.getPortNumber(&port_number);

		ipc->update(
			master_local_offset, local_system_offset, master_local_freq_offset,
			local_system_freq_offset, TIMESTAMP_TO_NS(local_time),
			sync_count, pdelay_count, port_state, asCapable);

		ipc->update_grandmaster(
			grandmaster_id, domain_number);

		ipc->update_network_interface(
			clock_id, priority1,
			clock_quality.cq_class,	clock_quality.offsetScaledLogVariance,
			clock_quality.clockAccuracy,
			priority2, domain_number,
			port->getSyncInterval(),
			port->getAnnounceInterval(),
			0, // TODO:  Was port->getPDelayInterval() before refactoring.  What do we do now?
			port_number);
	}

	if( master_local_offset == 0 && master_local_freq_offset == 1.0 ) {
		return;
	}

	if( _syntonize ) {
		if( _new_syntonization_set_point || _phase_error_violation > PHASE_ERROR_MAX_COUNT ) {
			_new_syntonization_set_point = false;
			_phase_error_violation = 0;
			/* Make sure that there are no transmit operations
			   in progress */
			getTxLockAll();
			if (port->getTestMode()) {
				GPTP_LOG_STATUS("Adjust clock phase offset:%lld", -master_local_offset);
			}
			port->adjustClockPhase( -master_local_offset );
			_master_local_freq_offset_init = false;
			restartPDelayAll();
			putTxLockAll();
			master_local_offset = 0;
		}

		// Adjust for frequency offset
		long double phase_error = (long double) -master_local_offset;
		if( fabsl(phase_error) > PHASE_ERROR_THRESHOLD ) {
			++_phase_error_violation;
		} else {
			_phase_error_violation = 0;

			float syncPerSec = (float)(1.0 / pow((float)2, port->getSyncInterval()));
			_ppm += (float) ((INTEGRAL * syncPerSec * phase_error) + PROPORTIONAL*((master_local_freq_offset-1.0)*1000000));

			GPTP_LOG_DEBUG("phase_error = %Lf, ppm = %f", phase_error, _ppm );
		}

		if( _ppm < LOWER_FREQ_LIMIT ) _ppm = LOWER_FREQ_LIMIT;
		if( _ppm > UPPER_FREQ_LIMIT ) _ppm = UPPER_FREQ_LIMIT;
		if ( port->getTestMode() ) {
			GPTP_LOG_STATUS("Adjust clock rate ppm:%f", _ppm);
		}
		if( !port->adjustClockRate( _ppm ) ) {
			GPTP_LOG_ERROR( "Failed to adjust clock rate" );
		}
	}

	return;
}

/* Get current time from system clock */
Timestamp IEEE1588Clock::getTime(void)
{
	return getSystemTime();
}

/* Get timestamp from hardware */
Timestamp IEEE1588Clock::getPreciseTime(void)
{
	return getSystemTime();
}

bool IEEE1588Clock::isBetterThan(PTPMessageAnnounce * msg)
{
	unsigned char this1[14];
	unsigned char that1[14];
	uint16_t tmp;

	if (msg == NULL)
		return true;

	this1[0] = priority1;
	that1[0] = msg->getGrandmasterPriority1();

	this1[1] = clock_quality.cq_class;
	that1[1] = msg->getGrandmasterClockQuality()->cq_class;

	this1[2] = clock_quality.clockAccuracy;
	that1[2] = msg->getGrandmasterClockQuality()->clockAccuracy;

	tmp = clock_quality.offsetScaledLogVariance;
	tmp = PLAT_htons(tmp);
	memcpy(this1 + 3, &tmp, sizeof(tmp));
	tmp = msg->getGrandmasterClockQuality()->offsetScaledLogVariance;
	tmp = PLAT_htons(tmp);
	memcpy(that1 + 3, &tmp, sizeof(tmp));

	this1[5] = priority2;
	that1[5] = msg->getGrandmasterPriority2();

	clock_identity.getIdentityString(this1 + 6);
	msg->getGrandmasterIdentity((char *)that1 + 6);

#if 0
	GPTP_LOG_DEBUG("(Clk)Us: ");
	for (int i = 0; i < 14; ++i)
		GPTP_LOG_DEBUG("%hhx ", this1[i]);
	GPTP_LOG_DEBUG("(Clk)Them: ");
	for (int i = 0; i < 14; ++i)
		GPTP_LOG_DEBUG("%hhx ", that1[i]);
#endif

	return (memcmp(this1, that1, 14) < 0) ? true : false;
}

/**
 * @brief Set clock quality based on profile configuration
 * @param milan_profile Enable Milan Baseline profile clock quality
 * @param automotive_profile Enable AVnu Automotive profile clock quality
 */
void IEEE1588Clock::setProfileClockQuality(bool milan_profile, bool automotive_profile)
{
	if (milan_profile) {
		// Milan Baseline Profile requirements
		clock_quality.clockAccuracy = 0x20;         // Enhanced accuracy (~1µs)
		clock_quality.cq_class = 248;               // End application time
		clock_quality.offsetScaledLogVariance = 0x4000; // Enhanced stability
		
		GPTP_LOG_INFO("Milan Profile: Enhanced clock quality applied");
		GPTP_LOG_INFO("Clock accuracy: 0x%02X, Variance: 0x%04X", 
		              clock_quality.clockAccuracy, clock_quality.offsetScaledLogVariance);
		
	} else if (automotive_profile) {
		// AVnu Automotive Profile requirements  
		clock_quality.clockAccuracy = 0x22;         // Standard accuracy
		clock_quality.cq_class = 248;               // End application time
		clock_quality.offsetScaledLogVariance = 0x436A; // Good stability
		
		GPTP_LOG_INFO("Automotive Profile: Standard clock quality applied");
		
	} else {
		// Standard IEEE 1588 defaults (already set in constructor)
		GPTP_LOG_INFO("Standard Profile: Default clock quality");
	}
	
	// Validate clock quality parameters
	if (!validateClockQuality()) {
		GPTP_LOG_WARNING("Clock quality validation failed - using safe defaults");
		clock_quality.clockAccuracy = 0x22;
		clock_quality.cq_class = 248;
		clock_quality.offsetScaledLogVariance = 0x436A;
	}
}

/**
 * @brief Validate clock quality parameters against IEEE 1588 specification
 * @return true if parameters are valid, false otherwise
 */
bool IEEE1588Clock::validateClockQuality()
{
	// Basic IEEE 1588 validation
	if (clock_quality.cq_class > 255) {
		GPTP_LOG_ERROR("Invalid clock class: %u", clock_quality.cq_class);
		return false;
	}
	
	// Clock accuracy enumeration validation (typical range 0x17-0x31)
	if (clock_quality.clockAccuracy < 0x17 || clock_quality.clockAccuracy > 0x31) {
		GPTP_LOG_WARNING("Clock accuracy 0x%02X outside typical range (0x17-0x31)", 
		                 clock_quality.clockAccuracy);
	}
	
	// Offset scaled log variance is uint16_t, so automatically valid range
	return true;
}

IEEE1588Clock::~IEEE1588Clock(void)
{
	// Do nothing
}
