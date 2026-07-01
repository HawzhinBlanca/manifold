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
 * The structural equivalence relation under which two realms' ratios count as "the
 * same structure". This is the heart of the Constellation Lock puzzle: the player
 * must INFER which relation is active this session, then normalize the six visible
 * (surface-distinct) ratios in their head and pick the subset that truly corresponds.
 *
 *  - Exact:           literal reduced ratio (today's behavior; 3:2 ~ 3:2 only).
 *  - OctaveInvariant: equal after dividing out factors of 2, i.e. pitch-class /
 *                     period-doubling equivalence — 3:2, 6:4 and 3:1 all correspond.
 *  - Reciprocal:      a ratio corresponds to its inverse (p:q ~ q:p) — "the same
 *                     interval seen from the other side".
 */
enum class ECorrespondenceRelation : uint8
{
    Exact,
    OctaveInvariant,
    Reciprocal
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

    /**
     * Canonicalize a "p:q" ratio string under a relation, so that any two ratios which
     * correspond under that relation map to the SAME normalized string (and any two
     * that don't map to different strings). Non-ratio / non-positive input is returned
     * unchanged. Pure and deterministic — the single source of truth for what
     * "corresponds" means, shared by the detector and the Constellation Lock verb.
     */
    static FString NormalizeRatio(const FString& Ratio, ECorrespondenceRelation Relation);

    /** Set the relation the generic N-realm detector matches under (default Exact).
     *  Exact reproduces the literal-ratio behavior; the others enable the harder
     *  Constellation Lock puzzle where corresponding realms look different on the surface. */
    void SetActiveRelation(ECorrespondenceRelation InRelation) { ActiveRelation = InRelation; }

    /** The relation currently in force for shared-structure detection. */
    ECorrespondenceRelation GetActiveRelation() const { return ActiveRelation; }

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

    /** Relation the shared-structure detector matches under (default Exact = literal). */
    ECorrespondenceRelation ActiveRelation = ECorrespondenceRelation::Exact;

    /** Guarantee a data-driven mapping exists: if none was supplied, synthesize the
     *  canonical default spec (OrbitalResonance 3:2 <-> VortexCenter) so detection is
     *  always driven by an explicit spec rather than a hardcoded inline rule. */
    void EnsureDefaultMapping();
};
