#ifndef PE1_GTE_TYPES_H
#define PE1_GTE_TYPES_H

#include "common.h"
#include "pe1/gte_short_vector.h"

typedef struct GteMatrix {
    s16 m[3][3];
    s32 t[3];
} GteMatrix;

/* Word transfers to GTE control registers pack successive matrix halfwords.
 * The fifth word includes the MATRIX alignment halfword, not translation.
 * Keep this view separate from the signed-halfword arithmetic view above.
 */
typedef struct GteMatrixWords {
    u32 r11_r12;
    u32 r13_r21;
    u32 r22_r23;
    u32 r31_r32;
    u32 r33_pad;
    s32 tx, ty, tz;
} GteMatrixWords;

PE1_STATIC_ASSERT(sizeof(GteMatrixWords) == sizeof(GteMatrix), gte_matrix_words_size);
PE1_STATIC_ASSERT(PE1_OFFSETOF(GteMatrixWords, r33_pad) == 16,
                  gte_matrix_words_last_rotation_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(GteMatrixWords, tx) == PE1_OFFSETOF(GteMatrix, t),
                  gte_matrix_words_translation_offset);

/* Full MATRIX storage with its packed COP2 transfer view. */
typedef union GteMatrixStorage {
    GteMatrix matrix;
    u32 words[8];
} GteMatrixStorage;

PE1_STATIC_ASSERT(sizeof(GteMatrixStorage) == 32, gte_matrix_storage_size);

/* MULTU: unsigned 32 x 32 -> 64 bit product in HI/LO. The Q12 matrix code
 * feeds signed operands and reads LO only, as the assembler does. A macro,
 * not an inline function, so the pinned operand registers feed MULTU
 * directly. */
#define GTE_MULTU(a, b) ((unsigned long long)(u32)(a) * (u32)(b))

typedef struct GteVector {
    s32 x, y, z, pad;
} GteVector;

GteMatrix *RotMatrix(GteShortVector *angles, GteMatrix *matrix);
GteMatrix *RotMatrixYXZ(GteShortVector *angles, GteMatrix *matrix);
GteMatrix *RotMatrixZYX(GteShortVector *angles, GteMatrix *matrix);
GteMatrix *RotMatrixY(long angle, GteMatrix *matrix);
GteMatrix *RotMatrixZ(long angle, GteMatrix *matrix);

GteShortVector *ApplyMatrixSV(const GteMatrix *matrix,
                            const GteShortVector *v, GteShortVector *out);
GteVector *ApplyMatrixLV(const GteMatrix *matrix, const GteVector *v,
                         GteVector *out);
void LoadAverageShort12(void *first, void *second, int first_scale,
                        int second_scale, void *output);
void LoadAverageShort0(void *first, void *second, int first_scale,
                        int second_scale, void *output);
GteMatrix *ScaleMatrix(GteMatrix *matrix, const GteVector *scale);
GteMatrix *TransMatrix(GteMatrix *matrix, const GteVector *translation);
void SetRotMatrix(const GteMatrix *matrix);
void SetLightMatrix(const GteMatrix *matrix);
void SetColorMatrix(const GteMatrix *matrix);
void SetTransMatrix(const GteMatrix *matrix);
int ratan2(int y, int x);
/* RATAN.OBJ arctangent table, indexed by the scaled ratio. */
extern s16 atan_table[];
/* An atan_table entry addressed by its byte offset (the doubled ratio). */
typedef union GteAtanEntryAddress {
    s16 *entry;
    u32 word;
} GteAtanEntryAddress;
long SquareRoot0(long value);
void VectorNormal(GteVector *vector, GteVector *unit);
long VectorNormalS(GteVector *vector, GteShortVector *unit);
GteVector *OuterProduct12(void *matrix_column, void *vector, void *output);
void SetFogNearFar(long near, long far, long h);
/* Original REG03 register-transfer entry points. */
void SetVertex0(void *vertex);
void SetVertex1(void *vertex);
void SetVertex2(void *vertex);
void SetVertexTri(void *v0, void *v1, void *v2);
void SetRGBfifo(void *c0, void *c1, void *c2);
void SetIR123(int ir1, int ir2, int ir3);
void SetIR0(int ir0);
void SetSZfifo3(int sz1, int sz2, int sz3);
void SetSZfifo4(int sz0, int sz1, int sz2, int sz3);
void SetSXSYfifo(int sxy0, int sxy1, int sxy2);
void SetRii(int r11r12, int r22r23, int r33);
void SetMAC123(int mac1, int mac2, int mac3);
void SetData32(int data);
void SetDQA(int dqa);
void SetDQB(int dqb);

PE1_STATIC_ASSERT(sizeof(GteMatrix) == 32, gte_matrix_size);
PE1_STATIC_ASSERT(PE1_OFFSETOF(GteMatrix, t) == 20, gte_matrix_translation_offset);
PE1_STATIC_ASSERT(sizeof(GteVector) == 16, gte_vector_size);

typedef struct GteRotation {
    s16 x;
    s16 y;
    s16 z;
    s16 flags;
} GteRotation;

int rcos(int angle);

int rsin(int angle);

GteMatrix *CompMatrix(GteMatrix *m0, GteMatrix *m1, GteMatrix *m2);

void RotTrans(const GteShortVector *v, GteVector *out, int *flag);

long RotTransPers(const GteShortVector *v, int *sxy, int *p, int *flag);

#endif
