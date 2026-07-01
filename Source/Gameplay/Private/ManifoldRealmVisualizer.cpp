// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "ManifoldRealmVisualizer.h"
#include "ManifoldGameMode.h"
#include "ManifoldSlice.h"
#include "OrbitsKernel.h"
#include "FluidsKernel.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

namespace
{
    const FColor GoldColor(255, 215, 60);
    const FColor StarColor(255, 220, 80);
    const FColor PlanetColor(120, 180, 255);
    constexpr double AstronomicalUnit = 1.496e11; // metres
}

AManifoldRealmVisualizer::AManifoldRealmVisualizer()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AManifoldRealmVisualizer::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UWorld* World = GetWorld();
    if (!World) return;

    AManifoldGameMode* GM = World->GetAuthGameMode<AManifoldGameMode>();
    if (!GM || !GM->Slice || !GM->Slice->Orbits || !GM->Slice->Fluids) return;

    UManifoldSlice* S = GM->Slice;

    // --- Orbits realm: star + planets, resonance ribbon ---
    if (FOrbitsState* OS = static_cast<FOrbitsState*>(S->Orbits->GetState().Get()))
    {
        for (const FOrbitsBodyDef& Body : OS->Bodies)
        {
            const FVector P = OrbitsCenter
                + FVector(Body.Position.X, Body.Position.Y, 0.0) / AstronomicalUnit * OrbitsScale;
            const float Radius = Body.bIsCentral ? 32.0f : 14.0f;
            const FColor Col = Body.bIsCentral ? StarColor : PlanetColor;
            DrawDebugSphere(World, P, Radius, 12, Col, false, -1.0f, 0, 1.5f);
        }

        // Gold "resonance language" ribbon between the two resonant planets.
        if (S->HasResonance() && OS->Bodies.Num() >= 3)
        {
            const FVector A = OrbitsCenter
                + FVector(OS->Bodies[1].Position.X, OS->Bodies[1].Position.Y, 0.0) / AstronomicalUnit * OrbitsScale;
            const FVector B = OrbitsCenter
                + FVector(OS->Bodies[2].Position.X, OS->Bodies[2].Position.Y, 0.0) / AstronomicalUnit * OrbitsScale;
            DrawDebugLine(World, A, B, GoldColor, false, -1.0f, 0, 3.0f);
        }
    }

    // --- Fluids realm: density field as a point cloud ---
    if (FFluidsState* FS = static_cast<FFluidsState*>(S->Fluids->GetState().Get()))
    {
        const int32 N = FS->GridSize;
        if (N > 0 && FS->Density.Num() >= (N + 2) * (N + 2))
        {
            const int32 Stride = N + 2;
            const int32 Skip = FMath::Max(1, N / 20); // sample for performance
            for (int32 j = 1; j <= N; j += Skip)
            {
                for (int32 i = 1; i <= N; i += Skip)
                {
                    const float Density = FS->Density[i + Stride * j];
                    if (Density < 0.02f) continue;

                    const float U = static_cast<float>(i - 1) / N;
                    const float V = static_cast<float>(j - 1) / N;
                    const FVector P = FluidsCenter
                        + FVector((U - 0.5f) * 2.0f * FluidsExtent, (V - 0.5f) * 2.0f * FluidsExtent, 0.0f);
                    const uint8 Alpha = static_cast<uint8>(FMath::Clamp(Density * 255.0f, 32.0f, 255.0f));
                    DrawDebugPoint(World, P, 7.0f, FColor(80, 180, 255, Alpha), false, -1.0f);
                }
            }
        }
    }

    // --- The seam: a gold ribbon across realms when a correspondence is lit ---
    if (S->IsCorrespondenceAvailable())
    {
        DrawDebugLine(World, OrbitsCenter, FluidsCenter, GoldColor, false, -1.0f, 0, 5.0f);
    }
}
