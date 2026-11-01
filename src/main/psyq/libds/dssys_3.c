/* Psy-Q LIBDS DSSYS_3.OBJ: DsMix, DsGetSector, DsGetSector2, DsDataSync, DsIntToPos, DsPosToInt, DsSetDebug, DsLastPos. */

#include "pe1/cdrom.h"
#include "pe1/psyq_cd.h"

int DsMix(CdlATV *volume) {
    CD_vol(volume);
    return 1;
}

int DsGetSector(void *address, int size) {
    /* At most one 2352-byte raw image sector; reject wrapped DMA lengths. */
    if (size < 0 || size > 2352 / 4) return 0;
    return CD_getsector(address, size) == 0;
}

int DsGetSector2(void *address, int size) {
    /* Both DMA variants read the same mounted-image sector buffer on PC. */
    return DsGetSector(address, size);
}

int DsDataSync(int mode) {
    /* Image-backed sector copies are synchronous; no hardware DMA is pending. */
    (void)mode;
    return 0;
}

static inline int ENCODE_BCD(int n) { return ((n / 10) << 4) + (n % 10); }

CdlLOC *DsIntToPos(int i, CdlLOC *p) {
    i += 150;
    p->sector = ENCODE_BCD(i % 75);
    p->second = ENCODE_BCD(i / 75 % 60);
    p->minute = ENCODE_BCD(i / 75 / 60);
    return p;
}

int DsPosToInt(CdlLOC *p) {
#define DECODE_BCD(x) (((x) >> 4) * 10 + ((x) & 0xF))
    u_char sector = p->sector;
    u_char second = p->second;
    u_char minute = p->minute;

    return (DECODE_BCD(minute) * 60 + DECODE_BCD(second)) * 75 +
           DECODE_BCD(sector) - 150;
}

int DsSetDebug(int level) {
    int old = CD_debug;
    CD_debug = level;
    return old;
}

CdlLOC *DsLastPos(CdlLOC *dst) {
    static CdlLOC position;
    u8 response[8] = {0};
    /* Query the mounted-image/XA cursor instead of the removed drive state. */
    if (CdControlB(16, NULL, response) > 0) {
        position.minute = response[0];
        position.second = response[1];
        position.sector = response[2];
        position.track = response[3];
    }
    if (dst) {
        *dst = position;
        return dst;
    }
    return &position;
}
