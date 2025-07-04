/**
 * @file milan_profile.cpp
 * @brief Milan Baseline Interoperability Profile Implementation
 */

#include "milan_profile.hpp"
#include "gptp_log.hpp"
#include <cmath>

MilanProfile::MilanProfile() {
    // Initialize Milan configuration with defaults from Milan Baseline Interoperability Specification 2.0a
    m_config.max_convergence_time_ms = 100;            // 100ms convergence target
    m_config.max_sync_jitter_ns = 1000;                // 1000ns max sync jitter
    m_config.max_path_delay_variation_ns = 10000;      // 10000ns max path delay variation
    m_config.stream_aware_bmca = false;                // Optional feature
    m_config.redundant_gm_support = false;             // Optional feature
    
    // Milan Baseline Interoperability Specification 2.0a Section 5.6.2.2:
    // logSyncInterval shall be set to -3 (125ms interval)
    m_config.milan_sync_interval_log = -3;             // 125ms (2^-3) - MILAN REQUIRED
    
    // Milan Baseline Interoperability Specification 2.0a Section 5.6.2.3:
    // logAnnounceInterval shall be set to 0 (1s interval)
    m_config.milan_announce_interval_log = 0;          // 1s (2^0) - MILAN REQUIRED
    
    // Milan Baseline Interoperability Specification 2.0a Section 5.6.2.5:
    // logPdelayReqInterval shall be set to 0 (1s interval)
    m_config.milan_pdelay_interval_log = 0;            // 1s (2^0) - MILAN REQUIRED

    // Milan B.1 Media Clock Holdover Configuration
    m_config.media_clock_holdover_time_ms = 5000;      // 5 seconds minimum - MILAN B.1 REQUIRED
    m_config.tu_bit_duration_ms = 250;                 // 0.25 seconds (250ms) - MILAN B.1.1 REQUIRED
    m_config.gm_change_convergence_time_ms = 5000;     // 5 seconds max - MILAN B.1.2 REQUIRED
    m_config.stream_stability_time_ms = 60000;         // 60 seconds min - MILAN B.1.2 REQUIRED
    m_config.bridge_propagation_time_ms = 50;          // <50ms bridge propagation - MILAN B.1 NOTE

    // Initialize statistics
    memset(&m_stats, 0, sizeof(m_stats));
    
    // Initialize Milan B.1 specific statistics
    m_stats.media_clock_holdover_active = false;
    m_stats.tu_bit_active = false;
    m_stats.current_stream_stability_time = 0;
    // Initialize grandmaster identities to zero
    m_stats.current_grandmaster.set((uint8_t*)"\x00\x00\x00\x00\x00\x00\x00\x00");
    m_stats.previous_grandmaster.set((uint8_t*)"\x00\x00\x00\x00\x00\x00\x00\x00");
}

ProfileTimingConfig MilanProfile::getTimingConfig() const {
    ProfileTimingConfig config;
    
    // Milan Baseline Interoperability Specification 2.0a Section 5.6.2.2:
    // logSyncInterval shall be set to -3 (125ms interval)
    config.sync_interval_log = m_config.milan_sync_interval_log;        // -3 (125ms) - MILAN REQUIRED
    
    // Milan Baseline Interoperability Specification 2.0a Section 5.6.2.3:
    // logAnnounceInterval shall be set to 0 (1s interval)
    config.announce_interval_log = m_config.milan_announce_interval_log; // 0 (1s) - MILAN REQUIRED
    
    // Milan Baseline Interoperability Specification 2.0a Section 5.6.2.5:
    // logPdelayReqInterval shall be set to 0 (1s interval)
    config.pdelay_interval_log = m_config.milan_pdelay_interval_log;     // 0 (1s) - MILAN REQUIRED
    
    // Standard timeout values
    config.sync_receipt_timeout = 3;                                     // Standard timeout
    config.announce_receipt_timeout = 3;                                 // Standard timeout
    
    // Milan Baseline Interoperability Specification 2.0a Section 5.6.2.7:
    // neighborPropDelayThresh shall be set to 800µs (800,000ns)
    config.neighbor_prop_delay_thresh = 800000;                         // 800μs threshold - MILAN REQUIRED
    
    return config;
}

