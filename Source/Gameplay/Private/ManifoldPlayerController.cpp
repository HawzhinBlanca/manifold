// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "ManifoldPlayerController.h"
#include "ManifoldGameMode.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"

AManifoldPlayerController::AManifoldPlayerController()
{
    // Build the input assets in code so no editor content is required. Both verbs are
    // simple digital button presses.
    TransportAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Transport"));
    TransportAction->ValueType = EInputActionValueType::Boolean;

    RestartAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Restart"));
    RestartAction->ValueType = EInputActionValueType::Boolean;

    MappingContext = CreateDefaultSubobject<UInputMappingContext>(TEXT("IMC_Manifold"));
    MappingContext->MapKey(TransportAction, EKeys::E);
    MappingContext->MapKey(RestartAction, EKeys::R);

    // Constellation Lock: number keys 1-6 pick a realm, Space locks, C toggles mode.
    static const FKey NumberKeys[6] = {
        EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six
    };
    SelectActions.SetNum(6);
    for (int32 i = 0; i < 6; ++i)
    {
        SelectActions[i] = CreateDefaultSubobject<UInputAction>(
            *FString::Printf(TEXT("IA_SelectRealm%d"), i + 1));
        SelectActions[i]->ValueType = EInputActionValueType::Boolean;
        MappingContext->MapKey(SelectActions[i], NumberKeys[i]);
    }

    LockAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Lock"));
    LockAction->ValueType = EInputActionValueType::Boolean;
    MappingContext->MapKey(LockAction, EKeys::SpaceBar);

    ModeAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_ToggleMode"));
    ModeAction->ValueType = EInputActionValueType::Boolean;
    MappingContext->MapKey(ModeAction, EKeys::C);

    RevealAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Reveal"));
    RevealAction->ValueType = EInputActionValueType::Boolean;
    MappingContext->MapKey(RevealAction, EKeys::V);
}

void AManifoldPlayerController::BeginPlay()
{
    Super::BeginPlay();

    // Register the mapping context with the local player's Enhanced Input subsystem.
    if (ULocalPlayer* LP = GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
                ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
        {
            Subsystem->AddMappingContext(MappingContext, /*Priority*/ 0);
        }
    }

    // Bind the actions (requires the project's input component to be Enhanced Input;
    // see Config/DefaultInput.ini).
    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
    {
        EIC->BindAction(TransportAction, ETriggerEvent::Started, this, &AManifoldPlayerController::OnTransport);
        EIC->BindAction(RestartAction, ETriggerEvent::Started, this, &AManifoldPlayerController::OnRestart);

        if (SelectActions.Num() == 6)
        {
            EIC->BindAction(SelectActions[0], ETriggerEvent::Started, this, &AManifoldPlayerController::OnSelect1);
            EIC->BindAction(SelectActions[1], ETriggerEvent::Started, this, &AManifoldPlayerController::OnSelect2);
            EIC->BindAction(SelectActions[2], ETriggerEvent::Started, this, &AManifoldPlayerController::OnSelect3);
            EIC->BindAction(SelectActions[3], ETriggerEvent::Started, this, &AManifoldPlayerController::OnSelect4);
            EIC->BindAction(SelectActions[4], ETriggerEvent::Started, this, &AManifoldPlayerController::OnSelect5);
            EIC->BindAction(SelectActions[5], ETriggerEvent::Started, this, &AManifoldPlayerController::OnSelect6);
        }
        EIC->BindAction(LockAction, ETriggerEvent::Started, this, &AManifoldPlayerController::OnLock);
        EIC->BindAction(ModeAction, ETriggerEvent::Started, this, &AManifoldPlayerController::OnToggleMode);
        EIC->BindAction(RevealAction, ETriggerEvent::Started, this, &AManifoldPlayerController::OnReveal);
    }
}

void AManifoldPlayerController::OnTransport()
{
    if (UWorld* World = GetWorld())
    {
        if (AManifoldGameMode* GM = World->GetAuthGameMode<AManifoldGameMode>())
        {
            GM->ManifoldTransport();
        }
    }
}

void AManifoldPlayerController::OnRestart()
{
    if (UWorld* World = GetWorld())
    {
        if (AManifoldGameMode* GM = World->GetAuthGameMode<AManifoldGameMode>())
        {
            GM->ManifoldRestart();
        }
    }
}

void AManifoldPlayerController::SelectRealm(int32 Index)
{
    if (UWorld* World = GetWorld())
    {
        if (AManifoldGameMode* GM = World->GetAuthGameMode<AManifoldGameMode>())
        {
            GM->ConstellationToggleRealm(Index);
        }
    }
}

void AManifoldPlayerController::OnSelect1() { SelectRealm(0); }
void AManifoldPlayerController::OnSelect2() { SelectRealm(1); }
void AManifoldPlayerController::OnSelect3() { SelectRealm(2); }
void AManifoldPlayerController::OnSelect4() { SelectRealm(3); }
void AManifoldPlayerController::OnSelect5() { SelectRealm(4); }
void AManifoldPlayerController::OnSelect6() { SelectRealm(5); }

void AManifoldPlayerController::OnLock()
{
    if (UWorld* World = GetWorld())
    {
        if (AManifoldGameMode* GM = World->GetAuthGameMode<AManifoldGameMode>())
        {
            GM->ConstellationLock();
        }
    }
}

void AManifoldPlayerController::OnToggleMode()
{
    if (UWorld* World = GetWorld())
    {
        if (AManifoldGameMode* GM = World->GetAuthGameMode<AManifoldGameMode>())
        {
            GM->ManifoldToggleMode();
        }
    }
}

void AManifoldPlayerController::OnReveal()
{
    if (UWorld* World = GetWorld())
    {
        if (AManifoldGameMode* GM = World->GetAuthGameMode<AManifoldGameMode>())
        {
            GM->ConstellationRevealNext();
        }
    }
}
