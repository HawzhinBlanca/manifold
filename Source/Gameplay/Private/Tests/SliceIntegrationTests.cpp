// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "ManifoldSlice.h"
#include "ManifoldTypes.h"
#include "OrbitsKernel.h"
#include "FluidsKernel.h"
#include "HarmonicsKernel.h"
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

    bool bIgnited = false;
    Correspond->OnCorrespondenceIgnited.AddLambda([&bIgnited](FGuid, FGuid, float) { bIgnited = true; });

    const int32 Found = Correspond->DetectSharedStructureCorrespondences();
    UTEST_GREATER("Shared 3:2 structure found across Orbits and Harmonics", Found, 0);
    UTEST_TRUE("Cross-domain correspondence ignited", bIgnited);

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
    UTEST_EQUAL("Orbits shows 3:2", Slice->GetOrbitsRatio(), FString(TEXT("3:2")));
    UTEST_EQUAL("Harmonics shows 3:2", Slice->GetHarmonicsRatio(), FString(TEXT("3:2")));
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
    UTEST_EQUAL("One cue per ignition + transport",
        Slice->GetAudioCueCount(),
        Slice->GetCorrespondencesIgnited() + Slice->GetTransportsCompleted());
    UTEST_TRUE("Last cue carries a realm identity", Slice->GetLastAudioCue().Realm != NAME_None);

    return true;
}
