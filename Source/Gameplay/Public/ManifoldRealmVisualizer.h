// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ManifoldRealmVisualizer.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UStaticMeshComponent;

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
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, Category = "MANIFOLD") FVector OrbitsCenter    = FVector(-900.0, 0.0, 250.0);
    UPROPERTY(EditAnywhere, Category = "MANIFOLD") FVector FluidsCenter    = FVector(-450.0, 0.0, 250.0);
    UPROPERTY(EditAnywhere, Category = "MANIFOLD") FVector HarmonicsCenter = FVector(   0.0, 0.0, 250.0);
    UPROPERTY(EditAnywhere, Category = "MANIFOLD") FVector WavesCenter     = FVector( 450.0, 0.0, 250.0);
    UPROPERTY(EditAnywhere, Category = "MANIFOLD") FVector RhythmCenter    = FVector( 900.0, 0.0, 250.0);
    UPROPERTY(EditAnywhere, Category = "MANIFOLD") FVector GearsCenter     = FVector(-450.0, 0.0, 560.0);
    UPROPERTY(EditAnywhere, Category = "MANIFOLD") FVector DecoyCenter     = FVector( 450.0, 0.0, 560.0);

    /** World units per astronomical unit (Orbits are ~1e11 m). */
    UPROPERTY(EditAnywhere, Category = "MANIFOLD") float OrbitsScale = 180.0f;

    /** Half-size of the Fluids field, in world units. */
    UPROPERTY(EditAnywhere, Category = "MANIFOLD") float FluidsExtent = 160.0f;

protected:
    UPROPERTY() UStaticMesh* SphereMesh = nullptr;
    UPROPERTY() UMaterialInterface* BaseMaterial = nullptr;
    UPROPERTY() USceneComponent* SceneRoot = nullptr;
    UPROPERTY() TArray<UStaticMeshComponent*> Pool;

    int32 Cursor = 0;

    /** Frame-pooled sphere placement: reuse components, hide the leftovers. */
    void BeginFrame();
    void EndFrame();
    UStaticMeshComponent* AcquireSphere();
    void PlaceSphere(const FVector& WorldPos, float DiameterUnits, const FLinearColor& Color);

    /** Draw a ratio realm as two beads sized p and q, side by side. */
    void PlaceRatioRealm(const FVector& Center, int32 P, int32 Q, const FLinearColor& Color);
};
