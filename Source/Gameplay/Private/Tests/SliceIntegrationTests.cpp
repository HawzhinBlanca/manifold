// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "ManifoldSlice.h"
#include "ManifoldTypes.h"
#include "OrbitsKernel.h"
#include "FluidsKernel.h"
#include "HarmonicsKernel.h"
#include "WavesKernel.h"
#include "RhythmKernel.h"
#include "RealmKernel.h"
#include "CorrespondenceSystem.h"
#include "TelemetrySystem.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

// End-to-end integration acceptance: proves the whole vertical-slice loop runs —
// two realms simulate, a correspondence is detected across the seam, power is
// transported, and the Insight Rate is positive (Build Plan §6 integration point).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSliceVerticalLoopTest, "MANIFOLD.Integration.VerticalSliceLoop", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSliceVerticalLoopTest::RunTest(const FString& Parameters)
{
    UManifoldSlice* Slice = NewObject<UManifoldSlice>();
    Slice->Setup(1111ULL, 2222ULL);

    FManifoldSliceResult Result = Slice->RunPlaythrough(30);

    UTEST_TRUE("Orbits resonance detected during playthrough", Result.bResonanceDetected);
    UTEST_TRUE("Fluids vortex detected during playthrough", Result.bVortexDetected);
    UTEST_GREATER("Correspondence ignited at least once", Result.CorrespondencesIgnited, 0);
    UTEST_GREATER("Transport completed at least once", Result.TransportsCompleted, 0);
    UTEST_GREATER("Insight Rate is positive", Result.InsightRate, 0.0f);

    return true;
}

// Control build (Build Plan D3): with NO correspondence content the loop must NOT
// manufacture insight — the moat is that unsolved seams stay unsolved. Here we run
// only the Fluids realm (no resonance to correspond with), so nothing ignites.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSliceControlNoCorrespondenceTest, "MANIFOLD.Integration.ControlNoCorrespondence", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSliceControlNoCorrespondenceTest::RunTest(const FString& Parameters)
{
    // Build a control slice by hand with only ONE planet: a single body yields no
    // body-body resonance, so the seam has nothing to match and nothing ignites.
    UManifoldSlice* Control = NewObject<UManifoldSlice>();
    Control->Orbits = NewObject<UOrbitsKernel>(Control);
    Control->Fluids = NewObject<UFluidsKernel>(Control);
    Control->Correspond = NewObject<UCorrespondenceSystem>(Control);
    Control->Telemetry = NewObject<UTelemetrySystem>(Control);
    Control->Orbits->Initialize(1111ULL);
    Control->Fluids->Initialize(2222ULL);
    Control->Correspond->RegisterKernels(Control->Orbits, Control->Fluids);
    Control->Telemetry->InitializeTelemetry(TEXT("ControlPlaythrough.log"));

    FOrbitsBodyDef Star; Star.Name = TEXT("Star"); Star.Mass = 1.989e30; Star.bIsCentral = true;
    Control->Orbits->AddBody(Star);
    FOrbitsBodyDef Planet; Planet.Name = TEXT("Solo"); Planet.Mass = 1e24; Planet.Position = FVector(1.496e11, 0.0, 0.0);
    const double V = FMath::Sqrt((Control->Orbits->G * Star.Mass) / Planet.Position.X);
    Planet.Velocity = FVector(0.0, V, 0.0);
    Control->Orbits->AddBody(Planet);
    Control->Fluids->AddVelocity(0.5f, 0.45f, 20.0f, 0.0f);

    for (int32 i = 0; i < 30; ++i)
    {
        Control->Orbits->Step(0.1f);
        Control->Fluids->Step(0.016f);
        Control->Correspond->DetectCorrespondence();
    }

    UTEST_EQUAL("Control: no resonance with a single planet", Control->Orbits->GetActiveResonances().Num(), 0);
    UTEST_EQUAL("Control: Insight Rate stays zero without a correspondence", Control->Telemetry->CalculateInsightRate(), 0.0f);

    return true;
}

