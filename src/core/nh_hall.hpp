#pragma once
#include <cstring> // for memset
#include <memory> // std::unique_ptr
#include <tuple> // std::tuple
#include <array> // std::array
#include <cmath> // cosf/sinf

/*
TODO:

- Improve damping
- Implement cubic interpolation
- Modulate early reflections

*/

namespace nh_ugens {

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
static inline std::tuple<float, float> rotate(float x1, float x2, float angle) {
    return std::make_tuple(
        cosf(angle) * x1 - sinf(angle) * x2,
        sinf(angle) * x1 + cosf(angle) * x2
    );
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

    std::tuple<float, float> process(void) {
        m_cosine -= m_k * m_sine;
        m_sine += m_k * m_cosine;
        return std::make_tuple(m_cosine, m_sine);
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

    std::tuple<float, float> process(void) {
        if (m_timeout <= 0) {
            m_timeout = (run_lcg() >> 3) * 3.0f / m_frequency;
            m_increment = (run_lcg() * (1.0f / 32767.0f) - 0.5f) / m_sample_rate * m_frequency;
        }
        m_timeout -= 1;
        m_phase += m_increment;
        return std::make_tuple(sin(m_phase), cos(m_phase));
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

    m_low_shelves {{ sample_rate, sample_rate, sample_rate, sample_rate }},
    m_hi_shelves {{ sample_rate, sample_rate, sample_rate, sample_rate }},

    // Double curly braces are required in C++11.
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
        m_feedback_left = 0.f;
        m_feedback_right = 0.f;

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

    Unit(
        float sample_rate
    ) :
    Unit(sample_rate, std::unique_ptr<Alloc>(new Alloc()))
    { }

    ~Unit() {
        for (auto& allpass : m_early_allpasses) {
            free_delay_line(allpass);
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

    inline std::tuple<float, float> process_early(float in_1, float in_2) {
        // Sound signal path
        float left = in_1;
        float right = in_2;

        float early_left = 0.f;
        float early_right = 0.f;

        left = m_early_allpasses[0].process(left);
        left = m_early_allpasses[1].process(left);
        right = m_early_allpasses[2].process(right);
        right = m_early_allpasses[3].process(right);
        std::tie(left, right) = rotate(left, right, 0.2f);
        early_left = left;
        early_right = right;

        left = m_early_delays[0].process(left);
        right = m_early_delays[1].process(right);

        left = m_early_allpasses[4].process(left);
        left = m_early_allpasses[5].process(left);
        right = m_early_allpasses[6].process(right);
        right = m_early_allpasses[7].process(right);
        std::tie(left, right) = rotate(left, right, 0.8f);
        early_left += left * 0.5f;
        early_right += right * 0.5f;

        return std::make_tuple(early_left, early_right);
    }

    std::tuple<float, float> process(float in_1, float in_2) {
        // LFO
        float lfo_1;
        float lfo_2;

        //m_lfo.set_frequency(0.5f);
        std::tie(lfo_1, lfo_2) = m_lfo.process();

        lfo_1 *= 0.32e-3f * 0.5f;
        lfo_2 *= -0.45e-3f * 0.5f;

        ///////////////////////////////////////////////////////////////////////
        // Early reflections

        float early_left;
        float early_right;
        std::tie(early_left, early_right) = process_early(in_1, in_2);

        ///////////////////////////////////////////////////////////////////////
        // Output taps

        // Keep the inter-channel delays somewhere between 0.1 and 0.7 ms --
        // this allows the Haas effect to come in.

        float out_1 = early_left * 0.5f;
        float out_2 = early_right * 0.5f;

        float haas_multiplier = -0.8f;

        out_1 += m_late_delays[0].tap(0.0e-3f, 1.0f);
        out_2 += m_late_delays[0].tap(0.3e-3f, haas_multiplier);

        out_1 += m_late_delays[1].tap(0.0e-3f, 1.0f);
        out_2 += m_late_delays[1].tap(0.1e-3f, haas_multiplier);

        out_1 += m_late_delays[2].tap(0.7e-3f, haas_multiplier);
        out_2 += m_late_delays[2].tap(0.0e-3f, 1.0f);

        out_1 += m_late_delays[3].tap(0.2e-3f, haas_multiplier);
        out_2 += m_late_delays[3].tap(0.0e-3f, 1.0f);

        ///////////////////////////////////////////////////////////////////////
        // Main reverb loop

        float left = 0.f;

        left += m_feedback_left;
        //left = m_dc_blocker.process(left);

        left += early_left;
        left = m_late_variable_allpasses[0].process(left, lfo_1);
        left = m_late_allpasses[0].process(left);
        left *= m_k;
        left = m_late_delays[0].process(left);
        left = m_low_shelves[0].process(left);
        left = m_hi_shelves[0].process(left);

        left += early_left;
        left = m_late_variable_allpasses[1].process(left, lfo_2);
        left = m_late_allpasses[1].process(left);
        left *= m_k;
        left = m_late_delays[1].process(left);
        left = m_low_shelves[1].process(left);
        left = m_hi_shelves[1].process(left);

        float right = 0.f;

        right += m_feedback_right;

        right += early_right;
        right = m_late_variable_allpasses[2].process(right, -lfo_1);
        right = m_late_allpasses[2].process(right);
        right *= m_k;
        right = m_late_delays[2].process(right);
        right = m_low_shelves[2].process(right);
        right = m_hi_shelves[2].process(right);

        right += early_right;
        right = m_late_variable_allpasses[3].process(right, -lfo_2);
        right = m_late_allpasses[3].process(right);
        right *= m_k;
        right = m_late_delays[3].process(right);
        right = m_low_shelves[3].process(right);
        right = m_hi_shelves[3].process(right);

        std::tie(left, right) = rotate(left, right, 0.6f);

        m_feedback_left = left;
        m_feedback_right = right;

        return std::make_tuple(out_1, out_2);
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

    float m_feedback_left = 0.f;
    float m_feedback_right = 0.f;

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
