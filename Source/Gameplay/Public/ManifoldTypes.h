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

    /** True if this was a Constellation-Lock session (scored on its own leaderboard). */
    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    bool bConstellation = false;
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
    int32 BestScore = 0; // best Classic score

    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    int32 SessionsPlayed = 0;

    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    int32 SessionsWon = 0;

    /** Best Constellation-Lock score — a separate leaderboard (the two modes aren't
     *  comparable, so "beat your best" tracks each independently). */
    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    int32 BestConstellationScore = 0;

    /** Best cumulative score across a Constellation Expedition campaign. */
    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    int32 BestExpeditionScore = 0;

    static constexpr uint32 Magic = 0x4D414E50; // 'MANP'
    static constexpr uint32 Version = 3;

    /**
     * Version-aware body (reads or writes, per Ar's direction). Fields have only ever been
     * APPENDED (v1: score/played/won; v2: +constellation; v3: +expedition), so an older save
     * is a valid *prefix* of the current layout. Reading with the file's on-disk version pulls
     * exactly the fields that existed then and leaves newer ones at their defaults — migrating
     * a returning player's career forward across a format bump instead of discarding it.
     */
    void SerializeVersioned(FArchive& Ar, uint32 InVersion)
    {
        Ar << BestScore;
        Ar << SessionsPlayed;
        Ar << SessionsWon;
        if (InVersion >= 2) { Ar << BestConstellationScore; }
        if (InVersion >= 3) { Ar << BestExpeditionScore; }
    }

    friend FArchive& operator<<(FArchive& Ar, FManifoldProfile& P)
    {
        // Writes/reads the CURRENT version's full field set (all call sites use current format).
        P.SerializeVersioned(Ar, FManifoldProfile::Version);
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

    /** Format tag so foreign / newer files are rejected rather than silently misread. */
    static constexpr uint32 Magic = 0x4D414E52; // 'MANR'
    static constexpr uint32 Version = 2;

    /**
     * Upper bound on a deserialized `Steps`. A replay is UNTRUSTED, shareable input, and `Steps` is
     * a raw scalar that RunReplay uses directly as a loop bound — each iteration advances every
     * physics kernel plus correspondence detection. Left unbounded, a crafted `Steps` near INT32_MAX
     * turns a ~44-byte file into ~2.1e9 iterations of full-CPU work (a denial-of-service on merely
     * replaying a shared file). This ceiling is ~1000x any real recorded session (which wins in tens
     * of steps and is bounded by the objective's StepBudget), so it never rejects a legitimate replay,
     * yet caps the worst case to a bounded, brief loop. Companion to the bounded-array guard above —
     * that bounds untrusted array COUNTS; this bounds the untrusted scalar step count.
     */
    static constexpr int32 MaxSteps = 1000000;

    /**
     * Version-aware body. Fields were only APPENDED (v1: the 7 seed/step/result fields;
     * v2: +Mode/ConstellationSize/LockSelection), so a v1 replay is a valid prefix: it loads
     * as a Classic session (Mode defaults to 0) instead of being rejected — a shared v1
     * replay still plays back after a format bump.
     */
    /**
     * Bounded read of a TArray<int32> from a possibly-UNTRUSTED archive (replays are shareable).
     * UE's default TArray serializer reads an int32 count prefix and pre-allocates count*sizeof(T)
     * BEFORE reading any element, and its 16MB safety cap applies ONLY to net archives — so a
     * crafted count in a replay file forces a multi-GB allocation (fatal OOM/DoS) on a non-net
     * FMemoryReader, before any IsError() check can run. We instead reject up front any count that
     * cannot be backed by the bytes actually remaining in the file (a tight, exact bound: N int32
     * elements need N*4 bytes). The write path is byte-for-byte identical to the default
     * operator<<, so the on-disk format is unchanged.
     */
    static void SerializeBoundedInt32Array(FArchive& Ar, TArray<int32>& Arr)
    {
        if (Ar.IsLoading())
        {
            int32 Count = 0;
            Ar << Count;
            const int64 Remaining = Ar.TotalSize() - Ar.Tell();
            if (Ar.IsError() || Count < 0 ||
                static_cast<int64>(Count) * static_cast<int64>(sizeof(int32)) > Remaining)
            {
                Ar.SetError(); // corrupt/hostile length — reject BEFORE allocating
                Arr.Reset();
                return;
            }
            Arr.SetNumUninitialized(Count);
            for (int32 i = 0; i < Count; ++i)
            {
                Ar << Arr[i];
            }
        }
        else
        {
            Ar << Arr; // saving: default layout (count + bulk elements) — unchanged on disk
        }
    }

    void SerializeVersioned(FArchive& Ar, uint32 InVersion)
    {
        Ar << OrbitsSeed;
        Ar << FluidsSeed;
        Ar << Steps;
        SerializeBoundedInt32Array(Ar, TransportSteps); // untrusted count — bound before alloc
        Ar << FinalDiscoveries;
        Ar << FinalTransports;
        Ar << FinalInsightRate;
        if (InVersion >= 2)
        {
            Ar << Mode;
            Ar << ConstellationSize;
            SerializeBoundedInt32Array(Ar, LockSelection); // untrusted count — bound before alloc
        }
    }

    friend FArchive& operator<<(FArchive& Ar, FManifoldReplay& R)
    {
        R.SerializeVersioned(Ar, FManifoldReplay::Version);
        return Ar;
    }
};
