/* Reading .pelang language packs: a 64-byte header, a key table sorted by hash, then the text.
 * Every offset is checked once at load, so lookups can trust the table. */
#include "pe_lang_pack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HEADER_SIZE 64u
#define ENTRY_SIZE 8u

static uint32_t rd32(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

uint32_t pe_lang_hash(const char *key) {
    uint32_t h = 0x811C9DC5u;
    for (; *key; ++key) h = (h ^ (unsigned char)*key) * 0x01000193u;
    return h;
}

static void copy_field(char *out, const unsigned char *in, size_t size) {
    memcpy(out, in, size);
    out[size - 1] = '\0';
}

int pe_lang_pack_parse(PeLangPack *pack, unsigned char *data, size_t size) {
    memset(pack, 0, sizeof *pack);
    if (!data || size < HEADER_SIZE || memcmp(data, "PELG", 4) || rd32(data + 4) != PE_LANG_PACK_VERSION)
        goto bad;
    uint32_t count = rd32(data + 8), text = rd32(data + 12);
    if (count > (size - HEADER_SIZE) / ENTRY_SIZE || text < HEADER_SIZE + count * ENTRY_SIZE || text > size)
        goto bad;
    for (uint32_t i = 0; i < count; ++i) {
        const unsigned char *e = data + HEADER_SIZE + i * ENTRY_SIZE;
        uint32_t at = rd32(e + 4);
        if (i && rd32(e) <= rd32(e - ENTRY_SIZE)) goto bad;     /* sorted, no duplicate hashes */
        if (at >= size - text || !memchr(data + text + at, 0xFF, size - text - at)) goto bad;
    }
    pack->data = data;
    pack->size = size;
    pack->count = count;
    copy_field(pack->lang, data + 16, sizeof pack->lang);
    copy_field(pack->name, data + 32, sizeof pack->name);
    return 0;
bad:
    free(data);
    memset(pack, 0, sizeof *pack);
    return -1;
}

int pe_lang_pack_load(PeLangPack *pack, const char *path) {
    FILE *f = fopen(path, "rb");
    long size;
    unsigned char *data = NULL;
    memset(pack, 0, sizeof *pack);
    if (!f) return -1;
    if (!fseek(f, 0, SEEK_END) && (size = ftell(f)) > 0 && size < (64L << 20) && !fseek(f, 0, SEEK_SET) &&
        (data = malloc((size_t)size)) && fread(data, 1, (size_t)size, f) == (size_t)size) {
        fclose(f);
        return pe_lang_pack_parse(pack, data, (size_t)size);
    }
    free(data);
    fclose(f);
    return -1;
}

void pe_lang_pack_free(PeLangPack *pack) {
    free(pack->data);
    memset(pack, 0, sizeof *pack);
}

const unsigned char *pe_lang_pack_find(const PeLangPack *pack, const char *key) {
    if (!pack || !pack->data || !key) return NULL;
    uint32_t want = pe_lang_hash(key), lo = 0, hi = pack->count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        const unsigned char *e = pack->data + HEADER_SIZE + mid * ENTRY_SIZE;
        uint32_t h = rd32(e);
        if (h == want) return pack->data + rd32(pack->data + 12) + rd32(e + 4);
        if (h < want) lo = mid + 1;
        else hi = mid;
    }
    return NULL;
}

const unsigned char *pe_lang_pack_table(const PeLangPack *pack, unsigned table, unsigned index) {
    char key[32];
    const char *prefix = table == 4 ? "sys" : table == 8 ? "item" : table == 12 ? "desc" : table == 16 ? "misc" : NULL;
    if (!prefix || index > 999) return NULL;
    snprintf(key, sizeof key, "%s/%03u", prefix, index);
    return pe_lang_pack_find(pack, key);
}

size_t pe_lang_bank_size(const unsigned char *bank, size_t max) {
    size_t i = 0;
    if (!bank || max < 2 || bank[0] != 0xFE) return 0;
    while (i + 1 < max && bank[i] == 0xFE) {
        size_t j = i + 2;
        while (j < max && bank[j] != 0xFF && bank[j] != 0xF9) {
            if (bank[j] == 0xFC || bank[j] == 0xFD) j += 2;
            else if (bank[j] == 0xFB && j + 1 < max) j += bank[j + 1] == 7 || bank[j + 1] == 9 ? 3 : 2;
            else j += 1;
        }
        if (j >= max) return 0;
        i = j + 1;
    }
    return i;
}

uint32_t pe_lang_crc32(const unsigned char *data, size_t size) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int k = 0; k < 8; ++k) crc = crc >> 1 ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

const unsigned char *pe_lang_pack_page(const PeLangPack *pack, uint32_t bank_crc, unsigned page) {
    char key[32], name[16];
    const unsigned char *bank;
    size_t n = 0;
    snprintf(key, sizeof key, "bank/%08x", (unsigned)bank_crc);
    if (page > 255 || !(bank = pe_lang_pack_find(pack, key))) return NULL;
    while (n + 1 < sizeof name && bank[n] != 0xFF) {
        name[n] = (char)bank[n];
        ++n;
    }
    name[n] = '\0';
    snprintf(key, sizeof key, "dlg/%s/%03u", name, page);
    return pe_lang_pack_find(pack, key);
}
