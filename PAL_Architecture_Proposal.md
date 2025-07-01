# Profile Abstraction Layer (PAL) Architecture Proposal

## 🎯 **Objective**
Transform the current scattered profile-specific code into a clean, maintainable Profile Abstraction Layer (PAL) similar to the Hardware Abstraction Layer (HAL) pattern.

## 📊 **Current State Analysis**

### **Issues with Current Implementation**
1. **Scattered Profile Logic**: 42+ `if(getProfile())` checks across multiple files
2. **Code Duplication**: Repeated profile detection patterns
3. **Maintenance Complexity**: Adding new profiles requires changes in multiple locations
4. **Configuration Inconsistency**: Different config file formats and naming
5. **Mixed Concerns**: Profile logic intertwined with core gPTP functionality

### **Current Profile-Specific Locations**
```
daemon_cl.cpp:           Profile detection, config parsing, command line handling
common_port.cpp:         Profile flag copying, Milan stats initialization  
ether_port.cpp:          42+ profile checks for timing, asCapable, behavior
ptp_message.cpp:         Profile-specific PDelay handling, asCapable logic
windows_hal.cpp:         Profile-aware link monitoring
```

## 🏗️ **Proposed PAL Architecture**

### **1. Interface-Based Design**
```cpp
class ProfileInterface {
    // Pure virtual methods for profile-specific behavior
    virtual ProfileTimingConfig getTimingConfig() const = 0;
    virtual ProfileAsCapableBehavior getAsCapableBehavior() const = 0;
    virtual bool evaluateAsCapable(...) const = 0;
    // ... other profile-specific methods
};
```

### **2. Concrete Profile Implementations**
```cpp
class MilanProfile : public ProfileInterface { /* Milan-specific logic */ };
class AvnuBaseProfile : public ProfileInterface { /* AVnu Base logic */ };
class AutomotiveProfile : public ProfileInterface { /* Automotive logic */ };
class StandardProfile : public ProfileInterface { /* IEEE 1588 logic */ };
```

### **3. Profile Factory Pattern**
```cpp
class ProfileFactory {
    static std::unique_ptr<ProfileInterface> createProfile(ProfileType type);
    static std::unique_ptr<ProfileInterface> createProfile(const std::string& name);
};
```

## 🔧 **Implementation Benefits**

### **Before (Current)**
```cpp
// Scattered throughout codebase
if( getMilanProfile() ) {
    // Milan-specific behavior
    setAsCapable(true);
    milan_sync_interval = -3;
    // ... more Milan logic
} else if( getAvnuBaseProfile() ) {
    // AVnu Base-specific behavior  
    setAsCapable(false);
    avnu_sync_interval = 0;
    // ... more AVnu logic
} else if( getAutomotiveProfile() ) {
    // Automotive-specific behavior
    // ... automotive logic
} else {
    // Standard behavior
    // ... standard logic
}
```

### **After (PAL)**
```cpp
// Clean, centralized
auto profile = ProfileFactory::createProfile(profile_type);
ProfileTimingConfig timing = profile->getTimingConfig();
bool as_capable = profile->evaluateAsCapable(pdelay_count, current_state, ...);
bool should_send_announce = profile->shouldSendAnnounce(as_capable, is_gm);
```

## 📁 **File Organization**

### **New PAL Structure**
```
common/
├── profile_interface.hpp          # Base interface definition
├── milan_profile.hpp/.cpp         # Milan profile implementation
├── avnu_base_profile.hpp/.cpp     # AVnu Base profile implementation  
├── automotive_profile.hpp/.cpp    # Automotive profile implementation
├── standard_profile.hpp/.cpp      # Standard IEEE 1588 profile
├── profile_factory.hpp/.cpp       # Profile factory
└── profile_config.hpp/.cpp        # Profile configuration management
```

### **Configuration Management**
```
profiles/
├── milan_profile.ini              # Milan-specific configuration
├── avnu_base_profile.ini          # AVnu Base configuration
├── automotive_profile.ini         # Automotive configuration
└── standard_profile.ini           # Standard configuration
```

## 🚀 **Migration Strategy**

### **Phase 1: Interface Definition**
- [x] Create `ProfileInterface` base class
- [x] Define profile-specific configuration structures
- [x] Create `MilanProfile` concrete implementation

### **Phase 2: Factory Implementation**
- [ ] Implement `ProfileFactory` class
- [ ] Create remaining profile implementations (`AvnuBaseProfile`, `AutomotiveProfile`, `StandardProfile`)
- [ ] Add profile auto-detection logic

### **Phase 3: Core Integration**
- [ ] Update `CommonPort` to use profile interface
- [ ] Replace scattered profile checks with profile method calls
- [ ] Refactor `ether_port.cpp` to use PAL

### **Phase 4: Configuration Standardization**
- [ ] Standardize profile configuration file formats
- [ ] Implement profile-specific config loading
- [ ] Add configuration validation

### **Phase 5: Testing & Validation**
- [ ] Unit tests for each profile implementation
- [ ] Integration testing with existing functionality
- [ ] Performance validation
- [ ] Compliance testing for each profile

## 📈 **Expected Improvements**

### **Code Maintainability**
- **90% reduction** in scattered profile checks
- **Single location** for profile-specific logic
- **Easy addition** of new profiles without touching core code

### **Configuration Management**
- **Consistent** configuration file format across profiles
- **Profile-specific** parameter validation
- **Automatic** profile detection and loading

### **Testing & Validation**
- **Isolated** testing of profile-specific behavior
- **Compliance checking** built into each profile
- **Performance monitoring** per profile

### **Developer Experience**
- **Clear separation** between core gPTP and profile logic
- **Self-documenting** code through interface contracts
- **Easier debugging** with centralized profile behavior

## 🎯 **Compatibility & Migration**

### **Backward Compatibility**
- All existing configuration files continue to work
- Current command line options remain supported
- No changes to external APIs

### **Gradual Migration**
- PAL can be introduced alongside existing code
- Profile-by-profile migration possible
- Existing tests continue to pass during transition

### **Future Extensibility**
- New profiles (e.g., TSN profiles) easily added
- Profile-specific features isolated
- Configuration management standardized

## 📋 **Next Steps**

1. **Review and approve** PAL architecture design
2. **Implement ProfileFactory** and remaining profile classes
3. **Begin integration** with CommonPort and EtherPort
4. **Test Milan profile** implementation with current functionality
5. **Migrate remaining profiles** one by one
6. **Standardize configuration** file formats
7. **Add comprehensive testing** for all profiles

This PAL architecture will significantly improve code maintainability, make adding new profiles easier, and provide a clean separation between core gPTP functionality and profile-specific behavior.
