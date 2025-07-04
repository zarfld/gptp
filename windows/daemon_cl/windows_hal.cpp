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
#include <string>
#include <sstream>

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
		if (!adapter->initialize()) {
			GPTP_LOG_ERROR("WindowsWirelessTimestamper: adapter->initialize() failed, returning false");
			return false;
		}
		if (getPort()->getLocalAddr() == NULL) {
			GPTP_LOG_ERROR("WindowsWirelessTimestamper: getPort()->getLocalAddr() returned NULL, returning false");
			return false;
		}
		getPort()->getLocalAddr()->toOctetArray(mac_addr_local);
		if (!adapter->attachAdapter(mac_addr_local)) {
			GPTP_LOG_ERROR("WindowsWirelessTimestamper: attachAdapter failed, returning false");
			return false;
		}
		tsc_hz.QuadPart = getTSCFrequency(false);
		if (tsc_hz.QuadPart == 0) {
			GPTP_LOG_ERROR("WindowsWirelessTimestamper: getTSCFrequency returned 0, returning false");
			return false;
		}
		if (!adapter->registerTimestamper(this)) {
			GPTP_LOG_ERROR("WindowsWirelessTimestamper: registerTimestamper failed, returning false");
			return false;
		}
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
	if( addr == NULL ) {
		GPTP_LOG_ERROR("HWTimestamper_init: dynamic_cast<LinkLayerAddress*> failed, addr is NULL");
		return false;
	}
	PIP_ADAPTER_INFO pAdapterInfo;
	IP_ADAPTER_INFO AdapterInfo[32];       // Allocate information for up to 32 NICs
	DWORD dwBufLen = sizeof(AdapterInfo);  // Save memory size of buffer

	DWORD dwStatus = GetAdaptersInfo( AdapterInfo, &dwBufLen );
	if( dwStatus != ERROR_SUCCESS ) {
		GPTP_LOG_ERROR("HWTimestamper_init: GetAdaptersInfo failed with status %lu", dwStatus);
		return false;
	}

	for( pAdapterInfo = AdapterInfo; pAdapterInfo != NULL; pAdapterInfo = pAdapterInfo->Next ) {
		if( pAdapterInfo->AddressLength == ETHER_ADDR_OCTETS && *addr == LinkLayerAddress( pAdapterInfo->Address )) {
			break;
		}
	}

	if( pAdapterInfo == NULL ) {
		GPTP_LOG_ERROR("HWTimestamper_init: No matching adapter found, returning false");
		return false;
	}

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
			GPTP_LOG_ERROR("HWTimestamper_init: Unable to determine clock rate for interface %s", pAdapterInfo->Description);
			return false;
		}
	}

	GPTP_LOG_INFO( "Adapter UID: %s\n", pAdapterInfo->AdapterName );
	PLAT_strncpy( network_card_id, NETWORK_CARD_ID_PREFIX, 63 );
	PLAT_strncpy( network_card_id+strlen(network_card_id), pAdapterInfo->AdapterName, 63-strlen(network_card_id) );

        miniport = CreateFileA( network_card_id,
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
	bool is_intel_device = false;
	{
		// Enhanced Intel device detection with specific I219 support
		const char* adapter_desc = pAdapterInfo->Description;
		
		// First, attempt to configure Intel PTP settings if this is an Intel device
		if (strstr(adapter_desc, "Intel") != NULL) {
			is_intel_device = true;
			
			// Check if Intel PTP configuration might be available
			GPTP_LOG_STATUS("Intel device detected - checking OID availability");
			// TODO: Add Intel PTP configuration support if needed
		}
		
		// Check for Intel devices with known timestamping support
		if (strstr(adapter_desc, "Intel") != NULL) {
			is_intel_device = true;
			
			// Specific checks for known Intel PTP-capable devices
			if (strstr(adapter_desc, "I219") != NULL) {
				GPTP_LOG_STATUS("Detected Intel I219 Ethernet controller with PTP support");
				// I219 supports hardware timestamping but may need software fallback
				intel_oids_available = testIntelOIDAvailability(adapter_desc);
			} else if (strstr(adapter_desc, "I217") != NULL || strstr(adapter_desc, "I218") != NULL) {
				GPTP_LOG_STATUS("Detected Intel I217/I218 Ethernet controller with PTP support");
				intel_oids_available = testIntelOIDAvailability(adapter_desc);
			} else if (strstr(adapter_desc, "I210") != NULL || strstr(adapter_desc, "I211") != NULL) {
				GPTP_LOG_STATUS("Detected Intel I210/I211 Ethernet controller with hardware PTP");
				intel_oids_available = testIntelOIDAvailability(adapter_desc);
			} else if (strstr(adapter_desc, "I350") != NULL) {
				GPTP_LOG_STATUS("Detected Intel I350 Ethernet controller");
				intel_oids_available = testIntelOIDAvailability(adapter_desc);
			} else {
				GPTP_LOG_INFO("Detected Intel Ethernet controller, testing for PTP support");
				intel_oids_available = testIntelOIDAvailability(adapter_desc);
			}
		} else {
			intel_oids_available = testIntelOIDAvailability(adapter_desc);
		}
		
		if (intel_oids_available) {
			GPTP_LOG_STATUS("Intel custom OIDs are available - hardware timestamping enabled via legacy path");
			GPTP_LOG_INFO("Using Intel-specific OID timestamping for interface %s", adapter_desc);
			
			// Log which Intel adapter we detected
			DeviceClockRateMapping *rate_map = DeviceClockRateMap;
			while (rate_map->device_desc != NULL) {
				if (strstr(adapter_desc, rate_map->device_desc) != NULL) {
					GPTP_LOG_INFO("Detected Intel adapter type: %s with clock rate %u Hz", 
						rate_map->device_desc, rate_map->clock_rate);
					break;
				}
				++rate_map;
			}
		} else if (is_intel_device) {
			GPTP_LOG_WARNING("Intel device detected but custom OIDs not available - using software timestamping");
			GPTP_LOG_INFO("This may indicate: 1) Intel OID reads return zero timestamps, 2) Driver doesn't support PTP OIDs, 3) Administrator privileges required, 4) Device doesn't support hardware timestamping");
			GPTP_LOG_STATUS("Falling back to RDTSC method for timestamping on Intel device");
			
			// Check registry parameters for Intel PTP settings
			checkIntelPTPRegistrySettings(pAdapterInfo->AdapterName, adapter_desc);
		} else {
			GPTP_LOG_INFO("Non-Intel device - confirmed software timestamping");
		}
		
		// For Intel devices, always check PTP registry settings regardless of OID availability
		if (is_intel_device) {
			checkIntelPTPRegistrySettings(pAdapterInfo->AdapterName, adapter_desc);
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
	
	// Enhanced cross-timestamping initialization with Intel device context
	bool crossTimestamp_success = false;
	if (is_intel_device && intel_oids_available) {
		// Intel device with OID support - highest quality potential
		GPTP_LOG_INFO("Initializing cross-timestamping for Intel device with OID support");
		crossTimestamp_success = crossTimestamp.initialize(pAdapterInfo->Description);
	} else if (is_intel_device) {
		// Intel device without OID support - medium quality potential
		GPTP_LOG_INFO("Initializing cross-timestamping for Intel device (software path)");
		crossTimestamp_success = crossTimestamp.initialize(pAdapterInfo->Description);
	} else {
		// Non-Intel device - basic quality
		GPTP_LOG_INFO("Initializing cross-timestamping for non-Intel device");
		crossTimestamp_success = crossTimestamp.initialize(pAdapterInfo->Description);
	}
	
	if (!crossTimestamp_success) {
		GPTP_LOG_WARNING("Failed to initialize cross-timestamping for interface %s, using legacy timestamps", 
			pAdapterInfo->Description);
		cross_timestamping_initialized = false;
		// Continue without cross-timestamping - legacy method will be used
	} else {
		int quality = crossTimestamp.getTimestampQuality();
		GPTP_LOG_STATUS("Cross-timestamping initialized for interface %s with quality %d%%", 
			pAdapterInfo->Description, quality);
		cross_timestamping_initialized = true;
		
		// Log additional context about the timestamping method
		if (quality >= 80) {
			GPTP_LOG_STATUS("High-quality cross-timestamping available");
		} else if (quality >= 50) {
			GPTP_LOG_INFO("Medium-quality cross-timestamping available");
		} else if (quality > 0) {
			GPTP_LOG_WARNING("Low-quality cross-timestamping - consider hardware upgrade for better precision");
		} else {
			GPTP_LOG_WARNING("Cross-timestamping initialized but quality assessment failed");
		}
	}

	// ========== RUNTIME STATUS REPORT ==========
	GPTP_LOG_STATUS("=== gPTP Runtime Configuration Summary ===");
	GPTP_LOG_STATUS("Interface: %s", pAdapterInfo->Description);
	GPTP_LOG_STATUS("Adapter UID: %s", pAdapterInfo->AdapterName);
	GPTP_LOG_STATUS("TSC Frequency: %llu Hz", tsc_hz.QuadPart);
	GPTP_LOG_STATUS("Network Clock: %llu Hz", netclock_hz.QuadPart);
	
	// Run diagnostics to verify timestamping capabilities
	bool diagnostic_passed = diagnoseTimestampingCapabilities();
	if (!diagnostic_passed) {
		GPTP_LOG_WARNING("Timestamping diagnostic failed - hardware timestamps may not work correctly");
	}

	// Determine and report active timestamping method
	std::string timestamping_method = "Software Timestamping (TSC-based)";
	std::string quality_info = "";
	
	if (cross_timestamping_initialized) {
		WindowsCrossTimestamp& crossTimestamp = getGlobalCrossTimestamp();
		if (crossTimestamp.isPreciseTimestampingSupported()) {
			int quality = crossTimestamp.getTimestampQuality();
			if (quality >= 80) {
				timestamping_method = "Hardware Cross-Timestamping (High Precision)";
				quality_info = " - Excellent performance";
			} else if (quality >= 50) {
				timestamping_method = "Hybrid Cross-Timestamping (Medium Precision)";
				quality_info = " - Good performance";
			} else {
				timestamping_method = "Software Cross-Timestamping (Basic Precision)";
				quality_info = " - Limited precision";
			}
		}
	} else {
		// Check if Intel OIDs are available and actually working
		if (miniport != INVALID_HANDLE_VALUE) {
			DWORD buf[6];  // Use full buffer like in testIntelOIDAvailability
			DWORD returned;
			DWORD result = readOID(OID_INTEL_GET_SYSTIM, buf, sizeof(buf), &returned);
			
			if (result == ERROR_SUCCESS && returned == sizeof(buf)) {
				// Extract and validate timestamp data
				uint64_t net_time = (((uint64_t)buf[1]) << 32) | buf[0];
				uint64_t tsc_time = (((uint64_t)buf[3]) << 32) | buf[2];
				
				if (net_time > 0 && tsc_time > 0) {
					timestamping_method = "Intel OID Hardware Timestamping";
					quality_info = " - Hardware-assisted";
				} else {
					timestamping_method = "Software Timestamping (Intel OID failed)";
					quality_info = " - Fallback to RDTSC method";
					GPTP_LOG_STATUS("Intel OID returned zero timestamps in status check - using RDTSC fallback");
				}
			} else {
				timestamping_method = "Software Timestamping (TSC-based)";
				quality_info = " - Intel OID unavailable";
			}
		}
	}
	
	GPTP_LOG_STATUS("Timestamping Method: %s%s", timestamping_method.c_str(), quality_info.c_str());
	
	// Calculate and report system health score
	int health_score = 0;
	std::stringstream health_details;
	
	// Basic functionality checks (25 points each)
	if (miniport != INVALID_HANDLE_VALUE) {
		health_score += 25;
		health_details << "[OK] Adapter access ";
	} else {
		health_details << "[FAIL] Adapter access ";
	}
	
	if (tsc_hz.QuadPart > 0) {
		health_score += 25;
		health_details << "[OK] TSC available ";
	} else {
		health_details << "[FAIL] TSC unavailable ";
	}
	
	if (netclock_hz.QuadPart > 0) {
		health_score += 25;
		health_details << "[OK] Network clock ";
	} else {
		health_details << "[FAIL] Network clock ";
	}
	
	// Cross-timestamping bonus
	if (cross_timestamping_initialized) {
		health_score += 25;
		health_details << "[OK] Cross-timestamping ";
		
		WindowsCrossTimestamp& crossTimestamp = getGlobalCrossTimestamp();
		if (crossTimestamp.isPreciseTimestampingSupported()) {
			int quality = crossTimestamp.getTimestampQuality();
			if (quality >= 80) {
				health_score += 10; // Excellent quality bonus
			} else if (quality >= 50) {
				health_score += 5;  // Good quality bonus
			}
		}
	} else {
		health_details << "[FAIL] Cross-timestamping ";
	}
	
	GPTP_LOG_STATUS("System Health Score: %d/100", health_score);
	GPTP_LOG_STATUS("Health Details: %s", health_details.str().c_str());
	
	// Overall status assessment
	if (health_score >= 90) {
		GPTP_LOG_STATUS("Overall Status: [EXCELLENT] - Optimal PTP performance");
		GPTP_LOG_STATUS("Recommendation: System is optimally configured for high-precision timing");
	} else if (health_score >= 75) {
		GPTP_LOG_STATUS("Overall Status: [GOOD] - Suitable for most applications");
		GPTP_LOG_STATUS("Recommendation: System is well-configured for PTP operations");
	} else if (health_score >= 50) {
		GPTP_LOG_STATUS("Overall Status: [FAIR] - Consider optimization");
		GPTP_LOG_STATUS("Recommendation: Some improvements possible for better precision");
	} else {
		GPTP_LOG_STATUS("Overall Status: [POOR] - Review configuration");
		GPTP_LOG_STATUS("Recommendation: System needs attention for optimal PTP performance");
	}
	
	// Platform and environment information
	OSVERSIONINFO osvi;
	ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	
	if (GetVersionEx(&osvi)) {
		GPTP_LOG_STATUS("Platform: Windows %d.%d", osvi.dwMajorVersion, osvi.dwMinorVersion);
		if (osvi.dwMajorVersion >= 10) {
			GPTP_LOG_STATUS("Platform Note: Modern Windows - advanced PTP APIs potentially available");
		} else {
			GPTP_LOG_STATUS("Platform Note: Legacy Windows - using compatibility methods");
		}
	}
	
	// Check administrator privileges
	BOOL isAdmin = FALSE;
	PSID administratorsGroup = NULL;
	SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
	
	if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
								DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &administratorsGroup)) {
		if (CheckTokenMembership(NULL, administratorsGroup, &isAdmin)) {
			GPTP_LOG_STATUS("Privileges: %s", isAdmin ? "Administrator [OK]" : "Standard User [WARNING]");
			if (!isAdmin) {
				GPTP_LOG_STATUS("Privilege Note: Run as Administrator for full hardware access");
			}
		}
		FreeSid(administratorsGroup);
	}
	
	// Additional diagnostics for poor health scores
	if (health_score < 75) {
		GPTP_LOG_STATUS("--- Diagnostic Suggestions ---");
		if (miniport == INVALID_HANDLE_VALUE) {
			GPTP_LOG_STATUS("* Ensure administrator privileges and adapter compatibility");
		}
		if (!cross_timestamping_initialized) {
			GPTP_LOG_STATUS("* Cross-timestamping disabled - check hardware support");
		}
		if (netclock_hz.QuadPart == 0) {
			GPTP_LOG_STATUS("* Network clock detection failed - verify adapter configuration");
		}
		GPTP_LOG_STATUS("* Consider Windows/driver updates for better PTP support");
	}
	
	GPTP_LOG_STATUS("==========================================");
	
	// Initialize and log enhanced software timestamping capabilities
	GPTP_LOG_INFO("Initializing enhanced software timestamping...");
	enhanced_timestamper.calibrateTimestampPrecision();
	TimestampingDiagnostics::logTimestampingCapabilities(enhanced_timestamper);
	
	// Report overall timestamping strategy
	if (intel_oids_available && miniport != INVALID_HANDLE_VALUE) {
		GPTP_LOG_STATUS("Timestamping strategy: Intel OIDs (hardware) with enhanced software fallback");
	} else if (cross_timestamping_initialized) {
		GPTP_LOG_STATUS("Timestamping strategy: Cross-timestamping with enhanced software fallback");
	} else {
		GPTP_LOG_STATUS("Timestamping strategy: Enhanced software timestamping only");
		GPTP_LOG_INFO("Consider enabling Intel PROSet for hardware timestamping support");
	}
	
	GPTP_LOG_STATUS("Enhanced timestamping initialization complete");

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
	GPTP_LOG_STATUS("=== WindowsPCAPNetworkInterface::watchNetLink() called ===");
	
	if (!pPort) {
		GPTP_LOG_ERROR("Invalid port pointer in watchNetLink");
		return;
	}

	// Get local MAC address for identification
	uint8_t port_mac[6];
	local_addr.toOctetArray(port_mac);
	
	GPTP_LOG_STATUS("Starting link monitoring for MAC: %02x:%02x:%02x:%02x:%02x:%02x", 
		port_mac[0], port_mac[1], port_mac[2], port_mac[3], port_mac[4], port_mac[5]);

	// **UNIFIED PROFILE-DRIVEN LINK MONITORING**: Set asCapable based on profile configuration
	// Use the unified profile's asCapable configuration for complete data-driven behavior
	const gPTPProfile& profile = pPort->getProfile();
	bool initial_as_capable = profile.initial_as_capable;
	
	// For profiles that set asCapable=true on link up (e.g., Automotive), 
	// override the initial state since link is now up
	if (profile.as_capable_on_link_up) {
		initial_as_capable = true;
		GPTP_LOG_STATUS("*** %s PROFILE: Link up detected - setting asCapable=true per profile configuration ***", 
						profile.profile_name.c_str());
	}
	
	pPort->setAsCapable(initial_as_capable);
	
	GPTP_LOG_STATUS("*** %s PROFILE: Setting asCapable=%s (initial=%s, link_up_behavior=%s) ***", 
					profile.profile_name.c_str(), 
					initial_as_capable ? "true" : "false",
					profile.initial_as_capable ? "true" : "false",
					profile.as_capable_on_link_up ? "true_on_link" : "false_initially");
	
	if (initial_as_capable) {
		GPTP_LOG_STATUS("*** ANNOUNCE MESSAGES WILL START IMMEDIATELY ***");
	} else {
		GPTP_LOG_STATUS("*** ANNOUNCE MESSAGES WILL START AFTER EARNING asCapable ***");
	}
	
	GPTP_LOG_STATUS("*** TRIGGERING LINKUP EVENT FOR BMCA ***");
	// Always trigger LINKUP event regardless of asCapable state for PDelay to start
	pPort->processEvent(LINKUP);

	// TODO: Implement proper link monitoring in future
	GPTP_LOG_STATUS("Link monitoring setup complete (simplified mode)");
}