// The core mechanic, generalized to N realms: ANY two realms exposing the same
// structure ratio correspond. Here Orbits (3:2 mean-motion resonance) and Harmonics
// (3:2 frequency ratio) share the "3:2" structure across totally different domains —
// the cross-domain analogy that is the heart of MANIFOLD.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMultiRealmCorrespondenceTest, "MANIFOLD.Integration.MultiRealmCorrespondence", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMultiRealmCorrespondenceTest::RunTest(const FString& Parameters)
{
    // Orbits with a 3:2 mean-motion resonance.
    UOrbitsKernel* Orbits = NewObject<UOrbitsKernel>();
    Orbits->Initialize(1111ULL);
    FOrbitsBodyDef Star; Star.Mass = 1.989e30; Star.bIsCentral = true; Orbits->AddBody(Star);
    FOrbitsBodyDef A; A.Mass = 1e24; A.Position = FVector(1.496e11, 0.0, 0.0);
    const double vA = FMath::Sqrt((Orbits->G * Star.Mass) / A.Position.X); A.Velocity = FVector(0.0, vA, 0.0);
    Orbits->AddBody(A);
    const double rB = A.Position.X * FMath::Pow(1.5, 2.0 / 3.0);
    FOrbitsBodyDef B; B.Mass = 1e24; B.Position = FVector(rB, 0.0, 0.0);
    const double vB = FMath::Sqrt((Orbits->G * Star.Mass) / rB); B.Velocity = FVector(0.0, vB, 0.0);
    Orbits->AddBody(B);
    Orbits->Step(0.1f);

    // Harmonics with a 3:2 frequency ratio (2 Hz vs 3 Hz).
    UHarmonicsKernel* Harmonics = NewObject<UHarmonicsKernel>();
    Harmonics->Initialize(999ULL);
    Harmonics->AddMode(2.0, 1.0);
    Harmonics->AddMode(3.0, 1.0);
    Harmonics->Step(0.01f);

    UCorrespondenceSystem* Correspond = NewObject<UCorrespondenceSystem>();
    Correspond->RegisterRealm(TEXT("Orbits"), TEXT("OrbitalResonance"), Orbits);
    Correspond->RegisterRealm(TEXT("Harmonics"), TEXT("HarmonicRatio"), Harmonics);

    bool bDiscovered = false;
    FString DiscoveredRatio;
    Correspond->OnSharedStructureDiscovered.AddLambda(
        [&bDiscovered, &DiscoveredRatio](FName, FName, FString Ratio, FGuid Id)
        {
            bDiscovered = true;
            DiscoveredRatio = Ratio;
            // The shared-structure id must be real (stable), not a throwaway zero/GUID.
            check(Id.IsValid());
        });

    const int32 Found = Correspond->DetectSharedStructureCorrespondences();
    UTEST_GREATER("Shared 3:2 structure found across Orbits and Harmonics", Found, 0);
    UTEST_TRUE("Cross-domain correspondence discovered", bDiscovered);
    UTEST_EQUAL("Discovery carries the real shared ratio", DiscoveredRatio, FString(TEXT("3:2")));

    // Idempotent: the same shared structure does not re-ignite.
    UTEST_EQUAL("No duplicate ignition on re-detect", Correspond->DetectSharedStructureCorrespondences(), 0);

    return true;
}

