// Platform-specific clock implementation for the ESP32-S3 target.
// Provides the ticks_by_far() symbol declared by the engine in synth/utils.h.
// Resolution: microseconds since boot, sourced from esp_timer.

#include "esp_timer.h"

#include <cstdint>

uint64_t ticks_by_far() {
    return static_cast<uint64_t>(esp_timer_get_time());
}