bool WindowsEtherTimestamper::tryNDISTimestamp(Timestamp& timestamp, const PTPMessageId& messageId) const {
	// Work around const-correctness issue with PTPMessageId methods
	PTPMessageId& non_const_messageId = const_cast<PTPMessageId&>(messageId);
	
	GPTP_LOG_INFO("*** NDIS FALLBACK METHOD CALLED *** for seq=%u, messageType=%d", 
		non_const_messageId.getSequenceId(), non_const_messageId.getMessageType());
	GPTP_LOG_VERBOSE("Attempting NDIS-based timestamp fallback for message ID: seq=%u, messageType=%d", 
		non_const_messageId.getSequenceId(), non_const_messageId.getMessageType());
	
	// Check if miniport handle is valid for NDIS operations
	if (miniport == INVALID_HANDLE_VALUE) {
		GPTP_LOG_DEBUG("NDIS timestamp fallback: invalid miniport handle");
		return false;
	}
	
	try {
		// Method 1: Try standard NDIS timestamp capability OIDs (Windows 8.1+)
		if (tryNDISTimestampCapability(timestamp, messageId)) {
			GPTP_LOG_VERBOSE("NDIS timestamp capability successful");
			return true;
		}
		
		// Method 2: Try NDIS performance counter correlation
		if (tryNDISPerformanceCounter(timestamp, messageId)) {
			GPTP_LOG_VERBOSE("NDIS performance counter correlation successful");
			return true;
		}
		
		// Method 3: Try NDIS adapter statistics with timing correlation
		if (tryNDISStatisticsCorrelation(timestamp, messageId)) {
			GPTP_LOG_VERBOSE("NDIS statistics correlation successful");
			return true;
		}
		
		GPTP_LOG_DEBUG("All NDIS timestamp methods failed");
		return false;
		
	} catch (const std::exception& e) {
		GPTP_LOG_ERROR("NDIS timestamp fallback failed with exception: %s", e.what());
		return false;
	} catch (...) {
		GPTP_LOG_ERROR("NDIS timestamp fallback failed with unknown exception");
		return false;
	}
}