// The playable-session logic behind the interactive shell: tick the slice over
// time, the correspondence lights up (but does NOT auto-transport), then the
// PLAYER'S verb carries it across the seam. This is the loop a human will feel.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractiveSessionTest, "MANIFOLD.Play.InteractiveSession", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FInteractiveSessionTest::RunTest(const FString& Parameters)
{
    UManifoldSlice* Slice = NewObject<UManifoldSlice>();
    Slice->bAutoTransportOnIgnite = false; // interactive: player triggers transport
    Slice->Setup(1111ULL, 2222ULL);

    for (int32 i = 0; i < 30; ++i)
    {
        Slice->Tick();
    }

    UTEST_TRUE("Resonance detected", Slice->HasResonance());
    UTEST_TRUE("Vortex detected", Slice->HasVortex());
    // All ratio realms realize this session's hidden ratio (whatever the seed chose).
    UTEST_EQUAL("Orbits realizes the hidden ratio", Slice->GetOrbitsRatio(), Slice->GetSharedRatio());
    UTEST_EQUAL("Harmonics realizes the hidden ratio", Slice->GetHarmonicsRatio(), Slice->GetSharedRatio());
    UTEST_GREATER("Cross-domain analogy (Orbits<->Harmonics) discovered", Slice->GetSharedDiscoveries(), 0);
    UTEST_GREATER("Correspondence lit at least once", Slice->GetCorrespondencesIgnited(), 0);
    UTEST_GREATER("Insight Rate positive", Slice->GetInsightRate(), 0.0f);
    UTEST_TRUE("Correspondence available to the player", Slice->IsCorrespondenceAvailable());
    UTEST_EQUAL("No auto-transport in interactive mode", Slice->GetTransportsCompleted(), 0);

    // The player's verb.
    UTEST_TRUE("Player transport succeeds when a correspondence is lit", Slice->PlayerRequestTransport());
    UTEST_GREATER("Transport recorded", Slice->GetTransportsCompleted(), 0);

    return true;
}

// Vertical-slice GATE (Build Plan §5 Stream P / P2): the numeric go/no-go. The
// slice with a correspondence must produce a positive Insight Rate; the control
// (no correspondence) must produce exactly zero — and the slice must strictly beat
// the control. This is the "prove the loop compounds vs. control" criterion in code.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVerticalSliceGateTest, "MANIFOLD.Play.VerticalSliceGate", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FVerticalSliceGateTest::RunTest(const FString& Parameters)
{
    // Treatment: the full slice.
    UManifoldSlice* Slice = NewObject<UManifoldSlice>();
    Slice->Setup(1111ULL, 2222ULL);
    const FManifoldSliceResult Treatment = Slice->RunPlaythrough(30);

    // Control: a single-planet Orbits + Fluids, no resonance -> no correspondence.
    UManifoldSlice* Control = NewObject<UManifoldSlice>();
    Control->Orbits = NewObject<UOrbitsKernel>(Control);
    Control->Fluids = NewObject<UFluidsKernel>(Control);
    Control->Correspond = NewObject<UCorrespondenceSystem>(Control);
    Control->Telemetry = NewObject<UTelemetrySystem>(Control);
    Control->Orbits->Initialize(1111ULL);
    Control->Fluids->Initialize(2222ULL);
    Control->Correspond->RegisterKernels(Control->Orbits, Control->Fluids);
    Control->Telemetry->InitializeTelemetry(TEXT("GateControl.log"));
    FOrbitsBodyDef Star; Star.Mass = 1.989e30; Star.bIsCentral = true; Control->Orbits->AddBody(Star);
    FOrbitsBodyDef Planet; Planet.Mass = 1e24; Planet.Position = FVector(1.496e11, 0.0, 0.0);
    const double V = FMath::Sqrt((Control->Orbits->G * Star.Mass) / Planet.Position.X);
    Planet.Velocity = FVector(0.0, V, 0.0);
    Control->Orbits->AddBody(Planet);
    Control->Fluids->AddVelocity(0.5f, 0.45f, 20.0f, 0.0f);
    for (int32 i = 0; i < 30; ++i)
    {
        Control->Orbits->Step(0.1f);
        Control->Fluids->Step(0.016f);
        Control->Correspond->DetectCorrespondence();
    }
    const float ControlInsight = Control->Telemetry->CalculateInsightRate();

    UTEST_GREATER("GATE: treatment Insight Rate is positive", Treatment.InsightRate, 0.0f);
    UTEST_EQUAL("GATE: control Insight Rate is zero", ControlInsight, 0.0f);
    UTEST_GREATER("GATE: treatment strictly beats control (loop compounds)", Treatment.InsightRate, ControlInsight);

    return true;
}

