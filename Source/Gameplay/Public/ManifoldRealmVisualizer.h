// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ManifoldRealmVisualizer.generated.h"

/**
 * AManifoldRealmVisualizer — a debug-draw view of the live realms so the
 * correspondence is *visible*, not just numeric:
 *   - Orbits: star + planets, with a gold ribbon along a detected resonance.
 *   - Fluids: the density field as a point cloud.
 *   - A gold ribbon across the seam when a correspondence is lit.
 *
 * This is the legible placeholder for the full World/VFX layer. Spawned by
 * AManifoldGameMode. (Debug draw is compiled out of Shipping builds.)
 */
UCLASS()
class MANIFOLDGAMEPLAY_API AManifoldRealmVisualizer : public AActor
{
    GENERATED_BODY()

public:
    AManifoldRealmVisualizer();
    virtual void Tick(float DeltaSeconds) override;

    /** World-space anchor for the Orbits realm view. */
    UPROPERTY(EditAnywhere, Category = "MANIFOLD")
    FVector OrbitsCenter = FVector(-700.0, 0.0, 200.0);

    /** World-space anchor for the Fluids realm view. */
    UPROPERTY(EditAnywhere, Category = "MANIFOLD")
    FVector FluidsCenter = FVector(700.0, 0.0, 200.0);

    /** World units per astronomical unit (Orbits are ~1e11 m). */
    UPROPERTY(EditAnywhere, Category = "MANIFOLD")
    float OrbitsScale = 450.0f;

    /** Half-size of the Fluids field plane, in world units. */
    UPROPERTY(EditAnywhere, Category = "MANIFOLD")
    float FluidsExtent = 350.0f;
};
