/**
 * @file gptp_profile.hpp
 * @brief Unified gPTP Profile Configuration Structure
 * 
 * This file defines a single unified structure that contains all profile-specific
 * settings and behaviors. Instead of scattered profile checks throughout the code,
 * all code simply reads from the current profile configuration.
 * 
 * This approach is much simpler and cleaner than interface-based polymorphism.
 */

#ifndef GPTP_PROFILE_HPP
#define GPTP_PROFILE_HPP

#include <string>
#include <cstdint>

/**
 * @brief Unified gPTP Profile Configuration
 * 
 * This structure contains all profile-specific settings and behaviors.
 * Different profiles (Milan, AVnu Base, Automotive, Standard) are just
 * different initializations of this same structure.
 */
struct gPTPProfile {
    // Profile identification
    std::string profile_name;              // "milan", "avnu_base", "automotive", "standard"
    std::string profile_version;           // Profile version string
    std::string profile_description;       // Human-readable description

    // Timing configuration
    int8_t sync_interval_log;              // Sync interval (log2 seconds)
    int8_t announce_interval_log;          // Announce interval (log2 seconds)
    int8_t pdelay_interval_log;            // PDelay interval (log2 seconds)
    
    // Timeout and receipt configuration
    unsigned int sync_receipt_timeout;     // Sync receipt timeout multiplier
    unsigned int announce_receipt_timeout; // Announce receipt timeout multiplier
    unsigned int pdelay_receipt_timeout;   // PDelay receipt timeout multiplier
    int8_t delay_req_interval_log;         // DelayReq interval (log2 seconds) for non-PDelay
    
    // Message rate configuration
    unsigned int announce_receipt_timeout_multiplier; // Announce timeout multiplier
    unsigned int pdelay_receipt_timeout_multiplier;   // PDelay timeout multiplier
    
    // Threshold configuration
    int64_t neighbor_prop_delay_thresh;    // Neighbor propagation delay threshold (ns)
    unsigned int sync_receipt_thresh;      // Sync receipt threshold
    
    // Clock quality configuration
    uint8_t clock_class;                   // Clock class value
    uint8_t clock_accuracy;                // Clock accuracy value
    uint16_t offset_scaled_log_variance;   // Offset scaled log variance
    uint8_t priority1;                     // Priority1 value
    uint8_t priority2;                     // Priority2 value
    
    // asCapable behavior configuration
    bool initial_as_capable;               // Initial asCapable state on startup
    bool as_capable_on_link_up;            // Set asCapable=true when link comes up
    bool as_capable_on_link_down;          // Set asCapable=false when link goes down
    unsigned int min_pdelay_successes;     // Minimum PDelay successes to set asCapable=true
    unsigned int max_pdelay_successes;     // Maximum PDelay successes threshold (0=unlimited)
    bool maintain_as_capable_on_timeout;   // Maintain asCapable=true on PDelay timeout
    bool maintain_as_capable_on_late_response; // Maintain asCapable=true on late responses
    
    // Late response handling (Milan Annex B.2.3)
    unsigned int late_response_threshold_ms;   // Threshold for "late" responses (ms)
    unsigned int consecutive_late_limit;       // Max consecutive late responses before losing asCapable
    bool reset_pdelay_count_on_timeout;      // Whether to reset pdelay_count on timeout
    
    // Protocol behavior flags
    bool send_announce_when_as_capable_only;  // Only send announce when asCapable=true
    bool process_sync_regardless_as_capable;  // Process sync even when asCapable=false
    bool start_pdelay_on_link_up;            // Start PDelay immediately when link up
    bool allows_negative_correction_field;   // Allow negative correction field
    bool requires_strict_timeouts;           // Require strict timeout handling
    bool supports_bmca;                      // Support Best Master Clock Algorithm
    
    // Profile-specific features
    bool stream_aware_bmca;                // Stream-aware BMCA (Milan)
    bool redundant_gm_support;             // Redundant grandmaster support (Milan)
    bool automotive_test_status;           // Automotive test status messages
    bool bmca_enabled;                     // BMCA enabled/disabled control
    bool follow_up_enabled;                // FollowUp message enabled
    
    // Test and diagnostic features
    unsigned int test_status_interval_log; // Test status message interval (Automotive)
    bool force_slave_mode;                 // Force slave mode (disable grandmaster)
    
    // Performance and compliance limits
    uint32_t max_convergence_time_ms;      // Maximum convergence time (0=no limit)
    uint32_t max_sync_jitter_ns;           // Maximum sync jitter (0=no limit)
    uint32_t max_path_delay_variation_ns;  // Maximum path delay variation (0=no limit)
    
