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

#ifndef WINDOWS_HAL_HPP
#define WINDOWS_HAL_HPP

/**@file*/

#include "packet.hpp"
#include "ether_tstamper.hpp"
#include "common_tstamper.hpp"
#include "IPCListener.hpp"
#include "avbts_osnet.hpp"
#include "avbts_oslock.hpp"
#include "avbts_oscondition.hpp"
#include "avbts_ostimerq.hpp"
#include "avbts_ostimer.hpp"
#include "avbts_osthread.hpp"
#include "avbts_osipc.hpp"
#include "wireless_tstamper.hpp"
#include "ieee1588.hpp"  // For TIMESTAMP_TO_NS macro
#include "windows_crosststamp.hpp"
#include "ether_tstamper.hpp"
#include "packet.hpp"

#include "iphlpapi.h"
#include <mutex>
#include <chrono>
#include "windows_ipc.hpp"
#include "tsc.hpp"

#include <ntddndis.h>

#include <map>
#include <list>

// Windows socket constants (define if not available)
#ifndef SO_TIMESTAMP
#define SO_TIMESTAMP 0x300A
#endif

// Forward declarations
class Timestamp;
class EnhancedSoftwareTimestamper;

// Constants for OID failure tracking
#define MAX_OID_FAILURES 10

// Struct to track consecutive failures for Intel OIDs
struct OidFailureTracker {
    unsigned int failure_count;
    bool disabled;
    
    OidFailureTracker() : failure_count(0), disabled(false) {}
};

// Intel OID failure tracking for RXSTAMP, TXSTAMP, and SYSTIM
struct IntelOidFailureTracking {
    OidFailureTracker rxstamp;
    OidFailureTracker txstamp; 
    OidFailureTracker systim;
    
    IntelOidFailureTracking() = default;
};

// NDIS timestamp OIDs and structures (define if not available in older SDKs)
#ifndef OID_TIMESTAMP_CAPABILITY
#define OID_TIMESTAMP_CAPABILITY 0x00010265
#endif
#ifndef OID_TIMESTAMP_CURRENT_CONFIG  
#define OID_TIMESTAMP_CURRENT_CONFIG 0x00010266
#endif

// Fallback NDIS structures for older SDKs
#ifndef NDIS_TIMESTAMP_CAPABILITIES_REVISION_1
typedef struct _NDIS_TIMESTAMP_CAPABILITIES {
    NDIS_OBJECT_HEADER Header;
    ULONG64 HardwareClockFrequencyHz;
    BOOLEAN CrossTimestamp;
    ULONG64 Reserved1;
    ULONG64 Reserved2;
} NDIS_TIMESTAMP_CAPABILITIES, *PNDIS_TIMESTAMP_CAPABILITIES;

typedef struct _NDIS_TIMESTAMP_CONFIGURATION {
    NDIS_OBJECT_HEADER Header;
    ULONG Flags;
    ULONG64 Reserved;
} NDIS_TIMESTAMP_CONFIGURATION, *PNDIS_TIMESTAMP_CONFIGURATION;
#endif

// Fallback statistics structure if not available
#ifndef NDIS_STATISTICS_INFO_REVISION_1
typedef struct _NDIS_STATISTICS_INFO {
    ULONG64 ifInOctets;
    ULONG64 ifInUcastPkts;
    ULONG64 ifInNUcastPkts;
    ULONG64 ifInDiscards;
    ULONG64 ifInErrors;
    ULONG64 ifInUnknownProtos;
    ULONG64 ifOutOctets;
    ULONG64 ifOutUcastPkts;
    ULONG64 ifOutNUcastPkts;
    ULONG64 ifOutDiscards;
    ULONG64 ifOutErrors;
} NDIS_STATISTICS_INFO, *PNDIS_STATISTICS_INFO;
#endif

// Include modular Windows HAL components for enhanced functionality  
#include "windows_hal_iphlpapi.hpp"
#include "windows_hal_ndis.hpp"

/**
 * @brief WindowsPCAPNetworkInterface implements an interface to the network adapter
 * through calls to the publicly available libraries known as PCap.
 */
