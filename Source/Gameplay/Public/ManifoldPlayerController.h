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

    // Constellation Lock verbs: pick realms 1-6, lock the subset, toggle play mode.
    void OnSelect1(); void OnSelect2(); void OnSelect3();
    void OnSelect4(); void OnSelect5(); void OnSelect6();
    void OnLock();
    void OnToggleMode();
    void OnReveal();
    void OnExpedition();

private:
    /** Route a realm pick (0-based) to the game mode. */
    void SelectRealm(int32 Index);

    UPROPERTY()
    UInputAction* TransportAction = nullptr;

    UPROPERTY()
    UInputAction* RestartAction = nullptr;

    /** One action per realm number key (1-6). */
    UPROPERTY()
    TArray<UInputAction*> SelectActions;

    UPROPERTY()
    UInputAction* LockAction = nullptr;

    UPROPERTY()
    UInputAction* ModeAction = nullptr;

    UPROPERTY()
    UInputAction* RevealAction = nullptr;

    UPROPERTY()
    UInputAction* ExpeditionAction = nullptr;

    UPROPERTY()
    UInputMappingContext* MappingContext = nullptr;
};
