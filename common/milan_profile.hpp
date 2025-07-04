/**
 * @file milan_profile.hpp
 * @brief Milan Baseline Interoperability Profile Implementation
 * 
 * Implementation of the Milan Baseline Interoperability Profile as defined in
 * Milan Baseline Interoperability Specification 2.0a
 * 
 * Key Requirements:
 * - asCapable TRUE after 2-5 successful PDelay exchanges
 * - Late response tolerance (10-15ms) without losing asCapable
 * - Fast convergence target: 100ms
 * - Enhanced clock quality requirements
 */

#ifndef MILAN_PROFILE_HPP
#define MILAN_PROFILE_HPP

#include "profile_interface.hpp"
#include "avbts_clock.hpp"
#include <cstdint>

/**
 * @brief Milan profile-specific configuration structure
 */
struct MilanConfig {
    uint32_t max_convergence_time_ms;      // 100ms convergence target
    uint32_t max_sync_jitter_ns;           // 1000ns max sync jitter
    uint32_t max_path_delay_variation_ns;  // 10000ns max path delay variation
    bool stream_aware_bmca;                 // Stream-aware BMCA support
    bool redundant_gm_support;              // Redundant grandmaster support
    int8_t milan_sync_interval_log;         // -3 (125ms)
    int8_t milan_announce_interval_log;     // 0 (1s)
    int8_t milan_pdelay_interval_log;       // 0 (1s)
    
    // Milan B.1 Media Clock Holdover Configuration
    uint32_t media_clock_holdover_time_ms; // Minimum 5 seconds (5000ms)
    uint32_t tu_bit_duration_ms;           // 0.25 seconds (250ms) for tu bit
    uint32_t gm_change_convergence_time_ms; // 5 seconds max for GM agreement
    uint32_t stream_stability_time_ms;     // 60 seconds min stream stability
    uint32_t bridge_propagation_time_ms;   // <50ms bridge propagation time
};

/**
 * @brief Milan profile runtime statistics
 */
struct MilanStats {
    uint64_t convergence_start_time;        // When convergence started (ns)
    uint64_t last_convergence_time;         // Last measured convergence time (ns)
    uint32_t sync_jitter_ns;                // Current sync jitter (ns)
    uint32_t path_delay_variation_ns;       // Current path delay variation (ns)
    uint64_t total_sync_messages;           // Total sync messages received
    uint64_t total_announce_messages;       // Total announce messages received
    bool convergence_achieved;               // Whether 100ms target was met
    
    // Milan B.1 Media Clock Holdover Statistics
    uint64_t last_gm_change_time;          // When last GM change occurred (ns)
    uint64_t current_stream_stability_time; // How long current stream has been stable (ns)
    bool media_clock_holdover_active;       // Whether holdover is currently active
    bool tu_bit_active;                     // Whether tu (timestamp uncertain) bit is set
    uint64_t tu_bit_start_time;            // When tu bit was set (ns)
    ClockIdentity current_grandmaster;      // Current grandmaster identity
    ClockIdentity previous_grandmaster;     // Previous grandmaster identity
};

/**
 * @brief Milan Baseline Interoperability Profile implementation
 * 
 * This class implements the Milan profile requirements including:
 * - Specific timing intervals (125ms sync, 1s announce/pdelay)
 * - Enhanced asCapable logic with late response tolerance
 * - Fast convergence requirements (100ms target)
 * - Enhanced clock quality parameters
 */
class MilanProfile : public ProfileInterface {
private:
    MilanConfig m_config;
    mutable MilanStats m_stats;

public:
    MilanProfile();
    virtual ~MilanProfile() = default;

    // Profile identification
    std::string getProfileName() const override {
        return "Milan Baseline Interoperability Profile";
    }

    std::string getProfileVersion() const override {
        return "2.0a";
    }

    // Configuration methods
    ProfileTimingConfig getTimingConfig() const override;
    ProfileClockQuality getClockQuality() const override;
    ProfileAsCapableBehavior getAsCapableBehavior() const override;

    // Runtime behavior methods
    bool shouldSendAnnounce(bool as_capable, bool is_grandmaster) const override;
    bool shouldProcessSync(bool as_capable) const override;
    bool shouldStartPDelay(bool link_up) const override;

    // asCapable state management (Milan-specific logic)
    bool evaluateAsCapable(
        unsigned int pdelay_count,
        bool current_as_capable,
        bool pdelay_success,
        bool pdelay_timeout,
        bool pdelay_late,
        unsigned int consecutive_late_count
    ) const override;

    bool evaluateAsCapableOnLinkUp(bool link_up) const override;
    bool evaluateAsCapableOnLinkDown(bool link_down) const override;

    // Protocol-specific behaviors
    bool allowsNegativeCorrectionField() const override { 
        // Milan Baseline Interoperability Specification: Standard gPTP behavior
        return false; 
    }
    
    bool requiresStrictTimeouts() const override { 
        // Milan requires strict timing compliance for interoperability
        return true; 
    }
    
    bool supportsBMCA() const override { return true; }

    // Milan-specific statistics and monitoring
    void updateTimingStats(uint64_t sync_timestamp, uint64_t arrival_time) override;
    void updatePDelayStats(uint64_t path_delay_ns) override;
    bool checkComplianceRequirements() const override;

    // Configuration management
    bool loadFromConfigFile(const std::string& config_path) override;
    bool validateConfiguration() const override;

    // Milan-specific getters
    const MilanConfig& getMilanConfig() const { return m_config; }
    const MilanStats& getMilanStats() const { return m_stats; }
    MilanStats& getMilanStats() { return m_stats; }

    // Milan-specific methods
    bool isConvergenceAchieved() const;
    uint64_t getConvergenceTime() const;
    bool isWithinJitterLimits() const;
    bool isWithinPathDelayLimits() const;
    
    // Milan B.1 Media Clock Holdover methods
    bool handleGrandmasterChange(const ClockIdentity& new_gm, const ClockIdentity& old_gm);
    bool handleGrandmasterChange(const PortIdentity& new_gm, const PortIdentity& old_gm);
    void handleAsCapableChange(bool new_as_capable);
    bool isMediaClockHoldoverRequired(const ClockIdentity& new_gm) const;
    bool shouldSetTimestampUncertain() const;
    void updateTimestampUncertainBit(bool force_update = false);
    bool isStreamStable() const;
    uint64_t getStreamStabilityTime() const;
    void notifyStreamStart();
    void notifyGrandmasterChange(const ClockIdentity& new_gm);
    bool isWithinGrandmasterConvergenceTime() const;
};

#endif // MILAN_PROFILE_HPP
