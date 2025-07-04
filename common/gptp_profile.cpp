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
    
    // Milan clock quality (corrected per specification)
    profile.clock_class = 248;              // Application clock (not grandmaster=6)
    profile.clock_accuracy = 0x20;          // 32ns accuracy
    profile.offset_scaled_log_variance = 0x4000;
    profile.priority1 = 248;                // Application clock priority
    profile.priority2 = 248;
    
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
    profile.late_response_threshold_ms = 10;  // 10ms threshold
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
    
    // Milan compliance limits
    profile.max_convergence_time_ms = 100;  // 100ms convergence target
    profile.max_sync_jitter_ns = 1000;      // 1000ns max sync jitter
    profile.max_path_delay_variation_ns = 10000; // 10000ns max path delay variation
    
    GPTP_LOG_INFO("*** MILAN PROFILE CREATED: 125ms sync, 100ms convergence target, asCapable starts FALSE, earned via 2-5 PDelay ***");
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
    profile.profile_version = "1.0";
    profile.profile_description = "AVnu Automotive Profile";
    
    // Automotive timing intervals
    profile.sync_interval_log = 0;          // 1 second
    profile.announce_interval_log = 0;      // 1 second
    profile.pdelay_interval_log = 0;        // 1 second
    
    // Automotive timeouts
    profile.sync_receipt_timeout = 3;
    profile.announce_receipt_timeout = 3;
    profile.pdelay_receipt_timeout = 3;
    profile.delay_req_interval_log = 0;     // DelayReq interval (not used in PDelay mode)
    
    // Automotive message rate configuration
    profile.announce_receipt_timeout_multiplier = 3;
    profile.pdelay_receipt_timeout_multiplier = 3;
    
    // Automotive thresholds
    profile.neighbor_prop_delay_thresh = 800000; // 800μs
    profile.sync_receipt_thresh = 8;        // Higher threshold for automotive
    
    // Automotive clock quality
    profile.clock_class = 248;
    profile.clock_accuracy = 0xFE;
    profile.offset_scaled_log_variance = 0x4E5D;
    profile.priority1 = 248;
    profile.priority2 = 248;
    
    // Automotive asCapable behavior
    profile.initial_as_capable = true;     // Start false
    profile.as_capable_on_link_up = true;   // Automotive: asCapable=true immediately on link up
    profile.as_capable_on_link_down = true; // Lose asCapable on link down
    profile.min_pdelay_successes = 0;       // No PDelay requirement
    profile.max_pdelay_successes = 0;       // No upper limit
    profile.maintain_as_capable_on_timeout = true;   // Always maintain
    profile.maintain_as_capable_on_late_response = true; // Always maintain
    
    // Automotive late response handling (lenient)
    profile.late_response_threshold_ms = 50; // More lenient threshold
    profile.consecutive_late_limit = 10;     // Allow more consecutive late responses
    profile.reset_pdelay_count_on_timeout = false; // Don't reset
    
    // Automotive protocol behavior
    profile.send_announce_when_as_capable_only = false; // Always send announce
    profile.process_sync_regardless_as_capable = true;
    profile.start_pdelay_on_link_up = true;
    profile.allows_negative_correction_field = true;
    profile.requires_strict_timeouts = false;
    profile.supports_bmca = false;          // Automotive typically disables BMCA
    
    // Automotive-specific features
    profile.stream_aware_bmca = false;
    profile.redundant_gm_support = false;
    profile.automotive_test_status = true;  // Enable automotive test status messages
    profile.bmca_enabled = false;           // Automotive typically disables BMCA
    profile.follow_up_enabled = true;       // FollowUp enabled
    
    // Automotive test and diagnostic features
    profile.test_status_interval_log = 0;   // 1 second test status messages
    profile.force_slave_mode = true;        // Automotive often forces slave mode
    
    // No specific compliance limits
    profile.max_convergence_time_ms = 0;
    profile.max_sync_jitter_ns = 0;
    profile.max_path_delay_variation_ns = 0;
    
    GPTP_LOG_INFO("*** AUTOMOTIVE PROFILE CREATED: Immediate asCapable on link up, test status enabled ***");
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
    
    // Validate timing intervals
    if (profile.sync_interval_log < -8 || profile.sync_interval_log > 8) {
        GPTP_LOG_ERROR("*** PROFILE VALIDATION ERROR: Invalid sync_interval_log %d ***", profile.sync_interval_log);
        valid = false;
    }
    
    // Validate asCapable parameters
    if (profile.min_pdelay_successes > profile.max_pdelay_successes && profile.max_pdelay_successes > 0) {
        GPTP_LOG_ERROR("*** PROFILE VALIDATION ERROR: min_pdelay_successes > max_pdelay_successes ***");
        valid = false;
    }
    
    // Validate Milan-specific limits
    if (profile.profile_name == "milan") {
        if (profile.max_convergence_time_ms == 0 || profile.max_convergence_time_ms > 1000) {
            GPTP_LOG_ERROR("*** MILAN VALIDATION ERROR: Invalid convergence time %dms ***", profile.max_convergence_time_ms);
            valid = false;
        }
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
