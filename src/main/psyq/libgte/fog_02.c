/* PSY-Q LIBGTE FOG_02: SetFogNearFar. */

#include "pe1/gte_types.h"

void SetFogNearFar(long near, long far, long h) {
    int range;
    int depth_scale;
    int dqa;
    int dqb_base;
    int dqb;
    int h_reg;
    range = (s32)((u32)far - (u32)near);
    h_reg = h;
    if (range < 100 || h_reg == 0) {
        return;
    }

    depth_scale = (s32)(0u - (u32)near * (u32)far) / range;
    dqb_base = (s32)((u32)far << 12) / range;
    dqa = (s32)((s64)(s32)((u32)depth_scale << 8) / h_reg);
    dqb = (s32)((u32)dqb_base << 12);

    if (dqa < -0x8000) {
        dqa = -0x8000;
    }
    if (dqa > 0x7FFF) {
        dqa = 0x7FFF;
    }

    SetDQA(dqa);
    SetDQB(dqb);
}
