// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UTexture2D;

/**
 * Procedurally-generated MANIFOLD emblem — real art authored in code (no external
 * asset). It draws the game's own iconography: two concentric "resonance" rings in a
 * 3:2 radius ratio, a central vortex spiral, and radial wave spokes, in gold on a deep
 * indigo field. Used for the title/HUD branding.
 *
 * Render() is pure and headlessly unit-tested; CreateTexture() wraps it in a runtime
 * UTexture2D for drawing.
 */
namespace ManifoldEmblem
{
    /** Render the emblem into an RGBA pixel buffer (BGRA byte order, Size x Size). */
    MANIFOLDGAMEPLAY_API void Render(TArray<FColor>& OutPixels, int32 Size);

    /** Build a transient UTexture2D of the emblem (runtime only). */
    MANIFOLDGAMEPLAY_API UTexture2D* CreateTexture(int32 Size = 256);
}
