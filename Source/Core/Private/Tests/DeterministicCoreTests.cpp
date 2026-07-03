// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "DeterministicRNG.h"
#include "FixedStepSimulation.h"
#include "RealmKernel.h"
#include "LazyRealizationManager.h"
#include "Serialization/BufferArchive.h"

// Pure C++ mock for interface testing
struct FMockKernel : public IRealmKernel
{
    int64 StepCount = 0;

    virtual FName GetRealmId() const override { return TEXT("Mock"); }
    virtual FText GetDisplayName() const override { return FText::FromString(TEXT("Mock")); }
    virtual uint32 GetSimulationVersion() const override { return 1; }
    
    virtual void Initialize(uint64 Seed) override { StepCount = 0; }
    virtual void Reset() override { StepCount = 0; }
    virtual void Shutdown() override {}
    
    virtual void Step(float DeltaTime) override { StepCount++; }
    
    virtual float GetSimulationTime() const override { return StepCount * 0.01f; }
    virtual int64 GetStepCount() const override { return StepCount; }
    
    virtual TSharedPtr<FRealmState> GetState() const override { return nullptr; }
    virtual void SetState(const FRealmState& NewState) override {}
    
    virtual void SerializeState(FArchive& Ar) override {}
    virtual void DeserializeState(FArchive& Ar) override {}
    
    virtual uint64 ComputeStateHash() const override { return 12345ULL; }
    
    virtual bool Query(const FRealmQuery& Query, FRealmQueryResult& Result) const override { return false; }
    virtual TArray<FName> GetSupportedQueryTypes() const override { return {}; }
    
    virtual TMap<FName, FString> GetParameterSchema() const override { return {}; }
    virtual bool SetParameter(FName Name, const FString& Value) override { return false; }
    virtual bool GetParameter(FName Name, FString& OutValue) const override { return false; }
    
