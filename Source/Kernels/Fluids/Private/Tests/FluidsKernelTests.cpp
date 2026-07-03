// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "FluidsKernel.h"

// The detected vortex's StructureId must be DETERMINISTIC across identical runs (it was
// FGuid::NewGuid(), violating the stable-id contract the other realms uphold). Two
// same-seed fluids with the same perturbation must return the same vortex id.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFluidsStableVortexIdTest, "MANIFOLD.Kernels.Fluids.StableVortexId", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFluidsStableVortexIdTest::RunTest(const FString& Parameters)
{
    auto BuildVortex = [](UFluidsKernel* K)
    {
        K->Initialize(2222ULL);
        K->AddVelocity(0.5f, 0.45f, 20.0f, 0.0f);
        K->AddVelocity(0.55f, 0.5f, 0.0f, 20.0f);
        K->AddVelocity(0.5f, 0.55f, -20.0f, 0.0f);
        K->AddVelocity(0.45f, 0.5f, 0.0f, -20.0f);
        for (int32 i = 0; i < 10; ++i) { K->Step(0.016f); }
    };

    UFluidsKernel* A = NewObject<UFluidsKernel>(); BuildVortex(A);
    UFluidsKernel* B = NewObject<UFluidsKernel>(); BuildVortex(B);

    FRealmQuery Q(TEXT("VortexCenter"));
    FRealmQueryResult RA, RB;
    const bool bOkA = A->Query(Q, RA);
    const bool bOkB = B->Query(Q, RB);
    UTEST_TRUE("Both same-seed fluids detect a vortex", bOkA && bOkB);
    UTEST_TRUE("Vortex id is valid", RA.StructureId.IsValid());
    UTEST_EQUAL("Same-seed vortex has the same stable id", RA.StructureId, RB.StructureId);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFluidsDeterministicFlowTest, "MANIFOLD.Kernels.Fluids.DeterministicFlow", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFluidsDeterministicFlowTest::RunTest(const FString& Parameters)
{
    UFluidsKernel* Kernel = NewObject<UFluidsKernel>();
    Kernel->Initialize(424242ULL);
    
    // Verify that running two identical simulations produces bitwise-identical state hashes
    UTEST_TRUE("Fluids determinism", Kernel->VerifyDeterminism(50));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFluidsStructureQueryTest, "MANIFOLD.Kernels.Fluids.StructureQuery", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFluidsStructureQueryTest::RunTest(const FString& Parameters)
{
    UFluidsKernel* Kernel = NewObject<UFluidsKernel>();
    Kernel->Initialize(424242ULL);
    
    // Inject velocity perturbations around normalized center (0.5, 0.5) to create a vortex
    Kernel->AddVelocity(0.5f, 0.45f, 20.0f, 0.0f);
    Kernel->AddVelocity(0.55f, 0.5f, 0.0f, 20.0f);
    Kernel->AddVelocity(0.5f, 0.55f, -20.0f, 0.0f);
    Kernel->AddVelocity(0.45f, 0.5f, 0.0f, -20.0f);
    
    // Step simulation to let the forces diffuse and project
    Kernel->Step(0.016f);
    Kernel->Step(0.016f);
    
    // Check if a vortex has been registered
    const TArray<FFluidVortex>& Vortices = Kernel->GetActiveVortices();
    UTEST_GREATER("Detected vortices in active simulation", Vortices.Num(), 0);
    
    // Execute a structural query for the vortex center
    FRealmQuery Query(TEXT("VortexCenter"));
    Query.MinStrength = 0.01f;
    FRealmQueryResult Result;
    
    UTEST_TRUE("Structural Vortex Center query", Kernel->Query(Query, Result));
    
    // Verify coordinates align near center
    float DistFromCenter = FVector::Dist(Result.Position, FVector(500.0f, 500.0f, 0.0f));
    UTEST_TRUE("Vortex position is localized around perturbation source", DistFromCenter < 150.0f);

    return true;
}

// Regression for two Fluids-kernel defects found by adversarial audit:
//  (1) A negative Viscosity/Diffusion (settable via SetParameter, previously unchecked) drove the
//      Diffuse implicit-solve denominator (1 + 4a) to zero -> NaN, which bypassed Advect's range
//      clamps (NaN compares false to both bounds) and cast to an out-of-bounds array index. Params
//      are now clamped to >= 0 and Advect snaps any non-finite advection coord back to its cell.
//  (2) The vortex stable-id quantized position by a fixed 50 units — coarser than the ~31.25-unit
//      grid spacing (1000/Size) — so two DISTINCT vortices in adjacent cells collapsed to one FGuid,
//      breaking the unique-structure-id contract. IDs are now quantized per grid cell.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFluidsRobustnessTest, "MANIFOLD.Kernels.Fluids.RobustParamsAndUniqueVortexIds", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFluidsRobustnessTest::RunTest(const FString& Parameters)
{
    // --- (1) Negative diffusion params are clamped; the field stays finite (no NaN / OOB). ---
    UFluidsKernel* K = NewObject<UFluidsKernel>();
    K->Initialize(123ULL);
    K->SetParameter(TEXT("Viscosity"), TEXT("-0.01526")); // the exact denominator-to-zero value
    K->SetParameter(TEXT("Diffusion"), TEXT("-5.0"));
    FString V, D;
    K->GetParameter(TEXT("Viscosity"), V);
    K->GetParameter(TEXT("Diffusion"), D);
    UTEST_TRUE("negative viscosity clamped to >= 0", FCString::Atof(*V) >= 0.0f);
    UTEST_TRUE("negative diffusion clamped to >= 0", FCString::Atof(*D) >= 0.0f);

    K->AddVelocity(0.5f, 0.45f, 30.0f, 0.0f);
    K->AddVelocity(0.5f, 0.55f, -30.0f, 0.0f);
    for (int32 s = 0; s < 6; ++s) { K->Step(0.016f); } // old code: NaN here -> out-of-bounds index

    const FFluidsState* FS = static_cast<const FFluidsState*>(K->GetState().Get());
    bool bFinite = (FS != nullptr);
    if (FS)
    {
        for (float u : FS->VelocityX) { if (!FMath::IsFinite(u)) { bFinite = false; break; } }
        for (float u : FS->VelocityY) { if (!FMath::IsFinite(u)) { bFinite = false; break; } }
    }
    UTEST_TRUE("velocity field stays finite despite hostile diffusion params", bFinite);

    // --- (2) Vortices at DIFFERENT positions must have DIFFERENT ids (the collision fix). ---
    UFluidsKernel* K2 = NewObject<UFluidsKernel>();
    K2->Initialize(456ULL);
    K2->AddVelocity(0.44f, 0.5f, 0.0f, 45.0f);
    K2->AddVelocity(0.56f, 0.5f, 0.0f, -45.0f);
    K2->AddVelocity(0.5f, 0.44f, 45.0f, 0.0f);
    K2->AddVelocity(0.5f, 0.56f, -45.0f, 0.0f);
    for (int32 s = 0; s < 4; ++s) { K2->Step(0.016f); }

    const TArray<FFluidVortex>& Vs = K2->GetActiveVortices();
    bool bDistinctIds = true;
    for (int32 a = 0; a < Vs.Num() && bDistinctIds; ++a)
    {
        for (int32 b = a + 1; b < Vs.Num(); ++b)
        {
            if (!Vs[a].Position.Equals(Vs[b].Position, 0.01f) && Vs[a].Id == Vs[b].Id)
            {
                bDistinctIds = false;
                break;
            }
        }
    }
    UTEST_TRUE(FString::Printf(TEXT("distinct-position vortices have distinct ids (%d vortices)"), Vs.Num()), bDistinctIds);

    return true;
}
