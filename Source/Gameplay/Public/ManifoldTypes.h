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
 * How a session plays. Classic is the Orbits<->Fluids seam plus auto-surfaced
 * cross-domain analogies (the original loop). Constellation is the harder puzzle:
 * six realms each show a different surface ratio and the player must infer the
 * session's structural relation and lock the exact corresponding subset.
 */
UENUM(BlueprintType)
enum class EManifoldPlayMode : uint8
{
    Classic,
    Constellation
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
 * Performance grade for a session, derived from its score (S is best).
 */
UENUM(BlueprintType)
enum class EManifoldRank : uint8
{
    D,
    C,
    B,
    A,
    S
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

    /** Session score: discoveries + transports + insight, with a speed bonus on a win. */
    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    int32 Score = 0;

    /** Performance grade derived from the score (S is best). */
    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    EManifoldRank Rank = EManifoldRank::D;
};

/**
 * Result of an expedition — a campaign of escalating-difficulty levels played back to
 * back until one is failed. LevelsCleared is how far the player got; bCompleted is true
 * only if every level was cleared.
 */
USTRUCT(BlueprintType)
struct MANIFOLDGAMEPLAY_API FManifoldExpeditionResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    int32 LevelsCleared = 0;

    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    int32 TotalScore = 0;

    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    bool bCompleted = false;
};

/**
 * Persistent player profile — sessions played/won and the best score achieved. Saved
 * to a versioned .manifoldprofile file so "beat your best" survives across runs.
 */
USTRUCT(BlueprintType)
struct MANIFOLDGAMEPLAY_API FManifoldProfile
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    int32 BestScore = 0;

    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    int32 SessionsPlayed = 0;

    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    int32 SessionsWon = 0;

    static constexpr uint32 Magic = 0x4D414E50; // 'MANP'
    static constexpr uint32 Version = 1;

    friend FArchive& operator<<(FArchive& Ar, FManifoldProfile& P)
    {
        Ar << P.BestScore;
        Ar << P.SessionsPlayed;
        Ar << P.SessionsWon;
        return Ar;
    }
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

    // --- Constellation Lock (Mode == 1) ---
    // Which mode this replay records: 0 = Classic (transport schedule above), 1 =
    // Constellation Lock (reproduced from the seed + the locked subset below).
    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    uint8 Mode = 0;

    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    int32 ConstellationSize = 0;

    /** The subset of realm indices the player locked (constellation mode). */
    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    TArray<int32> LockSelection;

    /** Format tag so old files are rejected rather than silently misread. */
    static constexpr uint32 Magic = 0x4D414E52; // 'MANR'
    static constexpr uint32 Version = 2;

    friend FArchive& operator<<(FArchive& Ar, FManifoldReplay& R)
    {
        Ar << R.OrbitsSeed;
        Ar << R.FluidsSeed;
        Ar << R.Steps;
        Ar << R.TransportSteps;
        Ar << R.FinalDiscoveries;
        Ar << R.FinalTransports;
        Ar << R.FinalInsightRate;
        Ar << R.Mode;
        Ar << R.ConstellationSize;
        Ar << R.LockSelection;
        return Ar;
    }
};
