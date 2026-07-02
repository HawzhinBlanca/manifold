// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Procedural standing-wave ribbon — built in code, no imported mesh. The N-th harmonic of
 * a string has N antinodes (N half-wavelengths along its length), so a ribbon shaped like
 * sin(N*pi*x/L) shows the harmonic number as the number of humps you see: an N=3 ribbon
 * beside an N=2 ribbon is the 3:2 made visible, the wave counterpart of the gear cog.
 *
 * Deterministic, engine-light vertex/triangle math — unit-testable headlessly; the
 * visualizer feeds the result to a UProceduralMeshComponent.
 */
namespace ManifoldWaveMesh
{
    /**
     * Build a flat ribbon following the N-th harmonic standing wave along X (centred on
     * the origin), of the given Length, peak Amplitude, and half-thickness in Z.
     * @param Harmonic  the harmonic index N (antinodes); clamped to >= 1
     */
    MANIFOLDGAMEPLAY_API void Build(
        int32 Harmonic, float Length, float Amplitude, float HalfThick,
        TArray<FVector>& OutVerts, TArray<int32>& OutTris);
}