    virtual bool VerifyDeterminism(int32 NumSteps) const override { return true; }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealmKernelInterfaceTest, "MANIFOLD.Systems.RealmKernelInterface", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRealmKernelInterfaceTest::RunTest(const FString& Parameters)
{
    FMockKernel Mock;
    IRealmKernel* Interface = &Mock;
    
    UTEST_EQUAL("Interface GetRealmId", Interface->GetRealmId().ToString(), TEXT("Mock"));
    
    Interface->Initialize(100ULL);
    Interface->Step(0.016f);
    UTEST_EQUAL("Interface Step execution", Interface->GetStepCount(), 1LL);
    UTEST_EQUAL("Interface State Hash", Interface->ComputeStateHash(), 12345ULL);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeterministicRNGTest, "MANIFOLD.Systems.DeterministicCore.RNG", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDeterministicRNGTest::RunTest(const FString& Parameters)
{
    FDeterministicRNG RNG_A(12345ULL);
    FDeterministicRNG RNG_B(12345ULL);
    
    // Verify seed equivalence
    UTEST_TRUE("RNG equivalence", FDeterministicRNG::VerifyEquivalence(RNG_A, RNG_B));
    
    // Verify substream determinism
    FDeterministicRNG Sub_A = RNG_A.CreateSubstream(1);
    FDeterministicRNG Sub_B = RNG_B.CreateSubstream(1);
    UTEST_TRUE("Substream equivalence", FDeterministicRNG::VerifyEquivalence(Sub_A, Sub_B));
    
    // Consume some values
    for (int32 i = 0; i < 50; ++i)
    {
        RNG_A.NextUint32();
    }

    // Serialize RNG state
    FBufferArchive Archive;
    Archive << RNG_A;
    
    // Deserialize into new RNG
    FDeterministicRNG RNG_C;
    FMemoryReader Reader(Archive);
    Reader << RNG_C;
    
    // Verify identical internal states
    UTEST_EQUAL("RNG state after deserialization", RNG_A.GetState(), RNG_C.GetState());
    UTEST_EQUAL("RNG consumption count", RNG_A.GetConsumptionCount(), RNG_C.GetConsumptionCount());
    UTEST_EQUAL("RNG original seed", RNG_A.GetOriginalSeed(), RNG_C.GetOriginalSeed());
    
    // Verify sequence matches after load
    UTEST_TRUE("RNG sequence equivalence after serialization", FDeterministicRNG::VerifyEquivalence(RNG_A, RNG_C, 100));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFixedStepSimulationTest, "MANIFOLD.Systems.DeterministicCore.FixedStep", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFixedStepSimulationTest::RunTest(const FString& Parameters)
{
    FFixedStepConfig Config;
    Config.FixedDeltaTime = 0.01f;
    Config.MaxAccumulatedTime = 0.05f;

    FFixedStepSimulation Sim(Config);
    Sim.Initialize(98765ULL);

    int32 StepCallbackCount = 0;
    Sim.OnStep.AddLambda([&StepCallbackCount](float Dt) {
        StepCallbackCount++;
    });

    // Tick by 0.025 seconds (should trigger exactly 2 steps, leaving 0.005 accumulator)
    Sim.Tick(0.025f);
    UTEST_EQUAL("Simulation steps run", Sim.GetStepCount(), 2LL);
    UTEST_EQUAL("Step callbacks triggered", StepCallbackCount, 2);

    // Tick by another 0.005 seconds (should trigger 1 step, total 3)
    Sim.Tick(0.005f);
    UTEST_EQUAL("Simulation steps run total", Sim.GetStepCount(), 3LL);
    UTEST_EQUAL("Step callbacks triggered total", StepCallbackCount, 3);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimulationReplayTest, "MANIFOLD.Systems.DeterministicCore.Replay", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSimulationReplayTest::RunTest(const FString& Parameters)
{
    FFixedStepConfig Config;
    Config.FixedDeltaTime = 0.016666f; // 60Hz
    Config.SnapshotInterval = 10;
    Config.bEnableSnapshots = true;

    FFixedStepSimulation Sim(Config);
    Sim.Initialize(1337ULL);

    // Advance to the snapshot interval and capture the deterministic state there.
    Sim.StepMultiple(10);
    Sim.CaptureSnapshot();

    const FSimulationSnapshot* Snap = Sim.GetSnapshotAt(10);
    UTEST_TRUE("Snapshot was captured", Snap != nullptr);
    UTEST_EQUAL("Snapshot step match", Snap->StepCount, 10LL);
    UTEST_EQUAL("Snapshot records the sim's real state hash", Snap->StateHash, Sim.GetStateHash());

    // Advance further; record the forward-simulated hash at the target step.
    Sim.StepMultiple(15); // now at step 25
    const uint64 ExpectedHash = Sim.GetStateHash();

    // REAL replay verification: re-derive the hash from the snapshot to step 25 and
    // confirm it matches the forward run. This actually recomputes and compares.
    UTEST_TRUE("Replay from snapshot reproduces the forward run",
        Sim.VerifyReplay(*Snap, 25, ExpectedHash));

    // Divergence must be CAUGHT, not silently passed: a wrong expected hash fails.
    UTEST_FALSE("A tampered expected hash is rejected",
        Sim.VerifyReplay(*Snap, 25, ExpectedHash ^ 0x1ULL));
    // A wrong target step also diverges.
    UTEST_FALSE("A wrong target step is rejected",
        Sim.VerifyReplay(*Snap, 24, ExpectedHash));

    // The detailed report is populated (WP S2 acceptance type, previously unused).
    const FReplayVerificationResult Report = Sim.VerifyReplayDetailed(*Snap, 25, ExpectedHash);
    UTEST_TRUE("Detailed report passes", Report.bPassed);
    UTEST_EQUAL("Detailed report step count", Report.StepsVerified, 15LL);
    UTEST_EQUAL("Detailed report recomputed hash", Report.ActualHash, ExpectedHash);

    // Serialization round-trip preserves the deterministic state.
    FBufferArchive Archive;
    Archive << Sim;

    FFixedStepSimulation LoadedSim(Config);
    FMemoryReader Reader(Archive);
    Reader << LoadedSim;

    UTEST_EQUAL("Loaded simulation step count match", LoadedSim.GetStepCount(), 25LL);
    UTEST_EQUAL("Loaded simulation time match", LoadedSim.GetSimulationTime(), Sim.GetSimulationTime());
    UTEST_EQUAL("Loaded simulation state hash match", LoadedSim.GetStateHash(), ExpectedHash);

    return true;
}

// Regression (core audit — defensive hardening of the public FFixedStepSimulation type): FixedDeltaTime
// and the replay interval are caller-supplied on a MANIFOLDCORE_API type, and the editor ClampMin metas
// do NOT constrain direct C++/deserialized values. A non-positive FixedDeltaTime made Tick's substep
// loop infinite (and InterpAlpha divide by zero); a degenerate replay interval drove VerifyReplay's loop
// unbounded (with an int64 length underflow in VerifyReplayDetailed's pre-guard subtraction). All are now
// clamped/validated. Reaching each assertion proves the guard holds — the pre-fix code would hang or hit
// UB before it. (Same discipline as the near-INT32_MAX StepBudget guard in the gameplay layer.)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFixedStepDegenerateConfigTest, "MANIFOLD.Systems.DeterministicCore.DegenerateConfigGuarded", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFixedStepDegenerateConfigTest::RunTest(const FString& Parameters)
{
    // (1) A zero FixedDeltaTime must NOT hang Tick (pre-fix: `while (Acc >= 0)` with `Acc -= 0` is
    //     infinite) and must leave a finite interpolation alpha (pre-fix: Acc / 0 = Inf/NaN).
    {
        FFixedStepConfig Config;
        Config.FixedDeltaTime = 0.0f;
        Config.MaxAccumulatedTime = 0.05f;
        FFixedStepSimulation Sim(Config);
        Sim.Initialize(1ULL);
        Sim.Tick(0.05f); // reaching the next line at all proves this terminated
        UTEST_TRUE("zero FixedDeltaTime: Tick terminates, steps a bounded amount",
            Sim.GetStepCount() > 0 && Sim.GetStepCount() < 100000);
        UTEST_TRUE("zero FixedDeltaTime: interpolation alpha stays finite",
            FMath::IsFinite(Sim.GetInterpolationAlpha()));
    }
    // (2) A negative FixedDeltaTime is likewise clamped (pre-fix: Accumulator grows, loop never ends).
    {
        FFixedStepConfig Config;
        Config.FixedDeltaTime = -1.0f;
        Config.MaxAccumulatedTime = 0.05f;
        FFixedStepSimulation Sim(Config);
        Sim.Initialize(2ULL);
        Sim.Tick(0.05f);
        UTEST_TRUE("negative FixedDeltaTime: Tick terminates",
            Sim.GetStepCount() >= 0 && Sim.GetStepCount() < 100000);
    }
    // (3) A degenerate replay interval must be rejected WITHOUT driving an unbounded loop or underflowing.
    {
        FFixedStepConfig Config;
        FFixedStepSimulation Sim(Config);
        Sim.Initialize(3ULL);

        FSimulationSnapshot Snap; // StepCount defaults to 0
        // A near-INT64_MAX target would be ~9.2e18 iterations pre-fix; the cap rejects it immediately.
        UTEST_FALSE("huge replay interval is rejected (no unbounded loop)",
            Sim.VerifyReplay(Snap, MAX_int64, 0ULL));

        // A target BEFORE the snapshot: rejected, and no int64 underflow in the detailed report's
        // StepsVerified (pre-fix it was computed as TargetStep - StepCount before the guard).
        FSimulationSnapshot Snap5; Snap5.StepCount = 5;
        const FReplayVerificationResult R = Sim.VerifyReplayDetailed(Snap5, 0, 0ULL);
        UTEST_FALSE("target before snapshot is rejected", R.bPassed);
        UTEST_EQUAL("rejected detailed report reports zero steps (no underflow)", R.StepsVerified, 0LL);

        // The over-cap interval is likewise rejected via the detailed report.
        const FReplayVerificationResult R2 = Sim.VerifyReplayDetailed(Snap, MAX_int64, 0ULL);
        UTEST_FALSE("over-cap interval is rejected in the detailed report", R2.bPassed);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLazyRealizationDeterministicDetailTest, "MANIFOLD.LazyRealization.DeterministicDetail", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FLazyRealizationDeterministicDetailTest::RunTest(const FString& Parameters)
{
    ULazyRealizationManager* Manager = NewObject<ULazyRealizationManager>();
    Manager->Initialize(54321ULL, 128, 100.0f);

    FVector QueryLoc(150.0f, 250.0f, 350.0f);
    TSharedPtr<FLazyRegionData> Data1 = Manager->GetRegionData(QueryLoc);
    TSharedPtr<FLazyRegionData> Data2 = Manager->GetRegionData(QueryLoc);

    UTEST_TRUE("Region generation succeeded", Data1.IsValid());
    UTEST_TRUE("Cache hit succeeded", Data2.IsValid());
    UTEST_EQUAL("Identical reference in cache", Data1, Data2);
    UTEST_EQUAL("Grid coordinates match", Data1->Coordinate, FIntVector(1, 2, 3));

    // Clear cache to force regeneration, check if it's identical
    Manager->ClearCache();
    TSharedPtr<FLazyRegionData> Data3 = Manager->GetRegionData(QueryLoc);
    UTEST_TRUE("Regeneration succeeded", Data3.IsValid());
    UTEST_EQUAL("Detail points count match", Data1->DetailPoints.Num(), Data3->DetailPoints.Num());
    
    for (int32 i = 0; i < Data1->DetailPoints.Num(); ++i)
    {
        UTEST_TRUE("Detail point coordinate match", Data1->DetailPoints[i].Equals(Data3->DetailPoints[i]));
    }
    for (int32 i = 0; i < Data1->DensityMap.Num(); ++i)
    {
        UTEST_EQUAL("Density value match", Data1->DensityMap[i], Data3->DensityMap[i]);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLazyRealizationMemoryBoundedTest, "MANIFOLD.LazyRealization.MemoryBounded", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FLazyRealizationMemoryBoundedTest::RunTest(const FString& Parameters)
{
    ULazyRealizationManager* Manager = NewObject<ULazyRealizationManager>();
    Manager->Initialize(54321ULL, 5, 100.0f); // Max 5 regions cached

    // Query 6 distinct regions
    for (int32 i = 0; i < 6; ++i)
    {
        Manager->GetRegionData(FVector(i * 100.0f + 50.0f, 0.0f, 0.0f));
    }

    UTEST_EQUAL("Cache count bounded at MaxCacheSize", Manager->GetCacheCount(), 5);

    // Oldest cell (0, 0, 0) should have been evicted
    // If we request it again, it will regenerate deterministically
    TSharedPtr<FLazyRegionData> ReconstructedData = Manager->GetRegionData(FVector(50.0f, 0.0f, 0.0f));
    UTEST_TRUE("Recreated evicted cell", ReconstructedData.IsValid());
    UTEST_EQUAL("Coordinate match", ReconstructedData->Coordinate, FIntVector(0, 0, 0));
    UTEST_EQUAL("Cache count still bounded", Manager->GetCacheCount(), 5);

    return true;
}