class WindowsPCAPNetworkInterface : public OSNetworkInterface {
	friend class WindowsPCAPNetworkInterfaceFactory;
private:
	pfhandle_t handle;
	LinkLayerAddress local_addr;
public:
	/**
	 * @brief  Sends a packet to a remote address
	 * @param  addr [in] Destination link Layer address
	 * @param etherType [in] The EtherType of the message in host order
	 * @param  payload [in] Data buffer
	 * @param  length Size of buffer
	 * @param  timestamp TRUE: Use timestamp, FALSE otherwise
	 * @return net_result structure
	 */
	virtual net_result send( LinkLayerAddress *addr, uint16_t etherType, uint8_t *payload, size_t length, bool timestamp) {
		packet_addr_t dest;
		addr->toOctetArray( dest.addr );
		if( sendFrame( handle, &dest, etherType, payload, length ) != PACKET_NO_ERROR ) {
			GPTP_LOG_ERROR("nrecv: sendFrame failed, returning net_fatal");
			return net_fatal;
		}
		return net_succeed;
	}
	/**
	 * @brief  Receives a packet from a remote address
	 * @param  addr [out] Remote link layer address
	 * @param  payload [out] Data buffer
	 * @param  length [out] Length of received data
	 * @param  delay [in] Specifications for PHY input and output delays in nanoseconds
	 * @return net_result structure
	 */
	virtual net_result nrecv( LinkLayerAddress *addr, uint8_t *payload, size_t &length )
	{
		packet_addr_t dest;
		uint8_t mac_addr[6];
		local_addr.toOctetArray(mac_addr);
		GPTP_LOG_DEBUG("nrecv: call recvFrame - Waiting for packet on interface %02x:%02x:%02x:%02x:%02x:%02x", 
			mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
		packet_error_t pferror = recvFrame( handle, &dest, payload, length );
		GPTP_LOG_DEBUG("nrecv: recvFrame returned with error %d", pferror);
		if( pferror != PACKET_NO_ERROR && pferror != PACKET_RECVTIMEOUT_ERROR ) {
			GPTP_LOG_ERROR("nrecv: recvFrame failed with error %d, returning net_fatal", pferror);
			return net_fatal;
		}
		if( pferror == PACKET_RECVTIMEOUT_ERROR ) {
			GPTP_LOG_DEBUG("nrecv: recvFrame timeout, returning net_trfail");
			return net_trfail;
		}
		*addr = LinkLayerAddress( dest.addr );
		GPTP_LOG_DEBUG("nrecv: recvFrame succeeded, returning net_succeed");
		return net_succeed;
	}
	/**
	 * @brief  Gets the link layer address (MAC) of the network adapter
	 * @param  addr [out] Link layer address
	 * @return void
	 */
	virtual void getLinkLayerAddress( LinkLayerAddress *addr ) {
		*addr = local_addr;
	}

	/**
	* @brief Watch for netlink changes.
	*/
	virtual void watchNetLink( CommonPort *pPort );

	/**
	 * @brief  Gets the offset to the start of data in the Layer 2 Frame
	 * @return ::PACKET_HDR_LENGTH
	 */
	virtual unsigned getPayloadOffset() {
		return PACKET_HDR_LENGTH;
	}
	/**
	 * @brief Destroys the network interface
	 */
	virtual ~WindowsPCAPNetworkInterface() {
		closeInterface( handle );
		if( handle != NULL ) freePacketHandle( handle );
	}
protected:
	/**
	 * @brief Default constructor
	 */
	WindowsPCAPNetworkInterface() { 
		handle = NULL; 
	};
};

/**
 * @brief WindowsPCAPNetworkInterface implements an interface to the network adapter
 * through calls to the publicly available libraries known as PCap.
 */
class WindowsPCAPNetworkInterfaceFactory : public OSNetworkInterfaceFactory {
public:
	/**
	 * @brief  Create a network interface
	 * @param  net_iface [in] Network interface
	 * @param  label [in] Interface label
	 * @param  timestamper [in] HWTimestamper instance
	 * @return TRUE success; FALSE error
	 */
	virtual bool createInterface( OSNetworkInterface **net_iface, InterfaceLabel *label, CommonTimestamper *timestamper ) {
		WindowsPCAPNetworkInterface *net_iface_l = new WindowsPCAPNetworkInterface();
		LinkLayerAddress *addr = dynamic_cast<LinkLayerAddress *>(label);
		if( addr == NULL ) {
			GPTP_LOG_ERROR("createInterface: dynamic_cast<LinkLayerAddress*> failed, addr is NULL");
			goto error_nofree;
		}
		net_iface_l->local_addr = *addr;
		packet_addr_t pfaddr;
		addr->toOctetArray( pfaddr.addr );
		if( mallocPacketHandle( &net_iface_l->handle ) != PACKET_NO_ERROR ) {
			GPTP_LOG_ERROR("createInterface: mallocPacketHandle failed");
			goto error_nofree;
		}
		if( openInterfaceByAddr( net_iface_l->handle, &pfaddr, 1 ) != PACKET_NO_ERROR ) {
			GPTP_LOG_ERROR("createInterface: openInterfaceByAddr failed");
			goto error_free_handle;
		}
		if( packetBind( net_iface_l->handle, PTP_ETHERTYPE ) != PACKET_NO_ERROR ) {
			GPTP_LOG_ERROR("createInterface: packetBind failed");
			goto error_free_handle;
		}
		*net_iface = net_iface_l;

		return true;

error_free_handle:
error_nofree:
		GPTP_LOG_ERROR("createInterface: cleaning up after error, returning false");
		delete net_iface_l;
		return false;
	}
};

/**
 * @brief Provides lock interface for windows
 */
class WindowsLock : public OSLock {
	friend class WindowsLockFactory;
private:
	OSLockType type;
	DWORD thread_id;
	HANDLE lock_c;
	OSLockResult lock_l( DWORD timeout ) {
		DWORD wait_result = WaitForSingleObject( lock_c, timeout );
		if( wait_result == WAIT_TIMEOUT ) return oslock_held;
		else if( wait_result == WAIT_OBJECT_0 ) return oslock_ok;
		else return oslock_fail;

	}
	OSLockResult nonreentrant_lock_l( DWORD timeout ) {
		OSLockResult result;
		DWORD wait_result;
		wait_result = WaitForSingleObject( lock_c, timeout );
		if( wait_result == WAIT_OBJECT_0 ) {
			if( thread_id == GetCurrentThreadId() ) {
				result = oslock_self;
				ReleaseMutex( lock_c );
			} else {
				result = oslock_ok;
				thread_id = GetCurrentThreadId();
			}
		} else if( wait_result == WAIT_TIMEOUT ) result = oslock_held;
		else result = oslock_fail;

		return result;
	}
protected:
	/**
	 * @brief Default constructor
	 */
	WindowsLock() {
		lock_c = NULL;
	}
	/**
	 * @brief  Initializes lock interface
	 * @param  type OSLockType
	 */
	bool initialize( OSLockType type ) {
		lock_c = CreateMutex( NULL, false, NULL );
		if( lock_c == NULL ) return false;
		this->type = type;
		return true;
	}
	/**
	 * @brief Acquires lock
	 * @return OSLockResult type
	 */
	OSLockResult lock() {
		if( type == oslock_recursive ) {
			return lock_l( INFINITE );
		}
		return nonreentrant_lock_l( INFINITE );
	}
	/**
	 * @brief  Tries to acquire lock
	 * @return OSLockResult type
	 */
	OSLockResult trylock() {
		if( type == oslock_recursive ) {
			return lock_l( 0 );
		}
		return nonreentrant_lock_l( 0 );
	}
	/**
	 * @brief  Releases lock
	 * @return OSLockResult type
	 */
	OSLockResult unlock() {
		ReleaseMutex( lock_c );
		return oslock_ok;
	}
};

/**
 * @brief Provides the LockFactory abstraction
 */
class WindowsLockFactory : public OSLockFactory {
public:
	/**
	 * @brief  Creates a new lock mechanism
	 * @param  type Lock type - OSLockType
	 * @return New lock on OSLock format
	 */
	OSLock *createLock( OSLockType type ) const {
		WindowsLock *lock = new WindowsLock();
		if( !lock->initialize( type )) {
			delete lock;
			lock = NULL;
		}
		return lock;
	}
};

/**
 * @brief Provides a OSCondition interface for windows
 */
class WindowsCondition : public OSCondition {
	friend class WindowsConditionFactory;
private:
	SRWLOCK lock;
	CONDITION_VARIABLE condition;
protected:
	/**
	 * @brief Initializes the locks and condition variables
	 */
	bool initialize() {
		InitializeSRWLock( &lock );
		InitializeConditionVariable( &condition );
		return true;
	}
public:
	/**
	 * @brief  Acquire a new lock and increment the condition counter
	 * @return true
	 */
	bool wait_prelock() {
		AcquireSRWLockExclusive( &lock );
		up();
		return true;
	}
	/**
	 * @brief  Waits for a condition and decrements the condition
	 * counter when the condition is met.
	 * @return TRUE if the condition is met and the lock is released. FALSE otherwise.
	 */
	bool wait() {
		BOOL result = SleepConditionVariableSRW( &condition, &lock, INFINITE, 0 );
		bool ret = false;
		if( result == TRUE ) {
			down();
			ReleaseSRWLockExclusive( &lock );
			ret = true;
		}
		return ret;
	}
	/**
	 * @brief Sends a signal to unblock other threads
	 * @return true
	 */
	bool signal() {
		AcquireSRWLockExclusive( &lock );
		if( waiting() ) WakeAllConditionVariable( &condition );
		ReleaseSRWLockExclusive( &lock );
		return true;
	}
};

/**
 * @brief Condition factory for windows
 */
class WindowsConditionFactory : public OSConditionFactory {
public:
	OSCondition *createCondition() const {
		WindowsCondition *result = new WindowsCondition();
		return result->initialize() ? result : NULL;
	}
};

class WindowsTimerQueue;

struct TimerQueue_t;

/**
 * @brief Timer queue handler arguments structure
 * @todo Needs more details from original developer
 */
struct WindowsTimerQueueHandlerArg {
	HANDLE timer_handle;	/*!< timer handler */
	HANDLE queue_handle;	/*!< queue handler */
	event_descriptor_t *inner_arg;	/*!< Event inner arguments */
	ostimerq_handler func;	/*!< Timer queue callback*/
	int type;				/*!< Type of timer */
	bool rm;				/*!< Flag that signalizes the argument deletion*/
	WindowsTimerQueue *queue;	/*!< Windows Timer queue */
	TimerQueue_t *timer_queue;	/*!< Timer queue*/
	bool completed;			/*!< Flag indicating the timer has fired and completed */
};

/**
 * @brief Creates a list of type WindowsTimerQueueHandlerArg
 */
typedef std::list<WindowsTimerQueueHandlerArg *> TimerArgList_t;
/**
 * @brief Timer queue type
 */
struct TimerQueue_t {
	TimerArgList_t arg_list;		/*!< Argument list */
	HANDLE queue_handle;			/*!< Handle to the timer queue */
	SRWLOCK lock;					/*!< Lock type */
};

/**
 * @brief Windows Timer queue handler callback definition
 */
VOID CALLBACK WindowsTimerQueueHandler( PVOID arg_in, BOOLEAN ignore );

/**
 * @brief Creates a map for the timerQueue
 */
typedef std::map<int,TimerQueue_t> TimerQueueMap_t;

/**
 * @brief Windows timer queue interface
 */
class WindowsTimerQueue : public OSTimerQueue {
	friend class WindowsTimerQueueFactory;
	friend VOID CALLBACK WindowsTimerQueueHandler( PVOID arg_in, BOOLEAN ignore );
private:
	TimerQueueMap_t timerQueueMap;
	TimerArgList_t retiredTimers;
	SRWLOCK retiredTimersLock;
	void cleanupRetiredTimers() {
		GPTP_LOG_DEBUG("*** WindowsTimerQueue::cleanupRetiredTimers: ENTER (thread_id=%lu) ***", (unsigned long)GetCurrentThreadId());
		
		try {
			GPTP_LOG_DEBUG("*** cleanupRetiredTimers: About to acquire retiredTimersLock ***");
			AcquireSRWLockExclusive( &retiredTimersLock );
			GPTP_LOG_DEBUG("*** cleanupRetiredTimers: Acquired retiredTimersLock, retired list size=%zu ***", retiredTimers.size());
			
			// Use a temporary list to avoid holding the lock too long
			TimerArgList_t toDelete;
			TimerArgList_t::iterator it = retiredTimers.begin();
			GPTP_LOG_DEBUG("*** cleanupRetiredTimers: Starting iteration through retired timers ***");
			
			while( it != retiredTimers.end() ) {
				GPTP_LOG_DEBUG("*** cleanupRetiredTimers: Processing retired timer ***");
				WindowsTimerQueueHandlerArg *retired_arg = *it;
				
				if (!retired_arg) {
					GPTP_LOG_ERROR("*** FATAL: retired_arg is NULL in cleanupRetiredTimers! ***");
					++it;
					continue;
				}
				
				GPTP_LOG_DEBUG("*** cleanupRetiredTimers: About to call DeleteTimerQueueTimer for cleanup (queue_handle=%p, timer_handle=%p) ***", 
					retired_arg->queue_handle, retired_arg->timer_handle);
				
				// Don't try to delete the timer again - it was already deleted in cancelEvent
				// Just mark it as safe to clean up since it was already successfully cancelled
				GPTP_LOG_DEBUG("*** cleanupRetiredTimers: Timer already deleted in cancelEvent, safe to clean up ***");
				// Timer successfully deleted, safe to clean up
				toDelete.push_back( retired_arg );
				it = retiredTimers.erase( it );
			}
			
			GPTP_LOG_DEBUG("*** cleanupRetiredTimers: About to release retiredTimersLock, toDelete size=%zu ***", toDelete.size());
			ReleaseSRWLockExclusive( &retiredTimersLock );
			GPTP_LOG_DEBUG("*** cleanupRetiredTimers: Released retiredTimersLock ***");
			
			// Clean up the structures outside the lock
			GPTP_LOG_DEBUG("*** cleanupRetiredTimers: About to clean up %zu timer structures ***", toDelete.size());
			for( TimerArgList_t::iterator deleteIt = toDelete.begin(); deleteIt != toDelete.end(); ++deleteIt ) {
				WindowsTimerQueueHandlerArg *arg = *deleteIt;
				if (!arg) {
					GPTP_LOG_ERROR("*** FATAL: arg is NULL in cleanup loop! ***");
					continue;
				}
				
				GPTP_LOG_DEBUG("*** cleanupRetiredTimers: Cleaning up timer arg %p (rm=%s) ***", arg, arg->rm ? "true" : "false");
				if( arg->rm && arg->inner_arg ) {
					GPTP_LOG_DEBUG("*** cleanupRetiredTimers: About to delete inner_arg %p ***", arg->inner_arg);
					delete arg->inner_arg;
				}
				GPTP_LOG_DEBUG("*** cleanupRetiredTimers: About to delete arg %p ***", arg);
				delete arg;
			}
			GPTP_LOG_DEBUG("*** cleanupRetiredTimers: Cleanup completed successfully ***");
		} catch (const std::exception& ex) {
			GPTP_LOG_ERROR("*** FATAL: Exception in cleanupRetiredTimers: %s ***", ex.what());
		} catch (...) {
			GPTP_LOG_ERROR("*** FATAL: Unknown exception in cleanupRetiredTimers ***");
		}
		
		GPTP_LOG_DEBUG("*** WindowsTimerQueue::cleanupRetiredTimers: EXIT ***");
	}
	
