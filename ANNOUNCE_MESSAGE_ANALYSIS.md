# gPTP Announce Message & BMCA Analysis

## **What Happens When an Announce Message is Received?**

When a gPTP Announce message is received, it triggers a sophisticated Best Master Clock Algorithm (BMCA) process that determines which clock should be the network's time source.

### **Immediate Processing Steps**

**File: `common/ptp_message.cpp:858` - `PTPMessageAnnounce::processMessage()`**

1. **Statistics Update**: Increment received Announce counter
2. **Timeout Management**: Cancel existing announce receipt timeout  
3. **Validation Checks**:
   - Reject if `stepsRemoved >= 255` (loop protection)
   - Reject if message is from ourselves (loop protection)
   - Reject if our clock identity is in TLV path (loop protection)
4. **Message Storage**: Store as `qualified_announce` for this port
5. **BMCA Trigger**: Schedule `STATE_CHANGE_EVENT` in 16ms
6. **Timeout Restart**: Restart announce receipt timeout timer

### **BMCA (Best Master Clock Algorithm) Execution**

**File: `common/common_port.cpp:340` - `processStateChange()`**

#### **Step 1: Port Scanning**
```cpp
// Find EBest (External Best) across all ports
for (int i = 0; i < number_ports; ++i) {
    if( EBest == NULL ) {
        EBest = ports[j]->calculateERBest();  // Returns qualified_announce
    }
    else if( ports[j]->calculateERBest() ) {
        if( ports[j]->calculateERBest()->isBetterThan(EBest)) {
            EBest = ports[j]->calculateERBest();  // New best found
        }
    }
}
```

#### **Step 2: Master Selection**
```cpp
if (clock->isBetterThan(EBest)) {
    // We are the Grandmaster - all ports become Master
    ports[j]->recommendState(PTP_MASTER, changed_external_master);
} else {
    if( EBest == ports[j]->calculateERBest() ) {
        // The port that received EBest becomes Slave
        ports[j]->recommendState(PTP_SLAVE, changed_external_master);
        // Update grandmaster information from EBest
        getClock()->setGrandmasterClockIdentity(EBest->getGrandmasterClockIdentity());
    }
}
```

### **BMCA Comparison Criteria**

**Files: `ptp_message.cpp:682`, `ieee1588clock.cpp:454` - `isBetterThan()`**

The algorithm builds a 14-byte comparison array following IEEE 1588 priority:

```cpp
// Lower values win (memcmp < 0)
this1[0] = grandmasterPriority1;         // Highest priority (0-255)
this1[1] = clockQuality.cq_class;        // Clock class (quality indicator)  
this1[2] = clockQuality.clockAccuracy;   // Accuracy specification
this1[3-4] = clockQuality.offsetScaledLogVariance;  // Stability measure
this1[5] = grandmasterPriority2;         // Secondary priority (0-255)
this1[6-13] = grandmasterClockIdentity;  // MAC-based tie-breaker

return (memcmp(this1, that1, 14) < 0) ? true : false;
```

**Priority Order** (most important to least):
1. **Priority1** - Administrative priority (lower wins)
2. **Clock Class** - Quality/type of clock source
3. **Clock Accuracy** - Specified accuracy to UTC
4. **Clock Variance** - Long-term stability
5. **Priority2** - Secondary administrative priority  
6. **Clock Identity** - Unique identifier (tie-breaker)

### **State Change Results**

Based on BMCA outcome:

- **PTP_MASTER**: Port sends Announce, Sync, Follow_Up messages
- **PTP_SLAVE**: Port receives and synchronizes to master
- **PTP_PASSIVE**: Port monitors but doesn't sync (unused in gPTP)

## **How Announce Messages Are Sent**

### **Timing & Intervals**
- **Default interval**: 1 second (log_mean_announce_interval = 0, so 2^0 = 1 second)
- **Timer event**: `ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES` triggers every interval
- **Code location**: `common/common_port.cpp` lines 607-642

### **Critical Condition: asCapable Flag**
```cpp
if ( asCapable)  // ← THIS IS THE KEY!
{
    // Send announce message
    PTPMessageAnnounce *annc = new PTPMessageAnnounce(this);
    // ... send logic ...
}
```

**Announce messages are ONLY sent when `asCapable = true`**

### **When asCapable Gets Set**

#### **Initial State:**
- **Automotive profile**: `asCapable = true` (immediate sending)
- **Standard profile**: `asCapable = false` (no sending until link ready)

#### **Link State Detection:**
```cpp
// In windows_hal.cpp - link monitoring sets asCapable based on link status
pPort->setAsCapable(link_status);  // true when link is up and ready
```

## **Enhanced Debugging Added**

### **Announce Message Transmission Tracking**
```
*** SENDING ANNOUNCE MESSAGE #X *** (asCapable=true, interval=0)
Announce message #X sent successfully
```

### **Announce Message Reception Tracking**
```
*** RECEIVED ANNOUNCE MESSAGE #X *** (size=XXX bytes)
```

### **asCapable State Change Monitoring**
```
*** AsCapable STATE CHANGE: ENABLED *** (Announce messages will NOW be sent)
*** AsCapable STATE CHANGE: DISABLED *** (Announce messages will NO LONGER be sent)
```