// Objective — WIN: a session with a reachable target resolves to Won once the
// player has surfaced enough discoveries. This is the goal that turns the endless
// simulation into a game with a finish line.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectiveWinTest, "MANIFOLD.Play.ObjectiveWin", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectiveWinTest::RunTest(const FString& Parameters)
{
    UManifoldSlice* Slice = NewObject<UManifoldSlice>();
    FManifoldObjective Obj;
    Obj.TargetDiscoveries = 1; // one discovery is enough to win
    Obj.StepBudget = 0;        // unlimited — cannot lose
    Slice->SetObjective(Obj);
    Slice->Setup(1111ULL, 2222ULL);

    Slice->RunPlaythrough(30);

    UTEST_GREATER("At least one discovery surfaced", Slice->GetTotalDiscoveries(), 0);
    UTEST_EQUAL("Session resolved to Won", (int32)Slice->GetSessionState(), (int32)EManifoldSessionState::Won);

    const FManifoldSessionSummary Summary = Slice->GetSessionSummary();
    UTEST_EQUAL("Summary state is Won", (int32)Summary.State, (int32)EManifoldSessionState::Won);
    UTEST_GREATER("Summary reports discoveries", Summary.Discoveries, 0);

    return true;
}

// Objective — LOSE: an unreachable target within a tight step budget resolves to
// Lost when the budget runs out. Proves the moat holds — you cannot manufacture a
// win the simulation did not earn.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectiveLoseTest, "MANIFOLD.Play.ObjectiveLose", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FObjectiveLoseTest::RunTest(const FString& Parameters)
{
    UManifoldSlice* Slice = NewObject<UManifoldSlice>();
    FManifoldObjective Obj;
    Obj.TargetDiscoveries = 999; // never reachable
    Obj.StepBudget = 5;          // and the clock runs out fast
    Slice->SetObjective(Obj);
    Slice->Setup(1111ULL, 2222ULL);

    Slice->RunPlaythrough(10); // more than the budget

    UTEST_EQUAL("Session resolved to Lost", (int32)Slice->GetSessionState(), (int32)EManifoldSessionState::Lost);

    return true;
}

// Determinism: the SAME seeds produce a bit-for-bit identical session — same
// discovery count, same transport schedule, same Insight Rate. This is the
// "un-pre-computable yet perfectly reproducible" pillar, tested at the whole-slice
// level (not just per-kernel).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSliceDeterminismTest, "MANIFOLD.Play.SliceDeterminism", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSliceDeterminismTest::RunTest(const FString& Parameters)
{
    UManifoldSlice* A = NewObject<UManifoldSlice>();
    UManifoldSlice* B = NewObject<UManifoldSlice>();
    const FManifoldReplay RA = A->RecordReplay(4242LL, 7777LL, 40);
    const FManifoldReplay RB = B->RecordReplay(4242LL, 7777LL, 40);

    UTEST_EQUAL("Same discoveries", RA.FinalDiscoveries, RB.FinalDiscoveries);
    UTEST_EQUAL("Same transports", RA.FinalTransports, RB.FinalTransports);
    UTEST_EQUAL("Same Insight Rate (bit-identical)", RA.FinalInsightRate, RB.FinalInsightRate);
    UTEST_TRUE("Same transport schedule", RA.TransportSteps == RB.TransportSteps);

    return true;
}

