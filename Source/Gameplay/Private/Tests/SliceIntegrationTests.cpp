// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "ManifoldSlice.h"
#include "OrbitsKernel.h"
#include "FluidsKernel.h"
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
