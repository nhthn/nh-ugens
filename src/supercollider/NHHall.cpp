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

#include "SC_PlugIn.hpp"
#include "../core/nh_hall.hpp"

static InterfaceTable* ft;

class real_time_allocation_failed : public std::exception { };

class SCAllocator {
public:
    World* m_world;
    SCAllocator(World* world) : m_world(world) { }

    void* allocate(int memory_size) {
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

private:
    nh_ugens::Unit<SCAllocator> m_core;

    void clear(int inNumSamples) {
        ClearUnitOutputs(this, inNumSamples);
    }

    void next(int inNumSamples) {
        const float* in1 = in(0);
        const float* in2 = in(1);
        float* out1 = out(0);
        float* out2 = out(1);

        m_core.process(in1, out1, out2);
    }
};

PluginLoad(NHUGens) {
    ft = inTable;
    registerUnit<NHHall>(ft, "NHHall");
}