ProfileClockQuality MilanProfile::getClockQuality() const {
    ProfileClockQuality quality;
    
    // Milan Baseline Interoperability Specification 2.0a Section 5.6.2.1:
    // If a PAAD is Grandmaster Capable, the default priority1 value shall be 248 (Other Time-Aware System)
    // Note: Section 6.1.1 defines that all Talker PAADs have to be Grandmaster Capable
    quality.priority1 = 248;                // Default application priority - MILAN REQUIRED
    quality.priority2 = 248;                // Default priority2 - MILAN REQUIRED
    
    // Milan Baseline Interoperability Specification 2.0a Section 5.6.2.1:
    // Clock quality parameters for Milan profile
    quality.clock_class = 248;              // Application specific time (Class 248) - MILAN REQUIRED
    quality.clock_accuracy = 0xFE;          // Time accurate to within unspecified bounds - MILAN REQUIRED
    quality.offset_scaled_log_variance = 0x4E5D; // Standard variance - MILAN REQUIRED
    
    return quality;
}

ProfileAsCapableBehavior MilanProfile::getAsCapableBehavior() const {
    /* Milan Baseline Interoperability Specification 2.0a Section 5.6.2.4:
       asCapable [gPTP, Clause 10.2.4.1]
       A PAAD shall report asCapable as TRUE after no less than 2 and no more than 5 successfully 
       received Pdelay Responses and Pdelay Response Follow Ups to the Pdelay Request messages 
       sent by the device.
       Note: This requirement ensures that all certified PAADs become asCapable within a bounded time.
    
       Section 5.6.2.6: Pdelay Turnaround Time
       Per [gPTP, Annex B.2.3], Pdelay turnaround time must be less than or equal to 10ms. 
       This requirement shall be relaxed by 50% for Avnu purposes resulting in a maximum Pdelay 
       turnaround time of 15ms. Responses later than this time may or may not be processed by a 
       gPTP device, but should not result in asCapable being set to FALSE if 3 or more consecutive 
       responses are received later than 10ms but before pdelayReqInterval (typically 1 second).
    */
    ProfileAsCapableBehavior behavior;
    
    // Milan starts with asCapable=false; it must be earned through successful PDelay exchanges
    behavior.initial_as_capable = false;                // MILAN REQUIRED: Start with asCapable=false
    
    // Milan requires 2-5 successful PDelay exchanges before setting asCapable=true
    behavior.min_pdelay_successes = 2;                  // MILAN REQUIRED: Minimum 2 successful exchanges
    behavior.max_pdelay_successes = 5;                  // MILAN REQUIRED: Maximum 5 successful exchanges
    
    // Milan allows maintaining asCapable even with late responses (10-15ms tolerance)
    behavior.maintain_on_late_response = true;          // MILAN REQUIRED: Maintain asCapable on late responses
    behavior.late_response_threshold_ms = 10;           // MILAN REQUIRED: 10ms threshold for "late"
    behavior.consecutive_late_limit = 3;                // MILAN REQUIRED: Allow 3+ consecutive late responses
    
    return behavior;
}

bool MilanProfile::shouldSendAnnounce(bool as_capable, bool is_grandmaster) const {
    // Milan Baseline Interoperability Specification:
    // Only send announce when asCapable=true and functioning as grandmaster
    // This ensures network stability and Milan compliance
    bool should_send = as_capable && is_grandmaster;
    
    if (!as_capable && is_grandmaster) {
        GPTP_LOG_DEBUG("*** MILAN: Suppressing announce - asCapable=false (Milan compliance) ***");
    }
    
    return should_send;
}

bool MilanProfile::shouldProcessSync(bool as_capable) const {
    // Milan Baseline Interoperability Specification:
    // Process sync messages for monitoring and faster convergence even when not asCapable
    // This helps with network monitoring and diagnostics
    return true;
}

bool MilanProfile::shouldStartPDelay(bool link_up) const {
    // Milan Baseline Interoperability Specification:
    // Start PDelay immediately when link is up to begin asCapable evaluation
    // Required for the 2-5 successful exchange requirement
    return link_up;
}

