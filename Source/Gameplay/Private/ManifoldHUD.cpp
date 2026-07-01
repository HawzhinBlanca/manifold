// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "ManifoldHUD.h"
#include "ManifoldGameMode.h"
#include "ManifoldSlice.h"
#include "ManifoldEmblem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"

void AManifoldHUD::DrawPanel(float X, float Y, float W, float H, const FLinearColor& Color)
{
    DrawRect(Color, X, Y, W, H);
}

void AManifoldHUD::DrawHUD()
{
    Super::DrawHUD();

    UWorld* World = GetWorld();
    AManifoldGameMode* GM = World ? World->GetAuthGameMode<AManifoldGameMode>() : nullptr;
    if (!GM || !GM->Slice) return;
    UManifoldSlice* S = GM->Slice;

    // Build the branded emblem texture once.
    if (!Emblem)
    {
        Emblem = ManifoldEmblem::CreateTexture(256);
    }

    UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
    UFont* Big = GEngine ? GEngine->GetLargeFont() : Font;

    const FLinearColor Gold(1.0f, 0.85f, 0.2f);
    const FLinearColor Cyan(0.3f, 0.8f, 1.0f);
    const FLinearColor Dim(0.62f, 0.66f, 0.74f);
    const FLinearColor Violet(0.8f, 0.5f, 1.0f);
    const FLinearColor Amber(1.0f, 0.6f, 0.4f);
    const FLinearColor PanelBg(0.02f, 0.03f, 0.07f, 0.72f);

    // --- Intro title card (over the live realms + starfield), until first action ---
    if (GM->bTitleShown)
    {
        const float CX = Canvas ? Canvas->ClipX * 0.5f : 640.0f;
        const float CY = Canvas ? Canvas->ClipY * 0.5f : 360.0f;

        DrawPanel(CX - 320.0f, CY - 180.0f, 640.0f, 340.0f, FLinearColor(0.02f, 0.03f, 0.07f, 0.8f));
        if (Emblem)
        {
            DrawTexture(Emblem, CX - 70.0f, CY - 160.0f, 140.0f, 140.0f, 0, 0, 1, 1);
        }
        if (Big)
        {
            DrawText(TEXT("MANIFOLD"), Gold, CX - 132.0f, CY + 0.0f, Big, 2.2f);
        }
        DrawText(TEXT("the correspondence engine"), Dim, CX - 96.0f, CY + 44.0f, Font);
        DrawText(TEXT("Five realms secretly share one hidden ratio. One is a decoy."),
            Cyan, CX - 210.0f, CY + 78.0f, Font);
        DrawText(TEXT("Find the correspondence and carry it across the seam."),
            Cyan, CX - 195.0f, CY + 100.0f, Font);
        DrawText(TEXT("[E] transport     [R] restart"), Gold, CX - 110.0f, CY + 134.0f, Font);
        return; // show only the title over the scene until the player begins
    }

    // --- Title bar: emblem + wordmark + framed readout panel ---
    const float PanelX = 32.0f;
    const float PanelY = 28.0f;
    const float PanelW = 560.0f;
    const float PanelH = 384.0f;
    DrawPanel(PanelX, PanelY, PanelW, PanelH, PanelBg);

    if (Emblem)
    {
        // Emblem top-left inside the panel.
        DrawTexture(Emblem, PanelX + 14.0f, PanelY + 14.0f, 56.0f, 56.0f, 0, 0, 1, 1);
    }
    if (Big)
    {
        DrawText(TEXT("MANIFOLD"), Gold, PanelX + 84.0f, PanelY + 20.0f, Big, 1.35f);
    }
    DrawText(TEXT("the correspondence engine"), Dim, PanelX + 86.0f, PanelY + 52.0f, Font);

    float X = PanelX + 20.0f;
    float Y = PanelY + 86.0f;
    auto Line = [&](const FString& Text, const FLinearColor& Color)
    {
        DrawText(Text, Color, X, Y, Font);
        Y += 24.0f;
    };

    Line(FString::Printf(TEXT("Step %lld"), S->GetStepCount()), Dim);
    Line(FString::Printf(TEXT("Orbits     |  resonance %s"), *S->GetOrbitsRatio()),
        S->HasResonance() ? Gold : Dim);
    Line(FString::Printf(TEXT("Fluids     |  vortex %s"), S->HasVortex() ? TEXT("present") : TEXT("-")),
        S->HasVortex() ? Cyan : Dim);
    Line(FString::Printf(TEXT("Harmonics  |  ratio %s"), *S->GetHarmonicsRatio()), Violet);
    Line(FString::Printf(TEXT("Rhythm     |  polyrhythm %s"), *S->GetRhythmRatio()), Amber);
    Line(FString::Printf(TEXT("Gears      |  gear ratio %s"), *S->GetGearsRatio()),
        FLinearColor(0.7f, 0.75f, 0.8f));
    Line(FString::Printf(TEXT("Decoy      |  ratio %s   (red herring - rule it out)"), *S->GetDecoyRatio()),
        FLinearColor(0.5f, 0.5f, 0.55f));
    Line(FString::Printf(TEXT("Analogies found: %d   (the hidden ratio, across domains)"),
        S->GetSharedDiscoveries()), S->GetSharedDiscoveries() > 0 ? Gold : Dim);
    Line(FString::Printf(TEXT("Insight Rate %.3f   (lit %d, transported %d)"),
        S->GetInsightRate(), S->GetCorrespondencesIgnited(), S->GetTransportsCompleted()),
        FLinearColor::Green);

    // --- Objective + last audio cue ---
    const FManifoldSessionSummary Sum = S->GetSessionSummary();
    if (S->GetAudioCueCount() > 0)
    {
        const FManifoldAudioCue Cue = S->GetLastAudioCue();
        const TCHAR* Intent =
            Cue.Intent == EManifoldCueIntent::DiscoveryChime ? TEXT("chime") :
            Cue.Intent == EManifoldCueIntent::ChordResolve   ? TEXT("resolve") : TEXT("ambient");
        Line(FString::Printf(TEXT("Objective %d/%d    Audio: %s +%d st"),
            Sum.Discoveries, GM->Objective.TargetDiscoveries, Intent, Cue.IntervalSemitones), Dim);
    }
    Line(FString::Printf(TEXT("Score %d    Best %d    (won %d/%d)"),
        Sum.Score, GM->Profile.BestScore, GM->Profile.SessionsWon, GM->Profile.SessionsPlayed), Gold);

    // --- Lit-correspondence prompt + controls ---
    if (S->IsCorrespondenceAvailable())
    {
        DrawText(TEXT(">> CORRESPONDENCE LIT  -  press [E] to carry it across the seam"),
            Gold, PanelX + 20.0f, PanelY + PanelH - 30.0f, Font);
    }
    else
    {
        DrawText(TEXT("[E] transport    [R] restart"), Dim, PanelX + 20.0f, PanelY + PanelH - 30.0f, Font);
    }

    // --- Win / lose banner, centered ---
    if (Sum.State != EManifoldSessionState::InProgress)
    {
        const float CX = Canvas ? Canvas->ClipX * 0.5f : 640.0f;
        const float CY = Canvas ? Canvas->ClipY * 0.42f : 360.0f;
        const bool bWon = Sum.State == EManifoldSessionState::Won;

        DrawPanel(CX - 300.0f, CY - 70.0f, 600.0f, 168.0f, FLinearColor(0.02f, 0.03f, 0.07f, 0.85f));
        if (Emblem)
        {
            DrawTexture(Emblem, CX - 48.0f, CY - 58.0f, 96.0f, 96.0f, 0, 0, 1, 1);
        }
        const FString Title = bWon ? TEXT("CORRESPONDENCE COMPLETE") : TEXT("SESSION LOST");
        const FLinearColor TitleCol = bWon ? Gold : FLinearColor(1.0f, 0.35f, 0.35f);
        if (Big)
        {
            DrawText(Title, TitleCol, CX - 230.0f, CY + 44.0f, Big, 1.25f);
        }
        // Big rank grade.
        const TCHAR* RankLetter =
            Sum.Rank == EManifoldRank::S ? TEXT("S") :
            Sum.Rank == EManifoldRank::A ? TEXT("A") :
            Sum.Rank == EManifoldRank::B ? TEXT("B") :
            Sum.Rank == EManifoldRank::C ? TEXT("C") : TEXT("D");
        if (Big)
        {
            DrawText(FString::Printf(TEXT("RANK %s"), RankLetter), Gold, CX + 150.0f, CY + 40.0f, Big, 1.8f);
        }
        DrawText(FString::Printf(TEXT("Score %d  (best %d)    %d discoveries    [R] play again"),
            Sum.Score, GM->Profile.BestScore, Sum.Discoveries), Dim, CX - 230.0f, CY + 72.0f, Font);
    }
}
