#pragma once
#include <cstring>
#include <memory>

/*

CURRENT STATUS

- Need to implement LPF and attenuation

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

constexpr float twopi = 6.283185307179586f;

class SineLFO {
public:
    const float m_sample_rate;
    const int m_buffer_size;
    float m_k;
    float m_cosine;
    float m_sine;

    SineLFO(
        float sample_rate,
        int buffer_size
    ) :
    m_sample_rate(sample_rate),
    m_buffer_size(buffer_size)
    {
        m_cosine = 1.0;
        m_sine = 0.0;
    }

    void set_frequency(float frequency) {
        m_k = twopi * frequency / m_sample_rate;
    }

    void process(float* cosine, float* sine) {
        for (int i = 0; i < m_buffer_size; i++) {
            m_cosine -= m_k * m_sine;
            m_sine += m_k * m_cosine;
            cosine[i] = m_cosine;
            sine[i] = m_sine;
        }
    }
};

class DCBlocker {
public:
    const float m_sample_rate;
    const int m_buffer_size;

    float m_x1 = 0.0f;
    float m_y1 = 0.0f;
    float m_k = 0.99f;

    DCBlocker(
        float sample_rate,
        int buffer_size
    ) :
    m_sample_rate(sample_rate),
    m_buffer_size(buffer_size)
    {
    }

    // NOTE: This method must be written to permit "in" and "out" to be the
    // same buffer. That is, always read from "in" first and then write to
    // "out".
    void process(const float* in, float* out) {
        for (int i = 0; i < m_buffer_size; i++) {
            float x = in[i];
            float y = x - m_x1 + m_k * m_y1;
            out[i] = y;
            m_x1 = x;
            m_y1 = y;
        }
    }
};

class HiShelf {
public:
    const float m_sample_rate;
    const int m_buffer_size;

    float m_x1 = 0.0f;
    // TODO: Make this sample-rate invariant
    float m_k = 0.3f;

    HiShelf(
        float sample_rate,
        int buffer_size
    ) :
    m_sample_rate(sample_rate),
    m_buffer_size(buffer_size)
    {
    }

    // NOTE: This method must be written to permit "in" and "out" to be the
    // same buffer. That is, always read from "in" first and then write to
    // "out".
    void process(const float* in, float* out) {
        for (int i = 0; i < m_buffer_size; i++) {
            float x = in[i];
            out[i] = (1 - m_k) * x + m_k * m_x1;
            m_x1 = x;
        }
    }
};

class BaseDelay {
public:
    const float m_sample_rate;
    const int m_buffer_size;

    int m_size;
    int m_mask;
    float* m_buffer;
    int m_read_position;

    float m_delay;
    int m_delay_in_samples;

    BaseDelay(
        float sample_rate,
        int buffer_size,
        float max_delay,
        float delay
    ) :
    m_sample_rate(sample_rate),
    m_buffer_size(buffer_size)
    {
        int max_delay_in_samples = m_sample_rate * max_delay;
        m_size = next_power_of_two(max_delay_in_samples);
        m_mask = m_size - 1;

        m_read_position = 0;

        m_delay = delay;
        m_delay_in_samples = m_sample_rate * delay;
    }
};

// Fixed delay line. Not used.
class Delay : public BaseDelay {
public:
    Delay(
        float sample_rate,
        int buffer_size,
        float delay
    ) :
    BaseDelay(sample_rate, buffer_size, delay, delay)
    {
    }

    // NOTE: This method must be written to permit "in" and "out" to be the
    // same buffer. Always read from "in" first and then write to "out".
    void process(const float* in, float* out) {
        for (int i = 0; i < m_buffer_size; i++) {
            float out_value = m_buffer[(m_read_position - m_delay_in_samples) & m_mask];
            m_buffer[m_read_position] = in[i];
            m_read_position = (m_read_position + 1) & m_mask;
            out[i] = out_value;
        }
    }

    void tap(float delay, float gain, float* out) {
        int delay_in_samples = delay * m_sample_rate;
        for (int i = 0; i < m_buffer_size; i++) {
            int position = m_read_position - m_buffer_size - delay_in_samples + i;
            out[i] += gain * m_buffer[position & m_mask];
        }
    }
};

// Fixed Schroeder allpass.
class Allpass : public BaseDelay {
public:
    float m_k;

    Allpass(
        float sample_rate,
        int buffer_size,
        float delay,
        float k
    ) :
    BaseDelay(sample_rate, buffer_size, delay, delay),
    m_k(k)
    {
    }

    // NOTE: This method must be written to permit "in" and "out" to be the
    // same buffer. Always read from "in" first and then write to "out".
    void process(const float* in, float* out) {
        for (int i = 0; i < m_buffer_size; i++) {
            float delayed_signal = m_buffer[(m_read_position - m_delay_in_samples) & m_mask];
            float feedback_plus_input =
                in[i] + delayed_signal * m_k;
            m_buffer[m_read_position] = feedback_plus_input;
            m_read_position = (m_read_position + 1) & m_mask;
            out[i] = feedback_plus_input * -m_k + delayed_signal;
        }
    }
};

// Schroeder allpass with variable delay and cubic interpolation.
class VariableAllpass : public BaseDelay {
public:
    float m_k;

    VariableAllpass(
        float sample_rate,
        int buffer_size,
        float max_delay,
        float delay,
        float k
    ) :
    BaseDelay(sample_rate, buffer_size, max_delay, delay),
    m_k(k)
    {
    }

    // NOTE: This method must be written to permit either of the inputs to be
    // identical to the output buffer. Always read from inputs first and then
    // write to outputs.
    void process(const float* in, const float* offset, float* out) {
        for (int i = 0; i < m_buffer_size; i++) {
            float position = m_read_position - (m_delay + offset[i]) * m_sample_rate;
            int iposition = position;
            float position_frac = position - iposition;

            float y0 = m_buffer[iposition & m_mask];
            float y1 = m_buffer[(iposition + 1) & m_mask];
            float y2 = m_buffer[(iposition + 2) & m_mask];
            float y3 = m_buffer[(iposition + 3) & m_mask];

            float delayed_signal = interpolate_cubic(position_frac, y0, y1, y2, y3);

            float feedback_plus_input =
                in[i] + delayed_signal * m_k;
            m_buffer[m_read_position] = feedback_plus_input;
            m_read_position = (m_read_position + 1) & m_mask;
            out[i] = feedback_plus_input * -m_k + delayed_signal;
        }
    }
};

template <class Alloc>
class Unit {
public:
    const float m_sample_rate;
    const int m_buffer_size;

    Unit(
        float sample_rate,
        int buffer_size,
        std::unique_ptr<Alloc> allocator
    ) :
    m_sample_rate(sample_rate),
    m_buffer_size(buffer_size),
    m_allocator(std::move(allocator)),

    m_lfo(sample_rate, buffer_size),
    m_dc_blocker(sample_rate, buffer_size),
    m_hi_shelf_1(sample_rate, buffer_size),
    m_hi_shelf_2(sample_rate, buffer_size),

    m_early_allpass_1(sample_rate, buffer_size, 3.5e-3f, 0.725f),
    m_early_allpass_2(sample_rate, buffer_size, 5.0e-3f, 0.633f),
    m_early_allpass_3(sample_rate, buffer_size, 8.5e-3f, 0.814f),
    m_early_allpass_4(sample_rate, buffer_size, 10.2e-3f, 0.611f),

    // TODO: Maximum delays for variable allpasses are temporary.
    m_allpass_1(sample_rate, buffer_size, 100e-3f, 20.6e-3f, 0.55f),
    m_delay_1(sample_rate, buffer_size, 6.3e-3f),
    m_allpass_2(sample_rate, buffer_size, 31.4e-3f, 0.63f),
    m_delay_2(sample_rate, buffer_size, 120.6e-3f),
    m_allpass_3(sample_rate, buffer_size, 100e-3f, 40.7e-3f, 0.55f),
    m_delay_3(sample_rate, buffer_size, 8.2e-3f),
    m_allpass_4(sample_rate, buffer_size, 61.6e-3f, 0.63f),
    m_delay_4(sample_rate, buffer_size, 180.3e-3f)

    {
        m_feedback = allocate_wire();
        zero_wire(m_feedback);

        m_wire_1 = allocate_wire();
        m_wire_2 = allocate_wire();
        m_wire_3 = allocate_wire();

        allocate_delay_line(m_early_allpass_1);
        allocate_delay_line(m_early_allpass_2);
        allocate_delay_line(m_early_allpass_3);
        allocate_delay_line(m_early_allpass_4);

        allocate_delay_line(m_allpass_1);
        allocate_delay_line(m_delay_1);
        allocate_delay_line(m_allpass_2);
        allocate_delay_line(m_delay_2);
        allocate_delay_line(m_allpass_3);
        allocate_delay_line(m_delay_3);
        allocate_delay_line(m_allpass_4);
        allocate_delay_line(m_delay_4);
    }

    ~Unit() {
        m_allocator->deallocate(m_feedback);
        m_allocator->deallocate(m_wire_1);
        m_allocator->deallocate(m_wire_2);
        m_allocator->deallocate(m_wire_3);

        free_delay_line(m_early_allpass_1);
        free_delay_line(m_early_allpass_2);
        free_delay_line(m_early_allpass_3);
        free_delay_line(m_early_allpass_4);

        free_delay_line(m_allpass_1);
        free_delay_line(m_delay_1);
        free_delay_line(m_allpass_2);
        free_delay_line(m_delay_2);
        free_delay_line(m_allpass_3);
        free_delay_line(m_delay_3);
        free_delay_line(m_allpass_4);
        free_delay_line(m_delay_4);
    }

    float* allocate_wire(void) {
        void* memory = m_allocator->allocate(sizeof(float) * m_buffer_size);
        return static_cast<float*>(memory);
    }

    void zero_wire(float* wire) {
        memset(wire, 0, sizeof(float) * m_buffer_size);
    }

    void allocate_delay_line(BaseDelay& delay) {
        void* memory = m_allocator->allocate(sizeof(float) * delay.m_size);
        delay.m_buffer = static_cast<float*>(memory);
        memset(delay.m_buffer, 0, sizeof(float) * delay.m_size);
    }

    void free_delay_line(BaseDelay& delay) {
        m_allocator->deallocate(delay.m_buffer);
    }

    void copy(float* in, float* out) {
        std::memcpy(out, in, sizeof(float) * m_buffer_size);
    }

    void multiply(float* in, float k, float* out) {
        for (int i = 0; i < m_buffer_size; i++) {
            out[i] = in[i] * k;
        }
    }

    void add(float* in_1, float* in_2, float* out) {
        for (int i = 0; i < m_buffer_size; i++) {
            out[i] = in_1[i] + in_2[i];
        }
    }

    void process(const float* in, float* out_1, float* out_2) {
        // LFO
        float* lfo_1 = m_wire_2;
        float* lfo_2 = m_wire_3;

        m_lfo.set_frequency(0.5f);
        m_lfo.process(lfo_1, lfo_2);

        multiply(lfo_1, 0.32e-3f, lfo_1);
        multiply(lfo_2, -0.45e-3f, lfo_2);

        // Sound signal path
        float* sound = m_wire_1;

        m_early_allpass_1.process(in, sound);
        m_early_allpass_2.process(sound, sound);
        m_early_allpass_3.process(sound, sound);
        m_early_allpass_4.process(sound, sound);

        add(m_feedback, sound, sound);

        m_dc_blocker.process(sound, sound);

        m_allpass_1.process(sound, lfo_1, sound);
        m_delay_1.process(sound, sound);
        m_allpass_2.process(sound, sound);
        m_delay_2.process(sound, sound);

        m_hi_shelf_1.process(sound, sound);
        multiply(sound, 0.9f, sound);

        m_allpass_3.process(sound, lfo_2, sound);
        m_delay_3.process(sound, sound);
        m_allpass_4.process(sound, sound);
        m_delay_4.process(sound, sound);

        m_hi_shelf_2.process(sound, sound);
        multiply(sound, 0.9f, sound);
        copy(sound, m_feedback);

        // Output taps

        zero_wire(out_1);
        zero_wire(out_2);

        m_delay_1.tap(0.123e-3f, 1.0f, out_1);
        m_delay_1.tap(0.750e-3f, 0.8f, out_2);

        m_delay_2.tap(0.113e-3f, 0.8f, out_1);
        m_delay_2.tap(0.212e-3f, 1.0f, out_2);

        m_delay_3.tap(0.538e-3f, 0.8f, out_1);
        m_delay_3.tap(0.169e-3f, 1.0f, out_2);

        m_delay_4.tap(0.25e-3f, 0.8f, out_1);
        m_delay_4.tap(0.131e-3f, 1.0f, out_2);
    }

private:
    std::unique_ptr<Alloc> m_allocator;

    // NOTE: When adding a new wire buffer, don't forget to allocate it in the
    // constructor and free it in the destructor.
    float* m_feedback;
    float* m_wire_1;
    float* m_wire_2;
    float* m_wire_3;

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

    VariableAllpass m_allpass_1;
    Delay m_delay_1;
    Allpass m_allpass_2;
    Delay m_delay_2;

    VariableAllpass m_allpass_3;
    Delay m_delay_3;
    Allpass m_allpass_4;
    Delay m_delay_4;
};

} // namespace nh_ugens
