#include "iff.h"
#include "ilbm.h"
#include "memory.h"
#include "lzo.h"
#include "inflate.h"

#define USE_LZO 1
#define USE_DEFLATE 1

#define ID_ILBM MAKE_ID('I', 'L', 'B', 'M')
#define ID_BMHD MAKE_ID('B', 'M', 'H', 'D')
#define ID_CMAP MAKE_ID('C', 'M', 'A', 'P')

/* Masking technique. */
#define mskNone                0
#define mskHasMask             1
#define mskHasTransparentColor 2
#define mskLasso               3

typedef struct BitmapHeader {
  UWORD w, h;                  /* raster width & height in pixels      */
  WORD  x, y;                  /* pixel position for this image        */
  UBYTE nPlanes;               /* # source bitplanes                   */
  UBYTE masking;
  UBYTE compression;
  UBYTE pad1;                  /* unused; ignore on read, write as 0   */
  UWORD transparentColor;      /* transparent "color number" (sort of) */
  UBYTE xAspect, yAspect;      /* pixel aspect, a ratio width : height */
  WORD  pageWidth, pageHeight; /* source "page" size in pixels    */
} BitmapHeaderT;

__regargs static void UnRLE(BYTE *data, LONG size, BYTE *uncompressed) {
  BYTE *src = data;
  BYTE *end = data + size;
  BYTE *dst = uncompressed;

  do {
    WORD code = *src++;

    if (code < 0) {
      BYTE b = *src++;
      WORD n = -code;

      do { *dst++ = b; } while (--n != -1);
    } else {
      WORD n = code;

      do { *dst++ = *src++; } while (--n != -1);
    }
  } while (src < end);
}

__regargs static void Deinterleave(BYTE *data, BitmapT *bitmap) { 
  LONG bytesPerRow = bitmap->bytesPerRow;
  LONG modulo = (WORD)bytesPerRow * (WORD)(bitmap->depth - 1);
  WORD bplnum = bitmap->depth;
  WORD count = bytesPerRow / 2;
  WORD i = count & 7;
  WORD k = (count + 7) / 8;
  APTR *plane = bitmap->planes;

  do {
    BYTE *src = data;
    BYTE *dst = *plane++;
    WORD rows = bitmap->height;

    do {
      WORD n = k - 1;
      switch (i) {
        case 0: do { *((WORD *)dst)++ = *((WORD *)src)++;
        case 7:      *((WORD *)dst)++ = *((WORD *)src)++;
        case 6:      *((WORD *)dst)++ = *((WORD *)src)++;
        case 5:      *((WORD *)dst)++ = *((WORD *)src)++;
        case 4:      *((WORD *)dst)++ = *((WORD *)src)++;
        case 3:      *((WORD *)dst)++ = *((WORD *)src)++;
        case 2:      *((WORD *)dst)++ = *((WORD *)src)++;
        case 1:      *((WORD *)dst)++ = *((WORD *)src)++;
        } while (--n != -1);
      }

      src += modulo;
    } while (--rows);

    data += bytesPerRow;
  } while (--bplnum);
}

__regargs void BitmapUnpack(BitmapT *bitmap, UWORD flags) {
  if (bitmap->compression) {
    ULONG inLen = (LONG)bitmap->planes[1];
    APTR inBuf = bitmap->planes[0];
    ULONG outLen = BitmapSize(bitmap);
    APTR outBuf = MemAlloc(outLen, MEMF_PUBLIC);

    if (bitmap->compression == COMP_RLE)
      UnRLE(inBuf, inLen, outBuf);
#if USE_LZO
    else if (bitmap->compression == COMP_LZO)
      lzo1x_decompress(inBuf, inLen, outBuf, &outLen);
#endif
#if USE_DEFLATE
    else if (bitmap->compression == COMP_DEFLATE)
      Inflate(inBuf, outBuf);
#endif

    MemFree(inBuf, inLen);

    bitmap->compression = COMP_NONE;
    BitmapSetPointers(bitmap, outBuf);
  }

  if ((bitmap->flags & BM_INTERLEAVED) && !(flags & BM_INTERLEAVED)) {
    ULONG size = BitmapSize(bitmap);
    APTR inBuf = bitmap->planes[0];
    APTR outBuf = MemAlloc(size, MEMF_PUBLIC);

    bitmap->flags &= ~BM_INTERLEAVED;
    BitmapSetPointers(bitmap, outBuf);

    Deinterleave(inBuf, bitmap);
    MemFree(inBuf, size);
  }

  if (flags & BM_DISPLAYABLE)
    BitmapMakeDisplayable(bitmap);
}

