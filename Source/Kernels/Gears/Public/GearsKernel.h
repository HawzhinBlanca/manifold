// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RealmKernel.h"
#include "DeterministicRNG.h"
#include "GearsKernel.generated.h"

/**
 * A gear in a meshed train: an integer number of teeth and a current angle. Two
 * meshed gears turn at angular speeds in the inverse ratio of their teeth, so a pair
 * with P and Q teeth exposes the exact integer ratio P:Q — the same abstract
 * structure as an orbital resonance, in the domain of MECHANISM.
 */
USTRUCT(BlueprintType)
struct MANIFOLDKERNELSGEARS_API FGear
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Teeth = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double Angle = 0.0; // radians

    FGear() = default;

    friend FArchive& operator<<(FArchive& Ar, FGear& G)
    {
        Ar << G.Id;
        Ar << G.Teeth;
        Ar << G.Angle;
        return Ar;
    }
};

/**
 * A detected exact integer gear ratio — the Gears realm's exposed structure.
 */
USTRUCT(BlueprintType)
struct MANIFOLDKERNELSGEARS_API FGearRatioMatch
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FIntPoint Ratio = FIntPoint(1, 1); // p:q, p >= q, reduced by GCD

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double Strength = 1.0;

    FGearRatioMatch() = default;

    friend FArchive& operator<<(FArchive& Ar, FGearRatioMatch& R)
    {
        Ar << R.Ratio;
        Ar << R.Strength;
        return Ar;
    }
};

/**
 * Gears kernel state — serializable.
 */
USTRUCT()
struct MANIFOLDKERNELSGEARS_API FGearsState : public FRealmState
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FGear> Gears;

    UPROPERTY()
    TArray<FGearRatioMatch> ActiveRatios;

    FGearsState() : FRealmState(TEXT("Gears"), 1, 0) {}

    friend FArchive& operator<<(FArchive& Ar, FGearsState& S)
    {
        Ar << S.RealmId;
        Ar << S.SimulationVersion;
        Ar << S.Seed;
        Ar << S.SimulationTime;
        Ar << S.StepCount;
        Ar << S.StateHash;
        Ar << S.BinaryData;
        Ar << S.Gears;
        Ar << S.ActiveRatios;
        return Ar;
    }
};

/**
 * Gears Kernel — a meshed gear train. Sixth realm; exposes the EXACT integer ratio
 * between gear tooth counts via a "GearRatio" query (no floating tolerance — gear
 * ratios are integers). Deterministic fixed-step.
 */
UCLASS(BlueprintType, meta = (ImplementedByInterface = "IRealmKernel"))
class MANIFOLDKERNELSGEARS_API UGearsKernel : public UObject, public IRealmKernel
{
    GENERATED_BODY()

public:
    UGearsKernel();

    virtual FName GetRealmId() const override { return TEXT("Gears"); }
    virtual FText GetDisplayName() const override { return NSLOCTEXT("Gears", "DisplayName", "Gears"); }
    virtual uint32 GetSimulationVersion() const override { return 1; }

    virtual void Initialize(uint64 Seed) override;
    virtual void Reset() override;
    virtual void Shutdown() override;

    virtual void Step(float DeltaTime) override;

    virtual float GetSimulationTime() const override { return static_cast<float>(SimTime); }
    virtual int64 GetStepCount() const override { return StepCount; }

    virtual TSharedPtr<FRealmState> GetState() const override;
    virtual void SetState(const FRealmState& NewState) override;
    virtual void SerializeState(FArchive& Ar) override;
    virtual void DeserializeState(FArchive& Ar) override;
    virtual uint64 ComputeStateHash() const override;

    virtual bool Query(const FRealmQuery& Query, FRealmQueryResult& Result) const override;
    virtual void QueryAll(const FRealmQuery& QueryDesc, TArray<FRealmQueryResult>& OutResults) const override;
    virtual TArray<FName> GetSupportedQueryTypes() const override;

    virtual TMap<FName, FString> GetParameterSchema() const override;
    virtual bool SetParameter(FName Name, const FString& Value) override;
    virtual bool GetParameter(FName Name, FString& OutValue) const override;

    virtual bool VerifyDeterminism(int32 NumSteps = 1000) const override;

    /** Add a gear with the given (positive) tooth count; returns its id. */
    FGuid AddGear(int32 Teeth);

    /** Exact integer ratios currently present between gear pairs. */
    const TArray<FGearRatioMatch>& GetActiveRatios() const;

    /** Detect exact tooth-count ratios between all gear pairs (reduced by GCD). */
    void DetectGearRatios();

protected:
    TSharedPtr<FGearsState> GState;
    double SimTime = 0.0;
    int64 StepCount = 0;
    FDeterministicRNG RNG;

    void UpdateDerivedState();
};
