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


#ifndef COMMON_PORT_HPP
#define COMMON_PORT_HPP

#include <avbts_message.hpp>
#include <avbts_osthread.hpp>
#include <avbts_oscondition.hpp>
#include <avbts_ostimer.hpp>
#include <avbts_oslock.hpp>
#include <avbts_osnet.hpp>
#include <unordered_map>
#include <gptp_profile.hpp>  // Unified gPTP profile support

#include <math.h>

#define SYNC_RECEIPT_TIMEOUT_MULTIPLIER 3 /*!< Sync rcpt timeout multiplier */
#define ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER 3 /*!< Annc rcpt timeout mult */
#define LOG2_INTERVAL_INVALID -127 /* Invalid Log base 2 interval value */

class IEEE1588Clock;
class MilanProfile;  // Forward declaration for Milan B.1 profile

/**
 * @brief PortIdentity interface
 * Defined at IEEE 802.1AS Clause 8.5.2
 */
class PortIdentity {
private:
	ClockIdentity clock_id;
	uint16_t portNumber;
public:
	/**
	 * @brief Default Constructor
	 */
	PortIdentity() { };

	/**
	 * @brief  Constructs PortIdentity interface.
	 * @param  clock_id Clock ID value as defined at IEEE 802.1AS Clause
	 * 8.5.2.2
	 * @param  portNumber Port Number
	 */
	PortIdentity(uint8_t * clock_id, uint16_t * portNumber)
	{
		this->portNumber = *portNumber;
		this->portNumber = PLAT_ntohs(this->portNumber);
		this->clock_id.set(clock_id);
	}

	/**
	 * @brief  Implements the operator '!=' overloading method. Compares
	 * clock_id and portNumber.
	 * @param  cmp Constant PortIdentity value to be compared against.
	 * @return TRUE if the comparison value differs from the object's
	 * PortIdentity value. FALSE otherwise.
	 */
	bool operator!=(const PortIdentity & cmp) const
	{
		return
			!(this->clock_id == cmp.clock_id) ||
			this->portNumber != cmp.portNumber ? true : false;
	}

	/**
	 * @brief  Implements the operator '==' overloading method. Compares
	 * clock_id and portNumber.
	 * @param  cmp Constant PortIdentity value to be compared against.
	 * @return TRUE if the comparison value equals to the object's
	 * PortIdentity value. FALSE otherwise.
	 */
	bool operator==(const PortIdentity & cmp)const
	{
		return
			this->clock_id == cmp.clock_id &&
			this->portNumber == cmp.portNumber ? true : false;
	}

	/**
	 * @brief  Implements the operator '<' overloading method. Compares
	 * clock_id and portNumber.
	 * @param  cmp Constant PortIdentity value to be compared against.
	 * @return TRUE if the comparison value is lower than the object's
	 * PortIdentity value. FALSE otherwise.
	 */
	bool operator<(const PortIdentity & cmp)const
	{
		return
			this->clock_id < cmp.clock_id ?
			true : this->clock_id == cmp.clock_id &&
			this->portNumber < cmp.portNumber ? true : false;
	}

	/**
	 * @brief  Implements the operator '>' overloading method. Compares
	 * clock_id and portNumber.
	 * @param  cmp Constant PortIdentity value to be compared against.
	 * @return TRUE if the comparison value is greater than the object's
	 * PortIdentity value. FALSE otherwise.
	 */
	bool operator>(const PortIdentity & cmp)const
	{
		return
			this->clock_id > cmp.clock_id ?
			true : this->clock_id == cmp.clock_id &&
			this->portNumber > cmp.portNumber ? true : false;
	}

	/**
	 * @brief  Gets the ClockIdentity string
	 * @param  id [out] Pointer to an array of octets.
	 * @return void
	 */
	void getClockIdentityString(uint8_t *id)
	{
		clock_id.getIdentityString(id);
	}

	/**
	 * @brief  Sets the ClockIdentity.
	 * @param  clock_id Clock Identity to be set.
	 * @return void
	 */
	void setClockIdentity(ClockIdentity clock_id)
	{
		this->clock_id = clock_id;
	}

	/**
	 * @brief  Gets the clockIdentity value
	 * @return A copy of Clock identity value.
	 */
	ClockIdentity getClockIdentity( void ) {
		return this->clock_id;
	}

	/**
	 * @brief  Gets the port number following the network byte order, i.e.
	 * Big-Endian.
	 * @param  id [out] Port number
	 * @return void
	 */
	void getPortNumberNO(uint16_t * id)	// Network byte order
	{
		uint16_t portNumberNO = PLAT_htons(portNumber);
		*id = portNumberNO;
	}

	/**
	 * @brief  Gets the port number in the host byte order, which can be
	 * either Big-Endian
	 * or Little-Endian, depending on the processor where it is running.
	 * @param  id Port number
	 * @return void
	 */
	void getPortNumber(uint16_t * id)	// Host byte order
	{
		*id = portNumber;
	}

	/**
	 * @brief  Sets the Port number
	 * @param  id [in] Port number
	 * @return void
	 */
	void setPortNumber(uint16_t * id)
	{
		portNumber = *id;
	}
};

