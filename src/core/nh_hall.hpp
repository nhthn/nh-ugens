#pragma once
#include <cstring> // for memset
#include <memory> // std::unique_ptr
#include <array> // std::array
#include <cmath> // cosf/sinf

/*
TODO:

- Make shelving filter parameters user-modulatable
- Improve handling of denormals
- Implement cubic interpolation
- Modulate early reflections

*/

namespace nh_ugens {

typedef std::array<float, 2> Stereo;

static inline int next_power_of_two(int x) {
    int result = 1;
    while (result < x) {
        result *= 2;
    }
    return result;
}

// TODO: implement proper cubic interpolation. using linear for now
static float interpolate_cubic(float x, float y0, float y1, float y2, float y3) {
    return y1 + (y2 - y1) * x;
}

// Unitary rotation matrix. Angle is given in radians.
static inline Stereo rotate(Stereo x, float angle) {
    Stereo result = {
        cosf(angle) * x[0] - sinf(angle) * x[1],
        sinf(angle) * x[0] + cosf(angle) * x[1]
    };
    return result;
}

constexpr float twopi = 6.283185307179586f;

// Default allocator -- not real-time safe!
class Allocator {
public:
    void* allocate(int memory_size) {
        return malloc(memory_size);
    }

    void deallocate(void* memory) {
        free(memory);
    }
};

// Quadrature sine LFO, not used.
class SineLFO {
public:
    SineLFO(
        float sample_rate
    ) :
    m_sample_rate(sample_rate),
    m_cosine(1.0f),
    m_sine(0.0f)
    {
    }

    void set_frequency(float frequency) {
        m_k = twopi * frequency / m_sample_rate;
    }

    Stereo process(void) {
        m_cosine -= m_k * m_sine;
        m_sine += m_k * m_cosine;
        Stereo out = {{m_cosine, m_sine}};
        return out;
    }

private:
    const float m_sample_rate;
    float m_k;
    float m_cosine;
    float m_sine;
};

// TODO: Sample-rate invariance is not established here.
class RandomLFO {
public:
    RandomLFO(
        float sample_rate
    ) :
    m_sample_rate(sample_rate)
    {
    }

    inline uint16_t run_lcg(void) {
        m_lcg_state = m_lcg_state * 22695477 + 1;
        uint16_t result = m_lcg_state >> 16;
        return result;
    }

    void set_frequency(float frequency) {
        m_frequency = frequency;
    }

    Stereo process(void) {
        if (m_timeout <= 0) {
            m_timeout = (run_lcg() >> 3) * 3.0f / m_frequency;
            m_increment = (run_lcg() * (1.0f / 32767.0f) - 0.5f) / m_sample_rate * m_frequency;
        }
        m_timeout -= 1;
        m_phase += m_increment;
        Stereo result = {{sinf(m_phase), cosf(m_phase)}};
        return result;
    }

private:
    const float m_sample_rate;
    uint32_t m_lcg_state = 1;
    int m_timeout = 0;

    float m_increment = 0.f;
    float m_phase = 0.f;
    float m_frequency = 12.f;
};

class DCBlocker {
public:
    DCBlocker(
        float sample_rate
    ) :
    m_sample_rate(sample_rate)
    {
    }

    float process(float in) {
        float x = in;
        float y = x - m_x1 + m_k * m_y1;
        float out = y;
        m_x1 = x;
        m_y1 = y;
        return out;
    }

private:
    const float m_sample_rate;

    float m_x1 = 0.0f;
    float m_y1 = 0.0f;
    float m_k = 0.99f;
};

class HiShelf {
public:
    HiShelf(
        float sample_rate
    ) :
    m_sample_rate(sample_rate)
    {
        set_frequency_and_gain(3000.0f, -2.0f);
    }

