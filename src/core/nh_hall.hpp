#pragma once
#include <cstring> // for memset
#include <memory> // std::unique_ptr
#include <tuple> // std::tuple
#include <cmath> // cosf/sinf

/*

TODO:

- Improve sound of early reflections
- Adjust parameters to fix undulation in reverb tail

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

static inline std::tuple<float, float> rotate(float x1, float x2, float angle) {
    return std::make_tuple(
        cosf(angle) * x1 - sinf(angle) * x2,
        sinf(angle) * x1 + cosf(angle) * x2
    );
}

constexpr float twopi = 6.283185307179586f;

class Allocator {
public:
    void* allocate(int memory_size) {
        return malloc(memory_size);
    }

    void deallocate(void* memory) {
        free(memory);
    }
};

class SineLFO {
public:
    const float m_sample_rate;
    float m_k;
    float m_cosine;
    float m_sine;

    SineLFO(
        float sample_rate
    ) :
    m_sample_rate(sample_rate)
    {
        m_cosine = 1.0;
        m_sine = 0.0;
    }

    void set_frequency(float frequency) {
        m_k = twopi * frequency / m_sample_rate;
    }

    std::tuple<float, float> process(void) {
        m_cosine -= m_k * m_sine;
        m_sine += m_k * m_cosine;
        return std::make_tuple(m_cosine, m_sine);
    }
};

class DCBlocker {
public:
    const float m_sample_rate;

    float m_x1 = 0.0f;
    float m_y1 = 0.0f;
    float m_k = 0.99f;

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
};

class HiShelf {
public:
    const float m_sample_rate;

    float m_x1 = 0.0f;
    // TODO: Make this sample-rate invariant
    float m_k = 0.3f;

    HiShelf(
        float sample_rate
    ) :
    m_sample_rate(sample_rate)
    {
    }

    float process(float in) {
        float x = in;
        float out = (1 - m_k) * x + m_k * m_x1;
        m_x1 = x;
        return out;
    }
};

class BaseDelay {
public:
    const float m_sample_rate;

    int m_size;
    int m_mask;
    float* m_buffer;
    int m_read_position;

    float m_delay;
    int m_delay_in_samples;

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
        int position = m_read_position - delay_in_samples;
        float out = gain * m_buffer[position & m_mask];
        return out;
    }
};

// Fixed Schroeder allpass.
class Allpass : public BaseDelay {
public:
    float m_k;

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
};

// Schroeder allpass with variable delay and cubic interpolation.
class VariableAllpass : public BaseDelay {
public:
    float m_k;

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
};

template <class Alloc = Allocator>
class Unit {
public:
    const float m_sample_rate;

    Unit(
        float sample_rate,
        std::unique_ptr<Alloc> allocator
    ) :
    m_sample_rate(sample_rate),
    m_allocator(std::move(allocator)),

    m_lfo(sample_rate),
    m_dc_blocker(sample_rate),
    m_hi_shelf_1(sample_rate),
    m_hi_shelf_2(sample_rate),

    m_early_allpass_1(sample_rate, 14.5e-3f, 0.5f),
    m_early_allpass_2(sample_rate, 13.0e-3f, 0.5f),
    m_early_allpass_3(sample_rate, 7.8e-3f, 0.5f),
    m_early_allpass_4(sample_rate, 6.2e-3f, 0.5f),
    m_early_delay_1(sample_rate, 7.45e-3),
    m_early_delay_2(sample_rate, 5.25e-3),
    m_early_delay_3(sample_rate, 2.36e-3),
    m_early_delay_4(sample_rate, 3.17e-3),

    // TODO: Maximum delays for variable allpasses are temporary.
    m_allpass_1(sample_rate, 100e-3f, 25.6e-3f, -0.6f),
    m_allpass_2(sample_rate, 31.4e-3f, 0.6f),
    m_delay_1(sample_rate, 180.6e-3f),
    m_allpass_3(sample_rate, 120e-3f, 40.7e-3f, 0.6f),
    m_allpass_4(sample_rate, 35.6e-3f, -0.6f),
    m_delay_2(sample_rate, 90.3e-3f),

    m_allpass_5(sample_rate, 38.6e-3f, 0.6f),
    m_allpass_6(sample_rate, 20.4e-3f, -0.6f),
    m_delay_3(sample_rate, 150.6e-3f),
    m_allpass_7(sample_rate, 20.7e-3f, 0.6f),
    m_allpass_8(sample_rate, 23.6e-3f, -0.6f),
    m_delay_4(sample_rate, 60.6e-3f)

    {
        m_feedback = 0.f;

        allocate_delay_line(m_early_allpass_1);
        allocate_delay_line(m_early_allpass_2);
        allocate_delay_line(m_early_allpass_3);
        allocate_delay_line(m_early_allpass_4);
        allocate_delay_line(m_early_delay_1);
        allocate_delay_line(m_early_delay_2);
        allocate_delay_line(m_early_delay_3);
        allocate_delay_line(m_early_delay_4);

        allocate_delay_line(m_allpass_1);
        allocate_delay_line(m_allpass_2);
        allocate_delay_line(m_allpass_3);
        allocate_delay_line(m_allpass_4);
        allocate_delay_line(m_allpass_5);
        allocate_delay_line(m_allpass_6);
        allocate_delay_line(m_allpass_7);
        allocate_delay_line(m_allpass_8);
        allocate_delay_line(m_delay_1);
        allocate_delay_line(m_delay_2);
        allocate_delay_line(m_delay_3);
        allocate_delay_line(m_delay_4);
    }

    Unit(
        float sample_rate
    ) :
    Unit(sample_rate, std::unique_ptr<Alloc>(new Alloc()))
    { }

    ~Unit() {
        free_delay_line(m_early_allpass_1);
        free_delay_line(m_early_allpass_2);
        free_delay_line(m_early_allpass_3);
        free_delay_line(m_early_allpass_4);
        free_delay_line(m_early_delay_1);
        free_delay_line(m_early_delay_2);
        free_delay_line(m_early_delay_3);
        free_delay_line(m_early_delay_4);

        free_delay_line(m_allpass_1);
        free_delay_line(m_allpass_2);
        free_delay_line(m_allpass_3);
        free_delay_line(m_allpass_4);
        free_delay_line(m_allpass_5);
        free_delay_line(m_allpass_6);
        free_delay_line(m_allpass_7);
        free_delay_line(m_allpass_8);
        free_delay_line(m_delay_1);
        free_delay_line(m_delay_2);
        free_delay_line(m_delay_3);
        free_delay_line(m_delay_4);
    }

    void allocate_delay_line(BaseDelay& delay) {
        void* memory = m_allocator->allocate(sizeof(float) * delay.m_size);
        delay.m_buffer = static_cast<float*>(memory);
        memset(delay.m_buffer, 0, sizeof(float) * delay.m_size);
    }

    void free_delay_line(BaseDelay& delay) {
        m_allocator->deallocate(delay.m_buffer);
    }

    std::tuple<float, float> process(float in_1, float in_2) {
        float k = 0.9f;

        // LFO
        float lfo_1;
        float lfo_2;

        m_lfo.set_frequency(0.5f);
        std::tie(lfo_1, lfo_2) = m_lfo.process();

        lfo_1 *= 0.32e-3f;
        lfo_2 *= -0.45e-3f;

        // Sound signal path
        float left = in_1;
        float right = in_2;

        float early_left = 0.f;
        float early_right = 0.f;

        left = m_early_allpass_1.process(left);
        right = m_early_allpass_2.process(right);
        std::tie(left, right) = rotate(left, right, 0.2f);
        early_left = left;
        early_right = right;

        left = m_early_delay_1.process(left);
        left = m_early_allpass_3.process(left);
        right = m_early_delay_2.process(right);
        right = m_early_allpass_4.process(right);
        std::tie(left, right) = rotate(left, right, 0.8f);
        early_left += left;
        early_right += right;

        float sound = 0.f;

        sound += m_feedback * 0.5f;
        sound = m_dc_blocker.process(sound);

        sound += early_left;
        sound = m_allpass_1.process(sound, lfo_1);
        sound = m_allpass_2.process(sound);
        sound = m_delay_2.process(sound);
        sound = m_hi_shelf_1.process(sound);
        sound *= k;

        sound += early_right;
        sound = m_allpass_3.process(sound, lfo_2);
        sound = m_allpass_4.process(sound);
        sound = m_delay_2.process(sound);
        sound = m_hi_shelf_2.process(sound);
        sound *= k;

        sound += early_left;
        sound = m_allpass_5.process(sound);
        sound = m_allpass_6.process(sound);
        sound = m_delay_3.process(sound);
        sound *= k;

        sound += early_right;
        sound = m_allpass_7.process(sound);
        sound = m_allpass_8.process(sound);
        sound = m_delay_4.process(sound);
        sound *= k;

        m_feedback = sound;

        // Keep the inter-channel delays somewhere between 0.1 and 0.7 ms --
        // this allows the Haas effect to come in.

        float out_1 = early_left;
        float out_2 = early_right;

        out_1 += m_delay_1.tap(0.0e-3f, 1.0f);
        out_2 += m_delay_1.tap(0.1e-3f, 0.8f);

        out_1 += m_delay_2.tap(0.5e-3f, 0.8f);
        out_2 += m_delay_2.tap(0.0e-3f, 1.0f);

        out_1 += m_delay_3.tap(0.0e-3f, 1.0f);
        out_2 += m_delay_3.tap(0.7e-3f, 0.8f);

        out_1 += m_delay_4.tap(0.2e-3f, 0.8f);
        out_2 += m_delay_4.tap(0.0e-3f, 1.0f);

        return std::make_tuple(out_1, out_2);
    }

private:
    std::unique_ptr<Alloc> m_allocator;

    float m_feedback = 0.f;

    SineLFO m_lfo;
    DCBlocker m_dc_blocker;

    HiShelf m_hi_shelf_1;
    HiShelf m_hi_shelf_2;

    // NOTE: When adding a new delay unit of some kind, don't forget to
    // allocate the memory in the constructor and free it in the destructor.
    Allpass m_early_allpass_1;
    Allpass m_early_allpass_2;
    Allpass m_early_allpass_3;
    Allpass m_early_allpass_4;
    Delay m_early_delay_1;
    Delay m_early_delay_2;
    Delay m_early_delay_3;
    Delay m_early_delay_4;

    VariableAllpass m_allpass_1;
    Allpass m_allpass_2;
    Delay m_delay_1;

    VariableAllpass m_allpass_3;
    Allpass m_allpass_4;
    Delay m_delay_2;

    Allpass m_allpass_5;
    Allpass m_allpass_6;
    Delay m_delay_3;

    Allpass m_allpass_7;
    Allpass m_allpass_8;
    Delay m_delay_4;
};

} // namespace nh_ugens
