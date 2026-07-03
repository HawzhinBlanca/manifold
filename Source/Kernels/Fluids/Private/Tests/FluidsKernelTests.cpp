// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "FluidsKernel.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"

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

// Regression (adversarial audit): ComputeStateHash folded ONLY the density grid `d`, omitting
// the velocity fields u/v that drive advection and vortex detection every step. Two states that
// differ ONLY in velocity — with a byte-identical density grid — therefore hashed IDENTICALLY,
// so a genuine simulation divergence in velocity slipped past VerifyDeterminism and the replay
// comparator (the state hash IS the divergence detector). The hash now folds u and v; a
// velocity-only difference must change it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFluidsHashCoversVelocityTest, "MANIFOLD.Kernels.Fluids.HashCoversVelocity", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFluidsHashCoversVelocityTest::RunTest(const FString& Parameters)
{
    UFluidsKernel* SeedK = NewObject<UFluidsKernel>();
    SeedK->Initialize(9090ULL);
    SeedK->AddVelocity(0.5f, 0.45f, 20.0f, 0.0f);
    SeedK->AddVelocity(0.5f, 0.55f, -20.0f, 0.0f);
    for (int32 s = 0; s < 4; ++s) { SeedK->Step(0.016f); }

    const FFluidsState Snapshot = *static_cast<const FFluidsState*>(SeedK->GetState().Get());
    UTEST_TRUE("snapshot has a populated velocity grid", Snapshot.VelocityX.Num() > 0);

    auto HashOf = [](const FFluidsState& S) -> uint64
    {
        UFluidsKernel* Tmp = NewObject<UFluidsKernel>();
        Tmp->SetState(S); // repopulates d/u/v from the state's Density/VelocityX/VelocityY
        return Tmp->ComputeStateHash();
    };

    const uint64 HBase = HashOf(Snapshot);
    UTEST_TRUE("identical state hashes identically", HashOf(Snapshot) == HBase);

    // Perturb ONE velocity-X cell; the density grid stays byte-identical to the baseline.
    FFluidsState VelPerturbed = Snapshot;
    VelPerturbed.VelocityX[VelPerturbed.VelocityX.Num() / 2] += 3.5f;
    UTEST_TRUE("a velocity-only divergence must change the state hash", HashOf(VelPerturbed) != HBase);

    // Control: a density-only change also changes the hash (as it always did).
    FFluidsState DenPerturbed = Snapshot;
    DenPerturbed.Density[DenPerturbed.Density.Num() / 2] += 0.25f;
    UTEST_TRUE("a density-only divergence changes the state hash", HashOf(DenPerturbed) != HBase);

    return true;
}