/**
 * @brief Physical delay specification for different link speeds
 */
class phy_delay_spec_t {
public:
	phy_delay_spec_t() : tx_delay(0), rx_delay(0) {}
	phy_delay_spec_t(uint64_t tx, uint64_t rx) : tx_delay(tx), rx_delay(rx) {}
	
	uint64_t get_tx_delay() const { return tx_delay; }
	uint64_t get_rx_delay() const { return rx_delay; }
	void set_tx_delay(uint64_t delay) { tx_delay = delay; }
	void set_rx_delay(uint64_t delay) { rx_delay = delay; }
	void set_delay(uint64_t tx, uint64_t rx) { tx_delay = tx; rx_delay = rx; }

private:
	uint64_t tx_delay;  // TX delay in nanoseconds
	uint64_t rx_delay;  // RX delay in nanoseconds
};

typedef std::unordered_map<uint32_t, phy_delay_spec_t> phy_delay_map_t;



/**
 * @brief Structure for initializing the port class
 */
typedef struct {
	/* clock IEEE1588Clock instance */
	IEEE1588Clock * clock;

	/* index Interface index */
	uint16_t index;

	/* timestamper Hardware timestamper instance */
	CommonTimestamper * timestamper;

	/* net_label Network label */
	InterfaceLabel *net_label;

	/* Virtual Network label (e.g. WiFi Direct network MAC) */
	InterfaceLabel *virtual_label;

	/* Set to true if the port is the grandmaster. Used for fixed GM in
	 * the the AVnu automotive profile */
	bool isGM;

	/* Set to true if the port is the grandmaster. Used for fixed GM in
	 * the the AVnu automotive profile */
	bool testMode;

	/* Set to true if the port's network interface is up. Used to filter
	 * false LINKUP/LINKDOWN events */
	bool linkUp;

	/* gPTP 10.2.4.4 */
	signed char initialLogSyncInterval;

	/* gPTP 11.5.2.2 */
	signed char initialLogPdelayReqInterval;

	/* CDS 6.2.1.5 */
	signed char operLogPdelayReqInterval;

	/* CDS 6.2.1.6 */
	signed char operLogSyncInterval;

	/* condition_factory OSConditionFactory instance */
	OSConditionFactory * condition_factory;

	/* thread_factory OSThreadFactory instance */
	OSThreadFactory * thread_factory;

	/* timer_factory OSTimerFactory instance */
	OSTimerFactory * timer_factory;

	/* lock_factory OSLockFactory instance */
	OSLockFactory * lock_factory;

	/* phy delay */
	phy_delay_map_t const *phy_delay;

	/* sync receipt threshold */
	unsigned int syncReceiptThreshold;

	/* neighbor delay threshold */
	int64_t neighborPropDelayThreshold;

	/* Allow processing SyncFollowUp with
	 * negative correction field */
	bool allowNegativeCorrField;

	/* Unified gPTP Profile configuration - replaces individual profile flags */
	gPTPProfile profile;                   // Unified profile configuration
} PortInit_t;


/**
 * @brief Structure for Port Counters
 */
typedef struct {
	uint32_t ieee8021AsPortStatRxSyncCount;
	uint32_t ieee8021AsPortStatRxFollowUpCount;
	uint32_t ieee8021AsPortStatRxPdelayRequest;
	uint32_t ieee8021AsPortStatRxPdelayResponse;
	uint32_t ieee8021AsPortStatRxPdelayResponseFollowUp;
	uint32_t ieee8021AsPortStatRxAnnounce;
	uint32_t ieee8021AsPortStatRxPTPPacketDiscard;
	uint32_t ieee8021AsPortStatRxSyncReceiptTimeouts;
	uint32_t ieee8021AsPortStatAnnounceReceiptTimeouts;
	uint32_t ieee8021AsPortStatPdelayAllowedLostResponsesExceeded;
	uint32_t ieee8021AsPortStatTxSyncCount;
	uint32_t ieee8021AsPortStatTxFollowUpCount;
	uint32_t ieee8021AsPortStatTxPdelayRequest;
	uint32_t ieee8021AsPortStatTxPdelayResponse;
	uint32_t ieee8021AsPortStatTxPdelayResponseFollowUp;
	uint32_t ieee8021AsPortStatTxAnnounce;
} PortCounters_t;

/**
 * @brief Port functionality common to all network media
 */
class CommonPort
{
private:
	LinkLayerAddress local_addr;
	PortIdentity port_identity;

	/* Network socket description
	   physical interface number that object represents */
	uint16_t ifindex;

	/* Link speed in kb/sec */
	uint32_t link_speed;

	/* Signed value allows this to be negative result because of inaccurate
	   timestamp */
	int64_t one_way_delay;
	int64_t neighbor_prop_delay_thresh;

	InterfaceLabel *net_label;

	OSNetworkInterface *net_iface;

	PortState port_state;
	bool testMode;
	
	/* Unified gPTP Profile - replaces individual profile flags */
	gPTPProfile active_profile;            // Current active profile configuration
	
