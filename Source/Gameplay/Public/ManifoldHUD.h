// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ManifoldHUD.generated.h"

/**
 * AManifoldHUD — minimal on-screen readout of the live session: each realm's
 * structure, whether a correspondence is lit, and the running Insight Rate.
 * A debug-legible front end until the full UI/VFX layer exists.
 */
UCLASS()
class MANIFOLDGAMEPLAY_API AManifoldHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;
};
