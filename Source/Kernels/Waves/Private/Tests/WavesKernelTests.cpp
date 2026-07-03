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

// Regression (kernel audit): ComputeStateHash folded only the per-wave HarmonicNumber/Phase/Amplitude
// and omitted WState->Fundamental — yet Step advances every wave by `HarmonicNumber * Fundamental`, so
// two states identical except for Fundamental hashed IDENTICALLY while diverging on the very next step
// (e.g. 220 Hz vs 440 Hz). Fundamental is serialized/restored and SetParameter-mutable, so those
// states are reachable, and this hash is the determinism/replay divergence detector. Fundamental is
// now folded. Same class as MANIFOLD.Kernels.Orbits.HashCoversMass.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWavesHashCoversFundamentalTest, "MANIFOLD.Kernels.Waves.HashCoversFundamental", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWavesHashCoversFundamentalTest::RunTest(const FString& Parameters)
{
    // Build identical wave sets (same harmonics/amplitudes, excited BEFORE overriding Fundamental so
    // the Waves arrays are byte-identical), differing ONLY in the Fundamental frequency.
    auto BuildHash = [](double Fund) -> uint64
    {
        UWavesKernel* K = NewObject<UWavesKernel>();
        K->Initialize(4242ULL);
        K->ExciteHarmonic(2, 1.0);
        K->ExciteHarmonic(3, 0.7);
        K->SetParameter(TEXT("Fundamental"), FString::Printf(TEXT("%f"), Fund));
        return K->ComputeStateHash();
    };

    const uint64 HRef = BuildHash(220.0);
    UTEST_TRUE("a Fundamental-only divergence must change the Waves state hash", HRef != BuildHash(440.0));
    UTEST_TRUE("identical Fundamental hashes identically", HRef == BuildHash(220.0));

    return true;
}
