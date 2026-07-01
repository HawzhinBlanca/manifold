// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "WavesKernel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWavesDeterminismTest, "MANIFOLD.Kernels.Waves.DeterministicPhases", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWavesDeterminismTest::RunTest(const FString& Parameters)
{
    UWavesKernel* Kernel = NewObject<UWavesKernel>();
    Kernel->Initialize(4242ULL);
    Kernel->ExciteHarmonic(2, 1.0);
    Kernel->ExciteHarmonic(3, 0.7);

    UTEST_TRUE("Waves determinism", Kernel->VerifyDeterminism(100));
    return true;
}

// The 2nd and 3rd harmonics of a string form a 3:2 — the SAME structure as an
// orbital 3:2 resonance and a harmonic 3:2, now in a third physical domain.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWavesRatioTest, "MANIFOLD.Kernels.Waves.WaveHarmonic", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWavesRatioTest::RunTest(const FString& Parameters)
{
    UWavesKernel* Kernel = NewObject<UWavesKernel>();
    Kernel->Initialize(4242ULL);
    Kernel->ExciteHarmonic(2, 1.0);
    Kernel->ExciteHarmonic(3, 1.0);
    Kernel->Step(0.001f);

    const TArray<FWaveRatioMatch>& Ratios = Kernel->GetActiveRatios();
    UTEST_GREATER("Detected wave ratios", Ratios.Num(), 0);
    UTEST_EQUAL("Ratio is 3:2", Ratios[0].Ratio, FIntPoint(3, 2));

    FRealmQuery Query(TEXT("WaveHarmonic"));
    FRealmQueryResult Result;
    UTEST_TRUE("WaveHarmonic query succeeds", Kernel->Query(Query, Result));
    UTEST_EQUAL("Query reports 3:2", Result.Parameters.FindRef(TEXT("Ratio")), FString(TEXT("3:2")));

    return true;
}
