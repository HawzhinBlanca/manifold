// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "LazyRealizationManager.h"
#include "DeterministicRNG.h"

ULazyRealizationManager::ULazyRealizationManager()
{
    MasterSeed = 0;
    MaxCacheSize = 128;
    RegionSize = 100.0f;
}

void ULazyRealizationManager::Initialize(uint64 InMasterSeed, int32 InMaxCacheSize, float InRegionSize)
{
    MasterSeed = InMasterSeed;
    MaxCacheSize = InMaxCacheSize;
    RegionSize = InRegionSize;
    ClearCache();
}

TSharedPtr<FLazyRegionData> ULazyRealizationManager::GetRegionData(const FVector& Location)
{
    int32 i = FMath::FloorToInt(Location.X / RegionSize);
    int32 j = FMath::FloorToInt(Location.Y / RegionSize);
    int32 k = FMath::FloorToInt(Location.Z / RegionSize);
    return GetRegionDataByCoordinate(FIntVector(i, j, k));
}

TSharedPtr<FLazyRegionData> ULazyRealizationManager::GetRegionDataByCoordinate(const FIntVector& Coordinate)
{
    if (Cache.Contains(Coordinate))
    {
        // Move to the back of LRUList (most recently used)
        LRUList.Remove(Coordinate);
        LRUList.Add(Coordinate);
        return Cache[Coordinate];
    }

    // Generate on cache miss
    TSharedPtr<FLazyRegionData> NewData = GenerateRegionData(Coordinate);

    // Evict oldest if cache is full
    if (Cache.Num() >= MaxCacheSize && LRUList.Num() > 0)
    {
        FIntVector OldestCoord = LRUList[0];
        LRUList.RemoveAt(0);
        Cache.Remove(OldestCoord);
    }

    Cache.Add(Coordinate, NewData);
    LRUList.Add(Coordinate);
    return NewData;
}

int32 ULazyRealizationManager::GetCacheCount() const
{
    return Cache.Num();
}

void ULazyRealizationManager::ClearCache()
{
    Cache.Empty();
    LRUList.Empty();
}

TSharedPtr<FLazyRegionData> ULazyRealizationManager::GenerateRegionData(const FIntVector& Coordinate)
{
    // Deterministically mix coordinates into the seed
    uint64 CellSeed = MasterSeed;
    CellSeed ^= static_cast<uint64>(Coordinate.X) * 0x9E3779B97F4A7C15ULL;
    CellSeed ^= static_cast<uint64>(Coordinate.Y) * 0xBF58476D1CE4E5B9ULL;
    CellSeed ^= static_cast<uint64>(Coordinate.Z) * 0x123456789ABCDEF0ULL;

    FDeterministicRNG CellRNG(CellSeed);

    TSharedPtr<FLazyRegionData> Data = MakeShared<FLazyRegionData>();
    Data->Coordinate = Coordinate;

    // Generate 5 to 15 detail points
    int32 Count = CellRNG.RandRange(5, 15);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        float X = CellRNG.FRandRange(0.0f, RegionSize);
        float Y = CellRNG.FRandRange(0.0f, RegionSize);
        float Z = CellRNG.FRandRange(0.0f, RegionSize);
        
        FVector WorldPos = FVector(Coordinate.X, Coordinate.Y, Coordinate.Z) * RegionSize + FVector(X, Y, Z);
        Data->DetailPoints.Add(WorldPos);
    }

    // Generate a 4x4 density map
    Data->DensityMap.Init(0.0f, 16);
    for (int32 Index = 0; Index < 16; ++Index)
    {
        Data->DensityMap[Index] = CellRNG.FRand();
    }

    return Data;
}
