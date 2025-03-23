#include "bitmap.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "hex_dump.h"
#include "limits.h"  // 		#include <limits.h>
#include "round.h"   // 		#include <round.h>
#define ASSERT(CONDITION) assert(CONDITION)

/* Returns the index of the element that contains the bit
   numbered BIT_IDX. */
static inline size_t
elem_idx(size_t bit_idx) {
    return bit_idx / ELEM_BITS;
}

/* Returns an elem_type where only the bit corresponding to
   BIT_IDX is turned on. */
static inline elem_type
bit_mask(size_t bit_idx) {
    return (elem_type)1 << (bit_idx % ELEM_BITS);
}

/* Returns the number of elements required for BIT_CNT bits. */
static inline size_t
elem_cnt(size_t bit_cnt) {
    return DIV_ROUND_UP(bit_cnt, ELEM_BITS);
}

/* Returns the number of bytes required for BIT_CNT bits. */
static inline size_t
byte_cnt(size_t bit_cnt) {
    return sizeof(elem_type) * elem_cnt(bit_cnt);
}

/* Returns a bit mask in which the bits actually used in the last
   element of B's bits are set to 1 and the rest are set to 0. */
static inline elem_type
last_mask(const struct bitmap *b) {
    int last_bits = b->bit_cnt % ELEM_BITS;
    return last_bits ? ((elem_type)1 << last_bits) - 1 : (elem_type)-1;
}

/* Creation and destruction. */

/* Initializes B to be a bitmap of BIT_CNT bits
   and sets all of its bits to false.
   Returns true if success, false if memory allocation
   failed. */
struct bitmap *
bitmap_create(size_t bit_cnt) {
    struct bitmap *b = malloc(sizeof *b);
    if (b != NULL) {
        b->bit_cnt = bit_cnt;
        b->bits = malloc(byte_cnt(bit_cnt));
        if (b->bits != NULL || bit_cnt == 0) {
            bitmap_set_all(b, false);
            return b;
        }
        free(b);
    }
    return NULL;
}

/* Creates and returns a bitmap with BIT_CNT bits in the
   BLOCK_SIZE bytes of storage preallocated at BLOCK.
   BLOCK_SIZE must be at least bitmap_needed_bytes(BIT_CNT). */
struct bitmap *
bitmap_create_in_buf(size_t bit_cnt, void *block, size_t block_size)
// Remove KERNEL MACRO 'UNUSED')
{
    struct bitmap *b = block;

    ASSERT(block_size >= bitmap_buf_size(bit_cnt));

    b->bit_cnt = bit_cnt;
    b->bits = (elem_type *)(b + 1);
    bitmap_set_all(b, false);
    return b;
}

/* Returns the number of bytes required to accomodate a bitmap
   with BIT_CNT bits (for use with bitmap_create_in_buf()). */
size_t
bitmap_buf_size(size_t bit_cnt) {
    return sizeof(struct bitmap) + byte_cnt(bit_cnt);
}

/* Destroys bitmap B, freeing its storage.
   Not for use on bitmaps created by
   bitmap_create_preallocated(). */
void bitmap_destroy(struct bitmap *b) {
    if (b != NULL) {
        free(b->bits);
        free(b);
    }
}

/* Bitmap size. */

/* Returns the number of bits in B. */
size_t
bitmap_size(const struct bitmap *b) {
    return b->bit_cnt;
}

/* Setting and testing single bits. */

/* Atomically sets the bit numbered IDX in B to VALUE. */
void bitmap_set(struct bitmap *b, size_t idx, bool value) {
    ASSERT(b != NULL);
    ASSERT(idx < b->bit_cnt);
    if (value)
        bitmap_mark(b, idx);
    else
        bitmap_reset(b, idx);
}

/* Atomically sets the bit numbered BIT_IDX in B to true. */
void bitmap_mark(struct bitmap *b, size_t bit_idx) {
    size_t idx = elem_idx(bit_idx);
    elem_type mask = bit_mask(bit_idx);

    /* This is equivalent to `b->bits[idx] |= mask' except that it
       is guaranteed to be atomic on a uniprocessor machine.  See
       the description of the OR instruction in [IA32-v2b]. */
    asm("orl %k1, %k0" : "=m"(b->bits[idx]) : "r"(mask) : "cc");
}

/* Atomically sets the bit numbered BIT_IDX in B to false. */
void bitmap_reset(struct bitmap *b, size_t bit_idx) {
    size_t idx = elem_idx(bit_idx);
    elem_type mask = bit_mask(bit_idx);

    /* This is equivalent to `b->bits[idx] &= ~mask' except that it
       is guaranteed to be atomic on a uniprocessor machine.  See
       the description of the AND instruction in [IA32-v2a]. */
    asm("andl %k1, %k0" : "=m"(b->bits[idx]) : "r"(~mask) : "cc");
}

/* Atomically toggles the bit numbered IDX in B;
   that is, if it is true, makes it false,
   and if it is false, makes it true. */
