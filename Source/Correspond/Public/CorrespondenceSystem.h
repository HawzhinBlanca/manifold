// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "RealmKernel.h"
#include "Engine/DataAsset.h"
#include "CorrespondenceSystem.generated.h"

/**
 * Event fired when a correspondence is successfully detected/ignited
 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnCorrespondenceIgnited, FGuid, FGuid, float);

/**
 * Event fired when a transport is completed
 */
DECLARE_MULTICAST_DELEGATE_FourParams(FOnTransportCompleted, FGuid, FName, FGuid, float);

/**
 * Event fired when the generic N-realm engine discovers that two realms share the
 * same structure ratio (a cross-domain analogy). Carries the two realm ids, the
 * shared ratio ("p:q"), and a STABLE id for the shared structure (deterministic
 * from the realm pair + ratio) so it can be deduped/referenced.
 */
DECLARE_MULTICAST_DELEGATE_FourParams(FOnSharedStructureDiscovered, FName /*RealmA*/, FName /*RealmB*/, FString /*Ratio*/, FGuid /*StableId*/);

/**
 * Data-driven correspondence specification (WP S5)
 */
USTRUCT(BlueprintType)
struct MANIFOLDCORRESPOND_API FCorrespondenceSpec
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName SourceStructureType; // e.g., "OrbitalResonance"

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName TargetStructureType; // e.g., "VortexCenter"

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString MatchingRatio; // e.g., "3:2" or strength match

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Tolerance = 0.05f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ScaleFactor = 1.0f;
};

/**
 * Correspondence Mapping asset
 */
UCLASS(BlueprintType)
class MANIFOLDCORRESPOND_API UCorrespondenceMapping : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FCorrespondenceSpec> Specs;

    /** Parse a correspondence mapping from JSON text (data-driven content, Build Plan D1).
     *  Returns nullptr if the JSON is malformed. */
    static UCorrespondenceMapping* CreateFromJsonString(const FString& JsonText, UObject* Outer = nullptr);

    /** Load a correspondence mapping from a JSON file (e.g. Data/Correspondences/*.json).
     *  Returns nullptr if the file is missing or malformed. */
    static UCorrespondenceMapping* CreateFromJsonFile(const FString& FilePath, UObject* Outer = nullptr);
};

/**
 * A realm registered with the correspondence engine, plus the query type that
 * exposes its structure (Build Plan §9: every realm exposes its structure for mappings).
 */
USTRUCT()
struct MANIFOLDCORRESPOND_API FRegisteredRealm
{
    GENERATED_BODY()

    UPROPERTY()
    FName RealmId;

    /** The query type that returns this realm's structure with a "Ratio" parameter. */
    UPROPERTY()
    FName StructureQueryType;

    UPROPERTY()
    UObject* Kernel = nullptr;
};

/**
 * Correspondence System manager
 */
UCLASS(BlueprintType)
class MANIFOLDCORRESPOND_API UCorrespondenceSystem : public UObject
{
    GENERATED_BODY()

public:
    UCorrespondenceSystem();

    /** Register source and target kernels (the original Orbits<->Fluids slice path). */
    void RegisterKernels(UObject* InOrbitsKernel, UObject* InFluidsKernel);

    /** Initialize from mapping asset */
    void InitializeMapping(UCorrespondenceMapping* MappingAsset);

    /** Query kernels and detect active correspondences */
    bool DetectCorrespondence();

    /** Transport a state change from source structure to target realm */
    bool Transport(FGuid SourceStructureId, FName TargetRealm);

    // =====================================================================
    // GENERIC N-REALM CORRESPONDENCE (the core mechanic, scaled)
    // =====================================================================

    /** Register any realm by id + the query type that exposes its structure ratio. */
    void RegisterRealm(FName RealmId, FName StructureQueryType, UObject* Kernel);

    /**
     * Detect correspondences between ANY two registered realms that expose the
     * same structure ratio (e.g. orbital 3:2 <-> harmonic 3:2 — the cross-domain
     * analogy generalized to N realms). Broadcasts OnCorrespondenceIgnited for each
     * newly-found pair (idempotent across calls). Returns the count of NEW ones.
     */
    int32 DetectSharedStructureCorrespondences();

    // =====================================================================
    // EVENTS
    // =====================================================================
    FOnCorrespondenceIgnited OnCorrespondenceIgnited;
    FOnTransportCompleted OnTransportCompleted;
    FOnSharedStructureDiscovered OnSharedStructureDiscovered;

protected:
    UPROPERTY()
    UObject* OrbitsKernel;

    UPROPERTY()
    UObject* FluidsKernel;

    UPROPERTY()
    UCorrespondenceMapping* Mapping;

    UPROPERTY()
    TArray<FGuid> IgnitedCorrespondences;

    /** Realms registered for generic N-realm correspondence. */
    UPROPERTY()
    TArray<FRegisteredRealm> RegisteredRealms;

    /** Dedup key set ("RealmA|RealmB|Ratio") so a shared structure ignites once. */
    TSet<FString> IgnitedSharedStructures;

    /** Guarantee a data-driven mapping exists: if none was supplied, synthesize the
     *  canonical default spec (OrbitalResonance 3:2 <-> VortexCenter) so detection is
     *  always driven by an explicit spec rather than a hardcoded inline rule. */
    void EnsureDefaultMapping();
};
