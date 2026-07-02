// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "ManifoldWaveMesh.h"

void ManifoldWaveMesh::Build(
    int32 Harmonic, float Length, float Amplitude, float HalfThick,
    TArray<FVector>& OutVerts, TArray<int32>& OutTris)
{
    OutVerts.Reset();
    OutTris.Reset();

    Harmonic = FMath::Max(1, Harmonic);
    // Enough samples per antinode that even high harmonics read as a smooth curve.
    const int32 Segments = 16 * Harmonic;
    const float HalfLen = 0.5f * Length;

    // Two vertices per sample (front/back of the thin ribbon), following y = A*sin(N*pi*t).
    for (int32 i = 0; i <= Segments; ++i)
    {
        const float T = static_cast<float>(i) / static_cast<float>(Segments); // 0..1
        const float X = -HalfLen + Length * T;
        const float Y = Amplitude * FMath::Sin(static_cast<float>(Harmonic) * PI * T);
        OutVerts.Add(FVector(X, Y,  HalfThick));
        OutVerts.Add(FVector(X, Y, -HalfThick));
    }

    // Stitch consecutive sample pairs into quads (two triangles each).
    for (int32 i = 0; i < Segments; ++i)
    {
        const int32 A = 2 * i;       // front, this sample
        const int32 B = 2 * i + 1;   // back,  this sample
        const int32 C = 2 * i + 2;   // front, next sample
        const int32 D = 2 * i + 3;   // back,  next sample
        OutTris.Add(A); OutTris.Add(B); OutTris.Add(C);
        OutTris.Add(C); OutTris.Add(B); OutTris.Add(D);
    }
}