    // Runtime statistics (mutable, updated during operation)
    mutable struct {
        uint64_t convergence_start_time;    // When convergence started (ns)
        uint64_t last_convergence_time;     // Last measured convergence time (ns)
        uint32_t current_sync_jitter_ns;    // Current sync jitter (ns)
        uint32_t current_path_delay_variation_ns; // Current path delay variation (ns)
        uint64_t total_sync_messages;       // Total sync messages received
        uint64_t total_announce_messages;   // Total announce messages received
        uint64_t total_pdelay_messages;     // Total PDelay messages sent
        bool convergence_achieved;          // Whether convergence target was met
        unsigned int consecutive_late_responses; // Current consecutive late response count
        unsigned int consecutive_missing_responses; // Current consecutive missing response count
    } stats;

    // Constructor with defaults (Standard IEEE 802.1AS profile)
    gPTPProfile() {
        // Default to Standard IEEE 802.1AS profile
        profile_name = "standard";
        profile_version = "1.0";
        profile_description = "Standard IEEE 802.1AS Profile";
        
        // Standard timing intervals
        sync_interval_log = 0;              // 1 second
        announce_interval_log = 0;          // 1 second
        pdelay_interval_log = 0;            // 1 second
        
        // Standard timeouts
        sync_receipt_timeout = 3;
        announce_receipt_timeout = 3;
        pdelay_receipt_timeout = 3;
        delay_req_interval_log = 0;         // 1 second
        
        // Standard message rate configuration
        announce_receipt_timeout_multiplier = 3;
        pdelay_receipt_timeout_multiplier = 3;
        
        // Standard thresholds
        neighbor_prop_delay_thresh = 800000; // 800μs
        sync_receipt_thresh = 3;
        
        // Standard clock quality
        clock_class = 248;
        clock_accuracy = 0xFE;
        offset_scaled_log_variance = 0x4E5D;
        priority1 = 248;
        priority2 = 248;
        
        // Standard asCapable behavior
        initial_as_capable = false;
        as_capable_on_link_up = false;
        as_capable_on_link_down = true;
        min_pdelay_successes = 1;
        max_pdelay_successes = 0;           // No upper limit
        maintain_as_capable_on_timeout = false;
        maintain_as_capable_on_late_response = false;
        
        // Standard late response handling
        late_response_threshold_ms = 10;
        consecutive_late_limit = 3;
        reset_pdelay_count_on_timeout = true;
        
        // Standard protocol behavior
        send_announce_when_as_capable_only = true;
        process_sync_regardless_as_capable = true;
        start_pdelay_on_link_up = true;
        allows_negative_correction_field = false;
        requires_strict_timeouts = false;
        supports_bmca = true;
        
        // Standard features (disabled)
        stream_aware_bmca = false;
        redundant_gm_support = false;
        automotive_test_status = false;
        bmca_enabled = true;                // BMCA enabled by default
        follow_up_enabled = true;           // FollowUp enabled by default
        
        // Test and diagnostic features
        test_status_interval_log = 0;       // 1 second
        force_slave_mode = false;           // Allow grandmaster mode
        
        // No compliance limits for standard profile
        max_convergence_time_ms = 0;
        max_sync_jitter_ns = 0;
        max_path_delay_variation_ns = 0;
        
        // Initialize statistics
        memset(&stats, 0, sizeof(stats));
    }
};

/**
 * @brief Profile factory functions to create specific profile configurations
 */
namespace gPTPProfileFactory {
    
    /**
     * @brief Create Milan Baseline Interoperability Profile configuration
     */
    gPTPProfile createMilanProfile();
    
    /**
     * @brief Create AVnu Base/ProAV Functional Interoperability Profile configuration
     */
    gPTPProfile createAvnuBaseProfile();
    
    /**
     * @brief Create AVnu Automotive Profile configuration
     */
    gPTPProfile createAutomotiveProfile();
    
    /**
     * @brief Create Standard IEEE 802.1AS Profile configuration
     */
    gPTPProfile createStandardProfile();
    
    /**
     * @brief Create profile from configuration file
     */
    gPTPProfile createProfileFromConfig(const std::string& config_path);
    
    /**
     * @brief Create profile by name
     */
    gPTPProfile createProfileByName(const std::string& profile_name);
    
    /**
     * @brief Validate profile configuration
     */
    bool validateProfile(const gPTPProfile& profile);
    
    /**
     * @brief Get profile description string
     */
    std::string getProfileDescription(const gPTPProfile& profile);
}

#endif // GPTP_PROFILE_HPP
