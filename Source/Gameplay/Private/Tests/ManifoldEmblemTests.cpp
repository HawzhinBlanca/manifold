// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "ManifoldEmblem.h"

// The MANIFOLD emblem is real art authored in code: verify the procedural renderer
// produces a well-formed image with gold linework, a dark field, and the left-right
// symmetry of its resonance rings.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManifoldEmblemTest, "MANIFOLD.UI.Emblem", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FManifoldEmblemTest::RunTest(const FString& Parameters)
{
    const int32 Size = 128;
    TArray<FColor> Px;
    ManifoldEmblem::Render(Px, Size);

    UTEST_EQUAL("Emblem has the right pixel count", Px.Num(), Size * Size);

    auto IsGold = [](const FColor& C) { return C.R >= 200 && C.G >= 150 && C.B < 150; };

    int32 GoldCount = 0;
    int32 DarkCount = 0;
    for (const FColor& C : Px)
    {
        if (IsGold(C)) { ++GoldCount; }
        else if (C.R < 60 && C.G < 60) { ++DarkCount; }
    }
    UTEST_GREATER("Emblem has gold linework", GoldCount, 100);
    UTEST_GREATER("Emblem has a dark field", DarkCount, 100);

    // A point on the outer resonance ring (angle 0) is gold, and its mirror is too.
    auto At = [&](int32 x, int32 y) -> const FColor& { return Px[y * Size + x]; };
    UTEST_TRUE("Outer ring is drawn", IsGold(At(116, 63)));
    UTEST_TRUE("Rings are left-right symmetric", IsGold(At(11, 63)));

    return true;
}
