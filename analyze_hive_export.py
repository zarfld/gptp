#!/usr/bin/env python3
"""
Analyze Hive AVDECC export file for network statistics and gPTP insights.
This script extracts key information from the PreSonus StudioLive 32SC export.
"""

import re
import sys
from pathlib import Path

def analyze_hive_export(file_path):
    """Analyze the Hive AVDECC export file for key network statistics."""
    
    print("=" * 80)
    print("HIVE AVDECC EXPORT ANALYSIS")
    print("=" * 80)
    
    try:
        with open(file_path, 'rb') as f:
            content = f.read()
        
        # Convert binary content to string, handling non-printable characters
        text_content = content.decode('latin-1', errors='ignore')
        
        # Extract key device information
        print("\n📍 DEVICE IDENTIFICATION:")
        print("-" * 40)
        
        # Entity ID and MAC
        entity_matches = re.findall(r'entity_id[^0-9A-Fa-fx]*0x([0-9A-Fa-f]+)', text_content)
        if entity_matches:
            print(f"Entity ID: 0x{entity_matches[0]}")
        
        mac_matches = re.findall(r'mac_address[^0-9A-Fa-f:]*([0-9A-Fa-f:]{17})', text_content)
        if mac_matches:
            print(f"MAC Address: {mac_matches[0]}")
        
        # Device name and model
        if 'StudioLive 32SC' in text_content:
            print("Device: PreSonus StudioLive 32SC")
        
        firmware_matches = re.findall(r'firmware_version[^0-9.]*([0-9.]+)', text_content)
        if firmware_matches:
            print(f"Firmware: {firmware_matches[0]}")
        
        # Extract gPTP/AVB interface information
        print("\n🕰️ gPTP/AVB INTERFACE:")
        print("-" * 40)
        
        # gPTP Grandmaster ID
        gm_matches = re.findall(r'gptp_grandmaster_id[^0-9A-Fa-fx]*0x([0-9A-Fa-f]+)', text_content)
        if gm_matches:
            print(f"Current Grandmaster: 0x{gm_matches[0]}")
        
        # Clock parameters
        clock_class_matches = re.findall(r'clock_class[^0-9]*(\d+)', text_content)
        if clock_class_matches:
            print(f"Clock Class: {clock_class_matches[0]}")
        
        clock_accuracy_matches = re.findall(r'clock_accuracy[^0-9]*(\d+)', text_content)
        if clock_accuracy_matches:
            print(f"Clock Accuracy: {clock_accuracy_matches[0]}")
        
        # Log intervals
        announce_matches = re.findall(r'log_announce_interval[^-0-9]*(-?\d+)', text_content)
        if announce_matches:
            print(f"Log Announce Interval: {announce_matches[0]}")
        
        sync_matches = re.findall(r'log_sync_interval[^-0-9]*(-?\d+)', text_content)
        if sync_matches:
            print(f"Log Sync Interval: {sync_matches[0]}")
        
        pdelay_matches = re.findall(r'log_pdelay_interval[^-0-9]*(-?\d+)', text_content)
        if pdelay_matches:
            print(f"Log PDelay Interval: {pdelay_matches[0]}")
        
        # Propagation delay
        prop_delay_matches = re.findall(r'propagation_delay[^0-9]*(\d+)', text_content)
        if prop_delay_matches:
            print(f"Propagation Delay: {prop_delay_matches[0]} ns")
        
        # AVB capabilities and flags
        print("\n🌐 AVB CAPABILITIES:")
        print("-" * 40)
        
        if 'AS_CAPABLE' in text_content:
            print("✅ AS_CAPABLE (Audio/Video capable)")
        if 'GPTP_ENABLED' in text_content:
            print("✅ GPTP_ENABLED")
        if 'SRP_ENABLED' in text_content:
            print("✅ SRP_ENABLED (Stream Reservation Protocol)")
        if 'GPTP_GRANDMASTER_SUPPORTED' in text_content:
            print("✅ GPTP_GRANDMASTER_SUPPORTED")
        if 'GPTP_SUPPORTED' in text_content:
            print("✅ GPTP_SUPPORTED")
        
        # Stream information
        print("\n🎵 STREAM CONFIGURATION:")
        print("-" * 40)
        
        # Count stream inputs and outputs
        input_streams = text_content.count('stream_input_descriptors')
        output_streams = text_content.count('stream_output_descriptors')
        
        print(f"Stream Inputs: {input_streams}")
        print(f"Stream Outputs: {output_streams}")
        
        # Connected streams
        connected_count = text_content.count('CONNECTED')
        not_connected_count = text_content.count('NOT_CONNECTED')
        
        print(f"Connected Streams: {connected_count}")
        print(f"Not Connected Streams: {not_connected_count}")
        
        # Stream formats
        stream_formats = re.findall(r'stream_format[^0-9A-Fa-fx]*0x([0-9A-Fa-f]+)', text_content)
        unique_formats = list(set(stream_formats))
        if unique_formats:
            print(f"Stream Formats: {', '.join([f'0x{fmt}' for fmt in unique_formats])}")
        
        # Extract talker/listener relationships
        print("\n🔗 STREAM CONNECTIONS:")
        print("-" * 40)
        
        # Find connected talkers
        talker_matches = re.findall(r'connected_talker[^0-9A-Fa-fx]*entity_id[^0-9A-Fa-fx]*0x([0-9A-Fa-f]+)', text_content)
        unique_talkers = list(set([t for t in talker_matches if t != 'FFFFFFFFFFFFFFFF']))
        
        if unique_talkers:
            print("Connected Talkers:")
            for talker in unique_talkers:
                print(f"  - 0x{talker}")
        else:
            print("No active talker connections found")
        
        # Clock domain information
        print("\n⏰ CLOCK DOMAINS:")
        print("-" * 40)
        
        clock_sources = text_content.count('clock_source_descriptors')
        if clock_sources:
            print(f"Clock Sources: {clock_sources}")
        
        if 'INPUT_STREAM' in text_content:
            print("✅ Input Stream Clock Source")
        if 'INTERNAL' in text_content:
            print("✅ Internal Clock Source")
        
        # Check for locked/unlocked status
        if 'LOCKED' in text_content:
            print("🔒 Clock Status: LOCKED found")
        if 'UNLOCKED' in text_content:
            print("🔓 Clock Status: UNLOCKED found")
        
        # Audio channel information
        print("\n🎛️ AUDIO CHANNELS:")
        print("-" * 40)
        
        # Count channels
        channel_matches = re.findall(r'Channel (\d+)', text_content)
        if channel_matches:
            channel_numbers = [int(ch) for ch in channel_matches]
            print(f"Total Channels Found: {len(set(channel_numbers))}")
            print(f"Channel Range: {min(channel_numbers)} - {max(channel_numbers)}")
        
        # MBLA format
        if 'MBLA' in text_content:
            print("✅ MBLA (Milan-Based Logic Audio) format supported")
        
        # Network analysis summary
        print("\n🔍 NETWORK ANALYSIS SUMMARY:")
        print("-" * 40)
        
        # Determine device role
        if gm_matches and gm_matches[0] == '000A92FFFE00E935':
            print("🏆 This device appears to be acting as gPTP Grandmaster")
        elif gm_matches:
            print(f"📡 This device is synchronized to Grandmaster: 0x{gm_matches[0]}")
        
        # Stream activity
        if connected_count > 0:
            print(f"🎵 Active streaming: {connected_count} connected streams")
        else:
            print("⏸️ No active streaming connections")
        
        # Milan compatibility
        if 'MBLA' in text_content and clock_class_matches and clock_class_matches[0] in ['6', '7']:
            print("✅ Appears to be Milan-compatible")
        
        return True
        
    except FileNotFoundError:
        print(f"❌ File not found: {file_path}")
        return False
    except Exception as e:
        print(f"❌ Error analyzing file: {e}")
        return False

