// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ManifoldTypes.h"
#include "ManifoldAudioDirector.h"
#include "ManifoldSlice.generated.h"

class UOrbitsKernel;
class UFluidsKernel;
class UHarmonicsKernel;
class UWavesKernel;
class UCorrespondenceSystem;
class UTelemetrySystem;
class UManifoldAudioDirector;

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

    // --- Objective / session (the win condition that makes this a game) ---

    /** Set the win condition for this session (target discoveries, step budget). */
    UFUNCTION(BlueprintCallable, Category = "MANIFOLD")
    void SetObjective(const FManifoldObjective& InObjective) { Objective = InObjective; }

    /** Total discoveries so far = Orbits<->Fluids ignitions + cross-domain analogies. */
    UFUNCTION(BlueprintPure, Category = "MANIFOLD")
    int32 GetTotalDiscoveries() const { return IgnitedCount + SharedDiscoveries; }

    /** Where the session stands relative to its objective. */
    UFUNCTION(BlueprintPure, Category = "MANIFOLD")
    EManifoldSessionState GetSessionState() const { return SessionState; }

    /** Snapshot of the session outcome (state + score + insight). */
    UFUNCTION(BlueprintPure, Category = "MANIFOLD")
    FManifoldSessionSummary GetSessionSummary() const;

    // --- Replay (deterministic record / reproduce / persist) ---

    /** Play a fresh session with these seeds/steps and return a recording of it. */
    UFUNCTION(BlueprintCallable, Category = "MANIFOLD")
    FManifoldReplay RecordReplay(int64 OrbitsSeed, int64 FluidsSeed, int32 Steps);

    /** Re-run a recording on a fresh slice; the result must match what was recorded. */
    UFUNCTION(BlueprintCallable, Category = "MANIFOLD")
    FManifoldSliceResult RunReplay(const FManifoldReplay& Replay);

    /** Steps at which a transport fired this session (the reproduced schedule). */
    const TArray<int32>& GetTransportSchedule() const { return TransportStepLog; }

    // --- Audio (the correspondence you can hear) ---

    /** The most recent audio cue the session emitted (for HUD / audio playback). */
    UFUNCTION(BlueprintPure, Category = "MANIFOLD")
    FManifoldAudioCue GetLastAudioCue() const { return LastAudioCue; }

    /** How many audio cues have been emitted this session. */
    UFUNCTION(BlueprintPure, Category = "MANIFOLD")
    int32 GetAudioCueCount() const { return AudioCues.Num(); }

    /** Persist / restore a recording to a .manifoldreplay file. */
    static bool SaveReplay(const FManifoldReplay& Replay, const FString& Path);
    static bool LoadReplay(FManifoldReplay& OutReplay, const FString& Path);

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
    UWavesKernel* Waves = nullptr;

    UPROPERTY()
    UCorrespondenceSystem* Correspond = nullptr;

    UPROPERTY()
    UTelemetrySystem* Telemetry = nullptr;

    UPROPERTY()
    UManifoldAudioDirector* Audio = nullptr;

private:
    int32 IgnitedCount = 0;
    int32 TransportCount = 0;
    int32 SharedDiscoveries = 0;
    int64 CurrentStep = 0;
    float CurrentTime = 0.0f;

    // A lit-but-not-yet-transported correspondence the player can act on.
    bool bCorrespondenceAvailable = false;
    FGuid PendingVortexId;

    // Objective / session outcome.
    FManifoldObjective Objective;
    EManifoldSessionState SessionState = EManifoldSessionState::InProgress;

    // Steps at which a transport fired — the schedule a replay reproduces.
    TArray<int32> TransportStepLog;

    // Audio cues emitted this session (discovery chimes, transport resolves).
    TArray<FManifoldAudioCue> AudioCues;
    FManifoldAudioCue LastAudioCue;

    void HandleIgnited(FGuid SourceStructure, FGuid TargetStructure, float Scale);
    void HandleTransport(FGuid Source, FName TargetRealm, FGuid TargetId, float Strength);
    void DoTransportPendingVortex();
    void EvaluateObjective();
};
