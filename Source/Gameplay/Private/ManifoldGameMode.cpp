// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "ManifoldGameMode.h"
#include "ManifoldSlice.h"
#include "ManifoldHUD.h"
#include "ManifoldPlayerController.h"
#include "ManifoldRealmVisualizer.h"
#include "ManifoldToneSynth.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/HUD.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"

AManifoldGameMode::AManifoldGameMode()
{
    PrimaryActorTick.bCanEverTick = true;

    // A free-fly camera to observe the realms, our on-screen readout, and a player
    // controller that binds the transport/restart verbs to keys (Enhanced Input).
    DefaultPawnClass = ADefaultPawn::StaticClass();
    HUDClass = AManifoldHUD::StaticClass();
    PlayerControllerClass = AManifoldPlayerController::StaticClass();

    // A reachable-but-earned goal: surface four discoveries (the Orbits<->Fluids
    // ignition plus the three cross-domain 3:2 analogies). Unlimited time by default.
    Objective.TargetDiscoveries = 4;
    Objective.StepBudget = 0;
}

FString AManifoldGameMode::ProfilePath() const
{
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Manifold.manifoldprofile"));
}

void AManifoldGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Restore the player's profile (best score, sessions) if one exists; on any load
    // failure (missing/corrupt/wrong-version) start from a clean default rather than
    // risk saving a partially-read profile back over the real one.
    if (!UManifoldSlice::LoadProfile(Profile, ProfilePath()))
    {
        Profile = FManifoldProfile();
    }

    // Real-time procedural synth: no sound asset needed, tones are generated in code.
    Synth = NewObject<UManifoldSynthComponent>(this, TEXT("ManifoldSynth"));
    Synth->RegisterComponent();
    Synth->Start(); // begin generating audio; PlayCue triggers tones

    StartSession();
}

void AManifoldGameMode::StartSession()
{
    Slice = NewObject<UManifoldSlice>(this);
    Accumulator = 0.0f;
    LastPlayedCue = 0;
    bSessionRecorded = false;
    bNewBestThisSession = false;
    bTitleShown = true;
    TitleTimer = 0.0f;
    PendingSelection.Reset();

    if (PlayMode == EManifoldPlayMode::Constellation)
    {
        if (bExpeditionActive)
        {
            // Campaign: a fixed sequence of escalating puzzles (rule hidden from level 3).
            const int64 Seed = ExpeditionBaseSeed + ExpeditionLevel;
            const bool bExpert = (ExpeditionLevel >= 2);
            Slice->SetupConstellation(Seed, 3, bExpert);
        }
        else
        {
            // A fresh puzzle each start: the seed rotates so the relation/constellation vary.
            const int64 Seed = 7001 + ConstellationSeedCounter;
            ++ConstellationSeedCounter;
            Slice->SetupConstellation(Seed, 3, bConstellationExpert);
        }
    }
    else
    {
        Slice->SetObjective(Objective);
        Slice->Setup(1111ULL, 2222ULL);
        // Interactive: the correspondence lights up, but the PLAYER triggers transport.
        // Set AFTER Setup (which resets the flag to its auto default).
        Slice->bAutoTransportOnIgnite = false;
    }

    // Spawn the debug-draw view of both realms + the resonance/seam ribbons (once).
    if (UWorld* World = GetWorld())
    {
        AManifoldRealmVisualizer* Existing = nullptr;
        for (TActorIterator<AManifoldRealmVisualizer> It(World); It; ++It) { Existing = *It; break; }
        if (!Existing)
        {
            World->SpawnActor<AManifoldRealmVisualizer>();
        }
    }
}

void AManifoldGameMode::ExitExpedition()
{
    bExpeditionActive = false;
    ExpeditionLevel = 0;
    ExpeditionScore = 0;
}

void AManifoldGameMode::ManifoldRestart()
{
    // [R] means "restart from scratch" (see header contract). Leave any active expedition campaign
    // entirely — otherwise StartSession sees bExpeditionActive and re-generates the CURRENT level
    // with the accumulated running total still banked and the banner still up, with no way out.
    ExitExpedition();
    StartSession();
    UE_LOG(LogTemp, Display, TEXT("[MANIFOLD] Session restarted"));
}

void AManifoldGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!Slice) return;

    // One-time: place the free-fly camera to frame the realm grid (X: -900..900, Z: 250..560, in
    // the Y=0 plane) so the visualization is on screen from the start rather than the player having
    // to fly to find it. Pulls back on -Y and looks toward the cluster centre.
    if (!bCameraFramed)
    {
        if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
        {
            if (APawn* P = PC->GetPawn())
            {
                bCameraFramed = true;
                P->SetActorLocation(FVector(0.0, -1150.0, 405.0));
                P->SetActorRotation(FRotator(0.0, 90.0, 0.0));
                PC->SetControlRotation(FRotator(0.0, 90.0, 0.0)); // look +Y toward the realm grid
            }
        }
    }

    // Auto-dismiss the intro title card after a few seconds.
    if (bTitleShown)
    {
        TitleTimer += DeltaSeconds;
        if (TitleTimer >= 6.0f) { bTitleShown = false; }
    }

    // Dev/CI affordance: `-ManifoldAutoShot` reveals the realm scene immediately and captures two
    // frame-counted HighResShots (frame-counted, not world-time — robust to the PSO/shader hitches
    // that make world-time crawl on a cold offscreen run) for headless visual verification and
    // marketing stills: first WITH the HUD (the in-game readout), then with the HUD hidden (the pure
    // 3D realm scene). Console commands route through the player controller so they reach the game
    // viewport's HighResShot handler (GEngine->Exec silently no-ops). Completely inert without the flag.
    if (!bAutoShotTaken && FParse::Param(FCommandLine::Get(), TEXT("ManifoldAutoShot")))
    {
        bTitleShown = false; // skip the intro card so the realms are on screen for the shot
        ++AutoShotFrames;
        if (AutoShotFrames == 1) { UE_LOG(LogTemp, Display, TEXT("[MANIFOLD] AutoShot armed")); }
        APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
        if (PC)
        {
            if (AutoShotFrames == 50)
            {
                UE_LOG(LogTemp, Display, TEXT("[MANIFOLD] AutoShot firing HUD shot at frame %d"), AutoShotFrames);
                PC->ConsoleCommand(TEXT("HighResShot 1280x720"), /*bWriteToLog*/ true);
            }
            else if (AutoShotFrames >= 100)
            {
                bAutoShotTaken = true;
                if (AHUD* HUD = PC->GetHUD()) { HUD->bShowHUD = false; }
                UE_LOG(LogTemp, Display, TEXT("[MANIFOLD] AutoShot firing beauty (HUD-off) shot at frame %d"), AutoShotFrames);
                PC->ConsoleCommand(TEXT("HighResShot 1280x720"), /*bWriteToLog*/ true);
            }
        }
    }

    // Classic mode advances the live simulation; Constellation is a static reasoning
    // puzzle resolved by locking a subset, so it isn't ticked.
    if (PlayMode == EManifoldPlayMode::Classic)
    {
        Accumulator += DeltaSeconds;
        while (Accumulator >= StepInterval)
        {
            Slice->Tick();
            Accumulator -= StepInterval;
        }
    }

    // Voice any cues the slice emitted this frame (discovery chimes, transport resolves).
    PlayNewAudioCues();

    // Gentle ambient pad: every few seconds, softly voice a realm's tonic so the world
    // always has a living, evolving hum under the event chimes.
    AmbientTimer += DeltaSeconds;
    if (Synth && Slice && Slice->Audio && AmbientTimer >= 3.0f)
    {
        AmbientTimer = 0.0f;
        static const FName Realms[] = {
            TEXT("Orbits"), TEXT("Harmonics"), TEXT("Waves"), TEXT("Rhythm"), TEXT("Gears"), TEXT("Circuits")
        };
        const FName Realm = Realms[AmbientRealmIndex % 6];
        ++AmbientRealmIndex;

        FManifoldAudioCue Cue = Slice->Audio->CueForRealmAmbient(Realm);
        Cue.Intensity = 0.25f; // soft, sits under everything
        Synth->PlayCue(Cue);
    }

    // When the session resolves, fold it into the persistent profile exactly once.
    if (!bSessionRecorded && Slice->GetSessionState() != EManifoldSessionState::InProgress)
    {
        bNewBestThisSession = UManifoldSlice::RecordSessionInProfile(Profile, Slice->GetSessionSummary());
        UManifoldSlice::SaveProfile(Profile, ProfilePath());

        // Save a shareable, re-watchable replay of a winning CLASSIC run (the replay
        // format records the transport schedule; a constellation lock is reproduced from
        // its seed instead, handled separately).
        if (PlayMode == EManifoldPlayMode::Classic && Slice->GetSessionState() == EManifoldSessionState::Won)
        {
            const FString ReplayPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("LastWin.manifoldreplay"));
            UManifoldSlice::SaveReplay(Slice->CaptureReplay(), ReplayPath);
        }
        bSessionRecorded = true;
    }
}

void AManifoldGameMode::PlayNewAudioCues()
{
    if (!Synth || !Slice) return;

    const TArray<FManifoldAudioCue>& Cues = Slice->GetAudioCues();
    for (int32 i = LastPlayedCue; i < Cues.Num(); ++i)
    {
        Synth->PlayCue(Cues[i]);
    }
    LastPlayedCue = Cues.Num();
}

