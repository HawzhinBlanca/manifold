// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RealmKernel.h"
#include "DeterministicRNG.h"
#include "CircuitsKernel.generated.h"

/**
 * A resonant LC tank: charge oscillates sinusoidally at its resonant frequency
 * f = 1/(2*pi*sqrt(L*C)). Two tanks whose resonant frequencies form a small-integer
 * ratio P:Q are the same abstract structure as an orbital P:Q resonance or a P:Q
 * harmonic — here in the domain of ELECTROMAGNETISM.
 */
USTRUCT(BlueprintType)
struct MANIFOLDKERNELSCIRCUITS_API FResonantTank
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double Frequency = 1.0; // resonant frequency (Hz, in arbitrary units)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double Amplitude = 1.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double Phase = 0.0; // radians — current oscillation phase of the charge

    FResonantTank() = default;

    friend FArchive& operator<<(FArchive& Ar, FResonantTank& T)
    {
        Ar << T.Frequency;
        Ar << T.Amplitude;
        Ar << T.Phase;
        return Ar;
    }
};

/**
 * A detected small-integer ratio between two tanks' resonant frequencies — the
 * Circuits realm's exposed structure.
 */
USTRUCT(BlueprintType)
struct MANIFOLDKERNELSCIRCUITS_API FCircuitRatioMatch
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FIntPoint Ratio = FIntPoint(1, 1); // p:q, p >= q, reduced by GCD

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double Strength = 1.0;

    FCircuitRatioMatch() = default;

    friend FArchive& operator<<(FArchive& Ar, FCircuitRatioMatch& R)
    {
        Ar << R.Ratio;
        Ar << R.Strength;
        return Ar;
    }
};

/**
 * Circuits kernel state — serializable.
 */
USTRUCT()
struct MANIFOLDKERNELSCIRCUITS_API FCircuitsState : public FRealmState
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FResonantTank> Tanks;

    UPROPERTY()
    TArray<FCircuitRatioMatch> ActiveRatios;

    FCircuitsState() : FRealmState(TEXT("Circuits"), 1, 0) {}

    friend FArchive& operator<<(FArchive& Ar, FCircuitsState& S)
    {
        Ar << S.RealmId;
        Ar << S.SimulationVersion;
        Ar << S.Seed;
        Ar << S.SimulationTime;
        Ar << S.StepCount;
        Ar << S.StateHash;
        Ar << S.BinaryData;
        Ar << S.Tanks;
        Ar << S.ActiveRatios;
        return Ar;
    }
};

/**
 * Circuits Kernel — resonant LC tanks. Seventh realm; exposes the small-integer ratio
 * between tank resonant frequencies via a "CircuitResonance" query. A 3:2 here (two
 * tanks tuned a perfect fifth apart) is the same structure as an orbital 3:2 — the
 * cross-domain analogy in the domain of electromagnetism. Deterministic fixed-step.
 */
UCLASS(BlueprintType, meta = (ImplementedByInterface = "IRealmKernel"))
class MANIFOLDKERNELSCIRCUITS_API UCircuitsKernel : public UObject, public IRealmKernel
{
    GENERATED_BODY()

public:
    UCircuitsKernel();

    virtual FName GetRealmId() const override { return TEXT("Circuits"); }
    virtual FText GetDisplayName() const override { return NSLOCTEXT("Circuits", "DisplayName", "Circuits"); }
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

    /** Add a resonant tank at the given (positive) frequency; returns nothing. */
    void AddTank(double Frequency, double Amplitude = 1.0);

    /** Small-integer ratios currently present between tank resonant frequencies. */
    const TArray<FCircuitRatioMatch>& GetActiveRatios() const;

    /** Detect exact small-integer frequency ratios between all tank pairs (reduced by GCD). */
    void DetectCircuitRatios();

protected:
    TSharedPtr<FCircuitsState> CState;
    double SimTime = 0.0;
    int64 StepCount = 0;
    FDeterministicRNG RNG;

    void UpdateDerivedState();
};
