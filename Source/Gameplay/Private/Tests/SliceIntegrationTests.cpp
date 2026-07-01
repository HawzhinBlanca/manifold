// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "ManifoldSlice.h"
#include "OrbitsKernel.h"
#include "FluidsKernel.h"
#include "HarmonicsKernel.h"
#include "CorrespondenceSystem.h"
#include "TelemetrySystem.h"

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
