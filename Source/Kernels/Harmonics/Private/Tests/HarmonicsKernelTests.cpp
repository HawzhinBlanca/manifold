// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "HarmonicsKernel.h"

// Realm template step 1 (Build Plan §9): the kernel is deterministic.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHarmonicsDeterminismTest, "MANIFOLD.Kernels.Harmonics.DeterministicPhases", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FHarmonicsDeterminismTest::RunTest(const FString& Parameters)
{
    UHarmonicsKernel* Kernel = NewObject<UHarmonicsKernel>();
    Kernel->Initialize(777ULL);
    Kernel->AddMode(2.0, 1.0);
    Kernel->AddMode(3.0, 0.5);

    UTEST_TRUE("Harmonics determinism", Kernel->VerifyDeterminism(100));
    return true;
}

// Realm template step 1: the kernel exposes its structure for mappings — here the
// small-integer frequency ratio. Two modes at 2 Hz and 3 Hz form a 3:2, the same
// structure as an orbital 3:2 resonance (the future cross-domain correspondence).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHarmonicsRatioTest, "MANIFOLD.Kernels.Harmonics.HarmonicRatio", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FHarmonicsRatioTest::RunTest(const FString& Parameters)
{
    UHarmonicsKernel* Kernel = NewObject<UHarmonicsKernel>();
    Kernel->Initialize(777ULL);
    Kernel->AddMode(2.0, 1.0);
    Kernel->AddMode(3.0, 1.0);
    Kernel->Step(0.01f);

    const TArray<FHarmonicRatioMatch>& Ratios = Kernel->GetActiveRatios();
    UTEST_GREATER("Detected harmonic ratios", Ratios.Num(), 0);

    bool bFound32 = false;
    for (const FHarmonicRatioMatch& Match : Ratios)
    {
        if (Match.Ratio.X == 3 && Match.Ratio.Y == 2)
        {
            bFound32 = true;
            break;
        }
    }
    UTEST_TRUE("Found 3:2 harmonic ratio", bFound32);

    FRealmQuery Query(TEXT("HarmonicRatio"));
    FRealmQueryResult Result;
    UTEST_TRUE("HarmonicRatio query succeeds", Kernel->Query(Query, Result));
    UTEST_EQUAL("Query reports 3:2", Result.Parameters.FindRef(TEXT("Ratio")), FString(TEXT("3:2")));

    return true;
}
