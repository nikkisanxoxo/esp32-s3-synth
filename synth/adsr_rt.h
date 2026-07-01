#ifndef ADSR_RT_H
#define ADSR_RT_H

#include <cstdint>
#include "constants.h"


// REAL-TIME ADSR (without Lookup-Table)

struct ADSR_RealTime {
    enum class State : uint8_t {
        Idle,
        Attack,
        Decay,
        Sustain,
        Release
    };

    State state{State::Idle};
    sample_t level{0.0f};
    sample_t current_level{0.0f};

    // ADSR-Parameters
    sample_t attack_s{0.2f};
    sample_t decay_s{0.7f};
    sample_t sustain_level{0.8f};
    sample_t release_s{0.4f};

    sample_t attack_step{0.0f};
    sample_t decay_step{0.0f};
    sample_t release_step{0.0f};

    sample_t sr{sample_rate};

    void set_sample_rate(sample_t rate) {
        sr = rate;
        recalc_steps();
    }

    void set_params(sample_t a, sample_t d, sample_t s, sample_t r) {
        attack_s = (a > 0.0001f) ? a : 0.0001f;
        decay_s = (d > 0.0001f) ? d : 0.0001f;
        sustain_level = (s < 0.0f) ? 0.0f : (s > 1.0f) ? 1.0f : s;
        release_s = (r > 0.0001f) ? r : 0.0001f;
        recalc_steps();
    }

    void recalc_steps() {
        attack_step = 1.0f / (attack_s * sr);
        decay_step = (1.0f - sustain_level) / (decay_s * sr);
        release_step = 1.0f / (release_s * sr);
    }

    void gate_on(sample_t start_level = 0.0f) {
        level = (start_level >= 0.0f) ? start_level : 0.0f;
        state = State::Attack;
    }

    void gate_off() {
        if (state != State::Idle && state != State::Release) {
            state = State::Release;
        }
    }

    bool is_idle() const {
        return state == State::Idle;
    }

    bool is_active() const {
        return state != State::Idle;
    }

    bool is_in_release() const {
        return state == State::Release;
    }

    bool in_release() const {
        return state == State::Release;
    }

    sample_t process() {
        switch (state) {
            case State::Attack:
                level += attack_step;
                if (level >= 1.0f) {
                    level = 1.0f;
                    state = State::Decay;
                }
                break;

            case State::Decay:
                level -= decay_step;
                if (level <= sustain_level) {
                    level = sustain_level;
                    state = State::Sustain;
                }
                break;

            case State::Sustain:
                break;

            case State::Release:
                level -= release_step;
                if (level <= 0.0f) {
                    level = 0.0f;
                    state = State::Idle;
                }
                break;

            case State::Idle:
                break;
        }
        current_level = level;
        return level;
    }

    // Mono-Buffer (in-place)
    void process_buffer(sample_t* buffer, uint32_t samples) {
        for (uint32_t i = 0; i < samples; ++i) {
            sample_t env = process();
            buffer[i] *= env;
        }
    }

    // Stereo-Buffer (in-place, interleaved L,R,L,R,...)
    void process_stereo_buffer(sample_t* buffer, uint32_t samples) {
        for (uint32_t i = 0; i < samples * 2; i += 2) {
            sample_t env = process();
            buffer[i] *= env;
            buffer[i + 1] *= env;
        }
    }

    void reset() {
        state = State::Idle;
        level = 0.0f;
        current_level = 0.0f;
        recalc_steps();
    }

    sample_t operator()() {
        return process();
    }
};

#endif // ADSR_RT_H