	bool allow_negative_correction_field;

	signed char log_mean_sync_interval;
	signed char log_mean_announce_interval;
	signed char initialLogSyncInterval;

	/*Sync threshold*/
	unsigned int sync_receipt_thresh;
	unsigned int wrongSeqIDCounter;

	PortCounters_t counters;

	OSThread *listening_thread;
	OSThread *link_thread;
	bool listening_thread_running;
	bool link_thread_running;

	FrequencyRatio _peer_rate_offset;
	Timestamp _peer_offset_ts_theirs;
	Timestamp _peer_offset_ts_mine;
	bool _peer_offset_init;
	bool asCapable;
	unsigned sync_count;  /* 0 for master, increment for each sync
			       * received as slave */
	unsigned pdelay_count;

	signed char initialLogPdelayReqInterval;
	signed char operLogPdelayReqInterval;
	signed char log_min_mean_pdelay_req_interval;

	PTPMessageAnnounce *qualified_announce;

	uint16_t announce_sequence_id;
	uint16_t signal_sequence_id;
	uint16_t sync_sequence_id;

	uint16_t lastGmTimeBaseIndicator;

	OSLock *syncReceiptTimerLock;
	OSLock *syncIntervalTimerLock;
	OSLock *announceIntervalTimerLock;

	// Milan profile: track late responses vs missing responses
	unsigned _consecutive_late_responses;	// Track consecutive late (but not missing) responses
	unsigned _consecutive_missing_responses;	// Track consecutive missing responses
	Timestamp _last_pdelay_req_timestamp;	// Track when last PDelay request was sent
	bool _pdelay_response_received;		// Track if response was received (late or on-time)

	// Milan B.1 Profile Integration
	MilanProfile* milan_profile;		// Milan profile instance for B.1 compliance features
	PortIdentity last_grandmaster_identity;	// Track grandmaster changes for B.1 holdover

protected:
	static const int64_t INVALID_LINKDELAY = 3600000000000;
	static const int64_t ONE_WAY_DELAY_DEFAULT = INVALID_LINKDELAY;

	OSThreadFactory const * const thread_factory;
	OSTimerFactory const * const timer_factory;
	OSLockFactory const * const lock_factory;
	OSConditionFactory const * const condition_factory;
	CommonTimestamper * const _hw_timestamper;
	IEEE1588Clock * const clock;
	const bool isGM;

	phy_delay_map_t const * const phy_delay;

public:
	static const int64_t NEIGHBOR_PROP_DELAY_THRESH = 800;
	static const unsigned int DEFAULT_SYNC_RECEIPT_THRESH = 5;

	CommonPort( PortInit_t *portInit );
	virtual ~CommonPort();

	/**
	 * @brief Global media dependent port initialization
	 * @return true on success
	 */
	bool init_port( void );

	/**
	 * @brief Media specific port initialization
	 * @return true on success
	 */
	virtual bool _init_port( void ) = 0;

	/**
	 * @brief Initializes the hwtimestamper
	 */
	void timestamper_init( void );

	/**
	 * @brief Resets the hwtimestamper
	 */
	void timestamper_reset( void );

	/**
	 * @brief  Gets the link delay information.
	 * @return one way delay if delay > 0, or zero otherwise.
	 */
	uint64_t getLinkDelay( void )
	{
		return one_way_delay > 0LL ? one_way_delay : 0LL;
	}

	/**
	 * @brief  Gets the link delay information.
	 * @param  [in] delay Pointer to the delay information
	 * @return True if valid, false if invalid
	 */
	bool getLinkDelay( uint64_t *delay )
	{
		if(delay == NULL) {
			return false;
		}
		*delay = getLinkDelay();
		return *delay < INVALID_LINKDELAY;
	}

	/**
	 * @brief  Sets link delay information.
	 * Signed value allows this to be negative result because
	 * of inaccurate timestamps.
	 * @param  delay Link delay
	 * @return True if one_way_delay is lower or equal than neighbor
	 * propagation delay threshold False otherwise
	 */
	bool setLinkDelay( int64_t delay )
	{
		one_way_delay = delay;
		int64_t abs_delay = (one_way_delay < 0 ?
				     -one_way_delay : one_way_delay);

		if (testMode) {
			GPTP_LOG_STATUS("Link delay: %d", delay);
		}

		return (abs_delay <= neighbor_prop_delay_thresh);
	}

	/**
	* @brief Return frequency offset between local timestamp clock
	*	system clock
	* @return local:system ratio
	*/
	FrequencyRatio getLocalSystemFreqOffset();

	/**
	 * @brief  Gets a pointer to IEEE1588Clock
	 * @return Pointer to clock
	 */
	IEEE1588Clock *getClock( void )
	{
		return clock;
	}

	/**
	 * @brief  Gets the local_addr
	 * @return LinkLayerAddress
	 */
	LinkLayerAddress *getLocalAddr( void )
	{
		return &local_addr;
	}

	/**
	 * @brief  Gets the testMode
	 * @return bool of the test mode value
	 */
	bool getTestMode( void )
	{
		return testMode;
	}

