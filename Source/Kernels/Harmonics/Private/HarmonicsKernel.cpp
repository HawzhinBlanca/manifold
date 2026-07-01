// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "HarmonicsKernel.h"
#include "UObject/UObjectGlobals.h"

UHarmonicsKernel::UHarmonicsKernel()
{
    HState = MakeShared<FHarmonicsState>();
}

void UHarmonicsKernel::Initialize(uint64 Seed)
{
    SimTime = 0.0;
    StepCount = 0;
    RNG.Initialize(Seed);

    HState = MakeShared<FHarmonicsState>();
    HState->RealmId = GetRealmId();
    HState->SimulationVersion = GetSimulationVersion();
    HState->Seed = Seed;
    HState->SimulationTime = 0.0;
    HState->StepCount = 0;
}

void UHarmonicsKernel::Reset()
{
    Initialize(HState->Seed);
}

void UHarmonicsKernel::Shutdown()
{
    HState.Reset();
}

void UHarmonicsKernel::Step(float DeltaTime)
{
    const double dt = static_cast<double>(DeltaTime);

    for (FHarmonicMode& Mode : HState->Modes)
    {
        // Deterministic phase advance: phi += 2*pi*f*dt, wrapped to [0, 2*pi).
        Mode.Phase += 2.0 * PI * Mode.Frequency * dt;
        Mode.Phase = FMath::Fmod(Mode.Phase, 2.0 * PI);
        if (Mode.Phase < 0.0)
        {
            Mode.Phase += 2.0 * PI;
        }
    }

    SimTime += dt;
    StepCount++;
    HState->SimulationTime = SimTime;
    HState->StepCount = StepCount;

    UpdateDerivedState();
}

void UHarmonicsKernel::StepMultiple(float DeltaTime, int32 NumSteps)
{
    for (int32 i = 0; i < NumSteps; ++i)
    {
        Step(DeltaTime);
    }
}

void UHarmonicsKernel::UpdateDerivedState()
{
    DetectHarmonicRatios();
    HState->StateHash = ComputeStateHash();
}

TSharedPtr<FRealmState> UHarmonicsKernel::GetState() const
{
    return HState;
}

void UHarmonicsKernel::SetState(const FRealmState& NewState)
{
    const FHarmonicsState* Casted = static_cast<const FHarmonicsState*>(&NewState);
    if (!Casted) return;

    HState = MakeShared<FHarmonicsState>(*Casted);
    SimTime = HState->SimulationTime;
    StepCount = HState->StepCount;
}

void UHarmonicsKernel::SerializeState(FArchive& Ar)
{
    if (HState.IsValid())
    {
        Ar << *HState;
    }
}

void UHarmonicsKernel::DeserializeState(FArchive& Ar)
{
    FHarmonicsState Loaded;
    Ar << Loaded;
    SetState(Loaded);
}

uint64 UHarmonicsKernel::ComputeStateHash() const
{
    uint64 Hash = 0x51ED270B2E5A6C1DULL;
    auto HashDouble = [](double Val) -> uint64
    {
        uint64 Bits;
        FMemory::Memcpy(&Bits, &Val, sizeof(double));
        return Bits;
    };

    for (const FHarmonicMode& Mode : HState->Modes)
    {
        Hash ^= HashDouble(Mode.Frequency) * 0x9E3779B97F4A7C15ULL;
        Hash ^= HashDouble(Mode.Phase) * 0xBF58476D1CE4E5B9ULL;
        Hash ^= HashDouble(Mode.Amplitude) * 0x2545F4914F6CDD1DULL;
    }
    return Hash;
}

bool UHarmonicsKernel::Query(const FRealmQuery& QueryObj, FRealmQueryResult& Result) const
{
    if (QueryObj.QueryType == TEXT("HarmonicRatio"))
    {
        const float MinStr = QueryObj.MinStrength;
        const FHarmonicRatioMatch* Best = nullptr;
        for (const FHarmonicRatioMatch& Match : HState->ActiveRatios)
        {
            if (Match.Strength < MinStr) continue;
            if (!Best || Match.Strength > Best->Strength)
            {
                Best = &Match;
            }
        }

        if (Best)
        {
            // Stable identity: same realm + same detected ratio => same id across
            // frames, so the correspondence layer can dedup and transport by it.
            Result.StructureId = FGuid(GetTypeHash(GetRealmId()),
                static_cast<uint32>(Best->Ratio.X), static_cast<uint32>(Best->Ratio.Y),
                GetSimulationVersion());
            Result.StructureType = QueryObj.QueryType;
            Result.Strength = static_cast<float>(Best->Strength);
            Result.Parameters.Add(TEXT("Ratio"), FString::Printf(TEXT("%d:%d"), Best->Ratio.X, Best->Ratio.Y));
            Result.Parameters.Add(TEXT("Deviation"), FString::Printf(TEXT("%f"), Best->Deviation));
            return true;
        }
    }
    return false;
}

