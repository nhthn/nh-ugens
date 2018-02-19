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

class MulAdd {
public:
    const float m_sample_rate;
    const int m_buffer_size;

    float m_mul;
    float m_add;

    MulAdd(
        float sample_rate,
        int buffer_size
    ) :
    m_sample_rate(sample_rate),
    m_buffer_size(buffer_size)
    {
        m_mul = 1.0f;
        m_add = 0.0f;
    }

    // NOTE: This method must be written to permit "in" and "out" to be the
    // same buffer. That is, always read from "in" first and then write to
    // "out".
    void process(const float* in, float* out) {
        for (int i = 0; i < m_buffer_size; i++) {
            out[i] = in[i] * m_mul + m_add;
        }
    }
};

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
    float m_k = 0.95f;

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

class BaseDelay {
public:
    const float m_sample_rate;
    const int m_buffer_size;

    int m_size;
    int m_mask;
    float* m_buffer;
    int m_read_position;
    int m_delay;

    BaseDelay(
        float sample_rate,
        int buffer_size,
        float delay
    ) :
    m_sample_rate(sample_rate),
    m_buffer_size(buffer_size)
    {
        int delay_in_samples = m_sample_rate * delay;
        m_size = next_power_of_two(delay_in_samples);
        m_mask = m_size - 1;

        m_read_position = 0;
        m_delay = delay_in_samples;
    }
};

// Fixed delay line.
class Delay : public BaseDelay {
public:
    Delay(
        float sample_rate,
        int buffer_size,
        float delay
    ) :
    BaseDelay(sample_rate, buffer_size, delay)
    {
    }

    // NOTE: This method must be written to permit "in" and "out" to be the
    // same buffer. Always read from "in" first and then write to "out".
    void process(const float* in, float* out) {
        for (int i = 0; i < m_buffer_size; i++) {
            float out_value = m_buffer[(m_read_position - m_delay) & m_mask];
            m_buffer[m_read_position] = in[i];
            m_read_position = (m_read_position + 1) & m_mask;
            out[i] = out_value;
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
    BaseDelay(sample_rate, buffer_size, delay),
    m_k(k)
    {
    }

    // NOTE: This method must be written to permit "in" and "out" to be the
    // same buffer. Always read from "in" first and then write to "out".
    void process(const float* in, float* out) {
        for (int i = 0; i < m_buffer_size; i++) {
            float delayed_signal = m_buffer[(m_read_position - m_delay) & m_mask];
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
    float m_last_delay;

    VariableAllpass(
        float sample_rate,
        int buffer_size,
        float max_delay,
        float k
    ) :
    BaseDelay(sample_rate, buffer_size, max_delay),
    m_k(k)
    {
    }

    // NOTE: This method must be written to permit either of the inputs to be
    // identical to the output buffer. Always read from inputs first and then
    // write to outputs.
    void process(const float* in, const float* delay, float* out) {
        for (int i = 0; i < m_buffer_size; i++) {
            float position = m_read_position - delay[i] * m_sample_rate;
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

    m_muladd(sample_rate, buffer_size),
    m_dc_blocker(sample_rate, buffer_size),
    m_lfo(sample_rate, buffer_size),
    m_delay(sample_rate, buffer_size, 0.01f),
    m_allpass(sample_rate, buffer_size, 0.05f, 0.5f)

    {
        m_wire_1 = allocate_wire();
        m_wire_2 = allocate_wire();
        m_wire_3 = allocate_wire();

        allocate_delay_line(m_delay);
        allocate_delay_line(m_allpass);

        m_lfo.set_frequency(1.0);

        m_muladd.m_mul = 0.001f;
        m_muladd.m_add = 0.03f;
    }

    ~Unit() {
        m_allocator->deallocate(m_wire_1);
        m_allocator->deallocate(m_wire_2);
        m_allocator->deallocate(m_wire_3);

        m_allocator->deallocate(m_delay.m_buffer);
        m_allocator->deallocate(m_allpass.m_buffer);
    }

    float* allocate_wire(void) {
        void* memory = m_allocator->allocate(sizeof(float) * m_buffer_size);
        return static_cast<float*>(memory);
    }

    void allocate_delay_line(BaseDelay& delay) {
        void* memory = m_allocator->allocate(sizeof(float) * delay.m_size);
        delay.m_buffer = static_cast<float*>(memory);
        memset(delay.m_buffer, 0, sizeof(float) * delay.m_size);
    }

    void copy(float* in, float* out) {
        std::memcpy(out, in, sizeof(float) * m_buffer_size);
    }

    void process(const float* in, float* out_1, float* out_2) {
        float* sound = m_wire_1;
        m_dc_blocker.process(in, sound);

        m_lfo.process(m_wire_2, m_wire_3);
        m_muladd.process(m_wire_2, m_wire_2);

        m_allpass.process(sound, m_wire_2, sound);

        copy(sound, out_1);
        copy(out_1, out_2);
    }

private:
    std::unique_ptr<Alloc> m_allocator;

    // NOTE: When adding a new wire buffer, don't forget to allocate it in the
    // constructor and free it in the destructor.
    float* m_wire_1;
    float* m_wire_2;
    float* m_wire_3;

    DCBlocker m_dc_blocker;
    MulAdd m_muladd;
    SineLFO m_lfo;

    // NOTE: When adding a new delay unit of some kind, don't forget to
    // allocate the memory in the constructor and free it in the destructor.
    Delay m_delay;
    VariableAllpass m_allpass;
};

} // namespace nh_ugens
