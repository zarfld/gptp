# Intel Driver Information Extraction Script

This document provides a practical approach for extracting information from Intel driver files when they become available in a linked folder.

## **What to Look for in Intel Driver Files**

### **1. INF File Analysis**

#### **Hardware IDs (Critical for Identification):**
```ini
[Intel.NTamd64.10.0]
; Look for entries like:
%E1210NC.DeviceDesc% = E1210.ndi, PCI\VEN_8086&DEV_1533
%E1210AT.DeviceDesc% = E1210.ndi, PCI\VEN_8086&DEV_1534
%E1219LM.DeviceDesc% = E1219.ndi, PCI\VEN_8086&DEV_156F
%I225V.DeviceDesc% = I225.ndi, PCI\VEN_8086&DEV_15F3
```

#### **Timestamping Parameters:**
```ini
[E1210.ndi.reg]
; Hardware timestamp support flags
HKR, Ndi\params\*HwTimestamp,         ParamDesc,  0, %HwTimestamp%
HKR, Ndi\params\*HwTimestamp,         default,    0, "1"

; IEEE 1588 PTP support
HKR, Ndi\params\*IEEE1588,            ParamDesc,  0, %IEEE1588%
HKR, Ndi\params\*IEEE1588,            default,    0, "1"

; Clock rate information (if available)
HKR, Ndi\params\*ClockRate,           ParamDesc,  0, %ClockRate%
HKR, Ndi\params\*ClockRate,           default,    0, "125000000"
```

### **2. Registry Extraction Commands**

When analyzing a system with Intel drivers installed:

```powershell
# Find Intel network adapters
Get-WmiObject Win32_NetworkAdapter | Where-Object {$_.Name -like "*Intel*" -and $_.Name -like "*Ethernet*"}

# Get specific adapter registry info
$adapter = Get-WmiObject Win32_NetworkAdapter | Where-Object {$_.Name -like "*Intel*I210*"}
$regPath = "HKLM:\SYSTEM\CurrentControlSet\Control\Class\{4d36e972-e325-11ce-bfc1-08002be10318}\$(([string]$adapter.DeviceID).PadLeft(4,'0'))"
Get-ItemProperty $regPath
```

### **3. Driver Binary Analysis**

#### **What to extract from .sys files:**
- Driver version information
- Export tables (API functions)
- Import dependencies
- Resource sections

#### **PowerShell commands for version info:**
```powershell
# Get driver file version
$driverPath = "C:\Windows\System32\drivers\e1d68x64.sys"
(Get-Item $driverPath).VersionInfo

# Check digital signatures
Get-AuthenticodeSignature $driverPath
```

## **Critical Information for gPTP**

### **1. Hardware Identification Data**
```cpp
// From INF files - Device IDs to look for:
{0x8086, 0x1533, "Intel I210"},           // I210 Gigabit Network Connection
{0x8086, 0x1534, "Intel I210-AT"},        // I210-AT Gigabit Network Connection  
{0x8086, 0x1535, "Intel I210-IT"},        // I210-IT Gigabit Network Connection
{0x8086, 0x156F, "Intel I219-LM"},        // I219-LM Gigabit Network Connection
{0x8086, 0x1570, "Intel I219-V"},         // I219-V Gigabit Network Connection
{0x8086, 0x15F3, "Intel I225-V"},         // I225-V 2.5 Gigabit Network Connection
```

### **2. Clock Rate Information**
```cpp
// Expected clock rates from driver analysis:
{"Intel(R) Ethernet Controller I210", 125000000},      // 125 MHz
{"Intel(R) Ethernet Controller I219", 125000000},      // 125 MHz  
{"Intel(R) Ethernet Controller I225", 200000000},      // 200 MHz (newer)
```

### **3. Capability Flags**
```cpp
// Features supported by different Intel controllers:
struct IntelCapabilities {
    bool hardware_timestamping;    // Hardware TX/RX timestamps
    bool ieee1588_support;         // IEEE 1588 PTP support
    bool cross_timestamping;       // Cross-timestamping capability
    bool auxiliary_device;         // Auxiliary device for PTP
    uint32_t max_timestamp_freq;   // Maximum timestamp frequency
};
```

## **Practical Extraction Workflow**

### **Step 1: Identify Driver Files**
```bash
# Look for Intel Ethernet driver files
find /path/to/intel/drivers -name "*.inf" -exec grep -l "Ethernet\|I210\|I219\|I225" {} \;
```

### **Step 2: Extract Device IDs**
```bash
# Extract all PCI device IDs from INF files
grep -E "PCI\\VEN_8086&DEV_[0-9A-F]{4}" *.inf
```

### **Step 3: Find Timestamp Parameters**
```bash
# Look for timestamp-related registry entries
grep -E "HwTimestamp|IEEE1588|PTP|Timestamp" *.inf
```

### **Step 4: Analyze Driver Capabilities**
```bash
# Check for advanced features in driver files
strings e1d68x64.sys | grep -E "timestamp|ptp|1588|clock"
```

## **Integration with Windows Driver Info Framework**

### **Update the Device Clock Rate Map:**
```cpp
// Add discovered devices to windows_hal_vendor_intel.cpp
static DeviceClockRateMapping DeviceClockRateMap[] = {
    // From INF analysis:
    {"Intel(R) Ethernet Controller I210", 125000000},
    {"Intel(R) Ethernet Controller I219", 125000000}, 
    {"Intel(R) Ethernet Controller I225", 200000000},
    // Add new devices found in driver files...
    {NULL, 0}
};
```

### **Update Registry Parameter Map:**
```cpp
// Add registry parameters from INF analysis
static const std::map<std::string, IntelRegistryParam> intel_registry_params = {
    {"*HwTimestamp", {DWORD_TYPE, "1", "Hardware timestamping support"}},
    {"*IEEE1588", {DWORD_TYPE, "1", "IEEE 1588 PTP support"}},
    {"*ClockRate", {DWORD_TYPE, "125000000", "Hardware clock rate"}},
    // Add parameters found in INF files...
};
```

## **Why This Analysis is Critical**

### **Beyond INF Files:**
1. **INF gives you**: Basic device IDs, default settings
2. **Registry gives you**: Actual configuration, runtime settings  
3. **Driver binary gives you**: Real capabilities, version-specific features
4. **Runtime testing gives you**: Actual performance characteristics

### **Complete Picture:**
The driver file analysis provides the **static foundation** that the Windows Driver Info Framework uses to make **dynamic runtime decisions** about:
- Which timestamping methods to use
- What precision to expect
- How to optimize for specific hardware
- When to fall back to legacy methods

This addresses the TODO in `common_tstamper.hpp` by providing **comprehensive preconditions** for driver-specific tasks, not just basic device identification.
