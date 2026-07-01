// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ManifoldTypes.generated.h"

/**
 * Where a playable session stands relative to its objective.
 * A session runs InProgress until the player reaches the objective (Won) or the
 * step budget is exhausted first (Lost). Sessions with an unlimited budget never
 * enter Lost — they simply stay InProgress until the target is met.
 */
UENUM(BlueprintType)
enum class EManifoldSessionState : uint8
{
    InProgress,
    Won,
    Lost
};

/**
 * The win condition for a playable session (the thing that turns the endless
 * simulation into a game with a goal). A discovery is any correspondence the
 * player surfaces — an Orbits<->Fluids ignition or a cross-domain shared-structure
 * analogy (e.g. orbital 3:2 == harmonic 3:2).
 */
USTRUCT(BlueprintType)
struct MANIFOLDGAMEPLAY_API FManifoldObjective
{
    GENERATED_BODY()

    /** Discoveries the player must accumulate to win. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MANIFOLD", meta = (ClampMin = "1"))
    int32 TargetDiscoveries = 3;

    /** Steps allowed to reach the target; 0 means unlimited (never Lost). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MANIFOLD", meta = (ClampMin = "0"))
    int32 StepBudget = 0;

    /** Minimum Insight Rate required to count the target as a win; 0 ignores it. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MANIFOLD", meta = (ClampMin = "0.0"))
    float MinInsightRate = 0.0f;
};

/**
 * The end-of-session readout — the numbers a player (and the QA gate) sees when a
 * session resolves.
 */
USTRUCT(BlueprintType)
struct MANIFOLDGAMEPLAY_API FManifoldSessionSummary
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    EManifoldSessionState State = EManifoldSessionState::InProgress;

    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    int32 Discoveries = 0;

    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    int32 Transports = 0;

    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    float InsightRate = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    int64 Steps = 0;
};

/**
 * A recorded session: the seeds that generate the two realms plus the exact steps
 * at which the player transported a correspondence. Because the whole slice is
 * deterministic, replaying these seeds with this transport schedule reproduces the
 * session bit-for-bit — the "un-pre-computable yet perfectly reproducible" pillar
 * made into a concrete, shareable artifact.
 */
USTRUCT(BlueprintType)
struct MANIFOLDGAMEPLAY_API FManifoldReplay
{
    GENERATED_BODY()

    // uint64 seeds are reflected for serialization but not Blueprint-exposed.
    UPROPERTY()
    uint64 OrbitsSeed = 0;

    UPROPERTY()
    uint64 FluidsSeed = 0;

    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    int32 Steps = 0;

    /** Steps at which a transport occurred (the player's verb schedule). */
    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    TArray<int32> TransportSteps;

    // Result captured at record time, used to verify a replay reproduced it.
    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    int32 FinalDiscoveries = 0;

    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    int32 FinalTransports = 0;

    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    float FinalInsightRate = 0.0f;

    /** Format tag so old files are rejected rather than silently misread. */
    static constexpr uint32 Magic = 0x4D414E52; // 'MANR'
    static constexpr uint32 Version = 1;

    friend FArchive& operator<<(FArchive& Ar, FManifoldReplay& R)
    {
        Ar << R.OrbitsSeed;
        Ar << R.FluidsSeed;
        Ar << R.Steps;
        Ar << R.TransportSteps;
        Ar << R.FinalDiscoveries;
        Ar << R.FinalTransports;
        Ar << R.FinalInsightRate;
        return Ar;
    }
};
