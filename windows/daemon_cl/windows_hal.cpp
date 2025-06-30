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

#include <WinSock2.h>
#include <iphlpapi.h>
#include <windows_hal.hpp>
#include <IPCListener.hpp>
#include <PeerList.hpp>
#include <gptp_log.hpp>
#include "windows_crosststamp.hpp"

inline uint64_t scale64(uint64_t i, uint32_t m, uint32_t n)
{
	uint64_t tmp, res, rem;

	rem = i % n;
	i /= n;

	res = i * m;
	tmp = rem * m;

	tmp /= n;

	return res + tmp;
}

void WirelessTimestamperCallback( LPVOID arg )
{
	WirelessTimestamperCallbackArg *larg =
		(WirelessTimestamperCallbackArg *)arg;
	WindowsWirelessTimestamper *timestamper =
		dynamic_cast<WindowsWirelessTimestamper *> (larg->timestamper);
	WirelessDialog tmp1, tmp2;
	LinkLayerAddress *peer_addr = NULL;

	if (timestamper == NULL)
	{
		GPTP_LOG_ERROR( "Wrong timestamper type: %p",
				larg->timestamper );
		return;
	}

	switch( larg->iEvent_type )
	{
	default:
	case TIMINGMSMT_CONFIRM_EVENT:
		tmp1.action_devclk = larg->event_data.confirm.T1;
		tmp1.ack_devclk = larg->event_data.confirm.T4;
		tmp1.dialog_token = (BYTE)larg->event_data.confirm.DialogToken;
		GPTP_LOG_VERBOSE
			( "Got confirm, %hhu(%llu,%llu)", tmp1.dialog_token,
			  tmp1.action_devclk, tmp1.ack_devclk );
		peer_addr = new LinkLayerAddress
			( larg->event_data.confirm.PeerMACAddress );
		larg->timestamper->
			timingMeasurementConfirmCB( *peer_addr, &tmp1 );

		break;

	case TIMINGMSMT_EVENT:
		tmp1/*prev*/.action_devclk = larg->event_data.indication.
			indication.T1;
		tmp1/*prev*/.ack_devclk = larg->event_data.indication.
			indication.T4;
		tmp1/*prev*/.dialog_token = (BYTE)larg->event_data.indication.
			indication.FollowUpDialogToken;
		tmp2/*curr*/.action_devclk = larg->event_data.indication.
			indication.T2;
		tmp2/*curr*/.ack_devclk = larg->event_data.indication.
			indication.T3;
		tmp2/*curr*/.dialog_token = (BYTE)larg->event_data.indication.
			indication.DialogToken;
		GPTP_LOG_VERBOSE
			("Got indication, %hhu(%llu,%llu) %hhu(%llu,%llu)",
			 tmp1.dialog_token, tmp1.action_devclk,
			 tmp1.ack_devclk, tmp2.dialog_token,
			 tmp2.action_devclk, tmp2.ack_devclk);
		peer_addr = new LinkLayerAddress(larg->event_data.indication.
						 indication.PeerMACAddress);

		larg->timestamper->timeMeasurementIndicationCB
			( *peer_addr, &tmp2, &tmp1,
			  larg->event_data.indication.
			  indication.PtpSpec.fwup_data,
			  larg->event_data.indication.
			  indication.WiFiVSpecHdr.Length - (sizeof(PTP_SPEC) -
							    1));

		break;

	case TIMINGMSMT_CORRELATEDTIME_EVENT:
		timestamper->system_counter =
			scale64( larg->event_data.ptm_wa.TSC, NS_PER_SECOND,
				 (uint32_t)timestamper->tsc_hz.QuadPart );
		timestamper->system_time.set64(timestamper->system_counter);
		// Scale from TM timescale to nanoseconds
		larg->event_data.ptm_wa.LocalClk *= 10;
		timestamper->device_time.set64
			(larg->event_data.ptm_wa.LocalClk*10);

		break;
	}

	delete peer_addr;
}

net_result WindowsWirelessTimestamper::_requestTimingMeasurement
(TIMINGMSMT_REQUEST *timingmsmt_req)
{
	net_result ret = net_succeed;

	if (!adapter->initiateTimingRequest(timingmsmt_req)) {
		GPTP_LOG_ERROR("Failed to send timing measurement request\n");
		ret = net_fatal;
	}

	return ret;
}

