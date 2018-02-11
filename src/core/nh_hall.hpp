#pragma once
#include <memory>

namespace nh_ugens {

class DCBlocker {
public:
    float m_x1 = 0.0f;
    float m_y1 = 0.0f;
    float m_k = 0.95f;

    DCBlocker(
        float m_sample_rate,
        int m_buffer_size
    ) :
    m_sample_rate(m_sample_rate),
    m_buffer_size(m_buffer_size)
    {
    }

    void process(const float* in, float* out) {
        for (int i = 0; i < m_buffer_size; i++) {
            float x = in[i];
            float y = x - m_x1 + m_y1 * m_k;
            m_y1 = y;
            m_x1 = x;
            out[i] = y;
        }
    }

private:
    const float m_sample_rate;
    const int m_buffer_size;
};

template <class Alloc>
class Unit {
public:
    std::unique_ptr<Alloc> m_allocator;

    Unit(
        float sample_rate,
        int buffer_size,
        std::unique_ptr<Alloc> allocator
    ) :
    m_sample_rate(sample_rate),
    m_buffer_size(buffer_size),
    m_allocator(std::move(allocator)),
    m_dc_blocker(sample_rate, buffer_size)
    {
        //m_wire = (float*)m_allocator.allocate(sizeof(float) * m_buffer_size);
    }

    ~Unit() {
        //m_allocator.deallocate(m_wire);
    }

    void process(const float* in, float* out_1, float* out_2) {
        for (int i = 0; i < m_buffer_size; i++) {
            memcpy(out_1, in, sizeof(float) * m_buffer_size);
            memcpy(out_2, in, sizeof(float) * m_buffer_size);
        }
    }

private:
    //float* m_wire;
    const float m_sample_rate;
    const int m_buffer_size;

    DCBlocker m_dc_blocker;
};

}
