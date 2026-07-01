// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DeterministicRNG.generated.h"

/**
 * Deterministic RNG using a custom PCG-XSH-RR 64/32 generator.
 * Guaranteed bitwise reproducibility across platforms.
 */
USTRUCT(BlueprintType)
struct MANIFOLDCORE_API FDeterministicRNG
{
    GENERATED_BODY()

private:
    UPROPERTY()
    uint64 State = 0;

    UPROPERTY()
    uint64 Inc = 1;

    UPROPERTY()
    uint64 OriginalSeed = 0;

    UPROPERTY()
    int32 SubstreamIndex = 0;

    UPROPERTY()
    uint64 ConsumptionCount = 0;

public:
    FDeterministicRNG() = default;
    explicit FDeterministicRNG(uint64 InSeed) { Initialize(InSeed); }

    void Initialize(uint64 InSeed)
    {
        OriginalSeed = InSeed;
        SubstreamIndex = 0;
        ConsumptionCount = 0;
        
        // PCG initialization: state is seed, Inc is odd number
        State = InSeed + 1442695040888963407ULL;
        Inc = (InSeed << 1) | 1ULL;
        // Warm up the generator
        NextUint32();
        ConsumptionCount = 0; // Reset count after initialization
    }

    uint32 NextUint32()
    {
        uint64 OldState = State;
        State = OldState * 6364136223846793005ULL + Inc;
        uint32 Xorshifted = static_cast<uint32>(((OldState >> 18u) ^ OldState) >> 27u);
        uint32 Rot = static_cast<uint32>(OldState >> 59u);
        ConsumptionCount++;
        return (Xorshifted >> Rot) | (Xorshifted << ((0u - Rot) & 31u));
    }

    FDeterministicRNG CreateSubstream(int32 SubstreamId) const
    {
        // Deterministically mix the master seed for the substream
        uint64 Mixed = OriginalSeed;
        Mixed ^= static_cast<uint64>(SubstreamId) * 0x9E3779B97F4A7C15ULL; // Golden ratio
        Mixed ^= static_cast<uint64>(SubstreamIndex) * 0xBF58476D1CE4E5B9ULL;
        
        FDeterministicRNG Sub;
        Sub.Initialize(Mixed);
        Sub.OriginalSeed = OriginalSeed;
        Sub.SubstreamIndex = SubstreamId;
        return Sub;
    }

    // =====================================================================
    // GENERATION API
    // =====================================================================

    float FRand()
    {
        // 24 bits of entropy scaled to [0, 1)
        return (NextUint32() >> 8) * (1.0f / 16777216.0f);
    }

    float FRandRange(float Min, float Max)
    {
        return Min + (Max - Min) * FRand();
    }

    int32 RandRange(int32 Min, int32 Max)
    {
        if (Min > Max)
        {
            int32 Temp = Min;
            Min = Max;
            Max = Temp;
        }
        uint64 Range = static_cast<uint64>(Max - Min) + 1;
        // Rejection sampling for perfect uniformity
        uint64 Limit = (0x100000000ULL / Range) * Range;
        uint32 Val;
        do {
            Val = NextUint32();
        } while (Val >= Limit);
        return Min + static_cast<int32>(Val % Range);
    }

    int32 RandHelper(int32 Max)
    {
        return Max > 0 ? RandRange(0, Max - 1) : 0;
    }

    bool RandBool()
    {
        return (NextUint32() & 1) != 0;
    }

    float FRandExp()
    {
        float U = FRand();
        if (U <= 0.0f) U = 1.192092896e-07f; // FLT_MIN
        return -FMath::Loge(U);
    }

    FVector VRand()
    {
        float Theta = FRand() * 2.0f * PI;
        float Phi = FMath::Acos(2.0f * FRand() - 1.0f);
        return FVector(FMath::Sin(Phi) * FMath::Cos(Theta), FMath::Sin(Phi) * FMath::Sin(Theta), FMath::Cos(Phi));
    }

    FVector VRandCone(FVector ConstDir, float Spread)
    {
        if (Spread <= 0.0f) return ConstDir;
        float CosTheta = FMath::Cos(Spread);
        float U = FRand();
        float CosSpread = U + (1.0f - U) * CosTheta;
        float SinSpread = FMath::Sqrt(1.0f - CosSpread * CosSpread);
        float Phi = FRand() * 2.0f * PI;
        
        FVector DirX, DirY;
        ConstDir.FindBestAxisVectors(DirX, DirY);
        return ConstDir * CosSpread + DirX * (FMath::Cos(Phi) * SinSpread) + DirY * (FMath::Sin(Phi) * SinSpread);
    }

    FVector VRandCone(FVector ConstDir, float HorizontalSpread, float VerticalSpread)
    {
        // Simple approximation/generalization: use average spread for direction
        return VRandCone(ConstDir, (HorizontalSpread + VerticalSpread) * 0.5f);
    }

    FVector2D VRand2D()
    {
        float Theta = FRand() * 2.0f * PI;
        return FVector2D(FMath::Cos(Theta), FMath::Sin(Theta));
    }

    float RandNormal(float Mean = 0.0f, float StdDev = 1.0f)
    {
        float u1 = FRand();
        float u2 = FRand();
        if (u1 <= 0.0f) u1 = 1.192092896e-07f;
        float z0 = FMath::Sqrt(-2.0f * FMath::Loge(u1)) * FMath::Cos(2.0f * PI * u2);
        return Mean + z0 * StdDev;
    }

    FVector RandNormalVector(float StdDev = 1.0f)
    {
        return FVector(RandNormal(0, StdDev), RandNormal(0, StdDev), RandNormal(0, StdDev));
    }

    // =====================================================================
    // STATE MANAGEMENT
    // =====================================================================

    uint64 GetState() const { return State; }
    uint64 GetOriginalSeed() const { return OriginalSeed; }
    int32 GetSubstreamIndex() const { return SubstreamIndex; }
    uint64 GetConsumptionCount() const { return ConsumptionCount; }

    friend FArchive& operator<<(FArchive& Ar, FDeterministicRNG& RNG)
    {
        Ar << RNG.State;
        Ar << RNG.Inc;
        Ar << RNG.OriginalSeed;
        Ar << RNG.SubstreamIndex;
        Ar << RNG.ConsumptionCount;
        return Ar;
    }

    static bool VerifyEquivalence(const FDeterministicRNG& A, const FDeterministicRNG& B, int32 NumSamples = 1000)
    {
        if (A.OriginalSeed != B.OriginalSeed || A.SubstreamIndex != B.SubstreamIndex)
            return false;

        FDeterministicRNG CopyA = A;
        FDeterministicRNG CopyB = B;

        for (int32 i = 0; i < NumSamples; ++i)
        {
            if (CopyA.NextUint32() != CopyB.NextUint32())
                return false;
        }
        return true;
    }
};

/**
 * Global deterministic RNG registry for kernel coordination.
 * Moved to .cpp to prevent modular/DLL singleton duplication.
 */
class MANIFOLDCORE_API FDeterministicRNGRegistry
{
public:
    static FDeterministicRNGRegistry& Get();

    void InitializeMaster(uint64 MasterSeed);
    FDeterministicRNG GetKernelRNG(FName KernelId);
    FDeterministicRNG GetSubstream(FName KernelId, FName Purpose);

private:
    FDeterministicRNGRegistry() = default;
    FDeterministicRNG MasterRNG;
    TMap<FName, int32> Kernels;
};