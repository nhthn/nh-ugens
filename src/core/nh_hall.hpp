#pragma once
#include <cstring>
#include <memory>

namespace nh_ugens {

static inline int next_power_of_two(int x) {
    int result = 1;
    while (result < x) {
        result *= 2;
    }
    return result;
}

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

class FixedDelayLine {
public:
    const float m_sample_rate;
    const int m_buffer_size;

    int m_size;
    int m_mask;
    float* m_buffer;
    int m_read_position;
    int m_delay;

    FixedDelayLine(
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
    m_dc_blocker(sample_rate, buffer_size),
    m_delay_line(sample_rate, buffer_size, 0.01f)
    {
        m_wire = (float*)m_allocator->allocate(sizeof(float) * m_buffer_size);

        m_delay_line.m_buffer = (float*)m_allocator->allocate(sizeof(float) * m_delay_line.m_size);
        memset(m_delay_line.m_buffer, 0, sizeof(float) * m_delay_line.m_size);
    }

    ~Unit() {
        m_allocator->deallocate(m_wire);
        m_allocator->deallocate(m_delay_line.m_buffer);
    }

    void process(const float* in, float* out_1, float* out_2) {
        //m_dc_blocker.process(m_wire, m_wire);
        m_delay_line.process(in, out_1);
        std::memcpy(out_2, out_1, sizeof(float) * m_buffer_size);
    }

private:
    std::unique_ptr<Alloc> m_allocator;
    float* m_wire;
    DCBlocker m_dc_blocker;
    FixedDelayLine m_delay_line;
};

} // namespace nh_ugens
