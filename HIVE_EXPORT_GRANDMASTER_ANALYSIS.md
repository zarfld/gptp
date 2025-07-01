# gPTP Analysis Results - Hive Export and Grandmaster Test

## Hive AVDECC Export Analysis

From the PreSonus StudioLive 32SC export file (`Entity_0x000A92FFFE00E935.ave`):

### Device Information:
- **Entity ID**: `0x000A92FFFE00E935` (StudioLive 32SC)
- **MAC Address**: `00:0A:92:00:E9:35`
- **Current gPTP Grandmaster**: `0x000A92FFFE01C2EB` (likely the SWE5 switch)
- **Firmware Version**: 1.0.0.103375
- **Model**: PreSonus StudioLive 32SC

### Stream Configuration:
- **Input Streams**: 8 streams (Return 1-8, 9-16, 17-24, 25-32, 33-40, 41-48, 49-56, 57-64)
- **Output Streams**: 8 streams (Send 1-8, 9-16, 17-24, 25-32, 33-40, 41-48, 49-56, 57-64)
- **Active Connections**: 4 input streams connected from entity `0x000A92FFFE007966` (32R mixer)
- **Stream Format**: `0x00A0020840000800` (8-channel, 48kHz)
- **AVB Interface**: Supports gPTP (GPTP_GRANDMASTER_SUPPORTED, GPTP_SUPPORTED)

### gPTP Configuration (from StudioLive 32SC):
- **Clock Class**: Not explicitly shown but derived from AVDECC data
- **Clock Accuracy**: Present in avb_interface_descriptors
- **Log Sync Interval**: Configured per stream
- **Log Announce Interval**: Configured per stream
- **Log PDelay Interval**: Configured per stream

## gPTP Grandmaster Test Results

### Configuration File Issues Found:
1. **Parser Section Mismatch**: 
   - Config files used `[gPTP]` section 
   - Parser expects `[ptp]` section
   - **FIXED**: Changed all config files to use `[ptp]`

2. **Parser Bugs**: 
   - Items reported as "Unrecognized" even though parser has handlers
   - Default values still used despite correct section name
   - `profile = milan` works correctly
   - `priority1`, `clockClass` not being parsed correctly

### Current Status:
- ✅ gPTP starts and loads Milan profile
- ✅ Becomes AsCapable initially  
- ✅ Starts PDelay protocol
- ✅ Announce messages should be sent (AsCapable=true)
- ❌ Hardware timestamping fails (falls back to software)
- ❌ PDelay responses not received (timeout after 3 seconds)
- ❌ Becomes AsCapable=DISABLED (stops Announce messages)
- ❌ Configuration parsing has bugs (priority1/clockClass not applied)

### Key Observations:

1. **Timing Protocol Status**:
   - PC can start as gPTP master candidate
   - Hardware timestamping unavailable (Intel I210 not functional)
   - Software timestamping fallback works
   - PDelay requests sent but no responses (no other gPTP devices responding)

2. **Network Topology**:
   - PreSonus devices have an existing gPTP grandmaster: `0x000A92FFFE01C2EB`
   - This is likely the SWE5 switch acting as grandmaster
   - PC needs to have better priority to become grandmaster
   - Current PC priority1=248 (default) vs configured priority1=50

3. **Next Steps**:
   - **FIX PARSER BUGS**: Debug why priority1/clockClass not being read
   - **WIRESHARK CAPTURE**: Capture PTP traffic to see network activity
   - **COMPARE PROFILES**: Test different profiles (Milan vs AVnu vs Standard)
   - **BMCA ANALYSIS**: Analyze Best Master Clock Algorithm behavior

## Recommendations:

### Immediate:
1. **Debug INI Parser**: Fix configuration value parsing
2. **Network Capture**: Use Wireshark to see existing gPTP traffic
3. **Hardware Check**: Verify Intel I210 PTP functionality

### Analysis:
1. **Compare gPTP Implementations**: PreSonus vs PC implementation compatibility
2. **Profile Compatibility**: Test Milan profile with PreSonus devices
3. **BMCA Behavior**: Understand master election process

### Long-term:
1. **Integration Testing**: Multi-device gPTP network testing
2. **Performance Analysis**: Timing accuracy and convergence
3. **Production Deployment**: Robust configuration and monitoring
