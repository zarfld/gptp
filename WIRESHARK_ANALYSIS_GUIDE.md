# Wireshark gPTP Analysis Guide for PreSonus Networks

## Quick Start - Wireshark Capture Setup

### 1. Install Wireshark (if needed)
```powershell
# Download from https://www.wireshark.org/download.html
# Or via chocolatey:
choco install wireshark
```

### 2. Capture Interface Setup
1. **Open Wireshark**
2. **Select Interface**: Choose "AVB" or "Intel(R) Ethernet I210-T1 GbE NIC"
3. **Start Capture** before running gPTP

### 3. Essential gPTP Filters

#### Basic gPTP Messages
```
eth.type == 0x88f7
```

#### Specific Message Types
```
# Announce messages (BMCA/Master election)
ptp.v2.messagetype == 0x0b

# Sync messages (time distribution)
ptp.v2.messagetype == 0x00

# Follow_Up messages (precise timestamps)
ptp.v2.messagetype == 0x08

# PDelay Request (peer delay measurement)
ptp.v2.messagetype == 0x02

# PDelay Response
ptp.v2.messagetype == 0x03

# PDelay Response Follow Up
ptp.v2.messagetype == 0x0a
```

#### Advanced Analysis Filters
```
# Show only master/slave changes
ptp.v2.messagetype == 0x0b and ptp.v2.flags.twostep == 1

# PDelay exchanges from specific device
ptp.v2.messagetype >= 0x02 and ptp.v2.messagetype <= 0x0a

# Show clock quality changes
ptp.v2.gm.identity or ptp.v2.clockquality.clockclass
```

## What to Look For in PreSonus Networks

### 1. Network Discovery Phase
- **Announce Messages**: Look for multiple devices announcing themselves
- **Clock Quality**: Compare clockClass values (lower = better master)
- **Priority Values**: Our PC should have priority1=50, PreSonus likely 248

### 2. Master Election (BMCA)
- **Winner**: Device with lowest priority1 should become master
- **Expected**: PC (priority1=50) should beat PreSonus (priority1=248)
- **Timing**: Election should complete within 2-3 announce intervals

### 3. Synchronization Phase
- **Sync Messages**: Master sends every 125ms (8/second) in Milan mode
- **Follow_Up**: Should follow each Sync message with precise timestamps
- **Convergence**: Network should stabilize within 100ms (Milan target)

### 4. PDelay Measurement
- **Request/Response**: Bi-directional delay measurement
- **Frequency**: Every 1 second between adjacent devices
- **Success**: Look for successful round-trip completions

## PreSonus-Specific Analysis

### Device Identification
```
# Look for PreSonus OUI in source MAC addresses
eth.src[0:3] == 00:26:ba  # PreSonus OUI
```

### Expected Behavior
1. **StudioLive Mixers**: Should announce with clockClass=248, priority1=248
2. **SWE5 Switch**: May act as transparent bridge or participate in gPTP
3. **PC Grandmaster**: Should dominate with clockClass=6, priority1=50

### Timing Analysis
```
# Calculate announce intervals
ptp.v2.messagetype == 0x0b
# Should see consistent 1-second intervals

# Calculate sync intervals  
ptp.v2.messagetype == 0x00
# Should see 125ms intervals in Milan mode
```

## Troubleshooting Common Issues

### No gPTP Traffic
- Check physical network connection
- Verify AVB interface is active
- Ensure gPTP daemon is running

### Master Election Not Working
- Compare priority1 values in Announce messages
- Check clockClass values (lower = better)
- Look for BMCA state changes in gPTP logs

### PDelay Failures
- Check for PDelay Request without Response
- Verify bidirectional connectivity
- Look for timestamp retrieval errors

### Sync Issues
- Verify master is sending Sync + Follow_Up pairs
- Check for consistent 125ms intervals
- Look for timestamp accuracy issues

## Advanced Analysis

### Packet Timing Precision
1. **Export packet data** to CSV for analysis
2. **Calculate jitter** between Sync messages
3. **Measure convergence time** from network join to stable sync

### Performance Metrics
- **Announce interval accuracy** (should be 1.000s ±0.001s)
- **Sync interval accuracy** (should be 0.125s ±0.001s)
- **PDelay response time** (should be <1ms typically)

### Integration Points
- **AVDECC discovery** (if present on network)
- **Stream establishment** (if audio streaming active)
- **Clock domain alignment** (verify all devices in same domain)

## Capture Commands

### Automated Capture Start
```powershell
# Start Wireshark capture programmatically
tshark -i "AVB" -f "ether proto 0x88f7" -w "presonus_gptp_capture.pcapng"
```

### Post-Capture Analysis
```powershell
# Analyze capture file
tshark -r "presonus_gptp_capture.pcapng" -Y "ptp.v2.messagetype == 0x0b" -T fields -e frame.time -e eth.src -e ptp.v2.clockquality.clockclass
```

This guide provides comprehensive Wireshark analysis capabilities for validating gPTP behavior with PreSonus devices.
