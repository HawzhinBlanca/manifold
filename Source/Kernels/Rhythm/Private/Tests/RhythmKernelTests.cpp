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