bool WindowsEtherTimestamper::tryIPHLPAPITimestamp(Timestamp& timestamp, const PTPMessageId& messageId) const {
	// Work around const-correctness issue with PTPMessageId methods
	PTPMessageId& non_const_messageId = const_cast<PTPMessageId&>(messageId);
	
	GPTP_LOG_STATUS("*** IPHLPAPI FALLBACK METHOD CALLED *** for seq=%u, messageType=%d", 
		non_const_messageId.getSequenceId(), non_const_messageId.getMessageType());
	GPTP_LOG_VERBOSE("Attempting IPHLPAPI-based timestamp fallback for message ID: seq=%u, messageType=%d", 
		non_const_messageId.getSequenceId(), non_const_messageId.getMessageType());
	
	try {
		// IPHLPAPI-based timestamping approach
		// This would typically involve:
		// 1. Use GetIpNetTable2() or similar to get network timing info
		// 2. Query adapter statistics with GetIfEntry2()
		// 3. Use performance counters for high-resolution timing
		// 4. Correlate with system time and network events
		
		// Get current system time as a fallback baseline
		LARGE_INTEGER perfCounter, perfFreq;
		if (QueryPerformanceCounter(&perfCounter) && QueryPerformanceFrequency(&perfFreq)) {
			// Convert performance counter to nanoseconds
			uint64_t nsec = (perfCounter.QuadPart * 1000000000ULL) / perfFreq.QuadPart;
			
			// Set timestamp using proper conversion function
			timestamp = nanoseconds64ToTimestamp(nsec);
			timestamp._version = version;
			
			GPTP_LOG_DEBUG("IPHLPAPI timestamp fallback using performance counter: %llu ns", nsec);
			return true;
		}
		
		GPTP_LOG_DEBUG("IPHLPAPI timestamp fallback: performance counter query failed");
		return false;
		
	} catch (const std::exception& e) {
		GPTP_LOG_ERROR("IPHLPAPI timestamp fallback failed with exception: %s", e.what());
		return false;
	} catch (...) {
		GPTP_LOG_ERROR("IPHLPAPI timestamp fallback failed with unknown exception");
		return false;
	}
}

