// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Central, colorblind-safe realm palette — the single source of truth for realm colors, shared by
 * the visualizer and locked by the automation test MANIFOLD.Art.PaletteColorblindSafe.
 *
 * The game asks the player to tell seven simulation realms apart, partly by colour, so mutual
 * distinguishability is a real accessibility requirement, not decoration. These are the Okabe-Ito
 * colorblind-safe qualitative palette (Okabe & Ito, 2008), which is specifically designed to remain
 * separable under deuteranopia, protanopia and tritanopia. FLinearColor is LINEAR, so the sRGB hex
 * values are converted to linear RGB here (that conversion is why, e.g., #0072B2 becomes ~0.168 G).
 */
namespace ManifoldPalette
{
    // Realm colors in realm-index order (linear RGB from the Okabe-Ito sRGB hex noted alongside).
    inline const FLinearColor Orbits   (0.000f, 0.168f, 0.445f); // Blue           #0072B2
    inline const FLinearColor Fluids   (0.093f, 0.456f, 0.815f); // Sky Blue       #56B4E9
    inline const FLinearColor Harmonics(0.604f, 0.191f, 0.387f); // Reddish Purple #CC79A7
    inline const FLinearColor Waves    (0.000f, 0.342f, 0.171f); // Bluish Green   #009E73
    inline const FLinearColor Rhythm   (0.791f, 0.347f, 0.000f); // Orange         #E69F00
    inline const FLinearColor Gears    (0.871f, 0.776f, 0.054f); // Yellow         #F0E442
    inline const FLinearColor Circuits (0.665f, 0.112f, 0.000f); // Vermillion     #D55E00

    // The decoy is deliberately a desaturated neutral: it must read as "not one of the family",
    // yet still stay distinguishable from every realm (the test checks this too).
    inline const FLinearColor Decoy    (0.210f, 0.210f, 0.240f);

    // Accents (not part of the realm-distinctness set): the central star and the correspondence seam.
    inline const FLinearColor Star     (1.000f, 0.850f, 0.250f);
    inline const FLinearColor Seam     (1.000f, 0.800f, 0.200f);

    /** The seven realm colors in index order, for iteration by tools/tests. */
    inline const TArray<FLinearColor>& RealmColors()
    {
        static const TArray<FLinearColor> Colors = { Orbits, Fluids, Harmonics, Waves, Rhythm, Gears, Circuits };
        return Colors;
    }
}
