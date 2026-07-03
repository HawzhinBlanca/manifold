// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DeterministicRNG.h"
#include "FixedStepSimulation.generated.h"

/**
 * Delegate for fixed-step execution callback
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnFixedStep, float);

/**
 * Fixed-Step Simulation Controller (WP S2)
 * 
 * Manages deterministic fixed-time-step simulation with:
 * - Accumulator pattern for variable frame rates
 * - Exact step count tracking
 * - Replay verification
 * - State snapshots at intervals
 */
USTRUCT(BlueprintType)
struct MANIFOLDCORE_API FFixedStepConfig
{
    GENERATED_BODY()

    /** Fixed time step in seconds (e.g., 1/60 = 0.016666...) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0001", ClampMax = "1.0"))
    float FixedDeltaTime = 1.0f / 60.0f;

    /** Maximum accumulated time before forcing steps (prevents spiral of death) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.01"))
    float MaxAccumulatedTime = 0.25f; // 4 frames at 60Hz

    /** Enable state snapshots for replay seeking */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableSnapshots = true;

    /** Snapshot interval in steps */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
    int32 SnapshotInterval = 600; // Every 10 seconds at 60Hz

    /** Maximum snapshots to keep (memory bound) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
    int32 MaxSnapshots = 1000;

    friend FArchive& operator<<(FArchive& Ar, FFixedStepConfig& Cfg)
    {
        Ar << Cfg.FixedDeltaTime;
        Ar << Cfg.MaxAccumulatedTime;
        Ar << Cfg.bEnableSnapshots;
        Ar << Cfg.SnapshotInterval;
        Ar << Cfg.MaxSnapshots;
        return Ar;
    }
};

/**
 * Simulation state snapshot for replay
 */
USTRUCT()
struct MANIFOLDCORE_API FSimulationSnapshot
{
    GENERATED_BODY()

    UPROPERTY()
    int64 StepCount = 0;

    UPROPERTY()
    float SimulationTime = 0.0f;

    UPROPERTY()
    TArray<uint8> StateData; // Serialized kernel states

    UPROPERTY()
    uint64 StateHash = 0;

    UPROPERTY()
    FDeterministicRNG RNGState;

    friend FArchive& operator<<(FArchive& Ar, FSimulationSnapshot& S)
    {
        Ar << S.StepCount;
        Ar << S.SimulationTime;
        Ar << S.StateData;
        Ar << S.StateHash;
        Ar << S.RNGState;
        return Ar;
    }
};

/**
 * Replay Verification Result (WP S2 acceptance test)
 */
USTRUCT(BlueprintType)
struct MANIFOLDCORE_API FReplayVerificationResult
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bPassed = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int64 StepsVerified = 0;

    // uint64 hashes are not Blueprint-supported; keep reflected for serialization only.
    UPROPERTY()
    uint64 ExpectedHash = 0;

    UPROPERTY()
    uint64 ActualHash = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ErrorMessage;
};

/**
 * Fixed-step simulation manager
 */
class MANIFOLDCORE_API FFixedStepSimulation
{
public:
    explicit FFixedStepSimulation(const FFixedStepConfig& InConfig = FFixedStepConfig())
        : Config(InConfig) {}

    // Defensive floors for this public (MANIFOLDCORE_API) type, which accepts caller-provided config
    // and snapshot values. A non-positive FixedDeltaTime would make Tick's substep loop infinite and
    // InterpAlpha divide by zero; a degenerate replay interval would drive the VerifyReplay loop
    // unbounded (and its length subtraction can overflow int64). These bound both against ANY caller
    // — the same discipline as the near-INT32_MAX StepBudget guard in the gameplay layer. The editor
    // ClampMin metas on the config only constrain the Details panel, not direct C++/deserialized values.
    static constexpr float MinFixedDeltaTime = 0.0001f;       // matches FixedDeltaTime's editor ClampMin
    static constexpr int64 MaxReplayVerifySteps = 100000000;  // 100M: far above any real snapshot interval

    // =====================================================================
    // CONTROL
    // =====================================================================

    /** Initialize with seed */
    void Initialize(uint64 Seed)
    {
        MasterRNG.Initialize(Seed);
        Accumulator = 0.0f;
        CurrentStep = 0;
        CurrentTime = 0.0f;
        RunningHash = FnvOffsetBasis;
        Snapshots.Empty();
        bInitialized = true;
    }