void bitmap_flip(struct bitmap *b, size_t bit_idx) {
    size_t idx = elem_idx(bit_idx);
    elem_type mask = bit_mask(bit_idx);

    /* This is equivalent to `b->bits[idx] ^= mask' except that it
       is guaranteed to be atomic on a uniprocessor machine.  See
       the description of the XOR instruction in [IA32-v2b]. */
    asm("xorl %k1, %k0" : "=m"(b->bits[idx]) : "r"(mask) : "cc");
}

/* Returns the value of the bit numbered IDX in B. */
bool bitmap_test(const struct bitmap *b, size_t idx) {
    ASSERT(b != NULL);
    ASSERT(idx < b->bit_cnt);
    return (b->bits[elem_idx(idx)] & bit_mask(idx)) != 0;
}

/* Setting and testing multiple bits. */

/* Sets all bits in B to VALUE. */
void bitmap_set_all(struct bitmap *b, bool value) {
    ASSERT(b != NULL);

    bitmap_set_multiple(b, 0, bitmap_size(b), value);
}

/* Sets the CNT bits starting at START in B to VALUE. */
void bitmap_set_multiple(struct bitmap *b, size_t start, size_t cnt, bool value) {
    size_t i;

    ASSERT(b != NULL);
    ASSERT(start <= b->bit_cnt);
    ASSERT(start + cnt <= b->bit_cnt);

    for (i = 0; i < cnt; i++)
        bitmap_set(b, start + i, value);
}

/* Returns the number of bits in B between START and START + CNT,
   exclusive, that are set to VALUE. */
size_t
bitmap_count(const struct bitmap *b, size_t start, size_t cnt, bool value) {
    size_t i, value_cnt;

    ASSERT(b != NULL);
    ASSERT(start <= b->bit_cnt);
    ASSERT(start + cnt <= b->bit_cnt);

    value_cnt = 0;
    for (i = 0; i < cnt; i++)
        if (bitmap_test(b, start + i) == value)
            value_cnt++;
    return value_cnt;
}

/* Returns true if any bits in B between START and START + CNT,
   exclusive, are set to VALUE, and false otherwise. */
bool bitmap_contains(const struct bitmap *b, size_t start, size_t cnt, bool value) {
    size_t i;

    ASSERT(b != NULL);
    ASSERT(start <= b->bit_cnt);
    ASSERT(start + cnt <= b->bit_cnt);

    for (i = 0; i < cnt; i++)
        if (bitmap_test(b, start + i) == value)
            return true;
    return false;
}

/* Returns true if any bits in B between START and START + CNT,
   exclusive, are set to true, and false otherwise.*/
bool bitmap_any(const struct bitmap *b, size_t start, size_t cnt) {
    return bitmap_contains(b, start, cnt, true);
}

/* Returns true if no bits in B between START and START + CNT,
   exclusive, are set to true, and false otherwise.*/
bool bitmap_none(const struct bitmap *b, size_t start, size_t cnt) {
    return !bitmap_contains(b, start, cnt, true);
}

/* Returns true if every bit in B between START and START + CNT,
   exclusive, is set to true, and false otherwise. */
bool bitmap_all(const struct bitmap *b, size_t start, size_t cnt) {
    return !bitmap_contains(b, start, cnt, false);
}

/* Finding set or unset bits. */

/* Finds and returns the starting index of the first group of CNT
   consecutive bits in B at or after START that are all set to
   VALUE.
   If there is no such group, returns BITMAP_ERROR. */
size_t
bitmap_scan(const struct bitmap *b, size_t start, size_t cnt, bool value) {
    ASSERT(b != NULL);
    ASSERT(start <= b->bit_cnt);

    if (cnt <= b->bit_cnt) {
        size_t last = b->bit_cnt - cnt;
        size_t i;
        for (i = start; i <= last; i++)
            if (!bitmap_contains(b, i, cnt, !value))
                return i;
    }
    return BITMAP_ERROR;
}

/* Finds the first group of CNT consecutive bits in B at or after
   START that are all set to VALUE, flips them all to !VALUE,
   and returns the index of the first bit in the group.
   If there is no such group, returns BITMAP_ERROR.
   If CNT is zero, returns 0.
   Bits are set atomically, but testing bits is not atomic with
   setting them. */
size_t
bitmap_scan_and_flip(struct bitmap *b, size_t start, size_t cnt, bool value) {
    size_t idx = bitmap_scan(b, start, cnt, value);
    if (idx != BITMAP_ERROR)
        bitmap_set_multiple(b, idx, cnt, !value);
    return idx;
}

/* Returns the number of bytes needed to store B in a file. */
size_t
bitmap_file_size(const struct bitmap *b) {
    return byte_cnt(b->bit_cnt);
}

/* Debugging. */

/* Dumps the contents of B to the console as hexadecimal. */
void bitmap_dump(const struct bitmap *b) {
    hex_dump(0, b->bits, byte_cnt(b->bit_cnt) / 2, false);
}

/*
20211507 DongGeon Kim
*/

/* Bitmap size. */
void Bitmap_Size(struct bitmap *Bitmap) {
    // size 출력
    printf("%zu\n", bitmap_size(Bitmap));
    return;
}

