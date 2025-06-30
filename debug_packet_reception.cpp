/******************************************************************************
  Enhanced gPTP packet reception debugging implementation
******************************************************************************/

#include "debug_packet_reception.hpp"
#include <pcap.h>
#include <iphlpapi.h>
#include <iostream>
#include <iomanip>

LARGE_INTEGER PacketReceptionDebugger::freq_ = {0};
bool PacketReceptionDebugger::freq_initialized_ = false;

void PacketReceptionDebugger::initTimer() {
    if (!freq_initialized_) {
        QueryPerformanceFrequency(&freq_);
        freq_initialized_ = true;
    }
}

double PacketReceptionDebugger::getTimestamp() {
    initTimer();
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / freq_.QuadPart;
}

bool PacketReceptionDebugger::testRawPacketReception(const char* interface_name, int timeout_ms) {
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t* handle = nullptr;
    
    std::cout << "\n=== Raw Packet Reception Test ===" << std::endl;
    std::cout << "Interface: " << interface_name << std::endl;
    std::cout << "Timeout: " << timeout_ms << "ms" << std::endl;
    
    // Open interface with optimized settings
    handle = pcap_open(interface_name, 
                      65536,  // snaplen
                      PCAP_OPENFLAG_PROMISCUOUS | PCAP_OPENFLAG_MAX_RESPONSIVENESS,
                      timeout_ms,
                      nullptr,
                      errbuf);
    
    if (!handle) {
        std::cout << "ERROR: Failed to open interface: " << errbuf << std::endl;
        return false;
    }
    
    std::cout << "✓ Interface opened successfully" << std::endl;
    
    // Set filter for PTP packets (EtherType 0x88F7)
    struct bpf_program filter;
    if (pcap_compile(handle, &filter, "ether proto 0x88F7", 1, 0) == -1) {
        std::cout << "WARNING: Failed to compile PTP filter: " << pcap_geterr(handle) << std::endl;
    } else if (pcap_setfilter(handle, &filter) == -1) {
        std::cout << "WARNING: Failed to set PTP filter: " << pcap_geterr(handle) << std::endl;
    } else {
        std::cout << "✓ PTP packet filter applied (EtherType 0x88F7)" << std::endl;
    }
    
    // Test packet reception for 10 seconds
    std::cout << "\nListening for packets (10 seconds)..." << std::endl;
    double start_time = getTimestamp();
    int packet_count = 0;
    int ptp_packet_count = 0;
    
    while (getTimestamp() - start_time < 10.0) {
        struct pcap_pkthdr* header;
        const u_char* packet;
        
        int result = pcap_next_ex(handle, &header, &packet);
        
        if (result == 1) {
            packet_count++;
            
            // Check if it's a PTP packet (EtherType 0x88F7)
            if (header->len >= 14) {
                uint16_t ethertype = (packet[12] << 8) | packet[13];
                if (ethertype == 0x88F7) {
                    ptp_packet_count++;
                    std::cout << "📦 PTP packet received! Size: " << header->len 
                             << " bytes, Time: " << std::fixed << std::setprecision(6) 
                             << (getTimestamp() - start_time) << "s" << std::endl;
                    
                    // Show basic PTP message info
                    if (header->len >= 15) {
                        uint8_t messageType = packet[14] & 0x0F;
                        std::cout << "   Message Type: ";
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
                        std::cout << std::endl;
                    }
                }
            }
        } else if (result == 0) {
            // Timeout - this is normal with low timeout values
            continue;
        } else {
            std::cout << "ERROR: pcap_next_ex returned " << result << ": " << pcap_geterr(handle) << std::endl;
            break;
        }
    }
    
    pcap_close(handle);
    
    std::cout << "\n=== Test Results ===" << std::endl;
    std::cout << "Total packets received: " << packet_count << std::endl;
    std::cout << "PTP packets received: " << ptp_packet_count << std::endl;
    
    if (packet_count == 0) {
        std::cout << "❌ NO packets received - possible issues:" << std::endl;
        std::cout << "   • Interface not connected" << std::endl;
        std::cout << "   • Cable not plugged in" << std::endl;
        std::cout << "   • Remote end not sending" << std::endl;
        std::cout << "   • Driver/Npcap issues" << std::endl;
        return false;
    } else if (ptp_packet_count == 0) {
        std::cout << "⚠️  Non-PTP packets received but no PTP packets" << std::endl;
        std::cout << "   • Remote gPTP daemon not running" << std::endl;
        std::cout << "   • PTP packets filtered/blocked" << std::endl;
        return false;
    } else {
        std::cout << "✅ PTP packets successfully received!" << std::endl;
        return true;
    }
}

void PacketReceptionDebugger::logPacketEvent(const char* event, const char* details) {
    double timestamp = getTimestamp();
    std::cout << "[" << std::fixed << std::setprecision(6) << timestamp << "] " 
              << event;
    if (details) {
        std::cout << ": " << details;
    }
    std::cout << std::endl;
}