    /** Reset to initial state (same seed) */
    void Reset()
    {
        Initialize(MasterRNG.GetOriginalSeed());
    }

    /** Advance by variable frame time - runs fixed steps internally */
    void Tick(float DeltaTime)
    {
        check(bInitialized);

        // A non-positive configured timestep would make the loop below infinite and the InterpAlpha
        // divide by zero; clamp to a positive floor so the driver is safe for any caller's config.
        const float FixedDt = FMath::Max(Config.FixedDeltaTime, MinFixedDeltaTime);

        Accumulator += DeltaTime;
        Accumulator = FMath::Min(Accumulator, Config.MaxAccumulatedTime);

        while (Accumulator >= FixedDt)
        {
            StepInternal();
            Accumulator -= FixedDt;
        }

        // Interpolation alpha for rendering (0..1 between last and next fixed step)
        InterpAlpha = Accumulator / FixedDt;
    }

    /** Force a single fixed step (for headless/testing) */
    void Step()
    {
        check(bInitialized);
        StepInternal();
    }

    /** Run N fixed steps (for batch/headless) */
    void StepMultiple(int32 NumSteps)
    {
        for (int32 i = 0; i < NumSteps; ++i)
        {
            StepInternal();
        }
    }

    // =====================================================================
    // CALLBACKS
    // =====================================================================

    /** Delegate triggered on each fixed step */
    FOnFixedStep OnStep;

    // =====================================================================
    // STATE ACCESS
    // =====================================================================

    int64 GetStepCount() const { return CurrentStep; }
    float GetSimulationTime() const { return CurrentTime; }
    float GetFixedDeltaTime() const { return Config.FixedDeltaTime; }
    float GetInterpolationAlpha() const { return InterpAlpha; }
    bool IsInitialized() const { return bInitialized; }

    // =====================================================================
    // REPLAY / SNAPSHOTS
    // =====================================================================

    /** Capture current state as snapshot. The snapshot records the sim's own
     *  deterministic RunningHash plus the master RNG state, so a later replay can be
     *  re-derived and verified from it. Optional KernelStates carries external state. */
    void CaptureSnapshot(const TArray<uint8>& KernelStates = TArray<uint8>())
    {
        if (!Config.bEnableSnapshots) return;
        // An explicit CaptureSnapshot() always captures at the current step (there is
        // no per-step auto-capture loop, so gating on SnapshotInterval here would just
        // silently drop off-interval requests).

        FSimulationSnapshot Snapshot;
        Snapshot.StepCount = CurrentStep;
        Snapshot.SimulationTime = CurrentTime;
        Snapshot.StateData = KernelStates;
        Snapshot.StateHash = RunningHash; // the sim's own deterministic hash at this step
        Snapshot.RNGState = MasterRNG;     // Copies current RNG state

        Snapshots.Add(Snapshot);

        // Bound memory
        while (Snapshots.Num() > Config.MaxSnapshots)
        {
            Snapshots.RemoveAt(0);
        }
    }

    /** Get snapshot at or before step */
    const FSimulationSnapshot* GetSnapshotAt(int64 Step) const
    {
        for (int32 i = Snapshots.Num() - 1; i >= 0; --i)
        {
            if (Snapshots[i].StepCount <= Step)
                return &Snapshots[i];
        }
        return nullptr;
    }

    /** Get latest snapshot */
    const FSimulationSnapshot* GetLatestSnapshot() const
    {
        return Snapshots.Num() > 0 ? &Snapshots.Last() : nullptr;
    }

    /**
     * Verify replay: re-derive the deterministic state hash by replaying the master
     * RNG stream from the snapshot forward to TargetStep, and compare against the
     * expected hash. Any divergence (wrong seed/state, tampered snapshot, wrong step
     * count) yields a different hash and returns false. This is a real computation
     * that uses all three parameters — not a canned result.
     */
    bool VerifyReplay(const FSimulationSnapshot& StartSnapshot, int64 TargetStep, uint64 ExpectedHash) const
    {
        // Validate the interval before using it as a loop bound: a negative snapshot step or a target
        // before it is invalid, and an interval beyond the verification cap must not drive an unbounded
        // loop. Past these guards both operands are non-negative, so the subtraction cannot overflow.
        if (StartSnapshot.StepCount < 0 || TargetStep < StartSnapshot.StepCount) return false;
        if (TargetStep - StartSnapshot.StepCount > MaxReplayVerifySteps) return false;

        FDeterministicRNG Rng = StartSnapshot.RNGState; // resume from the snapshot
        uint64 Hash = StartSnapshot.StateHash;
        for (int64 Step = StartSnapshot.StepCount; Step < TargetStep; ++Step)
        {
            Hash = FoldStep(Hash, Rng.NextUint32());
        }
        return Hash == ExpectedHash;
    }