bool MilanProfile::evaluateAsCapable(
    unsigned int pdelay_count,
    bool current_as_capable,
    bool pdelay_success,
    bool pdelay_timeout,
    bool pdelay_late,
    unsigned int consecutive_late_count
) const {
    // Milan Baseline Interoperability Specification 2.0a Section 5.6.2.4: asCapable logic
    
    if (pdelay_timeout) {
        // Timeout occurred - Milan allows some tolerance
        if (pdelay_count < getAsCapableBehavior().min_pdelay_successes) {
            // Haven't met minimum requirement yet - maintain current state
            GPTP_LOG_STATUS("*** MILAN: PDelay timeout before minimum exchanges (%d/%d) - maintaining current asCapable=%s ***", 
                pdelay_count, getAsCapableBehavior().min_pdelay_successes, current_as_capable ? "true" : "false");
            return current_as_capable;
        } else {
            // Had successful exchanges before - Milan specification allows some timeout tolerance
            // Don't immediately disable asCapable on single timeout
            GPTP_LOG_STATUS("*** MILAN: PDelay timeout after %d successful exchanges - maintaining asCapable=true (Milan tolerance) ***", pdelay_count);
            return true;
        }
    }
    
    if (pdelay_success) {
        // Successful PDelay exchange received
        
        // Check Milan turnaround time tolerance (Section 5.6.2.6)
        if (pdelay_late && consecutive_late_count >= getAsCapableBehavior().consecutive_late_limit) {
            // Multiple consecutive late responses - but Milan allows this
            // "should not result in asCapable being set to FALSE if 3 or more consecutive 
            // responses are received later than 10ms but before pdelayReqInterval"
            GPTP_LOG_STATUS("*** MILAN: %d consecutive late responses (>%dms) but within interval - maintaining asCapable=true (Milan Annex B.2.3) ***", 
                consecutive_late_count, getAsCapableBehavior().late_response_threshold_ms);
            return true;
        }
        
        // Check if we've met the Milan requirement for initial asCapable setting
        if (!current_as_capable && 
            pdelay_count >= getAsCapableBehavior().min_pdelay_successes && 
            pdelay_count <= getAsCapableBehavior().max_pdelay_successes) {
            // Met Milan requirement - set asCapable=true
            // "A PAAD shall report asCapable as TRUE after no less than 2 and no more than 5 
            // successfully received Pdelay Responses"
            GPTP_LOG_STATUS("*** MILAN: Setting asCapable=true after %d successful PDelay exchanges (Milan requirement: %d-%d) ***", 
                pdelay_count, getAsCapableBehavior().min_pdelay_successes, getAsCapableBehavior().max_pdelay_successes);
            return true;
        } else if (current_as_capable) {
            // Already asCapable - maintain state on successful response
            return true;
        } else if (pdelay_count < getAsCapableBehavior().min_pdelay_successes) {
            // Still building up to minimum requirement
            GPTP_LOG_STATUS("*** MILAN: PDelay success %d/%d - need %d more before setting asCapable=true ***", 
                pdelay_count, getAsCapableBehavior().min_pdelay_successes, 
                getAsCapableBehavior().min_pdelay_successes - pdelay_count);
            return current_as_capable;
        }
    }
    
    // Default: maintain current state
    return current_as_capable;
}

bool MilanProfile::evaluateAsCapableOnLinkUp(bool link_up) const {
    // Milan Baseline Interoperability Specification: 
    // asCapable must be earned through PDelay exchanges, not immediately set on link up
    // This differs from some other profiles that might set asCapable=true immediately
    if (link_up) {
        GPTP_LOG_STATUS("*** MILAN: Link up - asCapable remains false until PDelay exchanges complete (Milan requirement) ***");
        return false;  // Milan requires earning asCapable through PDelay success
    }
    return false;
}

bool MilanProfile::evaluateAsCapableOnLinkDown(bool link_down) const {
    // Milan: Lose asCapable when link goes down (standard behavior)
    if (link_down) {
        GPTP_LOG_STATUS("*** MILAN: Setting asCapable=false on link down ***");
        return false;
    }
    return true; // No change if link is not down
}

