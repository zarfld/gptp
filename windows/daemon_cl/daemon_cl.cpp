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

#include <winsock2.h>
#include <Windows.h>
#include <winnt.h>
#include "ieee1588.hpp"
#include "avbts_clock.hpp"
#include "avbts_osnet.hpp"
#include "avbts_oslock.hpp"
#include "windows_hal.hpp"
#include "avbts_message.hpp"
#include "gptp_cfg.hpp"
#include "watchdog.hpp"
#include <tchar.h>
#include <iphlpapi.h>
#include <cmath>

#include <ether_port.hpp>
#include <wireless_port.hpp>
#include <intel_wireless.hpp>

#include <avbts_osthread.hpp>
#include <avbts_oscondition.hpp>
#include <avbts_ostimer.hpp>

#include "windows_hal.hpp"
#include "IPCListener.hpp"
#include "tsc.hpp"
#include "packet.hpp"

/* Generic PCH delays */
#define PHY_DELAY_GB_TX_PCH 7750  //1G delay
#define PHY_DELAY_GB_RX_PCH 7750  //1G delay
#define PHY_DELAY_MB_TX_PCH 27500 //100M delay
#define PHY_DELAY_MB_RX_PCH 27500 //100M delay

/* I210 delays */
#define PHY_DELAY_GB_TX_I210 184  //1G delay
#define PHY_DELAY_GB_RX_I210 382  //1G delay
#define PHY_DELAY_MB_TX_I210 1044 //100M delay
#define PHY_DELAY_MB_RX_I210 2133 //100M delay

#define MACSTR_LENGTH 17

uint32_t findLinkSpeed(LinkLayerAddress *local_addr);

static bool exit_flag;

// Enhanced debugging support
extern void enablePacketReceptionDebug(bool enable);
static bool debug_packet_reception = false;

void print_usage( char *arg0 ) {
	fprintf( stderr,
		"%s "
		"[-R <priority 1>] [-debug-packets] [-Milan] <network interface>\n"
		"where <network interface> is a MAC address entered as xx-xx-xx-xx-xx-xx\n"
		"Options:\n"
		"  -R <priority>     Set priority1 value\n"
		"  -debug-packets    Enable enhanced packet reception debugging\n"
		"  -Milan            Enable Milan Baseline Interoperability Profile\n",
		arg0 );
}

BOOL WINAPI ctrl_handler( DWORD ctrl_type ) {
	bool ret;
	if( ctrl_type == CTRL_C_EVENT ) {
		exit_flag = true;
		ret = true;
	} else {
		ret = false;
	}
	return ret;
}

int watchdog_setup()
{
	WindowsWatchdogHandler *watchdog = new WindowsWatchdogHandler();
	int watchdog_result;
	long unsigned int watchdog_interval;
	
	watchdog_interval = watchdog->getWindowsWatchdogInterval(&watchdog_result);
	if (watchdog_result) {
		GPTP_LOG_INFO("Watchdog interval read from configuration: %lu us", watchdog_interval);
		watchdog->update_interval = watchdog_interval / 2;
		GPTP_LOG_STATUS("Starting Windows watchdog handler (Update every: %lu us)", watchdog->update_interval);
		
		if (!watchdog->startWatchdog()) {
			GPTP_LOG_ERROR("Failed to start Windows watchdog");
			delete watchdog;
			return -1;
		}
		
		return 0;
	} else {
		GPTP_LOG_INFO("Windows watchdog disabled");
		delete watchdog;
		return 0;
	}
}

