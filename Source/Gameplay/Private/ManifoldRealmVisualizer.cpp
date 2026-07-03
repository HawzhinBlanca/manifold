// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "ManifoldRealmVisualizer.h"
#include "ManifoldGameMode.h"
#include "ManifoldSlice.h"
#include "OrbitsKernel.h"
#include "FluidsKernel.h"
#include "HarmonicsKernel.h"
#include "WavesKernel.h"
#include "RhythmKernel.h"
#include "GearsKernel.h"
#include "CircuitsKernel.h"
#include "ManifoldGearMesh.h"
#include "ManifoldWaveMesh.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "DeterministicRNG.h"
#include "UObject/ConstructorHelpers.h"
#include "ManifoldPalette.h"

namespace
{
    // Realm colors come from the central, colorblind-safe ManifoldPalette (Okabe-Ito), which is
    // locked by MANIFOLD.Art.PaletteColorblindSafe. The local aliases keep the rest of this file
    // unchanged while making the palette the single source of truth.
    const FLinearColor StarGold = ManifoldPalette::Star;
    const FLinearColor PlanetBlue = ManifoldPalette::Orbits;
    const FLinearColor FluidsCyan = ManifoldPalette::Fluids;
    const FLinearColor HarmonicsViolet = ManifoldPalette::Harmonics;
    const FLinearColor WavesTeal = ManifoldPalette::Waves;
    const FLinearColor RhythmAmber = ManifoldPalette::Rhythm;
    const FLinearColor GearsSteel = ManifoldPalette::Gears;
    const FLinearColor CircuitsColor = ManifoldPalette::Circuits;
    const FLinearColor DecoyGrey = ManifoldPalette::Decoy;
    const FLinearColor SeamGold = ManifoldPalette::Seam;
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
    // Unlit emissive material for the realm orbs — self-lit so they glow the palette colour and
    // bloom via the post-process, rather than reading as shaded solid balls.
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> EmissiveFinder(TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
    if (EmissiveFinder.Succeeded())
    {
        EmissiveMaterial = EmissiveFinder.Object;
    }
}

void AManifoldRealmVisualizer::BeginPlay()
{
    Super::BeginPlay();

    UWorld* World = GetWorld();
    if (World)
    {
        // Key + fill directional lighting so the mesh geometry is clearly visible
        // regardless of what (if any) lighting the startup map ships with.
        auto SpawnLight = [&](const FRotator& Rot, float Intensity, const FLinearColor& Col)
        {
            ADirectionalLight* Light = World->SpawnActor<ADirectionalLight>(
                ADirectionalLight::StaticClass(), FVector::ZeroVector, Rot);
            if (Light)
            {
                if (UDirectionalLightComponent* LC = Cast<UDirectionalLightComponent>(Light->GetLightComponent()))
                {
                    LC->SetMobility(EComponentMobility::Movable);
                    LC->SetIntensity(Intensity);
                    LC->SetLightColor(Col);
                }
            }
        };
        // Multi-angle rig so the realm spheres read as fully-lit coloured orbs rather than
        // half-shadowed crescents (no HDRI/skylight on this code-only, headless-cooked scene, so
        // we fake ambient with fills from several directions). Camera looks +Y at the grid.
        SpawnLight(FRotator(-40.0f, -30.0f, 0.0f), 5.0f, FLinearColor(1.0f, 0.97f, 0.9f));  // warm key
        SpawnLight(FRotator(10.0f, 150.0f, 0.0f), 3.0f, FLinearColor(0.55f, 0.68f, 1.0f));  // cool fill (opposite)
        SpawnLight(FRotator(-20.0f, -110.0f, 0.0f), 2.5f, FLinearColor(0.8f, 0.85f, 1.0f)); // camera-side fill
        SpawnLight(FRotator(35.0f, 60.0f, 0.0f), 2.0f, FLinearColor(0.9f, 0.9f, 0.95f));    // top-back fill
        SpawnLight(FRotator(-55.0f, 40.0f, 0.0f), 1.8f, FLinearColor(0.7f, 0.8f, 1.0f));    // low fill (lifts undersides)

        // Cinematic post: bloom so the gold correspondences / seam / stars glow, a soft
        // vignette, and fixed exposure so the scene doesn't auto-brighten/flicker.
        if (APostProcessVolume* PP = World->SpawnActor<APostProcessVolume>())
        {
            PP->bUnbound = true;
            FPostProcessSettings& S = PP->Settings;
            S.bOverride_BloomIntensity = true;             S.BloomIntensity = 1.2f;
            S.bOverride_BloomThreshold = true;             S.BloomThreshold = 0.6f;
            S.bOverride_VignetteIntensity = true;          S.VignetteIntensity = 0.5f;
            S.bOverride_AutoExposureMinBrightness = true;  S.AutoExposureMinBrightness = 1.0f;
            S.bOverride_AutoExposureMaxBrightness = true;  S.AutoExposureMaxBrightness = 1.0f; // fixed exposure
        }
    }

    SpawnStarfield();

    // Two persistent procedural cogs for the Gears realm (mesh rebuilt on tooth-count change).
    auto MakeCog = [&](const TCHAR* Name) -> UProceduralMeshComponent*
    {
        UProceduralMeshComponent* Cog = NewObject<UProceduralMeshComponent>(this, Name);
        Cog->SetupAttachment(SceneRoot);
        Cog->RegisterComponent();
        Cog->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        // Emissive so the gear cogs / wave ribbons glow to match the realm orbs (fallback to lit).
        UMaterialInterface* CogMat = EmissiveMaterial ? EmissiveMaterial : BaseMaterial;
        if (CogMat) { Cog->CreateDynamicMaterialInstance(0, CogMat); }
        Cog->SetVisibility(false);
        return Cog;
    };
    GearCogP = MakeCog(TEXT("GearCogP"));
    GearCogQ = MakeCog(TEXT("GearCogQ"));
    WaveRibbonP = MakeCog(TEXT("WaveRibbonP")); // same procedural-mesh setup, different geometry
    WaveRibbonQ = MakeCog(TEXT("WaveRibbonQ"));
}

void AManifoldRealmVisualizer::ApplyGlow(UMaterialInstanceDynamic* MID, const FLinearColor& Color,
    float Pulse, float GlowMultiplier)
{
    if (!MID) return;
    // The emissive material is unlit, so push the emissive above 1.0 for a self-lit glow that reads
    // (and blooms) against the black; BaseColor carries the lit variant, and the four common param
    // names cover whatever the bound material calls them (missing params are harmless no-ops).
    const FLinearColor Lit = Color * Pulse;
    const FLinearColor Glow = Lit * GlowMultiplier;
    MID->SetVectorParameterValue(TEXT("Color"), Glow);
    MID->SetVectorParameterValue(TEXT("BaseColor"), Lit);
    MID->SetVectorParameterValue(TEXT("EmissiveColor"), Glow);
    MID->SetVectorParameterValue(TEXT("Emissive"), Glow);
}

void AManifoldRealmVisualizer::UpdateWaveRibbon(UProceduralMeshComponent* Ribbon, int32 Harmonic,
    int32& LastHarmonic, const FVector& Pos, const FLinearColor& Color)
{
    if (!Ribbon) return;
    Ribbon->SetVisibility(true);
    Ribbon->SetWorldLocation(Pos);
    Ribbon->SetWorldRotation(FRotator::ZeroRotator);

    if (Harmonic != LastHarmonic)
    {
        LastHarmonic = Harmonic;
        TArray<FVector> V; TArray<int32> Tris;
        ManifoldWaveMesh::Build(Harmonic, 150.0f, 34.0f, 4.0f, V, Tris);
        const TArray<FVector> NoNormals;
        const TArray<FVector2D> NoUV;
        const TArray<FLinearColor> NoColors;
        const TArray<FProcMeshTangent> NoTangents;
        Ribbon->CreateMeshSection_LinearColor(0, V, Tris, NoNormals, NoUV, NoColors, NoTangents, false);
    }

    if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Ribbon->GetMaterial(0)))
    {
        ApplyGlow(MID, Color, PulseFactor);
    }
}