// Replay round-trip: record a session, reproduce it on a fresh slice (player firing
// the verb on the recorded schedule) and get the same result; then persist the
// recording to disk, load it back, and reproduce again identically.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReplayRoundTripTest, "MANIFOLD.Play.ReplayRoundTrip", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FReplayRoundTripTest::RunTest(const FString& Parameters)
{
    UManifoldSlice* Recorder = NewObject<UManifoldSlice>();
    const FManifoldReplay Rec = Recorder->RecordReplay(1111LL, 2222LL, 30);
    UTEST_GREATER("Recording captured a transport", Rec.FinalTransports, 0);

    // Reproduce from the in-memory recording.
    UManifoldSlice* Player = NewObject<UManifoldSlice>();
    const FManifoldSliceResult Reproduced = Player->RunReplay(Rec);
    UTEST_EQUAL("Reproduced transports match", Reproduced.TransportsCompleted, Rec.FinalTransports);
    UTEST_EQUAL("Reproduced Insight Rate matches", Reproduced.InsightRate, Rec.FinalInsightRate);
    UTEST_TRUE("Reproduced transport schedule matches recording", Player->GetTransportSchedule() == Rec.TransportSteps);

    // Persist to disk and reload.
    const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("RoundTrip.manifoldreplay"));
    UTEST_TRUE("Replay saved to disk", UManifoldSlice::SaveReplay(Rec, Path));

    FManifoldReplay Loaded;
    UTEST_TRUE("Replay loaded from disk", UManifoldSlice::LoadReplay(Loaded, Path));
    UTEST_EQUAL("Loaded seeds preserved (orbits)", (int64)Loaded.OrbitsSeed, (int64)Rec.OrbitsSeed);
    UTEST_EQUAL("Loaded schedule preserved", Loaded.TransportSteps.Num(), Rec.TransportSteps.Num());
    UTEST_TRUE("Loaded schedule identical", Loaded.TransportSteps == Rec.TransportSteps);

    // Reproduce from the loaded recording — still identical.
    UManifoldSlice* Player2 = NewObject<UManifoldSlice>();
    const FManifoldSliceResult FromDisk = Player2->RunReplay(Loaded);
    UTEST_EQUAL("Disk-replay transports match", FromDisk.TransportsCompleted, Rec.FinalTransports);
    UTEST_EQUAL("Disk-replay Insight Rate matches", FromDisk.InsightRate, Rec.FinalInsightRate);

    // Clean up the scratch file.
    FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*Path);

    return true;
}

// Procedural variation: each seed hides a DIFFERENT ratio across all realms (so the
// game can't be pre-solved), and the same seed always hides the same ratio (so runs
// are reproducible). After play, every ratio realm actually realizes that hidden ratio.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProceduralVariationTest, "MANIFOLD.Play.ProceduralVariation", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FProceduralVariationTest::RunTest(const FString& Parameters)
{
    UManifoldSlice* A = NewObject<UManifoldSlice>();
    A->Setup(1ULL, 100ULL);
    UManifoldSlice* B = NewObject<UManifoldSlice>();
    B->Setup(2ULL, 100ULL);
    UManifoldSlice* A2 = NewObject<UManifoldSlice>();
    A2->Setup(1ULL, 999ULL); // different fluids seed, same orbits seed

    UTEST_NOT_EQUAL("Different seeds hide different ratios", A->GetSharedRatio(), B->GetSharedRatio());
    UTEST_EQUAL("Same orbits seed hides the same ratio", A->GetSharedRatio(), A2->GetSharedRatio());

    // After play, the realms actually realize the hidden ratio.
    A->RunPlaythrough(30);
    UTEST_EQUAL("Orbits realizes the hidden ratio", A->GetOrbitsRatio(), A->GetSharedRatio());
    UTEST_EQUAL("Harmonics realizes the hidden ratio", A->GetHarmonicsRatio(), A->GetSharedRatio());
    UTEST_EQUAL("Rhythm realizes the hidden ratio", A->GetRhythmRatio(), A->GetSharedRatio());
    UTEST_GREATER("The hidden ratio is discovered across domains", A->GetSharedDiscoveries(), 0);
    UTEST_GREATER("The seam ignites on the hidden ratio", A->GetCorrespondencesIgnited(), 0);

    return true;
}

// Expedition (campaign): escalating-difficulty levels played back to back, ending at
// the first level the player can't clear. A session can surface up to 7 discoveries
// (1 seam + 6 cross-domain), so a target of 8 is the natural difficulty wall.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExpeditionTest, "MANIFOLD.Play.Expedition", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FExpeditionTest::RunTest(const FString& Parameters)
{
    // Levels 0..2 demand 2, 4, 6 discoveries — all clearable.
    const FManifoldExpeditionResult Three = UManifoldSlice::RunExpedition(500LL, 3, 30);
    UTEST_EQUAL("Cleared all three levels", Three.LevelsCleared, 3);
    UTEST_TRUE("Three-level expedition completed", Three.bCompleted);
    UTEST_GREATER("Accumulated a positive total score", Three.TotalScore, 0);

    // Levels 0..4 demand 2, 4, 6, 8, 10 — level 3 (target 8) is unclearable, so the
    // expedition ends after 3 cleared levels.
    const FManifoldExpeditionResult Five = UManifoldSlice::RunExpedition(500LL, 5, 30);
    UTEST_EQUAL("Difficulty wall stops the run at 3 cleared", Five.LevelsCleared, 3);
    UTEST_FALSE("Five-level expedition is not completed", Five.bCompleted);

    // Deterministic in the base seed.
    const FManifoldExpeditionResult Again = UManifoldSlice::RunExpedition(500LL, 3, 30);
    UTEST_EQUAL("Same base seed -> same total score", Again.TotalScore, Three.TotalScore);

    return true;
}