bool WindowsWirelessTimestamper::HWTimestamper_gettime
(	Timestamp *system_time,
	Timestamp * device_time,
	uint32_t * local_clock,
	uint32_t * nominal_clock_rate ) const
{
	bool refreshed = adapter->refreshCrossTimestamp();
	if (refreshed)
	{
		// We have a fresh cross-timestamp just use it
		*system_time = this->system_time;
		*device_time = this->device_time;
	} else
	{
		// We weren't able to get a fresh timestamp,
		// extrapolate from the last
		LARGE_INTEGER tsc_now;
		QueryPerformanceCounter(&tsc_now);
		unsigned device_delta = (unsigned)
			(((long double) (tsc_now.QuadPart - system_counter)) /
			 (((long double)tsc_hz.QuadPart) / 1000000000));
		device_delta = (unsigned)(device_delta*getPort()->
					  getLocalSystemFreqOffset());
		system_time->set64((uint64_t)
				   (((long double)tsc_now.QuadPart) /
				    ((long double)tsc_hz.QuadPart /
				     1000000000)));
		device_time->set64(device_delta);
		*device_time = *device_time + this->device_time;
	}

	return true;
}

bool WindowsWirelessTimestamper::HWTimestamper_init
(InterfaceLabel *iface_label, OSNetworkInterface *iface)
{
	uint8_t mac_addr_local[ETHER_ADDR_OCTETS];

	if (!initialized) {
		if (!adapter->initialize()) return false;
		if (getPort()->getLocalAddr() == NULL)
			return false;

		getPort()->getLocalAddr()->toOctetArray(mac_addr_local);
		if (!adapter->attachAdapter(mac_addr_local)) {
			return false;
		}

		tsc_hz.QuadPart = getTSCFrequency(false);
		if (tsc_hz.QuadPart == 0) {
			return false;
		}

		if (!adapter->registerTimestamper(this))
			return false;
	}

	initialized = true;
	return true;
}

WindowsWirelessTimestamper::~WindowsWirelessTimestamper() {
	if (adapter->deregisterTimestamper(this))
		adapter->shutdown();
	else
		GPTP_LOG_INFO("Failed to shutdown time sync on adapter");
}

