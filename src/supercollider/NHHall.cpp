/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org>
*/

#include "../core/nh_hall.hpp"
#include "SC_PlugIn.hpp"

static InterfaceTable* ft;

// FIXME: throwing exceptions is not real-time safe!!!
class real_time_allocation_failed : public std::exception { };

class SCAllocator {
public:
    World* m_world;
    SCAllocator(World* world) : m_world(world) { }

    void* ASSUME_ALIGNED(64) allocate(int memory_size) {
        void* memory = RTAlloc(m_world, memory_size);
        if (memory == nullptr) {
            throw real_time_allocation_failed();
        }
        return memory;
    }

    void deallocate(void* memory) {
        RTFree(m_world, memory);
    }
};

struct NHHall : public SCUnit {
public:
    NHHall() try :
    m_core(
        sampleRate(),
        std::unique_ptr<SCAllocator>(new SCAllocator(mWorld))
    )
    {
        set_calc_function<NHHall, &NHHall::next>();
    }
    catch (const real_time_allocation_failed& ex) {
        printf("Could not allocate real-time memory for NHHall\n");
        set_calc_function<NHHall, &NHHall::clear>();
    }

    template <typename UnitType, void (UnitType::*PointerToMember)(int)>
    void set_calc_function(void)
    {
        mCalcFunc = make_calc_function<UnitType, PointerToMember>();
        clear(1);
        m_last_k = in0(2);
    }

private:
    nh_ugens::Unit<SCAllocator> m_core;
    float m_last_k;

    void clear(int inNumSamples) {
        ClearUnitOutputs(this, inNumSamples);
    }

    void next(int inNumSamples) {
        const float* in_left = in(0);
        const float* in_right = in(1);
        const float rt60 = in0(2);
        float* out_left = out(0);
        float* out_right = out(1);

        float new_k = m_core.compute_k_from_rt60(rt60);
        float k = m_last_k;
        float k_ramp = (k - m_last_k) / inNumSamples;
        for (int i = 0; i < inNumSamples; i++) {
            k += k_ramp;
            m_core.m_k = k;
            std::tie(out_left[i], out_right[i]) = m_core.process(in_left[i], in_right[i]);
        }
        m_last_k = new_k;
    }
};

PluginLoad(NHUGens) {
    ft = inTable;
    registerUnit<NHHall>(ft, "NHHall");
}