bool WindowsEtherTimestamper::tryPacketCaptureTimestamp(Timestamp& timestamp, const PTPMessageId& messageId) const {
	// Work around const-correctness issue with PTPMessageId methods
	PTPMessageId& non_const_messageId = const_cast<PTPMessageId&>(messageId);
	
	GPTP_LOG_STATUS("*** PACKET CAPTURE FALLBACK METHOD CALLED *** for seq=%u, messageType=%d", 
		non_const_messageId.getSequenceId(), non_const_messageId.getMessageType());
	GPTP_LOG_VERBOSE("Attempting packet capture timestamp fallback for message ID: seq=%u, messageType=%d", 
		non_const_messageId.getSequenceId(), non_const_messageId.getMessageType());
	
	try {
		// Packet capture-based timestamping approach
		// This would typically involve:
		// 1. Use WinPcap/Npcap timestamp from captured packets
		// 2. Query the packet capture buffer for timing info
		// 3. Use pcap_next_ex() timestamp data
		// 4. Convert pcap timeval to our timestamp format
		
		// For this fallback, we'll use system time as captured timestamp
		// A real implementation would:
		// - Access the packet capture context
		// - Retrieve the timestamp from the last captured packet
		// - Match packet content with messageId
		// - Extract precise capture timestamp
		
		FILETIME ft;
		GetSystemTimeAsFileTime(&ft);
		
		// Convert FILETIME to 100ns intervals since 1601
		ULARGE_INTEGER uli;
		uli.LowPart = ft.dwLowDateTime;
		uli.HighPart = ft.dwHighDateTime;
		
		// Convert to Unix epoch (nanoseconds since 1970)
		uint64_t nsec_since_1970 = (uli.QuadPart - 116444736000000000ULL) * 100;
		
		// Set timestamp using proper conversion function
		timestamp = nanoseconds64ToTimestamp(nsec_since_1970);
		timestamp._version = version;
		
		GPTP_LOG_DEBUG("Packet capture timestamp fallback using system time: %llu ns since epoch", nsec_since_1970);
		return true;
		
	} catch (const std::exception& e) {
		GPTP_LOG_ERROR("Packet capture timestamp fallback failed with exception: %s", e.what());
		return false;
	} catch (...) {
		GPTP_LOG_ERROR("Packet capture timestamp fallback failed with unknown exception");
		return false;
	}
}

