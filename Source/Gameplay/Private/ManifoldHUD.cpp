// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "ManifoldHUD.h"
#include "ManifoldGameMode.h"
#include "ManifoldSlice.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"

void AManifoldHUD::DrawHUD()
{
    Super::DrawHUD();

    UWorld* World = GetWorld();
    AManifoldGameMode* GM = World ? World->GetAuthGameMode<AManifoldGameMode>() : nullptr;
    if (!GM || !GM->Slice) return;

    UManifoldSlice* S = GM->Slice;
    UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;

    float X = 40.0f;
    float Y = 40.0f;
    auto Line = [&](const FString& Text, const FLinearColor& Color)
    {
        DrawText(Text, Color, X, Y, Font);
        Y += 24.0f;
    };

    const FLinearColor Gold(1.0f, 0.85f, 0.2f);
    const FLinearColor Cyan(0.3f, 0.8f, 1.0f);
    const FLinearColor Dim(0.6f, 0.6f, 0.6f);

    Line(TEXT("MANIFOLD  -  vertical slice"), FLinearColor::White);
    Line(FString::Printf(TEXT("Step %lld"), S->GetStepCount()), Dim);
    Line(FString::Printf(TEXT("Orbits     |  resonance %s"), *S->GetOrbitsRatio()),
        S->HasResonance() ? Gold : Dim);
    Line(FString::Printf(TEXT("Fluids     |  vortex %s"), S->HasVortex() ? TEXT("present") : TEXT("-")),
        S->HasVortex() ? Cyan : Dim);
    Line(FString::Printf(TEXT("Harmonics  |  ratio %s"), *S->GetHarmonicsRatio()),
        FLinearColor(0.8f, 0.5f, 1.0f));
    Line(FString::Printf(TEXT("Cross-domain analogies found: %d   (orbital 3:2 == harmonic 3:2)"),
        S->GetSharedDiscoveries()),
        S->GetSharedDiscoveries() > 0 ? Gold : Dim);
    Line(FString::Printf(TEXT("Insight Rate %.3f   (lit %d, transported %d)"),
        S->GetInsightRate(), S->GetCorrespondencesIgnited(), S->GetTransportsCompleted()),
        FLinearColor::Green);

    if (S->IsCorrespondenceAvailable())
    {
        Line(TEXT(">> CORRESPONDENCE LIT  -  console: ManifoldTransport"), Gold);
    }
}