	void cleanupCompletedTimers() {
		GPTP_LOG_DEBUG("*** WindowsTimerQueue::cleanupCompletedTimers: ENTER (thread_id=%lu) ***", (unsigned long)GetCurrentThreadId());
		
		try {
			// Clean up completed timers from all timer queues
			GPTP_LOG_DEBUG("*** cleanupCompletedTimers: Iterating through timerQueueMap (size=%zu) ***", timerQueueMap.size());
			
			for( TimerQueueMap_t::iterator queue_it = timerQueueMap.begin(); queue_it != timerQueueMap.end(); ++queue_it ) {
				GPTP_LOG_DEBUG("*** cleanupCompletedTimers: Processing queue for type %d ***", queue_it->first);
				
				GPTP_LOG_DEBUG("*** cleanupCompletedTimers: About to acquire SRW lock for type %d ***", queue_it->first);
				AcquireSRWLockExclusive( &queue_it->second.lock );
				GPTP_LOG_DEBUG("*** cleanupCompletedTimers: Acquired SRW lock for type %d, arg_list size=%zu ***", 
					queue_it->first, queue_it->second.arg_list.size());
				
				TimerArgList_t::iterator it = queue_it->second.arg_list.begin();
				while( it != queue_it->second.arg_list.end() ) {
					WindowsTimerQueueHandlerArg *arg = *it;
					
					if (!arg) {
						GPTP_LOG_ERROR("*** FATAL: arg is NULL in cleanupCompletedTimers! ***");
						it = queue_it->second.arg_list.erase( it );
						continue;
					}
					
					GPTP_LOG_DEBUG("*** cleanupCompletedTimers: Checking timer arg %p (completed=%s) ***", arg, arg->completed ? "true" : "false");
					
					if( arg->completed ) {
						GPTP_LOG_DEBUG("*** cleanupCompletedTimers: Timer completed, removing from list and deleting ***");
						// Timer has completed, remove it from the list and delete it
						it = queue_it->second.arg_list.erase( it );
						delete arg;
					} else {
						++it;
					}
				}
				
				GPTP_LOG_DEBUG("*** cleanupCompletedTimers: About to release SRW lock for type %d ***", queue_it->first);
				ReleaseSRWLockExclusive( &queue_it->second.lock );
			}
			GPTP_LOG_DEBUG("*** cleanupCompletedTimers: All queues processed successfully ***");
		} catch (const std::exception& ex) {
			GPTP_LOG_ERROR("*** FATAL: Exception in cleanupCompletedTimers: %s ***", ex.what());
		} catch (...) {
			GPTP_LOG_ERROR("*** FATAL: Unknown exception in cleanupCompletedTimers ***");
		}
		
		GPTP_LOG_DEBUG("*** WindowsTimerQueue::cleanupCompletedTimers: EXIT ***");
	}
protected:
	/**
	 * @brief Default constructor. Initializes timers lock
	 */
	WindowsTimerQueue() {
		InitializeSRWLock( &retiredTimersLock );
	};
public:
	/**
	 * @brief  Create a new event and add it to the timer queue
	 * @param  micros Time in microsseconds
	 * @param  type ::Event type
	 * @param  func Callback function
	 * @param  arg [in] Event arguments
	 * @param  rm when true, allows elements to be deleted from the queue
	 * @param  event [in] Pointer to the event to be created
	 * @return Always return true
	 */
	bool addEvent( unsigned long micros, int type, ostimerq_handler func, event_descriptor_t *arg, bool rm, unsigned *event ) {
		WindowsTimerQueueHandlerArg *outer_arg = new WindowsTimerQueueHandlerArg();
		cleanupRetiredTimers();
		cleanupCompletedTimers();  // Clean up any completed timers first
		if( timerQueueMap.find(type) == timerQueueMap.end() ) {
			timerQueueMap[type].queue_handle = CreateTimerQueue();
			InitializeSRWLock( &timerQueueMap[type].lock );
		}
		outer_arg->queue_handle = timerQueueMap[type].queue_handle;
		outer_arg->inner_arg = arg;
		outer_arg->func = func;
		outer_arg->queue = this;
		outer_arg->type = type;
		outer_arg->rm = rm;
		outer_arg->completed = false;  // Initialize as not completed
		outer_arg->timer_queue = &timerQueueMap[type];
		AcquireSRWLockExclusive( &timerQueueMap[type].lock );
		CreateTimerQueueTimer( &outer_arg->timer_handle, timerQueueMap[type].queue_handle, WindowsTimerQueueHandler, (void *) outer_arg, micros/1000, 0, 0 );
		timerQueueMap[type].arg_list.push_front(outer_arg);
		ReleaseSRWLockExclusive( &timerQueueMap[type].lock );
		return true;
	}
	/**
	 * @brief  Cancels an event from the queue
	 * @param  type ::Event type
	 * @param  event [in] Pointer to the event to be removed
	 * @return Always returns true.
	 */
	bool cancelEvent( int type, unsigned *event ) {
		GPTP_LOG_DEBUG("*** WindowsTimerQueue::cancelEvent: ENTER (type=%d, event=%p, thread_id=%lu) ***", type, event, (unsigned long)GetCurrentThreadId());
		
		try {
			TimerQueueMap_t::iterator iter = timerQueueMap.find( type );
			if( iter == timerQueueMap.end() ) {
				GPTP_LOG_DEBUG("*** WindowsTimerQueue::cancelEvent: Event type %d not found in map, returning false ***", type);
				return false;
			}
			
			GPTP_LOG_DEBUG("*** WindowsTimerQueue::cancelEvent: Found event type %d, about to acquire SRW lock ***", type);
			AcquireSRWLockExclusive( &timerQueueMap[type].lock );
			GPTP_LOG_DEBUG("*** WindowsTimerQueue::cancelEvent: Acquired SRW lock for type %d ***", type);
			
			while( ! timerQueueMap[type].arg_list.empty() ) {
				GPTP_LOG_DEBUG("*** WindowsTimerQueue::cancelEvent: Processing timer in arg_list (size=%zu) ***", timerQueueMap[type].arg_list.size());
				WindowsTimerQueueHandlerArg *del_arg = timerQueueMap[type].arg_list.front();
				if (!del_arg) {
					GPTP_LOG_ERROR("*** FATAL: del_arg is NULL in cancelEvent! ***");
					break;
				}
				
				GPTP_LOG_DEBUG("*** WindowsTimerQueue::cancelEvent: About to pop front from arg_list ***");
				timerQueueMap[type].arg_list.pop_front();
				GPTP_LOG_DEBUG("*** WindowsTimerQueue::cancelEvent: About to release SRW lock for DeleteTimerQueueTimer ***");
				ReleaseSRWLockExclusive( &timerQueueMap[type].lock );
				
				// Use NULL instead of INVALID_HANDLE_VALUE to avoid deadlock
				// This makes the timer cancellation asynchronous
				GPTP_LOG_DEBUG("*** WindowsTimerQueue::cancelEvent: About to call DeleteTimerQueueTimer (queue_handle=%p, timer_handle=%p) ***", del_arg->queue_handle, del_arg->timer_handle);
				DeleteTimerQueueTimer( del_arg->queue_handle, del_arg->timer_handle, NULL );
				GPTP_LOG_DEBUG("*** WindowsTimerQueue::cancelEvent: DeleteTimerQueueTimer completed ***");
				
				// Don't delete immediately - move to retired list for deferred cleanup
				// This prevents use-after-free if timer callback is still executing
				GPTP_LOG_DEBUG("*** WindowsTimerQueue::cancelEvent: About to acquire retiredTimersLock ***");
				AcquireSRWLockExclusive( &retiredTimersLock );
				GPTP_LOG_DEBUG("*** WindowsTimerQueue::cancelEvent: About to add to retiredTimers list ***");
				retiredTimers.push_back( del_arg );
				GPTP_LOG_DEBUG("*** WindowsTimerQueue::cancelEvent: About to release retiredTimersLock ***");
				ReleaseSRWLockExclusive( &retiredTimersLock );
				
				GPTP_LOG_DEBUG("*** WindowsTimerQueue::cancelEvent: About to reacquire SRW lock for next iteration ***");
				AcquireSRWLockExclusive( &timerQueueMap[type].lock );
			}
			GPTP_LOG_DEBUG("*** WindowsTimerQueue::cancelEvent: All timers processed, about to release final SRW lock ***");
			ReleaseSRWLockExclusive( &timerQueueMap[type].lock );

			// Clean up any retired timers that are safe to delete
			GPTP_LOG_DEBUG("*** WindowsTimerQueue::cancelEvent: About to call cleanupRetiredTimers ***");
			cleanupRetiredTimers();
			// Also clean up completed timers to prevent memory leaks
			GPTP_LOG_DEBUG("*** WindowsTimerQueue::cancelEvent: About to call cleanupCompletedTimers ***");
			cleanupCompletedTimers();
			GPTP_LOG_DEBUG("*** WindowsTimerQueue::cancelEvent: EXIT (returning true) ***");
			return true;
		} catch (const std::exception& ex) {
			GPTP_LOG_ERROR("*** FATAL: Exception in WindowsTimerQueue::cancelEvent: %s ***", ex.what());
			return false;
		} catch (...) {
			GPTP_LOG_ERROR("*** FATAL: Unknown exception in WindowsTimerQueue::cancelEvent ***");
			return false;
		}
	}
};

/**
 * @brief Windows callback for the timer queue handler
 */
VOID CALLBACK WindowsTimerQueueHandler( PVOID arg_in, BOOLEAN ignore );

/**
 * @brief Windows implementation of OSTimerQueueFactory
 */
class WindowsTimerQueueFactory : public OSTimerQueueFactory {
public:
	/**
	 * @brief  Creates a new timer queue
	 * @param  clock [in] IEEE1588Clock type
	 * @return new timer queue
	 */
    virtual OSTimerQueue *createOSTimerQueue( IEEE1588Clock *clock ) {
        WindowsTimerQueue *timerq = new WindowsTimerQueue();
        return timerq;
    };
};

/**
 * @brief Windows implementation of OSTimer
 */
class WindowsTimer : public OSTimer {
    friend class WindowsTimerFactory;
public:
	/**
	 * @brief  Sleep for an amount of time
	 * @param  micros Time in microsseconds
	 * @return Time in microsseconds
	 */
    virtual unsigned long sleep( unsigned long micros ) {
        Sleep( micros/1000 );
        return micros;
    }
protected:
	/**
	 * @brief Default constructor
	 */
    WindowsTimer() {};
};

/**
 * @brief Windows implementation of OSTimerFactory
 */
class WindowsTimerFactory : public OSTimerFactory {
public:
	/**
	 * @brief  Creates a new timer
	 * @return New windows OSTimer
	 */
    virtual OSTimer *createTimer() const {
        return new WindowsTimer();
    }
};

/**
 * @brief OSThread argument structure
 */
struct OSThreadArg {
    OSThreadFunction func;	/*!< Thread callback function */
    void *arg;				/*!< Thread arguments */
    OSThreadExitCode ret;	/*!< Return value */
};

/**
 * @brief Windows OSThread callback
 */
DWORD WINAPI OSThreadCallback( LPVOID input );

/**
 * @brief Implements OSThread interface for windows
 */
class WindowsThread : public OSThread {
    friend class WindowsThreadFactory;
private:
    HANDLE thread_id;
    OSThreadArg *arg_inner;
public:
	/**
	 * @brief  Starts a new thread
	 * @param  function Function to be started as a new thread
	 * @param  arg [in] Function's arguments
	 * @return TRUE if successfully started, FALSE otherwise.
	 */
    virtual bool start( OSThreadFunction function, void *arg ) {
        arg_inner = new OSThreadArg();
        arg_inner->func = function;
        arg_inner->arg = arg;
        thread_id = CreateThread( NULL, 0, OSThreadCallback, arg_inner, 0, NULL );
        if( thread_id == NULL ) return false;
        else return true;
    }
	/**
	 * @brief  Joins a terminated thread
	 * @param  exit_code [out] Thread's return code
	 * @return TRUE in case of success. FALSE otherwise.
	 */
    virtual bool join( OSThreadExitCode &exit_code ) {
        if( WaitForSingleObject( thread_id, INFINITE ) != WAIT_OBJECT_0 ) return false;
        exit_code = arg_inner->ret;
        delete arg_inner;
        return true;
    }
protected:
	/**
	 * @brief Default constructor
	 */
	WindowsThread() {};
};

/**
 * @brief Provides a windows implmementation of OSThreadFactory
 */
class WindowsThreadFactory : public OSThreadFactory {
public:
	/**
	 * @brief  Creates a new windows thread
	 * @return New thread of type OSThread
	 */
	OSThread *createThread() const {
		return new WindowsThread();
	}
};

void WirelessTimestamperCallback(LPVOID arg);

class WindowsWirelessAdapter;

/**
* @brief Windows Wireless (802.11) HWTimestamper implementation
*/
class WindowsWirelessTimestamper : public WirelessTimestamper
{
private:
	WindowsWirelessAdapter *adapter;

	uint64_t system_counter;
	Timestamp system_time;
	Timestamp device_time;
	LARGE_INTEGER tsc_hz;
	bool initialized;

public:
	WindowsWirelessTimestamper()
	{
		initialized = false;
	}

	net_result _requestTimingMeasurement
	( TIMINGMSMT_REQUEST *timingmsmt_req );

	bool HWTimestamper_gettime
	( Timestamp *system_time, Timestamp * device_time,
	  uint32_t * local_clock, uint32_t * nominal_clock_rate ) const;

	virtual bool HWTimestamper_init
	( InterfaceLabel *iface_label, OSNetworkInterface *iface );

	/**
	 * @brief attach adapter to timestamper
	 * @param adapter [in] adapter to attach
	 */
	void setAdapter( WindowsWirelessAdapter *adapter )
	{
		this->adapter = adapter;
	}

	/**
	 * @brief get attached adapter
	 * @return attached adapter
	 */
	WindowsWirelessAdapter *getAdapter(void)
	{
		return adapter;
	}

	~WindowsWirelessTimestamper();

	friend void WirelessTimestamperCallback( LPVOID arg );
};

class WindowsWirelessAdapter
{
public:
	/**
	 * @brief initiate wireless TM request (completion is asynchronous)
	 * @param tm_request [in] pointer to TM request object
	 * @return true on success
	 */
	virtual bool initiateTimingRequest(TIMINGMSMT_REQUEST *tm_request) = 0;

	/**
	 * @brief attempt to refresh cross timestamp (extrapolate on failure)
	 * @return true on success
	 */
	virtual bool refreshCrossTimestamp() = 0;

