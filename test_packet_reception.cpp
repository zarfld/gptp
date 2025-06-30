/******************************************************************************
  Copyright (c) 2024, Enhanced gPTP Debugging
  
  Simple packet reception test program for direct NIC-to-NIC debugging
******************************************************************************/

#include <windows.h>
#include <iostream>
#include <iomanip>
#include <string>
#include "packet.hpp"

// Simple test to verify packet reception works
int testPacketReception(const std::string& mac_address) {
    std::cout << "\n=== gPTP Packet Reception Test ===" << std::endl;
    std::cout << "Target MAC Address: " << mac_address << std::endl;
    
    // Parse MAC address
    uint8_t mac_bytes[6];
    if (sscanf_s(mac_address.c_str(), "%02hhx-%02hhx-%02hhx-%02hhx-%02hhx-%02hhx",
                 &mac_bytes[0], &mac_bytes[1], &mac_bytes[2], 
                 &mac_bytes[3], &mac_bytes[4], &mac_bytes[5]) != 6) {
        std::cout << "ERROR: Invalid MAC address format. Use xx-xx-xx-xx-xx-xx" << std::endl;
        return -1;
    }
    
    // Open packet interface
    struct packet_handle* handle;
    packet_addr_t addr;
    memcpy(addr.addr, mac_bytes, 6);
    
    if (mallocPacketHandle(&handle) != PACKET_NO_ERROR) {
        std::cout << "ERROR: Failed to allocate packet handle" << std::endl;
        return -1;
    }
    
    std::cout << "Opening interface..." << std::endl;
    if (openInterfaceByAddr(handle, &addr, 1) != PACKET_NO_ERROR) {  // 1ms timeout
        std::cout << "ERROR: Failed to open interface: " << handle->errbuf << std::endl;
        freePacketHandle(handle);
        return -1;
    }
    
    std::cout << "✓ Interface opened successfully" << std::endl;
    
    // Bind to PTP ethertype
    std::cout << "Binding to PTP EtherType (0x88F7)..." << std::endl;
    if (packetBind(handle, 0x88F7) != PACKET_NO_ERROR) {
        std::cout << "ERROR: Failed to bind to PTP ethertype" << std::endl;
        closeInterface(handle);
        freePacketHandle(handle);
        return -1;
    }
    
    std::cout << "✓ PTP filter applied" << std::endl;
    std::cout << "\nListening for PTP packets (10 seconds)..." << std::endl;
    std::cout << "Make sure the remote gPTP daemon is running!" << std::endl;
    
    // Listen for packets
    DWORD start_time = GetTickCount();
    int packet_count = 0;
    
    while (GetTickCount() - start_time < 10000) {  // 10 seconds
        uint8_t buffer[128];
        size_t length = sizeof(buffer);
        packet_addr_t source_addr;
        
        packet_error_t result = recvFrame(handle, &source_addr, buffer, length);
        
        if (result == PACKET_NO_ERROR) {
            packet_count++;
            std::cout << "📦 PTP Packet #" << packet_count << " received!" << std::endl;
            std::cout << "   Size: " << length << " bytes" << std::endl;
            std::cout << "   Source: ";
            for (int i = 0; i < 6; i++) {
                std::cout << std::hex << std::setfill('0') << std::setw(2) 
                         << (int)source_addr.addr[i];
                if (i < 5) std::cout << "-";
            }
            std::cout << std::dec << std::endl;
            
            // Show PTP message type
            if (length >= 1) {
                uint8_t messageType = buffer[0] & 0x0F;
                std::cout << "   PTP Message Type: ";
                switch (messageType) {
                    case 0: std::cout << "Sync"; break;
                    case 1: std::cout << "Delay_Req"; break;
                    case 2: std::cout << "Pdelay_Req"; break;
                    case 3: std::cout << "Pdelay_Resp"; break;
                    case 8: std::cout << "Follow_Up"; break;
                    case 9: std::cout << "Delay_Resp"; break;
                    case 10: std::cout << "Pdelay_Resp_Follow_Up"; break;
                    case 11: std::cout << "Announce"; break;
                    case 12: std::cout << "Signaling"; break;
                    default: std::cout << "Unknown (" << (int)messageType << ")"; break;
                }
                std::cout << std::endl << std::endl;
            }
        } else if (result == PACKET_RECVTIMEOUT_ERROR) {
            // Timeout is normal with 1ms timeout
            continue;
        } else {
            std::cout << "ERROR: Packet reception failed with error " << result << std::endl;
            break;
        }
    }
    
    closeInterface(handle);
    freePacketHandle(handle);
    
    std::cout << "\n=== Test Results ===" << std::endl;
    std::cout << "Total PTP packets received: " << packet_count << std::endl;
    
    if (packet_count == 0) {
        std::cout << "\n❌ NO PTP PACKETS RECEIVED" << std::endl;
        std::cout << "Possible issues:" << std::endl;
        std::cout << "• Remote gPTP daemon not running" << std::endl;
        std::cout << "• Cable not connected" << std::endl;
        std::cout << "• Wrong MAC address specified" << std::endl;
        std::cout << "• Interface not up" << std::endl;
        std::cout << "• Firewall/security software blocking" << std::endl;
        return -1;
    } else {
        std::cout << "\n✅ SUCCESS: PTP packets are being received!" << std::endl;
        std::cout << "The packet reception mechanism is working correctly." << std::endl;
        return 0;
    }
}

int main(int argc, char* argv[]) {
    std::cout << "Enhanced gPTP Packet Reception Test Tool" << std::endl;
    std::cout << "=======================================" << std::endl;
    
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <MAC_ADDRESS>" << std::endl;
        std::cout << "Example: " << argv[0] << " 00-1B-21-3C-5D-8F" << std::endl;
        return -1;
    }
    
    // Enable debug logging
    enablePacketReceptionDebug(true);
    
    return testPacketReception(argv[1]);
}