// Regression (adversarial audit): FFluidsState serializes GridSize and the three grid arrays
// (Density/VelocityX/VelocityY) INDEPENDENTLY, so a corrupted/hostile snapshot can carry a
// GridSize inconsistent with the array lengths, or a huge GridSize whose (Size+2)^2 overflows
// int32. SetState previously adopted GridSize verbatim; the next Step then indexed the short
// arrays out to (Size+2)^2 -> out-of-bounds read/write (crash/UB). SetState now validates the
// untrusted state and falls back to a clean zeroed grid at a clamped size on any mismatch.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFluidsSetStateUntrustedTest, "MANIFOLD.Kernels.Fluids.SetStateRejectsMalformedGrid", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFluidsSetStateUntrustedTest::RunTest(const FString& Parameters)
{
    // (1) GridSize (claims 64 -> area 66*66=4356) inconsistent with the array lengths (a Size=8
    //     grid, 100 floats). Pre-fix the next Step's AddSource loop indexed u[0..4355] on a
    //     100-element array. SetState must repair the grid to self-consistency and Step safely.
    FFluidsState Bad;
    Bad.GridSize = 64;
    const int32 ShortLen = (8 + 2) * (8 + 2); // 100
    Bad.Density.Init(0.25f, ShortLen);
    Bad.VelocityX.Init(1.0f, ShortLen);
    Bad.VelocityY.Init(-1.0f, ShortLen);

    UFluidsKernel* K = NewObject<UFluidsKernel>();
    K->Initialize(11ULL);
    K->SetState(Bad);

    const FFluidsState* S = static_cast<const FFluidsState*>(K->GetState().Get());
    UTEST_TRUE("state valid after malformed load", S != nullptr);
    const int32 N = S->GridSize;
    const int32 Expected = (N + 2) * (N + 2);
    UTEST_TRUE("grid arrays are self-consistent after malformed load",
        S->Density.Num() == Expected && S->VelocityX.Num() == Expected && S->VelocityY.Num() == Expected);

    for (int32 s = 0; s < 4; ++s) { K->Step(0.016f); } // pre-fix: out-of-bounds here
    const FFluidsState* S2 = static_cast<const FFluidsState*>(K->GetState().Get());
    bool bFinite = true;
    for (float x : S2->VelocityX) { if (!FMath::IsFinite(x)) { bFinite = false; break; } }
    for (float x : S2->Density)   { if (!FMath::IsFinite(x)) { bFinite = false; break; } }
    UTEST_TRUE("field stays finite after stepping a repaired grid", bFinite);

    // (2) Oversized GridSize whose int32 (Size+2)^2 overflows must be clamped, not honored.
    FFluidsState Huge;
    Huge.GridSize = 65535; // (65537)^2 overflows int32
    Huge.Density.Init(0.0f, 4);
    Huge.VelocityX.Init(0.0f, 4);
    Huge.VelocityY.Init(0.0f, 4);
    UFluidsKernel* K2 = NewObject<UFluidsKernel>();
    K2->Initialize(22ULL);
    K2->SetState(Huge);
    const FFluidsState* S3 = static_cast<const FFluidsState*>(K2->GetState().Get());
    UTEST_TRUE("oversized GridSize clamped into [1, MaxGridSize]", S3->GridSize >= 1 && S3->GridSize <= 512);
    UTEST_TRUE("clamped grid arrays are self-consistent",
        S3->Density.Num() == (S3->GridSize + 2) * (S3->GridSize + 2));
    K2->Step(0.016f); // must not overflow/crash

    // (3) A VALID self-consistent state is still adopted verbatim (happy path unchanged).
    UFluidsKernel* Src = NewObject<UFluidsKernel>();
    Src->Initialize(33ULL);
    Src->AddVelocity(0.5f, 0.5f, 15.0f, 0.0f);
    for (int32 s = 0; s < 3; ++s) { Src->Step(0.016f); }
    const FFluidsState Good = *static_cast<const FFluidsState*>(Src->GetState().Get());
    UFluidsKernel* Dst = NewObject<UFluidsKernel>();
    Dst->Initialize(33ULL);
    Dst->SetState(Good);
    UTEST_TRUE("valid state adopted with matching hash (happy path preserved)",
        Dst->ComputeStateHash() == Src->ComputeStateHash());

    return true;
}

