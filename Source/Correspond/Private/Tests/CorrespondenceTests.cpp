// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "CorrespondenceSystem.h"
#include "OrbitsKernel.h"
#include "FluidsKernel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCorrespondenceValidationTest, "MANIFOLD.Correspondence.MappingValidation", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCorrespondenceValidationTest::RunTest(const FString& Parameters)
{
    UOrbitsKernel* Orbits = NewObject<UOrbitsKernel>();
    UFluidsKernel* Fluids = NewObject<UFluidsKernel>();
    UCorrespondenceSystem* Correspond = NewObject<UCorrespondenceSystem>();
    
    Orbits->Initialize(1111ULL);
    Fluids->Initialize(2222ULL);
    Correspond->RegisterKernels(Orbits, Fluids);
    
    // Add resonance to Orbits
    FOrbitsBodyDef Star;
    Star.Mass = 1.989e30;
    Star.bIsCentral = true;
    Orbits->AddBody(Star);
    
    FOrbitsBodyDef PlanetA;
    PlanetA.Mass = 1e24;
    PlanetA.Position = FVector(1.496e11, 0.0, 0.0);
    double V_A = FMath::Sqrt((Orbits->G * Star.Mass) / PlanetA.Position.X);
    PlanetA.Velocity = FVector(0.0, V_A, 0.0);
    FGuid IdA = Orbits->AddBody(PlanetA);
    
    double R_B = PlanetA.Position.X * FMath::Pow(1.5, 2.0 / 3.0); // 3:2 resonance
    FOrbitsBodyDef PlanetB;
    PlanetB.Mass = 1e24;
    PlanetB.Position = FVector(R_B, 0.0, 0.0);
    double V_B = FMath::Sqrt((Orbits->G * Star.Mass) / R_B);
    PlanetB.Velocity = FVector(0.0, V_B, 0.0);
    FGuid IdB = Orbits->AddBody(PlanetB);
    
    Orbits->Step(0.1f); // compute elements & resonances
    
    // Add vortex to Fluids
    Fluids->AddVelocity(0.5f, 0.45f, 20.0f, 0.0f);
    Fluids->AddVelocity(0.55f, 0.5f, 0.0f, 20.0f);
    Fluids->AddVelocity(0.5f, 0.55f, -20.0f, 0.0f);
    Fluids->AddVelocity(0.45f, 0.5f, 0.0f, -20.0f);
    Fluids->Step(0.016f);
    Fluids->Step(0.016f);
    
    // Setup delegate
    bool bIgnited = false;
    Correspond->OnCorrespondenceIgnited.AddLambda([&bIgnited](FGuid A, FGuid B, float Scale) {
        bIgnited = true;
    });
    
    // Detect
    Correspond->DetectCorrespondence();
    
    UTEST_TRUE("Ignited correspondence successfully", bIgnited);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransportVerificationTest, "MANIFOLD.Transport.StateChangeVerification", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTransportVerificationTest::RunTest(const FString& Parameters)
{
    UOrbitsKernel* Orbits = NewObject<UOrbitsKernel>();
    UFluidsKernel* Fluids = NewObject<UFluidsKernel>();
    UCorrespondenceSystem* Correspond = NewObject<UCorrespondenceSystem>();
    
    Orbits->Initialize(1111ULL);
    Fluids->Initialize(2222ULL);
    Correspond->RegisterKernels(Orbits, Fluids);
    
    // Inject vortex in Fluids
    Fluids->AddVelocity(0.5f, 0.45f, 20.0f, 0.0f);
    Fluids->AddVelocity(0.55f, 0.5f, 0.0f, 20.0f);
    Fluids->AddVelocity(0.5f, 0.55f, -20.0f, 0.0f);
    Fluids->AddVelocity(0.45f, 0.5f, 0.0f, -20.0f);
    Fluids->Step(0.016f);
    Fluids->Step(0.016f);
    
    const TArray<FFluidVortex>& Vortices = Fluids->GetActiveVortices();
    UTEST_GREATER("Has vortex to transport", Vortices.Num(), 0);
    FGuid VortexId = Vortices[0].Id;
    
    // Setup delegate for transport
    bool bTransported = false;
    FGuid TargetId;
    Correspond->OnTransportCompleted.AddLambda([&bTransported, &TargetId](FGuid Src, FName DestRealm, FGuid DestId, float Strength) {
        bTransported = true;
        TargetId = DestId;
    });
    
    // Transport Fluids -> Orbits
    UTEST_TRUE("Transport Fluids to Orbits", Correspond->Transport(VortexId, TEXT("Orbits")));
    UTEST_TRUE("Transport delegate fired", bTransported);
    UTEST_TRUE("Target body created", TargetId.IsValid());
    
    // Check that Orbits has the new planet
    const FOrbitsBodyDef* Planet = Orbits->GetBody(TargetId);
    UTEST_TRUE("Planet exists in Orbits after transport", Planet != nullptr);
    UTEST_TRUE("Planet has mapped mass", Planet->Mass > 0.0);

    return true;
}
