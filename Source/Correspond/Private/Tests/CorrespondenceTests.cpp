// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "CorrespondenceSystem.h"
#include "OrbitsKernel.h"
#include "FluidsKernel.h"
#include "Misc/Paths.h"

// Shared scenario: a 3:2 orbital resonance in Orbits and a vortex in Fluids —
// the canonical Orbits<->Fluids correspondence pair.
static void Manifold_SetupResonanceAndVortex(UOrbitsKernel* Orbits, UFluidsKernel* Fluids)
{
    FOrbitsBodyDef Star; Star.Mass = 1.989e30; Star.bIsCentral = true; Orbits->AddBody(Star);

    FOrbitsBodyDef A; A.Mass = 1e24; A.Position = FVector(1.496e11, 0.0, 0.0);
    const double vA = FMath::Sqrt((Orbits->G * Star.Mass) / A.Position.X); A.Velocity = FVector(0.0, vA, 0.0);
    Orbits->AddBody(A);

    const double rB = A.Position.X * FMath::Pow(1.5, 2.0 / 3.0);
    FOrbitsBodyDef B; B.Mass = 1e24; B.Position = FVector(rB, 0.0, 0.0);
    const double vB = FMath::Sqrt((Orbits->G * Star.Mass) / rB); B.Velocity = FVector(0.0, vB, 0.0);
    Orbits->AddBody(B);
    Orbits->Step(0.1f);

    Fluids->AddVelocity(0.5f, 0.45f, 20.0f, 0.0f);
    Fluids->AddVelocity(0.55f, 0.5f, 0.0f, 20.0f);
    Fluids->AddVelocity(0.5f, 0.55f, -20.0f, 0.0f);
    Fluids->AddVelocity(0.45f, 0.5f, 0.0f, -20.0f);
    Fluids->Step(0.016f);
    Fluids->Step(0.016f);
}

// Constellation Lock — the relation normalizer is the single source of truth for what
// "corresponds". Two ratios correspond under a relation IFF they normalize to the same
// string. This proves each relation collapses the right classes and keeps the wrong ones
// apart, including that Exact reproduces the literal-ratio behavior (so all prior tests hold).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCorrespondenceRelationNormalizeTest, "MANIFOLD.Correspondence.RelationNormalize", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCorrespondenceRelationNormalizeTest::RunTest(const FString& Parameters)
{
    using EN = ECorrespondenceRelation;
    auto Norm = [](const FString& R, EN Rel) { return UCorrespondenceSystem::NormalizeRatio(R, Rel); };

    // --- Exact: literal reduced ratio; already-reduced input is unchanged (the
    //     invariant that keeps the existing 51 tests green). ---
    UTEST_EQUAL("Exact 3:2 stays 3:2", Norm(TEXT("3:2"), EN::Exact), FString(TEXT("3:2")));
    UTEST_EQUAL("Exact reduces 6:4 -> 3:2", Norm(TEXT("6:4"), EN::Exact), FString(TEXT("3:2")));
    UTEST_NOT_EQUAL("Exact keeps 3:2 and 3:1 distinct",
        Norm(TEXT("3:2"), EN::Exact), Norm(TEXT("3:1"), EN::Exact));

    // --- OctaveInvariant: divide out factors of 2, then reduce. 3:2, 6:4 and 3:1 all
    //     collapse to the same class; 5:4 does not join them. ---
    UTEST_EQUAL("Octave 6:4 == 3:2", Norm(TEXT("6:4"), EN::OctaveInvariant), Norm(TEXT("3:2"), EN::OctaveInvariant));
    UTEST_EQUAL("Octave 3:2 == 3:1", Norm(TEXT("3:2"), EN::OctaveInvariant), Norm(TEXT("3:1"), EN::OctaveInvariant));
    UTEST_EQUAL("Octave 3:2 == 12:8", Norm(TEXT("3:2"), EN::OctaveInvariant), Norm(TEXT("12:8"), EN::OctaveInvariant));
    UTEST_NOT_EQUAL("Octave keeps 3:2 and 5:4 apart",
        Norm(TEXT("3:2"), EN::OctaveInvariant), Norm(TEXT("5:4"), EN::OctaveInvariant));
    UTEST_NOT_EQUAL("Octave keeps 5:3 (->5:3) and 5:4 (->5:1) apart",
        Norm(TEXT("5:3"), EN::OctaveInvariant), Norm(TEXT("5:4"), EN::OctaveInvariant));

    // --- Reciprocal: p:q corresponds to q:p (order-independent min:max key). ---
    UTEST_EQUAL("Reciprocal 3:2 == 2:3", Norm(TEXT("3:2"), EN::Reciprocal), Norm(TEXT("2:3"), EN::Reciprocal));
    UTEST_EQUAL("Reciprocal 5:4 == 4:5", Norm(TEXT("5:4"), EN::Reciprocal), Norm(TEXT("4:5"), EN::Reciprocal));
    UTEST_NOT_EQUAL("Reciprocal keeps 3:2 and 5:4 apart",
        Norm(TEXT("3:2"), EN::Reciprocal), Norm(TEXT("5:4"), EN::Reciprocal));

    // --- Robustness: non-ratio / non-positive input is returned unchanged (never crashes). ---
    UTEST_EQUAL("Non-ratio passes through", Norm(TEXT("hello"), EN::Exact), FString(TEXT("hello")));
    UTEST_EQUAL("Zero denominator passes through", Norm(TEXT("3:0"), EN::OctaveInvariant), FString(TEXT("3:0")));

    return true;
}

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

