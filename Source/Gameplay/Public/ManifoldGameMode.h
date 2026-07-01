// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ManifoldTypes.h"
#include "ManifoldGameMode.generated.h"

class UManifoldSlice;

/**
 * AManifoldGameMode — the playable shell. Owns a live UManifoldSlice, advances it
 * at a fixed cadence, and exposes the player's transport verb. Set this as the
 * World's GameMode (or launch a level with it) to play the vertical slice.
 */
UCLASS()
class MANIFOLDGAMEPLAY_API AManifoldGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AManifoldGameMode();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    /** The live playable session (created in BeginPlay). */
    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    UManifoldSlice* Slice = nullptr;

    /**
     * Player verb. Console command `ManifoldTransport`, or the [E] key (bound by
     * AManifoldPlayerController): transport the currently-lit correspondence.
     */
    UFUNCTION(Exec, BlueprintCallable, Category = "MANIFOLD")
    void ManifoldTransport();

    /** Restart the session from scratch ([R] key or `ManifoldRestart` console). */
    UFUNCTION(Exec, BlueprintCallable, Category = "MANIFOLD")
    void ManifoldRestart();

    /** Fixed simulation cadence (seconds) so the feel is frame-rate independent. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MANIFOLD")
    float StepInterval = 0.05f;

    /** Win condition applied to each session. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MANIFOLD")
    FManifoldObjective Objective;

protected:
    float Accumulator = 0.0f;

    /** Build a fresh interactive session (slice + objective) and the realm view. */
    void StartSession();
};
