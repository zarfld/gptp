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
#include <tchar.h>
#include <iphlpapi.h>

#include <ether_port.hpp>
#include <wireless_port.hpp>
#include <intel_wireless.hpp>

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

void print_usage( char *arg0 ) {
	fprintf( stderr,
		"%s "
		"[-R <priority 1>] <network interface>\n"
		"where <network interface> is a MAC address entered as xx-xx-xx-xx-xx-xx\n",
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
			if (toupper(argv[i][1]) == 'W')
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

	// Create Clock object
	portInit.clock = new IEEE1588Clock(false, false, priority1, timerq_factory, ipc, portInit.lock_factory);  // Do not force slave

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
	for (; pAddr != NULL; pAddr = pAddr->Next)
	{
		//fprintf(stderr, "** here : %p\n", pAddr);
		if (pAddr->PhysicalAddressLength == ETHER_ADDR_OCTETS &&
			*local_addr == LinkLayerAddress(pAddr->PhysicalAddress))
		{
			break;
		}
	}

	if (pAddr == NULL)
		return INVALID_LINKSPEED;

	switch ( pAddr->ReceiveLinkSpeed / WIN_LINKSPEED_MULT )
	{
	default:
		GPTP_LOG_ERROR("Can't find link speed, %llu", pAddr->ReceiveLinkSpeed);
		ret = INVALID_LINKSPEED;
		break;
	case LINKSPEED_1G:
		ret = LINKSPEED_1G;
		break;
	case LINKSPEED_100MB:
		ret = LINKSPEED_100MB;
		break;
	}

	delete buffer;
	return ret;
}