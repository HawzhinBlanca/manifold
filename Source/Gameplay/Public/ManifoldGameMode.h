// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ManifoldTypes.h"
#include "ManifoldGameMode.generated.h"

class UManifoldSlice;
class UManifoldSynthComponent;

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

    // --- Constellation Lock verbs ([C] switches mode; 1-6 pick; Space locks) ---

    /** Toggle between Classic and Constellation play and start a fresh session. */
    UFUNCTION(Exec, BlueprintCallable, Category = "MANIFOLD")
    void ManifoldToggleMode();

    /** Add/remove a realm (0-based index) from the pending constellation selection. */
    UFUNCTION(Exec, BlueprintCallable, Category = "MANIFOLD")
    void ConstellationToggleRealm(int32 Index);

    /** Lock the pending selection: an exact match with the hidden constellation wins. */
    UFUNCTION(Exec, BlueprintCallable, Category = "MANIFOLD")
    void ConstellationLock();

    /** Probe economy: pay to reveal the next unrevealed realm's membership ([V]). */
    UFUNCTION(Exec, BlueprintCallable, Category = "MANIFOLD")
    void ConstellationRevealNext();

    /** Begin an interactive Constellation Expedition: a campaign of escalating puzzles
     *  the player solves one after another, accumulating score ([X]). */
    UFUNCTION(Exec, BlueprintCallable, Category = "MANIFOLD")
    void ManifoldStartExpedition();

    /** Toggle the controls/modes help overlay ([H]). */
    UFUNCTION(Exec, BlueprintCallable, Category = "MANIFOLD")
    void ManifoldToggleHelp() { bHelpShown = !bHelpShown; }

    /** True while the help overlay is showing. */
    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    bool bHelpShown = false;

    // --- Expedition HUD accessors ---
    UFUNCTION(BlueprintPure, Category = "MANIFOLD") bool IsExpeditionActive() const { return bExpeditionActive; }
    UFUNCTION(BlueprintPure, Category = "MANIFOLD") int32 GetExpeditionLevel() const { return ExpeditionLevel; }
    UFUNCTION(BlueprintPure, Category = "MANIFOLD") int32 GetExpeditionLevels() const { return ExpeditionLevels; }
    UFUNCTION(BlueprintPure, Category = "MANIFOLD") int32 GetExpeditionScore() const { return ExpeditionScore; }

    /** Which mode the next/current session uses. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MANIFOLD")
    EManifoldPlayMode PlayMode = EManifoldPlayMode::Classic;

    /** Expert Constellation: the active rule is hidden, so the player must infer it. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MANIFOLD")
    bool bConstellationExpert = false;

    /** The realms the player has tentatively selected (for the HUD). */
    const TArray<int32>& GetPendingSelection() const { return PendingSelection; }

    /** Fixed simulation cadence (seconds) so the feel is frame-rate independent. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MANIFOLD")
    float StepInterval = 0.05f;

    /** Win condition applied to each session. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MANIFOLD")
    FManifoldObjective Objective;

    /** Persistent player profile (best score, sessions played/won), loaded on start. */
    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    FManifoldProfile Profile;

    /** True while the intro title card is showing (dismisses on first action or timeout). */
    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    bool bTitleShown = true;

    /** True if the just-resolved session set a new best score for its mode. */
    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD")
    bool bNewBestThisSession = false;

    /** One-shot guard + frame counter for the -ManifoldAutoShot dev/CI screenshot affordance (see Tick). */
    bool bAutoShotTaken = false;
    int32 AutoShotFrames = 0;

    /** One-shot guard: place the camera to frame the realm grid on the first tick it's available. */
    bool bCameraFramed = false;

protected:
    float Accumulator = 0.0f;

    /** True once the current (resolved) session has been folded into the profile. */
    bool bSessionRecorded = false;

    /** Where the player profile is persisted. */
    FString ProfilePath() const;

    /** Real-time procedural synth that plays the slice's audio cues (audible audio). */
    UPROPERTY()
    UManifoldSynthComponent* Synth = nullptr;

    /** Index of the next unplayed cue in the slice's cue log. */
    int32 LastPlayedCue = 0;

    /** Timer + realm cursor for the gentle ambient pad (a soft recurring drone). */
    float AmbientTimer = 0.0f;
    int32 AmbientRealmIndex = 0;

    /** Pending constellation pick + a counter that rotates the puzzle seed each start. */
    TArray<int32> PendingSelection;
    int32 ConstellationSeedCounter = 0;

    // --- Interactive Constellation Expedition (a playable campaign) ---
    bool bExpeditionActive = false;
    int32 ExpeditionLevel = 0;   // 0-based current level
    int32 ExpeditionScore = 0;   // cumulative score across cleared levels
    static constexpr int32 ExpeditionLevels = 5;
    static constexpr int64 ExpeditionBaseSeed = 8000;

    /** Seconds the intro title card has been showing. */
    float TitleTimer = 0.0f;

    /** Build a fresh interactive session (slice + objective) and the realm view. */
    void StartSession();

    /** Send any newly-emitted audio cues to the synth. */
    void PlayNewAudioCues();
};