__regargs BitmapT *LoadILBMCustom(const char *filename, UWORD flags) {
  BitmapT *bitmap = NULL;
  PaletteT *palette = NULL;
  IffFileT iff;

  if (OpenIff(&iff, filename)) {
    if (iff.header.type == ID_ILBM) {
      while (ParseChunk(&iff)) {
        BitmapHeaderT bmhd;

        switch (iff.chunk.type) {
          case ID_BMHD:
            ReadChunk(&iff, &bmhd);
            if (flags & BM_KEEP_PACKED) {
              bitmap = NewBitmapCustom(bmhd.w, bmhd.h, bmhd.nPlanes,
                                       BM_MINIMAL|BM_INTERLEAVED);
              bitmap->compression = bmhd.compression;
            } else {
              bitmap = NewBitmapCustom(bmhd.w, bmhd.h, bmhd.nPlanes, flags);
            }
            break;

          case ID_CMAP:
            if (flags & BM_LOAD_PALETTE) {
              palette = NewPalette(iff.chunk.length / sizeof(ColorT));
              ReadChunk(&iff, palette->colors);
            } else {
              SkipChunk(&iff);
            }
            break;
        
          case ID_BODY:
            {
              BYTE *data = MemAlloc(iff.chunk.length, MEMF_PUBLIC);
              LONG size = iff.chunk.length;
              ReadChunk(&iff, data);

              if (flags & BM_KEEP_PACKED) {
                bitmap->planes[0] = data;
                bitmap->planes[1] = (APTR)size;
                bitmap->flags &= ~BM_MINIMAL;
              } else {
                if (bmhd.compression) {
                  ULONG newSize = bitmap->bplSize * bitmap->depth;
                  BYTE *uncompressed = MemAlloc(newSize, MEMF_PUBLIC);

                  if (bmhd.compression == COMP_RLE)
                    UnRLE(data, size, uncompressed);
#if USE_LZO
                  if (bmhd.compression == COMP_LZO)
                    lzo1x_decompress(data, size, uncompressed, &newSize);
#endif
#if USE_DEFLATE
                  if (bmhd.compression == COMP_DEFLATE)
                    Inflate(data, uncompressed);
#endif
                  MemFree(data, size);

                  data = uncompressed;
                  size = newSize;
                }

                if (flags & BM_INTERLEAVED)
                  memcpy(bitmap->planes[0], data, bitmap->bplSize * bitmap->depth);
                else
                  Deinterleave(data, bitmap);

                MemFree(data, size);
              }
            }
            break;

          default:
            SkipChunk(&iff);
            break;
        }
      }

      if (bitmap)
        bitmap->palette = palette;
    }

    CloseIff(&iff);
  } else {
    Log("File '%s' missing.\n", filename);
  }

  return bitmap;
}

__regargs PaletteT *LoadPalette(const char *filename) {
  PaletteT *palette = NULL;
  IffFileT iff;

  if (OpenIff(&iff, filename)) {
    if (iff.header.type == ID_ILBM) {
      while (ParseChunk(&iff)) {
        switch (iff.chunk.type) {
          case ID_CMAP:
            palette = NewPalette(iff.chunk.length / sizeof(ColorT));
            ReadChunk(&iff, palette->colors);
            break;

          default:
            SkipChunk(&iff);
            break;
        }
      }
    }

    CloseIff(&iff);
  }

  return palette;
}
