// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Math/Vector.h"
#include "Math/Quat.h"
#include "Misc/Guid.h"
#include "Serialization/Archive.h"
#include "RealmKernel.generated.h"

// Forward declarations
struct FRealmState;
struct FRealmQuery;
struct FRealmQueryResult;

/**
 * RealmKernel Interface (WP S1)
 * 
 * Every realm simulation (Orbits, Fluids, etc.) implements this interface.
 * The interface is deterministic: same seed + same inputs = same outputs.
 * 
 * Lifecycle: Create → Initialize(seed) → Step(dt) × N → Query → Serialize
 * Replay: Deserialize → Step(dt) × N → state matches original at each step
 */
UINTERFACE(MinimalAPI, BlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class URealmKernel : public UInterface
{
    GENERATED_BODY()
};

/**
 * IRealmKernel
 * 
 * @note All methods must be thread-safe for read-only queries during Step.
 * @note State serialization must be bitwise-deterministic across platforms.
 */
class MANIFOLDCORE_API IRealmKernel
{
    GENERATED_BODY()

public:
    virtual ~IRealmKernel() = default;

    // =====================================================================
    // IDENTITY & METADATA
    // =====================================================================

    /** Unique realm identifier (e.g., "Orbits", "Fluids", "Plasma") */
    virtual FName GetRealmId() const = 0;

    /** Human-readable name for UI */
    virtual FText GetDisplayName() const = 0;

    /** Version of this kernel's simulation format (for replay compatibility) */
    virtual uint32 GetSimulationVersion() const = 0;

    // =====================================================================
    // LIFECYCLE
    // =====================================================================

    /**
     * Initialize the kernel with a deterministic seed.
     * Must be called before any other method.
     * @param Seed 64-bit seed - same seed + same inputs = identical simulation
     */
    virtual void Initialize(uint64 Seed) = 0;

    /** Reset to post-Initialize state (same seed) */
    virtual void Reset() = 0;

    /** Clean up resources */
    virtual void Shutdown() = 0;

    // =====================================================================
    // SIMULATION STEP (DETERMINISTIC FIXED-STEP)
    // =====================================================================

    /**
     * Advance simulation by one fixed time step.
     * @param DeltaTime Fixed time step in seconds (e.g., 1/60.0)
     * @note Must be deterministic: same DeltaTime + same state = same next state
     */
    virtual void Step(float DeltaTime) = 0;

    /**
     * Advance simulation by multiple steps (optimization for batch processing).
     * Default implementation loops Step(); kernels may override for efficiency.
     */
    virtual void StepMultiple(float DeltaTime, int32 NumSteps)
    {
        for (int32 i = 0; i < NumSteps; ++i)
        {
            Step(DeltaTime);
        }
    }

    /** Current simulation time (sum of DeltaTimes since Initialize) */
    virtual float GetSimulationTime() const = 0;

    /** Current step count since Initialize */
    virtual int64 GetStepCount() const = 0;

    // =====================================================================
    // STATE MANAGEMENT (for replay, save/load, correspondence)
    // =====================================================================

    /** Opaque state handle for external references */
    using FStateHandle = FGuid;

    /** Get current full state (for serialization, correspondence mapping) */
    virtual TSharedPtr<FRealmState> GetState() const = 0;

    /** Set state (for replay, transport mechanic) */
    virtual void SetState(const FRealmState& NewState) = 0;

    /** Serialize state to binary archive (deterministic byte output) */
    virtual void SerializeState(FArchive& Ar) = 0;

    /** Deserialize state from binary archive */
    virtual void DeserializeState(FArchive& Ar) = 0;

    /** Compute deterministic hash of current state (for replay verification) */
    virtual uint64 ComputeStateHash() const = 0;

    // =====================================================================
    // QUERY INTERFACE (for correspondence detection, UI, transport)
    // =====================================================================

    /**
     * Query realm for structural features.
     * Used by Correspondence System to detect shared structures.
     * @param Query   What to look for (resonances, vortices, harmonics, etc.)
     * @param Result  Filled with matches (positions, parameters, strengths)
     * @return true if query succeeded
     */
    virtual bool Query(const FRealmQuery& Query, FRealmQueryResult& Result) const = 0;

    /**
     * Get all available query types this kernel supports.
     * Correspondence system uses this to know what structures can be mapped.
     */
    virtual TArray<FName> GetSupportedQueryTypes() const = 0;

    // =====================================================================
    // PARAMETER SPACE (for lazy realization, transport)
    // =====================================================================

    /** Get parameter schema (names, types, ranges) for this realm */
    virtual TMap<FName, FString> GetParameterSchema() const = 0;

    /** Set a runtime parameter (e.g., gravity, viscosity, resonance strength) */
    virtual bool SetParameter(FName Name, const FString& Value) = 0;

    /** Get a runtime parameter */
    virtual bool GetParameter(FName Name, FString& OutValue) const = 0;

    // =====================================================================
    // DETERMINISM VERIFICATION
    // =====================================================================

    /**
     * Verify determinism: run N steps from same seed twice, compare state hashes.
     * @return true if bitwise-identical
     */
    virtual bool VerifyDeterminism(int32 NumSteps = 1000) const = 0;
};

/**
 * Realm State - opaque serializable blob + metadata
 * Each kernel defines its own concrete state structure internally.
 * This base provides versioning and hash.
 */
USTRUCT(BlueprintType)
struct MANIFOLDCORE_API FRealmState
{
    GENERATED_BODY()

    UPROPERTY()
    FName RealmId;

    UPROPERTY()
    uint32 SimulationVersion = 1;

    UPROPERTY()
    uint64 Seed = 0;

    UPROPERTY()
    float SimulationTime = 0.0f;

    UPROPERTY()
    int64 StepCount = 0;

    UPROPERTY()
    uint64 StateHash = 0;

    // Kernel-specific binary data (opaque to correspondence system)
    UPROPERTY()
    TArray<uint8> BinaryData;

    FRealmState() = default;
    FRealmState(FName InRealmId, uint32 InVersion, uint64 InSeed)
        : RealmId(InRealmId), SimulationVersion(InVersion), Seed(InSeed) {}

    bool operator==(const FRealmState& Other) const
    {
        return RealmId == Other.RealmId
            && SimulationVersion == Other.SimulationVersion
            && Seed == Other.Seed
            && StateHash == Other.StateHash
            && BinaryData == Other.BinaryData;
    }
};

/**
 * Query descriptor - what structural feature to find
 * Examples: "OrbitalResonance", "VortexCore", "StandingWave", "HarmonicNode"
 */
USTRUCT(BlueprintType)
struct MANIFOLDCORE_API FRealmQuery
{
    GENERATED_BODY()

    /** Type of structure to query (kernel-specific) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName QueryType;

    /** Parameters for the query (e.g., resonance ratio range, frequency band) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FName, FString> Parameters;

    /** Spatial region to search (empty = whole realm) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FBox SearchBounds;

    /** Maximum number of results */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxResults = 100;

    /** Minimum confidence/strength threshold (0..1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinStrength = 0.0f;

    FRealmQuery() = default;
    explicit FRealmQuery(FName InType) : QueryType(InType) {}
};

/**
 * Query result - a detected structural feature
 */
USTRUCT(BlueprintType)
struct MANIFOLDCORE_API FRealmQueryResult
{
    GENERATED_BODY()

    /** Unique ID for this detected structure */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid StructureId;

    /** Type of structure (matches QueryType) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName StructureType;

    /** World-space position (if applicable) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Position = FVector::ZeroVector;

    /** Orientation (if applicable) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FQuat Rotation = FQuat::Identity;

    /** Strength/confidence (0..1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Strength = 0.0f;

    /** Parameters describing this structure (e.g., resonance ratio, vortex circulation) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FName, FString> Parameters;

    /** Kernel-specific extra data */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<uint8> ExtraData;

    FRealmQueryResult() = default;
};