void MilanProfile::updateTimingStats(uint64_t sync_timestamp, uint64_t arrival_time) {
    m_stats.total_sync_messages++;
    
    // Calculate jitter (simplified)
    static uint64_t last_arrival = 0;
    if (last_arrival != 0) {
        uint64_t interval = arrival_time - last_arrival;
        uint64_t expected_interval = (uint64_t)(pow(2.0, m_config.milan_sync_interval_log) * 1000000000.0);
        
        if (interval > expected_interval) {
            m_stats.sync_jitter_ns = (uint32_t)(interval - expected_interval);
        } else {
            m_stats.sync_jitter_ns = (uint32_t)(expected_interval - interval);
        }
    }
    last_arrival = arrival_time;
}

void MilanProfile::updatePDelayStats(uint64_t path_delay_ns) {
    // Track path delay variation (simplified)
    static uint64_t last_path_delay = 0;
    if (last_path_delay != 0) {
        if (path_delay_ns > last_path_delay) {
            m_stats.path_delay_variation_ns = (uint32_t)(path_delay_ns - last_path_delay);
        } else {
            m_stats.path_delay_variation_ns = (uint32_t)(last_path_delay - path_delay_ns);
        }
    }
    last_path_delay = path_delay_ns;
}

bool MilanProfile::checkComplianceRequirements() const {
    // Check Milan compliance requirements
    bool jitter_ok = m_stats.sync_jitter_ns <= m_config.max_sync_jitter_ns;
    bool path_delay_ok = m_stats.path_delay_variation_ns <= m_config.max_path_delay_variation_ns;
    
    GPTP_LOG_DEBUG("*** MILAN COMPLIANCE CHECK: Jitter=%dns (limit=%dns, %s), PathDelay=%dns (limit=%dns, %s) ***",
        m_stats.sync_jitter_ns, m_config.max_sync_jitter_ns, jitter_ok ? "OK" : "FAIL",
        m_stats.path_delay_variation_ns, m_config.max_path_delay_variation_ns, path_delay_ok ? "OK" : "FAIL");
    
    return jitter_ok && path_delay_ok;
}

bool MilanProfile::loadFromConfigFile(const std::string& config_path) {
    // TODO: Implement Milan-specific config file parsing
    GPTP_LOG_INFO("*** MILAN: Loading configuration from %s ***", config_path.c_str());
    return true;
}

bool MilanProfile::validateConfiguration() const {
    // Validate Milan configuration parameters against specification requirements
    bool valid = true;
    
    // Milan Baseline Interoperability Specification: 100ms max convergence time
    if (m_config.max_convergence_time_ms > 100) {
        GPTP_LOG_ERROR("*** MILAN CONFIG ERROR: Convergence time %dms exceeds Milan limit of 100ms ***", 
            m_config.max_convergence_time_ms);
        valid = false;
    }
    
    // Validate sync jitter limits for Milan compliance
    if (m_config.max_sync_jitter_ns > 10000) {
        GPTP_LOG_ERROR("*** MILAN CONFIG ERROR: Sync jitter %dns exceeds recommended limit of 10000ns ***", 
            m_config.max_sync_jitter_ns);
        valid = false;
    }
    
    // Validate required timing intervals
    if (m_config.milan_sync_interval_log != -3) {
        GPTP_LOG_ERROR("*** MILAN CONFIG ERROR: Sync interval log %d must be -3 (125ms) per Milan specification ***", 
            m_config.milan_sync_interval_log);
        valid = false;
    }
    
    if (m_config.milan_announce_interval_log != 0) {
        GPTP_LOG_ERROR("*** MILAN CONFIG ERROR: Announce interval log %d must be 0 (1s) per Milan specification ***", 
            m_config.milan_announce_interval_log);
        valid = false;
    }
    
    if (m_config.milan_pdelay_interval_log != 0) {
        GPTP_LOG_ERROR("*** MILAN CONFIG ERROR: PDelay interval log %d must be 0 (1s) per Milan specification ***", 
            m_config.milan_pdelay_interval_log);
        valid = false;
    }
    
    if (valid) {
        GPTP_LOG_INFO("*** MILAN: Configuration validation passed - compliant with Milan Baseline Interoperability Specification 2.0a ***");
    }
    
    return valid;
}

bool MilanProfile::isConvergenceAchieved() const {
    return m_stats.convergence_achieved;
}

uint64_t MilanProfile::getConvergenceTime() const {
    return m_stats.last_convergence_time;
}