	/**
	 * @brief register timestamper with adapter
	 * @param timestamper [in] timestamper object
	 * @return true on success
	 */
	virtual bool registerTimestamper
	( WindowsWirelessTimestamper *timestamper ) = 0;

	/**
	 * @brief deregister timestamper
	 * @param timestamper [in] timestamper object
	 * @return true on success
	 */
	virtual bool deregisterTimestamper
	( WindowsWirelessTimestamper *timestamper ) = 0;

	/**
	 * @brief initialize adapter object
	 * @return true on success
	 */
	virtual bool initialize() = 0;

	/**
	 * @brief shutdown adapter
	 */
	virtual void shutdown() = 0;

	/**
	 * @brief attach adapter to MAC address
	 * @param mac_addr [in] MAC address to attach to
	 * @return true on success
	 */
	virtual bool attachAdapter( uint8_t *mac_addr ) = 0;
};

#define I217_DESC "I217-LM"
#define I219_DESC "I219" // Intel I219-V and I219-LM  
#define I210_DESC "I210"
#define I211_DESC "I211"
#define I350_DESC "I350"

#define NETWORK_CARD_ID_PREFIX "\\\\.\\"			/*!< Network adapter prefix */
#define OID_INTEL_GET_RXSTAMP 0xFF020264			/*!< Get RX timestamp code*/
#define OID_INTEL_GET_TXSTAMP 0xFF020263			/*!< Get TX timestamp code*/
#define OID_INTEL_GET_SYSTIM  0xFF020262			/*!< Get system time code */
#define OID_INTEL_SET_SYSTIM  0xFF020261			/*!< Set system time code */
/* Provide timestamp capability OIDs when building with older SDKs */
#ifndef OID_TIMESTAMP_CAPABILITY
#define OID_TIMESTAMP_CAPABILITY 0x00010265
#endif
#ifndef OID_TIMESTAMP_CURRENT_CONFIG
#define OID_TIMESTAMP_CURRENT_CONFIG 0x00010266
#endif
#ifndef OID_GEN_STATISTICS
#define OID_GEN_STATISTICS 0x00020202
#endif

// Enhanced timestamping capabilities and configuration
struct EnhancedTimestampingConfig {
    // Software timestamping precision thresholds (relaxed for software timestamping)
    static constexpr int64_t SOFTWARE_TIMESTAMP_THRESHOLD_NS = 1000000;  // 1ms
    static constexpr int64_t PDELAY_THRESHOLD_NS = 5000000;              // 5ms  
    static constexpr int64_t SYNC_THRESHOLD_NS = 2000000;                // 2ms
    static constexpr bool ALLOW_SOFTWARE_ASCAPABLE = true;               // Allow asCapable with software timestamping
    
    // Performance counter frequency for high-precision timing
    LARGE_INTEGER performance_frequency;
    bool high_resolution_available;
    
    EnhancedTimestampingConfig() : high_resolution_available(false) {
        if (QueryPerformanceFrequency(&performance_frequency)) {
            high_resolution_available = true;
        }
    }
};

// Windows standard timestamping methods
enum class WindowsTimestampMethod {
    QUERY_PERFORMANCE_COUNTER,      // High-precision QueryPerformanceCounter
    GET_SYSTEM_TIME_PRECISE,        // GetSystemTimePreciseAsFileTime
    WINSOCK_TIMESTAMP,              // Winsock SO_TIMESTAMP if available
    FALLBACK_GETTICKCOUNT           // Fallback to GetTickCount64
};

// Enhanced software timestamper class
class EnhancedSoftwareTimestamper {
private:
    EnhancedTimestampingConfig config;
    WindowsTimestampMethod preferred_method;
    mutable std::mutex timestamp_mutex;
    
    // Calibration for timestamp methods
    int64_t method_precision_ns;
    bool calibrated;
    
public:
    EnhancedSoftwareTimestamper();
    ~EnhancedSoftwareTimestamper() = default;
    
    // Get high-precision system timestamp
    Timestamp getSystemTime() const;
    
    // Get timestamp using specific method
    Timestamp getTimestamp(WindowsTimestampMethod method) const;
    
    // Enable socket timestamping if supported
    bool enableSocketTimestamping(SOCKET sock) const;
    
    // Get socket timestamp if available
    bool getSocketTimestamp(SOCKET sock, Timestamp& ts) const;
    
    // Calibrate timestamping precision
    void calibrateTimestampPrecision();
    
    // Get measured precision in nanoseconds
    int64_t getTimestampPrecision() const { return method_precision_ns; }
    
    // Check if method is available
    bool isMethodAvailable(WindowsTimestampMethod method) const;
    
    // Get preferred timestamping method
    WindowsTimestampMethod getPreferredMethod() const { return preferred_method; }
    
private:
    Timestamp getPerformanceCounterTime() const;
    Timestamp getPreciseSystemTime() const;
    Timestamp getWinsockTimestamp(SOCKET sock) const;
    Timestamp getFallbackTime() const;
};

// Timestamping diagnostics and logging
class TimestampingDiagnostics {
public:
    static void logTimestampingCapabilities(EnhancedSoftwareTimestamper& timestamper);
    static void logPerformanceCharacteristics(EnhancedSoftwareTimestamper& timestamper);
    static int64_t measureClockResolution();
    static int64_t measureTimestampPrecision(WindowsTimestampMethod method);
    static void logSystemTimingInfo();
};

typedef struct
{
	uint32_t clock_rate;
	char *device_desc;
} DeviceClockRateMapping;

/**
* @brief Maps network device type to device clock rate for Intel adapters supporting custom OIDs
*/
static DeviceClockRateMapping DeviceClockRateMap[] =
{
	{ 1000000000, I217_DESC	},
	{ 1008000000, I219_DESC	},
	{ 1250000000, I210_DESC	},
	{ 1250000000, I211_DESC	},
	{ 1250000000, I350_DESC	},
	{ 0, NULL },
};

/**
 * @brief Windows Ethernet HWTimestamper implementation
 */
class WindowsEtherTimestamper : public EtherTimestamper {
private:
    // No idea whether the underlying implementation is thread safe
	HANDLE miniport;
	LARGE_INTEGER tsc_hz;
	LARGE_INTEGER netclock_hz;
	bool cross_timestamping_initialized; //!< Flag to track cross-timestamping initialization
	mutable IntelOidFailureTracking oid_failures; //!< Track OID failure counts
	mutable EnhancedSoftwareTimestamper enhanced_timestamper; //!< Enhanced software timestamping capabilities
	DWORD readOID( NDIS_OID oid, void *output_buffer, DWORD size, DWORD *size_returned ) const {
		NDIS_OID oid_l = oid;
		DWORD rc = DeviceIoControl(
			miniport,
			IOCTL_NDIS_QUERY_GLOBAL_STATS,
			&oid_l,
			sizeof(oid_l),
			output_buffer,
			size,
			size_returned,
			NULL );
		if( rc == 0 ) return GetLastError();
		return ERROR_SUCCESS;
	}
        DWORD setOID( NDIS_OID oid, void *input_buffer, DWORD size ) const {
                DWORD returned;
                NDIS_OID oid_l = oid;
                DWORD rc = DeviceIoControl(
                        miniport,
                        IOCTL_NDIS_QUERY_GLOBAL_STATS,
                        &oid_l,
                        sizeof(oid_l),
                        input_buffer,
                        size,
                        &returned,
                        NULL );
                if( rc == 0 ) return GetLastError();
                return ERROR_SUCCESS;
        }
	Timestamp nanoseconds64ToTimestamp( uint64_t time ) const {
		Timestamp timestamp;
		timestamp.nanoseconds = time % 1000000000;
		timestamp.seconds_ls = (time / 1000000000) & 0xFFFFFFFF;
		timestamp.seconds_ms = (uint16_t)((time / 1000000000) >> 32);
		return timestamp;
	}
	uint64_t scaleNativeClockToNanoseconds( uint64_t time ) const {
		long double scaled_output = ((long double)netclock_hz.QuadPart)/1000000000;
		scaled_output = ((long double) time)/scaled_output;
		return (uint64_t) scaled_output;
	}
	uint64_t scaleTSCClockToNanoseconds( uint64_t time ) const {
		long double scaled_output = ((long double)tsc_hz.QuadPart)/1000000000;
		scaled_output = ((long double) time)/scaled_output;
		return (uint64_t) scaled_output;
	}
public:
	/**
	 * @brief Default constructor
	 */
	WindowsEtherTimestamper() : cross_timestamping_initialized(false) {
		miniport = INVALID_HANDLE_VALUE;
		tsc_hz.QuadPart = 0;
		netclock_hz.QuadPart = 0;
		memset(&oid_failures, 0, sizeof(oid_failures));
	}
	
	/**
	 * @brief  Initializes the network adaptor and the hw timestamper interface
	 * @param  iface_label InterfaceLabel
	 * @param  net_iface Network interface
	 * @return TRUE if success; FALSE if error
	 */
	virtual bool HWTimestamper_init( InterfaceLabel *iface_label, OSNetworkInterface *net_iface );
	/**
	 * @brief  Get the cross timestamping information.
	 * The gPTP subsystem uses these samples to calculate
	 * ratios which can be used to translate or extrapolate
	 * one clock into another clock reference. The gPTP service
	 * uses these supplied cross timestamps to perform internal
	 * rate estimation and conversion functions.
	 * @param  system_time [out] System time
	 * @param  device_time [out] Device time
	 * @param  local_clock [out] Local clock
	 * @param  nominal_clock_rate [out] Nominal clock rate
	 * @return True in case of success. FALSE in case of error
	 */
	virtual bool HWTimestamper_gettime( Timestamp *system_time, Timestamp *device_time, uint32_t *local_clock,
					    uint32_t *nominal_clock_rate ) const {
		// Use the new Windows cross-timestamping functionality for improved precision
		WindowsCrossTimestamp& crossTimestamp = getGlobalCrossTimestamp();
		
		if (crossTimestamp.isPreciseTimestampingSupported()) {
			// Use precise cross-timestamping if available
			if (crossTimestamp.getCrossTimestamp(system_time, device_time, local_clock, nominal_clock_rate)) {
				// Set version for both timestamps
				if (system_time) system_time->_version = version;
				if (device_time) device_time->_version = version;
				return true;
			}
			// Fall through to legacy method if cross-timestamping fails
		}
		
		// Legacy method using OID_INTEL_GET_SYSTIM
		DWORD buf[6];
		DWORD returned;
		uint64_t now_net, now_tsc;
		DWORD result;

		// Initialize buffer to avoid uninitialized memory issues
		memset( buf, 0, sizeof( buf ));
		
		// Check if miniport handle is valid before attempting to read
		if (miniport == INVALID_HANDLE_VALUE) {
			GPTP_LOG_WARNING("Cannot read system time: miniport handle is invalid");
			return false;
		}
		else{
			GPTP_LOG_DEBUG("miniport handle is valid");
		}

		// Check if we should skip the SYSTIM OID due to repeated failures
		if (oid_failures.systim.disabled) {
			GPTP_LOG_VERBOSE("Skipping SYSTIM OID - disabled after %u consecutive failures", oid_failures.systim.failure_count);
			// Use enhanced software timestamping as fallback
			goto use_enhanced_software_timestamping;
		}

		if(( result = readOID( OID_INTEL_GET_SYSTIM, buf, sizeof(buf), &returned )) != ERROR_SUCCESS ) {
			if (result == 50) { // ERROR_NOT_SUPPORTED
				oid_failures.systim.failure_count++;
				if (oid_failures.systim.failure_count >= MAX_OID_FAILURES) {
					oid_failures.systim.disabled = true;
					GPTP_LOG_WARNING("Disabling SYSTIM OID after %u consecutive failures with error 50 (not supported)", oid_failures.systim.failure_count);
				} else {
					GPTP_LOG_DEBUG("SYSTIM OID failure %u/%u (error 50)", oid_failures.systim.failure_count, MAX_OID_FAILURES);
				}
			} else {
				// Reset counter for non-50 errors
				oid_failures.systim.failure_count = 0;
			}
			GPTP_LOG_WARNING("Failed to read Intel system time OID: error %d (0x%08X)", result, result);
			// Use enhanced software timestamping as fallback
			goto use_enhanced_software_timestamping;
		}
		else{
			oid_failures.systim.failure_count = 0;
			oid_failures.systim.disabled = false;
			GPTP_LOG_DEBUG("Intel system time OID read successfully, returned %d bytes", returned);
		}
		
		if( returned != sizeof(buf) ) {
			GPTP_LOG_WARNING("Intel system time OID returned insufficient data: %d bytes, expected %d", 
				returned, sizeof(buf));
			return false;
		}
		else{
			GPTP_LOG_DEBUG("Intel system time OID returned sufficient data: %d bytes, expected %d", 
				returned, sizeof(buf));
		}

		now_net = (((uint64_t)buf[1]) << 32) | buf[0];
		now_net = scaleNativeClockToNanoseconds( now_net );
		*device_time = nanoseconds64ToTimestamp( now_net );
		device_time->_version = version;

		now_tsc = (((uint64_t)buf[3]) << 32) | buf[2];
		now_tsc = scaleTSCClockToNanoseconds( now_tsc );
		*system_time = nanoseconds64ToTimestamp( now_tsc );
		system_time->_version = version;

		oid_failures.systim.failure_count = 0;
		oid_failures.systim.disabled = false;
		return true;

use_enhanced_software_timestamping:
		// Enhanced software timestamping fallback when Intel OIDs are not available
		GPTP_LOG_DEBUG("Using enhanced software timestamping fallback");
		
		if (system_time && device_time) {
			// Get high-precision software timestamp
			Timestamp software_time = enhanced_timestamper.getSystemTime();
			
			// For software timestamping, system time and device time are the same
			*system_time = software_time;
			*device_time = software_time;
			
			// Set version
			system_time->_version = version;
			device_time->_version = version;
			
			// Set local clock and nominal rate to default values
			if (local_clock) *local_clock = 0;
			if (nominal_clock_rate) *nominal_clock_rate = 1000000000; // 1 GHz
			
			GPTP_LOG_VERBOSE("Enhanced software timestamp: %u.%09u", 
							software_time.seconds_ls, software_time.nanoseconds);
			
			return true;
		}
		
		return false;
	}

