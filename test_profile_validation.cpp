/**
 * @file test_profile_validation.cpp
 * @brief Test program to validate all profile configurations
 * 
 * This program tests and validates all gPTP profile configurations
 * to ensure they meet specification requirements.
 */

#include "gptp_profile.hpp"
#include "gptp_log.hpp"
#include <iostream>
#include <iomanip>

void printProfile(const gPTPProfile& profile) {
    std::cout << "\n=== " << profile.profile_description << " v" << profile.profile_version << " ===\n";
    
    // Timing configuration
    std::cout << "Timing Intervals:\n";
    std::cout << "  sync_interval_log: " << (int)profile.sync_interval_log 
              << " (" << (1000 >> (-profile.sync_interval_log)) << "ms)\n";
    std::cout << "  announce_interval_log: " << (int)profile.announce_interval_log << "\n";
    std::cout << "  pdelay_interval_log: " << (int)profile.pdelay_interval_log << "\n";
    
    // Clock quality
    std::cout << "Clock Quality:\n";
    std::cout << "  clock_class: " << (int)profile.clock_class << "\n";
    std::cout << "  clock_accuracy: 0x" << std::hex << (int)profile.clock_accuracy << std::dec << "\n";
    std::cout << "  priority1: " << (int)profile.priority1 << "\n";
    
    // asCapable behavior
    std::cout << "asCapable Behavior:\n";
    std::cout << "  initial_as_capable: " << (profile.initial_as_capable ? "TRUE" : "FALSE") << "\n";
    std::cout << "  as_capable_on_link_up: " << (profile.as_capable_on_link_up ? "TRUE" : "FALSE") << "\n";
    std::cout << "  min_pdelay_successes: " << profile.min_pdelay_successes << "\n";
    std::cout << "  max_pdelay_successes: " << profile.max_pdelay_successes << "\n";
    
    // Protocol behavior
    std::cout << "Protocol Features:\n";
    std::cout << "  supports_bmca: " << (profile.supports_bmca ? "TRUE" : "FALSE") << "\n";
    std::cout << "  bmca_enabled: " << (profile.bmca_enabled ? "TRUE" : "FALSE") << "\n";
    std::cout << "  automotive_test_status: " << (profile.automotive_test_status ? "TRUE" : "FALSE") << "\n";
    std::cout << "  force_slave_mode: " << (profile.force_slave_mode ? "TRUE" : "FALSE") << "\n";
    
    // Compliance limits
    if (profile.max_convergence_time_ms > 0) {
        std::cout << "Compliance Limits:\n";
        std::cout << "  max_convergence_time_ms: " << profile.max_convergence_time_ms << "\n";
        std::cout << "  max_sync_jitter_ns: " << profile.max_sync_jitter_ns << "\n";
    }
}

void validateProfiles() {
    std::cout << "=== gPTP Profile Validation Test ===\n";
    
    // Test all profiles
    auto milan = gPTPProfileFactory::createMilanProfile();
    auto avnu_base = gPTPProfileFactory::createAvnuBaseProfile();
    auto automotive = gPTPProfileFactory::createAutomotiveProfile();
    auto standard = gPTPProfileFactory::createStandardProfile();
    
    printProfile(milan);
    printProfile(avnu_base);
    printProfile(automotive);
    printProfile(standard);
    
    // Validate profiles
    std::cout << "\n=== Profile Validation Results ===\n";
    std::cout << "Milan profile valid: " << (gPTPProfileFactory::validateProfile(milan) ? "PASS" : "FAIL") << "\n";
    std::cout << "AVnu Base profile valid: " << (gPTPProfileFactory::validateProfile(avnu_base) ? "PASS" : "FAIL") << "\n";
    std::cout << "Automotive profile valid: " << (gPTPProfileFactory::validateProfile(automotive) ? "PASS" : "FAIL") << "\n";
    std::cout << "Standard profile valid: " << (gPTPProfileFactory::validateProfile(standard) ? "PASS" : "FAIL") << "\n";
    
    // Test profile descriptions
    std::cout << "\n=== Profile Descriptions ===\n";
    std::cout << "Milan: " << gPTPProfileFactory::getProfileDescription(milan) << "\n";
    std::cout << "AVnu Base: " << gPTPProfileFactory::getProfileDescription(avnu_base) << "\n";
    std::cout << "Automotive: " << gPTPProfileFactory::getProfileDescription(automotive) << "\n";
    std::cout << "Standard: " << gPTPProfileFactory::getProfileDescription(standard) << "\n";
    
    // Test profile by name
    std::cout << "\n=== Profile by Name Test ===\n";
    auto milan_by_name = gPTPProfileFactory::createProfileByName("milan");
    std::cout << "Milan by name matches: " << (milan_by_name.profile_name == milan.profile_name ? "PASS" : "FAIL") << "\n";
    
    auto unknown = gPTPProfileFactory::createProfileByName("unknown");
    std::cout << "Unknown profile defaults to: " << unknown.profile_name << "\n";
}

int main() {
    // Initialize logging
    // GPTP_LOG_INFO("Starting profile validation test");
    
    validateProfiles();
    
    std::cout << "\n=== Profile Validation Complete ===\n";
    return 0;
}
