// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RealmKernel.h"
#include "DeterministicRNG.h"
#include "OrbitsKernel.generated.h"

// Forward declarations
class FOrbitsBody;
struct FOrbitsState;

/**
 * Orbital body definition
 */
USTRUCT(BlueprintType)
struct MANIFOLDKERNELSORBITS_API FOrbitsBodyDef
{
    GENERATED_BODY()

    /** Unique body ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid Id;

    /** Display name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName Name;

    /** Mass (kg) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
    double Mass = 1.0;

    /** Initial position (m) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Position = FVector::ZeroVector;

    /** Initial velocity (m/s) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Velocity = FVector::ZeroVector;

    /** Radius for collision/rendering (m) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
    double Radius = 1.0;

    /** Color for visualization */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor Color = FLinearColor::White;

    /** Whether this body is a central attractor (star) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsCentral = false;

    FOrbitsBodyDef() = default;

    friend FArchive& operator<<(FArchive& Ar, FOrbitsBodyDef& B)
    {
        Ar << B.Id;
        Ar << B.Name;
        Ar << B.Mass;
        Ar << B.Position;
        Ar << B.Velocity;
        Ar << B.Radius;
        Ar << B.Color;
        Ar << B.bIsCentral;
        return Ar;
    }
};

/**
 * Orbital elements (Keplerian) - computed from state
 */
USTRUCT(BlueprintType)
struct MANIFOLDKERNELSORBITS_API FOrbitalElements
{
    GENERATED_BODY()

    /** Semi-major axis (m) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double SemiMajorAxis = 0.0;

    /** Eccentricity (0..1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double Eccentricity = 0.0;

    /** Inclination (radians) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double Inclination = 0.0;

    /** Longitude of ascending node (radians) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double LongitudeOfAscendingNode = 0.0;

    /** Argument of periapsis (radians) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double ArgumentOfPeriapsis = 0.0;

    /** True anomaly (radians) - position in orbit */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double TrueAnomaly = 0.0;

    /** Orbital period (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double Period = 0.0;

    /** Mean motion (rad/s) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double MeanMotion = 0.0;

    FOrbitalElements() = default;

    /** Check if orbit is valid (bound) */
    bool IsValid() const { return SemiMajorAxis > 0 && Eccentricity >= 0 && Eccentricity < 1; }

    /** Check if orbit is circular */
    bool IsCircular(double Tolerance = 1e-6) const { return Eccentricity < Tolerance; }

    friend FArchive& operator<<(FArchive& Ar, FOrbitalElements& E)
    {
        Ar << E.SemiMajorAxis;
        Ar << E.Eccentricity;
        Ar << E.Inclination;
        Ar << E.LongitudeOfAscendingNode;
        Ar << E.ArgumentOfPeriapsis;
        Ar << E.TrueAnomaly;
        Ar << E.Period;
        Ar << E.MeanMotion;
        return Ar;
    }
};

/**
 * Resonance detection result
 */
USTRUCT(BlueprintType)
struct MANIFOLDKERNELSORBITS_API FResonanceMatch
{
    GENERATED_BODY()

    /** First body ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid BodyA;

    /** Second body ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid BodyB;

    /** Resonance ratio (p:q where p*PeriodA ≈ q*PeriodB) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FIntPoint Ratio; // X = p, Y = q

    /** Actual period ratio */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double ActualRatio = 0.0;

    /** Deviation from exact resonance (lower = stronger resonance) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double Deviation = 0.0;

    /** Resonance strength (0..1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double Strength = 0.0;

    /** Stable identity of this resonance, derived deterministically from the body
     *  pair — persists across recomputes so the correspondence engine and the
     *  player can reference the same structure frame to frame. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid Id;

    FResonanceMatch() = default;

    friend FArchive& operator<<(FArchive& Ar, FResonanceMatch& R)
    {
        Ar << R.BodyA;
        Ar << R.BodyB;
        Ar << R.Ratio;
        Ar << R.ActualRatio;
        Ar << R.Deviation;
        Ar << R.Strength;
        Ar << R.Id;
        return Ar;
    }
};

/**
 * Orbits Kernel State - serializable
 */
USTRUCT()
struct MANIFOLDKERNELSORBITS_API FOrbitsState : public FRealmState
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FOrbitsBodyDef> Bodies;

    UPROPERTY()
    TMap<FGuid, FOrbitalElements> CurrentElements;

    UPROPERTY()
    TArray<FResonanceMatch> ActiveResonances;