bool PacketReceptionDebugger::checkLinkStatus(const uint8_t* mac_addr) {
    std::cout << "\n=== Link Status Check ===" << std::endl;
    
    IP_ADAPTER_ADDRESSES adapters[32];
    DWORD bufLen = sizeof(adapters);
    
    DWORD result = GetAdaptersAddresses(AF_UNSPEC, 
                                      GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST | 
                                      GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
                                      nullptr, adapters, &bufLen);
    
    if (result != ERROR_SUCCESS) {
        std::cout << "ERROR: GetAdaptersAddresses failed with error " << result << std::endl;
        return false;
    }
    
    for (PIP_ADAPTER_ADDRESSES adapter = adapters; adapter != nullptr; adapter = adapter->Next) {
        if (adapter->PhysicalAddressLength == 6 && 
            memcmp(adapter->PhysicalAddress, mac_addr, 6) == 0) {
            
            std::cout << "Found target interface: " << adapter->AdapterName << std::endl;
            std::cout << "Description: ";
            std::wcout << adapter->Description << std::endl;
            
            std::cout << "Operational Status: ";
            switch (adapter->OperStatus) {
                case IfOperStatusUp:
                    std::cout << "UP ✓" << std::endl;
                    break;
                case IfOperStatusDown:
                    std::cout << "DOWN ❌" << std::endl;
                    return false;
                case IfOperStatusTesting:
                    std::cout << "TESTING ⚠️" << std::endl;
                    break;
                case IfOperStatusUnknown:
                    std::cout << "UNKNOWN ❓" << std::endl;
                    break;
                case IfOperStatusDormant:
                    std::cout << "DORMANT 😴" << std::endl;
                    break;
                case IfOperStatusNotPresent:
                    std::cout << "NOT PRESENT ❌" << std::endl;
                    return false;
                case IfOperStatusLowerLayerDown:
                    std::cout << "LOWER LAYER DOWN ❌" << std::endl;
                    return false;
                default:
                    std::cout << "OTHER (" << adapter->OperStatus << ")" << std::endl;
                    break;
            }
            
            std::cout << "Link Speed: ";
            if (adapter->TransmitLinkSpeed != 0) {
                std::cout << (adapter->TransmitLinkSpeed / 1000000) << " Mbps" << std::endl;
            } else {
                std::cout << "Unknown" << std::endl;
            }
            
            return adapter->OperStatus == IfOperStatusUp;
        }
    }
    
    std::cout << "ERROR: Target interface not found" << std::endl;
    return false;
}

bool PacketReceptionDebugger::verifyInterfaceConfig(void* handle, uint16_t ethertype) {
    if (!handle) return false;
    
    std::cout << "\n=== Interface Configuration Verification ===" << std::endl;
    
    pcap_t* pcap_handle = static_cast<pcap_t*>(handle);
    
    // Check if we can get stats
    struct pcap_stat stats;
    if (pcap_stats(pcap_handle, &stats) == 0) {
        std::cout << "Packets received: " << stats.ps_recv << std::endl;
        std::cout << "Packets dropped by kernel: " << stats.ps_drop << std::endl;
        std::cout << "Packets dropped by interface: " << stats.ps_ifdrop << std::endl;
    } else {
        std::cout << "WARNING: Could not get pcap statistics" << std::endl;
    }
    
    // Verify filter
    std::cout << "EtherType filter: 0x" << std::hex << ethertype << std::dec << std::endl;
    
    return true;
}

void PacketReceptionDebugger::monitorReceptionStats(void* handle, int duration_seconds) {
    if (!handle) return;
    
    std::cout << "\n=== Monitoring Reception Statistics ===" << std::endl;
    std::cout << "Duration: " << duration_seconds << " seconds" << std::endl;
    
    pcap_t* pcap_handle = static_cast<pcap_t*>(handle);
    double start_time = getTimestamp();
    
    struct pcap_stat initial_stats, current_stats;
    if (pcap_stats(pcap_handle, &initial_stats) != 0) {
        std::cout << "ERROR: Could not get initial statistics" << std::endl;
        return;
    }
    
    while (getTimestamp() - start_time < duration_seconds) {
        Sleep(1000); // Wait 1 second
        
        if (pcap_stats(pcap_handle, &current_stats) == 0) {
            int packets_received = current_stats.ps_recv - initial_stats.ps_recv;
            int packets_dropped = current_stats.ps_drop - initial_stats.ps_drop;
            
            std::cout << "Time: " << std::fixed << std::setprecision(1) 
                     << (getTimestamp() - start_time) << "s, "
                     << "Received: " << packets_received << ", "
                     << "Dropped: " << packets_dropped << std::endl;
        }
    }
}