void AManifoldGameMode::ManifoldTransport()
{
    if (!Slice) return;
    bTitleShown = false; // first action dismisses the intro
    const bool bOk = Slice->PlayerRequestTransport();
    UE_LOG(LogTemp, Display, TEXT("[MANIFOLD] Transport %s"),
        bOk ? TEXT("SUCCEEDED — power carried across the seam") : TEXT("failed — no correspondence lit"));
}

void AManifoldGameMode::ManifoldToggleMode()
{
    // Switching mode leaves any active expedition — otherwise its flag/score leak into the new mode
    // (swallowing the toggle while bExpeditionActive is still true, and silently resuming the old
    // campaign on a later switch back).
    ExitExpedition();

    // Cycle: Classic -> Constellation -> Constellation (Expert, rule hidden) -> Classic.
    if (PlayMode == EManifoldPlayMode::Classic)
    {
        PlayMode = EManifoldPlayMode::Constellation;
        bConstellationExpert = false;
    }
    else if (!bConstellationExpert)
    {
        bConstellationExpert = true; // same mode, now with the rule hidden
    }
    else
    {
        PlayMode = EManifoldPlayMode::Classic;
        bConstellationExpert = false;
    }

    StartSession();
    UE_LOG(LogTemp, Display, TEXT("[MANIFOLD] Mode: %s"),
        PlayMode == EManifoldPlayMode::Classic ? TEXT("Classic") :
        bConstellationExpert ? TEXT("Constellation Lock (Expert)") : TEXT("Constellation Lock"));
}

void AManifoldGameMode::ConstellationToggleRealm(int32 Index)
{
    if (!Slice || !Slice->IsConstellationMode()) return;
    if (Index < 0 || Index >= Slice->GetConstellationRealmCount()) return;
    if (Slice->GetSessionState() != EManifoldSessionState::InProgress) return;

    bTitleShown = false;
    if (PendingSelection.Contains(Index)) { PendingSelection.Remove(Index); }
    else { PendingSelection.Add(Index); }
}

void AManifoldGameMode::ConstellationLock()
{
    if (!Slice || !Slice->IsConstellationMode()) return;
    if (Slice->GetSessionState() != EManifoldSessionState::InProgress) return;

    bTitleShown = false;
    const bool bWon = Slice->PlayerLockConstellation(PendingSelection);
    UE_LOG(LogTemp, Display, TEXT("[MANIFOLD] Lock %s (%d realms) — probes used %d"),
        bWon ? TEXT("CORRECT — constellation complete") : TEXT("wrong — try another subset"),
        PendingSelection.Num(), Slice->GetFailedProbes());

    // A wrong lock clears the pick so the next attempt starts fresh.
    if (!bWon) { PendingSelection.Reset(); return; }

    // A correct lock during an expedition banks the level score and advances the campaign.
    if (bExpeditionActive)
    {
        ExpeditionScore += Slice->GetScore();
        ++ExpeditionLevel;
        if (ExpeditionLevel < ExpeditionLevels)
        {
            StartSession();      // load the next level
            bTitleShown = false; // no intro card between levels
        }
        else
        {
            // Campaign complete: bank the best, leave the final won level on screen.
            bExpeditionActive = false;
            Profile.BestExpeditionScore = FMath::Max(Profile.BestExpeditionScore, ExpeditionScore);
            UManifoldSlice::SaveProfile(Profile, ProfilePath());
            UE_LOG(LogTemp, Display, TEXT("[MANIFOLD] Expedition complete — total %d (best %d)"),
                ExpeditionScore, Profile.BestExpeditionScore);
        }
    }
}

void AManifoldGameMode::ManifoldStartExpedition()
{
    bExpeditionActive = true;
    ExpeditionLevel = 0;
    ExpeditionScore = 0;
    PlayMode = EManifoldPlayMode::Constellation;
    bConstellationExpert = false; // per-level difficulty is set by the campaign schedule
    StartSession();
    UE_LOG(LogTemp, Display, TEXT("[MANIFOLD] Constellation Expedition started (%d levels)"), ExpeditionLevels);
}

void AManifoldGameMode::ConstellationRevealNext()
{
    if (!Slice || !Slice->IsConstellationMode()) return;
    if (Slice->GetSessionState() != EManifoldSessionState::InProgress) return;

    bTitleShown = false;
    for (int32 i = 0; i < Slice->GetConstellationRealmCount(); ++i)
    {
        if (!Slice->IsRealmRevealed(i))
        {
            const bool bMember = Slice->PlayerRevealRealm(i);
            UE_LOG(LogTemp, Display, TEXT("[MANIFOLD] Revealed realm %d: %s (score paid)"),
                i + 1, bMember ? TEXT("IN the constellation") : TEXT("NOT in it"));
            return;
        }
    }
    UE_LOG(LogTemp, Display, TEXT("[MANIFOLD] All realms already revealed"));
}