def generate_recommendations():
    """Generate recommendations based on the analysis."""
    
    print("\n💡 RECOMMENDATIONS FOR gPTP INTEGRATION:")
    print("-" * 50)
    print("1. Test PC as Grandmaster:")
    print("   - Use grandmaster_config.ini to make PC the master clock")
    print("   - Monitor if PreSonus accepts PC as grandmaster")
    print("")
    print("2. Profile Compatibility:")
    print("   - Try Milan profile (test_milan_config.ini)")
    print("   - Try Standard IEEE 1588 profile (standard_presonus_config.ini)")
    print("   - Compare announce/sync message behavior")
    print("")
    print("3. Timing Analysis:")
    print("   - Monitor propagation delay measurements")
    print("   - Check if PC gPTP can achieve similar timing performance")
    print("   - Verify PDelay request/response cycle timing")
    print("")
    print("4. Stream Integration:")
    print("   - Test AVDECC enumeration between PC and PreSonus")
    print("   - Attempt stream connections if AVDECC controller available")
    print("   - Monitor for SRP (Stream Reservation Protocol) messages")
    print("")
    print("5. Wireshark Capture:")
    print("   - Capture PTP traffic during PC gPTP operation")
    print("   - Compare timing intervals with PreSonus requirements")
    print("   - Analyze BMCA (Best Master Clock Algorithm) negotiation")

if __name__ == "__main__":
    # Path to the Hive export file
    export_file = r"c:\Users\dzarf\Desktop\Entity_0x000A92FFFE00E935.ave"
    
    success = analyze_hive_export(export_file)
    
    if success:
        generate_recommendations()
        
        print("\n📋 NEXT STEPS:")
        print("-" * 20)
        print("1. Run gPTP with different profiles and monitor PreSonus response")
        print("2. Use Wireshark to capture PTP traffic for detailed analysis")
        print("3. Test bidirectional gPTP synchronization")
        print("4. Document timing performance differences")
    
    print("\n" + "=" * 80)
