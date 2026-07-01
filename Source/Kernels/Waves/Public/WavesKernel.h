// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RealmKernel.h"
#include "DeterministicRNG.h"
#include "WavesKernel.generated.h"

/**
 * A standing wave on a string: the n-th harmonic has frequency n * Fundamental,
 * with (n+1) nodes along the string. A different physical domain from Harmonics
 * (spatial standing waves vs. point oscillators) that exposes the SAME abstract
 * structure — an integer ratio.
 */
USTRUCT(BlueprintType)
struct MANIFOLDKERNELSWAVES_API FStandingWave
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 HarmonicNumber = 1; // n: frequency = n * Fundamental

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double Amplitude = 1.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double Phase = 0.0; // radians

    FStandingWave() = default;

    friend FArchive& operator<<(FArchive& Ar, FStandingWave& W)
    {
        Ar << W.HarmonicNumber;
        Ar << W.Amplitude;
        Ar << W.Phase;
        return Ar;
    }
};

USTRUCT(BlueprintType)
struct MANIFOLDKERNELSWAVES_API FWaveRatioMatch
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FIntPoint Ratio = FIntPoint(1, 1); // p:q, p >= q

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double Strength = 0.0;

    FWaveRatioMatch() = default;

    friend FArchive& operator<<(FArchive& Ar, FWaveRatioMatch& R)
    {
        Ar << R.Ratio;
        Ar << R.Strength;
        return Ar;
    }
};

USTRUCT()
struct MANIFOLDKERNELSWAVES_API FWavesState : public FRealmState
{
    GENERATED_BODY()

    UPROPERTY()
    double Fundamental = 1.0; // Hz

    UPROPERTY()
    TArray<FStandingWave> Waves;

    UPROPERTY()
    TArray<FWaveRatioMatch> ActiveRatios;

    FWavesState() : FRealmState(TEXT("Waves"), 1, 0) {}

    friend FArchive& operator<<(FArchive& Ar, FWavesState& S)
    {
        Ar << S.RealmId;
        Ar << S.SimulationVersion;
        Ar << S.Seed;
        Ar << S.SimulationTime;
        Ar << S.StepCount;
        Ar << S.StateHash;
        Ar << S.BinaryData;
        Ar << S.Fundamental;
        Ar << S.Waves;
        Ar << S.ActiveRatios;
        return Ar;
    }
};

/**
 * Waves Kernel — standing waves on a string. Fourth realm; exposes the integer
 * ratio between excited harmonics via a "WaveHarmonic" query. Deterministic.
 */
UCLASS(BlueprintType, meta = (ImplementedByInterface = "IRealmKernel"))
class MANIFOLDKERNELSWAVES_API UWavesKernel : public UObject, public IRealmKernel
{
    GENERATED_BODY()

public:
    UWavesKernel();

    virtual FName GetRealmId() const override { return TEXT("Waves"); }
    virtual FText GetDisplayName() const override { return NSLOCTEXT("Waves", "DisplayName", "Waves"); }
    virtual uint32 GetSimulationVersion() const override { return 1; }

    virtual void Initialize(uint64 Seed) override;
    virtual void Reset() override;
    virtual void Shutdown() override;

    virtual void Step(float DeltaTime) override;
    virtual void StepMultiple(float DeltaTime, int32 NumSteps) override;

    virtual float GetSimulationTime() const override { return static_cast<float>(SimTime); }
    virtual int64 GetStepCount() const override { return StepCount; }

    virtual TSharedPtr<FRealmState> GetState() const override;
    virtual void SetState(const FRealmState& NewState) override;
    virtual void SerializeState(FArchive& Ar) override;
    virtual void DeserializeState(FArchive& Ar) override;
    virtual uint64 ComputeStateHash() const override;

    virtual bool Query(const FRealmQuery& Query, FRealmQueryResult& Result) const override;
    virtual TArray<FName> GetSupportedQueryTypes() const override;

    virtual TMap<FName, FString> GetParameterSchema() const override;
    virtual bool SetParameter(FName Name, const FString& Value) override;
    virtual bool GetParameter(FName Name, FString& OutValue) const override;

    virtual bool VerifyDeterminism(int32 NumSteps = 1000) const override;

    /** Excite the n-th harmonic (n nodes). Returns nothing; recomputes structure. */
    void ExciteHarmonic(int32 HarmonicNumber, double Amplitude);

    const TArray<FWaveRatioMatch>& GetActiveRatios() const;
    void DetectWaveRatios();

    UPROPERTY()
    double Fundamental = 220.0; // Hz (A3), just a scale

protected:
    TSharedPtr<FWavesState> WState;
    double SimTime = 0.0;
    int64 StepCount = 0;
    FDeterministicRNG RNG;

    void UpdateDerivedState();
};