// Scoring: a played session yields a positive score that reflects what the player
// surfaced, and the summary carries it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScoringTest, "MANIFOLD.Play.Scoring", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FScoringTest::RunTest(const FString& Parameters)
{
    UManifoldSlice* S = NewObject<UManifoldSlice>();
    S->Setup(1111ULL, 2222ULL);
    S->RunPlaythrough(30);

    const int32 Score = S->GetScore();
    const FManifoldSessionSummary Sum = S->GetSessionSummary();
    UTEST_GREATER("Score is positive after a session", Score, 0);
    UTEST_EQUAL("Summary carries the score", Sum.Score, Score);
    UTEST_GREATER_EQUAL("Score reflects discoveries", Score, Sum.Discoveries * 100);

    return true;
}

// Persistent profile: sessions fold into a profile (counts + best score), and the
// profile round-trips through a versioned save file.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProfileRoundTripTest, "MANIFOLD.Play.ProfileRoundTrip", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FProfileRoundTripTest::RunTest(const FString& Parameters)
{
    FManifoldProfile P;
    UTEST_EQUAL("Fresh profile best is zero", P.BestScore, 0);

    FManifoldSessionSummary Won;
    Won.State = EManifoldSessionState::Won;
    Won.Score = 500;
    UManifoldSlice::RecordSessionInProfile(P, Won);
    UTEST_EQUAL("Played incremented", P.SessionsPlayed, 1);
    UTEST_EQUAL("Won incremented", P.SessionsWon, 1);
    UTEST_EQUAL("Best updated", P.BestScore, 500);

    FManifoldSessionSummary Lost;
    Lost.State = EManifoldSessionState::Lost;
    Lost.Score = 300; // lower — must not lower the best
    UManifoldSlice::RecordSessionInProfile(P, Lost);
    UTEST_EQUAL("Played incremented again", P.SessionsPlayed, 2);
    UTEST_EQUAL("Won not incremented on a loss", P.SessionsWon, 1);
    UTEST_EQUAL("Best stays at the higher score", P.BestScore, 500);

    const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Test.manifoldprofile"));
    UTEST_TRUE("Profile saved", UManifoldSlice::SaveProfile(P, Path));

    FManifoldProfile Loaded;
    UTEST_TRUE("Profile loaded", UManifoldSlice::LoadProfile(Loaded, Path));
    UTEST_EQUAL("Loaded best", Loaded.BestScore, 500);
    UTEST_EQUAL("Loaded played", Loaded.SessionsPlayed, 2);
    UTEST_EQUAL("Loaded won", Loaded.SessionsWon, 1);

    FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*Path);
    return true;
}

// Decoy realm (the moat): a red-herring realm exhibits a DIFFERENT ratio, and the
// correspondence engine must refuse to pair it with the true realms. This proves the
// game can't be trivially solved by "everything matches" — the player must actually
// discriminate the real correspondence from the false one.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecoyRealmTest, "MANIFOLD.Play.DecoyRealm", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDecoyRealmTest::RunTest(const FString& Parameters)
{
    UManifoldSlice* S = NewObject<UManifoldSlice>();
    S->Setup(1111ULL, 2222ULL);
    S->RunPlaythrough(30);

    // The decoy realizes a real ratio, but a DIFFERENT one from the hidden ratio.
    UTEST_TRUE("Decoy exhibits a ratio", S->GetDecoyRatio() != FString(TEXT("-")));
    UTEST_NOT_EQUAL("Decoy ratio differs from the hidden ratio (red herring)",
        S->GetDecoyRatio(), S->GetSharedRatio());

    // The four TRUE ratio realms correspond pairwise (C(4,2) = 6). The decoy shares
    // its ratio with none of them, so the engine adds no false correspondence.
    UTEST_EQUAL("Engine refuses the false correspondence (no decoy inflation)",
        S->GetSharedDiscoveries(), 6);

    return true;
}

