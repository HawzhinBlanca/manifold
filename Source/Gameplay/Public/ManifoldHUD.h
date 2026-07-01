// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ManifoldHUD.generated.h"

class UTexture2D;

/**
 * AManifoldHUD — the game's branded on-screen UI: the procedural MANIFOLD emblem, the
 * title, a framed live readout of each realm's structure + the Insight Rate, the lit
 * correspondence prompt, and a win/lose banner when the session resolves.
 */
UCLASS()
class MANIFOLDGAMEPLAY_API AManifoldHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;

private:
    /** The procedurally-generated emblem texture (built once, on first draw). */
    UPROPERTY()
    UTexture2D* Emblem = nullptr;

    void DrawPanel(float X, float Y, float W, float H, const FLinearColor& Color);
};
