// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Procedural gear-cog geometry — built in code, no imported mesh asset. A cog with N
 * teeth is generated as an extruded silhouette (2*N rim points alternating between the
 * tooth tip radius and the valley radius), so the number of teeth you SEE is literally
 * the integer in the ratio: a 3-tooth cog meshing a 2-tooth cog is the 3:2 made visible.
 *
 * Deterministic and engine-light (pure vertex/triangle math), so it is unit-testable
 * headlessly; the visualizer feeds the result to a UProceduralMeshComponent.
 */
namespace ManifoldGearMesh
{
    /**
     * Build a cog with Teeth teeth. Fills OutVerts / OutTris (a closed, extruded cog:
     * top face + bottom face + rim). Teeth is clamped to >= 1.
     * @param Teeth       number of teeth (the ratio integer)
     * @param Radius      valley radius (body of the cog)
     * @param ToothDepth  how far the teeth protrude beyond Radius
     * @param HalfThick   half the extrusion thickness along +/- Z
     */
    MANIFOLDGAMEPLAY_API void Build(
        int32 Teeth, float Radius, float ToothDepth, float HalfThick,
        TArray<FVector>& OutVerts, TArray<int32>& OutTris);
}