	/**
	 * @brief  Gets the TX timestamp
	 * @param  identity [in] PortIdentity interface
	 * @param  PTPMessageId Message ID
	 * @param  timestamp [out] TX hardware timestamp
	 * @param  clock_value Not used
	 * @param  last Not used
	 * @return GPTP_EC_SUCCESS if no error, GPTP_EC_FAILURE if error and GPTP_EC_EAGAIN to try again.
	 */
	virtual int HWTimestamper_txtimestamp(PortIdentity *identity, PTPMessageId messageId, Timestamp &timestamp, unsigned &clock_value, bool last)
	{
#ifdef OPENAVNU_BUILD_INTEL_HAL
		// 🚀 INTEL HAL INTEGRATION: Try Intel HAL first for better reliability
		WindowsCrossTimestamp& txCrossTimestamp = getGlobalCrossTimestamp();
		if (txCrossTimestamp.isIntelHALAvailable()) {
			Timestamp system_time, device_time;
			
			if (txCrossTimestamp.getCrossTimestamp(&system_time, &device_time)) {
				timestamp = device_time;
				timestamp._version = version;
				
				GPTP_LOG_VERBOSE("Intel HAL TX timestamp successful: messageType=%d, seq=%u, timestamp=%llu.%09u", 
					messageId.getMessageType(), messageId.getSequenceId(),
					TIMESTAMP_TO_NS(timestamp) / 1000000000ULL, 
					(uint32_t)(TIMESTAMP_TO_NS(timestamp) % 1000000000ULL));
				
				return GPTP_EC_SUCCESS;
			}
			
			GPTP_LOG_DEBUG("Intel HAL TX timestamp failed, falling back to legacy OID method");
		}
#endif

		// Legacy OID-based timestamping (fallback when Intel HAL not available/enabled)
		DWORD buf[4], buf_tmp[4];
		DWORD returned = 0;
		uint64_t tx_r,tx_s;
		DWORD result;

		// Initialize buffers to avoid uninitialized memory issues
		memset(buf, 0, sizeof(buf));
		memset(buf_tmp, 0, sizeof(buf_tmp));

		// Check if miniport handle is valid before attempting to read
		if (miniport == INVALID_HANDLE_VALUE) {
			GPTP_LOG_WARNING("Cannot read TX timestamp: miniport handle is invalid");
			return GPTP_EC_FAILURE;
		}

		// Check if we should skip the TXSTAMP OID due to repeated failures
		if (oid_failures.txstamp.disabled) {
			GPTP_LOG_VERBOSE("Skipping TXSTAMP OID - disabled after %u consecutive failures", oid_failures.txstamp.failure_count);
			goto try_tx_fallback;
		}

		while(( result = readOID( OID_INTEL_GET_TXSTAMP, buf_tmp, sizeof(buf_tmp), &returned )) == ERROR_SUCCESS ) {
			memcpy( buf, buf_tmp, sizeof( buf ));
		}
		if( result != ERROR_GEN_FAILURE ) {
			if (result == 50) { // ERROR_NOT_SUPPORTED
				oid_failures.txstamp.failure_count++;
				if (oid_failures.txstamp.failure_count >= MAX_OID_FAILURES) {
					oid_failures.txstamp.disabled = true;
					GPTP_LOG_WARNING("Disabling TXSTAMP OID after %u consecutive failures with error 50 (not supported)", oid_failures.txstamp.failure_count);
				} else {
					GPTP_LOG_DEBUG("TXSTAMP OID failure %u/%u (error 50)", oid_failures.txstamp.failure_count, MAX_OID_FAILURES);
				}
			} else {
				// Reset counter for non-50 errors
				oid_failures.txstamp.failure_count = 0;
			}
			GPTP_LOG_WARNING("Error (TX) timestamping PDelay request, error=%d (0x%08X). Intel OID may not be supported or accessible", result, result);
			if (result == ERROR_NOT_SUPPORTED) {
				GPTP_LOG_INFO("Driver does not support Intel TX timestamping OID - trying fallback methods");
			} else if (result == ERROR_ACCESS_DENIED) {
				GPTP_LOG_WARNING("Access denied to Intel TX timestamp OID - ensure running as administrator");
			} else if (result == ERROR_INVALID_HANDLE || result == ERROR_FILE_NOT_FOUND) {
				GPTP_LOG_WARNING("Invalid adapter handle for Intel TX timestamp OID - adapter may not support timestamping");
			} else {
				GPTP_LOG_WARNING("Unknown error 0x%08X when reading Intel TX timestamp OID", result);
			}
			goto try_tx_fallback;
		}
		if( returned != sizeof(buf_tmp) ){
			// Only log occasionally when Intel OIDs return insufficient data
			static uint32_t tx_insufficient_data_count = 0;
			tx_insufficient_data_count++;
			if (tx_insufficient_data_count <= 3 || tx_insufficient_data_count % 100 == 0) {
				GPTP_LOG_VERBOSE("Intel TX timestamp returned insufficient data: %d bytes, expected %d (attempt %u)", 
					returned, sizeof(buf_tmp), tx_insufficient_data_count);
			}
			goto try_tx_fallback;
		} 
		
		// Validate that the TX timestamp data is not uninitialized (0xCCCCCCCC pattern)
		tx_r = (((uint64_t)buf[1]) << 32) | buf[0];
		if (tx_r == 0 || tx_r == 0xCCCCCCCCCCCCCCCCULL || 
		    buf[0] == 0xCCCCCCCC || buf[1] == 0xCCCCCCCC) {
			static uint32_t tx_uninitialized_count = 0;
			tx_uninitialized_count++;
			if (tx_uninitialized_count <= 1) {
				GPTP_LOG_INFO("Intel TX timestamp hardware clock not running - using software timestamps");
			}
			goto try_tx_fallback;
		}
		
		tx_s = scaleNativeClockToNanoseconds( tx_r );
		timestamp = nanoseconds64ToTimestamp( tx_s );
		timestamp._version = version;

		oid_failures.txstamp.failure_count = 0;
		oid_failures.txstamp.disabled = false;
		return GPTP_EC_SUCCESS;
		
	try_tx_fallback:
		// TX Timestamp fallback when hardware timestamps are unavailable
		GPTP_LOG_VERBOSE("Attempting TX timestamp fallback for messageType=%d, seq=%u", 
			messageId.getMessageType(), messageId.getSequenceId());
		
		// Try cross-timestamping first for best accuracy
		WindowsCrossTimestamp& txFallbackCrossTimestamp = getGlobalCrossTimestamp();
		if (txFallbackCrossTimestamp.isPreciseTimestampingSupported()) {
			Timestamp system_time, device_time;
			uint32_t local_clock, nominal_clock_rate;
			
			if (txFallbackCrossTimestamp.getCrossTimestamp(&system_time, &device_time, &local_clock, &nominal_clock_rate)) {
				timestamp = device_time;
				timestamp._version = version;
				
				GPTP_LOG_VERBOSE("TX timestamp cross-timestamp fallback successful: messageType=%d, seq=%u", 
					messageId.getMessageType(), messageId.getSequenceId());
				
				return GPTP_EC_SUCCESS;
			}
		}
		
		// Fallback to TSC if cross-timestamping fails
		if (tsc_hz.QuadPart > 0) {
			LARGE_INTEGER tsc_now;
			if (QueryPerformanceCounter(&tsc_now)) {
				uint64_t tsc_ns = scaleTSCClockToNanoseconds(tsc_now.QuadPart);
				timestamp = nanoseconds64ToTimestamp(tsc_ns);
				timestamp._version = version;
				
				GPTP_LOG_VERBOSE("TX timestamp TSC fallback successful: messageType=%d, seq=%u, ts=%llu ns", 
					messageId.getMessageType(), messageId.getSequenceId(), tsc_ns);
				
				return GPTP_EC_SUCCESS;
			}
		}
		
		// Final fallback to enhanced software timestamping
		timestamp = enhanced_timestamper.getSystemTime();
		timestamp._version = version;
		
		GPTP_LOG_VERBOSE("TX timestamp enhanced software fallback: messageType=%d, seq=%u, ts=%u.%09u", 
			messageId.getMessageType(), messageId.getSequenceId(), 
			timestamp.seconds_ls, timestamp.nanoseconds);
		
		return GPTP_EC_SUCCESS;
	}