    float set_frequency_and_gain(float frequency, float gain) {
        float w0 = twopi * frequency / m_sample_rate;
        float sin_w0 = sinf(w0);
        float cos_w0 = cosf(w0);
        float a = powf(10.0f, gain / 40.0f);
        float s = 1.0f;
        float alpha = sin_w0 * 0.5 * sqrtf((a + 1 / a) * (1 / s - 1) + 2);
        float x = 2 * sqrtf(a) * alpha;
        float a0 = (a + 1) - (a - 1) * cos_w0 + x;
        m_b0 = (a * ((a + 1) + (a - 1) * cos_w0 + x)) / a0;
        m_b1 = (-2 * a * ((a - 1) + (a + 1) * cos_w0)) / a0;
        m_b2 = (a * ((a + 1) + (a - 1) * cos_w0 - x)) / a0;
        m_a1 = (2 * ((a - 1) - (a + 1) * cos_w0)) / a0;
        m_a2 = ((a + 1) - (a - 1) * cos_w0 - x) / a0;
    }

    float process(float in) {
        float out =
            m_b0 * in + m_b1 * m_x1 + m_b2 * m_x2
            - m_a1 * m_y1 - m_a2 * m_y2;
        m_x2 = m_x1;
        m_x1 = in;
        m_y2 = m_y1;
        m_y1 = out;
        return out;
    }

private:
    const float m_sample_rate;
    float m_x1 = 0.0f;
    float m_x2 = 0.0f;
    float m_y1 = 0.0f;
    float m_y2 = 0.0f;
    float m_b0, m_b1, m_b2, m_a0, m_a1, m_a2;
};

class LowShelf {
public:
    LowShelf(
        float sample_rate
    ) :
    m_sample_rate(sample_rate)
    {
        set_frequency_and_gain(150.0f, -2.0f);
    }

    float set_frequency_and_gain(float frequency, float gain) {
        float w0 = twopi * frequency / m_sample_rate;
        float sin_w0 = sinf(w0);
        float cos_w0 = cosf(w0);
        float a = powf(10.0f, gain / 40.0f);
        float s = 1.0f;
        float alpha = sin_w0 * 0.5 * sqrtf((a + 1 / a) * (1 / s - 1) + 2);
        float x = 2 * sqrtf(a) * alpha;
        float a0 = (a + 1) + (a - 1) * cos_w0 + x;
        m_b0 = (a * ((a + 1) - (a - 1) * cos_w0 + x)) / a0;
        m_b1 = (2 * a * ((a - 1) - (a + 1) * cos_w0)) / a0;
        m_b2 = (a * ((a + 1) - (a - 1) * cos_w0 - x)) / a0;
        m_a1 = (-2 * ((a - 1) + (a + 1) * cos_w0)) / a0;
        m_a2 = ((a + 1) + (a - 1) * cos_w0 - x) / a0;
    }

    float process(float in) {
        float out =
            m_b0 * in + m_b1 * m_x1 + m_b2 * m_x2
            - m_a1 * m_y1 - m_a2 * m_y2;
        m_x2 = m_x1;
        m_x1 = in;
        m_y2 = m_y1;
        m_y1 = out;
        return out;
    }

private:
    const float m_sample_rate;
    float m_x1 = 0.0f;
    float m_x2 = 0.0f;
    float m_y1 = 0.0f;
    float m_y2 = 0.0f;
    float m_b0, m_b1, m_b2, m_a0, m_a1, m_a2;
};


class BaseDelay {
public:
    int m_size;
    float* m_buffer;

    BaseDelay(
        float sample_rate,
        float max_delay,
        float delay
    ) :
    m_sample_rate(sample_rate)
    {
        int max_delay_in_samples = m_sample_rate * max_delay;
        m_size = next_power_of_two(max_delay_in_samples);
        m_mask = m_size - 1;

        m_read_position = 0;

        m_delay = delay;
        m_delay_in_samples = m_sample_rate * delay;
    }

protected:
    const float m_sample_rate;
    int m_mask;
    int m_read_position;
    float m_delay;
    int m_delay_in_samples;
};

// Fixed delay line.
class Delay : public BaseDelay {
public:
    Delay(
        float sample_rate,
        float delay
    ) :
    BaseDelay(sample_rate, delay, delay)
    {
    }

