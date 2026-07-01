// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "ManifoldGameMode.h"
#include "ManifoldSlice.h"
#include "ManifoldHUD.h"
#include "ManifoldPlayerController.h"
#include "ManifoldRealmVisualizer.h"
#include "ManifoldToneSynth.h"
#include "GameFramework/DefaultPawn.h"
#include "Engine/World.h"
#include "EngineUtils.h"

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

void AManifoldGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Real-time procedural synth: no sound asset needed, tones are generated in code.
    Synth = NewObject<UManifoldSynthComponent>(this, TEXT("ManifoldSynth"));
    Synth->RegisterComponent();
    Synth->Start(); // begin generating audio; PlayCue triggers tones

    StartSession();
}

void AManifoldGameMode::StartSession()
{
    Slice = NewObject<UManifoldSlice>(this);
    // Interactive: the correspondence lights up, but the PLAYER triggers transport.
    Slice->bAutoTransportOnIgnite = false;
    Slice->SetObjective(Objective);
    Slice->Setup(1111ULL, 2222ULL);
    Accumulator = 0.0f;
    LastPlayedCue = 0;

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

void AManifoldGameMode::ManifoldRestart()
{
    StartSession();
    UE_LOG(LogTemp, Display, TEXT("[MANIFOLD] Session restarted"));
}

void AManifoldGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!Slice) return;

    Accumulator += DeltaSeconds;
    while (Accumulator >= StepInterval)
    {
        Slice->Tick();
        Accumulator -= StepInterval;
    }

    // Voice any cues the slice emitted this frame (discovery chimes, transport resolves).
    PlayNewAudioCues();
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
    const bool bOk = Slice->PlayerRequestTransport();
    UE_LOG(LogTemp, Display, TEXT("[MANIFOLD] Transport %s"),
        bOk ? TEXT("SUCCEEDED — power carried across the seam") : TEXT("failed — no correspondence lit"));
}
