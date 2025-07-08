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
#include <memory>
#include <vector>

// Include full definitions for clock quality monitoring
#include "gptp_clock_quality.hpp"

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
    
    // Automotive Profile: Initial vs Operational intervals (Section 6.2.1.3-6.2.1.6)
    int8_t initial_sync_interval_log;      // Initial sync interval (automotive)
    int8_t operational_sync_interval_log;  // Operational sync interval (automotive)
    int8_t initial_pdelay_interval_log;    // Initial PDelay interval (automotive)
    int8_t operational_pdelay_interval_log; // Operational PDelay interval (automotive)
    
    // Automotive Profile: Interval management timeouts
    uint32_t interval_transition_timeout_s; // Time to wait before switching to operational intervals (60s default)
    bool signaling_enabled;                // Support for gPTP signaling messages
    uint32_t signaling_response_timeout_ms; // Response timeout for signaling messages (250ms or 2*interval)
    
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
    
    // Automotive Profile: Persistent storage configuration
    bool persistent_neighbor_delay;        // Store neighborPropDelay in non-volatile memory
    bool persistent_rate_ratio;            // Store rateRatio in non-volatile memory
    bool persistent_neighbor_rate_ratio;   // Store neighborRateRatio in non-volatile memory
    uint32_t neighbor_delay_update_threshold_ns; // Update stored value if differs by >100ns
    
    // Automotive Profile: Special behaviors
    bool disable_source_port_identity_check; // Disable sourcePortIdentity verification (no announces)
    bool disable_announce_transmission;     // Never send announce messages in automotive mode
    bool automotive_holdover_enabled;      // Enable automotive holdover behavior for AED-E
    bool automotive_bridge_behavior;       // Enable automotive AED-B sync timeout behavior
    
    // Automotive Profile: Port type classification and behavior
    bool is_time_critical_port;             // Time critical vs non-time critical port (Table 12)
    bool is_grandmaster_device;             // isGM variable (Section 6.2.1.1)
    bool disable_neighbor_delay_threshold;  // Don't set asCapable=false on threshold exceeded
    uint32_t max_startup_sync_wait_s;       // Max time to wait for initial sync (20s default)
    
    // Automotive Profile: Advanced signaling and interval management
    bool send_signaling_on_sync_achieved;   // Send signaling within 60s of sync achievement
    uint32_t signaling_send_timeout_s;      // Timeout for sending signaling after sync (60s)
    bool revert_to_initial_on_link_event;   // Revert to initial intervals on link down/up
    
    // Performance and compliance limits
    uint32_t max_convergence_time_ms;      // Maximum convergence time (0=no limit)
    uint32_t max_sync_jitter_ns;           // Maximum sync jitter (0=no limit)
    uint32_t max_path_delay_variation_ns;  // Maximum path delay variation (0=no limit)
    
    // Runtime statistics (mutable, updated during operation)
    mutable struct {
        uint64_t convergence_start_time;    // When convergence started (ns)
        uint64_t last_convergence_time;     // Last measured convergence time (ns)
        uint64_t last_sync_time;            // Last sync message timestamp (ns)
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
    gPTPProfile();
    
    // Custom destructor needed for unique_ptr with forward declarations
    ~gPTPProfile();
    
    // Clock Quality Monitoring Configuration
    // Based on Avnu Alliance "802.1AS Recovered Clock Quality Testing v1.0"
    bool clock_quality_monitoring_enabled;          ///< Enable clock quality monitoring
    uint32_t clock_quality_measurement_interval_ms; ///< Measurement interval (default: 125ms)
    uint32_t clock_quality_analysis_window_s;       ///< Analysis window (default: 300s)
    int64_t clock_quality_target_accuracy_ns;       ///< Target accuracy (default: ±80ns)
    uint32_t clock_quality_max_lock_time_s;         ///< Maximum lock time (default: 6s)
    uint32_t clock_quality_max_history;             ///< Maximum stored measurements (default: 10000)
    
    // Clock Quality Monitoring Objects (created on-demand)
    mutable std::unique_ptr<OpenAvnu::gPTP::IngressEventMonitor> clock_monitor;
    mutable std::unique_ptr<OpenAvnu::gPTP::ClockQualityAnalyzer> quality_analyzer;
    
    /**
     * @brief Enable clock quality monitoring
     */
    void enable_clock_quality_monitoring() const;
    
    /**
     * @brief Disable clock quality monitoring
     */
    void disable_clock_quality_monitoring() const;
    
    /**
     * @brief Check if clock quality monitoring is enabled and active
     */
    bool is_clock_quality_monitoring_active() const;
    
    /**
     * @brief Record Sync ingress event for clock quality analysis
     * @param t1_master_tx Master transmit time from Follow_Up originTimestamp
     * @param t2_slave_rx Slave receive timestamp from hardware
     * @param path_delay Measured path delay from PDelay mechanism
     * @param correction_field Correction field from Follow_Up message
     * @param sequence_id Sequence ID for duplicate detection
     */
    void record_sync_ingress_event(uint64_t t1_master_tx, uint64_t t2_slave_rx,
                                 uint64_t path_delay, uint64_t correction_field,
                                 uint16_t sequence_id) const;
    
    /**
     * @brief Get current clock quality metrics
     * @param window_seconds Analysis window in seconds (0 = all measurements)
     * @return Current clock quality metrics
     */
    OpenAvnu::gPTP::ClockQualityMetrics get_clock_quality_metrics(uint32_t window_seconds = 0) const;
    
    /**
     * @brief Validate certification compliance for this profile
     * @return true if all certification requirements are met
     */
    bool validate_clock_quality_certification() const;
    
    /**
     * @brief Generate clock quality compliance report
     * @return Human-readable compliance report
     */
    std::string generate_clock_quality_report() const;
    
    /**
     * @brief Export clock quality data as TLV for remote monitoring
     * @return TLV-encoded clock quality data
     */
    std::vector<uint8_t> export_clock_quality_tlv() const;
    
    /**
     * @brief Get profile type for clock quality validation
     */
    OpenAvnu::gPTP::ProfileType get_clock_quality_profile_type() const;

    // Copy control - needed because of unique_ptr members
    // Make the class non-copyable to avoid issues with unique_ptr
    gPTPProfile(const gPTPProfile&) = delete;
    gPTPProfile& operator=(const gPTPProfile&) = delete;
    
    // Allow move operations
    gPTPProfile(gPTPProfile&&) = default;
    gPTPProfile& operator=(gPTPProfile&&) = default;
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