int parseMacAddr( _TCHAR *macstr, uint8_t *octet_string ) {
	int i;
	_TCHAR *cur = macstr;

	if( strnlen_s( macstr, MACSTR_LENGTH ) != 17 ) return -1;

	for( i = 0; i < ETHER_ADDR_OCTETS; ++i ) {
		octet_string[i] = strtol( cur, &cur, 16 ) & 0xFF;
		++cur;
	}

	return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
	PortInit_t portInit;
	
	// Initialize Milan profile configuration with defaults first
	portInit.milan_config.milan_profile = false;
	portInit.milan_config.max_convergence_time_ms = 100;
	portInit.milan_config.max_sync_jitter_ns = 1000;
	portInit.milan_config.max_path_delay_variation_ns = 10000;
	portInit.milan_config.stream_aware_bmca = false;
	portInit.milan_config.redundant_gm_support = false;
	portInit.milan_config.milan_sync_interval_log = -3;  // 125ms
	portInit.milan_config.milan_announce_interval_log = 0; // 1s
	portInit.milan_config.milan_pdelay_interval_log = 0;  // 1s

	phy_delay_map_t ether_phy_delay;
	ether_phy_delay[LINKSPEED_1G].set_delay
	(PHY_DELAY_GB_TX_PCH, PHY_DELAY_GB_RX_PCH);
	ether_phy_delay[LINKSPEED_100MB].set_delay
	(PHY_DELAY_MB_TX_PCH, PHY_DELAY_MB_RX_PCH);


	portInit.clock = NULL;
	portInit.index = 1;
	portInit.timestamper = NULL;
	portInit.net_label = NULL;
	portInit.automotive_profile = false;
	portInit.isGM = false;
	portInit.testMode = false;
	portInit.initialLogSyncInterval = LOG2_INTERVAL_INVALID;
	portInit.initialLogPdelayReqInterval = LOG2_INTERVAL_INVALID;
	portInit.operLogPdelayReqInterval = LOG2_INTERVAL_INVALID;
	portInit.operLogSyncInterval = LOG2_INTERVAL_INVALID;
	portInit.condition_factory = NULL;
	portInit.thread_factory = NULL;
	portInit.timer_factory = NULL;
	portInit.lock_factory = NULL;
	portInit.neighborPropDelayThreshold =
		CommonPort::NEIGHBOR_PROP_DELAY_THRESH;

	bool syntonize = false;
	bool wireless = false;
	uint8_t priority1 = 248;
	int i;
	int phy_delays[4] =	{ -1, -1, -1, -1 };
	uint8_t addr_ostr[ETHER_ADDR_OCTETS];

	// Register default network interface
	WindowsPCAPNetworkInterfaceFactory *default_factory = new WindowsPCAPNetworkInterfaceFactory();
	OSNetworkInterfaceFactory::registerFactory( factory_name_t( "default" ), default_factory );

	// Create thread, lock, timer, timerq factories
	portInit.thread_factory = new WindowsThreadFactory();
	portInit.lock_factory = new WindowsLockFactory();
	portInit.timer_factory = new WindowsTimerFactory();
	portInit.condition_factory = new WindowsConditionFactory();
	WindowsNamedPipeIPC *ipc = new WindowsNamedPipeIPC();
	WindowsTimerQueueFactory *timerq_factory = new WindowsTimerQueueFactory();
	CommonPort *port;
	WindowsWirelessAdapter *wl_adapter = NULL;

	if( !ipc->init() ) {
		delete ipc;
		ipc = NULL;
	}

	// If there are no arguments, output usage
	if (1 == argc) {
		print_usage(argv[0]);
		return -1;
	}


	/* Process optional arguments */
	for( i = 1; i < argc; ++i ) {
		if (ispunct(argv[i][0]))
		{
			if (toupper(argv[i][1]) == 'H') {
				print_usage(argv[0]);
				return -1;
			}
			if (strcmp(argv[i], "-debug-packets") == 0) {
				debug_packet_reception = true;
				enablePacketReceptionDebug(true);
				printf("Enhanced packet reception debugging enabled\n");
			}
			else if (strcmp(argv[i], "-Milan") == 0) {
				portInit.milan_config.milan_profile = true;
				printf("Milan Baseline Interoperability Profile enabled\n");
				printf("  - Max convergence time: %dms\n", portInit.milan_config.max_convergence_time_ms);
				printf("  - Sync interval: %.3fms\n", pow(2.0, portInit.milan_config.milan_sync_interval_log) * 1000.0);
				printf("  - Enhanced BMCA with fast convergence\n");
			}
			else if (toupper(argv[i][1]) == 'W')
			{
				wireless = true;
			}
			else if (toupper(argv[i][1]) == 'R') {
				if (i + 1 >= argc) {
					printf("Priority 1 value must be specified on "
						"command line, using default value\n");
				}
				else {
					unsigned long tmp = strtoul(argv[i + 1], NULL, 0); ++i;
					if (tmp > 255) {
						printf("Invalid priority 1 value, using "
							"default value\n");
					}
					else {
						priority1 = (uint8_t)tmp;
					}
				}
			}
		} else
		{
			break;
		}
	}

	// Parse local HW MAC address
	if (i < argc)
	{
		parseMacAddr(argv[i++], addr_ostr);
		portInit.net_label = new LinkLayerAddress(addr_ostr);
	} else
	{
		printf("Local hardware MAC address required");
		return -1;
	}

	if( wireless )
	{
		if (i < argc)
		{
			parseMacAddr(argv[i++], addr_ostr);
			portInit.virtual_label = new LinkLayerAddress(addr_ostr);
		} else
		{
			printf("Wireless operation requires local virtual MAC address");
			return -1;
		}
	}

	if (!wireless)
	{
		// Create HWTimestamper object
		portInit.timestamper = new WindowsEtherTimestamper();
	} else
	{
		portInit.timestamper = new WindowsWirelessTimestamper();
		(static_cast<WindowsWirelessTimestamper *> (portInit.timestamper))->setAdapter(new IntelWirelessAdapter());
	}

	// Create Clock object with configuration support
	// Check for configuration file and read clock quality parameters
	bool use_config_file = false;
	std::string config_file_path = "gptp_cfg.ini";
	unsigned char config_clockClass = 248;
	unsigned char config_clockAccuracy = 0x22;
	uint16_t config_offsetScaledLogVariance = 0x436A;
	std::string config_profile = "standard";
	
	// Check if configuration file exists
	DWORD file_attr = GetFileAttributesA(config_file_path.c_str());
	if (file_attr != INVALID_FILE_ATTRIBUTES && !(file_attr & FILE_ATTRIBUTE_DIRECTORY)) {
		use_config_file = true;
		
		GptpIniParser iniParser(config_file_path);
		if (iniParser.parserError() < 0) {
			GPTP_LOG_ERROR("Cannot parse ini file. Aborting file reading, using defaults.");
			use_config_file = false;
		} else {
			GPTP_LOG_INFO("Reading configuration from %s", config_file_path.c_str());
			priority1 = iniParser.getPriority1();
			config_clockClass = iniParser.getClockClass();
			config_clockAccuracy = iniParser.getClockAccuracy();
			config_offsetScaledLogVariance = iniParser.getOffsetScaledLogVariance();
			config_profile = iniParser.getProfile();
			
			GPTP_LOG_INFO("priority1 = %d", priority1);
			GPTP_LOG_INFO("clockClass = %d", config_clockClass);
			GPTP_LOG_INFO("clockAccuracy = 0x%02X", config_clockAccuracy);
			GPTP_LOG_INFO("offsetScaledLogVariance = 0x%04X", config_offsetScaledLogVariance);
			GPTP_LOG_INFO("profile = %s", config_profile.c_str());
			
			// Override profile settings if Milan or automotive specified
			if (config_profile == "milan" || portInit.milan_config.milan_profile) {
				portInit.milan_config.milan_profile = true;
				GPTP_LOG_INFO("Milan profile enabled via configuration");
			} else if (config_profile == "automotive" || portInit.automotive_profile) {
				portInit.automotive_profile = true;
				GPTP_LOG_INFO("Automotive profile enabled via configuration");
			}
		}
	} else {
		GPTP_LOG_INFO("Configuration file %s not found, using default values", config_file_path.c_str());
	}
	
	portInit.clock = new IEEE1588Clock(false, false, priority1, timerq_factory, ipc, portInit.lock_factory);  // Do not force slave
	
	// Configure clock quality based on profile and configuration
	if (use_config_file) {
		// Apply configuration file settings directly
		ClockQuality quality;
		quality.cq_class = config_clockClass;
		quality.clockAccuracy = config_clockAccuracy;
		quality.offsetScaledLogVariance = config_offsetScaledLogVariance;
		portInit.clock->setClockQuality(quality);
		
		GPTP_LOG_INFO("Clock quality configured from file: class=%d, accuracy=0x%02X, variance=0x%04X",
			quality.cq_class, quality.clockAccuracy, quality.offsetScaledLogVariance);
	} else {
		// Apply profile-specific clock quality
		portInit.clock->setProfileClockQuality(portInit.milan_config.milan_profile, portInit.automotive_profile);
	}

	if (!wireless)
	{
		// Create Port Object linked to clock and low level
		portInit.phy_delay = &ether_phy_delay;
		EtherPort *eport = new EtherPort(&portInit);
		eport->setLinkSpeed( findLinkSpeed( static_cast <LinkLayerAddress *> ( portInit.net_label )));
		port = eport;
		if (!eport->init_port()) {
			printf("Failed to initialize port\n");
			return -1;
		}
		port->processEvent(POWERUP);
	} else
	{
		if (i < argc)
		{
			parseMacAddr(argv[i++], addr_ostr);
			LinkLayerAddress peer_addr(addr_ostr);
			port = new WirelessPort(&portInit, peer_addr);
			(static_cast <WirelessTimestamper *> (portInit.timestamper))->setPort( static_cast<WirelessPort *> ( port ));
			if (!port->init_port()) {
				printf("Failed to initialize port\n");
				return -1;
			}
			port->processEvent(POWERUP);
		} else
		{
			printf("Wireless operation requires remote MAC address");
			return -1;
		}
	}

	// Setup and start watchdog monitoring
	if (watchdog_setup() != 0) {
		GPTP_LOG_ERROR("Failed to setup watchdog, continuing without watchdog support");
	}

	// Wait for Ctrl-C
	if( !SetConsoleCtrlHandler( ctrl_handler, true )) {
		printf( "Unable to register Ctrl-C handler\n" );
		return -1;
	}

	while( !exit_flag ) Sleep( 1200 );

	// Cleanup link monitoring before exiting
	cleanupLinkMonitoring();

	delete( ipc );

	return 0;
}

