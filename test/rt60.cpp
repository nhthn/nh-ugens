#include "../src/core/nh_hall.hpp"
#include <iostream>

void find_rt60(float k, float time) {
    float sample_rate = 48000.0f;
    nh_ugens::Unit<> core(sample_rate);

    core.m_k = k;

    float left, right;
    {
        std::array<float, 2> out = core.process(1.0f, 1.0f);
        left = out[0];
        right = out[1];
    }

    float max_time = 50.0;
    int max_time_in_samples = sample_rate * max_time;
    int time_in_samples = 0.1f * sample_rate;

    int timer = 0;
    for (int i = 0; i < max_time_in_samples; i++) {
        std::array<float, 2> out = core.process(0.0f, 0.0f);
        float left = out[0];
        float right = out[1];
        float power = (left * left + right * right) * 0.5f;
        if (power >= 1e-6f) {
            timer = 0;
        } else {
            timer++;
            if (timer > time_in_samples) {
                float rt60 = (i - time_in_samples) / sample_rate;

                std::cout
                << "k = " << k
                << ", RT60 = "<< rt60 << "s"
                << std::endl;

                std::cout << powf(0.001, 124.0e-3f / rt60) << std::endl;

                return;
            }
        }
    }
    std::cout
    << "k = " << k
    << ", RT60 > "
    << max_time
    << "s"
    << std::endl;
}

int main(int argc, char* argv[]) {
    find_rt60(0.5f, 0.1f);
    find_rt60(0.6f, 0.1f);
    find_rt60(0.7f, 0.1f);
    find_rt60(0.8f, 0.1f);
    find_rt60(0.9f, 0.1f);
    find_rt60(0.91f, 0.1f);
    find_rt60(0.95f, 0.1f);
    find_rt60(0.96f, 0.1f);
    find_rt60(0.99f, 0.1f);

    return 0;
}
