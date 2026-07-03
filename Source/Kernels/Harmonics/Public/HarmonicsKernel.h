// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RealmKernel.h"
#include "DeterministicRNG.h"
#include "HarmonicsKernel.generated.h"

/**
 * A single vibrational mode (standing wave / oscillator).
 */
USTRUCT(BlueprintType)
struct MANIFOLDKERNELSHARMONICS_API FHarmonicMode
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double Frequency = 1.0; // Hz

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double Amplitude = 1.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double Phase = 0.0; // radians

    FHarmonicMode() = default;

    friend FArchive& operator<<(FArchive& Ar, FHarmonicMode& M)
    {
        Ar << M.Id;
        Ar << M.Frequency;
        Ar << M.Amplitude;
        Ar << M.Phase;
        return Ar;
    }
};

/**
 * Detected integer frequency ratio between two modes — the Harmonics realm's
 * exposed "structure" (the analogue of orbital resonance).
 */
USTRUCT(BlueprintType)
struct MANIFOLDKERNELSHARMONICS_API FHarmonicRatioMatch
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FIntPoint Ratio = FIntPoint(1, 1); // p:q, p >= q (outer:inner convention)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double ActualRatio = 1.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double Deviation = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double Strength = 0.0;

    FHarmonicRatioMatch() = default;

    friend FArchive& operator<<(FArchive& Ar, FHarmonicRatioMatch& R)
    {
        Ar << R.Ratio;
        Ar << R.ActualRatio;
        Ar << R.Deviation;
        Ar << R.Strength;
        return Ar;
    }
};

/**
 * Harmonics kernel state — serializable.
 */
USTRUCT()
struct MANIFOLDKERNELSHARMONICS_API FHarmonicsState : public FRealmState
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FHarmonicMode> Modes;

    UPROPERTY()
    TArray<FHarmonicRatioMatch> ActiveRatios;

    FHarmonicsState() : FRealmState(TEXT("Harmonics"), 1, 0) {}

    friend FArchive& operator<<(FArchive& Ar, FHarmonicsState& S)
    {
        Ar << S.RealmId;
        Ar << S.SimulationVersion;
        Ar << S.Seed;
        Ar << S.SimulationTime;
        Ar << S.StepCount;
        Ar << S.StateHash;
        Ar << S.BinaryData;
        Ar << S.Modes;
        Ar << S.ActiveRatios;
        return Ar;
    }
};

/**
 * Harmonics Kernel — coupled oscillators / standing waves whose exposed structure
 * is the set of small-integer frequency ratios between modes. Third realm proving
 * the realm production template scales (Build Plan §9). Deterministic fixed-step.
 */
UCLASS(BlueprintType, meta = (ImplementedByInterface = "IRealmKernel"))
class MANIFOLDKERNELSHARMONICS_API UHarmonicsKernel : public UObject, public IRealmKernel
{
    GENERATED_BODY()

public:
    UHarmonicsKernel();

    virtual FName GetRealmId() const override { return TEXT("Harmonics"); }
    virtual FText GetDisplayName() const override { return NSLOCTEXT("Harmonics", "DisplayName", "Harmonics"); }
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

    /** Add a vibrational mode; returns its id. */
    FGuid AddMode(double Frequency, double Amplitude);

    /** Small-integer frequency ratios currently present between modes. */
    const TArray<FHarmonicRatioMatch>& GetActiveRatios() const;

    /** Detect small-integer frequency ratios between all mode pairs. */
    void DetectHarmonicRatios(double MaxDeviation = 0.01, int32 MaxRatio = 10);

protected:
    TSharedPtr<FHarmonicsState> HState;
    double SimTime = 0.0;
    int64 StepCount = 0;
    FDeterministicRNG RNG;

    void UpdateDerivedState();
};