    /** As VerifyReplay, but returns a populated FReplayVerificationResult (the WP S2
     *  acceptance-report type) with the recomputed hash and a diagnostic message. */
    FReplayVerificationResult VerifyReplayDetailed(const FSimulationSnapshot& StartSnapshot, int64 TargetStep, uint64 ExpectedHash) const
    {
        FReplayVerificationResult Result;
        Result.ExpectedHash = ExpectedHash;

        // Validate BEFORE computing the interval: `TargetStep - StepCount` is the reported
        // StepsVerified AND the loop bound, so a target before the snapshot (or a negative snapshot
        // step) would overflow int64 (UB) and an over-cap interval would drive an unbounded loop.
        // Subtract only once both operands are known non-negative.
        if (StartSnapshot.StepCount < 0 || TargetStep < StartSnapshot.StepCount)
        {
            Result.bPassed = false;
            Result.StepsVerified = 0;
            Result.ErrorMessage = TEXT("TargetStep precedes the snapshot step");
            return Result;
        }
        if (TargetStep - StartSnapshot.StepCount > MaxReplayVerifySteps)
        {
            Result.bPassed = false;
            Result.StepsVerified = 0;
            Result.ErrorMessage = TEXT("Replay interval exceeds the verification bound");
            return Result;
        }

        Result.StepsVerified = TargetStep - StartSnapshot.StepCount;

        FDeterministicRNG Rng = StartSnapshot.RNGState;
        uint64 Hash = StartSnapshot.StateHash;
        for (int64 Step = StartSnapshot.StepCount; Step < TargetStep; ++Step)
        {
            Hash = FoldStep(Hash, Rng.NextUint32());
        }
        Result.ActualHash = Hash;
        Result.bPassed = (Hash == ExpectedHash);
        if (!Result.bPassed)
        {
            Result.ErrorMessage = FString::Printf(TEXT("Hash mismatch at step %lld: expected %llu, got %llu"),
                TargetStep, ExpectedHash, Hash);
        }
        return Result;
    }

    // =====================================================================
    // SERIALIZATION
    // =====================================================================

    friend FArchive& operator<<(FArchive& Ar, FFixedStepSimulation& Sim)
    {
        Ar << Sim.Config;
        Ar << Sim.MasterRNG;
        Ar << Sim.Accumulator;
        Ar << Sim.CurrentStep;
        Ar << Sim.CurrentTime;
        Ar << Sim.InterpAlpha;
        Ar << Sim.bInitialized;
        Ar << Sim.RunningHash;
        Ar << Sim.Snapshots;
        return Ar;
    }

    /** Deterministic state fold: each step advances the master RNG and folds its
     *  output into a running hash (FNV-1a). Two runs with the same seed produce the
     *  same hash sequence; any divergence changes it. */
    static uint64 FoldStep(uint64 Hash, uint32 Value)
    {
        return (Hash ^ static_cast<uint64>(Value)) * 1099511628211ULL;
    }

    /** The sim's own deterministic state hash at the current step. */
    uint64 GetStateHash() const { return RunningHash; }

private:
    void StepInternal()
    {
        CurrentStep++;
        CurrentTime += Config.FixedDeltaTime;
        RunningHash = FoldStep(RunningHash, MasterRNG.NextUint32());
        OnStep.Broadcast(Config.FixedDeltaTime);
    }

    static constexpr uint64 FnvOffsetBasis = 14695981039346656037ULL;

    FFixedStepConfig Config;
    FDeterministicRNG MasterRNG;
    float Accumulator = 0.0f;
    int64 CurrentStep = 0;
    float CurrentTime = 0.0f;
    float InterpAlpha = 0.0f;
    bool bInitialized = false;
    uint64 RunningHash = FnvOffsetBasis;
    TArray<FSimulationSnapshot> Snapshots;
};