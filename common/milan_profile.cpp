/**
 * @file milan_profile.cpp
 * @brief Milan Baseline Interoperability Profile Implementation
 */

#include "milan_profile.hpp"
#include "gptp_log.hpp"
#include <cmath>

MilanProfile::MilanProfile() {
    // Initialize Milan configuration with defaults
    m_config.max_convergence_time_ms = 100;            // 100ms convergence target
    m_config.max_sync_jitter_ns = 1000;                // 1000ns max sync jitter
    m_config.max_path_delay_variation_ns = 10000;      // 10000ns max path delay variation
    m_config.stream_aware_bmca = false;                 // Optional feature
    m_config.redundant_gm_support = false;             // Optional feature
    m_config.milan_sync_interval_log = -3;             // 125ms (2^-3)
    m_config.milan_announce_interval_log = 0;          // 1s (2^0)
    m_config.milan_pdelay_interval_log = 0;            // 1s (2^0)

    // Initialize statistics
    memset(&m_stats, 0, sizeof(m_stats));
}

ProfileTimingConfig MilanProfile::getTimingConfig() const {
    ProfileTimingConfig config;
    config.sync_interval_log = m_config.milan_sync_interval_log;        // -3 (125ms)
    config.announce_interval_log = m_config.milan_announce_interval_log; // 0 (1s)
    config.pdelay_interval_log = m_config.milan_pdelay_interval_log;     // 0 (1s)
    config.sync_receipt_timeout = 3;                                     // Standard timeout
    config.announce_receipt_timeout = 3;                                 // Standard timeout
    config.neighbor_prop_delay_thresh = 800000;                         // 800μs threshold
    return config;
}

ProfileClockQuality MilanProfile::getClockQuality() const {
    ProfileClockQuality quality;
    quality.clock_class = 6;                // Primary reference quality
    quality.clock_accuracy = 0x20;          // 32ns accuracy  
    quality.offset_scaled_log_variance = 0x4000; // Standard variance
    quality.priority1 = 248;                // Default application priority
    quality.priority2 = 248;                // Default priority2
    return quality;
}

ProfileAsCapableBehavior MilanProfile::getAsCapableBehavior() const {
    ProfileAsCapableBehavior behavior;
    behavior.initial_as_capable = true;                 // Milan starts with asCapable=true for fast convergence
    behavior.min_pdelay_successes = 2;                  // Minimum 2 successful PDelay exchanges
    behavior.max_pdelay_successes = 5;                  // Maximum 5 successful PDelay exchanges
    behavior.maintain_on_late_response = true;          // Maintain asCapable on late responses
    behavior.late_response_threshold_ms = 10;           // 10ms threshold for "late"
    behavior.consecutive_late_limit = 3;                // Allow 3+ consecutive late responses
    return behavior;
}

bool MilanProfile::shouldSendAnnounce(bool as_capable, bool is_grandmaster) const {
    // Milan: Only send announce when asCapable=true
    return as_capable;
}

bool MilanProfile::shouldProcessSync(bool as_capable) const {
    // Milan: Process sync messages regardless of asCapable state for monitoring
    return true;
}

bool MilanProfile::shouldStartPDelay(bool link_up) const {
    // Milan: Start PDelay immediately when link is up
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
    // Milan Specification 5.6.2.4: asCapable logic
    
    if (pdelay_timeout) {
        // Timeout occurred - check if we should lose asCapable
        if (pdelay_count < getAsCapableBehavior().min_pdelay_successes) {
            // Haven't met minimum requirement yet
            GPTP_LOG_STATUS("*** MILAN: PDelay timeout before minimum exchanges (%d/%d) - maintaining current asCapable=%s ***", 
                pdelay_count, getAsCapableBehavior().min_pdelay_successes, current_as_capable ? "true" : "false");
            return current_as_capable;
        } else {
            // Had successful exchanges before, temporary timeout - don't immediately disable
            GPTP_LOG_STATUS("*** MILAN: PDelay timeout after %d successful exchanges - maintaining asCapable=true ***", pdelay_count);
            return true;
        }
    }
    
    if (pdelay_success) {
        // Successful PDelay exchange
        if (pdelay_late && consecutive_late_count >= getAsCapableBehavior().consecutive_late_limit) {
            // Multiple consecutive late responses - but still receiving responses
            // Milan Annex B.2.3: Don't lose asCapable for late but not missing responses
            GPTP_LOG_STATUS("*** MILAN: %d consecutive late responses (>%dms) but still receiving - maintaining asCapable=true ***", 
                consecutive_late_count, getAsCapableBehavior().late_response_threshold_ms);
            return true;
        }
        
        // Check if we've met the Milan requirement (2-5 successful exchanges)
        if (pdelay_count >= getAsCapableBehavior().min_pdelay_successes && 
            pdelay_count <= getAsCapableBehavior().max_pdelay_successes) {
            // Met Milan requirement - set asCapable=true
            GPTP_LOG_STATUS("*** MILAN: Setting asCapable=true after %d successful PDelay exchanges (requirement: %d-%d) ***", 
                pdelay_count, getAsCapableBehavior().min_pdelay_successes, getAsCapableBehavior().max_pdelay_successes);
            return true;
        } else if (pdelay_count > getAsCapableBehavior().max_pdelay_successes) {
            // Beyond initial establishment phase - maintain asCapable=true
            return true;
        } else {
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
    // Milan: Set asCapable=true immediately on link up for fast convergence
    if (link_up) {
        GPTP_LOG_STATUS("*** MILAN: Setting asCapable=true on link up for fast convergence ***");
        return true;
    }
    return false;
}

bool MilanProfile::evaluateAsCapableOnLinkDown(bool link_down) const {
    // Milan: Lose asCapable when link goes down
    if (link_down) {
        GPTP_LOG_STATUS("*** MILAN: Setting asCapable=false on link down ***");
        return false;
    }
    return true;
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
    // Validate Milan configuration parameters
    bool valid = true;
    
    if (m_config.max_convergence_time_ms > 1000) {
        GPTP_LOG_ERROR("*** MILAN CONFIG ERROR: Convergence time %dms exceeds 1000ms limit ***", 
            m_config.max_convergence_time_ms);
        valid = false;
    }
    
    if (m_config.max_sync_jitter_ns > 10000) {
        GPTP_LOG_ERROR("*** MILAN CONFIG ERROR: Sync jitter %dns exceeds 10000ns limit ***", 
            m_config.max_sync_jitter_ns);
        valid = false;
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
