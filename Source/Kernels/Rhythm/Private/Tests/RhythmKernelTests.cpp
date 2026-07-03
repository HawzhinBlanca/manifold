// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "RhythmKernel.h"

// Realm template step 1 (Build Plan §9): the kernel is deterministic.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRhythmDeterminismTest, "MANIFOLD.Kernels.Rhythm.DeterministicPhases", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRhythmDeterminismTest::RunTest(const FString& Parameters)
{
    URhythmKernel* Kernel = NewObject<URhythmKernel>();
    Kernel->Initialize(555ULL);
    Kernel->AddVoice(3.0);
    Kernel->AddVoice(2.0);

    UTEST_TRUE("Rhythm determinism", Kernel->VerifyDeterminism(100));
    return true;
}

// Realm template step 1: the kernel exposes its structure — the small-integer tempo
// ratio. Three-against-two is a 3:2 polyrhythm, the SAME structure as an orbital 3:2
// resonance, now in the domain of time (the fifth domain to share the 3:2).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRhythmRatioTest, "MANIFOLD.Kernels.Rhythm.RhythmRatio", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRhythmRatioTest::RunTest(const FString& Parameters)
{
    URhythmKernel* Kernel = NewObject<URhythmKernel>();
    Kernel->Initialize(555ULL);
    Kernel->AddVoice(3.0);
    Kernel->AddVoice(2.0);
    Kernel->Step(0.01f);

    const TArray<FRhythmRatioMatch>& Ratios = Kernel->GetActiveRatios();
    UTEST_GREATER("Detected polyrhythm ratios", Ratios.Num(), 0);

    bool bFound32 = false;
    for (const FRhythmRatioMatch& Match : Ratios)
    {
        if (Match.Ratio.X == 3 && Match.Ratio.Y == 2)
        {
            bFound32 = true;
            break;
        }
    }
    UTEST_TRUE("Three-against-two polyrhythm detected as 3:2", bFound32);

    return true;
}

// The exact-integer fast path handles whole-number tempos (3 vs 2, covered above); this covers the
// OTHER branch — the approximate small-integer p/q search for non-integer tempo ratios and its
// MaxDeviation reject gate. Every other Rhythm test uses integer tempos, so that entire branch (the
// exp(-200*dev) strength falloff and the tolerance gate) shipped untested until now.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRhythmApproxRatioTest, "MANIFOLD.Kernels.Rhythm.ApproxRatioSearch", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRhythmApproxRatioTest::RunTest(const FString& Parameters)
{
    // 2.5 vs 1.0: not an integer pair (2.5 rounds to 3, off by 0.5), so the exact fast path is
    // skipped and the approximate search runs — 5/2 = 2.5 exactly, a zero-deviation hit.
    URhythmKernel* K = NewObject<URhythmKernel>();
    K->Initialize(777ULL);
    K->AddVoice(2.5);
    K->AddVoice(1.0);
    K->Step(0.01f);

    const TArray<FRhythmRatioMatch>& R = K->GetActiveRatios();
    UTEST_EQUAL("one detected ratio for the single voice pair", R.Num(), 1);
    UTEST_TRUE("2.5:1 resolves to 5:2 via the approximate search",
        R.Num() == 1 && R[0].Ratio.X == 5 && R[0].Ratio.Y == 2);
    UTEST_TRUE("the 5:2 match is essentially exact (deviation ~0)", R.Num() == 1 && R[0].Deviation < 1e-6);
    UTEST_TRUE("strength is ~1 at zero deviation", R.Num() == 1 && R[0].Strength > 0.99);

    // ~sqrt(2) : 1 — an irrational ratio whose nearest small-integer fraction (7:5) is ~0.014 off,
    // beyond the 0.01 default tolerance, so the MaxDeviation gate rejects it (no false polyrhythm).
    URhythmKernel* K2 = NewObject<URhythmKernel>();
    K2->Initialize(778ULL);
    K2->AddVoice(1.4142);
    K2->AddVoice(1.0);
    K2->Step(0.01f);
    UTEST_EQUAL("a near-irrational ratio is rejected by the MaxDeviation gate",
        K2->GetActiveRatios().Num(), 0);

    return true;
}
