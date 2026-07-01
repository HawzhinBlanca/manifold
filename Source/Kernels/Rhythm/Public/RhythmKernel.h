// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RealmKernel.h"
#include "DeterministicRNG.h"
#include "RhythmKernel.generated.h"

/**
 * A rhythmic voice: a pulse train at a fixed tempo (pulses per bar). Three voices
 * at 3 and 2 pulses form a 3:2 polyrhythm — the same abstract structure as an
 * orbital 3:2 resonance, but in the domain of TIME.
 */
USTRUCT(BlueprintType)
struct MANIFOLDKERNELSRHYTHM_API FRhythmVoice
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double Tempo = 1.0; // pulses per bar

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double Phase = 0.0; // fraction of a bar in [0,1)

    FRhythmVoice() = default;

    friend FArchive& operator<<(FArchive& Ar, FRhythmVoice& V)
    {
        Ar << V.Id;
        Ar << V.Tempo;
        Ar << V.Phase;
        return Ar;
    }
};

/**
 * A detected small-integer polyrhythm between two voices — the Rhythm realm's
 * exposed structure (analogue of orbital resonance / harmonic ratio).
 */
USTRUCT(BlueprintType)
struct MANIFOLDKERNELSRHYTHM_API FRhythmRatioMatch
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FIntPoint Ratio = FIntPoint(1, 1); // p:q, p >= q

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double ActualRatio = 1.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double Deviation = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double Strength = 0.0;

    FRhythmRatioMatch() = default;

    friend FArchive& operator<<(FArchive& Ar, FRhythmRatioMatch& R)
    {
        Ar << R.Ratio;
        Ar << R.ActualRatio;
        Ar << R.Deviation;
        Ar << R.Strength;
        return Ar;
    }
};

/**
 * Rhythm kernel state — serializable.
 */
USTRUCT()
struct MANIFOLDKERNELSRHYTHM_API FRhythmState : public FRealmState
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FRhythmVoice> Voices;

    UPROPERTY()
    TArray<FRhythmRatioMatch> ActiveRatios;

    FRhythmState() : FRealmState(TEXT("Rhythm"), 1, 0) {}

    friend FArchive& operator<<(FArchive& Ar, FRhythmState& S)
    {
        Ar << S.RealmId;
        Ar << S.SimulationVersion;
        Ar << S.Seed;
        Ar << S.SimulationTime;
        Ar << S.StepCount;
        Ar << S.StateHash;
        Ar << S.BinaryData;
        Ar << S.Voices;
        Ar << S.ActiveRatios;
        return Ar;
    }
};

/**
 * Rhythm Kernel — polyrhythms. Fifth realm; exposes the small-integer ratio between
 * pulse-train tempos via a "RhythmRatio" query. A 3:2 here (three-against-two) is
 * the same structure as an orbital 3:2 — the cross-domain analogy in the domain of
 * time. Deterministic fixed-step; adding it took only the template + registration.
 */
UCLASS(BlueprintType, meta = (ImplementedByInterface = "IRealmKernel"))
class MANIFOLDKERNELSRHYTHM_API URhythmKernel : public UObject, public IRealmKernel
{
    GENERATED_BODY()

public:
    URhythmKernel();

    virtual FName GetRealmId() const override { return TEXT("Rhythm"); }
    virtual FText GetDisplayName() const override { return NSLOCTEXT("Rhythm", "DisplayName", "Rhythm"); }
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
    virtual void QueryAll(const FRealmQuery& QueryDesc, TArray<FRealmQueryResult>& OutResults) const override;
    virtual TArray<FName> GetSupportedQueryTypes() const override;

    virtual TMap<FName, FString> GetParameterSchema() const override;
    virtual bool SetParameter(FName Name, const FString& Value) override;
    virtual bool GetParameter(FName Name, FString& OutValue) const override;

    virtual bool VerifyDeterminism(int32 NumSteps = 1000) const override;

    /** Add a rhythmic voice at the given tempo (pulses per bar); returns its id. */
    FGuid AddVoice(double Tempo);

    /** Small-integer polyrhythms currently present between voices. */
    const TArray<FRhythmRatioMatch>& GetActiveRatios() const;

    /** Detect small-integer tempo ratios between all voice pairs. */
    void DetectRhythmRatios(double MaxDeviation = 0.01, int32 MaxRatio = 10);

protected:
    TSharedPtr<FRhythmState> RState;
    double SimTime = 0.0;
    int64 StepCount = 0;
    FDeterministicRNG RNG;

    void UpdateDerivedState();
};