    float process(float in) {
        float out_value = m_buffer[(m_read_position - m_delay_in_samples) & m_mask];
        m_buffer[m_read_position] = in;
        m_read_position = (m_read_position + 1) & m_mask;
        float out = out_value;
        return out;
    }

    float tap(float delay, float gain) {
        int delay_in_samples = delay * m_sample_rate;
        int position = m_read_position - 1 - delay_in_samples;
        float out = gain * m_buffer[position & m_mask];
        return out;
    }
};

// Fixed Schroeder allpass.
class Allpass : public BaseDelay {
public:
    Allpass(
        float sample_rate,
        float delay,
        float k
    ) :
    BaseDelay(sample_rate, delay, delay),
    m_k(k)
    {
    }

    float process(float in) {
        float delayed_signal = m_buffer[(m_read_position - m_delay_in_samples) & m_mask];
        float feedback_plus_input =
            in + delayed_signal * m_k;
        m_buffer[m_read_position] = feedback_plus_input;
        m_read_position = (m_read_position + 1) & m_mask;
        float out = feedback_plus_input * -m_k + delayed_signal;
        return out;
    }

private:
    float m_k;
};

// Schroeder allpass with variable delay and cubic interpolation.
class VariableAllpass : public BaseDelay {
public:
    VariableAllpass(
        float sample_rate,
        float max_delay,
        float delay,
        float k
    ) :
    BaseDelay(sample_rate, max_delay, delay),
    m_k(k)
    {
    }

    float process(float in, float offset) {
        float position = m_read_position - (m_delay + offset) * m_sample_rate;
        int iposition = position;
        float position_frac = position - iposition;

        float y0 = m_buffer[iposition & m_mask];
        float y1 = m_buffer[(iposition + 1) & m_mask];
        float y2 = m_buffer[(iposition + 2) & m_mask];
        float y3 = m_buffer[(iposition + 3) & m_mask];

        float delayed_signal = interpolate_cubic(position_frac, y0, y1, y2, y3);

        float feedback_plus_input =
            in + delayed_signal * m_k;
        m_buffer[m_read_position] = feedback_plus_input;
        m_read_position = (m_read_position + 1) & m_mask;
        float out = feedback_plus_input * -m_k + delayed_signal;

        return out;
    }

private:
    float m_k;
};

template <class Alloc = Allocator>
class Unit {
public:
    float m_k;

    Unit(
        float sample_rate,
        std::unique_ptr<Alloc> allocator
    ) :
    m_sample_rate(sample_rate),
    m_allocator(std::move(allocator)),

    m_lfo(sample_rate),
    m_dc_blocker(sample_rate),

    m_low_shelves {{sample_rate, sample_rate, sample_rate, sample_rate}},
    m_hi_shelves {{sample_rate, sample_rate, sample_rate, sample_rate}},

    m_early_allpasses {{
        Allpass(sample_rate, 14.5e-3f, 0.5f),
        Allpass(sample_rate, 6.0e-3f, 0.5f),
        Allpass(sample_rate, 12.8e-3f, 0.5f),
        Allpass(sample_rate, 7.2e-3f, 0.5f),
        Allpass(sample_rate, 13.5e-3f, 0.5f),
        Allpass(sample_rate, 8.0e-3f, 0.5f),
        Allpass(sample_rate, 16.8e-3f, 0.5f),
        Allpass(sample_rate, 10.2e-3f, 0.5f)
    }},

    m_early_delays {{
        Delay(sample_rate, 5.45e-3),
        Delay(sample_rate, 3.25e-3),
        Delay(sample_rate, 7.36e-3),
        Delay(sample_rate, 2.17e-3)
    }},

    // TODO: Maximum delays for variable allpasses are temporary.
    m_late_variable_allpasses {{
        VariableAllpass(sample_rate, 100e-3f, 25.6e-3f, -0.55f),
        VariableAllpass(sample_rate, 120e-3f, 50.7e-3f, 0.55f),
        VariableAllpass(sample_rate, 100e-3f, 68.6e-3f, 0.55f),
        VariableAllpass(sample_rate, 100e-3f, 45.7e-3f, 0.55f)
    }},

