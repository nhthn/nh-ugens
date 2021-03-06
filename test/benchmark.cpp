#include "../src/core/nh_hall.hpp"
#include <iostream>
#include <vector>
#include <sys/time.h>

float rfloat(void) {
    return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
}

float bench(void) {
    float sample_rate = 48000.0f;

    nh_ugens::NHHall<> core(sample_rate);

    timeval time_before;
    timeval time_after;

    gettimeofday(&time_before, 0);

    int samples = 120.0f * sample_rate;
    for (int i = 0; i < samples; i++) {
        float out_1;
        float out_2;
        std::array<float, 2> out = core.process(rfloat(), rfloat());
        out_1 = out[0];
        out_2 = out[1];
    }
    gettimeofday(&time_after, 0);

    long elapsed_microseconds = (time_after.tv_sec - time_before.tv_sec) * 1000000 + time_after.tv_usec - time_before.tv_usec;
    float elapsed = (float)elapsed_microseconds * 1e-6;

    return elapsed;
}

int main(void) {

    float elapsed = bench();
    std::cout << "Took " << elapsed << " seconds to render 60s of audio." << std::endl;

    return 0;
}
