// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "DeterministicRNG.h"

FDeterministicRNGRegistry& FDeterministicRNGRegistry::Get()
{
    static FDeterministicRNGRegistry Instance;
    return Instance;
}

void FDeterministicRNGRegistry::InitializeMaster(uint64 MasterSeed)
{
    MasterRNG.Initialize(MasterSeed);
    Kernels.Empty();
}

FDeterministicRNG FDeterministicRNGRegistry::GetKernelRNG(FName KernelId)
{
    int32 Index = Kernels.FindOrAdd(KernelId, Kernels.Num());
    return MasterRNG.CreateSubstream(Index);
}

FDeterministicRNG FDeterministicRNGRegistry::GetSubstream(FName KernelId, FName Purpose)
{
    FName Combined = *FString::Printf(TEXT("%s.%s"), *KernelId.ToString(), *Purpose.ToString());
    int32 Index = Kernels.FindOrAdd(Combined, Kernels.Num());
    return MasterRNG.CreateSubstream(Index);
}
