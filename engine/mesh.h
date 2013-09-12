#ifndef __ENGINE_MESH_H__
#define __ENGINE_MESH_H__

#include "std/types.h"
#include "engine/vector3d.h"
#include "gfx/colors.h"

typedef struct Triangle {
  uint16_t surface;
  uint16_t p1, p2, p3;
} TriangleT;

typedef struct IndexArray {
  uint16_t count;
  uint16_t *index;
} IndexArrayT;

typedef struct IndexMap {
  IndexArrayT *vertex;
  uint16_t *indices;
} IndexMapT;

typedef struct Surface {
  char *name;
  bool sideness;
  union {
    RGB rgb;
    uint8_t clut;
  } color;
} SurfaceT;

typedef struct Mesh {
  size_t vertexNum;
  size_t polygonNum;
  size_t surfaceNum;

  Vector3D *vertex;
  TriangleT *polygon;
  SurfaceT *surface;

  /* map from vertex index to list of polygon indices */
  IndexMapT vertexToPoly;

  /* useful for lighting and backface culling */
  Vector3D *surfaceNormal;
  Vector3D *vertexNormal;
} MeshT;

MeshT *NewMesh(size_t vertices, size_t triangles, size_t surfaces);
MeshT *NewMeshFromFile(const char *fileName);
void NormalizeMeshSize(MeshT *mesh);
void CenterMeshPosition(MeshT *mesh);

void CalculateSurfaceNormals(MeshT *mesh);
void CalculateVertexToPolygonMap(MeshT *mesh);
void CalculateVertexNormals(MeshT *mesh);

#endif