// WP D1: the correspondence is authored as data (JSON), not hardcoded. Loading the
// content file and feeding it to the system must ignite the 3:2<->vortex seam.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCorrespondenceDataDrivenTest, "MANIFOLD.Correspondence.DataDrivenMapping", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCorrespondenceDataDrivenTest::RunTest(const FString& Parameters)
{
    const FString MappingPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/Correspondences/OrbitsFluids.json"));
    UCorrespondenceMapping* Mapping = UCorrespondenceMapping::CreateFromJsonFile(MappingPath, GetTransientPackage());

    UTEST_TRUE("Correspondence content file loaded", Mapping != nullptr);
    UTEST_EQUAL("One spec parsed from JSON", Mapping->Specs.Num(), 1);
    UTEST_EQUAL("Source structure parsed", Mapping->Specs[0].SourceStructureType, FName(TEXT("OrbitalResonance")));
    UTEST_EQUAL("Target structure parsed", Mapping->Specs[0].TargetStructureType, FName(TEXT("VortexCenter")));
    UTEST_EQUAL("Matching ratio parsed", Mapping->Specs[0].MatchingRatio, FString(TEXT("3:2")));

    UOrbitsKernel* Orbits = NewObject<UOrbitsKernel>();
    UFluidsKernel* Fluids = NewObject<UFluidsKernel>();
    UCorrespondenceSystem* Correspond = NewObject<UCorrespondenceSystem>();
    Orbits->Initialize(1111ULL);
    Fluids->Initialize(2222ULL);
    Correspond->RegisterKernels(Orbits, Fluids);
    Correspond->InitializeMapping(Mapping);
    Manifold_SetupResonanceAndVortex(Orbits, Fluids);

    bool bIgnited = false;
    Correspond->OnCorrespondenceIgnited.AddLambda([&bIgnited](FGuid, FGuid, float) { bIgnited = true; });

    UTEST_TRUE("Data-driven detect succeeds", Correspond->DetectCorrespondence());
    UTEST_TRUE("Data-driven correspondence ignited", bIgnited);

    return true;
}

// WP D1 (fairness/negative): a spec whose ratio doesn't match the actual resonance
// must NOT ignite — the system only fires on the authored structure, and with a
// mapping present it does not fall back to the built-in rule.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCorrespondenceRejectsInvalidSpecTest, "MANIFOLD.Correspondence.DataDrivenRejectsInvalid", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCorrespondenceRejectsInvalidSpecTest::RunTest(const FString& Parameters)
{
    const FString BadJson = TEXT("{\"specs\":[{\"sourceStructureType\":\"OrbitalResonance\",\"targetStructureType\":\"VortexCenter\",\"matchingRatio\":\"5:4\",\"tolerance\":0.05,\"scaleFactor\":1.0}]}");
    UCorrespondenceMapping* BadMapping = UCorrespondenceMapping::CreateFromJsonString(BadJson, GetTransientPackage());
    UTEST_TRUE("Bad mapping parsed", BadMapping != nullptr);
    UTEST_EQUAL("Bad mapping ratio is 5:4", BadMapping->Specs[0].MatchingRatio, FString(TEXT("5:4")));

    UOrbitsKernel* Orbits = NewObject<UOrbitsKernel>();
    UFluidsKernel* Fluids = NewObject<UFluidsKernel>();
    UCorrespondenceSystem* Correspond = NewObject<UCorrespondenceSystem>();
    Orbits->Initialize(1111ULL);
    Fluids->Initialize(2222ULL);
    Correspond->RegisterKernels(Orbits, Fluids);
    Correspond->InitializeMapping(BadMapping);
    Manifold_SetupResonanceAndVortex(Orbits, Fluids);

    bool bIgnited = false;
    Correspond->OnCorrespondenceIgnited.AddLambda([&bIgnited](FGuid, FGuid, float) { bIgnited = true; });

    UTEST_FALSE("Mismatched ratio does not detect", Correspond->DetectCorrespondence());
    UTEST_FALSE("Mismatched ratio does not ignite", bIgnited);

    return true;
}
