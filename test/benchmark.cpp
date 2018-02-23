#include "../src/core/nh_hall.hpp"
#include "../src/core/nh_hall_unbuffered.hpp"
#include <iostream>
#include <vector>
#include <sys/time.h>

float rfloat(void) {
    return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
}

float bench(int buffer_size) {
    float sample_rate = 48000.0f;

    nh_ugens::Unit<nh_ugens::Allocator> core(
        sample_rate,
        buffer_size,
        std::unique_ptr<nh_ugens::Allocator>(new nh_ugens::Allocator())
    );

    float* in = new float[buffer_size];
    float* out_1 = new float[buffer_size];
    float* out_2 = new float[buffer_size];

    timeval time_before;
    timeval time_after;

    gettimeofday(&time_before, 0);

    int blocks = 60.0f * sample_rate / buffer_size;
    for (int i = 0; i < blocks; i++) {
        for (int i = 0; i < buffer_size; i++) {
            in[i] = rfloat();
        }

        core.process(in, out_1, out_2);
    }
    gettimeofday(&time_after, 0);

    long elapsed_microseconds = (time_after.tv_sec - time_before.tv_sec) * 1000000 + time_after.tv_usec - time_before.tv_usec;
    float elapsed = (float)elapsed_microseconds * 1e-6;

    return elapsed;
}

float bench2(void) {
    float sample_rate = 48000.0f;

    nh_ugens_unbuffered::Unit<nh_ugens_unbuffered::Allocator> core(
        sample_rate,
        1,
        std::unique_ptr<nh_ugens_unbuffered::Allocator>(new nh_ugens_unbuffered::Allocator())
    );

    float out_1 = 0.f;
    float out_2 = 0.f;

    timeval time_before;
    timeval time_after;

    gettimeofday(&time_before, 0);

    int samples = 60.0f * sample_rate;
    for (int i = 0; i < samples; i++) {
        float in = rfloat();
        core.process(in, out_1, out_2);
    }
    gettimeofday(&time_after, 0);

    long elapsed_microseconds = (time_after.tv_sec - time_before.tv_sec) * 1000000 + time_after.tv_usec - time_before.tv_usec;
    float elapsed = (float)elapsed_microseconds * 1e-6;

    return elapsed;
}

int main(void) {

    float elapsed = bench2();
    std::cout << "Unbuffered took " << elapsed << " seconds to render 60s of audio." << std::endl;

    std::vector<int> buffer_sizes = {1, 2, 4, 16, 64, 128, 512, 1024};

    for (int buffer_size : buffer_sizes) {
        float elapsed = bench(buffer_size);
        std::cout << "Buffer size " << buffer_size << " took " << elapsed << " seconds to render 60s of audio." << std::endl;
    }



    return 0;
}