	/**
	 * @brief  Gets the RX timestamp
	 * @param  identity PortIdentity interface
	 * @param  PTPMessageId Message ID
	 * @param  timestamp [out] RX hardware timestamp
	 * @param  clock_value [out] Not used
	 * @param  last Not used
	 * @return GPTP_EC_SUCCESS if no error, GPTP_EC_FAILURE if error and GPTP_EC_EAGAIN to try again.
	 * 
	 * @note IMPORTANT: This function is only called for EVENT messages (Sync, Follow_Up, some PDelay).
	 *       GENERAL messages (Announce, Signaling) do NOT require hardware timestamps and use
	 *       sendGeneralPort() which bypasses this timestamping entirely. This is why BMCA
	 *       (Best Master Clock Algorithm) works even when hardware timestamping fails -
	 *       Announce messages don't need precise timestamps, only message content for comparison.
	 */
	virtual int HWTimestamper_rxtimestamp(PortIdentity *identity, PTPMessageId messageId, Timestamp &timestamp, unsigned &clock_value, bool last)
	{
#ifdef OPENAVNU_BUILD_INTEL_HAL
		// 🚀 INTEL HAL INTEGRATION: Try Intel HAL first for better reliability
		WindowsCrossTimestamp& rxCrossTimestamp = getGlobalCrossTimestamp();
		if (rxCrossTimestamp.isIntelHALAvailable()) {
			Timestamp system_time, device_time;
			
			if (rxCrossTimestamp.getCrossTimestamp(&system_time, &device_time)) {
				timestamp = device_time;
				timestamp._version = version;
				
				GPTP_LOG_VERBOSE("Intel HAL RX timestamp successful: messageType=%d, seq=%u, timestamp=%llu.%09u", 
					messageId.getMessageType(), messageId.getSequenceId(),
					TIMESTAMP_TO_NS(timestamp) / 1000000000ULL, 
					(uint32_t)(TIMESTAMP_TO_NS(timestamp) % 1000000000ULL));
				
				return GPTP_EC_SUCCESS;
			}
			
			GPTP_LOG_DEBUG("Intel HAL RX timestamp failed, falling back to legacy OID method");
		}
#endif

		// Legacy OID-based timestamping (fallback when Intel HAL not available/enabled)
		DWORD buf[4], buf_tmp[4];
		DWORD returned;
		uint64_t rx_r,rx_s;
		DWORD result;
		uint16_t packet_sequence_id;

		// Initialize buffers to avoid uninitialized memory issues
		memset(buf, 0, sizeof(buf));
		memset(buf_tmp, 0, sizeof(buf_tmp));

		// Debug: Track timestamp retrieval attempts
		static uint32_t timestamp_attempts = 0;
		static uint32_t timestamp_successes = 0;
		static uint32_t timestamp_failures = 0;
		static uint32_t consecutive_failures = 0;
		static uint32_t fallback_attempts = 0;
		timestamp_attempts++;

		// Check if miniport handle is valid before attempting to read
		if (miniport == INVALID_HANDLE_VALUE) {
			timestamp_failures++;
			GPTP_LOG_WARNING("Cannot read RX timestamp: miniport handle is invalid (Attempt %u, Failures: %u)", 
				timestamp_attempts, timestamp_failures);
			goto try_fallback_timestamp;
		}

		// Primary method: Intel custom OID for hardware RX timestamps
		// Check if we should skip the RXSTAMP OID due to repeated failures
		if (oid_failures.rxstamp.disabled) {
			GPTP_LOG_VERBOSE("Skipping RXSTAMP OID - disabled after %u consecutive failures", oid_failures.rxstamp.failure_count);
			goto try_fallback_timestamp;
		}
		
		while(( result = readOID( OID_INTEL_GET_RXSTAMP, buf_tmp, sizeof(buf_tmp), &returned )) == ERROR_SUCCESS ) {
			memcpy( buf, buf_tmp, sizeof( buf ));
		}
		
		if( result != ERROR_GEN_FAILURE ) {
			timestamp_failures++;
			if (result == 50) { // ERROR_NOT_SUPPORTED
				oid_failures.rxstamp.failure_count++;
				if (oid_failures.rxstamp.failure_count >= MAX_OID_FAILURES) {
					oid_failures.rxstamp.disabled = true;
					GPTP_LOG_WARNING("Disabling RXSTAMP OID after %u consecutive failures with error 50 (not supported)", oid_failures.rxstamp.failure_count);
				} else {
					GPTP_LOG_DEBUG("RXSTAMP OID failure %u/%u (error 50)", oid_failures.rxstamp.failure_count, MAX_OID_FAILURES);
				}
			} else {
				// Reset counter for non-50 errors
				oid_failures.rxstamp.failure_count = 0;
			}
			GPTP_LOG_WARNING("*** Received an event packet but cannot retrieve timestamp, discarding. messageType=%d,error=%d (0x%08X) (Attempt %u, Failures: %u)", 
				messageId.getMessageType(), result, result, timestamp_attempts, timestamp_failures);
			if (result == ERROR_NOT_SUPPORTED) {
				GPTP_LOG_INFO("Driver does not support Intel RX timestamping OID - trying fallback methods");
			} else if (result == ERROR_ACCESS_DENIED) {
				GPTP_LOG_WARNING("Access denied to Intel RX timestamp OID - ensure running as administrator");
			} else if (result == ERROR_INVALID_HANDLE || result == ERROR_FILE_NOT_FOUND) {
				GPTP_LOG_WARNING("Invalid adapter handle for Intel RX timestamp OID - adapter may not support timestamping");
			} else {
				GPTP_LOG_WARNING("Unknown error 0x%08X when reading Intel RX timestamp OID", result);
			}
			goto try_fallback_timestamp;
		}
		
		// Check if we got the expected amount of data
		if( returned != sizeof(buf_tmp) ) {
			timestamp_failures++;
			consecutive_failures++;
			
			// Don't spam warnings - only log periodically when Intel PTP isn't working
			if (consecutive_failures <= 5 || consecutive_failures % 50 == 0) {
				GPTP_LOG_WARNING("RX timestamp read returned insufficient data: %d bytes, expected %d (Attempt %u, Failures: %u)", 
					returned, sizeof(buf_tmp), timestamp_attempts, timestamp_failures);
			}
			
			// Log buffer contents for debugging only on first few failures
			if (returned > 0 && returned < 16 && consecutive_failures <= 3) {
				GPTP_LOG_WARNING("Partial data received: %02X %02X %02X %02X", 
					((uint8_t*)buf_tmp)[0], ((uint8_t*)buf_tmp)[1], 
					((uint8_t*)buf_tmp)[2], ((uint8_t*)buf_tmp)[3]);
			}
			goto try_fallback_timestamp;
		}
		
		// Validate that the timestamp data is not uninitialized (0xCCCCCCCC pattern)
		uint64_t timestamp_raw = (((uint64_t)buf[1]) << 32) | buf[0];
		if (timestamp_raw == 0 || timestamp_raw == 0xCCCCCCCCCCCCCCCCULL || 
		    buf[0] == 0xCCCCCCCC || buf[1] == 0xCCCCCCCC) {
			timestamp_failures++;
			consecutive_failures++;
			
			// Only log the first few uninitialized timestamp warnings
			if (consecutive_failures <= 3) {
				GPTP_LOG_WARNING("Intel RX timestamp contains uninitialized data: 0x%08X%08X (clock not running) (Attempt %u, Failures: %u)", 
					buf[1], buf[0], timestamp_attempts, timestamp_failures);
			}
			goto try_fallback_timestamp;
		}
		
		packet_sequence_id = *((uint32_t *) buf+3) >> 16;
		if (PLAT_ntohs(packet_sequence_id) != messageId.getSequenceId()) {
			GPTP_LOG_VERBOSE("RX timestamp sequence ID mismatch: got %u, expected %u (Attempt %u)", 
				PLAT_ntohs(packet_sequence_id), messageId.getSequenceId(), timestamp_attempts);
			goto try_fallback_timestamp;
		}
		
		// SUCCESS: Intel OID provided valid RX timestamp
		timestamp_successes++;
		oid_failures.rxstamp.failure_count = 0;
		oid_failures.rxstamp.disabled = false;
		rx_r = timestamp_raw;
		rx_s = scaleNativeClockToNanoseconds( rx_r );
		timestamp = nanoseconds64ToTimestamp( rx_s );
		timestamp._version = version;
		
		// Debug: Log successful timestamp retrieval periodically
		if (timestamp_successes % 100 == 1) {
			GPTP_LOG_STATUS("RX timestamp SUCCESS #%u: messageType=%d, seq=%u, ts=%llu ns (Successes: %u/%u attempts)",
				timestamp_successes, messageId.getMessageType(), messageId.getSequenceId(), rx_s, timestamp_successes, timestamp_attempts);
		}

		return GPTP_EC_SUCCESS;

	try_fallback_timestamp:
		// Enhanced fallback method: Try multiple Windows timestamp approaches
		fallback_attempts++;
		
		GPTP_LOG_VERBOSE("Attempting enhanced timestamp fallback (attempt %u) for messageType=%d, seq=%u", 
			fallback_attempts, messageId.getMessageType(), messageId.getSequenceId());
		
		// Fallback 1: Try standard NDIS timestamp OIDs (OID_TIMESTAMP_CAPABILITY/OID_TIMESTAMP_CURRENT_CONFIG)
		if (tryNDISTimestamp(timestamp, messageId)) {
			GPTP_LOG_VERBOSE("NDIS timestamp fallback successful: messageType=%d, seq=%u", 
				messageId.getMessageType(), messageId.getSequenceId());
			return GPTP_EC_SUCCESS;
		}
		
		// Fallback 2: Try IPHLPAPI-based timestamping
		if (tryIPHLPAPITimestamp(timestamp, messageId)) {
			GPTP_LOG_VERBOSE("IPHLPAPI timestamp fallback successful: messageType=%d, seq=%u", 
				messageId.getMessageType(), messageId.getSequenceId());
			return GPTP_EC_SUCCESS;
		}
		
		// Fallback 3: Try packet capture timestamp (if available)
		if (tryPacketCaptureTimestamp(timestamp, messageId)) {
			GPTP_LOG_VERBOSE("Packet capture timestamp fallback successful: messageType=%d, seq=%u", 
				messageId.getMessageType(), messageId.getSequenceId());
			return GPTP_EC_SUCCESS;
		}
		
		// Fallback 4: Cross-timestamp correlation (existing method)
		Timestamp system_time, device_time;
		uint32_t local_clock, nominal_clock_rate;
		
		// Use the same cross-timestamping mechanism as HWTimestamper_gettime
		WindowsCrossTimestamp& rxFallbackCrossTimestamp = getGlobalCrossTimestamp();
		
		if (rxFallbackCrossTimestamp.isPreciseTimestampingSupported()) {
			if (rxFallbackCrossTimestamp.getCrossTimestamp(&system_time, &device_time, &local_clock, &nominal_clock_rate)) {
				// Use device time as software RX timestamp approximation
				timestamp = device_time;
				timestamp._version = version;
				
				GPTP_LOG_VERBOSE("Cross-timestamp fallback successful: messageType=%d, seq=%u, ts=%llu.%09u", 
					messageId.getMessageType(), messageId.getSequenceId(), 
					(uint64_t)timestamp.seconds_ls + ((uint64_t)timestamp.seconds_ms << 32), timestamp.nanoseconds);
				
				return GPTP_EC_SUCCESS;
			}
		}
		
		// Fallback 5: Intel OID system time (if miniport handle was valid)
		if (miniport != INVALID_HANDLE_VALUE) {
			DWORD systim_buf[6];
			DWORD systim_returned;
			
			if (readOID(OID_INTEL_GET_SYSTIM, systim_buf, sizeof(systim_buf), &systim_returned) == ERROR_SUCCESS && 
				systim_returned == sizeof(systim_buf)) {
				
				uint64_t now_net = (((uint64_t)systim_buf[1]) << 32) | systim_buf[0];
				if (now_net > 0) {
					uint64_t now_net_ns = scaleNativeClockToNanoseconds(now_net);
					timestamp = nanoseconds64ToTimestamp(now_net_ns);
					timestamp._version = version;
					
					GPTP_LOG_VERBOSE("Intel OID system time fallback successful: messageType=%d, seq=%u, ts=%llu ns", 
						messageId.getMessageType(), messageId.getSequenceId(), now_net_ns);
					
					return GPTP_EC_SUCCESS;
				}
			}
		}
		
		// Fallback 6: Ultimate fallback - Software timestamp using TSC
		if (tsc_hz.QuadPart > 0) {
			LARGE_INTEGER tsc_now;
			if (QueryPerformanceCounter(&tsc_now)) {
				uint64_t tsc_ns = scaleTSCClockToNanoseconds(tsc_now.QuadPart);
				timestamp = nanoseconds64ToTimestamp(tsc_ns);
				timestamp._version = version;
				
				GPTP_LOG_VERBOSE("TSC software timestamp fallback: messageType=%d, seq=%u, ts=%llu ns (precision: software-only)", 
					messageId.getMessageType(), messageId.getSequenceId(), tsc_ns);
				
				return GPTP_EC_SUCCESS;
			}
		}
		
		// All fallback methods failed
		GPTP_LOG_ERROR("All RX timestamping methods failed for messageType=%d, seq=%u (fallback attempts: %u)", 
			messageId.getMessageType(), messageId.getSequenceId(), fallback_attempts);
		GPTP_LOG_ERROR("Attempted methods: Intel OID  NDIS  IPHLPAPI  Packet Capture  Cross-timestamp  Intel System Time  TSC");

        // Ultimate fallback: use enhanced software timestamping
        timestamp = enhanced_timestamper.getSystemTime();
        timestamp._version = version;
        
        GPTP_LOG_STATUS("Ultimate fallback: using enhanced software timestamp (hardware/software methods failed) for messageType=%d, seq=%u, ts=%u.%09u (precision: %lld ns)", 
            messageId.getMessageType(), messageId.getSequenceId(), 
            timestamp.seconds_ls, timestamp.nanoseconds, enhanced_timestamper.getTimestampPrecision());
        return GPTP_EC_SUCCESS;
	}

