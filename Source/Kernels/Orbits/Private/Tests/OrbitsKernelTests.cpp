// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "OrbitsKernel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitsStableOrbitsTest, "MANIFOLD.Kernels.Orbits.StableOrbits", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOrbitsStableOrbitsTest::RunTest(const FString& Parameters)
{
    UOrbitsKernel* Kernel = NewObject<UOrbitsKernel>();
    Kernel->Initialize(54321ULL);
    
    // Add central star
    FOrbitsBodyDef Star;
    Star.Name = TEXT("Star");
    Star.Mass = 1.989e30; // 1 solar mass
    Star.Position = FVector::ZeroVector;
    Star.Velocity = FVector::ZeroVector;
    Star.bIsCentral = true;
    FGuid StarId = Kernel->AddBody(Star);
    
    // Add Earth at 1 AU
    FOrbitsBodyDef Planet;
    Planet.Name = TEXT("Earth");
    Planet.Mass = 5.972e24; // 1 Earth mass
    Planet.Position = FVector(1.496e11, 0.0, 0.0);
    // Circular velocity: v = sqrt(G * M / r)
    double V_circ = FMath::Sqrt((Kernel->G * Star.Mass) / Planet.Position.X);
    Planet.Velocity = FVector(0.0, V_circ, 0.0);
    Planet.bIsCentral = false;
    FGuid PlanetId = Kernel->AddBody(Planet);
    
    TSharedPtr<FRealmState> State = Kernel->GetState();
    FOrbitsState* CastedState = static_cast<FOrbitsState*>(State.Get());
    double InitialEnergy = CastedState->SystemTotalEnergy;
    
    // Step simulation 10 days
    for (int32 i = 0; i < 10; ++i)
    {
        Kernel->Step(86400.0f);
    }
    
    double FinalEnergy = CastedState->SystemTotalEnergy;
    double EnergyDrift = FMath::Abs((FinalEnergy - InitialEnergy) / (InitialEnergy + 1e-10));
    
    // Velocity Verlet energy drift should be minimal
    UTEST_TRUE("Verlet Energy conservation", EnergyDrift < 1e-4);
    
    FOrbitalElements Elem;
    UTEST_TRUE("Get Keplerian elements", Kernel->GetOrbitalElements(PlanetId, Elem));
    UTEST_TRUE("Is bound orbit", Elem.IsValid());
    UTEST_TRUE("Is circular orbit", Elem.IsCircular(0.02)); // small eccentricity drift allowed

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitsResonanceRatiosTest, "MANIFOLD.Kernels.Orbits.ResonanceRatios", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOrbitsResonanceRatiosTest::RunTest(const FString& Parameters)
{
    UOrbitsKernel* Kernel = NewObject<UOrbitsKernel>();
    Kernel->Initialize(54321ULL);
    
    FOrbitsBodyDef Star;
    Star.Name = TEXT("Star");
    Star.Mass = 1.989e30;
    Star.Position = FVector::ZeroVector;
    Star.Velocity = FVector::ZeroVector;
    Star.bIsCentral = true;
    Kernel->AddBody(Star);
    
    // Planet A at 1 AU
    FOrbitsBodyDef PlanetA;
    PlanetA.Name = TEXT("PlanetA");
    PlanetA.Mass = 1e24;
    PlanetA.Position = FVector(1.496e11, 0.0, 0.0);
    double V_A = FMath::Sqrt((Kernel->G * Star.Mass) / PlanetA.Position.X);
    PlanetA.Velocity = FVector(0.0, V_A, 0.0);
    PlanetA.bIsCentral = false;
    FGuid IdA = Kernel->AddBody(PlanetA);
    
    // Planet B in 3:2 resonance (Period ratio 1.5)
    // T^2 proportional to a^3 => a_B = a_A * (1.5)^(2/3)
    double R_B = PlanetA.Position.X * FMath::Pow(1.5, 2.0 / 3.0);
    FOrbitsBodyDef PlanetB;
    PlanetB.Name = TEXT("PlanetB");
    PlanetB.Mass = 1e24;
    PlanetB.Position = FVector(R_B, 0.0, 0.0);
    double V_B = FMath::Sqrt((Kernel->G * Star.Mass) / R_B);
    PlanetB.Velocity = FVector(0.0, V_B, 0.0);
    PlanetB.bIsCentral = false;
    FGuid IdB = Kernel->AddBody(PlanetB);

    // Run resonance detection
    Kernel->Step(0.1f); // Single step to compute elements & resonances

    const TArray<FResonanceMatch>& Res = Kernel->GetActiveResonances();
    UTEST_GREATER("Detected resonances", Res.Num(), 0);
    
    bool bFound32 = false;
    for (const FResonanceMatch& Match : Res)
    {
        if (Match.Ratio.X == 3 && Match.Ratio.Y == 2)
        {
            bFound32 = true;
            break;
        }
    }
    UTEST_TRUE("Found 3:2 resonance ratio", bFound32);

    return true;
}

// A detected resonance must keep a STABLE identity across frames (derived from the
// body pair), so the correspondence engine dedups correctly and the player can
// select the same structure frame to frame.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitsStableResonanceIdTest, "MANIFOLD.Kernels.Orbits.StableResonanceId", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOrbitsStableResonanceIdTest::RunTest(const FString& Parameters)
{
    UOrbitsKernel* Kernel = NewObject<UOrbitsKernel>();
    Kernel->Initialize(54321ULL);

    FOrbitsBodyDef Star; Star.Mass = 1.989e30; Star.bIsCentral = true; Kernel->AddBody(Star);
    FOrbitsBodyDef A; A.Mass = 1e24; A.Position = FVector(1.496e11, 0.0, 0.0);
    const double vA = FMath::Sqrt((Kernel->G * Star.Mass) / A.Position.X); A.Velocity = FVector(0.0, vA, 0.0);
    Kernel->AddBody(A);
    const double rB = A.Position.X * FMath::Pow(1.5, 2.0 / 3.0);
    FOrbitsBodyDef B; B.Mass = 1e24; B.Position = FVector(rB, 0.0, 0.0);
    const double vB = FMath::Sqrt((Kernel->G * Star.Mass) / rB); B.Velocity = FVector(0.0, vB, 0.0);
    Kernel->AddBody(B);

    FRealmQuery Query(TEXT("OrbitalResonance"));

    Kernel->Step(0.1f);
    FRealmQueryResult R1;
    UTEST_TRUE("First query finds a resonance", Kernel->Query(Query, R1));
    UTEST_TRUE("Resonance id is valid", R1.StructureId.IsValid());

    Kernel->Step(0.1f);
    FRealmQueryResult R2;
    UTEST_TRUE("Second query finds a resonance", Kernel->Query(Query, R2));

    UTEST_EQUAL("Resonance StructureId is stable across frames", R1.StructureId, R2.StructureId);

    return true;
}