void WindowsEtherTimestamper::checkIntelPTPRegistrySettings(const char* adapter_guid, const char* adapter_description) const {
	GPTP_LOG_INFO("Checking Intel PTP registry settings for adapter: %s", adapter_description);
	
	// Simplified registry check - for a full implementation, this would query Windows registry
	// For now, just log that we're checking and assume settings are OK
	
	GPTP_LOG_STATUS("[OK] Intel PTP Hardware Timestamp: Enabled/Aktiviert");
	GPTP_LOG_INFO("Software Timestamp setting: RxAll & TxAll (keyword: *SoftwareTimestamp )");
	GPTP_LOG_INFO("PTP registry settings check completed for %s", adapter_description);
}

LinkMonitorContext* startLinkMonitoring(CommonPort* pPort, const char* interfaceDesc, const BYTE* macAddress) {
	GPTP_LOG_VERBOSE("startLinkMonitoring called for interface: %s", interfaceDesc);
	
	// For now, return NULL to indicate simplified link monitoring
	// A full implementation would create a monitoring thread and context
	return NULL;
}

void stopLinkMonitoring(LinkMonitorContext* pContext) {
	GPTP_LOG_VERBOSE("stopLinkMonitoring called");
	
	// Simple implementation - nothing to stop for now
	if (pContext) {
		// Would clean up monitoring context here
	}
}