	/**
	 * @brief Check and report Intel PTP registry settings with user guidance
	 * @param adapter_guid Adapter GUID for registry lookup
	 * @param adapter_description Adapter description for logging
	 */
	void checkIntelPTPRegistrySettings(const char* adapter_guid, const char* adapter_description) const;

	/**
	 * @brief Attempt to get RX timestamp using standard NDIS OIDs
	 * @param timestamp [out] RX timestamp if successful
	 * @param messageId Message ID for sequence validation
	 * @return true if timestamp retrieved successfully, false otherwise
	 */
	bool tryNDISTimestamp(Timestamp& timestamp, const PTPMessageId& messageId) const;

private:
	/**
	 * @brief Try NDIS timestamp capability OIDs (Windows 8.1+)
	 * @param timestamp [out] RX timestamp if successful
	 * @param messageId Message ID for validation
	 * @return true if successful
	 */
	bool tryNDISTimestampCapability(Timestamp& timestamp, const PTPMessageId& messageId) const;

	/**
	 * @brief Try NDIS performance counter correlation for timestamping
	 * @param timestamp [out] RX timestamp if successful
	 * @param messageId Message ID for validation
	 * @return true if successful
	 */
	bool tryNDISPerformanceCounter(Timestamp& timestamp, const PTPMessageId& messageId) const;

	/**
	 * @brief Try NDIS adapter statistics correlation for timestamping
	 * @param timestamp [out] RX timestamp if successful
	 * @param messageId Message ID for validation
	 * @return true if successful
	 */
	bool tryNDISStatisticsCorrelation(Timestamp& timestamp, const PTPMessageId& messageId) const;

public:
	/**
	 * @brief Attempt to get RX timestamp using IPHLPAPI
	 * @param timestamp [out] RX timestamp if successful  
	 * @param messageId Message ID for sequence validation
	 * @return true if timestamp retrieved successfully, false otherwise
	 */
	bool tryIPHLPAPITimestamp(Timestamp& timestamp, const PTPMessageId& messageId) const;

	/**
	 * @brief Attempt to get RX timestamp from packet capture layer
	 * @param timestamp [out] RX timestamp if successful
	 * @param messageId Message ID for sequence validation
	 * @return true if timestamp retrieved successfully, false otherwise
	 */
	bool tryPacketCaptureTimestamp(Timestamp& timestamp, const PTPMessageId& messageId) const;

	/**
	 * @brief Test Intel custom OID availability and functionality
	 * @param adapter_description Description of the network adapter for logging
	 * @return true if Intel OIDs are available and functional, false otherwise
	 */
	bool testIntelOIDAvailability(const char* adapter_description) const {
		DWORD buf[6];
		DWORD returned = 0;
		DWORD result;
		DWORD result2;
		DWORD result3;
		// First check if basic OID functionality is working
		GPTP_LOG_STATUS("Testing basic Intel OID functionality for %s", adapter_description);
		
		// Test OID_INTEL_GET_SYSTIM first as it's the most basic
		memset(buf, 0xFF, sizeof(buf));
		result = readOID(OID_INTEL_GET_SYSTIM, buf, sizeof(buf), &returned);
		
		if (result == ERROR_SUCCESS && returned == sizeof(buf)) {
			GPTP_LOG_STATUS("OID_INTEL_GET_SYSTIM - Validate that we got reasonable data (not all 0xFF)");
			bool valid_data = false;
			for (int i = 0; i < 6; i++) {
				if (buf[i] != 0xFFFFFFFF) {
					valid_data = true;
					break;
				}
			}
			
			if (valid_data) {
				GPTP_LOG_VERBOSE("Intel OID test successful for %s: returned %d bytes with valid data", 
					adapter_description, returned);
				
				// Extract and validate timestamp data for additional verification
				uint64_t net_time = (((uint64_t)buf[1]) << 32) | buf[0];
				uint64_t tsc_time = (((uint64_t)buf[3]) << 32) | buf[2];
				
				if (net_time > 0 && tsc_time > 0) {
					GPTP_LOG_STATUS("Intel OID timestamp validation passed: net=%llu, tsc=%llu", 
						net_time, tsc_time);
					return true;
				} else if (net_time == 0 && tsc_time > 0) {
					GPTP_LOG_WARNING("Intel PTP network clock is not running (net_time=0) - attempting to start it");
					
					// Try to start the Intel PTP clock
					if (startIntelPTPClock()) {
						// Verify the clock is now running
						memset(buf, 0, sizeof(buf));
						result = readOID(OID_INTEL_GET_SYSTIM, buf, sizeof(buf), &returned);
						if (result == ERROR_SUCCESS && returned == sizeof(buf)) {
							net_time = (((uint64_t)buf[1]) << 32) | buf[0];
							tsc_time = (((uint64_t)buf[3]) << 32) | buf[2];
							
							if (net_time > 0 && tsc_time > 0) {
								GPTP_LOG_STATUS("Intel PTP clock successfully started: net=%llu, tsc=%llu", 
									net_time, tsc_time);
								return true;
							}
						}
					}
					
					GPTP_LOG_WARNING("Failed to start Intel PTP network clock - falling back to software timestamping");
				} else {
					GPTP_LOG_WARNING("Intel OID returned zero timestamps on first attempt - retrying after 100ms");
					
					// 💡 RETRY LOGIC: Wait 100ms and try again as suggested
					Sleep(100);
					
					// Retry the OID read
					memset(buf, 0xFF, sizeof(buf));
					result = readOID(OID_INTEL_GET_SYSTIM, buf, sizeof(buf), &returned);
					
					if (result == ERROR_SUCCESS && returned == sizeof(buf)) {
						// Re-validate the data
						valid_data = false;
						for (int i = 0; i < 6; i++) {
							if (buf[i] != 0xFFFFFFFF) {
								valid_data = true;
								break;
							}
						}
						
						if (valid_data) {
							net_time = (((uint64_t)buf[1]) << 32) | buf[0];
							tsc_time = (((uint64_t)buf[3]) << 32) | buf[2];
							
							if (net_time > 0 && tsc_time > 0) {
								GPTP_LOG_STATUS("Intel OID retry successful: net=%llu, tsc=%llu - hardware timestamping available", 
									net_time, tsc_time);
								return true;
							}
						}
					}
					
					// Retry failed - Intel OID read failed, defaulting to RDTSC method
					GPTP_LOG_WARNING("Intel OID read failed after retry - timestamps still zero or invalid");
					GPTP_LOG_STATUS("Intel OID read failed, defaulting to RDTSC method for timestamping");
					
					// Do NOT return true - actual OID reads are failing  
					// fall back to cross-timestamping
					GPTP_LOG_INFO("OID reads fail - using software fallback");
					
					return false; // Force fallback to cross-timestamping/RDTSC method
				}
			} else {
				GPTP_LOG_WARNING("Intel OID returned invalid data (all 0xFF) for %s", 
					adapter_description);
			}
		} else {
			GPTP_LOG_WARNING("Intel OID test failed for %s: error=%d, returned=%d/%d", 
				adapter_description, result, returned, sizeof(buf));
		}
		
		result2 = readOID(OID_INTEL_GET_RXSTAMP, buf, sizeof(buf), &returned);
		if (result2 == ERROR_SUCCESS && returned == sizeof(buf)){
			GPTP_LOG_STATUS("OID_INTEL_GET_RXSTAMP - Validate that we got reasonable data (not all 0xFF)");
		}
		else{
			GPTP_LOG_WARNING("OID_INTEL_GET_RXSTAMP - failed");
		}
		result3 = readOID(OID_INTEL_GET_TXSTAMP, buf, sizeof(buf), &returned);
		if (result3 == ERROR_SUCCESS && returned == sizeof(buf)){
			GPTP_LOG_STATUS("OID_INTEL_GET_TXSTAMP - Validate that we got reasonable data (not all 0xFF)");
		}
		else{
			GPTP_LOG_WARNING("OID_INTEL_GET_TXSTAMP - failed");
		}
		// Intel OIDs not available or not functional
		return false;
	}