void UHarmonicsKernel::QueryAll(const FRealmQuery& QueryDesc, TArray<FRealmQueryResult>& OutResults) const
{
    if (QueryDesc.QueryType != TEXT("HarmonicRatio")) return;

    const float MinStr = QueryDesc.MinStrength;
    for (const FHarmonicRatioMatch& Match : HState->ActiveRatios)
    {
        if (Match.Strength < MinStr) continue;

        FRealmQueryResult Result;
        Result.StructureId = FGuid(GetTypeHash(GetRealmId()),
            static_cast<uint32>(Match.Ratio.X), static_cast<uint32>(Match.Ratio.Y),
            GetSimulationVersion());
        Result.StructureType = QueryDesc.QueryType;
        Result.Strength = static_cast<float>(Match.Strength);
        Result.Parameters.Add(TEXT("Ratio"), FString::Printf(TEXT("%d:%d"), Match.Ratio.X, Match.Ratio.Y));
        OutResults.Add(Result);
    }
}

TArray<FName> UHarmonicsKernel::GetSupportedQueryTypes() const
{
    return { TEXT("HarmonicRatio") };
}

TMap<FName, FString> UHarmonicsKernel::GetParameterSchema() const
{
    TMap<FName, FString> Schema;
    Schema.Add(TEXT("ModeCount"), TEXT("int"));
    return Schema;
}

bool UHarmonicsKernel::SetParameter(FName /*Name*/, const FString& /*Value*/)
{
    return false;
}

bool UHarmonicsKernel::GetParameter(FName Name, FString& OutValue) const
{
    if (Name == TEXT("ModeCount"))
    {
        OutValue = FString::FromInt(HState->Modes.Num());
        return true;
    }
    return false;
}

bool UHarmonicsKernel::VerifyDeterminism(int32 NumSteps) const
{
    UHarmonicsKernel* SimA = NewObject<UHarmonicsKernel>();
    UHarmonicsKernel* SimB = NewObject<UHarmonicsKernel>();
    SimA->Initialize(HState->Seed);
    SimB->Initialize(HState->Seed);

    for (const FHarmonicMode& Mode : HState->Modes)
    {
        SimA->AddMode(Mode.Frequency, Mode.Amplitude);
        SimB->AddMode(Mode.Frequency, Mode.Amplitude);
    }

    for (int32 i = 0; i < NumSteps; ++i)
    {
        SimA->Step(0.01f);
        SimB->Step(0.01f);
    }

    return SimA->ComputeStateHash() == SimB->ComputeStateHash();
}

FGuid UHarmonicsKernel::AddMode(double Frequency, double Amplitude)
{
    FHarmonicMode Mode;
    Mode.Id = FGuid::NewGuid();
    Mode.Frequency = Frequency;
    Mode.Amplitude = Amplitude;
    Mode.Phase = 0.0;
    HState->Modes.Add(Mode);

    UpdateDerivedState();
    return Mode.Id;
}

const TArray<FHarmonicRatioMatch>& UHarmonicsKernel::GetActiveRatios() const
{
    return HState->ActiveRatios;
}

void UHarmonicsKernel::DetectHarmonicRatios(double MaxDeviation, int32 MaxRatio)
{
    HState->ActiveRatios.Empty();

    const TArray<FHarmonicMode>& Modes = HState->Modes;
    for (int32 i = 0; i < Modes.Num(); ++i)
    {
        for (int32 j = i + 1; j < Modes.Num(); ++j)
        {
            const double Fa = Modes[i].Frequency;
            const double Fb = Modes[j].Frequency;
            if (Fa <= 0.0 || Fb <= 0.0) continue;

            // Higher:lower convention so 3 Hz vs 2 Hz is labelled 3:2 (matches Orbits).
            const double RatioVal = FMath::Max(Fa, Fb) / FMath::Min(Fa, Fb);

            // Exact reduced ratio when both frequencies are (near-)integers, like the
            // Waves/Gears kernels — so integer ratios ABOVE MaxRatio are still detected
            // exactly instead of being silently missed.
            const double Hi = FMath::Max(Fa, Fb);
            const double Lo = FMath::Min(Fa, Fb);
            const int32 HiI = FMath::RoundToInt(Hi);
            const int32 LoI = FMath::RoundToInt(Lo);
            if (HiI > 0 && LoI > 0 && FMath::Abs(Hi - HiI) < 1e-6 && FMath::Abs(Lo - LoI) < 1e-6)
            {
                const int32 G = FMath::GreatestCommonDivisor(HiI, LoI);
                FHarmonicRatioMatch ExactMatch;
                ExactMatch.Ratio = FIntPoint(HiI / G, LoI / G);
                ExactMatch.ActualRatio = RatioVal;
                ExactMatch.Deviation = 0.0;
                ExactMatch.Strength = 1.0;
                HState->ActiveRatios.Add(ExactMatch);
                continue;
            }

            int32 BestP = 1;
            int32 BestQ = 1;
            double BestDev = 1e9;
            for (int32 p = 1; p <= MaxRatio; ++p)
            {
                for (int32 q = 1; q <= MaxRatio; ++q)
                {
                    const double Rational = static_cast<double>(p) / q;
                    const double Dev = FMath::Abs(RatioVal - Rational);
                    if (Dev < BestDev)
                    {
                        BestDev = Dev;
                        BestP = p;
                        BestQ = q;
                    }
                }
            }

            if (BestDev <= MaxDeviation)
            {
                FHarmonicRatioMatch Match;
                Match.Ratio = FIntPoint(BestP, BestQ);
                Match.ActualRatio = RatioVal;
                Match.Deviation = BestDev;
                Match.Strength = FMath::Exp(-200.0 * BestDev);
                HState->ActiveRatios.Add(Match);
            }
        }
    }
}