bool MilanProfile::isWithinJitterLimits() const {
    return m_stats.sync_jitter_ns <= m_config.max_sync_jitter_ns;
}

bool MilanProfile::isWithinPathDelayLimits() const {
    return m_stats.path_delay_variation_ns <= m_config.max_path_delay_variation_ns;
}

// Milan B.1 Media Clock Holdover Implementation

bool MilanProfile::handleGrandmasterChange(const ClockIdentity& new_gm, const ClockIdentity& old_gm) {
    /* Milan B.1: Media Clock Holdover during Grandmaster Change
       
       In setups using gPTP, BTCA [802.1AS, Clause 10.3] is used to select the grandmaster. 
       In the event of a change of grandmaster during runtime, the media clock free-wheels 
       until a new grandmaster is selected. The time that the media clock is able to 
       free-wheel and regain synchronization without loss of audio samples is called the 
       holdover time. This specification recommends a minimum value of 5 seconds holdover 
       time and the following parameter behavior.
    */
    
    // Check if this is actually a grandmaster change
    if (new_gm == old_gm) {
        return false; // No change
    }
    
		GPTP_LOG_STATUS("*** MILAN B.1: Grandmaster change detected ***");
		GPTP_LOG_STATUS("*** MILAN B.1: Previous GM: %s ***", const_cast<ClockIdentity&>(old_gm).getIdentityString().c_str());
		GPTP_LOG_STATUS("*** MILAN B.1: New GM: %s ***", const_cast<ClockIdentity&>(new_gm).getIdentityString().c_str());
    
    // Store the grandmaster change information
    m_stats.previous_grandmaster = old_gm;
    m_stats.current_grandmaster = new_gm;
    m_stats.last_gm_change_time = TIMESTAMP_TO_NS(IEEE1588Clock::getSystemTime());
    
    // B.1.1: Set tu (timestamp uncertain) bit for 0.25 seconds
    m_stats.tu_bit_active = true;
    m_stats.tu_bit_start_time = m_stats.last_gm_change_time;
    
    GPTP_LOG_STATUS("*** MILAN B.1.1: Setting tu (timestamp uncertain) bit for %u ms ***", 
        m_config.tu_bit_duration_ms);
    
    // B.1.2: Activate media clock holdover if stream has been stable
    if (isStreamStable()) {
        m_stats.media_clock_holdover_active = true;
        GPTP_LOG_STATUS("*** MILAN B.1.2: Activating media clock holdover (stream stable for %llu seconds) ***", 
            getStreamStabilityTime() / 1000000000ULL);
    } else {
        GPTP_LOG_WARNING("*** MILAN B.1.2: Stream not stable enough for holdover (stable for %llu seconds, required: %u seconds) ***", 
            getStreamStabilityTime() / 1000000000ULL, m_config.stream_stability_time_ms / 1000);
    }
    
    // Reset stream stability timer for new grandmaster
    m_stats.current_stream_stability_time = 0;
    
    return true;
}

bool MilanProfile::isMediaClockHoldoverRequired(const ClockIdentity& new_gm) const {
    // Milan B.1.2: Media clock holdover is required if:
    // 1. Stream has been stable for more than 60 seconds before GM change
    // 2. All devices in AVB domain agree on new GM within 5 seconds
    
    return isStreamStable() && 
           m_stats.media_clock_holdover_active &&
           isWithinGrandmasterConvergenceTime();
}

bool MilanProfile::shouldSetTimestampUncertain() const {
    // Milan B.1.1: tu bit shall be set to 1 for duration of 0.25 seconds
    if (!m_stats.tu_bit_active) {
        return false;
    }
    
    uint64_t current_time = TIMESTAMP_TO_NS(IEEE1588Clock::getSystemTime());
    uint64_t elapsed_time_ms = (current_time - m_stats.tu_bit_start_time) / 1000000ULL;
    
    return elapsed_time_ms < m_config.tu_bit_duration_ms;
}

