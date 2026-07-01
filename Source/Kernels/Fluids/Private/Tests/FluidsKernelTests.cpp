// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "FluidsKernel.h"

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