// Stable structure identity: a realm's Query must return the SAME id for the same
// detected structure across calls (not a fresh GUID each time), so the correspondence
// layer can dedup and transport by it. Regression guard for the Harmonics/Waves/Rhythm
// kernels, which previously returned FGuid::NewGuid() on every query.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStableStructureIdTest, "MANIFOLD.Play.StableStructureIds", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FStableStructureIdTest::RunTest(const FString& Parameters)
{
    UHarmonicsKernel* H = NewObject<UHarmonicsKernel>();
    H->Initialize(1ULL); H->AddMode(2.0, 1.0); H->AddMode(3.0, 1.0); H->Step(0.01f);

    UWavesKernel* W = NewObject<UWavesKernel>();
    W->Initialize(2ULL); W->ExciteHarmonic(2, 1.0); W->ExciteHarmonic(3, 1.0); W->Step(0.001f);

    URhythmKernel* R = NewObject<URhythmKernel>();
    R->Initialize(3ULL); R->AddVoice(3.0); R->AddVoice(2.0); R->Step(0.016f);

    struct FCase { IRealmKernel* Kernel; FName Query; const TCHAR* Name; };
    const FCase Cases[] = {
        { Cast<IRealmKernel>(H), TEXT("HarmonicRatio"), TEXT("Harmonics") },
        { Cast<IRealmKernel>(W), TEXT("WaveHarmonic"),  TEXT("Waves") },
        { Cast<IRealmKernel>(R), TEXT("RhythmRatio"),   TEXT("Rhythm") },
    };

    for (const FCase& C : Cases)
    {
        FRealmQuery Q(C.Query);
        FRealmQueryResult R1, R2;
        const bool bOk1 = C.Kernel->Query(Q, R1);
        const bool bOk2 = C.Kernel->Query(Q, R2);
        UTEST_TRUE(FString::Printf(TEXT("%s query succeeds"), C.Name), bOk1 && bOk2);
        UTEST_TRUE(FString::Printf(TEXT("%s id is valid (not zero)"), C.Name), R1.StructureId.IsValid());
        UTEST_EQUAL(FString::Printf(TEXT("%s id is stable across calls"), C.Name), R1.StructureId, R2.StructureId);
    }

    // Different realms with the same ratio must still have DISTINCT ids.
    FRealmQueryResult HR, WR;
    Cast<IRealmKernel>(H)->Query(FRealmQuery(TEXT("HarmonicRatio")), HR);
    Cast<IRealmKernel>(W)->Query(FRealmQuery(TEXT("WaveHarmonic")), WR);
    UTEST_NOT_EQUAL("Distinct realms have distinct ids", HR.StructureId, WR.StructureId);

    return true;
}

// Audio integration: playing the slice emits exactly one audio cue per discovery
// (a chime voicing its ratio) and one per transport (a chord-resolve). This is the
// event routing the audio pipeline binds sounds to.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSliceAudioIntegrationTest, "MANIFOLD.Play.AudioIntegration", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSliceAudioIntegrationTest::RunTest(const FString& Parameters)
{
    UManifoldSlice* Slice = NewObject<UManifoldSlice>();
    Slice->Setup(1111ULL, 2222ULL);
    Slice->RunPlaythrough(30);

    UTEST_GREATER("Audio cues were emitted during play", Slice->GetAudioCueCount(), 0);
    // One cue per seam ignition, per cross-domain discovery, and per transport.
    UTEST_EQUAL("One cue per ignition + discovery + transport",
        Slice->GetAudioCueCount(),
        Slice->GetCorrespondencesIgnited() + Slice->GetSharedDiscoveries() + Slice->GetTransportsCompleted());
    UTEST_TRUE("Last cue carries a realm identity", Slice->GetLastAudioCue().Realm != NAME_None);

    return true;
}
