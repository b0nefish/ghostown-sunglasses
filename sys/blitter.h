#ifndef __BLITTER_H__
#define __BLITTER_H__

#include "gfx.h"
#include "hardware.h"

/* Values for bltcon0. */
#define LINE_OR  ((ABC | ABNC | NABC | NANBC) | (SRCA | SRCC | DEST))
#define LINE_EOR ((ABNC | NABC | NANBC) | (SRCA | SRCC | DEST))

#define A_XOR_B (ANBC | NABC | ANBNC | NABNC)
#define A_AND_B (ABC | ABNC)
#define A_AND_NOT_B (ANBC | ANBNC)

#define HALF_ADDER ((SRCA | SRCB | DEST) | A_XOR_B)
#define HALF_ADDER_CARRY ((SRCA | SRCB | DEST) | A_AND_B)
#define FULL_ADDER ((SRCA | SRCB | SRCC | DEST) | (NANBC | NABNC | ANBNC | ABC))
#define FULL_ADDER_CARRY ((SRCA | SRCB | SRCC | DEST) | (NABC | ANBC | ABNC | ABC))

#define HALF_SUB ((SRCA | SRCB | DEST) | A_XOR_B)
#define HALF_SUB_BORROW ((SRCA | SRCB | DEST) | (NABC | NABNC))

/* Values for bltcon1. */
#define LINE_SOLID  (LINEMODE)
#define LINE_ONEDOT (LINEMODE | ONEDOT)

__regargs void BlitterClearSync(BitmapT *bitmap, WORD plane);

__regargs void BlitterFill(BitmapT *bitmap, WORD plane);
__regargs void BlitterFillSync(BitmapT *bitmap, WORD plane);

void BlitterSetSync(BitmapT *dst, WORD dstbpl,
                    UWORD x, UWORD y, UWORD w, UWORD h, UWORD val);
void BlitterSetMaskedSync(BitmapT *dst, WORD dstbpl, UWORD x, UWORD y,
                          BitmapT *msk, UWORD val);

__regargs void BlitterLineSetup(BitmapT *bitmap, UWORD plane, UWORD bltcon0, UWORD bltcon1);
__regargs void BlitterLine(WORD x1, WORD y1, WORD x2, WORD y2);
__regargs void BlitterLineSync(WORD x1, WORD y1, WORD x2, WORD y2);

static inline BOOL BlitterBusy() {
  return custom->dmaconr & DMAF_BLTDONE;
}

static inline void WaitBlitter() {
  asm("1: btst #6,0xdff002\n"
      "   bnes 1b");
}

#endif
