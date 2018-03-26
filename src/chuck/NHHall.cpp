#include "../core/nh_hall.hpp"
#include "chuck_dl.h"
#include "chuck_def.h"

CK_DLL_CTOR(nhhall_ctor);
CK_DLL_DTOR(nhhall_dtor);

CK_DLL_MFUN(nhhall_getRt60);
CK_DLL_MFUN(nhhall_setRt60);
CK_DLL_MFUN(nhhall_getLowFreq);
CK_DLL_MFUN(nhhall_setLowFreq);
CK_DLL_MFUN(nhhall_getLowRatio);
CK_DLL_MFUN(nhhall_setLowRatio);
CK_DLL_MFUN(nhhall_getHiFreq);
CK_DLL_MFUN(nhhall_setHiFreq);
CK_DLL_MFUN(nhhall_getHiRatio);
CK_DLL_MFUN(nhhall_setHiRatio);

CK_DLL_TICKF(nhhall_tickf);

class NHHall {
public:
    // NOTE: The default allocator calls malloc.
    nh_ugens::Unit<> m_core;

    float m_rt60 = 1.0f;
    float m_stereo = 0.5;
    float m_lowFreq = 200;
    float m_lowRatio = 0.5;
    float m_hiFreq = 4000;
    float m_hiRatio = 0.5;

    NHHall(float sample_rate)
    : m_core(sample_rate)
    {
        m_core.set_rt60(m_rt60);
        m_core.set_low_shelf_parameters(m_lowFreq, m_lowRatio);
        m_core.set_hi_shelf_parameters(m_hiFreq, m_hiRatio);
    }
};

t_CKINT nhhall_unit_offset = 0;

CK_DLL_QUERY(NHHall) {
    QUERY->setname(QUERY, "NHHall");

    QUERY->begin_class(QUERY, "NHHall", "UGen");

    QUERY->add_ctor(QUERY, nhhall_ctor);
    QUERY->add_dtor(QUERY, nhhall_dtor);

    QUERY->add_ugen_funcf(QUERY, nhhall_tickf, NULL, 2, 2);

    QUERY->add_mfun(QUERY, nhhall_setRt60, "float", "rt60");
    QUERY->add_arg(QUERY, "float", "arg");
    QUERY->add_mfun(QUERY, nhhall_getRt60, "float", "rt60");

    QUERY->add_mfun(QUERY, nhhall_setLowFreq, "float", "lowFreq");
    QUERY->add_arg(QUERY, "float", "arg");
    QUERY->add_mfun(QUERY, nhhall_getLowFreq, "float", "lowFreq");

    QUERY->add_mfun(QUERY, nhhall_setLowRatio, "float", "lowRatio");
    QUERY->add_arg(QUERY, "float", "arg");
    QUERY->add_mfun(QUERY, nhhall_getLowRatio, "float", "lowRatio");

    QUERY->add_mfun(QUERY, nhhall_setHiFreq, "float", "hiFreq");
    QUERY->add_arg(QUERY, "float", "arg");
    QUERY->add_mfun(QUERY, nhhall_getHiFreq, "float", "hiFreq");

    QUERY->add_mfun(QUERY, nhhall_setHiRatio, "float", "hiRatio");
    QUERY->add_arg(QUERY, "float", "arg");
    QUERY->add_mfun(QUERY, nhhall_getHiRatio, "float", "hiRatio");

    nhhall_unit_offset = QUERY->add_mvar(QUERY, "int", "@data", false);

    QUERY->end_class(QUERY);

    return TRUE;
}

CK_DLL_CTOR(nhhall_ctor) {
    OBJ_MEMBER_INT(SELF, nhhall_unit_offset) = 0;

    NHHall* unit = new NHHall(API->vm->get_srate());

    OBJ_MEMBER_INT(SELF, nhhall_unit_offset) = (t_CKINT)unit;
}

CK_DLL_DTOR(nhhall_dtor) {
    NHHall* unit = (NHHall*)OBJ_MEMBER_INT(SELF, nhhall_unit_offset);
    if (unit) {
        delete unit;
        OBJ_MEMBER_INT(SELF, nhhall_unit_offset) = 0;
        unit = NULL;
    }
}

