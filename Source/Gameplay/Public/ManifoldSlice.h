// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ManifoldSlice.generated.h"

class UOrbitsKernel;
class UFluidsKernel;
class UHarmonicsKernel;
class UCorrespondenceSystem;
class UTelemetrySystem;

/**
 * Result of a headless vertical-slice playthrough — the numbers the
 * Production/QA gate cares about (Build Plan §5 Stream P).
 */
USTRUCT(BlueprintType)
struct MANIFOLDGAMEPLAY_API FManifoldSliceResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    bool bResonanceDetected = false;

    UPROPERTY(BlueprintReadOnly)
    bool bVortexDetected = false;

    UPROPERTY(BlueprintReadOnly)
    int32 CorrespondencesIgnited = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 TransportsCompleted = 0;

    UPROPERTY(BlueprintReadOnly)
    float InsightRate = 0.0f;
};

/**
 * UManifoldSlice — the playable vertical-slice loop (the "brain" that ties the
 * Systems stream into an actual game beat):
 *
 *   Orbits realm (3:2 resonance) + Fluids realm (vortex)
 *     -> Correspondence detection across the seam
 *     -> Transport power from one realm into the other on ignition
 *     -> Telemetry records the Insight Rate.
 *
 * This is deterministic and runnable headless so the whole loop is CI-verifiable
 * (Build Plan §6 integration point: Kernels + Correspondence + Telemetry).
 */
UCLASS(BlueprintType)
class MANIFOLDGAMEPLAY_API UManifoldSlice : public UObject
{
    GENERATED_BODY()

public:
    /** Build the two realms with the canonical slice scenario and wire reactions. */
    void Setup(uint64 OrbitsSeed, uint64 FluidsSeed);

    /** Advance both realms for N steps, detecting/igniting/transporting each step. */
    FManifoldSliceResult RunPlaythrough(int32 Steps);

    /** Advance the session by one simulation step (interactive / per-frame use). */
    void Tick();

    /**
     * Player verb: transport the currently-lit correspondence across the seam.
     * Returns true if a correspondence was available and transported.
     */
    UFUNCTION(BlueprintCallable, Category = "MANIFOLD")
    bool PlayerRequestTransport();

    /** When true (default), a detected correspondence auto-transports (headless/CI).
     *  Interactive play sets this false so the player triggers transport. */
    UPROPERTY(BlueprintReadWrite, Category = "MANIFOLD")
    bool bAutoTransportOnIgnite = true;

    // --- Live state (for HUD / gameplay) ---
    UFUNCTION(BlueprintPure, Category = "MANIFOLD") float GetInsightRate() const;
    UFUNCTION(BlueprintPure, Category = "MANIFOLD") int32 GetCorrespondencesIgnited() const { return IgnitedCount; }
    UFUNCTION(BlueprintPure, Category = "MANIFOLD") int32 GetTransportsCompleted() const { return TransportCount; }
    UFUNCTION(BlueprintPure, Category = "MANIFOLD") bool IsCorrespondenceAvailable() const { return bCorrespondenceAvailable; }
    UFUNCTION(BlueprintPure, Category = "MANIFOLD") bool HasResonance() const;
    UFUNCTION(BlueprintPure, Category = "MANIFOLD") bool HasVortex() const;
    UFUNCTION(BlueprintPure, Category = "MANIFOLD") FString GetOrbitsRatio() const;
    UFUNCTION(BlueprintPure, Category = "MANIFOLD") FString GetHarmonicsRatio() const;
    /** Cross-domain analogies found via the generic N-realm engine (e.g. orbital 3:2 == harmonic 3:2). */
    UFUNCTION(BlueprintPure, Category = "MANIFOLD") int32 GetSharedDiscoveries() const { return SharedDiscoveries; }
    UFUNCTION(BlueprintPure, Category = "MANIFOLD") int64 GetStepCount() const { return CurrentStep; }

    UPROPERTY()
    UOrbitsKernel* Orbits = nullptr;

    UPROPERTY()
    UFluidsKernel* Fluids = nullptr;

    UPROPERTY()
    UHarmonicsKernel* Harmonics = nullptr;

    UPROPERTY()
    UCorrespondenceSystem* Correspond = nullptr;

    UPROPERTY()
    UTelemetrySystem* Telemetry = nullptr;

private:
    int32 IgnitedCount = 0;
    int32 TransportCount = 0;
    int32 SharedDiscoveries = 0;
    int64 CurrentStep = 0;
    float CurrentTime = 0.0f;

    // A lit-but-not-yet-transported correspondence the player can act on.
    bool bCorrespondenceAvailable = false;
    FGuid PendingVortexId;

    void HandleIgnited(FGuid SourceStructure, FGuid TargetStructure, float Scale);
    void HandleTransport(FGuid Source, FName TargetRealm, FGuid TargetId, float Strength);
    void DoTransportPendingVortex();
};
