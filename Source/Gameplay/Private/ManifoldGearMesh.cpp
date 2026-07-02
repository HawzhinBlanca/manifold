// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "ManifoldGearMesh.h"

void ManifoldGearMesh::Build(
    int32 Teeth, float Radius, float ToothDepth, float HalfThick,
    TArray<FVector>& OutVerts, TArray<int32>& OutTris)
{
    OutVerts.Reset();
    OutTris.Reset();

    Teeth = FMath::Max(1, Teeth);
    const int32 Rim = 2 * Teeth;            // alternating tip / valley points
    const float TwoPi = 2.0f * PI;

    // --- Vertices ---
    // 0 = top centre, 1 = bottom centre, then Rim top-rim verts, then Rim bottom-rim verts.
    OutVerts.Add(FVector(0.0f, 0.0f,  HalfThick)); // 0 top centre
    OutVerts.Add(FVector(0.0f, 0.0f, -HalfThick)); // 1 bottom centre

    const int32 TopBase = OutVerts.Num();          // = 2
    for (int32 i = 0; i < Rim; ++i)
    {
        const float Angle = TwoPi * static_cast<float>(i) / static_cast<float>(Rim);
        const float R = Radius + ((i % 2 == 0) ? ToothDepth : 0.0f); // even = tooth tip
        OutVerts.Add(FVector(R * FMath::Cos(Angle), R * FMath::Sin(Angle),  HalfThick));
    }
    const int32 BotBase = OutVerts.Num();          // = 2 + Rim
    for (int32 i = 0; i < Rim; ++i)
    {
        const float Angle = TwoPi * static_cast<float>(i) / static_cast<float>(Rim);
        const float R = Radius + ((i % 2 == 0) ? ToothDepth : 0.0f);
        OutVerts.Add(FVector(R * FMath::Cos(Angle), R * FMath::Sin(Angle), -HalfThick));
    }

    // --- Triangles ---
    for (int32 i = 0; i < Rim; ++i)
    {
        const int32 Next = (i + 1) % Rim;

        // Top face fan (CCW seen from +Z).
        OutTris.Add(0);
        OutTris.Add(TopBase + i);
        OutTris.Add(TopBase + Next);

        // Bottom face fan (reverse winding).
        OutTris.Add(1);
        OutTris.Add(BotBase + Next);
        OutTris.Add(BotBase + i);

        // Rim quad (two triangles) connecting top and bottom rings.
        const int32 T0 = TopBase + i;
        const int32 T1 = TopBase + Next;
        const int32 B0 = BotBase + i;
        const int32 B1 = BotBase + Next;
        OutTris.Add(T0); OutTris.Add(B0); OutTris.Add(T1);
        OutTris.Add(T1); OutTris.Add(B0); OutTris.Add(B1);
    }
}
