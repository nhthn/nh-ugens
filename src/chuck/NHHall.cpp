#include "../core/nh_hall.hpp"
#include "chuck_dl.h"
#include "chuck_def.h"

CK_DLL_CTOR(nhhall_ctor);
CK_DLL_DTOR(nhhall_dtor);
CK_DLL_TICKF(nhhall_tickf);

typedef nh_ugens::Unit<> NHHallCore;

t_CKINT nhhall_core_offset = 0;

CK_DLL_QUERY(NHHall) {
    QUERY->setname(QUERY, "NHHall");

    QUERY->begin_class(QUERY, "NHHall", "UGen");

    QUERY->add_ctor(QUERY, nhhall_ctor);
    QUERY->add_dtor(QUERY, nhhall_dtor);

    QUERY->add_ugen_funcf(QUERY, nhhall_tickf, NULL, 1, 2);

    //QUERY->add_mfun(QUERY, nhhall_setFreq, "float", "freq");
    //QUERY->add_arg(QUERY, "float", "arg");

    //QUERY->add_mfun(QUERY, nhhall_getFreq, "float", "freq");

    nhhall_core_offset = QUERY->add_mvar(QUERY, "int", "@data", false);

    QUERY->end_class(QUERY);

    return TRUE;
}

CK_DLL_CTOR(nhhall_ctor) {
    OBJ_MEMBER_INT(SELF, nhhall_core_offset) = 0;

    NHHallCore* core = new NHHallCore(API->vm->get_srate());

    OBJ_MEMBER_INT(SELF, nhhall_core_offset) = (t_CKINT)core;
}

CK_DLL_DTOR(nhhall_dtor) {
    NHHallCore* core = (NHHallCore*)OBJ_MEMBER_INT(SELF, nhhall_core_offset);
    if (core) {
        delete core;
        OBJ_MEMBER_INT(SELF, nhhall_core_offset) = 0;
        core = NULL;
    }
}

CK_DLL_TICKF(nhhall_tickf) {
    NHHallCore* core = (NHHallCore*)OBJ_MEMBER_INT(SELF, nhhall_core_offset);

    for (int i = 0; i < nframes; i++) {
        float in_sample = in[i];
        float out_left;
        float out_right;
        std::tie(out_left, out_right) = core->process(in_sample);
        out[i * 2 + 0] = out_left;
        out[i * 2 + 1] = out_right;
    }

    return TRUE;
}
