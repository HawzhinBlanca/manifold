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

    // Step 10 times to hit the snapshot interval
    Sim.StepMultiple(10);
    
    // Capture snapshot
    TArray<uint8> MockState = { 1, 2, 3, 4, 5 };
    uint64 CombinedHash = 0xAA55AA55ULL;
    Sim.CaptureSnapshot(MockState, CombinedHash);

    const FSimulationSnapshot* Snap = Sim.GetSnapshotAt(10);
    UTEST_TRUE("Snapshot was captured", Snap != nullptr);
    UTEST_EQUAL("Snapshot step match", Snap->StepCount, 10LL);
    UTEST_EQUAL("Snapshot hash match", Snap->StateHash, CombinedHash);
    UTEST_EQUAL("Snapshot state data match", Snap->StateData, MockState);

    // Save simulation state to archive
    FBufferArchive Archive;
    Archive << Sim;

    // Restore into new simulation
    FFixedStepSimulation LoadedSim(Config);
    FMemoryReader Reader(Archive);
    Reader << LoadedSim;

    UTEST_EQUAL("Loaded simulation step count match", LoadedSim.GetStepCount(), 10LL);
    UTEST_EQUAL("Loaded simulation time match", LoadedSim.GetSimulationTime(), Sim.GetSimulationTime());

    const FSimulationSnapshot* LoadedSnap = LoadedSim.GetSnapshotAt(10);
    UTEST_TRUE("Loaded snapshot exists", LoadedSnap != nullptr);
    UTEST_EQUAL("Loaded snapshot hash match", LoadedSnap->StateHash, CombinedHash);

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
