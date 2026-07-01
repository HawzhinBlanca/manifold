// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "ManifoldRealmVisualizer.h"
#include "ManifoldGameMode.h"
#include "ManifoldSlice.h"
#include "OrbitsKernel.h"
#include "FluidsKernel.h"
#include "HarmonicsKernel.h"
#include "WavesKernel.h"
#include "RhythmKernel.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    const FLinearColor StarGold(1.0f, 0.85f, 0.25f);
    const FLinearColor PlanetBlue(0.35f, 0.6f, 1.0f);
    const FLinearColor FluidsCyan(0.25f, 0.7f, 1.0f);
    const FLinearColor HarmonicsViolet(0.75f, 0.45f, 1.0f);
    const FLinearColor WavesTeal(0.3f, 0.9f, 0.8f);
    const FLinearColor RhythmAmber(1.0f, 0.6f, 0.35f);
    const FLinearColor SeamGold(1.0f, 0.8f, 0.2f);
    constexpr double AstronomicalUnit = 1.496e11; // metres
    constexpr float EngineSphereDiameter = 100.0f; // /Engine/BasicShapes/Sphere is ~100uu
}

AManifoldRealmVisualizer::AManifoldRealmVisualizer()
{
    PrimaryActorTick.bCanEverTick = true;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    // Real renderable assets from the engine content (available at runtime).
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereFinder.Succeeded())
    {
        SphereMesh = SphereFinder.Object;
    }
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (MatFinder.Succeeded())
    {
        BaseMaterial = MatFinder.Object;
    }
}

void AManifoldRealmVisualizer::BeginFrame()
{
    Cursor = 0;
}

void AManifoldRealmVisualizer::EndFrame()
{
    // Hide any pooled components not used this frame.
    for (int32 i = Cursor; i < Pool.Num(); ++i)
    {
        if (Pool[i]) { Pool[i]->SetVisibility(false); }
    }
}

UStaticMeshComponent* AManifoldRealmVisualizer::AcquireSphere()
{
    UStaticMeshComponent* Comp = nullptr;
    if (Cursor < Pool.Num())
    {
        Comp = Pool[Cursor];
    }
    else
    {
        Comp = NewObject<UStaticMeshComponent>(this);
        Comp->SetupAttachment(SceneRoot);
        Comp->RegisterComponent();
        Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        if (SphereMesh) { Comp->SetStaticMesh(SphereMesh); }
        if (BaseMaterial)
        {
            Comp->SetMaterial(0, BaseMaterial);
            Comp->CreateAndSetMaterialInstanceDynamic(0); // per-instance color
        }
        Pool.Add(Comp);
    }

    if (Comp) { Comp->SetVisibility(true); }
    ++Cursor;
    return Comp;
}

void AManifoldRealmVisualizer::PlaceSphere(const FVector& WorldPos, float DiameterUnits, const FLinearColor& Color)
{
    UStaticMeshComponent* Comp = AcquireSphere();
    if (!Comp) return;

    Comp->SetWorldLocation(WorldPos);
    Comp->SetWorldScale3D(FVector(DiameterUnits / EngineSphereDiameter));
    if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Comp->GetMaterial(0)))
    {
        MID->SetVectorParameterValue(TEXT("Color"), Color);
    }
}

void AManifoldRealmVisualizer::PlaceRatioRealm(const FVector& Center, int32 P, int32 Q, const FLinearColor& Color)
{
    if (P <= 0 || Q <= 0) return;

    // Two beads sized in proportion to the ratio: the shape IS the ratio.
    const float Unit = 14.0f;
    PlaceSphere(Center + FVector(0.0, -55.0, 0.0), Unit * P, Color);
    PlaceSphere(Center + FVector(0.0,  55.0, 0.0), Unit * Q, Color);
}

void AManifoldRealmVisualizer::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UWorld* World = GetWorld();
    if (!World || !SphereMesh) return;

    AManifoldGameMode* GM = World->GetAuthGameMode<AManifoldGameMode>();
    if (!GM || !GM->Slice) return;
    UManifoldSlice* S = GM->Slice;

    BeginFrame();

    // --- Orbits: star + planets ---
    if (S->Orbits)
    {
        if (FOrbitsState* OS = static_cast<FOrbitsState*>(S->Orbits->GetState().Get()))
        {
            for (const FOrbitsBodyDef& Body : OS->Bodies)
            {
                const FVector P = OrbitsCenter
                    + FVector(Body.Position.X, Body.Position.Y, 0.0) / AstronomicalUnit * OrbitsScale;
                PlaceSphere(P, Body.bIsCentral ? 70.0f : 28.0f, Body.bIsCentral ? StarGold : PlanetBlue);
            }
        }
    }

    // --- Fluids: density field as a bead cloud ---
    if (S->Fluids)
    {
        if (FFluidsState* FS = static_cast<FFluidsState*>(S->Fluids->GetState().Get()))
        {
            const int32 N = FS->GridSize;
            if (N > 0 && FS->Density.Num() >= (N + 2) * (N + 2))
            {
                const int32 Stride = N + 2;
                const int32 Skip = FMath::Max(1, N / 16);
                for (int32 j = 1; j <= N; j += Skip)
                {
                    for (int32 i = 1; i <= N; i += Skip)
                    {
                        const float Density = FS->Density[i + Stride * j];
                        if (Density < 0.03f) continue;
                        const float U = static_cast<float>(i - 1) / N;
                        const float V = static_cast<float>(j - 1) / N;
                        const FVector P = FluidsCenter
                            + FVector((U - 0.5f) * 2.0f * FluidsExtent, (V - 0.5f) * 2.0f * FluidsExtent, 0.0f);
                        PlaceSphere(P, 10.0f + FMath::Clamp(Density, 0.0f, 1.0f) * 18.0f, FluidsCyan);
                    }
                }
            }
        }
    }

    // --- Ratio realms: two beads whose sizes are the integer ratio p:q ---
    if (S->Harmonics && S->Harmonics->GetActiveRatios().Num() > 0)
    {
        const FIntPoint R = S->Harmonics->GetActiveRatios()[0].Ratio;
        PlaceRatioRealm(HarmonicsCenter, R.X, R.Y, HarmonicsViolet);
    }
    if (S->Waves && S->Waves->GetActiveRatios().Num() > 0)
    {
        const FIntPoint R = S->Waves->GetActiveRatios()[0].Ratio;
        PlaceRatioRealm(WavesCenter, R.X, R.Y, WavesTeal);
    }
    if (S->Rhythm && S->Rhythm->GetActiveRatios().Num() > 0)
    {
        const FIntPoint R = S->Rhythm->GetActiveRatios()[0].Ratio;
        PlaceRatioRealm(RhythmCenter, R.X, R.Y, RhythmAmber);
    }

    // --- Seam: a bright bead-bridge from Orbits to Fluids when a correspondence is lit ---
    if (S->IsCorrespondenceAvailable())
    {
        const int32 Beads = 10;
        for (int32 b = 0; b <= Beads; ++b)
        {
            const float T = static_cast<float>(b) / Beads;
            PlaceSphere(FMath::Lerp(OrbitsCenter, FluidsCenter, T), 12.0f, SeamGold);
        }
    }

    EndFrame();
}