#define WIN_LINKSPEED_MULT (1000/*1 Kbit*/)

uint32_t findLinkSpeed( LinkLayerAddress *local_addr )
{
	ULONG ret_sz;
	char *buffer;
	PIP_ADAPTER_ADDRESSES pAddr;
	ULONG err;
	uint32_t ret;

	buffer = (char *) malloc((size_t)15000);
	ret_sz = 15000;
	pAddr = (PIP_ADAPTER_ADDRESSES)buffer;
	err = GetAdaptersAddresses( AF_UNSPEC, 0, NULL, pAddr, &ret_sz );
	
	if (err != ERROR_SUCCESS) {
		GPTP_LOG_ERROR("GetAdaptersAddresses failed with error %d", err);
		free(buffer);
		return INVALID_LINKSPEED;
	}
	
	for (; pAddr != NULL; pAddr = pAddr->Next)
	{
		if (pAddr->PhysicalAddressLength == ETHER_ADDR_OCTETS &&
			*local_addr == LinkLayerAddress(pAddr->PhysicalAddress))
		{
			break;
		}
	}

	if (pAddr == NULL) {
		GPTP_LOG_ERROR("Could not find adapter for specified MAC address");
		free(buffer);
		return INVALID_LINKSPEED;
	}

	// Check for invalid/uninitialized link speed values
	if (pAddr->ReceiveLinkSpeed == 0 || pAddr->ReceiveLinkSpeed == UINT64_MAX) {
		GPTP_LOG_WARNING("Adapter reports invalid link speed (%llu), attempting fallback detection", pAddr->ReceiveLinkSpeed);
		
		// Try TransmitLinkSpeed as fallback
		if (pAddr->TransmitLinkSpeed != 0 && pAddr->TransmitLinkSpeed != UINT64_MAX) {
			GPTP_LOG_INFO("Using TransmitLinkSpeed as fallback: %llu", pAddr->TransmitLinkSpeed);
			pAddr->ReceiveLinkSpeed = pAddr->TransmitLinkSpeed;
		} else {
			// Default to 1Gbps for modern interfaces
			GPTP_LOG_WARNING("No valid link speed detected, defaulting to 1Gbps");
			free(buffer);
			return LINKSPEED_1G;
		}
	}

	GPTP_LOG_INFO("Detected raw link speed: %llu bits/sec", pAddr->ReceiveLinkSpeed);
	
	// Convert to Kbps (divide by 1000) for comparison
	uint64_t speed_kbps = pAddr->ReceiveLinkSpeed / WIN_LINKSPEED_MULT;
	GPTP_LOG_VERBOSE("Link speed in Kbps: %llu", speed_kbps);

	// Enhanced speed detection with more realistic ranges
	if (speed_kbps >= 900000 && speed_kbps <= 1100000) {
		// 1 Gbps (900Mbps - 1.1Gbps range to account for variations)
		ret = LINKSPEED_1G;
		GPTP_LOG_INFO("Detected 1 Gigabit Ethernet");
	} else if (speed_kbps >= 90000 && speed_kbps <= 110000) {
		// 100 Mbps (90Mbps - 110Mbps range)
		ret = LINKSPEED_100MB;
		GPTP_LOG_INFO("Detected 100 Megabit Ethernet");
	} else if (speed_kbps >= 9000000) {
		// 10+ Gbps - treat as 1Gbps for gPTP purposes
		ret = LINKSPEED_1G;
		GPTP_LOG_INFO("Detected high-speed interface (%llu Kbps), treating as 1Gbps for gPTP", speed_kbps);
	} else {
		GPTP_LOG_WARNING("Unrecognized link speed %llu Kbps, defaulting to 1Gbps", speed_kbps);
		ret = LINKSPEED_1G;
	}

	free(buffer);
	return ret;
}