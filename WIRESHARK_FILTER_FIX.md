# Wireshark gPTP Filter Fix Guide

## Problem: `eth.type == 0x88f7` shows parsing error

## Solutions (try in order):

### 1. Check Npcap Installation
- Open Wireshark
- Go to Help → About Wireshark
- Look for "with Npcap" in the version info
- If missing, download Npcap from: https://npcap.com/#download

### 2. Select Network Interface First
- Open Wireshark
- Select your AVB network interface (Intel I210 or similar)
- **Then** apply the filter

### 3. Alternative Filter Syntaxes (try these):

```
# Option 1: Standard Ethernet type filter
eth.type == 0x88f7

# Option 2: IEEE 1588 PTP filter  
ptp

# Option 3: Explicit frame type
frame contains 88:f7

# Option 4: Ethernet length/type field
eth.len_type == 0x88f7

# Option 5: Raw bytes filter
eth[12:2] == 88:f7
```

### 4. Step-by-Step Wireshark Setup:

1. **Open Wireshark**
2. **Select Interface**: Choose "Intel(R) Ethernet I210-T1 GbE NIC" or "AVB"
3. **Start Capture** (without filter first)
4. **Apply Filter**: Try `ptp` first (simplest)
5. **If that works**, then try `eth.type == 0x88f7`

### 5. Useful gPTP Filters:

```
# All gPTP/PTP traffic
ptp

# Only Announce messages (master election)
ptp.v2.messagetype == 11

# Only Sync messages (time sync)
ptp.v2.messagetype == 0

# PDelay messages
ptp.v2.messagetype >= 2 and ptp.v2.messagetype <= 5

# Show priority and clock class
ptp.v2.gm.priority1 or ptp.v2.gm.clockclass
```

### 6. Test Network Connectivity:

Before starting Wireshark capture, verify your network setup:
