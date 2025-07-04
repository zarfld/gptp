# Enhanced asCapable Progress Logging Implementation

## Overview
Enhanced the Windows gPTP daemon logging to provide detailed visibility into the progress toward achieving asCapable=true status, including timeout information and estimated completion times.

## Problem Addressed
Previously, the logs only showed information AFTER timeouts occurred or when asCapable status changed, but users couldn't see:
- How many successful PDelay exchanges were needed for asCapable=true
- Current progress toward that goal  
- Time remaining until the next timeout
- When timeouts were approaching
- Estimated time to achieve asCapable=true

## Changes Made

### 1. Enhanced PDelay Request Logging (ether_port.cpp)
**Location**: Around line 910 in PDelay request scheduling

**Added logging for each PDelay request**:
- Current progress: "X/Y successful exchanges (need Z more for asCapable=true)"
- Timeout information: "Expecting response within X.X seconds"
- Current asCapable status and requirements

**Example output**:
```
*** PDELAY PROGRESS: PDelay request sent - asCapable progress: 1/3 successful exchanges (need 2 more for asCapable=true) ***
*** PDELAY TIMEOUT: Expecting response within 3.0 seconds (timeout multiplier: 3 x interval) ***
*** ASCAPABLE STATUS: Currently asCapable=false - 2 more successful PDelay exchanges needed for asCapable=true ***
```

### 2. Enhanced Timeout Handling (ether_port.cpp)
**Location**: PDELAY_RESP_RECEIPT_TIMEOUT_EXPIRES case

**Added detailed timeout analysis**:
- Timing information about the timeout that occurred
- Next request timing
- Estimated time to asCapable based on remaining requirements
- Progress impact of missed responses

**Example output**:
```
*** TIMEOUT DETAILS: PDelay response timeout occurred after 3.0 seconds, next request in 1.0 seconds ***
*** ASCAPABLE PROGRESS: Missing responses delay progress toward asCapable=true (estimated time: 2.0 seconds minimum) ***
```

### 3. Enhanced Success Logging (ptp_message.cpp)
**Location**: PDelay followup success handling around line 1918

**Added progress tracking for successful exchanges**:
- Estimated time to asCapable=true (assuming no further timeouts)
- Next request timing
- Clear progress indicators

**Example output**:
```
*** MILAN COMPLIANCE: PDelay success 2/3 - need 1 more before setting asCapable=true ***
*** ASCAPABLE PROGRESS: Estimated time to asCapable=true: 1.0 seconds (assuming no timeouts) ***
*** ASCAPABLE TIMING: Next PDelay request in 1.0 seconds ***
```

### 4. Startup Progress Logging (ether_port.cpp)
**Location**: startPDelay() function

**Added initial status reporting**:
- Requirements overview
- Current status
- Estimated completion time
- Profile configuration details

**Example output**:
```
*** ASCAPABLE STARTUP: Beginning PDelay exchanges - need 3 successful exchanges for asCapable=true (estimated: 3.0 seconds) ***
*** ASCAPABLE CONFIG: Profile 'MILAN' requires 3 successful PDelay exchanges minimum ***
```

## Key Timing Information Now Visible

### 1. **Timeout Calculations**
- PDelay response timeout = 3 × PDelay interval (configurable via PDELAY_RESP_RECEIPT_TIMEOUT_MULTIPLIER)
- Default intervals: typically 1 second (2^0), so 3-second timeout

### 2. **Progress Tracking**
- Current successful exchanges vs requirements
- Estimated time to completion
- Impact of timeouts on progress

### 3. **Real-time Status**
- Shows progress with each PDelay attempt
- Indicates when getting close to asCapable=true
- Warns about timeout impacts

## Files Modified
- `common/ether_port.cpp` - Enhanced timeout scheduling and handling
- `common/ptp_message.cpp` - Enhanced success processing

## Usage Examples

### During Startup:
```
*** ASCAPABLE STARTUP: Beginning PDelay exchanges - need 3 successful exchanges for asCapable=true (estimated: 3.0 seconds) ***
*** PDELAY PROGRESS: PDelay request sent - asCapable progress: 0/3 successful exchanges (need 3 more for asCapable=true) ***
*** PDELAY TIMEOUT: Expecting response within 3.0 seconds (timeout multiplier: 3 x interval) ***
```

### During Progress:
```
*** MILAN COMPLIANCE: PDelay success 1/3 - need 2 more before setting asCapable=true ***
*** ASCAPABLE PROGRESS: Estimated time to asCapable=true: 2.0 seconds (assuming no timeouts) ***
*** PDELAY PROGRESS: PDelay request sent - asCapable progress: 1/3 successful exchanges (need 2 more for asCapable=true) ***
```

### On Timeout:
```
*** TIMEOUT DETAILS: PDelay response timeout occurred after 3.0 seconds, next request in 1.0 seconds ***
*** MILAN COMPLIANCE: asCapable remains false - need 3 more successful PDelay exchanges (current: 0/3 minimum) ***
*** ASCAPABLE PROGRESS: Missing responses delay progress toward asCapable=true (estimated time: 3.0 seconds minimum) ***
```

### When Achieved:
```
*** MILAN COMPLIANCE: Setting asCapable=true after 3 successful PDelay exchanges (requirement: 3-unlimited) ***
*** ASCAPABLE STATUS: Currently asCapable=true with 3 successful exchanges ***
```

This implementation provides complete visibility into the asCapable acquisition process, making it easy to understand timing, progress, and any issues that may be delaying the achievement of asCapable=true status.
