// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "WavesKernel.h"
#include "UObject/UObjectGlobals.h"

UWavesKernel::UWavesKernel()
{
    WState = MakeShared<FWavesState>();
}

void UWavesKernel::Initialize(uint64 Seed)
{
    SimTime = 0.0;
    StepCount = 0;
    RNG.Initialize(Seed);

    WState = MakeShared<FWavesState>();
    WState->RealmId = GetRealmId();
    WState->SimulationVersion = GetSimulationVersion();
    WState->Seed = Seed;
    WState->SimulationTime = 0.0;
    WState->StepCount = 0;
    WState->Fundamental = Fundamental;
}

void UWavesKernel::Reset()
{
    Initialize(WState->Seed);
}

void UWavesKernel::Shutdown()
{
    WState.Reset();
}

void UWavesKernel::Step(float DeltaTime)
{
    const double dt = static_cast<double>(DeltaTime);

    for (FStandingWave& Wave : WState->Waves)
    {
        const double Freq = Wave.HarmonicNumber * WState->Fundamental;
        Wave.Phase += 2.0 * PI * Freq * dt;
        Wave.Phase = FMath::Fmod(Wave.Phase, 2.0 * PI);
        if (Wave.Phase < 0.0) Wave.Phase += 2.0 * PI;
    }

    SimTime += dt;
    StepCount++;
    WState->SimulationTime = SimTime;
    WState->StepCount = StepCount;

    UpdateDerivedState();
}

void UWavesKernel::StepMultiple(float DeltaTime, int32 NumSteps)
{
    for (int32 i = 0; i < NumSteps; ++i)
    {
        Step(DeltaTime);
    }
}

void UWavesKernel::UpdateDerivedState()
{
    DetectWaveRatios();
    WState->StateHash = ComputeStateHash();
}

TSharedPtr<FRealmState> UWavesKernel::GetState() const
{
    return WState;
}

void UWavesKernel::SetState(const FRealmState& NewState)
{
    const FWavesState* Casted = static_cast<const FWavesState*>(&NewState);
    if (!Casted) return;

    WState = MakeShared<FWavesState>(*Casted);
    SimTime = WState->SimulationTime;
    StepCount = WState->StepCount;
    Fundamental = WState->Fundamental;
}

void UWavesKernel::SerializeState(FArchive& Ar)
{
    if (WState.IsValid())
    {
        Ar << *WState;
    }
}

void UWavesKernel::DeserializeState(FArchive& Ar)
{
    FWavesState Loaded;
    Ar << Loaded;
    SetState(Loaded);
}

uint64 UWavesKernel::ComputeStateHash() const
{
    uint64 Hash = 0x2B992DDFA23249D6ULL;
    auto HashDouble = [](double Val) -> uint64
    {
        uint64 Bits;
        FMemory::Memcpy(&Bits, &Val, sizeof(double));
        return Bits;
    };

    for (const FStandingWave& Wave : WState->Waves)
    {
        Hash ^= static_cast<uint64>(Wave.HarmonicNumber) * 0x9E3779B97F4A7C15ULL;
        Hash ^= HashDouble(Wave.Phase) * 0xBF58476D1CE4E5B9ULL;
        Hash ^= HashDouble(Wave.Amplitude) * 0x2545F4914F6CDD1DULL;
    }
    return Hash;
}

bool UWavesKernel::Query(const FRealmQuery& QueryObj, FRealmQueryResult& Result) const
{
    if (QueryObj.QueryType == TEXT("WaveHarmonic"))
    {
        const float MinStr = QueryObj.MinStrength;
        const FWaveRatioMatch* Best = nullptr;
        for (const FWaveRatioMatch& Match : WState->ActiveRatios)
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
            return true;
        }
    }
    return false;
}

void UWavesKernel::QueryAll(const FRealmQuery& QueryDesc, TArray<FRealmQueryResult>& OutResults) const
{
    if (QueryDesc.QueryType != TEXT("WaveHarmonic")) return;

    const float MinStr = QueryDesc.MinStrength;
    for (const FWaveRatioMatch& Match : WState->ActiveRatios)
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

TArray<FName> UWavesKernel::GetSupportedQueryTypes() const
{
    return { TEXT("WaveHarmonic") };
}

TMap<FName, FString> UWavesKernel::GetParameterSchema() const
{
    TMap<FName, FString> Schema;
    Schema.Add(TEXT("Fundamental"), TEXT("float"));
    return Schema;
}

bool UWavesKernel::SetParameter(FName Name, const FString& Value)
{
    if (Name == TEXT("Fundamental"))
    {
        Fundamental = FCString::Atod(*Value);
        WState->Fundamental = Fundamental;
        return true;
    }
    return false;
}

bool UWavesKernel::GetParameter(FName Name, FString& OutValue) const
{
    if (Name == TEXT("Fundamental"))
    {
        OutValue = FString::Printf(TEXT("%f"), WState->Fundamental);
        return true;
    }
    return false;
}

bool UWavesKernel::VerifyDeterminism(int32 NumSteps) const
{
    UWavesKernel* SimA = NewObject<UWavesKernel>();
    UWavesKernel* SimB = NewObject<UWavesKernel>();
    SimA->Fundamental = Fundamental;
    SimB->Fundamental = Fundamental;
    SimA->Initialize(WState->Seed);
    SimB->Initialize(WState->Seed);

    for (const FStandingWave& Wave : WState->Waves)
    {
        SimA->ExciteHarmonic(Wave.HarmonicNumber, Wave.Amplitude);
        SimB->ExciteHarmonic(Wave.HarmonicNumber, Wave.Amplitude);
    }

    for (int32 i = 0; i < NumSteps; ++i)
    {
        SimA->Step(0.001f);
        SimB->Step(0.001f);
    }

    return SimA->ComputeStateHash() == SimB->ComputeStateHash();
}

void UWavesKernel::ExciteHarmonic(int32 HarmonicNumber, double Amplitude)
{
    FStandingWave Wave;
    Wave.HarmonicNumber = FMath::Max(1, HarmonicNumber);
    Wave.Amplitude = Amplitude;
    Wave.Phase = 0.0;
    WState->Waves.Add(Wave);

    UpdateDerivedState();
}

const TArray<FWaveRatioMatch>& UWavesKernel::GetActiveRatios() const
{
    return WState->ActiveRatios;
}

void UWavesKernel::DetectWaveRatios()
{
    WState->ActiveRatios.Empty();

    const TArray<FStandingWave>& Waves = WState->Waves;
    for (int32 i = 0; i < Waves.Num(); ++i)
    {
        for (int32 j = i + 1; j < Waves.Num(); ++j)
        {
            const int32 Na = Waves[i].HarmonicNumber;
            const int32 Nb = Waves[j].HarmonicNumber;
            if (Na <= 0 || Nb <= 0) continue;

            // Frequencies are n * Fundamental, so the ratio is just the harmonic
            // numbers reduced to lowest terms (higher:lower).
            const int32 Hi = FMath::Max(Na, Nb);
            const int32 Lo = FMath::Min(Na, Nb);
            const int32 G = FMath::GreatestCommonDivisor(Hi, Lo);

            FWaveRatioMatch Match;
            Match.Ratio = FIntPoint(Hi / G, Lo / G);
            Match.Strength = 1.0; // exact by construction
            WState->ActiveRatios.Add(Match);
        }
    }
}
