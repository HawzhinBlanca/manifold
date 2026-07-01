// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "LazyRealizationManager.generated.h"

/**
 * Procedurally generated data for a single spatial region cell
 */
USTRUCT(BlueprintType)
struct MANIFOLDCORE_API FLazyRegionData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FIntVector Coordinate = FIntVector::ZeroValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FVector> DetailPoints; // Procedural positions of nodes/obstacles

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<float> DensityMap; // Grid density map for this region

    FLazyRegionData() = default;
};

/**
 * Procedural Lazy Realization Manager (WP S7)
 * Implements an LRU cache to page deterministic procedural details on demand.
 */
UCLASS(BlueprintType)
class MANIFOLDCORE_API ULazyRealizationManager : public UObject
{
    GENERATED_BODY()

public:
    ULazyRealizationManager();

    /** Initialize the manager with master seed, cache limits, and region size.
     *  Not BlueprintCallable: uint64 seed is not a Blueprint-supported type. */
    void Initialize(uint64 InMasterSeed, int32 InMaxCacheSize = 128, float InRegionSize = 100.0f);

    /** Query region data at a given 3D world location (causes paging/generation on demand) */
    TSharedPtr<FLazyRegionData> GetRegionData(const FVector& Location);

    /** Query region data by explicit integer coordinate */
    TSharedPtr<FLazyRegionData> GetRegionDataByCoordinate(const FIntVector& Coordinate);

    /** Get current number of cached regions (for verification) */
    UFUNCTION(BlueprintCallable, Category = "MANIFOLD|LazyRealization")
    int32 GetCacheCount() const;

    /** Clear cache and reset registry */
    UFUNCTION(BlueprintCallable, Category = "MANIFOLD|LazyRealization")
    void ClearCache();

protected:
    UPROPERTY()
    uint64 MasterSeed = 0;

    UPROPERTY()
    int32 MaxCacheSize = 128;

    UPROPERTY()
    float RegionSize = 100.0f;

    // Memory-bounded LRU Cache
    TMap<FIntVector, TSharedPtr<FLazyRegionData>> Cache;
    TArray<FIntVector> LRUList;

    /** Generate region data deterministically based on seed and coordinates */
    TSharedPtr<FLazyRegionData> GenerateRegionData(const FIntVector& Coordinate);
};
