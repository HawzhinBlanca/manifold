// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "RhythmKernel.h"
#include "UObject/UObjectGlobals.h"

URhythmKernel::URhythmKernel()
{
    RState = MakeShared<FRhythmState>();
}

void URhythmKernel::Initialize(uint64 Seed)
{
    SimTime = 0.0;
    StepCount = 0;
    RNG.Initialize(Seed);

    RState = MakeShared<FRhythmState>();
    RState->RealmId = GetRealmId();
    RState->SimulationVersion = GetSimulationVersion();
    RState->Seed = Seed;
    RState->SimulationTime = 0.0;
    RState->StepCount = 0;
}

void URhythmKernel::Reset()
{
    Initialize(RState->Seed);
}

void URhythmKernel::Shutdown()
{
    RState.Reset();
}

void URhythmKernel::Step(float DeltaTime)
{
    const double dt = static_cast<double>(DeltaTime);

    for (FRhythmVoice& Voice : RState->Voices)
    {
        // Deterministic phase advance: fraction-of-bar += tempo*dt, wrapped to [0,1).
        Voice.Phase += Voice.Tempo * dt;
        Voice.Phase = FMath::Fmod(Voice.Phase, 1.0);
        if (Voice.Phase < 0.0)
        {
            Voice.Phase += 1.0;
        }
    }

    SimTime += dt;
    StepCount++;
    RState->SimulationTime = SimTime;
    RState->StepCount = StepCount;

    UpdateDerivedState();
}

void URhythmKernel::StepMultiple(float DeltaTime, int32 NumSteps)
{
    for (int32 i = 0; i < NumSteps; ++i)
    {
        Step(DeltaTime);
    }
}

void URhythmKernel::UpdateDerivedState()
{
    DetectRhythmRatios();
    RState->StateHash = ComputeStateHash();
}

TSharedPtr<FRealmState> URhythmKernel::GetState() const
{
    return RState;
}

void URhythmKernel::SetState(const FRealmState& NewState)
{
    const FRhythmState* Casted = static_cast<const FRhythmState*>(&NewState);
    if (!Casted) return;

    RState = MakeShared<FRhythmState>(*Casted);
    SimTime = RState->SimulationTime;
    StepCount = RState->StepCount;
}

void URhythmKernel::SerializeState(FArchive& Ar)
{
    if (RState.IsValid())
    {
        Ar << *RState;
    }
}

void URhythmKernel::DeserializeState(FArchive& Ar)
{
    FRhythmState Loaded;
    Ar << Loaded;
    SetState(Loaded);
}

uint64 URhythmKernel::ComputeStateHash() const
{
    uint64 Hash = 0x27D4EB2F165667C5ULL;
    auto HashDouble = [](double Val) -> uint64
    {
        uint64 Bits;
        FMemory::Memcpy(&Bits, &Val, sizeof(double));
        return Bits;
    };

    for (const FRhythmVoice& Voice : RState->Voices)
    {
        Hash ^= HashDouble(Voice.Tempo) * 0x9E3779B97F4A7C15ULL;
        Hash ^= HashDouble(Voice.Phase) * 0xBF58476D1CE4E5B9ULL;
    }
    return Hash;
}

