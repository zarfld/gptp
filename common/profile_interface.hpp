/**
 * @file profile_interface.hpp
 * @brief Profile Abstraction Layer (PAL) Interface Definition
 * 
 * This file defines the interface for different gPTP profiles, similar to the
 * Hardware Abstraction Layer (HAL) pattern. Each profile implements specific
 * behaviors for timing, asCapable management, and protocol compliance.
 * 
 * Supported Profiles:
 * - Standard IEEE 802.1AS
 * - Milan Baseline Interoperability Profile
 * - AVnu Base/ProAV Functional Interoperability Profile  
 * - AVnu Automotive Profile
 */

#ifndef PROFILE_INTERFACE_HPP
#define PROFILE_INTERFACE_HPP

#include <string>
#include <memory>
#include "avbts_osnet.hpp"
#include "common_tstamper.hpp"

// Forward declarations
class CommonPort;
class IEEE1588Clock;

/**
 * @brief Profile-specific timing configuration
 */
struct ProfileTimingConfig {
    int8_t sync_interval_log;           // Sync interval (log2 seconds)
    int8_t announce_interval_log;       // Announce interval (log2 seconds)  
    int8_t pdelay_interval_log;         // PDelay interval (log2 seconds)
    unsigned int sync_receipt_timeout;  // Sync receipt timeout multiplier
    unsigned int announce_receipt_timeout; // Announce receipt timeout multiplier
    int64_t neighbor_prop_delay_thresh; // Neighbor propagation delay threshold (ns)
};

/**
 * @brief Profile-specific clock quality configuration
 */
struct ProfileClockQuality {
    uint8_t clock_class;               // Clock class value
    uint8_t clock_accuracy;            // Clock accuracy value
    uint16_t offset_scaled_log_variance; // Offset scaled log variance
    uint8_t priority1;                 // Priority1 value
    uint8_t priority2;                 // Priority2 value
};

/**
 * @brief Profile-specific asCapable behavior configuration
 */
struct ProfileAsCapableBehavior {
    bool initial_as_capable;           // Initial asCapable state
    unsigned int min_pdelay_successes; // Minimum PDelay successes to set asCapable=true
    unsigned int max_pdelay_successes; // Maximum PDelay successes threshold
    bool maintain_on_late_response;    // Maintain asCapable on late (but not missing) responses
    unsigned int late_response_threshold_ms; // Threshold for "late" responses (ms)
    unsigned int consecutive_late_limit; // Max consecutive late responses before losing asCapable
};

/**
 * @brief Abstract base class for gPTP profile implementations
 * 
 * This interface defines the contract that all profile implementations must follow.
 * Each profile provides specific behaviors for timing, asCapable management,
 * and protocol compliance requirements.
 */
class ProfileInterface {
public:
    virtual ~ProfileInterface() = default;

    // Profile identification
    virtual std::string getProfileName() const = 0;
    virtual std::string getProfileVersion() const = 0;

    // Configuration methods
    virtual ProfileTimingConfig getTimingConfig() const = 0;
    virtual ProfileClockQuality getClockQuality() const = 0;
    virtual ProfileAsCapableBehavior getAsCapableBehavior() const = 0;

    // Runtime behavior methods
    virtual bool shouldSendAnnounce(bool as_capable, bool is_grandmaster) const = 0;
    virtual bool shouldProcessSync(bool as_capable) const = 0;
    virtual bool shouldStartPDelay(bool link_up) const = 0;

    // asCapable state management
    virtual bool evaluateAsCapable(
        unsigned int pdelay_count,
        bool current_as_capable,
        bool pdelay_success,
        bool pdelay_timeout,
        bool pdelay_late,
        unsigned int consecutive_late_count
    ) const = 0;

    // Link state management
    virtual bool evaluateAsCapableOnLinkUp(bool link_up) const = 0;
    virtual bool evaluateAsCapableOnLinkDown(bool link_down) const = 0;

    // Protocol-specific behaviors
    virtual bool allowsNegativeCorrectionField() const = 0;
    virtual bool requiresStrictTimeouts() const = 0;
    virtual bool supportsBMCA() const = 0;

    // Statistics and monitoring
    virtual void updateTimingStats(uint64_t sync_timestamp, uint64_t arrival_time) {}
    virtual void updatePDelayStats(uint64_t path_delay_ns) {}
    virtual bool checkComplianceRequirements() const { return true; }

    // Configuration loading
    virtual bool loadFromConfigFile(const std::string& config_path) { return true; }
    virtual bool validateConfiguration() const { return true; }
};

/**
 * @brief Profile factory for creating profile instances
 */
class ProfileFactory {
public:
    enum ProfileType {
        PROFILE_STANDARD,
        PROFILE_MILAN,
        PROFILE_AVNU_BASE,
        PROFILE_AUTOMOTIVE
    };

    static std::unique_ptr<ProfileInterface> createProfile(ProfileType type);
    static std::unique_ptr<ProfileInterface> createProfile(const std::string& profile_name);
    static ProfileType getProfileTypeFromString(const std::string& profile_name);
    static std::string getProfileNameFromType(ProfileType type);
};

#endif // PROFILE_INTERFACE_HPP