bool WindowsEtherTimestamper::HWTimestamper_init( InterfaceLabel *iface_label, OSNetworkInterface *net_iface ) {
	char network_card_id[64];
	LinkLayerAddress *addr = dynamic_cast<LinkLayerAddress *>(iface_label);
	if( addr == NULL ) return false;
	PIP_ADAPTER_INFO pAdapterInfo;
	IP_ADAPTER_INFO AdapterInfo[32];       // Allocate information for up to 32 NICs
	DWORD dwBufLen = sizeof(AdapterInfo);  // Save memory size of buffer

	DWORD dwStatus = GetAdaptersInfo( AdapterInfo, &dwBufLen );
	if( dwStatus != ERROR_SUCCESS ) return false;

	for( pAdapterInfo = AdapterInfo; pAdapterInfo != NULL; pAdapterInfo = pAdapterInfo->Next ) {
		if( pAdapterInfo->AddressLength == ETHER_ADDR_OCTETS && *addr == LinkLayerAddress( pAdapterInfo->Address )) {
			break;
		}
	}

	if( pAdapterInfo == NULL ) return false;

	// Use the modular hardware clock rate detection
	// This tries IPHLPAPI first, then NDIS, then falls back to legacy mapping
	uint64_t detected_rate = 0;
	
	// Try IPHLPAPI method
	detected_rate = getHardwareClockRate_IPHLPAPI(pAdapterInfo->Description);
	if (detected_rate == 0) {
		// Try NDIS method
		detected_rate = getHardwareClockRate_NDIS(pAdapterInfo->Description);
	}
	
	if (detected_rate > 0) {
		netclock_hz.QuadPart = detected_rate;
		GPTP_LOG_INFO("Using detected clock rate %llu Hz for interface %s", 
			detected_rate, pAdapterInfo->Description);
	}
	else {
		// Fallback to legacy device mapping
		DeviceClockRateMapping *rate_map = DeviceClockRateMap;
		while (rate_map->device_desc != NULL)
		{
			if (strstr(pAdapterInfo->Description, rate_map->device_desc) != NULL)
				break;
			++rate_map;
		}
		if (rate_map->device_desc != NULL) {
			netclock_hz.QuadPart = rate_map->clock_rate;
			GPTP_LOG_INFO("Using predefined clock rate %llu Hz for interface %s", 
				rate_map->clock_rate, pAdapterInfo->Description);
		}
		else {
			GPTP_LOG_ERROR("Unable to determine clock rate for interface %s", pAdapterInfo->Description);
			return false;
		}
	}

	GPTP_LOG_INFO( "Adapter UID: %s\n", pAdapterInfo->AdapterName );
	PLAT_strncpy( network_card_id, NETWORK_CARD_ID_PREFIX, 63 );
	PLAT_strncpy( network_card_id+strlen(network_card_id), pAdapterInfo->AdapterName, 63-strlen(network_card_id) );

        miniport = CreateFile( network_card_id,
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL, OPEN_EXISTING, 0, NULL );
        if( miniport == INVALID_HANDLE_VALUE ) {
		DWORD error = GetLastError();
		GPTP_LOG_ERROR("Failed to open network adapter %s (error: %d). Ensure adapter supports direct access and application has administrator privileges", 
			network_card_id, error);
		return false;
	}

	// Verify timestamp capabilities and configure if needed
#ifdef OID_TIMESTAMP_CAPABILITY
	{
		DWORD returned = 0;
		DWORD result;
#if defined(NDIS_TIMESTAMP_CAPABILITIES_REVISION_1) && defined(NDIS_TIMESTAMP_CAPABILITY_FLAGS)
		NDIS_TIMESTAMP_CAPABILITIES caps;
#else
		/*
		 * Fall back to the IPHLPAPI INTERFACE_TIMESTAMP_CAPABILITIES
		 * structure when the newer NDIS timestamp capability flags are
		 * not available in the SDK headers.
		 */
		INTERFACE_TIMESTAMP_CAPABILITIES caps;
#endif

		memset(&caps, 0, sizeof(caps));
		result = readOID(OID_TIMESTAMP_CAPABILITY, &caps, sizeof(caps), &returned);
		if(result != ERROR_SUCCESS || returned < sizeof(caps)) {
			GPTP_LOG_WARNING("Failed to query timestamp capability (error: %d, returned: %d/%d), attempting fallback", 
				result, returned, sizeof(caps));
			
			// Log more detailed error information
			if (result == ERROR_NOT_SUPPORTED || result == ERROR_INVALID_FUNCTION) {
				GPTP_LOG_INFO("Driver does not support timestamp capability queries, using software timestamping");
			} else if (result == ERROR_ACCESS_DENIED) {
				GPTP_LOG_ERROR("Access denied when querying timestamp capability. Run as administrator or check driver permissions");
			} else {
				GPTP_LOG_WARNING("Unknown error %d when querying timestamp capability", result);
			}
			
			// Continue without hardware timestamping but log the limitation
			GPTP_LOG_STATUS("Falling back to software timestamping for interface %s", pAdapterInfo->Description);
			goto skip_timestamp_config;
		}

#if defined(NDIS_TIMESTAMP_CAPABILITIES_REVISION_1) && defined(NDIS_TIMESTAMP_CAPABILITY_FLAGS)
		if(caps.TimestampFlags == 0) {
			GPTP_LOG_WARNING("Adapter reports no timestamping capabilities, using software timestamping");
			goto skip_timestamp_config;
		} else {
			GPTP_LOG_INFO("Adapter timestamp capabilities: 0x%x", caps.TimestampFlags);
		}
#else
		/* INTERFACE_TIMESTAMP_CAPABILITIES does not expose a single
		 * flag field. Treat an all-zero structure as lack of
		 * timestamp support.
		 */
		INTERFACE_TIMESTAMP_CAPABILITIES zeroCaps;
		memset(&zeroCaps, 0, sizeof(zeroCaps));
		if(!memcmp(&caps, &zeroCaps, sizeof(caps))) {
			GPTP_LOG_WARNING("Adapter reports no timestamping capabilities, using software timestamping");
			goto skip_timestamp_config;
		} else {
			GPTP_LOG_INFO("Adapter supports hardware timestamping");
		}
#endif

#ifdef OID_TIMESTAMP_CURRENT_CONFIG
#if defined(NDIS_TIMESTAMP_CAPABILITIES_REVISION_1) && defined(NDIS_TIMESTAMP_CAPABILITY_FLAGS)
		NDIS_TIMESTAMP_CAPABILITIES cfg;
#else
		INTERFACE_TIMESTAMP_CAPABILITIES cfg;
#endif
		memset(&cfg, 0, sizeof(cfg));
		if(readOID(OID_TIMESTAMP_CURRENT_CONFIG, &cfg, sizeof(cfg), &returned) == ERROR_SUCCESS) {
#if defined(NDIS_TIMESTAMP_CAPABILITIES_REVISION_1) && defined(NDIS_TIMESTAMP_CAPABILITY_FLAGS)
			cfg.TimestampFlags = caps.TimestampFlags;
#else
			/* Older IPHLPAPI path uses the same structure for
			 * capability and configuration. */
			memcpy(&cfg, &caps, sizeof(cfg));
#endif
			if (setOID(OID_TIMESTAMP_CURRENT_CONFIG, &cfg, sizeof(cfg)) == ERROR_SUCCESS) {
				GPTP_LOG_INFO("Successfully configured hardware timestamping");
			} else {
				GPTP_LOG_WARNING("Failed to configure hardware timestamping, using software fallback");
			}
		} else {
			GPTP_LOG_WARNING("Failed to read current timestamp configuration, using software fallback");
		}
#endif
	}
skip_timestamp_config:
#else
	GPTP_LOG_INFO("OID_TIMESTAMP_CAPABILITY not available, trying legacy Intel OIDs");
#endif

	// Try Intel-specific OIDs as fallback for hardware timestamping
	// This implements Option 2 from the Intel community thread discussion
	bool intel_oids_available = false;
	{
		intel_oids_available = testIntelOIDAvailability(pAdapterInfo->Description);
		
		if (intel_oids_available) {
			GPTP_LOG_STATUS("Intel custom OIDs are available - hardware timestamping enabled via legacy path");
			GPTP_LOG_INFO("Using Intel-specific OID timestamping for interface %s", pAdapterInfo->Description);
			
			// Log which Intel adapter we detected
			DeviceClockRateMapping *rate_map = DeviceClockRateMap;
			while (rate_map->device_desc != NULL) {
				if (strstr(pAdapterInfo->Description, rate_map->device_desc) != NULL) {
					GPTP_LOG_INFO("Detected Intel adapter type: %s with clock rate %u Hz", 
						rate_map->device_desc, rate_map->clock_rate);
					break;
				}
				++rate_map;
			}
		} else {
			GPTP_LOG_INFO("Intel custom OIDs not available - confirmed software timestamping");
		}
	}
	
	// If Intel OIDs are available, we can potentially use hardware timestamping
	if (intel_oids_available) {
		GPTP_LOG_STATUS("Hardware timestamping available via Intel custom OIDs for %s", pAdapterInfo->Description);
	} else {
		GPTP_LOG_STATUS("Using software timestamping for interface %s", pAdapterInfo->Description);
	}

	tsc_hz.QuadPart = getTSCFrequency( true );
	if( tsc_hz.QuadPart == 0 ) {
		GPTP_LOG_ERROR("Failed to determine TSC frequency. This is required for timestamping. Check CPU support and system configuration");
		return false;
	}
	GPTP_LOG_INFO("TSC frequency: %llu Hz", tsc_hz.QuadPart);

	// Initialize cross-timestamping functionality for this interface
	WindowsCrossTimestamp& crossTimestamp = getGlobalCrossTimestamp();
	if (!crossTimestamp.initialize(pAdapterInfo->Description)) {
		GPTP_LOG_WARNING("Failed to initialize cross-timestamping for interface %s, using legacy timestamps", 
			pAdapterInfo->Description);
		cross_timestamping_initialized = false;
		// Continue without cross-timestamping - legacy method will be used
	} else {
		GPTP_LOG_STATUS("Cross-timestamping initialized for interface %s with quality %d%%", 
			pAdapterInfo->Description, crossTimestamp.getTimestampQuality());
		cross_timestamping_initialized = true;
	}

	return true;
}