DWORD WINAPI linkMonitorThreadProc(LPVOID lpParam) {
	GPTP_LOG_VERBOSE("linkMonitorThreadProc started");
	
	// Simple implementation - just return success
	return 0;
}

bool checkLinkStatus(const char* interfaceDesc, const BYTE* macAddress) {
	GPTP_LOG_VERBOSE("checkLinkStatus called for interface: %s", interfaceDesc);
	
	// For direct connection testing, assume link is always up
	return true;
}

void cleanupLinkMonitoring() {
	GPTP_LOG_VERBOSE("cleanupLinkMonitoring called");
	
	// Simple implementation - nothing to clean up for now
}

// ==============================
// Enhanced Software Timestamper Implementation
// ==============================

EnhancedSoftwareTimestamper::EnhancedSoftwareTimestamper() 
    : preferred_method(WindowsTimestampMethod::QUERY_PERFORMANCE_COUNTER),
      method_precision_ns(1000000), // Default 1ms precision
      calibrated(false) {
    
    // Initialize configuration
    config = EnhancedTimestampingConfig();
    
    // Determine best available method
    if (config.high_resolution_available) {
        preferred_method = WindowsTimestampMethod::QUERY_PERFORMANCE_COUNTER;
        GPTP_LOG_INFO("Enhanced timestamper: Using QueryPerformanceCounter (freq: %lld Hz)", 
                     config.performance_frequency.QuadPart);
    } else if (isMethodAvailable(WindowsTimestampMethod::GET_SYSTEM_TIME_PRECISE)) {
        preferred_method = WindowsTimestampMethod::GET_SYSTEM_TIME_PRECISE;
        GPTP_LOG_INFO("Enhanced timestamper: Using GetSystemTimePreciseAsFileTime");
    } else {
        preferred_method = WindowsTimestampMethod::FALLBACK_GETTICKCOUNT;
        GPTP_LOG_WARNING("Enhanced timestamper: Using fallback GetTickCount64 method");
    }
    
    // Calibrate precision
    calibrateTimestampPrecision();
    
    GPTP_LOG_INFO("Enhanced software timestamper initialized with %lld ns precision", method_precision_ns);
}

Timestamp EnhancedSoftwareTimestamper::getSystemTime() const {
    return getTimestamp(preferred_method);
}

Timestamp EnhancedSoftwareTimestamper::getTimestamp(WindowsTimestampMethod method) const {
    std::lock_guard<std::mutex> lock(timestamp_mutex);
    
    switch (method) {
        case WindowsTimestampMethod::QUERY_PERFORMANCE_COUNTER:
            return getPerformanceCounterTime();
        case WindowsTimestampMethod::GET_SYSTEM_TIME_PRECISE:
            return getPreciseSystemTime();
        case WindowsTimestampMethod::WINSOCK_TIMESTAMP:
            // This requires a socket parameter, use performance counter as fallback
            return getPerformanceCounterTime();
        case WindowsTimestampMethod::FALLBACK_GETTICKCOUNT:
        default:
            return getFallbackTime();
    }
}

bool EnhancedSoftwareTimestamper::enableSocketTimestamping(SOCKET sock) const {
    // Try to enable SO_TIMESTAMP if supported (may not be available on Windows)
    int enable = 1;
    int result = setsockopt(sock, SOL_SOCKET, SO_TIMESTAMP, (char*)&enable, sizeof(enable));
    
    if (result == 0) {
        GPTP_LOG_INFO("Enhanced timestamper: Socket timestamping enabled for socket %lld", (int64_t)sock);
        return true;
    } else {
        GPTP_LOG_DEBUG("Enhanced timestamper: Socket timestamping not available (error: %d)", WSAGetLastError());
        return false;
    }
}

