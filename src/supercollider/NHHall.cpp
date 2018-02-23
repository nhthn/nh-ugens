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

#include "../core/nh_hall_unbuffered.hpp"
#include "SC_PlugIn.hpp"

static InterfaceTable* ft;

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
        bufferSize(),
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
        // The reason we are overriding this method is to remove this line:
        // (mCalcFunc)(this, 1);
        // Our particular calc function always expects inNumSamples to be equal
        // to the buffer size. In the worst case, if inNumSamples is less
        // than the buffer length that the core DSP class expects, the core
        // DSP class will start accessing garbage memory.

        // This could be fixed by adding a guard in the calc function, but for
        // efficiency it's better to just never call the calc function
        // improperly in the first place. Instead, we just output 0 as the
        // initial sample.
        clear(1);
    }

private:
    nh_ugens_unbuffered::Unit<SCAllocator> m_core;

    void clear(int inNumSamples) {
        ClearUnitOutputs(this, inNumSamples);
    }

    void next(int inNumSamples) {
        const float* in1 = in(0);
        //const float* in2 = in(1);
        float* out1 = out(0);
        float* out2 = out(1);

        // Safety guard in case scsynth gives us a buffer size incompatible
        // with the core DSP.

        // I don't believe this is necessary since there aren't really any
        // conditions where scsynth calls the calc function with
        // inNumSamples != bufferSize.

        // if (inNumSamples != bufferSize()) {
        //     clear(inNumSamples);
        // }

        for (int i = 0; i < inNumSamples; i++) {
            m_core.process(in1[i], out1[i], out2[i]);
        }
    }
};

PluginLoad(NHUGens) {
    ft = inTable;
    registerUnit<NHHall>(ft, "NHHall");
}
