# Automotive Profile Adaptation Analysis - AVB Specification 1.6 Compliance

## Document: "Automotive Ethernet AVB Functional and Interoperability Specification" Revision 1.6 (07 November 2019)

### Key Findings from Section 6 (gPTP):

## 1. Critical Missing Features (URGENT):

### 1.1 Table 12 Timing Requirements Not Properly Implemented
**SPEC REQUIREMENT**: Section 6.2.6 specifies different timing values for Time Critical vs Non-Time Critical ports:

**Time Critical Ports:**
- initialLogPDelayReqInterval: 1s (fixed)
- operLogPdelayReqInterval: 1s-8s (0-3)
- initialLogSyncInterval: 31.25ms-125ms (-5 to -3)
- operLogSyncInterval: 125ms-1s (-3 to 0)

**Non-Time Critical Ports:**
- initialLogPDelayReqInterval: 1s (fixed)
- operLogPdelayReqInterval: 1s-8s (0-3)
- initialLogSyncInterval: 125ms (-3)
- operLogSyncInterval: 125ms-1s (-3 to 0)

**CURRENT IMPL**: Fixed values, needs port type configuration and compliant ranges.

### 1.2 Operational vs Initial Interval Switching Logic Missing
**SPEC REQUIREMENT**: Section 6.2.3.1 and 6.2.3.2 - Devices must:
- Start with initial intervals for fast startup
- Switch to operational intervals after 60s for reduced overhead
- Support gPTP signaling messages for interval requests
- Process signaling within 2 intervals or 250ms (whichever is larger)
- Send signaling within 60s after achieving synchronization

**CURRENT IMPL**: Has fields but no switching logic implemented.

### 1.3 sourcePortIdentity Verification Must Be Disabled
**SPEC REQUIREMENT**: Section 6.3 - "No verification of sourcePortIdentity field of Sync and Follow_Up messages"
**CURRENT IMPL**: Standard verification still enabled, needs to be disabled for automotive.

### 1.4 asCapable Threshold Behavior Different from Standard
**SPEC REQUIREMENT**: Section 6.2.2.1 - "port shall not allow to set asCapable if neighborPropDelay exceeds a configured threshold"
**CURRENT IMPL**: Standard behavior that sets asCapable=false on threshold exceeded.

## 2. Important Features to Implement (HIGH PRIORITY):

### 2.1 gPTP Signaling Message Support
**SPEC REQUIREMENT**: Section 6.2.4 - Support for interval change requests
- AED slaves should support transmission of signaling TLV
- AED masters shall support processing of signaling TLV
- Only timeSyncInterval requests (linkDelayInterval and announceInterval set to 127)
- Process within 2 intervals or 250ms

### 2.2 Persistent Storage for Performance
**SPEC REQUIREMENT**: Section 6.2.2 - Store in non-volatile memory:
- neighborPropDelay (restore on startup for faster convergence)
- rateRatio (optional, may improve startup accuracy)
- neighborRateRatio (optional, may improve startup accuracy)
- Update stored values if actual differs by >100ns

### 2.3 Automotive-Specific Sync Timeout Behavior
**SPEC REQUIREMENT**: Section 6.3 - Special behaviors for AED-B and AED-E:
- AED-B: Continue sending sync using last valid record during loss of sync
- AED-B: Use default time record if no sync within 20s on startup
- AED-E: Holdover behavior during sync loss
- AED-E: Handle time discontinuities per section 9.3.2

### 2.4 Follow_Up TLV Requirement
**SPEC REQUIREMENT**: Section 6.2.5 - "AEDs shall include the Follow_Up information TLV on all Follow_Up messages"
**CURRENT IMPL**: ✅ Correctly enabled

## 3. Configuration Issues (MEDIUM PRIORITY):

### 3.1 Announce Message Behavior
**SPEC REQUIREMENT**: Section 6.3 - "Announce messages are neither required nor expected"
**CURRENT IMPL**: ⚠️ May still send announces, should be completely disabled

### 3.2 Port Type Classification Missing
**SPEC REQUIREMENT**: Table 12 - Different timing requirements for time critical vs non-time critical ports
**CURRENT IMPL**: No port type classification, uses single timing set

### 3.3 Fixed Grandmaster Configuration
**SPEC REQUIREMENT**: Section 6.2.1.1 - isGM variable, preconfigured port roles
**CURRENT IMPL**: Partial - BMCA disabled but no isGM variable

## 4. Recommendations for Implementation:

### PHASE 1 (Critical - Fix Compliance Issues):
1. ✅ **COMPLETED**: Disable BMCA (`bmca_enabled = false`)
2. ✅ **COMPLETED**: Set asCapable=true on link up (`as_capable_on_link_up = true`)
3. ❌ **TODO**: Disable sourcePortIdentity verification completely
4. ❌ **TODO**: Implement Table 12 timing ranges with port type classification
5. ❌ **TODO**: Disable announce transmission completely (`disable_announce_transmission = true`)
6. ❌ **TODO**: Modify asCapable threshold behavior (don't set false on threshold)

### PHASE 2 (Enhanced Features):
1. ❌ **TODO**: Implement initial→operational interval switching logic
2. ❌ **TODO**: Add gPTP signaling message support
3. ❌ **TODO**: Implement persistent storage for neighborPropDelay
4. ❌ **TODO**: Add automotive-specific sync timeout behaviors

### PHASE 3 (Optimization):
1. ❌ **TODO**: Add port type configuration (time critical vs non-time critical)
2. ❌ **TODO**: Implement persistent storage for rate ratios
3. ❌ **TODO**: Add isGM variable and fixed grandmaster logic
4. ❌ **TODO**: Implement "Avnu 802.1AS Recovered Clock Quality Testing" recommendation

## 5. Current Implementation Status:

### ✅ **CORRECTLY IMPLEMENTED**:
- BMCA disabled (`supports_bmca = false`, `bmca_enabled = false`)
- asCapable=true on link up (`as_capable_on_link_up = true`)
- Follow_Up TLVs enabled (`follow_up_enabled = true`)
- Automotive test status messages (`automotive_test_status = true`)
- Basic persistent storage fields present
- Initial/operational interval fields present

### ⚠️ **PARTIALLY IMPLEMENTED**:
- Announce behavior (fields present but may still send)
- sourcePortIdentity checking (field present but not used)
- Timing intervals (fields present but fixed values)

### ❌ **MISSING IMPLEMENTATION**:
- Table 12 compliant timing ranges
- Initial→operational interval switching logic
- gPTP signaling message support
- Automotive-specific sync timeout behaviors
- Port type classification
- Persistent storage actual implementation
- sourcePortIdentity verification disable

## 6. Next Steps:

1. **IMMEDIATE**: Fix critical compliance issues (Phase 1)
2. **SHORT TERM**: Implement interval switching and signaling (Phase 2)
3. **LONG TERM**: Add optimization features (Phase 3)
4. **TESTING**: Create automotive profile compliance test suite
5. **DOCUMENTATION**: Update deployment guides for automotive use cases

This analysis shows our automotive profile has the right foundation but needs significant work to be fully AVB Spec 1.6 compliant.