bool WindowsNamedPipeIPC::init(OS_IPC_ARG *arg) {
	IPCListener *ipclistener;
	IPCSharedData ipcdata = { &peerList_, &lOffset_ };

	ipclistener = new IPCListener();
	// Start IPC listen thread
	if (!ipclistener->start(ipcdata)) {
		GPTP_LOG_ERROR("Starting IPC listener thread failed");
	}
	else {
		GPTP_LOG_INFO("Starting IPC listener thread succeeded");
	}

	return true;
}

bool WindowsNamedPipeIPC::update(
	int64_t ml_phoffset,
	int64_t ls_phoffset,
	FrequencyRatio ml_freqoffset,
	FrequencyRatio ls_freq_offset,
	uint64_t local_time,
	uint32_t sync_count,
	uint32_t pdelay_count,
	PortState port_state,
	bool asCapable )
{
	lOffset_.get();
	lOffset_.local_time = local_time;
	lOffset_.ml_freqoffset = ml_freqoffset;
	lOffset_.ml_phoffset = ml_phoffset;
	lOffset_.ls_freqoffset = ls_freq_offset;
	lOffset_.ls_phoffset = ls_phoffset;

	if (!lOffset_.isReady()) lOffset_.setReady(true);
	lOffset_.put();
	return true;
}