CK_DLL_TICKF(nhhall_tickf) {
    NHHall* unit = (NHHall*)OBJ_MEMBER_INT(SELF, nhhall_unit_offset);

    for (int i = 0; i < nframes; i++) {
        float in_left = in[i * 2 + 0];
        float in_right = in[i * 2 + 1];
        float out_left;
        float out_right;
        std::array<float, 2> out = unit->m_core.process(in_left, in_right);
        out_left = out[0];
        out_right = out[1];
        out[i * 2 + 0] = out_left;
        out[i * 2 + 1] = out_right;
    }

    return TRUE;
}

CK_DLL_MFUN(nhhall_getRt60) {
    NHHall* unit = (NHHall*)OBJ_MEMBER_INT(SELF, nhhall_unit_offset);
    RETURN->v_float = unit->m_rt60;
}

CK_DLL_MFUN(nhhall_setRt60) {
    NHHall* unit = (NHHall*)OBJ_MEMBER_INT(SELF, nhhall_unit_offset);
    unit->m_rt60 = GET_NEXT_DUR(ARGS);
    unit->m_core.set_rt60(unit->m_rt60);
    RETURN->v_float = unit->m_rt60;
}

CK_DLL_MFUN(nhhall_getLowFreq) {
    NHHall* unit = (NHHall*)OBJ_MEMBER_INT(SELF, nhhall_unit_offset);
    RETURN->v_float = unit->m_lowFreq;
}

CK_DLL_MFUN(nhhall_setLowFreq) {
    NHHall* unit = (NHHall*)OBJ_MEMBER_INT(SELF, nhhall_unit_offset);
    unit->m_lowFreq = GET_NEXT_DUR(ARGS);
    unit->m_core.set_low_shelf_parameters(unit->m_lowFreq, unit->m_lowRatio);
    RETURN->v_float = unit->m_lowFreq;
}

CK_DLL_MFUN(nhhall_getLowRatio) {
    NHHall* unit = (NHHall*)OBJ_MEMBER_INT(SELF, nhhall_unit_offset);
    RETURN->v_float = unit->m_lowRatio;
}

CK_DLL_MFUN(nhhall_setLowRatio) {
    NHHall* unit = (NHHall*)OBJ_MEMBER_INT(SELF, nhhall_unit_offset);
    unit->m_lowRatio = GET_NEXT_DUR(ARGS);
    unit->m_core.set_low_shelf_parameters(unit->m_lowFreq, unit->m_lowRatio);
    RETURN->v_float = unit->m_lowRatio;
}

CK_DLL_MFUN(nhhall_getHiFreq) {
    NHHall* unit = (NHHall*)OBJ_MEMBER_INT(SELF, nhhall_unit_offset);
    RETURN->v_float = unit->m_hiFreq;
}

CK_DLL_MFUN(nhhall_setHiFreq) {
    NHHall* unit = (NHHall*)OBJ_MEMBER_INT(SELF, nhhall_unit_offset);
    unit->m_hiFreq = GET_NEXT_DUR(ARGS);
    unit->m_core.set_hi_shelf_parameters(unit->m_hiFreq, unit->m_hiRatio);
    RETURN->v_float = unit->m_hiFreq;
}

CK_DLL_MFUN(nhhall_getHiRatio) {
    NHHall* unit = (NHHall*)OBJ_MEMBER_INT(SELF, nhhall_unit_offset);
    RETURN->v_float = unit->m_hiRatio;
}

CK_DLL_MFUN(nhhall_setHiRatio) {
    NHHall* unit = (NHHall*)OBJ_MEMBER_INT(SELF, nhhall_unit_offset);
    unit->m_hiRatio = GET_NEXT_DUR(ARGS);
    unit->m_core.set_hi_shelf_parameters(unit->m_hiFreq, unit->m_hiRatio);
    RETURN->v_float = unit->m_hiRatio;
}