	/**
	 * @brief  Sets the testMode
	 * @param testMode changes testMode to this value
	 */
	void setTestMode( bool testMode )
	{
		this->testMode = testMode;
	}

	/**
	 * @brief  Serializes (i.e. copy over buf pointer) the information from
	 * the variables (in that order):
	 *  - asCapable;
	 *  - Port Sate;
	 *  - Link Delay;
	 *  - Neighbor Rate Ratio
	 * @param  buf [out] Buffer where to put the results.
	 * @param  count [inout] Length of buffer. It contains maximum length
	 * to be written
	 * when the function is called, and the value is decremented by the
	 * same amount the
	 * buf size increases.
	 * @return TRUE if it has successfully written to buf all the values
	 * or if buf is NULL.
	 * FALSE if count should be updated with the right size.
	 */
	bool serializeState( void *buf, long *count );

	/**
	 * @brief  Restores the serialized state from the buffer. Copies the
	 * information from buffer
	 * to the variables (in that order):
	 *  - asCapable;
	 *  - Port State;
	 *  - Link Delay;
	 *  - Neighbor Rate Ratio
	 * @param  buf Buffer containing the serialized state.
	 * @param  count Buffer lenght. It is decremented by the same size of
	 * the variables that are
	 * being copied.
	 * @return TRUE if everything was copied successfully, FALSE otherwise.
	 */
	bool restoreSerializedState( void *buf, long *count );

	/**
	 * @brief  Sets the internal variabl sync_receipt_thresh, which is the
	 * flag that monitors the amount of wrong syncs enabled before
	 * switching
	 * the ptp to master.
	 * @param  th Threshold to be set
	 * @return void
	 */
	void setSyncReceiptThresh(unsigned int th)
	{
		sync_receipt_thresh = th;
	}

	/**
	 * @brief  Gets the internal variabl sync_receipt_thresh, which is the
	 * flag that monitors the amount of wrong syncs enabled before
	 * switching
	 * the ptp to master.
	 * @return sync_receipt_thresh value
	 */
	unsigned int getSyncReceiptThresh( void )
	{
		return sync_receipt_thresh;
	}

	/**
	 * @brief  Sets the wrongSeqIDCounter variable
	 * @param  cnt Value to be set
	 * @return void
	 */
	void setWrongSeqIDCounter(unsigned int cnt)
	{
		wrongSeqIDCounter = cnt;
	}

	/**
	 * @brief  Gets the wrongSeqIDCounter value
	 * @param  [out] cnt Pointer to the counter value. It must be valid
	 * @return TRUE if ok and lower than the syncReceiptThreshold value.
	 * FALSE otherwise
	 */
	bool getWrongSeqIDCounter(unsigned int *cnt)
	{
		if( cnt == NULL )
		{
			return false;
		}
		*cnt = wrongSeqIDCounter;

		return( *cnt < getSyncReceiptThresh() );
	}

	/**
	 * @brief  Increments the wrongSeqIDCounter value
	 * @param  [out] cnt Pointer to the counter value. Must be valid
	 * @return TRUE if incremented value is lower than the
	 * syncReceiptThreshold. FALSE otherwise.
	 */
	bool incWrongSeqIDCounter(unsigned int *cnt)
	{
		if( getAsCapable() )
		{
			wrongSeqIDCounter++;
		}
		bool ret = wrongSeqIDCounter < getSyncReceiptThresh();

		if( cnt != NULL)
		{
			*cnt = wrongSeqIDCounter;
		}

		return ret;
	}

	/**
	 * @brief  Gets the hardware timestamper version
	 * @return HW timestamper version
	 */
	int getTimestampVersion();

	/**
	 * @brief  Adjusts the clock frequency.
	 * @param  freq_offset Frequency offset
	 * @return TRUE if adjusted. FALSE otherwise.
	 */
	bool _adjustClockRate( FrequencyRatio freq_offset );

	/**
	 * @brief  Adjusts the clock frequency.
	 * @param  freq_offset Frequency offset
	 * @return TRUE if adjusted. FALSE otherwise.
	 */
	bool adjustClockRate( FrequencyRatio freq_offset ) {
		return _adjustClockRate( freq_offset );
	}

	/**
	 * @brief  Adjusts the clock phase.
	 * @param  phase_adjust phase offset in ns
	 * @return TRUE if adjusted. FALSE otherwise.
	 */
	bool adjustClockPhase( int64_t phase_adjust );