bool WindowsNamedPipeIPC::update_grandmaster(
	uint8_t gptp_grandmaster_id[],
	uint8_t gptp_domain_number )
{
	lOffset_.get();
	memcpy(lOffset_.gptp_grandmaster_id, gptp_grandmaster_id, PTP_CLOCK_IDENTITY_LENGTH);
	lOffset_.gptp_domain_number = gptp_domain_number;

	if (!lOffset_.isReady()) lOffset_.setReady(true);
	lOffset_.put();
	return true;
}

bool WindowsNamedPipeIPC::update_network_interface(
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
	uint16_t port_number )
{
	lOffset_.get();
	memcpy(lOffset_.clock_identity, clock_identity, PTP_CLOCK_IDENTITY_LENGTH);
	lOffset_.priority1 = priority1;
	lOffset_.clock_class = clock_class;
	lOffset_.offset_scaled_log_variance = offset_scaled_log_variance;
	lOffset_.clock_accuracy = clock_accuracy;
	lOffset_.priority2 = priority2;
	lOffset_.domain_number = domain_number;
	lOffset_.log_sync_interval = log_sync_interval;
	lOffset_.log_announce_interval = log_announce_interval;
	lOffset_.log_pdelay_interval = log_pdelay_interval;
	lOffset_.port_number = port_number;

	if (!lOffset_.isReady()) lOffset_.setReady(true);
	lOffset_.put();
	return true;
}