    m_late_allpasses {{
        Allpass(sample_rate, 41.4e-3f, 0.55f),
        Allpass(sample_rate, 25.6e-3f, -0.55f),
        Allpass(sample_rate, 29.4e-3f, -0.55f),
        Allpass(sample_rate, 23.6e-3f, -0.55f)
    }},

    m_late_delays {{
        Delay(sample_rate, delay_time_1),
        Delay(sample_rate, delay_time_2),
        Delay(sample_rate, delay_time_3),
        Delay(sample_rate, delay_time_4)
    }}

    {
        m_k = 0.0f;

        for (auto& x : m_early_allpasses) {
            allocate_delay_line(x);
        }
        for (auto& x : m_early_delays) {
            allocate_delay_line(x);
        }
        for (auto& x : m_late_variable_allpasses) {
            allocate_delay_line(x);
        }
        for (auto& x : m_late_allpasses) {
            allocate_delay_line(x);
        }
        for (auto& x : m_late_delays) {
            allocate_delay_line(x);
        }
    }

    // If no allocator object is passed in, we try to make one ourselves by
    // calling the constructor with no arguments.
    Unit(
        float sample_rate
    ) :
    Unit(sample_rate, std::unique_ptr<Alloc>(new Alloc()))
    { }

    ~Unit() {
        for (auto& x : m_early_allpasses) {
            free_delay_line(x);
        }
        for (auto& x : m_early_delays) {
            free_delay_line(x);
        }
        for (auto& x : m_late_variable_allpasses) {
            free_delay_line(x);
        }
        for (auto& x : m_late_allpasses) {
            free_delay_line(x);
        }
        for (auto& x : m_late_delays) {
            free_delay_line(x);
        }
    }

    inline float compute_k_from_rt60(float rt60) {
        return powf(0.001f, average_delay_time / rt60);
    }

    inline void set_rt60(float rt60) {
        m_k = compute_k_from_rt60(rt60);
    }

    inline Stereo process_early(Stereo in) {
        Stereo sig = {{in[0], in[1]}};

        sig[0] = m_early_allpasses[0].process(sig[0]);
        sig[0] = m_early_allpasses[1].process(sig[0]);
        sig[1] = m_early_allpasses[2].process(sig[1]);
        sig[1] = m_early_allpasses[3].process(sig[1]);
        sig = rotate(sig, 0.2f);
        Stereo early = {{sig[0], sig[1]}};

        sig[0] = m_early_delays[0].process(sig[0]);
        sig[1] = m_early_delays[1].process(sig[1]);

        sig[0] = m_early_allpasses[4].process(sig[0]);
        sig[0] = m_early_allpasses[5].process(sig[0]);
        sig[1] = m_early_allpasses[6].process(sig[1]);
        sig[1] = m_early_allpasses[7].process(sig[1]);
        sig = rotate(sig, 0.8f);
        early[0] += sig[0] * 0.5f;
        early[1] += sig[1] * 0.5f;

        return early;
    }

    inline float process_late_left(float early_left, Stereo lfo) {
        float sig = 0.f;

        sig += m_feedback[0];

        sig += early_left;
        sig = m_late_variable_allpasses[0].process(sig, -lfo[0]);
        sig = m_late_allpasses[0].process(sig);
        sig *= m_k;
        sig = m_late_delays[0].process(sig);
        sig = m_low_shelves[0].process(sig);
        sig = m_hi_shelves[0].process(sig);

        sig += early_left;
        sig = m_late_variable_allpasses[1].process(sig, -lfo[1]);
        sig = m_late_allpasses[1].process(sig);
        sig *= m_k;
        sig = m_late_delays[1].process(sig);
        sig = m_low_shelves[1].process(sig);
        sig = m_hi_shelves[1].process(sig);

        return sig;
    }

