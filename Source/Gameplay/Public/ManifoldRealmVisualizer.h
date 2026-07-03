// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ManifoldRealmVisualizer.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UProceduralMeshComponent;

/**
 * AManifoldRealmVisualizer — a REAL 3D view of the live realms, built from spawned
 * static-mesh geometry (not debug lines), so the correspondence is visible as solid
 * objects in the world:
 *   - Orbits: a star + orbiting planets.
 *   - Fluids: the density field as a cloud of beads.
 *   - Harmonics / Waves / Rhythm: two beads whose SIZES are the integer ratio (e.g.
 *     a "3" bead beside a "2" bead), so the shared 3:2 is literally the same shape
 *     across all the ratio realms.
 *   - A bright bead-bridge across the seam when a correspondence is lit.
 *
 * Geometry is pooled and updated each frame from kernel state. Spawned by
 * AManifoldGameMode. (Art-directed materials/VFX are layered on top later; this is
 * real renderable geometry, not a debug overlay.)
 */
UCLASS()
class MANIFOLDGAMEPLAY_API AManifoldRealmVisualizer : public AActor
{
    GENERATED_BODY()

public:
    AManifoldRealmVisualizer();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, Category = "MANIFOLD") FVector OrbitsCenter    = FVector(-900.0, 0.0, 250.0);
    UPROPERTY(EditAnywhere, Category = "MANIFOLD") FVector FluidsCenter    = FVector(-450.0, 0.0, 250.0);
    UPROPERTY(EditAnywhere, Category = "MANIFOLD") FVector HarmonicsCenter = FVector(   0.0, 0.0, 250.0);
    UPROPERTY(EditAnywhere, Category = "MANIFOLD") FVector WavesCenter     = FVector( 450.0, 0.0, 250.0);
    UPROPERTY(EditAnywhere, Category = "MANIFOLD") FVector RhythmCenter    = FVector( 900.0, 0.0, 250.0);
    UPROPERTY(EditAnywhere, Category = "MANIFOLD") FVector GearsCenter     = FVector(-450.0, 0.0, 560.0);
    UPROPERTY(EditAnywhere, Category = "MANIFOLD") FVector CircuitsCenter  = FVector(   0.0, 0.0, 560.0);
    UPROPERTY(EditAnywhere, Category = "MANIFOLD") FVector DecoyCenter     = FVector( 450.0, 0.0, 560.0);

    /** World units per astronomical unit (Orbits are ~1e11 m). */
    UPROPERTY(EditAnywhere, Category = "MANIFOLD") float OrbitsScale = 180.0f;

    /** Half-size of the Fluids field, in world units. */
    UPROPERTY(EditAnywhere, Category = "MANIFOLD") float FluidsExtent = 160.0f;

protected:
    UPROPERTY() UStaticMesh* SphereMesh = nullptr;
    UPROPERTY() UMaterialInterface* BaseMaterial = nullptr;
    UPROPERTY() UMaterialInterface* EmissiveMaterial = nullptr; // glowing realm orbs (unlit, blooms)
    UPROPERTY() UMaterialInterface* NebulaMaterial = nullptr;   // procedural nebula backdrop
    UPROPERTY() UStaticMeshComponent* Backdrop = nullptr;       // giant inside-out nebula shell
    UPROPERTY() UMaterialInterface* StarMaterial = nullptr;     // hot star material (M_Star)
    UPROPERTY() UStaticMeshComponent* StarComp = nullptr;       // the Orbits-centre sun
    UPROPERTY() USceneComponent* SceneRoot = nullptr;
    UPROPERTY() TArray<UStaticMeshComponent*> Pool;

    /** Persistent procedural starfield (spawned once) — the cosmic backdrop. */
    UPROPERTY() TArray<UStaticMeshComponent*> Stars;

    /** The Gears realm rendered as two real, code-generated meshing cogs whose TOOTH
     *  COUNTS are the ratio (a P-tooth cog beside a Q-tooth cog IS the P:Q). */
    UPROPERTY() UProceduralMeshComponent* GearCogP = nullptr;
    UPROPERTY() UProceduralMeshComponent* GearCogQ = nullptr;
    int32 LastGearP = -1;
    int32 LastGearQ = -1;

    /** Position + spin a cog, rebuilding its mesh only when the tooth count changes. */
    void UpdateGearCog(UProceduralMeshComponent* Cog, int32 Teeth, int32& LastTeeth,
                       const FVector& Pos, float Spin, const FLinearColor& Color);

    /** The Waves realm rendered as two standing-wave ribbons whose HARMONIC (hump count)
     *  is the ratio (a P-hump ribbon beside a Q-hump ribbon IS the P:Q). */
    UPROPERTY() UProceduralMeshComponent* WaveRibbonP = nullptr;
    UPROPERTY() UProceduralMeshComponent* WaveRibbonQ = nullptr;
    int32 LastWaveP = -1;
    int32 LastWaveQ = -1;

    /** Position a ribbon, rebuilding its mesh only when the harmonic changes. */
    void UpdateWaveRibbon(UProceduralMeshComponent* Ribbon, int32 Harmonic, int32& LastHarmonic,
                          const FVector& Pos, const FLinearColor& Color);

    /** Number of stars in the backdrop shell. */
    UPROPERTY(EditAnywhere, Category = "MANIFOLD") int32 StarCount = 640;

    void SpawnStarfield();

    int32 Cursor = 0;

    /** Accumulated spin so the ratio-pair clusters gently orbit (a living scene). */
    float SpinAngle = 0.0f;

    /** Discovery "flash": a brief scene brightening (blooms via post) when a new
     *  correspondence surfaces, so an insight lands with a visible pulse. */
    float PulseTimer = 0.0f;   // seconds remaining in the current flash
    float PulseFactor = 1.0f;  // color multiplier applied to spheres this frame
    int32 LastDiscoveryCount = 0;

    /** Frame-pooled sphere placement: reuse components, hide the leftovers. */
    void BeginFrame();
    void EndFrame();
    UStaticMeshComponent* AcquireSphere();
    void PlaceSphere(const FVector& WorldPos, float DiameterUnits, const FLinearColor& Color);

    /** Position + light the dedicated Orbits sun (its own hotter M_Star material). */
    void UpdateStar(const FVector& Pos, float DiameterUnits);

    /** Apply the shared realm-glow look to a dynamic material: a pulse-scaled lit colour and a
     *  brighter emissive that blooms via the post-process. Writes the four common param names so it
     *  works for both the lit and unlit-emissive material variants (missing params are no-ops). */
    void ApplyGlow(UMaterialInstanceDynamic* MID, const FLinearColor& Color, float Pulse, float GlowMultiplier = 1.7f);

    /** Draw a ratio realm as two beads sized p and q, side by side. */
    void PlaceRatioRealm(const FVector& Center, int32 P, int32 Q, const FLinearColor& Color);
};