### **BMCA Debug Output**
```
*** BMCA: Processing STATE_CHANGE_EVENT ***
*** BMCA: Found EBest from port X ***
*** BMCA: We are BETTER than EBest - becoming Grandmaster ***
*** BMCA: EBest is BETTER than us - becoming Slave ***
*** BMCA: Port X recommended state: PTP_MASTER ***
*** BMCA: Port X recommended state: PTP_SLAVE ***
```

### **Blocked Announce Messages**
```
*** ANNOUNCE MESSAGE BLOCKED #X *** (asCapable=false - not ready to send)
```

## **Key Insights: What Happens on Announce Reception**

### **Immediate Actions (within microseconds)**
1. **Validation**: Message is checked for loops and validity
2. **Storage**: Valid Announce becomes this port's `qualified_announce`
3. **Timer Cancel**: Existing announce receipt timeout is cancelled
4. **BMCA Scheduling**: `STATE_CHANGE_EVENT` scheduled for 16ms later

### **BMCA Execution (16ms later)**
1. **Port Scanning**: All ports' `qualified_announce` messages compared
2. **Best Selection**: Algorithm finds the "External Best" (EBest) Announce
3. **Clock Comparison**: Local clock vs. EBest using priority/quality rules
4. **Role Assignment**: Ports become Master, Slave, or remain unchanged
5. **Grandmaster Update**: If new master selected, update network time source

### **Network Convergence Result**
- **Single Grandmaster**: One node becomes the authoritative time source
- **Hierarchical Sync**: Master ports send Sync messages, Slave ports receive
- **Automatic Recovery**: Process repeats if better clocks join or failures occur

## **Diagnostic Workflow**

### **Step 1: Run Enhanced Debug Mode**
```bash
build\Debug\gptp.exe [MAC_ADDRESS] -debug-packets
```

### **Step 2: Look for These Patterns**

#### **✅ Normal Operation:**
```
*** AsCapable STATE CHANGE: ENABLED ***
*** SENDING ANNOUNCE MESSAGE #1 ***
*** RECEIVED ANNOUNCE MESSAGE #1 ***
*** BMCA: Processing STATE_CHANGE_EVENT ***
*** BMCA: We are BETTER than EBest - becoming Grandmaster ***
```

#### **⚠️ Problem - Not Ready:**
```
*** ANNOUNCE MESSAGE BLOCKED #1 *** (asCapable=false)
```

#### **⚠️ Problem - Network Issues:**
```
*** SENDING ANNOUNCE MESSAGE #1 ***
(No corresponding RECEIVED message on peer)
```

## **Common Issues & Solutions**

### **1. asCapable Never Set to True**
- **Cause**: Link state detection not working
- **Debug**: Look for `AsCapable STATE CHANGE: ENABLED`
- **Solution**: Check physical connection and interface status

### **2. Messages Sent But Not Received by Peer**
- **Cause**: Network configuration, filtering, or peer not listening
- **Debug**: Compare send vs. receive logs between nodes
- **Solution**: Use packet capture (Wireshark) to verify transmission

### **3. BMCA Not Converging**
- **Cause**: Identical priorities or clock comparison issues
- **Debug**: Monitor `BMCA:` output for comparison results
- **Solution**: Adjust priority1 values to ensure clear winner

### **4. Wrong Announce Interval**
- **Cause**: Configuration mismatch between peers
- **Debug**: Check timer intervals in debug output
- **Solution**: Verify announce interval settings match

## **Test Scenario: Direct NIC Connection**

### **Two-Node Setup**
```
Node A: Priority1=200, MAC=00:11:22:33:44:55
Node B: Priority1=220, MAC=00:66:77:88:99:AA
```

### **Expected Debug Sequence**
```
# Both nodes start
Node A: *** AsCapable STATE CHANGE: ENABLED ***
Node B: *** AsCapable STATE CHANGE: ENABLED ***

# Both send initial Announce
Node A: *** SENDING ANNOUNCE MESSAGE #1 ***
Node B: *** SENDING ANNOUNCE MESSAGE #1 ***

# Cross-reception triggers BMCA
Node A: *** RECEIVED ANNOUNCE MESSAGE #1 ***
Node B: *** RECEIVED ANNOUNCE MESSAGE #1 ***

# BMCA determines Node A is better (200 < 220)
Node A: *** BMCA: We are BETTER than EBest - becoming Grandmaster ***
Node B: *** BMCA: EBest is BETTER than us - becoming Slave ***

# Final state: A=Master, B=Slave
Node A: *** BMCA: Port X recommended state: PTP_MASTER ***
Node B: *** BMCA: Port X recommended state: PTP_SLAVE ***
```

## **Summary: Complete Announce Message Flow**

1. **Prerequisites**: `asCapable=true` (link ready) + announce timer
2. **Transmission**: Announce sent every interval (default: 1 second)
3. **Reception**: Received Announce stored as `qualified_announce`
4. **BMCA Trigger**: `STATE_CHANGE_EVENT` scheduled (+16ms)
5. **Master Selection**: All port Announces compared via priority rules
6. **Role Assignment**: Best master selected, port roles updated
7. **Network Sync**: Grandmaster provides time via Sync messages

The enhanced debugging makes this entire process visible, enabling easy diagnosis of any issues in direct NIC-to-NIC gPTP setups.
