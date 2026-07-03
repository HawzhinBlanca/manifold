// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "OrbitsKernel.h"

// Realm template step 1 (Build Plan §9): the kernel is deterministic — the same seed
// + bodies advanced identically must produce bitwise-identical state. (Orbits was the
// only kernel without this test, which hid a VerifyDeterminism bug.)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitsDeterminismTest, "MANIFOLD.Kernels.Orbits.Deterministic", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOrbitsDeterminismTest::RunTest(const FString& Parameters)
{
    UOrbitsKernel* Kernel = NewObject<UOrbitsKernel>();
    Kernel->Initialize(777ULL);

    FOrbitsBodyDef Star;
    Star.Mass = 1.989e30;
    Star.bIsCentral = true;
    Kernel->AddBody(Star);

    FOrbitsBodyDef Planet;
    Planet.Mass = 1e24;
    Planet.Position = FVector(1.496e11, 0.0, 0.0);
    const double V = FMath::Sqrt((Kernel->G * Star.Mass) / Planet.Position.X);
    Planet.Velocity = FVector(0.0, V, 0.0);
    Kernel->AddBody(Planet);

    UTEST_TRUE("Orbits determinism", Kernel->VerifyDeterminism(100));
    return true;
}

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

// The resonance StructureId must also be CONTENT-STABLE across identical seeded runs: body
// ids were FGuid::NewGuid() (random per run), so the XOR-derived resonance id diverged run
// to run — breaking the deterministic-id contract the sibling realms uphold. Two fresh
// kernels built the same way from the same seed must expose the SAME id.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitsResonanceIdAcrossRunsTest, "MANIFOLD.Kernels.Orbits.StableResonanceIdAcrossRuns", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOrbitsResonanceIdAcrossRunsTest::RunTest(const FString& Parameters)
{
    auto BuildResonanceId = [](uint64 Seed) -> FGuid
    {
        UOrbitsKernel* K = NewObject<UOrbitsKernel>();
        K->Initialize(Seed);
        FOrbitsBodyDef Star; Star.Mass = 1.989e30; Star.bIsCentral = true; K->AddBody(Star);
        FOrbitsBodyDef A; A.Mass = 1e24; A.Position = FVector(1.496e11, 0.0, 0.0);
        A.Velocity = FVector(0.0, FMath::Sqrt((K->G * Star.Mass) / A.Position.X), 0.0); K->AddBody(A);
        const double rB = A.Position.X * FMath::Pow(1.5, 2.0 / 3.0);
        FOrbitsBodyDef B; B.Mass = 1e24; B.Position = FVector(rB, 0.0, 0.0);
        B.Velocity = FVector(0.0, FMath::Sqrt((K->G * Star.Mass) / rB), 0.0); K->AddBody(B);
        K->Step(0.1f);
        FRealmQuery Q(TEXT("OrbitalResonance"));
        FRealmQueryResult R;
        return K->Query(Q, R) ? R.StructureId : FGuid();
    };

    const FGuid Id1 = BuildResonanceId(4242ULL);
    const FGuid Id2 = BuildResonanceId(4242ULL);
    UTEST_TRUE("a resonance is found", Id1.IsValid());
    UTEST_EQUAL("same seed -> same resonance StructureId across separate runs", Id1, Id2);

    return true;
}

// Regression (adversarial audit): ComputeStateHash folded only Positions and Velocities and
// omitted per-body Mass — yet mass drives the next step's accelerations (ComputeAccelerations
// multiplies by Masses[j]/Masses[i]). Two states with identical positions/velocities but a
// different mass are genuinely divergent and previously hashed IDENTICALLY, hiding the
// divergence from the hash-based determinism/round-trip checks. Mass is now folded in.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitsHashCoversMassTest, "MANIFOLD.Kernels.Orbits.HashCoversMass", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FOrbitsHashCoversMassTest::RunTest(const FString& Parameters)
{
    // Build a two-body state at fixed positions/velocities, parameterized only by the central
    // mass. No Step() is taken, so Positions/Velocities are byte-identical across mass values —
    // the ONLY differing state is the mass.
    auto BuildHash = [](double StarMass) -> uint64
    {
        UOrbitsKernel* K = NewObject<UOrbitsKernel>();
        K->Initialize(2024ULL);
        FOrbitsBodyDef Star; Star.Name = TEXT("Star"); Star.Mass = StarMass; Star.bIsCentral = true;
        K->AddBody(Star);
        FOrbitsBodyDef Planet; Planet.Name = TEXT("Planet"); Planet.Mass = 1e24;
        Planet.Position = FVector(1.496e11, 0.0, 0.0);
        Planet.Velocity = FVector(0.0, 30000.0, 0.0);
        K->AddBody(Planet);
        return K->ComputeStateHash();
    };

    const uint64 HRef = BuildHash(1.989e30);
    UTEST_TRUE("a mass-only divergence must change the Orbits state hash", HRef != BuildHash(1.989e31));
    UTEST_TRUE("identical bodies hash identically", HRef == BuildHash(1.989e30));

    return true;
}