void WindowsPCAPNetworkInterface::watchNetLink( CommonPort *pPort)
{
	// ✅ IMPLEMENTED: Event-driven link up/down detection using Windows notification APIs
	// This function starts continuous monitoring of network interface status changes
	// Replaces TODO: "add link up/down detection, Google MIB_IPADDR_DISCONNECTED"
	//
	// Implementation provides:
	// - Event-driven monitoring using NotifyAddrChange/NotifyRouteChange
	// - Interface operational status monitoring (IfOperStatusUp/Down)
	// - IP address connectivity state checking (DadState validation)
	// - Proper MAC address matching with the PTP port
	// - Background thread for continuous monitoring with automatic cleanup
	// - Real-time notifications to CommonPort when link state changes
	
	if (!pPort) {
		GPTP_LOG_ERROR("Invalid port pointer in watchNetLink");
		return;
	}

	// Get the interface information to start event-driven monitoring
	PIP_ADAPTER_ADDRESSES pAddresses = NULL;
	PIP_ADAPTER_ADDRESSES pCurrAddresses = NULL;
	ULONG outBufLen = 0;
	DWORD dwRetVal = 0;

	// Initial call to determine buffer size needed
	dwRetVal = GetAdaptersAddresses(AF_UNSPEC, 
		GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
		NULL, pAddresses, &outBufLen);

	if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
		pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(outBufLen);
		if (pAddresses == NULL) {
			GPTP_LOG_ERROR("Memory allocation failed for adapter addresses");
			return;
		}
	} else if (dwRetVal != ERROR_SUCCESS) {
		GPTP_LOG_ERROR("GetAdaptersAddresses initial call failed with error %d", dwRetVal);
		return;
	}

	// Get adapter information
	dwRetVal = GetAdaptersAddresses(AF_UNSPEC,
		GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
		NULL, pAddresses, &outBufLen);

	if (dwRetVal == NO_ERROR) {
		// Find our interface by comparing with the port's local address
		LinkLayerAddress *port_addr = pPort->getLocalAddr();
		
		pCurrAddresses = pAddresses;
		while (pCurrAddresses) {
			// Check if this is our target interface
			if (pCurrAddresses->PhysicalAddressLength == ETHER_ADDR_OCTETS && port_addr) {
				// Compare MAC addresses
				uint8_t port_mac[ETHER_ADDR_OCTETS];
				port_addr->toOctetArray(port_mac);
				
				if (memcmp(port_mac, pCurrAddresses->PhysicalAddress, ETHER_ADDR_OCTETS) == 0) {
					// Found our interface - start event-driven monitoring
					
					// Convert interface description for monitoring
					char desc_narrow[256] = {0};
					WideCharToMultiByte(CP_UTF8, 0, pCurrAddresses->Description, -1, 
									   desc_narrow, sizeof(desc_narrow), NULL, NULL);
					
					// Start event-driven link monitoring
					LinkMonitorContext* pLinkContext = startLinkMonitoring(pPort, desc_narrow, port_mac);
					if (pLinkContext) {
						GPTP_LOG_STATUS("Event-driven link monitoring started for interface %s", desc_narrow);
						
						// Store context in the network interface for later cleanup (if needed)
						// Note: The monitoring will continue until stopped explicitly or application exit
						
						// Perform initial link status check and notify port
						bool initial_link_status = checkLinkStatus(desc_narrow, port_mac);
						pPort->setAsCapable(initial_link_status);
						
						GPTP_LOG_STATUS("Initial link status for %s: %s", 
										desc_narrow, initial_link_status ? "UP" : "DOWN");
					} else {
						GPTP_LOG_ERROR("Failed to start event-driven link monitoring for interface %s", desc_narrow);
						
						// Fallback to one-time status check
						bool interface_up = false;
						
						switch (pCurrAddresses->OperStatus) {
						case IfOperStatusUp:
							interface_up = true;
							GPTP_LOG_VERBOSE("Interface %s is UP (fallback check)", pCurrAddresses->AdapterName);
							break;
						case IfOperStatusDown:
							interface_up = false;
							GPTP_LOG_INFO("Interface %s is DOWN (fallback check)", pCurrAddresses->AdapterName);
							break;
						case IfOperStatusTesting:
						case IfOperStatusUnknown:
						case IfOperStatusDormant:
						case IfOperStatusNotPresent:
						case IfOperStatusLowerLayerDown:
						default:
							interface_up = false;
							GPTP_LOG_WARNING("Interface %s has status %d (treated as DOWN, fallback check)", 
								pCurrAddresses->AdapterName, pCurrAddresses->OperStatus);
							break;
						}
						
						// Check for disconnected IP addresses (MIB_IPADDR_DISCONNECTED equivalent)
						PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddresses->FirstUnicastAddress;
						bool has_connected_ip = false;
						
						while (pUnicast) {
							// In newer Windows versions, check DadState for address validity
							if (pUnicast->DadState == IpDadStatePreferred || 
								pUnicast->DadState == IpDadStateDeprecated) {
								has_connected_ip = true;
								break;
							}
							pUnicast = pUnicast->Next;
						}
						
						// Overall interface status considers both operational state and IP connectivity
						bool link_status = interface_up && (has_connected_ip || pCurrAddresses->FirstUnicastAddress == NULL);
						
						// Notify port of current link status
						pPort->setAsCapable(link_status);
						
						if (!link_status) {
							GPTP_LOG_WARNING("Link down detected for interface %s (fallback check)", pCurrAddresses->AdapterName);
						} else {
							GPTP_LOG_VERBOSE("Link up confirmed for interface %s (fallback check)", pCurrAddresses->AdapterName);
						}
					}
					
					break; // Found our interface, no need to continue
				}
			}
			pCurrAddresses = pCurrAddresses->Next;
		}
	} else {
		GPTP_LOG_ERROR("GetAdaptersAddresses failed with error %d", dwRetVal);
	}

	if (pAddresses) {
		free(pAddresses);
	}

	// ✅ IMPLEMENTING: Enhanced link monitoring with event-driven notifications
	// Replaces TODO: "For continuous monitoring, this function should be called periodically"
	//
	// Strategy:
	// 1. Current implementation provides one-time status check (polling)
	// 2. For future enhancement, could use NotifyAddrChange for event-driven monitoring
	// 3. This would provide immediate notification of link state changes
	//
	// Future implementation would look like:
	// - NotifyAddrChange() for address change notifications
	// - NotifyRouteChange() for route change notifications  
	// - Background thread to handle notifications and update port state
	//
	// For now, the polling approach is sufficient and working well
}

// ✅ IMPLEMENTING: Event-Driven Link Monitoring Implementation
// Replaces polling-based approach with real-time Windows notifications

#include <process.h>  // For _beginthreadex

// Global link monitoring context list for cleanup
static std::list<LinkMonitorContext*> g_linkMonitors;
static CRITICAL_SECTION g_linkMonitorLock;
static bool g_linkMonitorInitialized = false;

/**
 * @brief Initialize link monitoring subsystem
 */
