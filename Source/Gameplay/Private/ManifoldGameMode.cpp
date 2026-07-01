// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "ManifoldGameMode.h"
#include "ManifoldSlice.h"
#include "ManifoldHUD.h"
#include "GameFramework/DefaultPawn.h"

AManifoldGameMode::AManifoldGameMode()
{
    PrimaryActorTick.bCanEverTick = true;

    // A free-fly camera to observe the realms, plus our on-screen readout.
    DefaultPawnClass = ADefaultPawn::StaticClass();
    HUDClass = AManifoldHUD::StaticClass();
}

void AManifoldGameMode::BeginPlay()
{
    Super::BeginPlay();

    Slice = NewObject<UManifoldSlice>(this);
    // Interactive: the correspondence lights up, but the PLAYER triggers transport.
    Slice->bAutoTransportOnIgnite = false;
    Slice->Setup(1111ULL, 2222ULL);
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
}

void AManifoldGameMode::ManifoldTransport()
{
    if (!Slice) return;
    const bool bOk = Slice->PlayerRequestTransport();
    UE_LOG(LogTemp, Display, TEXT("[MANIFOLD] Transport %s"),
        bOk ? TEXT("SUCCEEDED — power carried across the seam") : TEXT("failed — no correspondence lit"));
}
