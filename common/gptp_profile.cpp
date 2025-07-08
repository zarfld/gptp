/**
 * @file gptp_profile.cpp
 * @brief Unified gPTP Profile Configuration Implementation
 */

#include "gptp_profile.hpp"
#include "gptp_log.hpp"
#include <cstring>

namespace gPTPProfileFactory {

gPTPProfile createMilanProfile() {
    gPTPProfile profile;
    
    // Milan profile identification
    profile.profile_name = "milan";
    profile.profile_version = "2.0a";
    profile.profile_description = "Milan Baseline Interoperability Profile";
    
    // Milan timing intervals
    profile.sync_interval_log = -3;         // 125ms (2^-3)
    profile.announce_interval_log = 0;      // 1 second
    profile.pdelay_interval_log = 0;        // 1 second
    
    // Milan timeouts
    profile.sync_receipt_timeout = 3;
    profile.announce_receipt_timeout = 3;
    profile.pdelay_receipt_timeout = 3;
    profile.delay_req_interval_log = 0;     // DelayReq interval (not used in PDelay mode)
    
    // Milan message rate configuration
    profile.announce_receipt_timeout_multiplier = 3;
    profile.pdelay_receipt_timeout_multiplier = 3;
    
    // Milan thresholds
    profile.neighbor_prop_delay_thresh = 800000; // 800μs
    profile.sync_receipt_thresh = 3;
    
    // Milan clock quality (corrected per Milan Baseline Interoperability Specification 2.0a)
    profile.clock_class = 248;              // Application specific time (Class 248) - MILAN REQUIRED
    profile.clock_accuracy = 0xFE;          // Time accurate to within unspecified bounds - MILAN REQUIRED
    profile.offset_scaled_log_variance = 0x4E5D; // Standard variance - MILAN REQUIRED
    profile.priority1 = 248;                // Application clock priority - MILAN REQUIRED
    profile.priority2 = 248;                // Application clock priority - MILAN REQUIRED
    
    /* MILAN Baseline Interoperability Profile Clock Quality January30,2019:
        5.6.2.4. asCapable [gPTP, Clause 10.2.4.1]
            A PAAD shall report asCapable as TRUE after no less than 2 and no more than 5 successfully received Pdelay Responses and Pdelay Response Follow Ups to the Pdelay Request messages sent by the device.
            Note: This requirement ensures that all certified PAADs become asCapable within a bounded time.
    
        5.6.2.6. Pdelay Turnaround Time
        Per [gPTP, Annex B.2.3], Pdelay turnaround time must be less than or equal to 10ms. This requirement 
        shall be relaxed by 50% for Avnu purposes resulting in a maximum Pdelay turnaround time of 15ms. 
        Responses later than this time may or may not be processed by a gPTP device, but should not result in 
        asCapable being set to FALSE if 3 or more consecutive responses are received later than 10ms but before 
        pdelayReqInterval (typically 1 second).
    
    */
    // Milan asCapable behavior (corrected per specification)
    profile.initial_as_capable = false;     // Milan starts asCapable=false, must earn it
    profile.as_capable_on_link_up = false;  // Must earn asCapable via PDelay (not immediate)
    profile.as_capable_on_link_down = true; // Lose asCapable on link down
    profile.min_pdelay_successes = 2;       // Milan: 2-5 successful PDelay exchanges
    profile.max_pdelay_successes = 5;       // Milan: maximum 5 threshold
    profile.maintain_as_capable_on_timeout = true;   // Maintain on timeout after establishment
    profile.maintain_as_capable_on_late_response = true; // Milan Annex B.2.3
    
    // Milan late response handling (Annex B.2.3)
    // SPEC: Pdelay turnaround time relaxed by 50% = 15ms maximum
    // Responses later than 10ms but before pdelayReqInterval should not result in asCapable=FALSE
    profile.late_response_threshold_ms = 15;  // 15ms threshold per Milan Annex B.2.3
    profile.consecutive_late_limit = 3;       // Allow 3+ consecutive late responses
    profile.reset_pdelay_count_on_timeout = false; // Don't reset after establishment
    
    // Milan protocol behavior
    profile.send_announce_when_as_capable_only = true;
    profile.process_sync_regardless_as_capable = true;
    profile.start_pdelay_on_link_up = true;
    profile.allows_negative_correction_field = false;
    profile.requires_strict_timeouts = true;
    profile.supports_bmca = true;
    
    // Milan-specific features
    profile.stream_aware_bmca = false;      // Optional feature (can be enabled)
    profile.redundant_gm_support = false;   // Optional feature (can be enabled)
    profile.automotive_test_status = false;
    profile.bmca_enabled = true;            // BMCA enabled for Milan
    profile.follow_up_enabled = true;       // FollowUp enabled
    
    // Milan test and diagnostic features
    profile.test_status_interval_log = 0;   // Not used in Milan
    profile.force_slave_mode = false;       // Milan allows grandmaster mode
    
    // Milan compliance limits (enhanced for better compliance)
    profile.max_convergence_time_ms = 100;  // 100ms convergence target
    profile.max_sync_jitter_ns = 1000;      // 1000ns max sync jitter
    profile.max_path_delay_variation_ns = 10000; // 10000ns max path delay variation
    
    GPTP_LOG_INFO("*** MILAN PROFILE CREATED: 125ms sync, 15ms late response threshold, asCapable earned via 2-5 PDelay ***");
    return profile;
}

gPTPProfile createAvnuBaseProfile() {
    gPTPProfile profile;
    
    // AVnu Base profile identification
    profile.profile_name = "avnu_base";
    profile.profile_version = "1.1";
    profile.profile_description = "AVnu Base/ProAV Functional Interoperability Profile";
    
    // AVnu Base timing intervals
    profile.sync_interval_log = 0;          // 1 second
    profile.announce_interval_log = 0;      // 1 second
    profile.pdelay_interval_log = 0;        // 1 second
    
    // AVnu Base timeouts
    profile.sync_receipt_timeout = 3;
    profile.announce_receipt_timeout = 3;
    profile.pdelay_receipt_timeout = 3;
    profile.delay_req_interval_log = 0;     // DelayReq interval (not used in PDelay mode)
    
    // AVnu Base message rate configuration
    profile.announce_receipt_timeout_multiplier = 3;
    profile.pdelay_receipt_timeout_multiplier = 3;
    
    // AVnu Base thresholds
    profile.neighbor_prop_delay_thresh = 800000; // 800μs
    profile.sync_receipt_thresh = 3;
    
    // AVnu Base clock quality
    profile.clock_class = 248;              // Default application time
    profile.clock_accuracy = 0xFE;          // Time accurate to within unspecified bounds
    profile.offset_scaled_log_variance = 0x4E5D;
    profile.priority1 = 248;
    profile.priority2 = 248;
    
    // AVnu Base asCapable behavior
    profile.initial_as_capable = false;     // AVnu Base starts asCapable=false
    profile.as_capable_on_link_up = false;  // Must earn asCapable via PDelay
    profile.as_capable_on_link_down = true; // Lose asCapable on link down
    profile.min_pdelay_successes = 2;       // AVnu Base: 2-10 successful PDelay exchanges
    profile.max_pdelay_successes = 10;      // AVnu Base: maximum 10 threshold
    profile.maintain_as_capable_on_timeout = true;   // Maintain after establishment
    profile.maintain_as_capable_on_late_response = false; // Standard late response handling
    
    // AVnu Base late response handling
    profile.late_response_threshold_ms = 10;
    profile.consecutive_late_limit = 3;
    profile.reset_pdelay_count_on_timeout = true;
    
    // AVnu Base protocol behavior
    profile.send_announce_when_as_capable_only = true;
    profile.process_sync_regardless_as_capable = true;
    profile.start_pdelay_on_link_up = true;
    profile.allows_negative_correction_field = false;
    profile.requires_strict_timeouts = false;
    profile.supports_bmca = true;
    
    // AVnu Base features
    profile.stream_aware_bmca = false;
    profile.redundant_gm_support = false;
    profile.automotive_test_status = false;
    profile.bmca_enabled = true;            // BMCA enabled for AVnu Base
    profile.follow_up_enabled = true;       // FollowUp enabled
    
    // AVnu Base test and diagnostic features
    profile.test_status_interval_log = 0;   // Not used in AVnu Base
    profile.force_slave_mode = false;       // Allow grandmaster mode
    
    // No specific compliance limits
    profile.max_convergence_time_ms = 0;
    profile.max_sync_jitter_ns = 0;
    profile.max_path_delay_variation_ns = 0;
    
    GPTP_LOG_INFO("*** AVNU BASE PROFILE CREATED: 1s intervals, asCapable requires 2-10 PDelay ***");
    return profile;
}

gPTPProfile createAutomotiveProfile() {
    gPTPProfile profile;
    
    // Automotive profile identification
    profile.profile_name = "automotive";
    profile.profile_version = "1.6";
    profile.profile_description = "AVnu Automotive Profile (AVB Spec 1.6 Compliant)";
    
    // Automotive timing intervals per Table 12 (Time Critical Ports by default)
    // Section 6.2.6: Time Critical Port ranges
    profile.initial_sync_interval_log = -3;    // 125ms (31.25ms-125ms range, using 125ms)
    profile.initial_pdelay_interval_log = 0;   // 1s (fixed at 1s per spec)
    
    // Operational intervals for reduced overhead  
    profile.operational_sync_interval_log = 0; // 1s (125ms-1s range, using 1s for reduced overhead)
    profile.operational_pdelay_interval_log = 3; // 8s (1s-8s range, using 8s for minimal overhead)
    
    // Use initial intervals as current intervals (will switch after 60s)
    profile.sync_interval_log = profile.initial_sync_interval_log;
    profile.pdelay_interval_log = profile.initial_pdelay_interval_log;
    profile.announce_interval_log = 127;       // SPEC: No announce messages (disabled)
    
    // Automotive interval management (Section 6.2.3)
    profile.interval_transition_timeout_s = 60;    // 60 seconds per spec
    profile.signaling_enabled = true;              // Support gPTP signaling
    profile.signaling_response_timeout_ms = 250;   // 250ms or 2*interval per spec (minimum 250ms)
    
    // Automotive timeouts
    profile.sync_receipt_timeout = 3;
    profile.announce_receipt_timeout = 3;      // Not used (no announces)
    profile.pdelay_receipt_timeout = 3;
    profile.delay_req_interval_log = 0;        // DelayReq interval (not used in PDelay mode)
    
    // Automotive message rate configuration
    profile.announce_receipt_timeout_multiplier = 3;
    profile.pdelay_receipt_timeout_multiplier = 3;
    
    // Automotive thresholds (Section 6.2.2.1)
    profile.neighbor_prop_delay_thresh = 800000; // 800μs (still used for monitoring, not asCapable)
    profile.sync_receipt_thresh = 8;             // Higher threshold for automotive
    profile.neighbor_delay_update_threshold_ns = 100; // Update stored value if differs by >100ns
    
    // Automotive clock quality
    profile.clock_class = 248;
    profile.clock_accuracy = 0xFE;
    profile.offset_scaled_log_variance = 0x4E5D;
    profile.priority1 = 248;
    profile.priority2 = 248;
    
    // Automotive asCapable behavior (Section 6.2.1.2)
    profile.initial_as_capable = false;        // Start false, set true on link up
    profile.as_capable_on_link_up = true;      // SPEC REQUIRED: asCapable=true on link up
    profile.as_capable_on_link_down = true;    // Lose asCapable on link down
    profile.min_pdelay_successes = 0;          // No PDelay requirement for asCapable
    profile.max_pdelay_successes = 0;          // No upper limit
    profile.maintain_as_capable_on_timeout = true;    // Don't lose asCapable on timeout
    profile.maintain_as_capable_on_late_response = true; // Don't lose asCapable on late response
    
    // Automotive late response handling (lenient for fixed topology)
    profile.late_response_threshold_ms = 50;   // More lenient threshold
    profile.consecutive_late_limit = 10;       // Allow more consecutive late responses
    profile.reset_pdelay_count_on_timeout = false; // Don't reset count
    
    // Automotive protocol behavior (Section 6.3)
    profile.send_announce_when_as_capable_only = false; // SPEC: Never send announces
    profile.disable_announce_transmission = true;      // SPEC REQUIRED: No announce messages  
    profile.process_sync_regardless_as_capable = true; // Always process sync
    profile.start_pdelay_on_link_up = true;
    profile.allows_negative_correction_field = true;
    profile.requires_strict_timeouts = false;
    profile.supports_bmca = false;             // SPEC REQUIRED: BMCA shall not execute
    profile.disable_source_port_identity_check = true; // SPEC REQUIRED: No sourcePortIdentity verification
    
    // Automotive-specific features (Section 6.2.2, 6.3)
    profile.stream_aware_bmca = false;
    profile.redundant_gm_support = false;
    profile.automotive_test_status = true;     // Enable automotive test status messages
    profile.bmca_enabled = false;              // SPEC REQUIRED: BMCA disabled
    profile.follow_up_enabled = true;          // SPEC REQUIRED: FollowUp with TLVs
    profile.automotive_holdover_enabled = true; // Enable automotive holdover (AED-E)
    profile.automotive_bridge_behavior = true;  // Enable automotive bridge behavior (AED-B)
    
    // Automotive persistent storage (Section 6.2.2)
    profile.persistent_neighbor_delay = true;     // Store neighborPropDelay
    profile.persistent_rate_ratio = true;         // Store rateRatio
    profile.persistent_neighbor_rate_ratio = true; // Store neighborRateRatio
    
    // Automotive port type and behavior (Section 6.2.1.1, 6.2.2.1)
    profile.is_time_critical_port = true;         // Time critical port by default (can be configured)
    profile.is_grandmaster_device = false;        // Not GM by default (configurable per device)
    profile.disable_neighbor_delay_threshold = true; // SPEC REQUIRED: Don't set asCapable=false on threshold
    profile.max_startup_sync_wait_s = 20;         // 20 seconds max wait for initial sync
    
    // Automotive signaling and interval management (Section 6.2.3)
    profile.send_signaling_on_sync_achieved = true; // Send signaling within 60s of sync
    profile.signaling_send_timeout_s = 60;        // 60 seconds per spec
    profile.revert_to_initial_on_link_event = true; // Revert to initial intervals on link events
    
    // Automotive test and diagnostic features
    profile.test_status_interval_log = 0;      // 1 second test status messages
    profile.force_slave_mode = false;          // Can be GM or slave (configurable)
    
    // Automotive compliance - no specific limits but enable monitoring
    profile.max_convergence_time_ms = 0;       // No hard limit
    profile.max_sync_jitter_ns = 0;            // No hard limit  
    profile.max_path_delay_variation_ns = 0;   // No hard limit
    
    GPTP_LOG_INFO("*** AUTOMOTIVE PROFILE CREATED (AVB Spec 1.6): No BMCA, no announces, signaling enabled, asCapable on link up, interval management after 60s ***");
    return profile;
}

gPTPProfile createStandardProfile() {
    // Standard profile is the default constructor
    gPTPProfile profile; // Uses default constructor values
    
    GPTP_LOG_INFO("*** STANDARD PROFILE CREATED: IEEE 802.1AS default behavior ***");
    return profile;
}

gPTPProfile createProfileFromConfig(const std::string& config_path) {
    // TODO: Implement config file parsing
    // For now, return standard profile
    GPTP_LOG_INFO("*** LOADING PROFILE FROM CONFIG: %s ***", config_path.c_str());
    return createStandardProfile();
}

gPTPProfile createProfileByName(const std::string& profile_name) {
    if (profile_name == "milan") {
        return createMilanProfile();
    } else if (profile_name == "avnu_base") {
        return createAvnuBaseProfile();
    } else if (profile_name == "automotive") {
        return createAutomotiveProfile();
    } else if (profile_name == "standard") {
        return createStandardProfile();
    } else {
        GPTP_LOG_WARNING("*** Unknown profile name '%s', using standard profile ***", profile_name.c_str());
        return createStandardProfile();
    }
}

bool validateProfile(const gPTPProfile& profile) {
    bool valid = true;
    
    GPTP_LOG_INFO("*** VALIDATING PROFILE: %s v%s ***", 
                  profile.profile_name.c_str(), profile.profile_version.c_str());
    
    // Validate timing intervals
    if (profile.sync_interval_log < -8 || profile.sync_interval_log > 8) {
        GPTP_LOG_ERROR("*** PROFILE VALIDATION ERROR: Invalid sync_interval_log %d (valid range: -8 to 8) ***", 
                       profile.sync_interval_log);
        valid = false;
    }
    
    if (profile.announce_interval_log < -8 || profile.announce_interval_log > 8) {
        GPTP_LOG_ERROR("*** PROFILE VALIDATION ERROR: Invalid announce_interval_log %d (valid range: -8 to 8) ***", 
                       profile.announce_interval_log);
        valid = false;
    }
    
    if (profile.pdelay_interval_log < -8 || profile.pdelay_interval_log > 8) {
        GPTP_LOG_ERROR("*** PROFILE VALIDATION ERROR: Invalid pdelay_interval_log %d (valid range: -8 to 8) ***", 
                       profile.pdelay_interval_log);
        valid = false;
    }
    
    // Validate asCapable parameters
    if (profile.min_pdelay_successes > profile.max_pdelay_successes && profile.max_pdelay_successes > 0) {
        GPTP_LOG_ERROR("*** PROFILE VALIDATION ERROR: min_pdelay_successes (%d) > max_pdelay_successes (%d) ***",
                       profile.min_pdelay_successes, profile.max_pdelay_successes);
        valid = false;
    }
    
    // Profile-specific validation
    if (profile.profile_name == "milan") {
        // Milan Baseline Interoperability Specification validation
        if (profile.sync_interval_log != -3) {
            GPTP_LOG_ERROR("*** MILAN VALIDATION ERROR: sync_interval_log must be -3 (125ms), got %d ***", 
                           profile.sync_interval_log);
            valid = false;
        }
        
        if (profile.announce_interval_log != 0) {
            GPTP_LOG_ERROR("*** MILAN VALIDATION ERROR: announce_interval_log must be 0 (1s), got %d ***", 
                           profile.announce_interval_log);
            valid = false;
        }
        
        if (profile.pdelay_interval_log != 0) {
            GPTP_LOG_ERROR("*** MILAN VALIDATION ERROR: pdelay_interval_log must be 0 (1s), got %d ***", 
                           profile.pdelay_interval_log);
            valid = false;
        }
        
        if (profile.min_pdelay_successes < 2 || profile.max_pdelay_successes > 5) {
            GPTP_LOG_ERROR("*** MILAN VALIDATION ERROR: PDelay successes must be 2-5, got %d-%d ***",
                           profile.min_pdelay_successes, profile.max_pdelay_successes);
            valid = false;
        }
        
        if (profile.neighbor_prop_delay_thresh != 800000) {
            GPTP_LOG_ERROR("*** MILAN VALIDATION ERROR: neighborPropDelayThresh must be 800000ns, got %ld ***",
                           profile.neighbor_prop_delay_thresh);
            valid = false;
        }
        
        if (profile.clock_class != 248 || profile.clock_accuracy != 0xFE || 
            profile.offset_scaled_log_variance != 0x4E5D || profile.priority1 != 248) {
            GPTP_LOG_ERROR("*** MILAN VALIDATION ERROR: Invalid clock quality parameters ***");
            valid = false;
        }
        
    } else if (profile.profile_name == "avnu_base") {
        // AVnu Base/ProAV Functional Interoperability Specification validation
        if (profile.min_pdelay_successes < 2 || profile.max_pdelay_successes > 10) {
            GPTP_LOG_ERROR("*** AVNU BASE VALIDATION ERROR: PDelay successes must be 2-10, got %d-%d ***",
                           profile.min_pdelay_successes, profile.max_pdelay_successes);
            valid = false;
        }
        
        if (profile.neighbor_prop_delay_thresh != 800000) {
            GPTP_LOG_ERROR("*** AVNU BASE VALIDATION ERROR: neighborPropDelayThresh must be 800000ns, got %ld ***",
                           profile.neighbor_prop_delay_thresh);
            valid = false;
        }
        
    } else if (profile.profile_name == "automotive") {
        // Automotive Ethernet AVB Functional Interoperability Specification validation
        if (!profile.as_capable_on_link_up) {
            GPTP_LOG_ERROR("*** AUTOMOTIVE VALIDATION ERROR: asCapable must be TRUE on link up ***");
            valid = false;
        }
        
        if (profile.min_pdelay_successes != 0) {
            GPTP_LOG_ERROR("*** AUTOMOTIVE VALIDATION ERROR: No PDelay requirement for asCapable, got %d ***",
                           profile.min_pdelay_successes);
            valid = false;
        }
        
        if (profile.bmca_enabled) {
            GPTP_LOG_ERROR("*** AUTOMOTIVE VALIDATION ERROR: BMCA must be disabled ***");
            valid = false;
        }
        
        if (!profile.disable_announce_transmission) {
            GPTP_LOG_ERROR("*** AUTOMOTIVE VALIDATION ERROR: Announce transmission must be disabled ***");
            valid = false;
        }
        
        if (!profile.disable_source_port_identity_check) {
            GPTP_LOG_ERROR("*** AUTOMOTIVE VALIDATION ERROR: sourcePortIdentity check must be disabled ***");
            valid = false;
        }
        
        if (!profile.signaling_enabled) {
            GPTP_LOG_ERROR("*** AUTOMOTIVE VALIDATION ERROR: gPTP signaling must be enabled ***");
            valid = false;
        }
        
        if (profile.interval_transition_timeout_s != 60) {
            GPTP_LOG_ERROR("*** AUTOMOTIVE VALIDATION ERROR: Interval transition timeout must be 60s, got %ds ***",
                           profile.interval_transition_timeout_s);
            valid = false;
        }
    }
    
    if (valid) {
        GPTP_LOG_INFO("*** PROFILE VALIDATION PASSED: %s ***", profile.profile_name.c_str());
    } else {
        GPTP_LOG_ERROR("*** PROFILE VALIDATION FAILED: %s ***", profile.profile_name.c_str());
    }
    
    return valid;
}

std::string getProfileDescription(const gPTPProfile& profile) {
    std::string desc = profile.profile_description;
    desc += " (v" + profile.profile_version + ")";
    
    // Add key characteristics
    if (profile.profile_name == "milan") {
        desc += " - 125ms sync, 100ms convergence, enhanced asCapable";
    } else if (profile.profile_name == "avnu_base") {
        desc += " - 1s intervals, 2-10 PDelay requirement";
    } else if (profile.profile_name == "automotive") {
        desc += " - Immediate asCapable, test status messages";
    }
    
    return desc;
}

} // namespace gPTPProfileFactory