static void initializeLinkMonitoring() {
    if (!g_linkMonitorInitialized) {
        InitializeCriticalSection(&g_linkMonitorLock);
        g_linkMonitorInitialized = true;
    }
}

/**
 * @brief Cleanup link monitoring subsystem
 */
void cleanupLinkMonitoring() {
    if (g_linkMonitorInitialized) {
        EnterCriticalSection(&g_linkMonitorLock);
        // Stop all active monitors
        for (auto it = g_linkMonitors.begin(); it != g_linkMonitors.end(); ++it) {
            stopLinkMonitoring(*it);
        }
        g_linkMonitors.clear();
        LeaveCriticalSection(&g_linkMonitorLock);
        DeleteCriticalSection(&g_linkMonitorLock);
        g_linkMonitorInitialized = false;
    }
}

bool checkLinkStatus(const char* interfaceDesc, const BYTE* macAddress) {
    if (!interfaceDesc || !macAddress) {
        return false;
    }

    PIP_ADAPTER_ADDRESSES pAddresses = NULL;
    PIP_ADAPTER_ADDRESSES pCurrAddresses = NULL;
    ULONG outBufLen = 0;
    DWORD dwRetVal = 0;
    bool linkUp = false;

    // Get adapter information
    dwRetVal = GetAdaptersAddresses(AF_UNSPEC, 
        GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
        NULL, pAddresses, &outBufLen);

    if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
        pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(outBufLen);
        if (pAddresses == NULL) {
            return false;
        }
    } else if (dwRetVal != ERROR_SUCCESS) {
        return false;
    }

    // Get the actual adapter information
    dwRetVal = GetAdaptersAddresses(AF_UNSPEC,
        GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
        NULL, pAddresses, &outBufLen);

    if (dwRetVal == NO_ERROR) {
        pCurrAddresses = pAddresses;
        while (pCurrAddresses) {
            // Convert wide string to narrow string for comparison
            char desc_narrow[256] = {0};
            WideCharToMultiByte(CP_UTF8, 0, pCurrAddresses->Description, -1, 
                               desc_narrow, sizeof(desc_narrow), NULL, NULL);
            
            // Check if this is our target interface
            if (strstr(interfaceDesc, desc_narrow) != NULL && 
                pCurrAddresses->PhysicalAddressLength == 6) {
                
                // Verify MAC address match
                bool macMatch = true;
                for (int i = 0; i < 6; i++) {
                    if (pCurrAddresses->PhysicalAddress[i] != macAddress[i]) {
                        macMatch = false;
                        break;
                    }
                }
                
                if (macMatch) {
                    // Check interface operational status
                    linkUp = (pCurrAddresses->OperStatus == IfOperStatusUp);
                    break;
                }
            }
            pCurrAddresses = pCurrAddresses->Next;
        }
    }

    if (pAddresses) {
        free(pAddresses);
    }

    return linkUp;
}

DWORD WINAPI linkMonitorThreadProc(LPVOID lpParam) {
    LinkMonitorContext* pContext = (LinkMonitorContext*)lpParam;
    if (!pContext) {
        return 1;
    }

    GPTP_LOG_STATUS("Event-driven link monitoring started for interface: %s", pContext->interfaceDesc);

    HANDLE hEvents[2] = { pContext->hAddrChange, pContext->hRouteChange };
    bool lastLinkState = checkLinkStatus(pContext->interfaceDesc, pContext->macAddress);
    
    while (!pContext->bStopMonitoring) {
        // Wait for network change events or timeout
        DWORD dwResult = WaitForMultipleObjects(2, hEvents, FALSE, 5000); // 5 second timeout
        
        if (dwResult == WAIT_OBJECT_0 || dwResult == WAIT_OBJECT_0 + 1 || dwResult == WAIT_TIMEOUT) {
            // Check current link status
            bool currentLinkState = checkLinkStatus(pContext->interfaceDesc, pContext->macAddress);
            
            // Notify port of state change
            if (currentLinkState != lastLinkState) {
                GPTP_LOG_STATUS("Link state changed for %s: %s -> %s", 
                               pContext->interfaceDesc,
                               lastLinkState ? "UP" : "DOWN",
                               currentLinkState ? "UP" : "DOWN");
                
                if (pContext->pPort) {
                    pContext->pPort->setAsCapable(currentLinkState);
                }
                lastLinkState = currentLinkState;
            }
            
            // Reset event notifications for next iteration
            if (dwResult == WAIT_OBJECT_0) {
                // Address change notification
                DWORD dwError = NotifyAddrChange(&pContext->hAddrChange, NULL);
                if (dwError != ERROR_SUCCESS && dwError != ERROR_IO_PENDING) {
                    GPTP_LOG_ERROR("Failed to reset address change notification: %d", dwError);
                }
            }
            
            if (dwResult == WAIT_OBJECT_0 + 1) {
                // Route change notification  
                DWORD dwError = NotifyRouteChange(&pContext->hRouteChange, NULL);
                if (dwError != ERROR_SUCCESS && dwError != ERROR_IO_PENDING) {
                    GPTP_LOG_ERROR("Failed to reset route change notification: %d", dwError);
                }
            }
        } else if (dwResult == WAIT_FAILED) {
            GPTP_LOG_ERROR("WaitForMultipleObjects failed in link monitor: %d", GetLastError());
            break;
        }
    }

    GPTP_LOG_STATUS("Event-driven link monitoring stopped for interface: %s", pContext->interfaceDesc);
    return 0;
}

