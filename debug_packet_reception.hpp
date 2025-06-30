/******************************************************************************
  Copyright (c) 2024, Enhanced gPTP Debugging
  
  Debug utilities for investigating packet reception issues in direct
  NIC-to-NIC gPTP setups.
******************************************************************************/

#ifndef DEBUG_PACKET_RECEPTION_HPP
#define DEBUG_PACKET_RECEPTION_HPP

#include <windows.h>
#include <iostream>
#include <chrono>

/**
 * @brief Enhanced debugging utilities for packet reception issues
 */
class PacketReceptionDebugger {
public:
    /**
     * @brief Check if packets are being received at the raw pcap level
     */
    static bool testRawPacketReception(const char* interface_name, int timeout_ms = 1000);
    
    /**
     * @brief Monitor packet reception statistics
     */
    static void monitorReceptionStats(void* handle, int duration_seconds = 10);
    
    /**
     * @brief Verify interface binding and filter configuration
     */
    static bool verifyInterfaceConfig(void* handle, uint16_t ethertype);
    
    /**
     * @brief Enhanced logging for packet reception events
     */
    static void logPacketEvent(const char* event, const char* details = nullptr);
    
    /**
     * @brief Check link state and interface status
     */
    static bool checkLinkStatus(const uint8_t* mac_addr);
    
private:
    static LARGE_INTEGER freq_;
    static bool freq_initialized_;
    static void initTimer();
    static double getTimestamp();
};

/**
 * @brief Optimized packet reception configuration
 */
struct OptimizedPacketConfig {
    int timeout_ms;              // Reduced timeout for faster response
    bool promiscuous_mode;       // Enhanced promiscuous mode
    bool immediate_mode;         // Immediate packet delivery
    int buffer_size;            // Optimized buffer size
    
    // Default optimized settings for direct NIC-to-NIC
    OptimizedPacketConfig() :
        timeout_ms(1),           // 1ms timeout for near real-time
        promiscuous_mode(true),
        immediate_mode(true),
        buffer_size(65536) {}
};

#endif // DEBUG_PACKET_RECEPTION_HPP