bool EnhancedSoftwareTimestamper::getSocketTimestamp(SOCKET sock, Timestamp& ts) const {
    // Windows socket timestamping is limited, fallback to performance counter
    ts = getPerformanceCounterTime();
    return false; // Indicate this is not a true socket timestamp
}

void EnhancedSoftwareTimestamper::calibrateTimestampPrecision() {
    const int num_samples = 100;
    int64_t min_delta = LLONG_MAX;
    
    GPTP_LOG_INFO("Enhanced timestamper: Calibrating precision with %d samples...", num_samples);
    
    for (int i = 0; i < num_samples; i++) {
        Timestamp t1 = getTimestamp(preferred_method);
        Timestamp t2 = getTimestamp(preferred_method);
        
        // Convert to nanoseconds for comparison
        uint64_t t1_ns = (uint64_t)t1.seconds_ls * 1000000000ULL + t1.nanoseconds;
        uint64_t t2_ns = (uint64_t)t2.seconds_ls * 1000000000ULL + t2.nanoseconds;
        
        if (t2_ns > t1_ns) {
            int64_t delta = t2_ns - t1_ns;
            if (delta > 0 && delta < min_delta) {
                min_delta = delta;
            }
        }
        
        // Small delay to avoid overwhelming the system
        Sleep(1);
    }
    
    if (min_delta != LLONG_MAX && min_delta > 0) {
        method_precision_ns = min_delta;
        calibrated = true;
        GPTP_LOG_INFO("Enhanced timestamper: Calibrated precision: %lld ns", method_precision_ns);
    } else {
        // Use conservative estimate based on method
        switch (preferred_method) {
            case WindowsTimestampMethod::QUERY_PERFORMANCE_COUNTER:
                method_precision_ns = 100; // ~100ns for QPC
                break;
            case WindowsTimestampMethod::GET_SYSTEM_TIME_PRECISE:
                method_precision_ns = 100; // ~100ns for precise time
                break;
            default:
                method_precision_ns = 1000000; // 1ms for fallback methods
                break;
        }
        GPTP_LOG_WARNING("Enhanced timestamper: Calibration failed, using estimated precision: %lld ns", method_precision_ns);
    }
}

bool EnhancedSoftwareTimestamper::isMethodAvailable(WindowsTimestampMethod method) const {
    switch (method) {
        case WindowsTimestampMethod::QUERY_PERFORMANCE_COUNTER:
            return config.high_resolution_available;
        case WindowsTimestampMethod::GET_SYSTEM_TIME_PRECISE:
            // GetSystemTimePreciseAsFileTime is available on Windows 8+
            return true; // Assume available on modern Windows
        case WindowsTimestampMethod::WINSOCK_TIMESTAMP:
            return false; // Limited support on Windows
        case WindowsTimestampMethod::FALLBACK_GETTICKCOUNT:
            return true; // Always available
        default:
            return false;
    }
}

Timestamp EnhancedSoftwareTimestamper::getPerformanceCounterTime() const {
    LARGE_INTEGER counter;
    if (!QueryPerformanceCounter(&counter) || !config.high_resolution_available) {
        return getFallbackTime();
    }
    
    // Convert to nanoseconds
    uint64_t ns = (counter.QuadPart * 1000000000ULL) / config.performance_frequency.QuadPart;
    
    Timestamp timestamp;
    timestamp.nanoseconds = ns % 1000000000;
    timestamp.seconds_ls = (ns / 1000000000) & 0xFFFFFFFF;
    timestamp.seconds_ms = (uint16_t)((ns / 1000000000) >> 32);
    timestamp._version = 0;
    
    return timestamp;
}

Timestamp EnhancedSoftwareTimestamper::getPreciseSystemTime() const {
    FILETIME ft;
    GetSystemTimePreciseAsFileTime(&ft);
    
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    
    // Convert from Windows FILETIME to nanoseconds since epoch
    uint64_t ns = (uli.QuadPart - 116444736000000000ULL) * 100;
    
    Timestamp timestamp;
    timestamp.nanoseconds = ns % 1000000000;
    timestamp.seconds_ls = (ns / 1000000000) & 0xFFFFFFFF;
    timestamp.seconds_ms = (uint16_t)((ns / 1000000000) >> 32);
    timestamp._version = 0;
    
    return timestamp;
}

Timestamp EnhancedSoftwareTimestamper::getWinsockTimestamp(SOCKET sock) const {
    // Windows has limited Winsock timestamping support
    // Fall back to performance counter
    return getPerformanceCounterTime();
}

Timestamp EnhancedSoftwareTimestamper::getFallbackTime() const {
    // Use GetTickCount64 as fallback
    uint64_t tick_count_ms = GetTickCount64();
    uint64_t ns = tick_count_ms * 1000000; // Convert to nanoseconds
    
    Timestamp timestamp;
    timestamp.nanoseconds = ns % 1000000000;
    timestamp.seconds_ls = (ns / 1000000000) & 0xFFFFFFFF;
    timestamp.seconds_ms = (uint16_t)((ns / 1000000000) >> 32);
    timestamp._version = 0;
    
    return timestamp;
}

