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
#include "KismetProceduralMeshLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/ExponentialHeightFog.h"
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
    // Authored energy-sphere material (M_RealmOrb): an unlit emissive with a Fresnel rim so each
    // realm reads as a glowing 3D sphere with a hot edge, not a flat disc. Driven by the "Color"
    // param the visualizer's ApplyGlow already sets. Falls back to the engine emissive if missing.
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> EmissiveFinder(TEXT("/Game/Materials/M_RealmOrb.M_RealmOrb"));
    if (EmissiveFinder.Succeeded())
    {
        EmissiveMaterial = EmissiveFinder.Object;
    }
    else
    {
        static ConstructorHelpers::FObjectFinder<UMaterialInterface> EmissiveFallback(TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
        if (EmissiveFallback.Succeeded()) { EmissiveMaterial = EmissiveFallback.Object; }
    }
    // Procedural nebula backdrop material (two-sided, unlit) for a giant inside-out shell.
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> NebulaFinder(TEXT("/Game/Materials/M_Nebula.M_Nebula"));
    if (NebulaFinder.Succeeded())
    {
        NebulaMaterial = NebulaFinder.Object;
    }
    // Real deep-space HDRI backdrop (public-domain NASA Milky Way panorama on the sky shell). Preferred
    // over the procedural nebula when present; authored by Tools/Art/manifold_materials.py. Falls back
    // to M_Nebula if the sky material/texture is absent.
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> SkyFinder(TEXT("/Game/Materials/M_SkyDome.M_SkyDome"));
    if (SkyFinder.Succeeded())
    {
        SkyMaterial = SkyFinder.Object;
    }
    // Hot star material for the Orbits centre (brighter core/rim than a realm orb).
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> StarFinder(TEXT("/Game/Materials/M_Star.M_Star"));
    if (StarFinder.Succeeded())
    {
        StarMaterial = StarFinder.Object;
    }
    // Lit brass PBR (M_Metal) for the gear cogs — real machined mechanism rather than energy glow.
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> MetalFinder(TEXT("/Game/Materials/M_Metal.M_Metal"));
    if (MetalFinder.Succeeded())
    {
        MetalMaterial = MetalFinder.Object;
    }
    // Scrolling standing-wave ribbon material (M_Wave) — energy flows along the wave's length.
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> WaveFinder(TEXT("/Game/Materials/M_Wave.M_Wave"));
    if (WaveFinder.Succeeded())
    {
        WaveMaterial = WaveFinder.Object;
    }
}