    UPROPERTY()
    double SystemTotalEnergy = 0.0;

    UPROPERTY()
    double SystemTotalAngularMomentum = 0.0;

    FOrbitsState() : FRealmState(TEXT("Orbits"), 1, 0) {}

    friend FArchive& operator<<(FArchive& Ar, FOrbitsState& StateObj)
    {
        Ar << StateObj.RealmId;
        Ar << StateObj.SimulationVersion;
        Ar << StateObj.Seed;
        Ar << StateObj.SimulationTime;
        Ar << StateObj.StepCount;
        Ar << StateObj.StateHash;
        Ar << StateObj.BinaryData;
        Ar << StateObj.Bodies;
        Ar << StateObj.CurrentElements;
        Ar << StateObj.ActiveResonances;
        Ar << StateObj.SystemTotalEnergy;
        Ar << StateObj.SystemTotalAngularMomentum;
        return Ar;
    }
};

/**
 * Orbits Kernel - Keplerian/n-body simulation with resonance detection
 * 
 * Implements IRealmKernel for the Orbits realm.
 * Supports: central star + planets/moons, n-body perturbations, resonance queries.
 */
UCLASS(BlueprintType, meta = (ImplementedByInterface = "IRealmKernel"))
class MANIFOLDKERNELSORBITS_API UOrbitsKernel : public UObject, public IRealmKernel
{
    GENERATED_BODY()

public:
    UOrbitsKernel();

    // =====================================================================
    // IRealmKernel INTERFACE
    // =====================================================================

    virtual FName GetRealmId() const override { return TEXT("Orbits"); }
    virtual FText GetDisplayName() const override { return NSLOCTEXT("Orbits", "DisplayName", "Orbits"); }
    virtual uint32 GetSimulationVersion() const override { return 1; }

    virtual void Initialize(uint64 Seed) override;
    virtual void Reset() override;
    virtual void Shutdown() override;

    virtual void Step(float DeltaTime) override;
    virtual void StepMultiple(float DeltaTime, int32 NumSteps) override;

    virtual float GetSimulationTime() const override { return SimTime; }
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

    // =====================================================================
    // ORBITS-SPECIFIC API
    // =====================================================================

    /** Add a body to the simulation */
    FGuid AddBody(const FOrbitsBodyDef& Def);

    /** Remove a body */
    bool RemoveBody(FGuid BodyId);

    /** Get body definition */
    const FOrbitsBodyDef* GetBody(FGuid BodyId) const;

    /** Get current orbital elements for a body */
    bool GetOrbitalElements(FGuid BodyId, FOrbitalElements& OutElements) const;

    /** Get all active resonances */
    const TArray<FResonanceMatch>& GetActiveResonances() const;

    /** Detect resonances between all body pairs */
    void DetectResonances(double MaxDeviation = 0.01, int32 MaxRatio = 10);

    /** Compute orbital elements from current state vectors */
    void RecomputeOrbitalElements();

    // =====================================================================
    // CONFIGURATION
    // =====================================================================

    /** Gravitational constant (m^3 kg^-1 s^-2) */
    double G = 6.67430e-11;

    /** Softening factor for close encounters (m) */
    double Softening = 1000.0; // 1 km

    /** Use full n-body (true) or Keplerian with perturbations (false) */
    bool bFullNBody = true;

    /** Maximum integration sub-steps per frame */
    int32 MaxSubSteps = 4;

protected:
    // =====================================================================
    // INTERNAL STATE
    // =====================================================================

    TSharedPtr<FOrbitsState> OrbitState;
    double SimTime = 0.0;
    int64 StepCount = 0;
    FDeterministicRNG RNG;

    // Integration arrays synced with OrbitState->Bodies
    TArray<FVector> Positions;
    TArray<FVector> Velocities;
    TArray<FVector> Accelerations;
    TArray<double> Masses;
    TArray<FGuid> BodyIds;

    // =====================================================================
    // INTERNAL METHODS
    // =====================================================================

    void ComputeAccelerations();
    void IntegrateVelocityVerlet(double DeltaTime);
    void UpdateOrbitalElements(int32 BodyIndex);
    double ComputeResonanceStrength(const FOrbitalElements& A, const FOrbitalElements& B, int32 p, int32 q) const;

    /** Recompute elements, resonances, and system totals (energy/angular momentum/hash).
     *  Called after every Step AND right after bodies are added, so the initial
     *  SystemTotalEnergy baseline is valid before the first Step. */
    void UpdateDerivedState();
};