	/**
	 * @brief  Gets extended error message from hardware timestamper
	 * @param  msg [out] Extended error message
	 * @return void
	 */
	void getExtendedError(char *msg);

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatRxSyncCount
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatRxSyncCount( void )
	{
		counters.ieee8021AsPortStatRxSyncCount++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatRxFollowUpCount
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatRxFollowUpCount( void )
	{
		counters.ieee8021AsPortStatRxFollowUpCount++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatRxPdelayRequest
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatRxPdelayRequest( void )
	{
		counters.ieee8021AsPortStatRxPdelayRequest++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatRxPdelayResponse
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatRxPdelayResponse( void )
	{
		counters.ieee8021AsPortStatRxPdelayResponse++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatRxPdelayResponseFollowUp
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatRxPdelayResponseFollowUp( void )
	{
		counters.ieee8021AsPortStatRxPdelayResponseFollowUp++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatRxAnnounce
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatRxAnnounce( void )
	{
		counters.ieee8021AsPortStatRxAnnounce++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatRxPTPPacketDiscard
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatRxPTPPacketDiscard( void )
	{
		counters.ieee8021AsPortStatRxPTPPacketDiscard++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatRxSyncReceiptTimeouts
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatRxSyncReceiptTimeouts( void )
	{
		counters.ieee8021AsPortStatRxSyncReceiptTimeouts++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatAnnounceReceiptTimeouts
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatAnnounceReceiptTimeouts( void )
	{
		counters.ieee8021AsPortStatAnnounceReceiptTimeouts++;
	}


	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatPdelayAllowedLostResponsesExceeded
	 * @return void
	 */
	// TODO: Not called
	void incCounter_ieee8021AsPortStatPdelayAllowedLostResponsesExceeded
	( void )
	{
		counters.
			ieee8021AsPortStatPdelayAllowedLostResponsesExceeded++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatTxSyncCount
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatTxSyncCount( void )
	{
		counters.ieee8021AsPortStatTxSyncCount++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatTxFollowUpCount
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatTxFollowUpCount( void )
	{
		counters.ieee8021AsPortStatTxFollowUpCount++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatTxPdelayRequest
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatTxPdelayRequest( void )
	{
		counters.ieee8021AsPortStatTxPdelayRequest++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatTxPdelayResponse
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatTxPdelayResponse( void )
	{
		counters.ieee8021AsPortStatTxPdelayResponse++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatTxPdelayResponseFollowUp
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatTxPdelayResponseFollowUp( void )
	{
		counters.ieee8021AsPortStatTxPdelayResponseFollowUp++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatTxAnnounce
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatTxAnnounce( void )
	{
		counters.ieee8021AsPortStatTxAnnounce++;
	}

	/**
	 * @brief  Logs port counters
	 * @return void
	 */
	void logIEEEPortCounters( void )
	{
		GPTP_LOG_STATUS
			( "IEEE Port Counter "
			  "ieee8021AsPortStatRxSyncCount : %u",
			  counters.ieee8021AsPortStatRxSyncCount );
		GPTP_LOG_STATUS
			( "IEEE Port Counter "
			  "ieee8021AsPortStatRxFollowUpCount : %u",
			  counters.ieee8021AsPortStatRxFollowUpCount );
		GPTP_LOG_STATUS
			( "IEEE Port Counter "
			  "ieee8021AsPortStatRxPdelayRequest : %u",
			  counters.ieee8021AsPortStatRxPdelayRequest );
		GPTP_LOG_STATUS
			( "IEEE Port Counter "
			  "ieee8021AsPortStatRxPdelayResponse : %u",
			  counters.ieee8021AsPortStatRxPdelayResponse );
		GPTP_LOG_STATUS
			( "IEEE Port Counter "
			  "ieee8021AsPortStatRxPdelayResponseFollowUp "
			  ": %u", counters.
			  ieee8021AsPortStatRxPdelayResponseFollowUp );
		GPTP_LOG_STATUS
			( "IEEE Port Counter "
			  "ieee8021AsPortStatRxAnnounce : %u",
			  counters.ieee8021AsPortStatRxAnnounce );
		GPTP_LOG_STATUS
			( "IEEE Port Counter "
			  "ieee8021AsPortStatRxPTPPacketDiscard : %u",
			  counters.
			  ieee8021AsPortStatRxPTPPacketDiscard );
		GPTP_LOG_STATUS
			( "IEEE Port Counter "
			  "ieee8021AsPortStatRxSyncReceiptTimeouts "
			  ": %u", counters.
			  ieee8021AsPortStatRxSyncReceiptTimeouts );
		GPTP_LOG_STATUS
			( "IEEE Port Counter "
			  "ieee8021AsPortStatAnnounceReceiptTimeouts "
			  ": %u", counters.
			  ieee8021AsPortStatAnnounceReceiptTimeouts );
		GPTP_LOG_STATUS
			( "IEEE Port Counter "
			  "ieee8021AsPortStatPdelayAllowed"
			  "LostResponsesExceeded : %u", counters.
			  ieee8021AsPortStatPdelayAllowedLostResponsesExceeded
				);
		GPTP_LOG_STATUS
			( "IEEE Port Counter "
			  "ieee8021AsPortStatTxSyncCount : %u",
			  counters.ieee8021AsPortStatTxSyncCount );
		GPTP_LOG_STATUS
			( "IEEE Port Counter "
			  "ieee8021AsPortStatTxFollowUpCount : %u", counters.
			  ieee8021AsPortStatTxFollowUpCount);
		GPTP_LOG_STATUS
			( "IEEE Port Counter "
			  "ieee8021AsPortStatTxPdelayRequest : %u",
			  counters.ieee8021AsPortStatTxPdelayRequest);
		GPTP_LOG_STATUS
			( "IEEE Port Counter "
			  "ieee8021AsPortStatTxPdelayResponse : %u", counters.
			  ieee8021AsPortStatTxPdelayResponse);
		GPTP_LOG_STATUS
			( "IEEE Port Counter "
			  "ieee8021AsPortStatTxPdelayResponseFollowUp : %u",
			  counters.ieee8021AsPortStatTxPdelayResponseFollowUp
				);
		GPTP_LOG_STATUS
			( "IEEE Port Counter "
			  "ieee8021AsPortStatTxAnnounce : %u",
			  counters.ieee8021AsPortStatTxAnnounce);
	}

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
	void getDeviceTime
	( Timestamp &system_time, Timestamp &device_time,
	  uint32_t &local_clock, uint32_t & nominal_clock_rate );

	/**
	 * @brief  Sets asCapable flag
	 * @param  ascap flag to be set. If FALSE, marks peer_offset_init as
	 * false.
	 * @return void
	 */
	void setAsCapable(bool ascap)
	{
		if ( ascap != asCapable ) {
			GPTP_LOG_STATUS
				("*** AsCapable STATE CHANGE: %s *** (Announce messages will %s be sent)", 
				 ascap == true ? "ENABLED" : "DISABLED",
				 ascap == true ? "NOW" : "NO LONGER");
			
			// Milan B.1: Handle asCapable changes for stream stability tracking
			handleMilanAsCapableChange(ascap);
		}
		if( !ascap )
		{
			_peer_offset_init = false;
		}
		asCapable = ascap;
	}

	/**
	 * @brief  Gets the asCapable flag
	 * @return asCapable flag.
	 */
	bool getAsCapable()
	{
		return( asCapable );
	}

	/**
	 * @brief  Gets the Peer rate offset. Used to calculate neighbor
	 * rate ratio.
	 * @return FrequencyRatio peer rate offset
	 */
	FrequencyRatio getPeerRateOffset( void )
	{
		return _peer_rate_offset;
	}

	/**
	 * @brief  Sets the peer rate offset. Used to calculate neighbor rate
	 * ratio.
	 * @param  offset Offset to be set
	 * @return void
	 */
	void setPeerRateOffset( FrequencyRatio offset ) {
		_peer_rate_offset = offset;
	}

	/**
	 * @brief  Sets peer offset timestamps
	 * @param  mine Local timestamps
	 * @param  theirs Remote timestamps
	 * @return void
	 */
	void setPeerOffset(Timestamp mine, Timestamp theirs) {
		_peer_offset_ts_mine = mine;
		_peer_offset_ts_theirs = theirs;
		_peer_offset_init = true;
	}

	/**
	 * @brief  Gets peer offset timestamps
	 * @param  mine [out] Reference to local timestamps
	 * @param  theirs [out] Reference to remote timestamps
	 * @return TRUE if peer offset has already been initialized. FALSE
	 * otherwise.
	 */
	bool getPeerOffset(Timestamp & mine, Timestamp & theirs) {
		mine = _peer_offset_ts_mine;
		theirs = _peer_offset_ts_theirs;
		return _peer_offset_init;
	}

	/**
	 * @brief  Sets the neighbor propagation delay threshold
	 * @param  delay Delay in nanoseconds
	 * @return void
	 */
	void setNeighPropDelayThresh(int64_t delay) {
		neighbor_prop_delay_thresh = delay;
	}

	/**
	 * @brief  Restart PDelay
	 * @return void
	 */
	void restartPDelay() {
		_peer_offset_init = false;
	}

	/**
	 * @brief  Gets a pointer to timer_factory object
	 * @return timer_factory pointer
	 */
	const OSTimerFactory *getTimerFactory() {
		return timer_factory;
	}

	/**
	 * @brief Watch for link up and down events.
	 * @return Its an infinite loop. Returns NULL in case of error.
	 */
	void *watchNetLink( void )
	{
		// Should never return
		net_iface->watchNetLink(this);

		return NULL;
	}

	/**
	 * @brief Receive frame
	 */
	net_result recv
	( LinkLayerAddress *addr, uint8_t *payload, size_t &length,
	  uint32_t &link_speed )
	{
		net_result result = net_iface->nrecv( addr, payload, length );
		link_speed = this->link_speed;
		return result;
	}

	/**
	 * @brief Send frame
	 */
	net_result send
	( LinkLayerAddress *addr, uint16_t etherType, uint8_t *payload,
	  size_t length, bool timestamp )
	{
		return net_iface->send
		( addr, etherType, payload, length, timestamp );
	}

	/**
	 * @brief Get the payload offset inside a packet
	 * @return 0
	 */
	unsigned getPayloadOffset()
	{
		return net_iface->getPayloadOffset();
	}

	/**
	 * @brief Starts link thread
	 * @return TRUE if ok, FALSE if error
	 */
	bool linkWatch( OSThreadFunction func, OSThreadFunctionArg arg )
	{
		return link_thread->start( func, arg );
	}

	/**
	 * @brief Starts listening thread
	 * @return TRUE if ok, FALSE if error
	 */
	bool linkOpen( OSThreadFunction func, OSThreadFunctionArg arg )
	{
		return listening_thread->start( func, arg );
	}

	/**
	 * @brief Terminates the thread
	 * @return void
	 */
	void stopLinkWatchThread()
	{
		GPTP_LOG_VERBOSE("Stop link watch thread");
		setLinkThreadRunning(false);
	}

	/**
	 * @brief Terminates the thread
	 * @return void
	 */
	void stopListeningThread()
	{
		GPTP_LOG_VERBOSE("Stop listening thread");
		setListeningThreadRunning(false);
	}

	/**
	 * @brief Joins terminated thread
	 * @param exit_code [out]
	 * @return TRUE if ok, FALSE if error
	 */
	bool joinLinkWatchThread( OSThreadExitCode & exit_code )
	{
		return link_thread->join( exit_code );
	}

	/**
	 * @brief Joins terminated thread
	 * @param exit_code [out]
	 * @return TRUE if ok, FALSE if error
	 */
	bool joinListeningThread( OSThreadExitCode & exit_code )
	{
		return listening_thread->join( exit_code );
	}

	/**
	 * @brief Sets the listeningThreadRunning flag
	 * @param state value to be set
	 * @return void
	 */
	void setListeningThreadRunning(bool state)
	{
		listening_thread_running = state;
	}

	/**
	 * @brief Gets the listeningThreadRunning flag
	 * @return TRUE if running, FALSE if stopped
	 */
	bool getListeningThreadRunning()
	{
		return listening_thread_running;
	}

	/**
	 * @brief Sets the linkThreadRunning flag
	 * @param state value to be set
	 * @return void
	 */
	void setLinkThreadRunning(bool state)
	{
		link_thread_running = state;
	}

	/**
	 * @brief Gets the linkThreadRunning flag
	 * @return TRUE if running, FALSE if stopped
	 */
	bool getLinkThreadRunning()
	{
		return link_thread_running;
	}

	/**
	 * @brief  Gets the portState information
	 * @return PortState
	 */
	PortState getPortState( void ) {
		return port_state;
	}

	/**
	 * @brief Sets the PortState
	 * @param state value to be set
	 * @return void
	 */
	void setPortState( PortState state ) {
		port_state = state;
	}

	/**
	 * @brief  Gets port identity
	 * @param  identity [out] Reference to PortIdentity
	 * @return void
	 */
	void getPortIdentity(PortIdentity & identity) {
		identity = this->port_identity;
	}

	/**
	 * @brief  Gets the "best" announce
	 * @return Pointer to PTPMessageAnnounce
	 */
	PTPMessageAnnounce *calculateERBest( void );

	/**
	 * @brief  Changes the port state
	 * @param  state Current state
	 * @param  changed_external_master TRUE if external master has
	 * changed, FALSE otherwise
	 * @return void
	 */
	void recommendState(PortState state, bool changed_external_master);

	/**
	 * @brief  Locks the TX port
	 * @return TRUE if success. FALSE otherwise.
	 */
	virtual bool getTxLock()
	{
		return true;
	}

	/**
	 * @brief  Unlocks the port TX.
	 * @return TRUE if success. FALSE otherwise.
	 */
	virtual bool putTxLock()
	{
		return false;
	}

	/**
	 * @brief  Adds a new qualified announce the port. IEEE 802.1AS
	 * Clause 10.3.10.2
	 * @param  annc PTP announce message
	 * @return void
	 */
	void setQualifiedAnnounce( PTPMessageAnnounce *annc )
	{
		delete qualified_announce;
		qualified_announce = annc;
	}

	/**
	 * @brief  Switches port to a gPTP master
	 * @param  annc If TRUE, starts announce event timer.
	 * @return void
	 */
	virtual void becomeMaster( bool annc ) = 0;

	/**
	 * @brief  Switches port to a gPTP slave.
	 * @param  restart_syntonization if TRUE, restarts the syntonization
	 * @return void
	 */
	virtual void becomeSlave( bool restart_syntonization ) = 0;

	// Unified profile accessors
	const gPTPProfile& getProfile() const { return active_profile; }
	gPTPProfile& getProfile() { return active_profile; }
	void setProfile(gPTPProfile&& profile) { active_profile = std::move(profile); }
	
	// Profile-specific behavior helpers (recommended approach)
	bool shouldSetAsCapableOnStartup() const { return active_profile.initial_as_capable; }
	bool shouldSetAsCapableOnLinkUp() const { return active_profile.as_capable_on_link_up; }
	bool shouldSetAsCapableOnLinkDown() const { return active_profile.as_capable_on_link_down; }
	bool shouldSendAnnounceWhenAsCapable() const { return active_profile.send_announce_when_as_capable_only; }
	bool shouldProcessSyncRegardlessAsCapable() const { return active_profile.process_sync_regardless_as_capable; }
	bool shouldStartPDelayOnLinkUp() const { return active_profile.start_pdelay_on_link_up; }
	int8_t getProfileSyncInterval() const { return active_profile.sync_interval_log; }
	int8_t getProfileAnnounceInterval() const { return active_profile.announce_interval_log; }
	int8_t getProfilePDelayInterval() const { return active_profile.pdelay_interval_log; }
	unsigned int getProfileSyncReceiptThreshold() const { return active_profile.sync_receipt_thresh; }
	bool getAllowsNegativeCorrectionField() const { return active_profile.allows_negative_correction_field; }
	bool getRequiresStrictTimeouts() const { return active_profile.requires_strict_timeouts; }
	bool getSupportsBMCA() const { return active_profile.supports_bmca; }

	// Profile statistics and monitoring
	void updateProfileJitterStats(uint64_t jitter_ns);
	bool checkProfileConvergence();

	// Milan B.1 profile integration helpers
	void handleMilanAsCapableChange(bool new_as_capable);
	void handleMilanGrandmasterChange(const PortIdentity& old_gm, const PortIdentity& new_gm);

	// Counter and interval accessors
	unsigned int getSyncCount() const { return sync_count; }
	void setSyncCount(unsigned int v) { sync_count = v; }
	void incSyncCount() { ++sync_count; }
	unsigned int getPdelayCount() const { return pdelay_count; }
	void setPdelayCount(unsigned int v) { pdelay_count = v; }
	void incPdelayCount() { ++pdelay_count; }
	unsigned int getConsecutiveLateResponses() const { return _consecutive_late_responses; }
	void setConsecutiveLateResponses(unsigned int v) { _consecutive_late_responses = v; }
	unsigned int getConsecutiveMissingResponses() const { return _consecutive_missing_responses; }
	void setConsecutiveMissingResponses(unsigned int v) { _consecutive_missing_responses = v; }
	Timestamp getLastPDelayReqTimestamp() const { return _last_pdelay_req_timestamp; }
	void setLastPDelayReqTimestamp(Timestamp t) { _last_pdelay_req_timestamp = t; }
	bool getPDelayResponseReceived() const { return _pdelay_response_received; }
	void setPDelayResponseReceived(bool v) { _pdelay_response_received = v; }

	// Interval accessors (stubs, to be implemented as needed)
	int getInitSyncInterval() const { return initialLogSyncInterval; }
	void setInitSyncInterval(int v) { initialLogSyncInterval = v; }
	int getInitPDelayInterval() const { return initialLogPdelayReqInterval; }
	void setInitPDelayInterval(int v) { initialLogPdelayReqInterval = v; }
	int getSyncInterval() const { return log_mean_sync_interval; }
	void setSyncInterval(int v) { log_mean_sync_interval = v; }
	int getAnnounceInterval() const { return log_mean_announce_interval; }
	void setAnnounceInterval(int v) { log_mean_announce_interval = v; }
	int getPDelayInterval() const { return log_min_mean_pdelay_req_interval; }
	void setPDelayInterval(int v) { log_min_mean_pdelay_req_interval = v; }
	void resetInitSyncInterval() { initialLogSyncInterval = 0; }
	void resetInitPDelayInterval() { initialLogPdelayReqInterval = 0; }

	// GM Time Base Indicator accessors
	uint16_t getLastGmTimeBaseIndicator() const { return lastGmTimeBaseIndicator; }
	void setLastGmTimeBaseIndicator(uint16_t v) { lastGmTimeBaseIndicator = v; }

	// Allow negative correction field accessor
	bool getAllowNegativeCorrField() const { return allow_negative_correction_field; }

	// Timer lock accessors (stubs, to be implemented as needed)
	void startSyncReceiptTimer();
	void startSyncReceiptTimer(uint64_t interval);
	void stopSyncReceiptTimer();
	void startSyncIntervalTimer();
	void startSyncIntervalTimer(uint64_t interval);
	void stopSyncIntervalTimer();
	void startAnnounceIntervalTimer();
	void startAnnounceIntervalTimer(uint64_t interval);
	void stopAnnounceIntervalTimer();
	void startPDelayIntervalTimer(uint64_t interval);
	virtual void stopPDelayIntervalTimer();

	// Event processing (stubs)
	void processEvent();
	bool processEvent(Event);
	void processStateChange(PortState state, bool changed_external_master);
	bool processStateChange(Event);
	void processSyncAnnounceTimeout();
	bool processSyncAnnounceTimeout(Event);
	void startAnnounce();
	void sendGeneralPort();
	void sendGeneralPort(int, uint8_t*, uint16_t, MulticastType, PortIdentity*);
	Timestamp getTxPhyDelay(uint32_t link_speed) const;
	Timestamp getRxPhyDelay(uint32_t link_speed) const;

	// Missing virtual functions that derived classes must implement
	virtual bool _processEvent(Event e) { return false; }
	virtual void syncDone() { }

	// Sequence ID accessors
	int getNextSyncSequenceId() const { return sync_sequence_id + 1; }
	int getNextAnnounceSequenceId() const { return announce_sequence_id + 1; }
	int getNextSignalSequenceId() const { return signal_sequence_id + 1; }
};

#endif /*COMMON_PORT_HPP*/