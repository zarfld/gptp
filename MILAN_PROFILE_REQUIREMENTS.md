# Milan Baseline Profile Implementation Plan

## Current Status
❌ **No Milan-specific profile exists in this gPTP implementation**

The codebase supports:
- ✅ Standard IEEE 802.1AS profile
- ✅ AVnu Automotive Profile  
- ❌ Milan Baseline Interoperability Specification Chapter 5.6 gPTP requirements

## Required Milan Profile Features

### 1. Enhanced Timing Requirements
- **Sub-microsecond accuracy**: Stricter than standard gPTP
- **Fast convergence**: < 100ms network convergence time
- **Jitter limits**: Tighter phase/frequency stability requirements

### 2. BMCA Enhancements
- **Priority-based selection**: Enhanced priority schemes for TSN
- **Stream-aware selection**: Consider traffic load in master selection
- **Redundancy support**: Multiple grandmaster backup mechanisms

### 3. Configuration Requirements
```cpp
// Proposed Milan profile configuration
typedef struct {
    bool milan_profile;                    // Enable Milan compliance mode
    uint32_t max_convergence_time_ms;      // < 100ms requirement
    uint32_t max_sync_jitter_ns;          // Stricter jitter limits
    uint32_t max_path_delay_variation_ns;  // Path delay stability
    bool stream_aware_bmca;               // Consider traffic in BMCA
    bool redundant_gm_support;            // Multiple GM support
} MilanProfileConfig_t;
```

### 4. Interoperability Testing
- **Certification requirements**: Milan compliance testing
- **Multi-vendor compatibility**: Interop with other Milan devices
- **Performance validation**: Meet timing specifications

## Implementation Strategy

### Phase 1: Profile Infrastructure
1. Add `milan_profile` flag similar to `automotive_profile`
2. Create Milan-specific timing parameters
3. Implement Milan configuration parsing

### Phase 2: Enhanced BMCA
1. Stream-aware grandmaster selection
2. Redundancy mechanisms
3. Fast convergence optimizations

### Phase 3: Timing Enhancements
1. Stricter clock synchronization
2. Enhanced jitter control
3. Path delay optimization

### Phase 4: Validation & Certification
1. Milan compliance testing suite
2. Interoperability validation
3. Performance benchmarking

## Code Integration Points

### Configuration Setup
```cpp
// In daemon_cl.cpp
portInit.milan_profile = parse_milan_config();
if (portInit.milan_profile) {
    setup_milan_timing_requirements();
    setup_milan_bmca_enhancements();
}
```

### Runtime Behavior
```cpp
// In common_port.cpp
if (getMilanProfile()) {
    // Milan-specific BMCA logic
    // Enhanced timing requirements
    // Stream awareness
}
```

## Current Workarounds

Until Milan profile is implemented:

1. **Use Automotive Profile** as baseline:
   ```bash
   gptp.exe MAC_ADDRESS -V  # Enable automotive profile
   ```

2. **Manual Configuration**:
   - Set aggressive sync intervals
   - Use fixed grandmaster mode
   - Monitor timing performance

3. **Custom Tuning**:
   - Adjust timing parameters in source
   - Enable enhanced debugging
   - Validate against Milan requirements

## References

- IEEE 802.1AS-2020 (latest gPTP standard)
- AVnu Alliance Milan Baseline Interoperability Specification v2.0a
- Chapter 5.6 gPTP Profile Requirements
- TSN timing and synchronization best practices
