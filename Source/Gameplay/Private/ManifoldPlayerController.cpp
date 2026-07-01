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