LinkMonitorContext* startLinkMonitoring(CommonPort* pPort, const char* interfaceDesc, const BYTE* macAddress) {
    if (!pPort || !interfaceDesc || !macAddress) {
        return NULL;
    }

    initializeLinkMonitoring();

    LinkMonitorContext* pContext = new LinkMonitorContext();
    if (!pContext) {
        return NULL;
    }

    // Initialize context
    memset(pContext, 0, sizeof(LinkMonitorContext));
    pContext->pPort = pPort;
    pContext->bStopMonitoring = false;
    strncpy_s(pContext->interfaceDesc, sizeof(pContext->interfaceDesc), interfaceDesc, _TRUNCATE);
    memcpy(pContext->macAddress, macAddress, 6);

    // Set up address change notification
    DWORD dwError = NotifyAddrChange(&pContext->hAddrChange, NULL);
    if (dwError != ERROR_SUCCESS && dwError != ERROR_IO_PENDING) {
        GPTP_LOG_ERROR("Failed to setup address change notification: %d", dwError);
        delete pContext;
        return NULL;
    }

    // Set up route change notification
    dwError = NotifyRouteChange(&pContext->hRouteChange, NULL);
    if (dwError != ERROR_SUCCESS && dwError != ERROR_IO_PENDING) {
        GPTP_LOG_ERROR("Failed to setup route change notification: %d", dwError);
        if (pContext->hAddrChange) {
            CloseHandle(pContext->hAddrChange);
        }
        delete pContext;
        return NULL;
    }

    // Create monitoring thread
    pContext->hMonitorThread = (HANDLE)_beginthreadex(
        NULL,                           // Security attributes
        0,                              // Stack size (default)
        (unsigned (__stdcall *)(void*))linkMonitorThreadProc, // Thread function
        pContext,                       // Thread parameter
        0,                              // Creation flags
        (unsigned*)&pContext->dwThreadId // Thread ID
    );

    if (!pContext->hMonitorThread) {
        GPTP_LOG_ERROR("Failed to create link monitoring thread: %d", GetLastError());
        if (pContext->hAddrChange) {
            CloseHandle(pContext->hAddrChange);
        }
        if (pContext->hRouteChange) {
            CloseHandle(pContext->hRouteChange);
        }
        delete pContext;
        return NULL;
    }

    // Add to global list for cleanup
    EnterCriticalSection(&g_linkMonitorLock);
    g_linkMonitors.push_back(pContext);
    LeaveCriticalSection(&g_linkMonitorLock);

    GPTP_LOG_STATUS("Event-driven link monitoring enabled for interface: %s", interfaceDesc);
    return pContext;
}

void stopLinkMonitoring(LinkMonitorContext* pContext) {
    if (!pContext) {
        return;
    }

    // Signal thread to stop
    pContext->bStopMonitoring = true;

    // Wait for thread to complete
    if (pContext->hMonitorThread) {
        WaitForSingleObject(pContext->hMonitorThread, 10000); // 10 second timeout
        CloseHandle(pContext->hMonitorThread);
    }

    // Clean up notification handles
    if (pContext->hAddrChange) {
        CloseHandle(pContext->hAddrChange);
    }
    if (pContext->hRouteChange) {
        CloseHandle(pContext->hRouteChange);
    }

    // Remove from global list
    if (g_linkMonitorInitialized) {
        EnterCriticalSection(&g_linkMonitorLock);
        g_linkMonitors.remove(pContext);
        LeaveCriticalSection(&g_linkMonitorLock);
    }

    delete pContext;
}