void AManifoldRealmVisualizer::UpdateGearCog(UProceduralMeshComponent* Cog, int32 Teeth,
    int32& LastTeeth, const FVector& Pos, float Spin, const FLinearColor& Color)
{
    if (!Cog) return;
    Cog->SetVisibility(true);
    Cog->SetWorldLocation(Pos);
    // Stand the cog up facing the camera (depth axis) and spin it; a cog with more teeth
    // turns slower, as meshed gears do.
    const float RollDeg = FMath::RadiansToDegrees(Spin / static_cast<float>(FMath::Max(1, Teeth)));
    Cog->SetWorldRotation(FRotator(90.0f, 0.0f, RollDeg));

    // Rebuild the mesh only when the tooth count changes (the teeth ARE the ratio integer).
    if (Teeth != LastTeeth)
    {
        LastTeeth = Teeth;
        TArray<FVector> V; TArray<int32> Tris;
        ManifoldGearMesh::Build(Teeth, 34.0f, 12.0f, 8.0f, V, Tris);
        const TArray<FVector> NoNormals;
        const TArray<FVector2D> NoUV;
        const TArray<FLinearColor> NoColors;
        const TArray<FProcMeshTangent> NoTangents;
        Cog->CreateMeshSection_LinearColor(0, V, Tris, NoNormals, NoUV, NoColors, NoTangents, false);
    }

    if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Cog->GetMaterial(0)))
    {
        ApplyGlow(MID, Color, PulseFactor);
    }
}

