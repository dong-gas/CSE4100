#ifndef __MYLIB_BITMAP_H
#define __MYLIB_BITMAP_H

#include <stdbool.h>
#include <stddef.h>
#include <inttypes.h>

/* Element type.

   This must be an unsigned integer type at least as wide as int.

   Each bit represents one bit in the bitmap.
   If bit 0 in an element represents bit K in the bitmap,
   then bit 1 in the element represents bit K+1 in the bitmap,
   and so on. */
   typedef unsigned long elem_type;

   /* Number of bits in an element. */
   #define ELEM_BITS (sizeof (elem_type) * CHAR_BIT)
   
   /* From the outside, a bitmap is an array of bits.  From the
      inside, it's an array of elem_type (defined above) that
      simulates an array of bits. */
   struct bitmap
     {
       size_t bit_cnt;     /* Number of bits. */
       elem_type *bits;    /* Elements that represent bits. */
     };

/* Bitmap abstract data type. */

/* Creation and destruction. */
struct bitmap *bitmap_create (size_t bit_cnt);
struct bitmap *bitmap_create_in_buf (size_t bit_cnt, void *, size_t byte_cnt);
size_t bitmap_buf_size (size_t bit_cnt);
void bitmap_destroy (struct bitmap *);

/* Bitmap size. */
size_t bitmap_size (const struct bitmap *);

/* Setting and testing single bits. */
void bitmap_set (struct bitmap *, size_t idx, bool);
void bitmap_mark (struct bitmap *, size_t idx);
void bitmap_reset (struct bitmap *, size_t idx);
void bitmap_flip (struct bitmap *, size_t idx);
bool bitmap_test (const struct bitmap *, size_t idx);

/* Setting and testing multiple bits. */
void bitmap_set_all (struct bitmap *, bool);
void bitmap_set_multiple (struct bitmap *, size_t start, size_t cnt, bool);
size_t bitmap_count (const struct bitmap *, size_t start, size_t cnt, bool);
bool bitmap_contains (const struct bitmap *, size_t start, size_t cnt, bool);
bool bitmap_any (const struct bitmap *, size_t start, size_t cnt);
bool bitmap_none (const struct bitmap *, size_t start, size_t cnt);
bool bitmap_all (const struct bitmap *, size_t start, size_t cnt);

/* Finding set or unset bits. */
#define BITMAP_ERROR SIZE_MAX
size_t bitmap_scan (const struct bitmap *, size_t start, size_t cnt, bool);
size_t bitmap_scan_and_flip (struct bitmap *, size_t start, size_t cnt, bool);

/* File input and output. */
size_t bitmap_file_size (const struct bitmap *);

/* Debugging. */
void bitmap_dump (const struct bitmap *);

/*
20211507 DongGeon Kim
*/

/* Bitmap size. */
void Bitmap_Size(struct bitmap * );

/* Setting and testing single bits. */
void Bitmap_Set(struct bitmap * );
void Bitmap_Mark(struct bitmap * );
void Bitmap_Reset(struct bitmap * );
void Bitmap_Flip(struct bitmap * );
void Bitmap_Test(struct bitmap * );

/* Setting and testing multiple bits. */
void Bitmap_Set_All(struct bitmap * );
void Bitmap_Set_Multiple(struct bitmap * );
void Bitmap_Count(struct bitmap * );
void Bitmap_Contains(struct bitmap * );
void Bitmap_Any(struct bitmap * );
void Bitmap_None(struct bitmap * );
void Bitmap_All(struct bitmap * );

/* Finding set or unset bits. */
void Bitmap_Scan(struct bitmap * );
void Bitmap_Scan_And_Flip(struct bitmap * );

/* Debugging. */
void Bitmap_Dump(struct bitmap * );
void Bitmap_Print(struct bitmap * );

/* Other */
struct bitmap* Bitmap_Expand(struct bitmap *);

#endif /* bitmap.h */
