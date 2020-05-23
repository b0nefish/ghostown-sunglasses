#include "startup.h"
#include "blitter.h"
#include "coplist.h"
#include "3d.h"
#include "fx.h"
#include "iff.h"
#include "ffp.h"
#include "memory.h"

#define WIDTH  320
#define HEIGHT 256
#define DEPTH 1

static Object3D *cube;

static CopListT *cp;
static BitmapT *screen[2];
static UWORD active = 0;
static CopInsT *bplptr[DEPTH];

#define ID_LWOB MAKE_ID('L', 'W', 'O', 'B')
#define ID_LWO2 MAKE_ID('L', 'W', 'O', '2')
#define ID_PNTS MAKE_ID('P', 'N', 'T', 'S')
#define ID_POLS MAKE_ID('P', 'O', 'L', 'S')

static __regargs Object3D *LoadLWO(char *filename, FLOAT scale) {
  Object3D *obj = NULL;
  IffFileT iff;

  if (OpenIff(&iff, filename)) {
    if (iff.header.type == ID_LWOB || iff.header.type == ID_LWO2) {
      FLOAT *pnts = NULL;
      WORD *pols = NULL;
      LONG pntsLength = 0;
      LONG polsLength = 0;

      while (ParseChunk(&iff)) {
        switch (iff.chunk.type) {
          case ID_PNTS:
            pntsLength = iff.chunk.length;
            pnts = MemAlloc(pntsLength, MEMF_PUBLIC);
            ReadChunk(&iff, pnts);
            break;

          case ID_POLS:
            polsLength = iff.chunk.length;
            pols = MemAlloc(polsLength, MEMF_PUBLIC);
            ReadChunk(&iff, pols);
            break;

          default:
            SkipChunk(&iff);
            break;
        }
      }

      {
        UWORD points = pntsLength / 12;
        UWORD polygons = 0;
        UWORD polygonVertices = 0;

        LONG i = 0;
        LONG n = polsLength / 2;

        if (iff.header.type == ID_LWOB) {
          while (i < n) {
            WORD vertices = pols[i++];
            polygonVertices += vertices;
            polygons++;
            i += vertices + 1;
          }
        } else {
          i += 2;
          while (i < n) {
            WORD vertices = pols[i++];
            polygonVertices += vertices;
            polygons++;
            i += vertices;
          }
        }

        obj = NewObject3D(points, polygons);

        Log("File '%s' has %ld points and %ld polygons.\n", 
            filename, (LONG)points, (LONG)polygons);

        /* Process points. */
        {
          FLOAT s = SPMul(scale, SPFlt(16));

          for (i = 0; i < points; i++) {
            obj->point[i].x = SPFix(SPMul(SPFieee(pnts[i * 3 + 0]), s));
            obj->point[i].y = SPFix(SPMul(SPFieee(pnts[i * 3 + 1]), s));
            obj->point[i].z = SPFix(SPMul(SPFieee(pnts[i * 3 + 2]), s));
          }

          MemFree(pnts, pntsLength);
        }

        /* Process polygons. */
        {
          WORD *polygonVertex = MemAlloc(sizeof(UWORD) * polygonVertices,
                                         MEMF_PUBLIC);
          WORD p = 0, j = 0;

          i = 0;
          n = polsLength / 2;

          if (iff.header.type == ID_LWOB) {
            while (i < n) {
              WORD vertices = pols[i++];
              obj->polygon[p].vertices = vertices;
              obj->polygon[p].index = j;
              while (--vertices >= 0)
                polygonVertex[j++] = pols[i++];
              i++;
              p++;
            }
          } else {
            i += 2;
            while (i < n) {
              WORD vertices = pols[i++];
              obj->polygon[p].vertices = vertices;
              obj->polygon[p].index = j;
              while (--vertices >= 0)
                polygonVertex[j++] = pols[i++];
              p++;
            }
          }

          obj->polygonVertices = polygonVertices;
          obj->polygonVertex = polygonVertex;

          MemFree(pols, polsLength);
        }
      }
    }

    CloseIff(&iff);
  }

  return obj;
}

static void Load() {
  screen[0] = NewBitmap(WIDTH, HEIGHT, 1);
  screen[1] = NewBitmap(WIDTH, HEIGHT, 1);
  cp = NewCopList(80);
  cube = LoadLWO("data/new_2.lwo", SPFlt(80));

  CalculateEdges(cube);
}

static void UnLoad() {
  DeleteObject3D(cube);
  DeleteCopList(cp);
  DeleteBitmap(screen[0]);
  DeleteBitmap(screen[1]);
}

static void MakeCopperList(CopListT *cp) {
  CopInit(cp);
  CopMakePlayfield(cp, bplptr, screen[active], DEPTH);
  CopMakeDispWin(cp, X(0), Y(0), WIDTH, HEIGHT);
  CopSetRGB(cp, 0, 0x000);
  CopSetRGB(cp, 1, 0xfff);
  CopEnd(cp);
}

static void Init() {
  MakeCopperList(cp);
  CopListActivate(cp);
  custom->dmacon = DMAF_SETCLR | DMAF_BLITTER | DMAF_RASTER;
}

__regargs static void CalculatePerspective(Point3D *p, WORD points) {
  WORD *src = (WORD *)p;
  WORD *dst = (WORD *)p;
  WORD n = points;

  while (--n >= 0) {
    WORD x = *src++;
    WORD y = *src++;
    WORD z = *src++;

    *dst++ = div16(256 * x, z) + WIDTH / 2;
    *dst++ = div16(256 * y, z) + HEIGHT / 2;
    dst++;
  }
}

__regargs static void DrawObject(Object3D *object) {
  Point3D *point = object->cameraPoint;
  UWORD *edge = (UWORD *)object->edge;
  WORD edges = object->edges;

  BlitterLineSetup(screen[active], 0, LINE_OR, LINE_SOLID);

  while (--edges >= 0) {
    Point3D *p0 = (APTR)point + (ULONG)*edge++;
    Point3D *p1 = (APTR)point + (ULONG)*edge++;
    BlitterLineSync(p0->x, p0->y, p1->x, p1->y);
  }
}

static Point3D rotate = { 0, 0, 0 };

static void Render() {
  Matrix3D t;

  rotate.x += 16;
  rotate.y += 16;
  rotate.z += 16;

  {
    LONG lines = ReadLineCounter();
    LoadRotate3D(&t, rotate.x, rotate.y, rotate.z);
    Translate3D(&t, 0, 0, fx4i(-250));
    Transform3D(&t, cube->cameraPoint, cube->point, cube->points);

    CalculatePerspective(cube->cameraPoint, cube->points);
    Log("transform: %ld\n", ReadLineCounter() - lines);
  }

  BlitterClear(screen[active], 0);
  WaitBlitter();

  {
    LONG lines = ReadLineCounter();
    DrawObject(cube);
    Log("draw: %ld\n", ReadLineCounter() - lines);
  }

  WaitVBlank();
  ITER(i, 0, DEPTH - 1, CopInsSet32(bplptr[i], screen[active]->planes[i]));
  active ^= 1;
}

EffectT Effect = { Load, UnLoad, Init, NULL, Render };