bool URhythmKernel::Query(const FRealmQuery& QueryObj, FRealmQueryResult& Result) const
{
    if (QueryObj.QueryType == TEXT("RhythmRatio"))
    {
        const float MinStr = QueryObj.MinStrength;
        const FRhythmRatioMatch* Best = nullptr;
        for (const FRhythmRatioMatch& Match : RState->ActiveRatios)
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

void URhythmKernel::QueryAll(const FRealmQuery& QueryDesc, TArray<FRealmQueryResult>& OutResults) const
{
    if (QueryDesc.QueryType != TEXT("RhythmRatio")) return;

    const float MinStr = QueryDesc.MinStrength;
    for (const FRhythmRatioMatch& Match : RState->ActiveRatios)
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

TArray<FName> URhythmKernel::GetSupportedQueryTypes() const
{
    return { TEXT("RhythmRatio") };
}

TMap<FName, FString> URhythmKernel::GetParameterSchema() const
{
    TMap<FName, FString> Schema;
    Schema.Add(TEXT("VoiceCount"), TEXT("int"));
    return Schema;
}

bool URhythmKernel::SetParameter(FName /*Name*/, const FString& /*Value*/)
{
    return false;
}

bool URhythmKernel::GetParameter(FName Name, FString& OutValue) const
{
    if (Name == TEXT("VoiceCount"))
    {
        OutValue = FString::FromInt(RState->Voices.Num());
        return true;
    }
    return false;
}

bool URhythmKernel::VerifyDeterminism(int32 NumSteps) const
{
    URhythmKernel* SimA = NewObject<URhythmKernel>();
    URhythmKernel* SimB = NewObject<URhythmKernel>();
    SimA->Initialize(RState->Seed);
    SimB->Initialize(RState->Seed);

    for (const FRhythmVoice& Voice : RState->Voices)
    {
        SimA->AddVoice(Voice.Tempo);
        SimB->AddVoice(Voice.Tempo);
    }

    for (int32 i = 0; i < NumSteps; ++i)
    {
        SimA->Step(0.01f);
        SimB->Step(0.01f);
    }

    return SimA->ComputeStateHash() == SimB->ComputeStateHash();
}

FGuid URhythmKernel::AddVoice(double Tempo)
{
    FRhythmVoice Voice;
    Voice.Id = FGuid::NewGuid();
    Voice.Tempo = Tempo;
    Voice.Phase = 0.0;
    RState->Voices.Add(Voice);

    UpdateDerivedState();
    return Voice.Id;
}

const TArray<FRhythmRatioMatch>& URhythmKernel::GetActiveRatios() const
{
    return RState->ActiveRatios;
}

void URhythmKernel::DetectRhythmRatios(double MaxDeviation, int32 MaxRatio)
{
    RState->ActiveRatios.Empty();

    const TArray<FRhythmVoice>& Voices = RState->Voices;
    for (int32 i = 0; i < Voices.Num(); ++i)
    {
        for (int32 j = i + 1; j < Voices.Num(); ++j)
        {
            const double Ta = Voices[i].Tempo;
            const double Tb = Voices[j].Tempo;
            if (Ta <= 0.0 || Tb <= 0.0) continue;

            // Higher:lower convention so 3-against-2 is labelled 3:2 (matches Orbits).
            const double RatioVal = FMath::Max(Ta, Tb) / FMath::Min(Ta, Tb);

            // Exact reduced ratio when both tempos are (near-)integers, like Waves/Gears,
            // so integer polyrhythms above MaxRatio are detected exactly.
            const double Hi = FMath::Max(Ta, Tb);
            const double Lo = FMath::Min(Ta, Tb);
            const int32 HiI = FMath::RoundToInt(Hi);
            const int32 LoI = FMath::RoundToInt(Lo);
            if (HiI > 0 && LoI > 0 && FMath::Abs(Hi - HiI) < 1e-6 && FMath::Abs(Lo - LoI) < 1e-6)
            {
                const int32 G = FMath::GreatestCommonDivisor(HiI, LoI);
                FRhythmRatioMatch ExactMatch;
                ExactMatch.Ratio = FIntPoint(HiI / G, LoI / G);
                ExactMatch.ActualRatio = RatioVal;
                ExactMatch.Deviation = 0.0;
                ExactMatch.Strength = 1.0;
                RState->ActiveRatios.Add(ExactMatch);
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
                FRhythmRatioMatch Match;
                Match.Ratio = FIntPoint(BestP, BestQ);
                Match.ActualRatio = RatioVal;
                Match.Deviation = BestDev;
                Match.Strength = FMath::Exp(-200.0 * BestDev);
                RState->ActiveRatios.Add(Match);
            }
        }
    }
}
