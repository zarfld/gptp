# Intel Driver File Analysis Guide

## What Information Can Be Extracted from Intel Driver Files

### **1. INF Files (.inf)**

#### **Basic Driver Information:**
```ini
[Version]
Signature = "$Windows NT$"
Class = Net
ClassGUID = {4d36e972-e325-11ce-bfc1-08002be10318}
Provider = %Intel%
DriverVer = 10/25/2023,28.3.1.3
CatalogFile = e1d68x64.cat
PnpLockdown = 1
```

#### **Hardware Support:**
```ini
[Intel.NTamd64.10.0]
; Intel(R) Ethernet Controller I210
%E1210NC.DeviceDesc% = E1210.ndi, PCI\VEN_8086&DEV_1533
%E1210AT.DeviceDesc% = E1210.ndi, PCI\VEN_8086&DEV_1534
%E1210IT.DeviceDesc% = E1210.ndi, PCI\VEN_8086&DEV_1535

; Intel(R) Ethernet Controller I217
%E1217LM.DeviceDesc% = E1217.ndi, PCI\VEN_8086&DEV_153A
%E1217V.DeviceDesc% = E1217.ndi, PCI\VEN_8086&DEV_153B

; Intel(R) Ethernet Controller I219
%E1219LM.DeviceDesc% = E1219.ndi, PCI\VEN_8086&DEV_156F
%E1219V.DeviceDesc% = E1219.ndi, PCI\VEN_8086&DEV_1570
```

#### **Advanced Features:**
```ini
[E1210.ndi.reg]
HKR, Ndi\params\*PriorityVLANTag,     ParamDesc,  0, %PriorityVLANTag%
HKR, Ndi\params\*PriorityVLANTag,     default,    0, "3"
HKR, Ndi\params\*PriorityVLANTag,     type,       0, "enum"

; Hardware Timestamping Support
HKR, Ndi\params\*HwTimestamp,         ParamDesc,  0, %HwTimestamp%
HKR, Ndi\params\*HwTimestamp,         default,    0, "1"
HKR, Ndi\params\*HwTimestamp,         type,       0, "enum"

; IEEE 1588 PTP Support
HKR, Ndi\params\*IEEE1588,            ParamDesc,  0, %IEEE1588%
HKR, Ndi\params\*IEEE1588,            default,    0, "1"
HKR, Ndi\params\*IEEE1588,            type,       0, "enum"
```

### **2. System Files (.sys)**

#### **Binary Analysis Information:**
- Driver version and build information
- Supported OIDs and capabilities
- Hardware-specific code paths
- Timestamping implementation details

### **3. Registry Information**

#### **Driver Parameters:**
```
HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Class\{4d36e972-e325-11ce-bfc1-08002be10318}\0001
- DriverDesc: "Intel(R) Ethernet Controller I210"
- DriverVersion: "28.3.1.3"
- DriverDate: "10-25-2023"
- *HwTimestamp: 1
- *IEEE1588: 1
- *PriorityVLANTag: 3
```

### **4. Device Manager Information**

#### **Hardware IDs:**
```
PCI\VEN_8086&DEV_1533&SUBSYS_00000000&REV_03
PCI\VEN_8086&DEV_1533&SUBSYS_00000000
PCI\VEN_8086&DEV_1533&CC_020000
PCI\VEN_8086&DEV_1533&CC_0200
```

#### **Resources:**
- Memory ranges
- IRQ assignments
- DMA channels
- I/O ports

## **Extractable Information Categories**

### **🔍 Hardware Identification**
- PCI Vendor/Device IDs
- Subsystem identification
- Revision numbers
- Compatible device lists

### **⚙️ Feature Capabilities**
- Hardware timestamping support
- IEEE 1588 PTP capabilities
- VLAN tag priority support
- Power management features
- Advanced offload capabilities

### **📊 Performance Parameters**
- Clock frequencies
- Buffer sizes
- Interrupt coalescing settings
- DMA parameters

### **🔧 Configuration Options**
- Registry parameter defaults
- Advanced properties
- Feature enable/disable flags
- Performance tuning options

### **📝 Version Information**
- Driver version and date
- Supported OS versions
- Digital signature information
- Catalog file references