/* Setting and testing single bits. */
void Bitmap_Set(struct bitmap *Bitmap) {
    // i를 option(true or false)로 세팅
    int i;
    char option[10];
    scanf("%d %s", &i, option);
    bitmap_set(Bitmap, i, strcmp(option, "false"));
    return;
}

void Bitmap_Mark(struct bitmap *Bitmap) {
    // i를 true로
    int i;
    scanf("%d", &i);
    bitmap_mark(Bitmap, i);
    return;
}

void Bitmap_Reset(struct bitmap *Bitmap) {
    // i를 false로
    int i;
    scanf("%d", &i);
    bitmap_reset(Bitmap, i);
    return;
}

void Bitmap_Flip(struct bitmap *Bitmap) {
    // 뒤집기
    int i;
    scanf("%d", &i);
    bitmap_flip(Bitmap, i);
}

void Bitmap_Test(struct bitmap *Bitmap) {
    // i가 true인지 false인지
    int i;
    scanf("%d", &i);
    if (bitmap_test(Bitmap, i)) printf("true\n");
    else printf("false\n");
    return;
}

/* Setting and testing multiple bits. */
void Bitmap_Set_All(struct bitmap *Bitmap) {
    // 모든 비트를 option으로 설정
    char option[10];
    scanf("%s", option);
    bitmap_set_all(Bitmap, strcmp(option, "false"));
    return;
}

void Bitmap_Set_Multiple(struct bitmap *Bitmap) {
    // s부터 cnt개를 option으로 설정
    int s, cnt;
    char option[10];
    scanf("%d %d %s", &s, &cnt, option);
    bitmap_set_multiple(Bitmap, s, cnt, strcmp(option, "false"));
    return;
}

void Bitmap_Count(struct bitmap *Bitmap) {
    // s부터 cnt개 중에서 option인 것의 개수
    int s, cnt;
    char option[10];
    scanf("%d %d %s", &s, &cnt, option);
    printf("%zu\n", bitmap_count(Bitmap, s, cnt, strcmp(option, "false")));
    return;
}

void Bitmap_Contains(struct bitmap *Bitmap) {
    // s부터 cnt개 중에서 option이 있는지 여부
    int s, cnt;
    char option[10];
    scanf("%d %d %s", &s, &cnt, option);
    if (bitmap_contains(Bitmap, s, cnt, strcmp(option, "false"))) printf("true\n");
    else printf("false\n");
    return;
}

void Bitmap_Any(struct bitmap *Bitmap) {
    // s부터 cnt개 중에서 true가 있는지
    int s, cnt;
    scanf("%d %d", &s, &cnt);
    if (bitmap_any(Bitmap, s, cnt)) printf("true\n");
    else printf("false\n");
    return;
}

void Bitmap_None(struct bitmap *Bitmap) {
    // s부터 cnt개 모두 false인지
    int s, cnt;
    scanf("%d %d", &s, &cnt);
    if (bitmap_none(Bitmap, s, cnt)) printf("true\n");
    else printf("false\n");
    return;
}
void Bitmap_All(struct bitmap *Bitmap) {
    // s부터 cnt개 모두 true인지
    int s, cnt;
    scanf("%d %d", &s, &cnt);
    if (bitmap_all(Bitmap, s, cnt)) printf("true\n");
    else printf("false\n");
    return;
}

/* Finding set or unset bits. */
void Bitmap_Scan(struct bitmap *Bitmap) {
    // s부터 cnt가 처음으로 option을 가지는 인덱스 출력
    int s, cnt;
    char option[10];
    scanf("%d %d %s", &s, &cnt, option);
    printf("%zu\n", bitmap_scan(Bitmap, s, cnt, strcmp(option, "false")));
    return;
}

void Bitmap_Scan_And_Flip(struct bitmap *Bitmap) {
    // s부터 cnt가 처음으로 option을 가지는 인덱스 출력
    // + 그 구간을 flip
    int s, cnt;
    char option[10];
    scanf("%d %d %s", &s, &cnt, option);
    printf("%zu\n", bitmap_scan_and_flip(Bitmap, s, cnt, strcmp(option, "false")));
    return;
}

/* Debugging. */
void Bitmap_Dump(struct bitmap *Bitmap) {
    // 16진수로 출력하는 함수
    bitmap_dump(Bitmap);
    return;
}

void Bitmap_Print(struct bitmap *Bitmap) {
    // Bitmap 출력하는 함수
    for (int i = 0; i < Bitmap->bit_cnt; i++) printf("%d", bitmap_test(Bitmap, i));
    printf("\n");
}

/* Other */
struct bitmap* Bitmap_Expand(struct bitmap *Bitmap) {
    // bitmap을 size만큼 확장
    int sz;
    scanf("%d", &sz);
    struct bitmap *new_bitmap = bitmap_create(Bitmap->bit_cnt + sz);
    for (int i = 0; i < Bitmap->bit_cnt; i++) bitmap_set(new_bitmap, i, bitmap_test(Bitmap, i));
    return new_bitmap;
}