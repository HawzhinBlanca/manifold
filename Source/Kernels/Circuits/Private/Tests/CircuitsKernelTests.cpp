// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "CircuitsKernel.h"

// Two tanks tuned to 2 and 3 (units) resonate a 3:2 — the same structure as an orbital
// 3:2, in the domain of electromagnetism. Exact integer ratio via GCD.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCircuitsRatioTest, "MANIFOLD.Kernels.Circuits.ResonanceRatio", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCircuitsRatioTest::RunTest(const FString& Parameters)
{
    UCircuitsKernel* Kernel = NewObject<UCircuitsKernel>();
    Kernel->Initialize(1ULL);
    Kernel->AddTank(2.0, 1.0);
    Kernel->AddTank(3.0, 1.0);
    Kernel->Step(0.01f);

    const TArray<FCircuitRatioMatch>& Ratios = Kernel->GetActiveRatios();
    UTEST_GREATER("Detected circuit ratios", Ratios.Num(), 0);

    bool bFound32 = false;
    for (const FCircuitRatioMatch& Match : Ratios)
    {
        if (Match.Ratio.X == 3 && Match.Ratio.Y == 2) { bFound32 = true; break; }
    }
    UTEST_TRUE("Found 3:2 resonance ratio", bFound32);

    FRealmQuery Query(TEXT("CircuitResonance"));
    FRealmQueryResult Result;
    UTEST_TRUE("CircuitResonance query succeeds", Kernel->Query(Query, Result));
    UTEST_EQUAL("Query reports 3:2", Result.Parameters.FindRef(TEXT("Ratio")), FString(TEXT("3:2")));

    return true;
}

// Integer frequency ratios are detected EXACTLY via GCD, including above the typical
// small-ratio range. Tanks at 9 and 4 form 9:4.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCircuitsExactRatioTest, "MANIFOLD.Kernels.Circuits.ExactRatio", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCircuitsExactRatioTest::RunTest(const FString& Parameters)
{
    UCircuitsKernel* Kernel = NewObject<UCircuitsKernel>();
    Kernel->Initialize(1ULL);
    Kernel->AddTank(4.0, 1.0);
    Kernel->AddTank(9.0, 1.0);
    Kernel->Step(0.01f);

    bool bFound = false;
    for (const FCircuitRatioMatch& M : Kernel->GetActiveRatios())
    {
        if (M.Ratio.X == 9 && M.Ratio.Y == 4) { bFound = true; break; }
    }
    UTEST_TRUE("9:4 is detected exactly via GCD", bFound);
    return true;
}

// The kernel is deterministic: same seed + same tanks => identical state hash.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCircuitsDeterminismTest, "MANIFOLD.Kernels.Circuits.Deterministic", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCircuitsDeterminismTest::RunTest(const FString& Parameters)
{
    UCircuitsKernel* Kernel = NewObject<UCircuitsKernel>();
    Kernel->Initialize(777ULL);
    Kernel->AddTank(2.0, 1.0);
    Kernel->AddTank(3.0, 0.5);

    UTEST_TRUE("Circuits determinism", Kernel->VerifyDeterminism(100));
    return true;
}
