// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RealmKernel.h"
#include "DeterministicRNG.h"
#include "FluidsKernel.generated.h"

/**
 * Fluid vortex definition
 */
USTRUCT(BlueprintType)
struct MANIFOLDKERNELSFLUIDS_API FFluidVortex
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Position = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Strength = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Radius = 1.0f;

    FFluidVortex() = default;

    friend FArchive& operator<<(FArchive& Ar, FFluidVortex& V)
    {
        Ar << V.Id;
        Ar << V.Position;
        Ar << V.Strength;
        Ar << V.Radius;
        return Ar;
    }
};

/**
 * Fluids Kernel State
 */
USTRUCT()
struct MANIFOLDKERNELSFLUIDS_API FFluidsState : public FRealmState
{
    GENERATED_BODY()

    UPROPERTY()
    int32 GridSize = 32;

    UPROPERTY()
    TArray<float> Density;

    UPROPERTY()
    TArray<float> VelocityX;

    UPROPERTY()
    TArray<float> VelocityY;

    UPROPERTY()
    TArray<FFluidVortex> ActiveVortices;

    // --- Solver configuration, mirrored into the serialized state so save / replay round-trips it.
    //     These are the live UFluidsKernel config members; they drive the solver every step
    //     (Viscosity/Diffusion set the implicit-diffuse coefficients, Decay scales density), so a
    //     state that reverts to default config after loading a non-default-config save would diverge
    //     on the next step. Populated from the kernel at GetState()/SerializeState() and restored
    //     (sanitized — negative diffusion coefficients are the documented NaN->OOB hazard) at
    //     SetState(); folded into ComputeStateHash for the same reason the velocity grids are. ---
    UPROPERTY()
    float Viscosity = 0.0001f;

    UPROPERTY()
    float Diffusion = 0.0001f;

    UPROPERTY()
    float Decay = 0.995f;

    FFluidsState() : FRealmState(TEXT("Fluids"), 1, 0) {}

    friend FArchive& operator<<(FArchive& Ar, FFluidsState& StateObj)
    {
        Ar << StateObj.RealmId;
        Ar << StateObj.SimulationVersion;
        Ar << StateObj.Seed;
        Ar << StateObj.SimulationTime;
        Ar << StateObj.StepCount;
        Ar << StateObj.StateHash;
        Ar << StateObj.BinaryData;
        Ar << StateObj.GridSize;
        Ar << StateObj.Density;
        Ar << StateObj.VelocityX;
        Ar << StateObj.VelocityY;
        Ar << StateObj.ActiveVortices;
        // Appended after the pre-existing fields (append-only). No persisted MANIFOLD saves/replays
        // embed realm state, so this format change only affects the in-memory round-trip path.
        Ar << StateObj.Viscosity;
        Ar << StateObj.Diffusion;
        Ar << StateObj.Decay;
        return Ar;
    }
};

/**
 * Fluids Kernel - Stam Stable Fluids solver in 2D with Vortex tracking.
 * Implements IRealmKernel.
 */
UCLASS(BlueprintType, meta = (ImplementedByInterface = "IRealmKernel"))
class MANIFOLDKERNELSFLUIDS_API UFluidsKernel : public UObject, public IRealmKernel
{
    GENERATED_BODY()

public:
    UFluidsKernel();

    // =====================================================================
    // IRealmKernel INTERFACE
    // =====================================================================

    virtual FName GetRealmId() const override { return TEXT("Fluids"); }
    virtual FText GetDisplayName() const override { return NSLOCTEXT("Fluids", "DisplayName", "Fluids"); }
    virtual uint32 GetSimulationVersion() const override { return 1; }

    virtual void Initialize(uint64 Seed) override;
    virtual void Reset() override;
    virtual void Shutdown() override;

    virtual void Step(float DeltaTime) override;

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
    // FLUIDS-SPECIFIC API
    // =====================================================================

    /** Add density source at normalized coords (0..1) */
    void AddDensity(float X, float Y, float Amount);

    /** Add force/velocity source at normalized coords (0..1) */
    void AddVelocity(float X, float Y, float AmountX, float AmountY);

    /** Get density at normalized coordinates */
    float GetDensityAt(float X, float Y) const;

    /** Get velocity at normalized coordinates */
    FVector GetVelocityAt(float X, float Y) const;

    /** Get active vortices */
    const TArray<FFluidVortex>& GetActiveVortices() const;

    // =====================================================================
    // CONFIGURATION
    // =====================================================================

    /** Viscosity of fluid */
    float Viscosity = 0.0001f;

    /** Diffusion of density */
    float Diffusion = 0.0001f;

    /** Density decay multiplier per step */
    float Decay = 0.995f;

    /** Size of grid (width/height) */
    int32 Size = 32;

    /** Upper bound on a deserialized GridSize. GridSize and the grid arrays are serialized
     *  independently, so SetState validates/clamps an untrusted GridSize to this range — a hostile
     *  or corrupt value must not drive out-of-bounds indexing or overflow the int32 (Size+2)^2. */
    static constexpr int32 MaxGridSize = 512;

protected:
    // =====================================================================
    // INTERNAL STATE
    // =====================================================================

    TSharedPtr<FFluidsState> FluidsState;
    double SimTime = 0.0;
    int64 StepCount = 0;
    FDeterministicRNG RNG;

    // Grid representation: (Size+2) * (Size+2) elements
    // Cells 1..Size are simulation; 0 and Size+1 are boundary cells
    TArray<float> u;       // velocity X
    TArray<float> v;       // velocity Y
    TArray<float> u_prev;  // previous velocity X
    TArray<float> v_prev;  // previous velocity Y
    TArray<float> d;       // density
    TArray<float> d_prev;  // previous density

    // =====================================================================
    // SOLVER SUBSTEPS (Stam's Stable Fluids)
    // =====================================================================

    void AddSource(TArray<float>& x, const TArray<float>& s, float dt);
    void Diffuse(int32 b, TArray<float>& x, const TArray<float>& x0, float diff, float dt);
    void Advect(int32 b, TArray<float>& d_arr, const TArray<float>& d0, const TArray<float>& u_arr, const TArray<float>& v_arr, float dt);
    void Project(TArray<float>& u_arr, TArray<float>& v_arr, TArray<float>& p, TArray<float>& div);
    void SetBoundary(int32 b, TArray<float>& x);

    void TickFluid(float dt);
    void DetectVortices();

    FORCEINLINE int32 IX(int32 i, int32 j) const
    {
        return i + (Size + 2) * j;
    }
};