    inline float process_late_right(float early_right, Stereo lfo) {
        float sig = 0.f;

        sig += m_feedback[1];

        sig += early_right;
        sig = m_late_variable_allpasses[2].process(sig, -lfo[0]);
        sig = m_late_allpasses[2].process(sig);
        sig *= m_k;
        sig = m_late_delays[2].process(sig);
        sig = m_low_shelves[2].process(sig);
        sig = m_hi_shelves[2].process(sig);

        sig += early_right;
        sig = m_late_variable_allpasses[3].process(sig, -lfo[1]);
        sig = m_late_allpasses[3].process(sig);
        sig *= m_k;
        sig = m_late_delays[3].process(sig);
        sig = m_low_shelves[3].process(sig);
        sig = m_hi_shelves[3].process(sig);

        return sig;
    }

    inline Stereo process_outputs(Stereo early) {
        // Keep the inter-channel delays somewhere between 0.1 and 0.7 ms --
        // this allows the Haas effect to come in.

        Stereo out = {{early[0] * 0.5f, early[1] * 0.5f}};

        float haas_multiplier = -0.8f;

        out[0] += m_late_delays[0].tap(0.0e-3f, 1.0f);
        out[1] += m_late_delays[0].tap(0.3e-3f, haas_multiplier);

        out[0] += m_late_delays[1].tap(0.0e-3f, 1.0f);
        out[1] += m_late_delays[1].tap(0.1e-3f, haas_multiplier);

        out[0] += m_late_delays[2].tap(0.7e-3f, haas_multiplier);
        out[1] += m_late_delays[2].tap(0.0e-3f, 1.0f);

        out[0] += m_late_delays[3].tap(0.2e-3f, haas_multiplier);
        out[1] += m_late_delays[3].tap(0.0e-3f, 1.0f);

        return out;
    }

    Stereo process(Stereo in) {
        Stereo lfo = m_lfo.process();
        lfo[0] *= 0.32e-3f * 0.5f;
        lfo[1] *= -0.45e-3f * 0.5f;

        Stereo early = process_early(in);

        Stereo out = process_outputs(early);

        Stereo late = {{
            process_late_left(early[0], lfo),
            process_late_right(early[1], lfo)
        }};
        late = rotate(late, 0.6f);
        m_feedback = late;

        return out;
    }

    Stereo process(float in_left, float in_right) {
        Stereo in = {{in_left, in_right}};
        return process(in);
    }

private:
    std::unique_ptr<Alloc> m_allocator;

    const float m_sample_rate;

    static constexpr float delay_time_1 = 183.6e-3f;
    static constexpr float delay_time_2 = 94.3e-3f;
    static constexpr float delay_time_3 = 157.6e-3f;
    static constexpr float delay_time_4 = 63.6e-3f;

    static constexpr float average_delay_time =
        (delay_time_1 + delay_time_2 + delay_time_3 + delay_time_4) / 4.0f;

    Stereo m_feedback = {{0.f, 0.f}};

    RandomLFO m_lfo;
    DCBlocker m_dc_blocker;

    std::array<LowShelf, 4> m_low_shelves;
    std::array<HiShelf, 4> m_hi_shelves;

    // NOTE: When adding new delay units, don't forget to allocate the memory
    // in the constructor and free it in the destructor.
    std::array<Allpass, 8> m_early_allpasses;
    std::array<Delay, 4> m_early_delays;

    std::array<VariableAllpass, 4> m_late_variable_allpasses;
    std::array<Allpass, 4> m_late_allpasses;
    std::array<Delay, 4> m_late_delays;

    void allocate_delay_line(BaseDelay& delay) {
        void* memory = m_allocator->allocate(sizeof(float) * delay.m_size);
        delay.m_buffer = static_cast<float*>(memory);
        memset(delay.m_buffer, 0, sizeof(float) * delay.m_size);
    }

    void free_delay_line(BaseDelay& delay) {
        m_allocator->deallocate(delay.m_buffer);
    }
};

} // namespace nh_ugens