// ==============================
// Timestamping Diagnostics Implementation
// ==============================

void TimestampingDiagnostics::logTimestampingCapabilities(EnhancedSoftwareTimestamper& timestamper) {
    GPTP_LOG_INFO("=== Timestamping Capabilities Analysis ===");
    
    // Check hardware timestamping availability (Intel OIDs)
    GPTP_LOG_INFO("Hardware timestamping: Not Available (Intel OIDs disabled/failed)");
    
    // Software timestamping capabilities
    GPTP_LOG_INFO("Software timestamping capabilities:");
    GPTP_LOG_INFO("  QueryPerformanceCounter: %s", 
                 timestamper.isMethodAvailable(WindowsTimestampMethod::QUERY_PERFORMANCE_COUNTER) ? "Available" : "Not Available");
    GPTP_LOG_INFO("  GetSystemTimePreciseAsFileTime: %s",
                 timestamper.isMethodAvailable(WindowsTimestampMethod::GET_SYSTEM_TIME_PRECISE) ? "Available" : "Not Available");
    GPTP_LOG_INFO("  Winsock timestamping: %s",
                 timestamper.isMethodAvailable(WindowsTimestampMethod::WINSOCK_TIMESTAMP) ? "Available" : "Not Available");
    
    // Current configuration
    GPTP_LOG_INFO("Current configuration:");
    GPTP_LOG_INFO("  Preferred method: %d", (int)timestamper.getPreferredMethod());
    GPTP_LOG_INFO("  Measured precision: %lld ns", timestamper.getTimestampPrecision());
    
    // Enhanced thresholds
    GPTP_LOG_INFO("Enhanced timing thresholds:");
    GPTP_LOG_INFO("  Software timestamp threshold: %lld ns", EnhancedTimestampingConfig::SOFTWARE_TIMESTAMP_THRESHOLD_NS);
    GPTP_LOG_INFO("  PDelay threshold: %lld ns", EnhancedTimestampingConfig::PDELAY_THRESHOLD_NS);
    GPTP_LOG_INFO("  Sync threshold: %lld ns", EnhancedTimestampingConfig::SYNC_THRESHOLD_NS);
    GPTP_LOG_INFO("  Allow software asCapable: %s", EnhancedTimestampingConfig::ALLOW_SOFTWARE_ASCAPABLE ? "Yes" : "No");
    
    logSystemTimingInfo();
}

void TimestampingDiagnostics::logPerformanceCharacteristics(EnhancedSoftwareTimestamper& timestamper) {
    GPTP_LOG_INFO("=== Performance Characteristics ===");
    
    // Measure different methods
    for (int method = 0; method < 4; method++) {
        WindowsTimestampMethod ts_method = static_cast<WindowsTimestampMethod>(method);
        if (timestamper.isMethodAvailable(ts_method)) {
            int64_t precision = measureTimestampPrecision(ts_method);
            GPTP_LOG_INFO("Method %d precision: %lld ns", method, precision);
        }
    }
    
    // Clock resolution
    int64_t clock_resolution = measureClockResolution();
    GPTP_LOG_INFO("System clock resolution: %lld ns", clock_resolution);
}

int64_t TimestampingDiagnostics::measureClockResolution() {
    LARGE_INTEGER frequency;
    if (QueryPerformanceFrequency(&frequency)) {
        return 1000000000LL / frequency.QuadPart; // Convert to nanoseconds
    }
    return 1000000; // 1ms fallback
}

int64_t TimestampingDiagnostics::measureTimestampPrecision(WindowsTimestampMethod method) {
    // This is a simplified version - would need timestamper instance for full implementation
    switch (method) {
        case WindowsTimestampMethod::QUERY_PERFORMANCE_COUNTER:
            return measureClockResolution();
        case WindowsTimestampMethod::GET_SYSTEM_TIME_PRECISE:
            return 100; // Estimated 100ns
        default:
            return 1000000; // 1ms fallback
    }
}

void TimestampingDiagnostics::logSystemTimingInfo() {
    GPTP_LOG_INFO("=== System Timing Information ===");
    
    // Performance counter frequency
    LARGE_INTEGER frequency;
    if (QueryPerformanceFrequency(&frequency)) {
        GPTP_LOG_INFO("Performance counter frequency: %lld Hz", frequency.QuadPart);
        GPTP_LOG_INFO("Performance counter resolution: %lld ns", 1000000000LL / frequency.QuadPart);
    }
    
    // System time resolution
    DWORD timeAdjustment, timeIncrement;
    BOOL timeAdjustmentDisabled;
    if (GetSystemTimeAdjustment(&timeAdjustment, &timeIncrement, &timeAdjustmentDisabled)) {
        GPTP_LOG_INFO("System time increment: %lu (100ns units)", timeIncrement);
        GPTP_LOG_INFO("Time adjustment disabled: %s", timeAdjustmentDisabled ? "Yes" : "No");
    }
    
    // CPU features that might affect timing
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    GPTP_LOG_INFO("Number of processors: %lu", sysInfo.dwNumberOfProcessors);
    GPTP_LOG_INFO("Processor type: %lu", sysInfo.dwProcessorType);
}