// Regression (kernel audit): the solver config (Viscosity / Diffusion / Decay) drives the fluid
// every step, but was NOT carried through serialize/deserialize — a save taken after a config change
// reverted to defaults on load, silently diverging the reproduced simulation. Config is now mirrored
// into FFluidsState (round-tripped) and folded into the divergence hash. This test proves (a) config
// survives a serialize round-trip and the recomputed hash matches, (b) a config-only divergence
// changes the hash (the detector has teeth), and (c) hostile config from an untrusted snapshot is
// sanitized in SetState so the next Step can't NaN -> out-of-bounds.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFluidsConfigRoundTripTest, "MANIFOLD.Kernels.Fluids.ConfigRoundTrips", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFluidsConfigRoundTripTest::RunTest(const FString& Parameters)
{
    // (a) Non-default config survives serialize -> deserialize into a fresh kernel.
    UFluidsKernel* A = NewObject<UFluidsKernel>();
    A->Initialize(7ULL);
    A->SetParameter(TEXT("Viscosity"), TEXT("0.05"));
    A->SetParameter(TEXT("Diffusion"), TEXT("0.03"));
    A->SetParameter(TEXT("Decay"),     TEXT("0.9"));
    A->AddVelocity(0.5f, 0.45f, 20.0f, 0.0f);
    A->AddVelocity(0.55f, 0.5f, 0.0f, 20.0f);
    A->Step(0.016f);

    TArray<uint8> Bytes;
    FMemoryWriter W(Bytes, /*bIsPersistent*/ true);
    A->SerializeState(W);

    UFluidsKernel* B = NewObject<UFluidsKernel>();
    B->Initialize(999ULL); // different seed: only the loaded state should determine B
    FMemoryReader R(Bytes, /*bIsPersistent*/ true);
    B->DeserializeState(R);

    FString AV, AD, ADe, BV, BD, BDe;
    A->GetParameter(TEXT("Viscosity"), AV); A->GetParameter(TEXT("Diffusion"), AD); A->GetParameter(TEXT("Decay"), ADe);
    B->GetParameter(TEXT("Viscosity"), BV); B->GetParameter(TEXT("Diffusion"), BD); B->GetParameter(TEXT("Decay"), BDe);
    UTEST_TRUE("viscosity survives round-trip", FMath::IsNearlyEqual(FCString::Atof(*AV), FCString::Atof(*BV), 1e-5f));
    UTEST_TRUE("diffusion survives round-trip", FMath::IsNearlyEqual(FCString::Atof(*AD), FCString::Atof(*BD), 1e-5f));
    UTEST_TRUE("decay survives round-trip",     FMath::IsNearlyEqual(FCString::Atof(*ADe), FCString::Atof(*BDe), 1e-5f));
    UTEST_TRUE("round-trip recomputed hash matches (config carried)", A->ComputeStateHash() == B->ComputeStateHash());

    // (b) A config-only divergence must change the hash. Two same-seed kernels have byte-identical
    //     seeded grids; changing only Viscosity must make them hash apart, else the detector is blind.
    auto HashWithViscosity = [](float Visc) -> uint64
    {
        UFluidsKernel* K = NewObject<UFluidsKernel>();
        K->Initialize(321ULL);
        K->SetParameter(TEXT("Viscosity"), FString::SanitizeFloat(Visc));
        return K->ComputeStateHash();
    };
    const uint64 HRef = HashWithViscosity(0.0001f);
    UTEST_TRUE("a viscosity-only divergence changes the hash", HRef != HashWithViscosity(0.05f));
    UTEST_TRUE("identical config hashes identically", HRef == HashWithViscosity(0.0001f));

    // (c) Hostile config on an otherwise VALID grid: negative diffusion coefficients (the NaN->OOB
    //     hazard) and a non-finite decay must be sanitized in SetState so the next Step stays finite.
    UFluidsKernel* SrcC = NewObject<UFluidsKernel>();
    SrcC->Initialize(44ULL);
    SrcC->AddVelocity(0.5f, 0.5f, 15.0f, 0.0f);
    for (int32 s = 0; s < 3; ++s) { SrcC->Step(0.016f); }
    FFluidsState Hostile = *static_cast<const FFluidsState*>(SrcC->GetState().Get()); // grid is self-consistent
    Hostile.Viscosity = -5.0f;
    Hostile.Diffusion = -3.0f;
    float NanF; const uint32 NanBits = 0x7FC00000u; FMemory::Memcpy(&NanF, &NanBits, sizeof(float)); // quiet NaN
    Hostile.Decay = NanF;

    UFluidsKernel* KC = NewObject<UFluidsKernel>();
    KC->Initialize(44ULL);
    KC->SetState(Hostile);

    FString HV, HD, HDe;
    KC->GetParameter(TEXT("Viscosity"), HV); KC->GetParameter(TEXT("Diffusion"), HD); KC->GetParameter(TEXT("Decay"), HDe);
    UTEST_TRUE("hostile negative viscosity clamped to >= 0", FCString::Atof(*HV) >= 0.0f);
    UTEST_TRUE("hostile negative diffusion clamped to >= 0", FCString::Atof(*HD) >= 0.0f);
    UTEST_TRUE("hostile non-finite decay sanitized to a finite value", FMath::IsFinite(FCString::Atof(*HDe)));

    for (int32 s = 0; s < 6; ++s) { KC->Step(0.016f); } // pre-fix: negative diffusion -> NaN -> OOB index
    const FFluidsState* SC = static_cast<const FFluidsState*>(KC->GetState().Get());
    bool bFinite = true;
    for (float x : SC->VelocityX) { if (!FMath::IsFinite(x)) { bFinite = false; break; } }
    for (float x : SC->Density)   { if (!FMath::IsFinite(x)) { bFinite = false; break; } }
    UTEST_TRUE("field stays finite after stepping sanitized hostile config", bFinite);

    return true;
}
