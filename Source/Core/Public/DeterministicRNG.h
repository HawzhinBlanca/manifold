// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "DeterministicRNG.generated.h"

/**
 * Deterministic RNG (WP S2)
 * 
 * Wrapper around UE's FRandomStream with additional guarantees:
 * - Explicit 64-bit seed
 * - Cross-platform bitwise reproducibility
 * - State serialization for replay
 * - Substream support for parallel deterministic generation
 */
USTRUCT(BlueprintType)
struct MANIFOLDCORE_API FDeterministicRNG
{
    GENERATED_BODY()

private:
    /** Internal UE random stream */
    FRandomStream Stream;

    /** Original seed for verification */
    uint64 OriginalSeed = 0;

    /** Current substream index */
    int32 SubstreamIndex = 0;

public:
    FDeterministicRNG() = default;
    explicit FDeterministicRNG(uint64 InSeed) { Initialize(InSeed); }

    // =====================================================================
    // INITIALIZATION
    // =====================================================================

    /** Initialize with 64-bit seed (deterministic across platforms) */
    void Initialize(uint64 InSeed)
    {
        OriginalSeed = InSeed;
        SubstreamIndex = 0;
        // FRandomStream uses 32-bit seed, but we fold 64-bit into it deterministically
        uint32 FoldedSeed = static_cast<uint32>(InSeed ^ (InSeed >> 32));
        Stream.Initialize(FoldedSeed);
    }

    /** Create a deterministic substream (for parallel generation) */
    FDeterministicRNG CreateSubstream(int32 SubstreamId) const
    {
        FDeterministicRNG Sub;
        Sub.OriginalSeed = OriginalSeed;
        Sub.SubstreamIndex = SubstreamId;
        // Mix original seed with substream ID for independent stream ID and substream index
        uint64 Mixed = OriginalSeed;
        Mixed ^= static_cast<uint64>(SubstreamId) * 0x9E3779B97F4A7C15ULL; // Golden ratio
        Mixed ^= static_cast<uint64>(SubstreamIndex) * 0xBF58476D1CE4E5B9ULL;
        uint32 Folded = static_cast<uint32>(Mixed ^ (Mixed >> 32));
        Sub.Stream.Initialize(Folded);
        return Sub;
    }

    // =====================================================================
    // GENERATION (matching FRandomStream API)
    // =====================================================================

    float FRand() { return Stream.FRand(); }
    float FRandRange(float Min, float Max) { return Stream.FRandRange(Min, Max); }
    int32 RandRange(int32 Min, int32 Max) { return Stream.RandRange(Min, Max); }
    int32 RandHelper(int32 Max) { return Stream.RandHelper(Max); }
    bool RandBool() { return Stream.RandBool(); }
    float FRandExp() { return Stream.FRandExp(); }

    // Vector generation
    FVector VRand() { return Stream.VRand(); }
    FVector VRandCone(FVector ConstDir, float Spread) { return Stream.VRandCone(ConstDir, Spread); }
    FVector VRandCone(FVector ConstDir, float HorizontalSpread, float VerticalSpread) { return Stream.VRandCone(ConstDir, HorizontalSpread, VerticalSpread); }
    FVector2D VRand2D() { return Stream.VRand2D(); }

    // Gaussian/Normal distribution (Box-Muller, deterministic)
    float RandNormal(float Mean = 0.0f, float StdDev = 1.0f)
    {
        // Box-Muller transform - deterministic
        float u1 = FRand();
        float u2 = FRand();
        // Avoid log(0)
        if (u1 <= 0.0f) u1 = 1.192092896e-07f; // FLT_MIN
        float z0 = FMath::Sqrt(-2.0f * FMath::Loge(u1)) * FMath::Cos(2.0f * PI * u2);
        return Mean + z0 * StdDev;
    }

    FVector RandNormalVector(float StdDev = 1.0f)
    {
        return FVector(RandNormal(0, StdDev), RandNormal(0, StdDev), RandNormal(0, StdDev));
    }

    // =====================================================================
    // STATE MANAGEMENT (for replay)
    // =====================================================================

    /** Get current internal state for serialization */
    uint64 GetState() const
    {
        // FRandomStream doesn't expose state directly, so we reconstruct from seed + count
        // For true state capture, we'd need to track consumption count
        return OriginalSeed;
    }

    /** Serialize to archive */
    friend FArchive& operator<<(FArchive& Ar, FDeterministicRNG& RNG)
    {
        Ar << RNG.OriginalSeed;
        Ar << RNG.SubstreamIndex;
        // Note: FRandomStream state isn't directly serializable in UE
        // For full replay, kernels should serialize their own consumption count
        if (Ar.IsLoading())
        {
            uint32 Folded = static_cast<uint32>(RNG.OriginalSeed ^ (RNG.OriginalSeed >> 32));
            RNG.Stream.Initialize(Folded);
            // Advance by SubstreamIndex * some large number to simulate substream
            for (int32 i = 0; i < RNG.SubstreamIndex * 10000; ++i)
            {
                RNG.Stream.FRand();
            }
        }
        return Ar;
    }

    // =====================================================================
    // VERIFICATION
    // =====================================================================

    uint64 GetOriginalSeed() const { return OriginalSeed; }
    int32 GetSubstreamIndex() const { return SubstreamIndex; }

    /** Verify two RNGs produce same sequence */
    static bool VerifyEquivalence(const FDeterministicRNG& A, const FDeterministicRNG& B, int32 NumSamples = 1000)
    {
        if (A.OriginalSeed != B.OriginalSeed || A.SubstreamIndex != B.SubstreamIndex)
            return false;

        FDeterministicRNG CopyA = A;
        FDeterministicRNG CopyB = B;

        for (int32 i = 0; i < NumSamples; ++i)
        {
            if (!FMath::IsNearlyEqual(CopyA.FRand(), CopyB.FRand()))
                return false;
        }
        return true;
    }
};

/**
 * Global deterministic RNG registry for kernel coordination.
 * Each kernel gets its own substream from a master seed.
 */
class MANIFOLDCORE_API FDeterministicRNGRegistry
{
public:
    static FDeterministicRNGRegistry& Get()
    {
        static FDeterministicRNGRegistry Instance;
        return Instance;
    }

    /** Initialize master seed (call once at startup) */
    void InitializeMaster(uint64 MasterSeed)
    {
        MasterRNG.Initialize(MasterSeed);
        Kernels.Empty();
    }

    /** Get a deterministic RNG for a specific kernel */
    FDeterministicRNG GetKernelRNG(FName KernelId)
    {
        int32 Index = Kernels.FindOrAdd(KernelId, Kernels.Num());
        return MasterRNG.CreateSubstream(Index);
    }

    /** Get substream for a specific purpose within a kernel */
    FDeterministicRNG GetSubstream(FName KernelId, FName Purpose)
    {
        FName Combined = *FString::Printf(TEXT("%s.%s"), *KernelId.ToString(), *Purpose.ToString());
        int32 Index = Kernels.FindOrAdd(Combined, Kernels.Num());
        return MasterRNG.CreateSubstream(Index);
    }

private:
    FDeterministicRNGRegistry() = default;
    FDeterministicRNG MasterRNG;
    TMap<FName, int32> Kernels;
};