void AManifoldRealmVisualizer::SpawnStarfield()
{
    if (!SphereMesh) return;

    // Deterministic backdrop: a fixed-seed shell of star points around the scene.
    FDeterministicRNG Rng(0x4D414E5354415253ULL); // 'MANSTARS'
    const FVector Center(0.0, 0.0, 300.0);

    for (int32 i = 0; i < StarCount; ++i)
    {
        const FVector Dir = Rng.VRand();
        const float Radius = Rng.FRandRange(2600.0f, 4200.0f);
        const float Size = Rng.FRandRange(4.0f, 16.0f);
        const float Bright = Rng.FRandRange(0.3f, 1.0f);

        // Mostly white; a few pale blue or warm gold for variety.
        FLinearColor Col(Bright, Bright, Bright);
        const float Tint = Rng.FRand();
        if (Tint < 0.18f) { Col = FLinearColor(Bright * 0.6f, Bright * 0.8f, Bright); }
        else if (Tint > 0.88f) { Col = FLinearColor(Bright, Bright * 0.85f, Bright * 0.5f); }

        UStaticMeshComponent* Star = NewObject<UStaticMeshComponent>(this);
        Star->SetupAttachment(SceneRoot);
        Star->RegisterComponent();
        Star->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Star->SetStaticMesh(SphereMesh);
        // Stars GLOW (emissive) instead of being dim lit dots that depend on the scene lights —
        // brighter ones bloom via the post-process, giving the backdrop real depth.
        UMaterialInterface* StarMat = EmissiveMaterial ? EmissiveMaterial : BaseMaterial;
        if (StarMat)
        {
            Star->SetMaterial(0, StarMat);
            if (UMaterialInstanceDynamic* MID = Star->CreateAndSetMaterialInstanceDynamic(0))
            {
                // Stars don't pulse; a fixed 1.3x emissive gives the brighter ones their bloom.
                ApplyGlow(MID, Col, 1.0f, 1.3f);
            }
        }
        Star->SetWorldLocation(Center + Dir * Radius);
        Star->SetWorldScale3D(FVector(Size / 100.0f));
        Stars.Add(Star);
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
        UMaterialInterface* SphereMat = EmissiveMaterial ? EmissiveMaterial : BaseMaterial;
        if (SphereMat)
        {
            Comp->SetMaterial(0, SphereMat);
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
        ApplyGlow(MID, Color, PulseFactor);
    }
}

void AManifoldRealmVisualizer::PlaceRatioRealm(const FVector& Center, int32 P, int32 Q, const FLinearColor& Color)
{
    if (P <= 0 || Q <= 0) return;

    // Two beads sized in proportion to the ratio (the shape IS the ratio), gently
    // orbiting each other in the plane facing the camera so the pairing feels alive.
    const float Unit = 22.0f;
    const float Sep = 75.0f;
    const FVector Off(0.0, Sep * FMath::Cos(SpinAngle), Sep * FMath::Sin(SpinAngle));
    PlaceSphere(Center + Off, Unit * P, Color);
    PlaceSphere(Center - Off, Unit * Q, Color);
}

void AManifoldRealmVisualizer::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UWorld* World = GetWorld();
    if (!World || !SphereMesh) return;

    AManifoldGameMode* GM = World->GetAuthGameMode<AManifoldGameMode>();
    if (!GM || !GM->Slice) return;
    UManifoldSlice* S = GM->Slice;

    SpinAngle += DeltaSeconds * 0.6f; // gentle orbit of the ratio pairs

    // Discovery flash: when the total discoveries tick up, brighten the scene briefly
    // (the bloom post makes it read as a pulse of insight).
    const int32 DiscoveriesNow = S->GetTotalDiscoveries();
    if (DiscoveriesNow > LastDiscoveryCount) { PulseTimer = 0.5f; }
    LastDiscoveryCount = DiscoveriesNow;
    PulseTimer = FMath::Max(0.0f, PulseTimer - DeltaSeconds);
    PulseFactor = 1.0f + 1.3f * (PulseTimer / 0.5f);

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

                // Trace each planet's orbital PATH as a faint ring — the orbit (the
                // periodic structure the resonance lives in) made visible.
                if (!Body.bIsCentral)
                {
                    const float OrbitR = FVector(Body.Position.X, Body.Position.Y, 0.0).Size()
                        / AstronomicalUnit * OrbitsScale;
                    const int32 PathBeads = 32;
                    for (int32 k = 0; k < PathBeads; ++k)
                    {
                        const float A = 2.0f * PI * static_cast<float>(k) / PathBeads;
                        PlaceSphere(OrbitsCenter + FVector(FMath::Cos(A) * OrbitR, FMath::Sin(A) * OrbitR, 0.0f),
                            5.0f, FLinearColor(0.28f, 0.36f, 0.6f));
                    }
                }
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
    // Waves: two standing-wave ribbons whose hump counts (harmonics) are the ratio.
    if (S->Waves && S->Waves->GetActiveRatios().Num() > 0)
    {
        const FIntPoint R = S->Waves->GetActiveRatios()[0].Ratio;
        UpdateWaveRibbon(WaveRibbonP, R.X, LastWaveP, WavesCenter + FVector(0.0, 0.0, 55.0), WavesTeal);
        UpdateWaveRibbon(WaveRibbonQ, R.Y, LastWaveQ, WavesCenter - FVector(0.0, 0.0, 55.0), WavesTeal);
    }
    else
    {
        if (WaveRibbonP) { WaveRibbonP->SetVisibility(false); }
        if (WaveRibbonQ) { WaveRibbonQ->SetVisibility(false); }
    }
    if (S->Rhythm && S->Rhythm->GetActiveRatios().Num() > 0)
    {
        const FIntPoint R = S->Rhythm->GetActiveRatios()[0].Ratio;
        PlaceRatioRealm(RhythmCenter, R.X, R.Y, RhythmAmber);
    }
    // Gears: two real, code-generated meshing cogs whose tooth counts are the ratio.
    if (S->Gears && S->Gears->GetActiveRatios().Num() > 0)
    {
        const FIntPoint R = S->Gears->GetActiveRatios()[0].Ratio;
        const FVector Off(0.0, 60.0, 0.0);
        UpdateGearCog(GearCogP, R.X, LastGearP, GearsCenter + Off,  SpinAngle * 4.0f, GearsSteel);
        UpdateGearCog(GearCogQ, R.Y, LastGearQ, GearsCenter - Off, -SpinAngle * 4.0f, GearsSteel);
    }
    else
    {
        if (GearCogP) { GearCogP->SetVisibility(false); }
        if (GearCogQ) { GearCogQ->SetVisibility(false); }
    }
    if (S->Circuits && S->Circuits->GetActiveRatios().Num() > 0)
    {
        const FIntPoint R = S->Circuits->GetActiveRatios()[0].Ratio;
        PlaceRatioRealm(CircuitsCenter, R.X, R.Y, CircuitsColor);
    }
    // The decoy realm, set apart (above) and grey — a visible red herring.
    if (S->Decoy && S->Decoy->GetActiveRatios().Num() > 0)
    {
        const FIntPoint R = S->Decoy->GetActiveRatios()[0].Ratio;
        PlaceRatioRealm(DecoyCenter, R.X, R.Y, DecoyGrey);
    }

    // --- Seam: the "carry it across the seam" money shot. A glowing golden ARC from Orbits to
    //     Fluids when a correspondence is lit, with a pulse of light travelling across it. ---
    if (S->IsCorrespondenceAvailable())
    {
        const int32 Beads = 24;
        const float ArcHeight = 220.0f;
        const float PulsePhase = FMath::Frac(SpinAngle * 0.35f); // 0..1 sweep along the seam
        for (int32 b = 0; b <= Beads; ++b)
        {
            const float T = static_cast<float>(b) / Beads;
            FVector P = FMath::Lerp(OrbitsCenter, FluidsCenter, T);
            P.Z += ArcHeight * FMath::Sin(T * PI); // bow the bridge upward into an arc
            // A travelling brightness pulse: beads near the phase flare (bloom via post).
            const float Pulse = FMath::Max(0.0f, 1.0f - FMath::Abs(T - PulsePhase) * 6.0f);
            PlaceSphere(P, 9.0f + 11.0f * Pulse, SeamGold * (1.0f + 1.6f * Pulse));
        }
    }

    EndFrame();
}
