# gPTP Deployment Guide

## ✅ **Configuration File Deployment - COMPLETED**

### **Automatic Configuration Deployment**

The build system now automatically copies configuration files to the executable output directories:

#### **Build Output Structure**
```
build/
├── Debug/
│   ├── gptp.exe               ← Debug executable
│   ├── gptp.pdb               ← Debug symbols
│   ├── gptp_cfg.ini           ← ✅ Configuration file (auto-copied)
│   └── test_milan_config.ini  ← ✅ Test configuration (auto-copied)
└── Release/
    ├── gptp.exe               ← Release executable  
    ├── gptp_cfg.ini           ← ✅ Configuration file (auto-copied)
    └── test_milan_config.ini  ← ✅ Test configuration (auto-copied)
```

#### **CMake Configuration**
The `CMakeLists.txt` has been updated with post-build steps that automatically copy configuration files:

```cmake
# Copy configuration files to output directory for deployment
add_custom_command(TARGET gptp POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/gptp_cfg.ini"
    "$<TARGET_FILE_DIR:gptp>/gptp_cfg.ini"
  COMMENT "Copying gptp_cfg.ini to output directory"
)

add_custom_command(TARGET gptp POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/test_milan_config.ini"
    "$<TARGET_FILE_DIR:gptp>/test_milan_config.ini"
  COMMENT "Copying test_milan_config.ini to output directory"
)
```

### **Deployment Benefits**

✅ **Self-Contained**: Each build output directory contains everything needed to run  
✅ **No Path Dependencies**: Configuration files are automatically found in the executable directory  
✅ **Version Isolation**: Debug and Release builds have their own configuration copies  
✅ **Distribution Ready**: Can copy entire output directory for deployment  

### **Deployment Process**

#### **For Distribution**
1. Build the desired configuration:
   ```powershell
   cmake --build build --config Release
   ```

2. Copy the entire output directory:
   ```powershell
   # For Release deployment
   robocopy build\Release deployment\gptp /E
   
   # For Debug deployment  
   robocopy build\Debug deployment\gptp_debug /E
   ```

3. The deployment directory will contain:
   ```
   deployment/gptp/
   ├── gptp.exe               ← Main executable
   ├── gptp_cfg.ini           ← Runtime configuration
   └── test_milan_config.ini  ← Alternative test configuration
   ```

#### **For Testing**
Run directly from build output directory:
```powershell
cd build\Release
.\gptp.exe [MAC_ADDRESS]
```

### **Configuration File Priority**

The application reads configuration files in this order:
1. **`gptp_cfg.ini`** in the same directory as the executable (primary)
2. **`gptp_cfg.ini`** in the current working directory (fallback)
3. **Default values** if no configuration file is found

### **Verification**

The deployment is working correctly as evidenced by:

✅ **Build Logs**: Show "Copying gptp_cfg.ini to output directory"  
✅ **File Presence**: Configuration files exist in both Debug and Release directories  
✅ **Runtime Logs**: Application logs "Reading configuration from gptp_cfg.ini"  
✅ **Parameter Application**: Milan profile parameters correctly loaded and applied  

### **Production Deployment Notes**

1. **Administrator Privileges**: For full hardware timestamping, run as Administrator
2. **Network Adapter**: Ensure Intel I210/I211 adapter is available for hardware PTP
3. **Driver Configuration**: Intel PTP hardware timestamping may require driver settings
4. **Profile Selection**: Edit `gptp_cfg.ini` to change profiles (milan/avnu/ieee1588)

### **Configuration Templates**

#### **Milan Profile** (Default in gptp_cfg.ini)
```ini
[ptp]
priority1 = 248
clockClass = 248
clockAccuracy = 0x20
offsetScaledLogVariance = 0x4000
profile = milan
```

#### **AVnu Profile**
```ini
[ptp] 
priority1 = 248
clockClass = 248
clockAccuracy = 0x21
offsetScaledLogVariance = 0x4E5D
profile = avnu
```

#### **IEEE 1588 Profile**
```ini
[ptp]
priority1 = 128
clockClass = 187
clockAccuracy = 0xFE
offsetScaledLogVariance = 0xFFFF
profile = ieee1588
```

## **Deployment Status: PRODUCTION READY** ✅

The gPTP application is now properly configured for deployment with automatic configuration file management, comprehensive error handling, and robust timestamp processing capabilities.