void AManifoldRealmVisualizer::BeginPlay()
{
    Super::BeginPlay();

    UWorld* World = GetWorld();
    if (World)
    {
        // ---------------- Cinematic cosmic lighting & atmosphere ----------------
        // A dramatic directional key + cool back-rim (both feeding the volumetric fog so the
        // haze catches light), a deep nebula fog the realms glow INTO for real depth, and a
        // coloured point light at each realm so every world casts its palette hue through the mist.

        auto SpawnDir = [&](const FRotator& Rot, float Intensity, const FLinearColor& Col, float VolScatter)
        {
            if (ADirectionalLight* Light = World->SpawnActor<ADirectionalLight>(
                    ADirectionalLight::StaticClass(), FVector::ZeroVector, Rot))
            {
                if (UDirectionalLightComponent* LC = Cast<UDirectionalLightComponent>(Light->GetLightComponent()))
                {
                    LC->SetMobility(EComponentMobility::Movable);
                    LC->SetIntensity(Intensity);
                    LC->SetLightColor(Col);
                    LC->SetVolumetricScatteringIntensity(VolScatter);
                }
            }
        };
        SpawnDir(FRotator(-32.0f, -35.0f, 0.0f), 3.5f, FLinearColor(1.00f, 0.95f, 0.86f), 2.0f); // warm key
        SpawnDir(FRotator(-14.0f, 152.0f, 0.0f), 2.2f, FLinearColor(0.42f, 0.58f, 1.00f), 2.6f); // cool back-rim
        SpawnDir(FRotator(-60.0f,  55.0f, 0.0f), 1.1f, FLinearColor(0.70f, 0.80f, 1.00f), 1.0f); // soft top fill

        // Deep nebula haze — a volumetric fog the realms and their coloured lights glow into.
        if (AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(
                AExponentialHeightFog::StaticClass(), FVector(0.0, 0.0, -600.0), FRotator::ZeroRotator))
        {
            if (UExponentialHeightFogComponent* FC = Fog->GetComponent())
            {
                FC->SetMobility(EComponentMobility::Movable);
                FC->SetFogDensity(0.004f);                                // thin — keep the void dark
                FC->SetFogHeightFalloff(0.02f);
                FC->SetFogInscatteringColor(FLinearColor(0.006f, 0.012f, 0.030f)); // near-black blue (no wash)
                FC->SetStartDistance(450.0f);
                FC->SetVolumetricFog(true);
                FC->SetVolumetricFogScatteringDistribution(0.2f);
                FC->SetVolumetricFogAlbedo(FColor(140, 160, 230));
                FC->SetVolumetricFogExtinctionScale(3.0f);                // dense volumetric glow near lights only
                FC->SetVolumetricFogDistance(7000.0f);
            }
        }

        // Per-realm coloured point lights: each world casts its palette colour into the fog.
        auto SpawnRealmLight = [&](const FVector& Pos, const FLinearColor& Col, float Intensity)
        {
            if (APointLight* PL = World->SpawnActor<APointLight>(
                    APointLight::StaticClass(), Pos, FRotator::ZeroRotator))
            {
                if (UPointLightComponent* PC = Cast<UPointLightComponent>(PL->GetLightComponent()))
                {
                    PC->SetMobility(EComponentMobility::Movable);
                    PC->SetLightColor(Col);
                    PC->SetIntensity(Intensity);
                    PC->SetAttenuationRadius(650.0f);
                    PC->SetSourceRadius(24.0f);
                    PC->SetVolumetricScatteringIntensity(6.5f); // glow through the fog
                }
            }
        };
        SpawnRealmLight(OrbitsCenter,    StarGold,        12000.0f); // the star anchors the cluster
        SpawnRealmLight(FluidsCenter,    FluidsCyan,       7000.0f);
        SpawnRealmLight(HarmonicsCenter, HarmonicsViolet,  7000.0f);
        SpawnRealmLight(WavesCenter,     WavesTeal,        7000.0f);
        SpawnRealmLight(RhythmCenter,    RhythmAmber,      7000.0f);
        SpawnRealmLight(GearsCenter,     GearsSteel,       7000.0f);
        SpawnRealmLight(CircuitsCenter,  CircuitsColor,    7000.0f);

        // Film-grade post: cinematic depth of field on the realm cluster, generous bloom for the
        // emissive orbs + gold seam, a cool colour grade, a touch of grain + chromatic aberration,
        // and fixed exposure so the scene never flickers.
        if (APostProcessVolume* PP = World->SpawnActor<APostProcessVolume>())
        {
            PP->bUnbound = true;
            FPostProcessSettings& S = PP->Settings;
            S.bOverride_BloomIntensity = true;             S.BloomIntensity = 1.6f;
            S.bOverride_BloomThreshold = true;             S.BloomThreshold = 0.35f;
            S.bOverride_AutoExposureMinBrightness = true;  S.AutoExposureMinBrightness = 1.0f;
            S.bOverride_AutoExposureMaxBrightness = true;  S.AutoExposureMaxBrightness = 1.0f; // fixed exposure
            // Depth of field — realms sharp (~1200uu ahead), the far starfield softly out of focus.
            S.bOverride_DepthOfFieldFocalDistance = true;  S.DepthOfFieldFocalDistance = 1200.0f;
            S.bOverride_DepthOfFieldFstop = true;          S.DepthOfFieldFstop = 4.0f;
            // Colour grade — gently lifted saturation, cool bias, filmic.
            S.bOverride_ColorSaturation = true;            S.ColorSaturation = FVector4(1.12f, 1.12f, 1.20f, 1.0f);
            S.bOverride_ColorContrast = true;              S.ColorContrast = FVector4(1.05f, 1.05f, 1.10f, 1.0f);
            S.bOverride_WhiteTemp = true;                  S.WhiteTemp = 5400.0f; // slightly cool
            S.bOverride_SceneColorTint = true;             S.SceneColorTint = FLinearColor(0.93f, 0.96f, 1.06f);
            // Filmic texture.
            S.bOverride_FilmGrainIntensity = true;         S.FilmGrainIntensity = 0.16f;
            S.bOverride_SceneFringeIntensity = true;       S.SceneFringeIntensity = 0.5f;
            S.bOverride_VignetteIntensity = true;          S.VignetteIntensity = 0.42f;
        }
    }

    // Giant inside-out sky shell — a real cosmic backdrop instead of empty black. Prefer the NASA
    // Milky Way HDRI (M_SkyDome) when present; fall back to the procedural nebula.
    UMaterialInterface* BackdropMat = SkyMaterial ? SkyMaterial : NebulaMaterial;
    if (SphereMesh && BackdropMat)
    {
        Backdrop = NewObject<UStaticMeshComponent>(this, TEXT("NebulaBackdrop"));
        Backdrop->SetupAttachment(SceneRoot);
        Backdrop->RegisterComponent();
        Backdrop->SetStaticMesh(SphereMesh);
        Backdrop->SetMaterial(0, BackdropMat);
        Backdrop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Backdrop->SetCastShadow(false);
        Backdrop->SetWorldLocation(FVector(0.0, 0.0, 300.0));
        Backdrop->SetWorldScale3D(FVector(220.0f)); // ~11000uu radius shell around the whole scene
    }

    // Dedicated hot sun for the Orbits centre (its own brighter M_Star material).
    if (SphereMesh)
    {
        StarComp = NewObject<UStaticMeshComponent>(this, TEXT("OrbitsStar"));
        StarComp->SetupAttachment(SceneRoot);
        StarComp->RegisterComponent();
        StarComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        StarComp->SetStaticMesh(SphereMesh);
        UMaterialInterface* SM = StarMaterial ? StarMaterial : EmissiveMaterial;
        if (SM) { StarComp->SetMaterial(0, SM); StarComp->CreateAndSetMaterialInstanceDynamic(0); }
        StarComp->SetVisibility(false);
    }

    SpawnStarfield();

    // Two persistent procedural cogs for the Gears realm (mesh rebuilt on tooth-count change).
    auto MakeCog = [&](const TCHAR* Name, UMaterialInterface* Mat) -> UProceduralMeshComponent*
    {
        UProceduralMeshComponent* Cog = NewObject<UProceduralMeshComponent>(this, Name);
        Cog->SetupAttachment(SceneRoot);
        Cog->RegisterComponent();
        Cog->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        UMaterialInterface* CogMat = Mat ? Mat : (EmissiveMaterial ? EmissiveMaterial : BaseMaterial);
        if (CogMat) { Cog->CreateDynamicMaterialInstance(0, CogMat); }
        Cog->SetVisibility(false);
        return Cog;
    };
    // Gear cogs use lit brass PBR (M_Metal); wave ribbons stay unlit-emissive to glow like the orbs.
    UMaterialInterface* const CogMat = MetalMaterial ? MetalMaterial : EmissiveMaterial;
    GearCogP = MakeCog(TEXT("GearCogP"), CogMat);
    GearCogQ = MakeCog(TEXT("GearCogQ"), CogMat);
    UMaterialInterface* const RibbonMat = WaveMaterial ? WaveMaterial : EmissiveMaterial;
    WaveRibbonP = MakeCog(TEXT("WaveRibbonP"), RibbonMat); // same procedural-mesh setup, different geometry
    WaveRibbonQ = MakeCog(TEXT("WaveRibbonQ"), RibbonMat);
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
        // UVs so the M_Wave panner scrolls its emissive band along the ribbon's LENGTH: U=0..1 from X
        // (the ribbon spans X = -75..+75 for Length 150). Unlit material -> no normals needed.
        TArray<FVector2D> UV; UV.Reserve(V.Num());
        for (const FVector& Vtx : V) { UV.Add(FVector2D((Vtx.X + 75.0f) / 150.0f, 0.0f)); }
        const TArray<FVector> NoNormals;
        const TArray<FLinearColor> NoColors;
        const TArray<FProcMeshTangent> NoTangents;
        Ribbon->CreateMeshSection_LinearColor(0, V, Tris, NoNormals, UV, NoColors, NoTangents, false);
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
        // The lit brass material needs normals + tangents to shade + catch specular (the mesh ships
        // none -> a lit material renders black). Planar UVs feed the tangent solve; normals are
        // computed smooth from the geometry.
        TArray<FVector2D> UV; UV.Reserve(V.Num());
        for (const FVector& Vtx : V) { UV.Add(FVector2D(Vtx.X * 0.02f, Vtx.Y * 0.02f)); }
        TArray<FVector> Normals;
        TArray<FProcMeshTangent> Tangents;
        UKismetProceduralMeshLibrary::CalculateTangentsForMesh(V, Tris, UV, Normals, Tangents);
        const TArray<FLinearColor> NoColors;
        Cog->CreateMeshSection_LinearColor(0, V, Tris, Normals, UV, NoColors, Tangents, false);
    }

    if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Cog->GetMaterial(0)))
    {
        // Brass PBR: set the base 'Color' to the realm's identity colour at unit brightness (the metal
        // reflects the scene + gears point light; M_Metal's own emissive keeps it legible). A subtle
        // member-flash lift via PulseFactor, but NOT the unlit glow-boost (which would clip the albedo).
        MID->SetVectorParameterValue(TEXT("Color"), Color * FMath::Min(1.0f, 0.9f * PulseFactor));
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
        // Depth: a few big, bright "hero" stars nearer the front over a faint distant field,
        // rather than a flat wash of uniform dots.
        const bool bHero = (Rng.FRand() < 0.06f);
        const float Radius = bHero ? Rng.FRandRange(2200.0f, 3100.0f) : Rng.FRandRange(2900.0f, 4400.0f);
        const float Size   = bHero ? Rng.FRandRange(20.0f, 42.0f)     : Rng.FRandRange(3.0f, 11.0f);
        const float Bright = bHero ? Rng.FRandRange(1.1f, 1.7f)       : Rng.FRandRange(0.22f, 0.85f);

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

void AManifoldRealmVisualizer::UpdateStar(const FVector& Pos, float DiameterUnits)
{
    if (!StarComp) return;
    StarComp->SetVisibility(true);
    StarComp->SetWorldLocation(Pos);
    StarComp->SetWorldScale3D(FVector(DiameterUnits / EngineSphereDiameter));
    if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(StarComp->GetMaterial(0)))
    {
        ApplyGlow(MID, StarGold, PulseFactor, 1.9f); // hotter glow than a ratio orb
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

void AManifoldRealmVisualizer::SpawnVfxBurst(const FVector& Center, int32 Count, float Speed,
    float Life, float Size, const FLinearColor& Color)
{
    // Hard cap so a long-lived correspondence can't grow the particle set without bound.
    if (VfxParticles.Num() > 600) { return; }
    for (int32 k = 0; k < Count; ++k)
    {
        // Even spherical spray via the golden angle (deterministic — no RNG, so renders reproduce).
        const float Frac = (static_cast<float>(k) + 0.5f) / static_cast<float>(FMath::Max(1, Count));
        const float Phi = FMath::Acos(1.0f - 2.0f * Frac);       // polar 0..pi
        const float Theta = 2.399963f * static_cast<float>(k);   // golden angle
        const float SinPhi = FMath::Sin(Phi);
        const FVector Dir(SinPhi * FMath::Cos(Theta), SinPhi * FMath::Sin(Theta), FMath::Cos(Phi));
        FManifoldVfxParticle P;
        P.Pos = Center;
        P.Vel = Dir * Speed * (0.55f + 0.45f * Frac);
        P.Age = 0.0f;
        P.Life = Life;
        P.Size = Size;
        P.Color = Color;
        VfxParticles.Add(P);
    }
}

void AManifoldRealmVisualizer::UpdateAndDrawVfx(float DeltaSeconds)
{
    for (int32 i = VfxParticles.Num() - 1; i >= 0; --i)
    {
        FManifoldVfxParticle& P = VfxParticles[i];
        P.Age += DeltaSeconds;
        if (P.Age >= P.Life)
        {
            VfxParticles.RemoveAtSwap(i);
            continue;
        }
        P.Pos += P.Vel * DeltaSeconds;
        P.Vel *= 0.94f; // gentle drag so sparks decelerate as they fade
        const float T = 1.0f - (P.Age / P.Life); // 1 -> 0
        // Shrink + dim over life; the bright young sparks bloom via the post-process.
        PlaceSphere(P.Pos, P.Size * (0.35f + 0.65f * T), P.Color * (0.25f + 1.5f * T));
    }
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
    if (DiscoveriesNow > LastDiscoveryCount)
    {
        PulseTimer = 0.5f;
        // Energy burst: a spray of golden sparks radiating from the Orbits hub as the
        // correspondence surfaces — the insight lands as a visible pop, not just a brightening.
        SpawnVfxBurst(OrbitsCenter, 28, 330.0f, 1.1f, 8.0f, SeamGold);
    }
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
                if (Body.bIsCentral) { UpdateStar(P, 84.0f); }
                else { PlaceSphere(P, 28.0f, PlanetBlue); }

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

    // --- Seam: the "carry it across the seam" money shot. A glowing golden energy ARC from Orbits
    //     to Fluids when a correspondence is lit, with a bright energy "packet" racing across it. ---
    if (S->IsCorrespondenceAvailable())
    {
        // Dense beads read as a continuous beam rather than dotted; a wide, hot travelling packet
        // (a comet of light that blooms via post) sells the transport as energy flowing across.
        const int32 Beads = 52;
        const float ArcHeight = 230.0f;
        const float PulsePhase = FMath::Frac(SpinAngle * 0.5f); // 0..1 sweep, a touch faster
        for (int32 b = 0; b <= Beads; ++b)
        {
            const float T = static_cast<float>(b) / Beads;
            FVector P = FMath::Lerp(OrbitsCenter, FluidsCenter, T);
            P.Z += ArcHeight * FMath::Sin(T * PI); // bow the bridge upward into an arc
            const float Pulse = FMath::Max(0.0f, 1.0f - FMath::Abs(T - PulsePhase) * 4.0f); // wider packet
            PlaceSphere(P, 8.0f + 18.0f * Pulse, SeamGold * (1.15f + 2.6f * Pulse)); // brighter beam + hot packet
        }

        // Sparks crackle off the travelling energy packet — hot embers that fly UP/OUT clearly away
        // from the beam (so they read as sparks, not more beam beads) and fade over ~1s.
        FVector Pp = FMath::Lerp(OrbitsCenter, FluidsCenter, PulsePhase);
        Pp.Z += ArcHeight * FMath::Sin(PulsePhase * PI);
        for (int32 s = 0; s < 3; ++s)
        {
            const float a = SpinAngle * 3.1f + static_cast<float>(VfxSpawnCounter) * 0.7f;
            const FVector V(FMath::Cos(a) * 95.0f, FMath::Sin(a) * 48.0f, 120.0f + 65.0f * FMath::Sin(a * 1.7f));
            FManifoldVfxParticle Sp;
            Sp.Pos = Pp; Sp.Vel = V; Sp.Life = 0.95f; Sp.Size = 6.0f;
            Sp.Color = FLinearColor(1.0f, 0.86f, 0.5f) * 1.8f; // hot white-gold ember (contrasts the beam)
            VfxParticles.Add(Sp);
            ++VfxSpawnCounter;
        }
    }

    // Advance + draw the energy-VFX particles (bursts + seam sparks) as fading glowing beads.
    UpdateAndDrawVfx(DeltaSeconds);
    EndFrame();
}