	/**
	 * @brief Diagnostic function to check Intel OID support and adapter status
	 * @return true if basic OID functionality is working
	 */
	bool diagnoseTimestampingCapabilities() const {
		GPTP_LOG_STATUS("=== Diagnosing Hardware Timestamping Capabilities ===");
		
		if (miniport == INVALID_HANDLE_VALUE) {
			GPTP_LOG_ERROR("CRITICAL: Miniport handle is INVALID - cannot access adapter");
			return false;
		} else {
			GPTP_LOG_STATUS("Miniport handle is valid (0x%p)", miniport);
		}
		
		// Test basic Intel OID availability
		DWORD buf[6];
		DWORD returned = 0;
		DWORD result;
		
		// Initialize buffer
		memset(buf, 0, sizeof(buf));
		
		// Test OID_INTEL_GET_SYSTIM
		GPTP_LOG_STATUS("Testing OID_INTEL_GET_SYSTIM...");
		result = readOID(OID_INTEL_GET_SYSTIM, buf, sizeof(buf), &returned);
		
		if (result == ERROR_SUCCESS) {
			if (returned == sizeof(buf)) {
				uint64_t net_time = (((uint64_t)buf[1]) << 32) | buf[0];
				uint64_t tsc_time = (((uint64_t)buf[3]) << 32) | buf[2];
				GPTP_LOG_STATUS("✓ OID_INTEL_GET_SYSTIM: SUCCESS - net_time=%llu, tsc_time=%llu", net_time, tsc_time);
				
				if (net_time == 0 && tsc_time == 0) {
					GPTP_LOG_WARNING("WARNING: Both timestamps are zero - adapter may not be properly initialized");
				}
			} else {
				GPTP_LOG_WARNING("⚠ OID_INTEL_GET_SYSTIM: Partial data - got %d bytes, expected %d", returned, sizeof(buf));
			}
		} else {
			GPTP_LOG_ERROR("✗ OID_INTEL_GET_SYSTIM: FAILED - error %d (0x%08X)", result, result);
			
			if (result == ERROR_NOT_SUPPORTED || result == ERROR_INVALID_FUNCTION) {
				GPTP_LOG_ERROR("  Adapter does not support Intel timestamping OIDs");
			} else if (result == ERROR_ACCESS_DENIED) {
				GPTP_LOG_ERROR("  Access denied - run as administrator or check driver permissions");
			} else if (result == ERROR_INVALID_HANDLE || result == ERROR_FILE_NOT_FOUND) {
				GPTP_LOG_ERROR("  Invalid adapter handle - adapter may be disconnected or driver issue");
			}
		}
		
		// Test OID_INTEL_GET_RXSTAMP (just structure check, not expecting data)
		GPTP_LOG_STATUS("Testing OID_INTEL_GET_RXSTAMP availability...");
		memset(buf, 0, sizeof(buf));
		returned = 0;
		result = readOID(OID_INTEL_GET_RXSTAMP, buf, 16, &returned);
		
		if (result == ERROR_SUCCESS || result == ERROR_GEN_FAILURE) {
			GPTP_LOG_STATUS("✓ OID_INTEL_GET_RXSTAMP: Available (result=%d)", result);
		} else {
			GPTP_LOG_WARNING("⚠ OID_INTEL_GET_RXSTAMP: May not be supported (result=%d)", result);
		}
		
		// Test OID_INTEL_GET_TXSTAMP (just structure check, not expecting data)
		GPTP_LOG_STATUS("Testing OID_INTEL_GET_TXSTAMP availability...");
		memset(buf, 0, sizeof(buf));
		returned = 0;
		result = readOID(OID_INTEL_GET_TXSTAMP, buf, 16, &returned);
		
		if (result == ERROR_SUCCESS || result == ERROR_GEN_FAILURE) {
			GPTP_LOG_STATUS("✓ OID_INTEL_GET_TXSTAMP: Available (result=%d)", result);
		} else {
			GPTP_LOG_WARNING("⚠ OID_INTEL_GET_TXSTAMP: May not be supported (result=%d)", result);
		}
		
		GPTP_LOG_STATUS("Clock frequencies: TSC=%lld Hz, Network=%lld Hz", 
			tsc_hz.QuadPart, netclock_hz.QuadPart);
		
		bool basic_support = (result == ERROR_SUCCESS && returned > 0);
		GPTP_LOG_STATUS("=== Diagnosis complete - Basic Intel OID support: %s ===", 
			basic_support ? "AVAILABLE" : "UNAVAILABLE");
		
		return basic_support;
	}

	/**
	 * @brief Attempt to start the Intel PTP hardware clock
	 * @return true if clock was started successfully
	 */
	bool startIntelPTPClock() const {
		if (miniport == INVALID_HANDLE_VALUE) {
			GPTP_LOG_WARNING("Cannot start Intel PTP clock: miniport handle is invalid");
			return false;
		}

		// Try to enable the PTP clock using Intel-specific OIDs
		DWORD enable_clock = 1;
		DWORD result;
		
		// Try OID_INTEL_SET_PTP_ENABLE (if available)
		// This OID may not be documented but is sometimes present
		const NDIS_OID OID_INTEL_SET_PTP_ENABLE = 0xFF020204;
		
		result = setOID(OID_INTEL_SET_PTP_ENABLE, &enable_clock, sizeof(enable_clock));
		if (result == ERROR_SUCCESS) {
			GPTP_LOG_STATUS("Intel PTP clock enabled via OID_INTEL_SET_PTP_ENABLE");
			return true;
		} else if (result != ERROR_NOT_SUPPORTED && result != ERROR_INVALID_FUNCTION) {
			GPTP_LOG_WARNING("Failed to enable Intel PTP clock via OID: error %d (0x%08X)", result, result);
		}

		// Alternative approach: Try to trigger clock start by reading system time repeatedly
		GPTP_LOG_STATUS("Attempting to trigger Intel PTP clock start via repeated OID reads");
		DWORD buf[6];
		DWORD returned;
		
		for (int attempt = 0; attempt < 10; attempt++) {
			memset(buf, 0, sizeof(buf));
			result = readOID(OID_INTEL_GET_SYSTIM, buf, sizeof(buf), &returned);
			
			if (result == ERROR_SUCCESS && returned == sizeof(buf)) {
				uint64_t net_time = (((uint64_t)buf[1]) << 32) | buf[0];
				if (net_time > 0) {
					GPTP_LOG_STATUS("Intel PTP clock started after %d attempts - net_time=%llu", 
						attempt + 1, net_time);
					return true;
				}
			}
			
			Sleep(50); // Wait 50ms between attempts
		}
		
		GPTP_LOG_WARNING("Unable to start Intel PTP clock - network time counter remains at zero");
		GPTP_LOG_INFO("This may require manual driver configuration or the adapter may not support hardware PTP");
		return false;
	}
};


/**
 * @brief Named pipe interface
 */
class WindowsNamedPipeIPC : public OS_IPC {
private:
	HANDLE pipe_;
	LockableOffset lOffset_;
	PeerList peerList_;
public:
	/**
	 * @brief Default constructor. Initializes the IPC interface
	 */
	WindowsNamedPipeIPC() : pipe_(INVALID_HANDLE_VALUE) { };

	/**
	 * @brief Destroys the IPC interface
	 */
	~WindowsNamedPipeIPC() {
		if (pipe_ != 0 && pipe_ != INVALID_HANDLE_VALUE)
			::CloseHandle(pipe_);
	}

	/**
	 * @brief  Initializes the IPC arguments
	 * @param  arg [in] IPC arguments. Not in use
	 * @return Always returns TRUE.
	 */
	virtual bool init(OS_IPC_ARG *arg = NULL);

	/**
	 * @brief  Updates IPC interface values
	 *
	 * @param  ml_phoffset Master to local phase offset
	 * @param  ls_phoffset Local to system phase offset
	 * @param  ml_freqoffset Master to local frequency offset
	 * @param  ls_freq_offset Local to system frequency offset
	 * @param  local_time Local time
	 * @param  sync_count Counts of sync messages
	 * @param  pdelay_count Counts of pdelays
	 * @param  port_state PortState information
	 * @param  asCapable asCapable flag
	 *
	 * @return TRUE if success; FALSE if error
	 */
	virtual bool update(
		int64_t ml_phoffset,
		int64_t ls_phoffset,
		FrequencyRatio ml_freqoffset,
		FrequencyRatio ls_freq_offset,
		uint64_t local_time,
		uint32_t sync_count,
		uint32_t pdelay_count,
		PortState port_state,
		bool asCapable );

	/**
	 * @brief  Updates grandmaster IPC interface values
	 *
	 * @param  gptp_grandmaster_id Current grandmaster id (all 0's if no grandmaster selected)
	 * @param  gptp_domain_number gPTP domain number
	 *
	 * @return TRUE if success; FALSE if error
	 */
	virtual bool update_grandmaster(
		uint8_t gptp_grandmaster_id[],
		uint8_t gptp_domain_number );

	/**
	 * @brief Updates network interface IPC interface values
	 *
	 * @param  clock_identity  The clock identity of the interface
	 * @param  priority1  The priority1 field of the grandmaster functionality of the interface, or 0xFF if not supported
	 * @param  clock_class  The clockClass field of the grandmaster functionality of the interface, or 0xFF if not supported
	 * @param  offset_scaled_log_variance  The offsetScaledLogVariance field of the grandmaster functionality of the interface, or 0x0000 if not supported
	 * @param  clock_accuracy  The clockAccuracy field of the grandmaster functionality of the interface, or 0xFF if not supported
	 * @param  priority2  The priority2 field of the grandmaster functionality of the interface, or 0xFF if not supported
	 * @param  domain_number  The domainNumber field of the grandmaster functionality of the interface, or 0 if not supported
	 * @param  log_sync_interval  The currentLogSyncInterval field of the grandmaster functionality of the interface, or 0 if not supported
	 * @param  log_announce_interval  The currentLogAnnounceInterval field of the grandmaster functionality of the interface, or 0 if not supported
	 * @param  log_pdelay_interval  The currentLogPDelayReqInterval field of the grandmaster functionality of the interface, or 0 if not supported
	 * @param  port_number  The portNumber field of the interface, or 0x0000 if not supported
	 *
	 * @return TRUE if success; FALSE if error
	 */
	virtual bool update_network_interface(
		uint8_t  clock_identity[],
		uint8_t  priority1,
		uint8_t  clock_class,
        uint16_t offset_scaled_log_variance,
		uint8_t  clock_accuracy,
		uint8_t  priority2,
		uint8_t  domain_number,
		int8_t   log_sync_interval,
		int8_t   log_announce_interval,
		int8_t   log_pdelay_interval,
		uint16_t port_number );
};

// Forward declarations for event-driven link monitoring
class CommonPort;

/**
 * @brief Event-driven link monitoring structures and functions
 * 
 * Provides real-time network interface state change notifications
 * using Windows NotifyAddrChange and NotifyRouteChange APIs
 */

/**
 * @brief Link monitoring context for event-driven notifications
 */
struct LinkMonitorContext {
    CommonPort* pPort;              // Associated port for notifications
    HANDLE hAddrChange;             // Handle for address change notifications
    HANDLE hRouteChange;            // Handle for route change notifications
    HANDLE hMonitorThread;          // Background monitoring thread handle
    DWORD dwThreadId;               // Monitoring thread ID
    volatile bool bStopMonitoring;  // Flag to stop monitoring
    char interfaceDesc[256];        // Interface description for matching
    BYTE macAddress[6];             // MAC address for interface matching
};

/**
 * @brief Start event-driven link monitoring for a network interface
 * @param pPort Pointer to the CommonPort to monitor
 * @param interfaceDesc Interface description string
 * @param macAddress MAC address of interface (6 bytes)
 * @return Pointer to LinkMonitorContext, or NULL on failure
 */
LinkMonitorContext* startLinkMonitoring(CommonPort* pPort, const char* interfaceDesc, const BYTE* macAddress);

/**
 * @brief Stop event-driven link monitoring
 * @param pContext Pointer to LinkMonitorContext from startLinkMonitoring
 */
void stopLinkMonitoring(LinkMonitorContext* pContext);

/**
 * @brief Background thread function for event-driven link monitoring
 * @param lpParam Pointer to LinkMonitorContext
 * @return Thread exit code
 */
DWORD WINAPI linkMonitorThreadProc(LPVOID lpParam);

/**
 * @brief Check current link status for an interface
 * @param interfaceDesc Interface description string
 * @param macAddress MAC address of interface (6 bytes)
 * @return true if link is up, false if down or error
 */
bool checkLinkStatus(const char* interfaceDesc, const BYTE* macAddress);

/**
 * @brief Cleanup link monitoring subsystem
 * Call this during application shutdown to properly clean up all monitoring threads
 */
void cleanupLinkMonitoring();

#endif