void MilanProfile::updateTimestampUncertainBit(bool force_update) {
    // Update tu bit state based on elapsed time since GM change
    if (m_stats.tu_bit_active || force_update) {
        bool should_set = shouldSetTimestampUncertain();
        
        if (m_stats.tu_bit_active && !should_set) {
            // tu bit period has expired
            m_stats.tu_bit_active = false;
            GPTP_LOG_STATUS("*** MILAN B.1.1: tu (timestamp uncertain) bit cleared after %u ms ***", 
                m_config.tu_bit_duration_ms);
        }
    }
}

bool MilanProfile::isStreamStable() const {
    // Milan B.1.2: Stream must be stable for more than 60 seconds
    return getStreamStabilityTime() >= (m_config.stream_stability_time_ms * 1000000ULL); // Convert ms to ns
}

uint64_t MilanProfile::getStreamStabilityTime() const {
    // Return current stream stability time in nanoseconds
    return m_stats.current_stream_stability_time;
}

void MilanProfile::notifyStreamStart() {
    // Called when a stream starts or becomes stable
    uint64_t current_time = TIMESTAMP_TO_NS(IEEE1588Clock::getSystemTime());
    
    if (m_stats.current_stream_stability_time == 0) {
        // First time stream is starting
        m_stats.current_stream_stability_time = current_time;
        GPTP_LOG_STATUS("*** MILAN B.1.2: Stream stability timer started ***");
    }
    
    // Update stream stability tracking
    uint64_t stability_duration = current_time - m_stats.current_stream_stability_time;
    if (stability_duration >= (m_config.stream_stability_time_ms * 1000000ULL)) {
        GPTP_LOG_DEBUG("*** MILAN B.1.2: Stream has been stable for %llu seconds ***", 
            stability_duration / 1000000000ULL);
    }
}

void MilanProfile::notifyGrandmasterChange(const ClockIdentity& new_gm) {
    // Public method to notify of grandmaster change
    handleGrandmasterChange(new_gm, m_stats.current_grandmaster);
}

bool MilanProfile::isWithinGrandmasterConvergenceTime() const {
    // Milan B.1.2: All devices must agree on new grandmaster within 5 seconds
    if (m_stats.last_gm_change_time == 0) {
        return true; // No GM change yet
    }
    
    uint64_t current_time = TIMESTAMP_TO_NS(IEEE1588Clock::getSystemTime());
    uint64_t elapsed_time_ms = (current_time - m_stats.last_gm_change_time) / 1000000ULL;
    
    bool within_time = elapsed_time_ms < m_config.gm_change_convergence_time_ms;
    
    if (!within_time && m_stats.media_clock_holdover_active) {
        // Convergence time exceeded - disable holdover
        const_cast<MilanStats&>(m_stats).media_clock_holdover_active = false;
        GPTP_LOG_WARNING("*** MILAN B.1.2: Grandmaster convergence time exceeded (%llu ms > %u ms) - disabling media clock holdover ***", 
            elapsed_time_ms, m_config.gm_change_convergence_time_ms);
    }
    
    return within_time;
}

// Milan B.1 PortIdentity-based grandmaster change handling
bool MilanProfile::handleGrandmasterChange(const PortIdentity& new_gm, const PortIdentity& old_gm) {
    // Extract ClockIdentity from PortIdentity and call existing method
    // Note: We need to cast away const to call getClockIdentity()
    PortIdentity& new_gm_ref = const_cast<PortIdentity&>(new_gm);
    PortIdentity& old_gm_ref = const_cast<PortIdentity&>(old_gm);
    
    ClockIdentity new_gm_clock = new_gm_ref.getClockIdentity();
    ClockIdentity old_gm_clock = old_gm_ref.getClockIdentity();
    
    return handleGrandmasterChange(new_gm_clock, old_gm_clock);
}

// Milan B.1 asCapable state change handling for stream stability
void MilanProfile::handleAsCapableChange(bool new_as_capable) {
    GPTP_LOG_STATUS("*** MILAN B.1: asCapable state changed to %s ***", 
        new_as_capable ? "TRUE" : "FALSE");
    
    if (new_as_capable) {
        // Stream is now capable - start/update stability tracking
        notifyStreamStart();
    } else {
        // Stream lost capability - reset stability tracking
        m_stats.current_stream_stability_time = 0;
        m_stats.media_clock_holdover_active = false;
        GPTP_LOG_STATUS("*** MILAN B.1.2: Stream stability reset due to asCapable=false ***");
    }
}
