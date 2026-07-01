// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "HarmonicsKernel.h"

// Integer frequency ratios are detected EXACTLY via GCD, including ratios above the
// old approximate-search cap (MaxRatio=10). Modes at 11 and 12 Hz form 12:11.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHarmonicsExactHighRatioTest, "MANIFOLD.Kernels.Harmonics.ExactHighRatio", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FHarmonicsExactHighRatioTest::RunTest(const FString& Parameters)
{
    UHarmonicsKernel* Kernel = NewObject<UHarmonicsKernel>();
    Kernel->Initialize(1ULL);
    Kernel->AddMode(11.0, 1.0);
    Kernel->AddMode(12.0, 1.0);
    Kernel->Step(0.01f);

    bool bFound = false;
    for (const FHarmonicRatioMatch& M : Kernel->GetActiveRatios())
    {
        if (M.Ratio.X == 12 && M.Ratio.Y == 11) { bFound = true; break; }
    }
    UTEST_TRUE("12:11 (above the old MaxRatio=10 cap) is detected exactly", bFound);
    return true;
}

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
