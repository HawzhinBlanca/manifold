// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "GearsKernel.h"

// Realm template step 1 (Build Plan §9): the kernel is deterministic.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGearsDeterminismTest, "MANIFOLD.Kernels.Gears.DeterministicAngles", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FGearsDeterminismTest::RunTest(const FString& Parameters)
{
    UGearsKernel* Kernel = NewObject<UGearsKernel>();
    Kernel->Initialize(333ULL);
    Kernel->AddGear(3);
    Kernel->AddGear(2);

    UTEST_TRUE("Gears determinism", Kernel->VerifyDeterminism(100));
    return true;
}

// Realm template step 1: the kernel exposes its structure — the EXACT integer gear
// ratio. Gears of 3 and 2 teeth form a 3:2, the same structure as an orbital 3:2
// resonance, now in the domain of MECHANISM (the sixth domain to share the ratio).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGearsRatioTest, "MANIFOLD.Kernels.Gears.GearRatio", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FGearsRatioTest::RunTest(const FString& Parameters)
{
    UGearsKernel* Kernel = NewObject<UGearsKernel>();
    Kernel->Initialize(333ULL);
    Kernel->AddGear(3);
    Kernel->AddGear(2);
    Kernel->Step(0.01f);

    const TArray<FGearRatioMatch>& Ratios = Kernel->GetActiveRatios();
    UTEST_GREATER("Detected gear ratios", Ratios.Num(), 0);

    bool bFound32 = false;
    for (const FGearRatioMatch& Match : Ratios)
    {
        if (Match.Ratio.X == 3 && Match.Ratio.Y == 2)
        {
            bFound32 = true;
            break;
        }
    }
    UTEST_TRUE("Gears of 3 and 2 teeth detected as 3:2", bFound32);

    // Non-coprime tooth counts reduce exactly: 8 and 6 teeth -> 4:3.
    UGearsKernel* K2 = NewObject<UGearsKernel>();
    K2->Initialize(1ULL);
    K2->AddGear(8);
    K2->AddGear(6);
    K2->Step(0.01f);
    bool bFound43 = false;
    for (const FGearRatioMatch& Match : K2->GetActiveRatios())
    {
        if (Match.Ratio.X == 4 && Match.Ratio.Y == 3) { bFound43 = true; break; }
    }
    UTEST_TRUE("8:6 teeth reduce exactly to 4:3", bFound43);

    return true;
}
