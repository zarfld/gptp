# WinPcap to Npcap Migration Plan

## Overview
Migrate gPTP from legacy WinPcap to modern Npcap for better Windows 10/11 compatibility and improved performance.

## Current State Analysis

### WinPcap Dependencies
- **Build**: CMakeLists.txt uses `$ENV{WPCAP_DIR}` environment variable
- **Headers**: `#include <pcap.h>` from WinPcap SDK
- **Library**: Links against `wpcap.lib`
- **Interface**: Uses `rpcap://\Device\NPF_` naming convention
- **Functions**: Standard libpcap API calls

### Issues with WinPcap
1. **No Windows 10/11 Support**: WinPcap hasn't been updated since 2013
2. **Driver Issues**: WinPcap driver doesn't work reliably on modern Windows
3. **Security**: Unsigned drivers cause issues with Secure Boot
4. **Performance**: Older packet capture implementation

## Migration Strategy

### Phase 1: Environment Detection ✅
- Detect whether Npcap or WinPcap is available
- Fallback mechanism for compatibility
- Build system updates

### Phase 2: Code Compatibility ✅ 
- Npcap uses same libpcap API as WinPcap
- Interface naming remains compatible
- Minimal code changes required

### Phase 3: Build System Migration ✅
- Update CMakeLists.txt for Npcap SDK detection
- Support both WPCAP_DIR (legacy) and NPCAP_DIR (modern)
- Updated library linking

### Phase 4: Testing and Validation ✅
- Test on Windows 10/11 with Npcap
- Backwards compatibility testing with WinPcap
- Performance validation

## Implementation Details

### Build System Changes
```cmake
# Support both Npcap (preferred) and WinPcap (fallback)
if(DEFINED ENV{NPCAP_DIR})
  # Modern Npcap SDK
  set(PCAP_ROOT_DIR $ENV{NPCAP_DIR})
  set(PCAP_LIBRARY_NAME "Packet")  # Npcap uses Packet.lib instead of wpcap.lib
  message(STATUS "Using Npcap SDK from: ${PCAP_ROOT_DIR}")
elseif(DEFINED ENV{WPCAP_DIR}) 
  # Legacy WinPcap SDK
  set(PCAP_ROOT_DIR $ENV{WPCAP_DIR})
  set(PCAP_LIBRARY_NAME "wpcap")
  message(STATUS "Using WinPcap SDK from: ${PCAP_ROOT_DIR}")
else()
  message(FATAL_ERROR "Neither NPCAP_DIR nor WPCAP_DIR environment variable found")
endif()
```

### Code Changes Required
- **Minimal**: Npcap maintains libpcap API compatibility
- **Interface Naming**: Already using compatible `rpcap://\Device\NPF_` format
- **Headers**: Same `#include <pcap.h>` works with both
- **Runtime Detection**: Add capability to detect which library is active

### Benefits of Migration
1. **Windows 10/11 Compatibility**: Full support for modern Windows
2. **Better Performance**: Improved packet capture efficiency
3. **Active Maintenance**: Npcap is actively developed and maintained
4. **Security**: Properly signed drivers, Secure Boot compatible
5. **Enhanced Features**: Better timestamping, loopback capture

### Compatibility Matrix
| Feature | WinPcap | Npcap | Status |
|---------|---------|--------|--------|
| Windows 7/8 | ✅ | ✅ | Full compatibility |
| Windows 10 | ⚠️ | ✅ | Npcap recommended |
| Windows 11 | ❌ | ✅ | Npcap required |
| libpcap API | ✅ | ✅ | Full compatibility |
| Hardware Timestamping | Limited | Enhanced | Npcap improved |

## Migration Phases

### Phase 1: Dual SDK Support (Immediate)
- Update build system to detect both SDKs
- Prefer Npcap when available, fallback to WinPcap
- No runtime code changes needed

### Phase 2: Runtime Detection (Optional)
- Add capability to detect which library is running
- Log information about packet capture backend
- Enhanced error reporting

### Phase 3: Npcap-Specific Features (Future)
- Leverage Npcap's enhanced timestamping
- Improved loopback capture for testing
- Better performance optimizations

## Risk Assessment
- **Low Risk**: API compatibility means minimal code changes
- **High Reward**: Modern Windows compatibility essential
- **Backwards Compatible**: Maintains WinPcap support for legacy systems

## Dependencies
- **Npcap SDK**: Available from https://npcap.com/dist/
- **Build Environment**: Visual Studio 2019+ recommended
- **Testing**: Windows 10/11 systems with Npcap installed

---
**Priority**: HIGH - Essential for modern Windows compatibility
**Effort**: LOW - Minimal code changes due to API compatibility  
**Impact**: HIGH - Enables Windows 10/11 support
