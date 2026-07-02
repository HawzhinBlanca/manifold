// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ManifoldTypes.h"
#include "ManifoldAudioDirector.h"
#include "CorrespondenceSystem.h" // ECorrespondenceRelation (Constellation Lock)
#include "ManifoldSlice.generated.h"

class UOrbitsKernel;
class UFluidsKernel;
class UHarmonicsKernel;
class UWavesKernel;
class URhythmKernel;
class UGearsKernel;
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

    /** Discoveries required to win this session (C(K,2) in constellation mode). */
    UFUNCTION(BlueprintPure, Category = "MANIFOLD")
    int32 GetObjectiveTarget() const { return Objective.TargetDiscoveries; }

    /** Snapshot of the session outcome (state + score + insight). */
    UFUNCTION(BlueprintPure, Category = "MANIFOLD")
    FManifoldSessionSummary GetSessionSummary() const;

    /** Session score: discoveries + transports + insight, plus a speed bonus on a win. */
    UFUNCTION(BlueprintPure, Category = "MANIFOLD")
    int32 GetScore() const;

    /** Performance grade for the current score (S is best). */
    UFUNCTION(BlueprintPure, Category = "MANIFOLD")
    EManifoldRank GetRank() const { return RankForScore(GetScore()); }

    /** Map a score to a rank tier. */
    UFUNCTION(BlueprintPure, Category = "MANIFOLD")
    static EManifoldRank RankForScore(int32 Score);

    /** Persist / restore the player profile (best score, sessions played/won). */
    static bool SaveProfile(const FManifoldProfile& Profile, const FString& Path);
    static bool LoadProfile(FManifoldProfile& OutProfile, const FString& Path);

    /** Fold a finished session into a profile (updates counts + the mode's best score).
     *  Returns true if this session set a NEW best for its mode (for a "new best!" cue). */
    static bool RecordSessionInProfile(FManifoldProfile& Profile, const FManifoldSessionSummary& Summary);

    // --- Replay (deterministic record / reproduce / persist) ---

    /** Play a fresh session with these seeds/steps and return a recording of it. */
    UFUNCTION(BlueprintCallable, Category = "MANIFOLD")
    FManifoldReplay RecordReplay(int64 OrbitsSeed, int64 FluidsSeed, int32 Steps);

    /** Re-run a recording on a fresh slice; the result must match what was recorded.
     *  Handles both Classic (transport schedule) and Constellation (locked subset) modes. */
    UFUNCTION(BlueprintCallable, Category = "MANIFOLD")
    FManifoldSliceResult RunReplay(const FManifoldReplay& Replay);

    /** Record a Constellation-Lock session as a reproducible replay: build the puzzle from
     *  the seed, lock the correct subset, and capture the seed + subset + result. Re-running
     *  it via RunReplay reproduces the session exactly (SetupConstellation is deterministic). */
    UFUNCTION(BlueprintCallable, Category = "MANIFOLD|Constellation")
    FManifoldReplay RecordConstellationReplay(int64 Seed, int32 ConstellationSize = 3);

    /** Capture the CURRENT (interactively-played) session as a shareable replay: its
     *  seeds, the exact steps the player transported, and the result so far. Re-running
     *  it via RunReplay reproduces the session bit-for-bit. */
    UFUNCTION(BlueprintCallable, Category = "MANIFOLD")
    FManifoldReplay CaptureReplay() const;

    /** Steps at which a transport fired this session (the reproduced schedule). */
    const TArray<int32>& GetTransportSchedule() const { return TransportStepLog; }

    // --- Audio (the correspondence you can hear) ---

    /** The most recent audio cue the session emitted (for HUD / audio playback). */
    UFUNCTION(BlueprintPure, Category = "MANIFOLD")
    FManifoldAudioCue GetLastAudioCue() const { return LastAudioCue; }

    /** How many audio cues have been emitted this session. */
    UFUNCTION(BlueprintPure, Category = "MANIFOLD")
    int32 GetAudioCueCount() const { return AudioCues.Num(); }

    /** All audio cues emitted this session, in order (so a synth can play new ones). */
    const TArray<FManifoldAudioCue>& GetAudioCues() const { return AudioCues; }

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
    UFUNCTION(BlueprintPure, Category = "MANIFOLD") FString GetWavesRatio() const;
    UFUNCTION(BlueprintPure, Category = "MANIFOLD") FString GetRhythmRatio() const;
    UFUNCTION(BlueprintPure, Category = "MANIFOLD") FString GetGearsRatio() const;

    /** The decoy realm's ratio — a RED HERRING that deliberately does NOT match the
     *  hidden ratio, so the player must actually discriminate the true correspondence. */
    UFUNCTION(BlueprintPure, Category = "MANIFOLD") FString GetDecoyRatio() const;

    /** The hidden ratio this session's realms all secretly share (picked from the
     *  seed). Different seeds hide different ratios — the puzzle is which one. */
    UFUNCTION(BlueprintPure, Category = "MANIFOLD")
    FString GetSharedRatio() const { return FString::Printf(TEXT("%d:%d"), SharedP, SharedQ); }

    /** Deterministically choose the session's hidden ratio (coprime, p > q) from a seed. */
    static void PickSharedRatio(uint64 Seed, int32& OutP, int32& OutQ);

    /**
     * Play an EXPEDITION: NumLevels sessions back to back with an escalating discovery
     * target, stopping at the first level the player cannot clear. Returns how far they
     * got and the cumulative score. Deterministic in BaseSeed.
     */
    static FManifoldExpeditionResult RunExpedition(int64 BaseSeed, int32 NumLevels, int32 StepsPerLevel = 30, UObject* Outer = nullptr);

    /**
     * Play a CONSTELLATION expedition: NumLevels subset-hunt puzzles back to back with
     * escalating difficulty (the rule is hidden — Expert — from level 3 onward), each
     * solved by a perfect player, returning how many were cleared and the cumulative
     * score. Deterministic in BaseSeed. Gives the harder mode a campaign + a single score.
     */
    static FManifoldExpeditionResult RunConstellationExpedition(int64 BaseSeed, int32 NumLevels, UObject* Outer = nullptr);

    // =====================================================================
    // CONSTELLATION LOCK — the harder puzzle (infer the relation, pick the subset)
    // =====================================================================

    /**
     * Build a Constellation-Lock session: six ratio realms each show a DIFFERENT surface
     * ratio, but a hidden subset (the "constellation" of ConstellationSize realms) truly
     * corresponds under this session's structural relation — Exact (they share the literal
     * ratio) or OctaveInvariant (they share a ratio only after dividing out factors of 2,
     * so 3:2, 6:1 and 3:1 all correspond even though they look different). The player must
     * INFER the relation and select exactly the corresponding subset — cross-domain
     * reasoning, not number-spotting. Deterministic in Seed. Resolved via
     * PlayerLockConstellation (not by ticking).
     */
    UFUNCTION(BlueprintCallable, Category = "MANIFOLD|Constellation")
    void SetupConstellation(int64 Seed, int32 ConstellationSize = 3, bool bExpertHideRule = false);

    /** Expert mode: the active relation is NOT revealed on the HUD, so the player must
     *  infer the rule (Exact vs Octave) from the ratios themselves — the design's full
     *  challenge. Earns a scoring bonus on a win. */
    UFUNCTION(BlueprintPure, Category = "MANIFOLD|Constellation")
    bool IsRelationHintHidden() const { return bHideRelationHint; }

    /** Deterministically choose this session's structural relation from the seed. */
    static ECorrespondenceRelation PickRelation(uint64 Seed);

    /** Deterministically choose K distinct realm indices (the hidden constellation) out
     *  of NumRealms; returned sorted ascending. */
    static void PickConstellation(uint64 Seed, int32 NumRealms, int32 K, TArray<int32>& OutMembers);

    /**
     * The player's verb: lock in the subset of realms believed to correspond. Scores
     * ONLY if the sorted selection exactly equals the hidden constellation — then the true
     * C(K,2) analogies ignite (driving discoveries/score/rank/telemetry through the same
     * path as organic discovery) and the session is won. A wrong selection consumes a
     * probe and scores nothing. Returns true on a correct lock.
     */
    UFUNCTION(BlueprintCallable, Category = "MANIFOLD|Constellation")
    bool PlayerLockConstellation(const TArray<int32>& SelectedRealmIndices);

    /**
     * Probe economy: pay to REVEAL whether a realm is in the hidden constellation. The
     * first reveal of a valid realm costs score (a paid probe); returns true if that realm
     * is a member. Trading points for certainty — spend when you're stuck. Idempotent per
     * realm (re-revealing the same one is free).
     */
    UFUNCTION(BlueprintCallable, Category = "MANIFOLD|Constellation")
    bool PlayerRevealRealm(int32 Index);

    /** Whether realm Index has been paid-revealed. */
    UFUNCTION(BlueprintPure, Category = "MANIFOLD|Constellation")
    bool IsRealmRevealed(int32 Index) const { return RevealedRealms.Contains(Index); }

    /** Revealed membership of realm Index: 1 = in the constellation, 0 = not, -1 = not revealed. */
    UFUNCTION(BlueprintPure, Category = "MANIFOLD|Constellation")
    int32 GetRevealedMembership(int32 Index) const;

    /** How many paid reveals the player has bought this session. */
    UFUNCTION(BlueprintPure, Category = "MANIFOLD|Constellation")
    int32 GetRevealCount() const { return RevealedRealms.Num(); }

    /** The hidden corresponding subset (sorted realm indices). Empty outside constellation mode. */
    const TArray<int32>& GetConstellation() const { return Constellation; }

    /** True once SetupConstellation has built a constellation session. */
    UFUNCTION(BlueprintPure, Category = "MANIFOLD|Constellation")
    bool IsConstellationMode() const { return bConstellationMode; }

    /** The active structural relation as a short human label ("Exact" / "Octave"). */
    UFUNCTION(BlueprintPure, Category = "MANIFOLD|Constellation")
    FString GetActiveRelationName() const;

    /** How many wrong locks the player has made this session. */
    UFUNCTION(BlueprintPure, Category = "MANIFOLD|Constellation")
    int32 GetFailedProbes() const { return FailedProbes; }

    /** Number of ratio realms shown in constellation mode (six). */
    UFUNCTION(BlueprintPure, Category = "MANIFOLD|Constellation")
    int32 GetConstellationRealmCount() const { return ConstellationRealmIds.Num(); }

    /** The surface ratio ("p:q") realm Index displays — what the player actually sees. */
    UFUNCTION(BlueprintPure, Category = "MANIFOLD|Constellation")
    FString GetRealmSurfaceRatio(int32 Index) const;

    /** The display name of realm Index (e.g. "Orbits", "Gears"). */
    UFUNCTION(BlueprintPure, Category = "MANIFOLD|Constellation")
    FString GetRealmName(int32 Index) const;
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
    URhythmKernel* Rhythm = nullptr;

    UPROPERTY()
    UGearsKernel* Gears = nullptr;

    /** A decoy ratio realm (red herring) that does NOT share the hidden ratio. */
    UPROPERTY()
    UHarmonicsKernel* Decoy = nullptr;

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

    // The seeds this session was built from (stored so it can be captured as a replay).
    uint64 SavedOrbitsSeed = 0;
    uint64 SavedFluidsSeed = 0;

    // The hidden ratio this session's realms share (chosen from the seed in Setup).
    int32 SharedP = 3;
    int32 SharedQ = 2;

    // The decoy realm's (deliberately non-matching) ratio.
    int32 DecoyP = 5;
    int32 DecoyQ = 4;
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

    // --- Constellation Lock state (set by SetupConstellation) ---
    bool bConstellationMode = false;
    int32 ConstellationSize = 0;
    ECorrespondenceRelation ActiveRelation = ECorrespondenceRelation::Exact;
    TArray<int32> Constellation;         // hidden corresponding realm indices (sorted)
    TArray<FName> ConstellationRealmIds; // realm id per index, in display order
    TArray<FString> RealmSurfaceRatios;  // surface ratio "p:q" per index (as shown)
    int32 FailedProbes = 0;
    bool bHideRelationHint = false;      // expert mode: don't reveal the active relation
    TArray<int32> RevealedRealms;        // realms the player paid to reveal (probe economy)

    void HandleIgnited(FGuid SourceStructure, FGuid TargetStructure, float Scale);
    void HandleSharedDiscovery(FName RealmA, FName RealmB, FString Ratio, FGuid StableId);
    void HandleTransport(FGuid Source, FName TargetRealm, FGuid TargetId, float Strength);
    void DoTransportPendingVortex();
    void EvaluateObjective();
};
