# Automotive Profile Analysis - AVB Specification 1.6 Compliance

## Document Analysis: Automotive Ethernet AVB Functional and Interoperability Specification Rev 1.6

### Key Requirements from Section 6 (gPTP):

#### 6.2.1 Static gPTP Values

1. **6.2.1.1 Grandmaster Information and Topology**
   - Fixed gPTP GrandMaster (AED-GM) expected
   - No BMCA support required
   - isGM variable for each AED (TRUE for GM, FALSE for others)
   - Port roles are preconfigured (master/slave)

2. **6.2.1.2 asCapable**
   - ✅ **SPEC REQUIREMENT**: Set asCapable=TRUE on link up for time-aware ports
   - ✅ **CURRENT IMPL**: `as_capable_on_link_up = true`

3. **6.2.1.3 initialLogPdelayReqInterval**
   - ❌ **SPEC REQUIREMENT**: Preconfigurable, see Table 12
   - ❌ **CURRENT IMPL**: Fixed at 0 (1s), needs range support

4. **6.2.1.4 initialLogSyncInterval**
   - ❌ **SPEC REQUIREMENT**: Preconfigurable, see Table 12
   - ❌ **CURRENT IMPL**: Fixed at 0 (1s), needs range support

5. **6.2.1.5 operLogPdelayReqInterval**
   - ❌ **MISSING**: Not implemented in current profile
   - ❌ **SPEC REQUIREMENT**: 1s-8s operational interval

6. **6.2.1.6 operLogSyncInterval**
   - ❌ **MISSING**: Not implemented in current profile  
   - ❌ **SPEC REQUIREMENT**: 125ms-1s operational interval

#### 6.2.2 Persistent gPTP Values

1. **6.2.2.1 neighborPropDelay**
   - ❌ **SPEC REQUIREMENT**: Store in non-volatile memory, restore on startup
   - ❌ **SPEC REQUIREMENT**: Don't set asCapable=false if exceeds threshold (different from standard)
   - ❌ **SPEC REQUIREMENT**: Update stored value if actual differs by >100ns
   - ⚠️ **CURRENT IMPL**: Standard threshold behavior

2. **6.2.2.2 rateRatio**
   - ❌ **MISSING**: Persistent storage not implemented

3. **6.2.2.3 neighborRateRatio**
   - ❌ **MISSING**: Persistent storage not implemented

#### 6.2.3 Management Updated Values

1. **6.2.3.1 SyncInterval Management**
   - ❌ **SPEC REQUIREMENT**: Support gPTP Signaling for interval adjustment
   - ❌ **SPEC REQUIREMENT**: Initial vs operational intervals
   - ❌ **SPEC REQUIREMENT**: Process signaling within 2 intervals or 250ms
   - ❌ **SPEC REQUIREMENT**: Send signaling within 60s after sync

2. **6.2.3.2 pdelayReqInterval Management**
   - ❌ **SPEC REQUIREMENT**: Move to operational interval after stabilization
   - ❌ **SPEC REQUIREMENT**: 60s timeout for interval change

#### 6.2.6 Required Values (Table 12)

**Time Critical Ports:**
- initialLogPDelayReqInterval: 1s (0)
- operLogPdelayReqInterval: 1s-8s (0-3)
- initialLogSyncInterval: 31.25ms-125ms (-5 to -3)
- operLogSyncInterval: 125ms-1s (-3 to 0)

**Non-Time Critical Ports:**
- initialLogPDelayReqInterval: 1s (0)
- operLogPdelayReqInterval: 1s-8s (0-3)
- initialLogSyncInterval: 125ms (-3)
- operLogSyncInterval: 125ms-1s (-3 to 0)

#### 6.3 gPTP Operation Differences

1. **BMCA Behavior**
   - ✅ **SPEC REQUIREMENT**: BMCA shall not execute
   - ✅ **CURRENT IMPL**: `supports_bmca = false`

2. **Announce Messages**
   - ✅ **SPEC REQUIREMENT**: No announce messages required/expected
   - ⚠️ **CURRENT IMPL**: `send_announce_when_as_capable_only = false` (may send announces)

3. **sourcePortIdentity Verification**
   - ❌ **SPEC REQUIREMENT**: No verification of sourcePortIdentity (no announces)
   - ❌ **MISSING**: Need to disable verification

4. **syncReceiptTimeout Behavior**
   - ❌ **SPEC REQUIREMENT**: Modified behavior for AED-B devices
   - ❌ **SPEC REQUIREMENT**: Holdover behavior for AED-E devices
   - ❌ **MISSING**: Special automotive timeout handling

5. **Follow_Up TLVs**
   - ✅ **SPEC REQUIREMENT**: Include Follow_Up information TLV
   - ✅ **CURRENT IMPL**: `follow_up_enabled = true`

## Issues with Current Implementation:

### ❌ Critical Missing Features:
1. **Operational Intervals**: No support for initial vs operational log intervals
2. **Persistent Storage**: No neighborPropDelay/rate ratio persistence
3. **Signaling Support**: No gPTP signaling message handling
4. **Custom Timeout Behavior**: Missing automotive-specific sync timeout handling
5. **Neighbor Delay Threshold**: Should not cause asCapable=false in automotive

### ⚠️ Incorrect Configurations:
1. **Timing Intervals**: Fixed values don't match specification ranges
2. **Announce Behavior**: May still send announces (should not in automotive)
3. **sourcePortIdentity**: Standard verification still enabled

### ✅ Correct Implementations:
1. **asCapable on Link Up**: Correctly set to true
2. **BMCA Disabled**: Correctly disabled
3. **Follow_Up TLVs**: Correctly enabled
4. **Test Status**: Automotive test status enabled

## Recommended Actions:

1. **URGENT**: Update automotive profile with correct timing intervals
2. **HIGH**: Implement operational vs initial interval support
3. **HIGH**: Disable announce message transmission in automotive mode
4. **MEDIUM**: Add persistent storage for neighborPropDelay
5. **MEDIUM**: Implement gPTP signaling support
6. **LOW**: Add automotive-specific sync timeout behavior
