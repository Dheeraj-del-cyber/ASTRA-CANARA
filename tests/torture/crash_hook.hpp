#pragma once

#include <cstdlib>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

namespace torture {

inline std::string& requested_pause_phase() {
    static std::string phase;
    return phase;
}

inline void arm_test_phase_pause(const std::string& phase) {
    requested_pause_phase() = phase;
}

inline void crash_if_selected(const char* point) {
#if defined(KVDB_ENABLE_CRASH_HOOKS)
    const char* selected = std::getenv("KVDB_TEST_CRASH_POINT");
    if (selected && std::string(selected) == point) {
        std::_Exit(86);
    }
#else
    (void)point;
#endif
}

inline void report_test_phase(const char* phase) {
#if defined(KVDB_ENABLE_CRASH_HOOKS)
    if (std::getenv("KVDB_TEST_REPORT_PHASES")) {
        std::cout << "KVDB_TEST_PHASE " << phase << "\n";
        std::cout.flush();
        const char* pause_phase = std::getenv("KVDB_TEST_PAUSE_PHASE");
        const std::string target = pause_phase
            ? pause_phase : requested_pause_phase();
        if (target == phase) {
            for (;;) std::this_thread::sleep_for(std::chrono::hours(24));
        }
    }
#else
    (void)phase;
#endif
}

} // namespace torture
