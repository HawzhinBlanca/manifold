// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ManifoldPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;

/**
 * AManifoldPlayerController — binds the game's verbs to keys via Enhanced Input.
 *
 * The InputAction / InputMappingContext are built in code (no editor content asset
 * required), so the whole binding ships in source: [E] transports the currently-lit
 * correspondence across the seam, [R] restarts the session.
 */
UCLASS()
class MANIFOLDGAMEPLAY_API AManifoldPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AManifoldPlayerController();

protected:
    virtual void BeginPlay() override;

    void OnTransport();
    void OnRestart();

private:
    UPROPERTY()
    UInputAction* TransportAction = nullptr;

    UPROPERTY()
    UInputAction* RestartAction = nullptr;

    UPROPERTY()
    UInputMappingContext* MappingContext = nullptr;
};
